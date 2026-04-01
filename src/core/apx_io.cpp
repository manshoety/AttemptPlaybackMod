// apx_io.cpp
#include <ostream>
#include <istream>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include "types.hpp"
#include "apx_format.hpp"
#include "apx_io.hpp"

static constexpr double kAPXPosScale_  = 64.0;   // 1/64 px
static constexpr double kAPXRotScale_  = 16.0;   // 1/16 deg
static constexpr double kAPXSizeScale_ = 256.0;  // 1/256
static constexpr double kAPXTimeScale_ = 48000;  // up to 48k Hz (so any large weird physics update people set won't look jittery)
static constexpr size_t kAPXKeyEvery_  = 64;
static constexpr uint32_t kAPXMaxChunkSize_ = 0x7fffffffu; // 2 GB hard cap (Already like impossibly high and would not happen unless an error occurred)
static constexpr uint32_t kAPXMaxFramesPerTrack_ = 10'000'000u; // Who in the world is doing over 10M attempts (also aint no way any PC could run that many replaying at once)
static constexpr uint32_t kAPXMaxPracticeSessions_ = 100'000u; // You better not be doing 100k practice sessions on a level or you need to go outside
static constexpr uint32_t kAPXMaxPracticeItemsPerSession_ = 1'000'000u; // 1M practice attempt session would be crazy

// I like how I added all of this, and the user who crashed had an old pre-release file format which the public release code doesn't support
// They said they only used the official Geode release, but the file that crashed their game was created March 25th and the mod was released March 28th
// How in the world did that file get in their game folder????
// Still good to have to avoid a crash if a file is to corrupted

enum : uint8_t {
    kAPXRec_Key_     = 1 << 0,
    kAPXRec_Mode_    = 1 << 1,
    kAPXRec_Flags_   = 1 << 2,
    kAPXRec_Vehicle_ = 1 << 3,
    kAPXRec_Wave_    = 1 << 4,
};

struct APXQuantState_ {
    int32_t xQ = 0;
    int32_t yQ = 0;
    int32_t rotQ = 0;
    uint16_t vehicleQ = 1;
    uint16_t waveQ = 1;
    uint32_t tQ = 0;
    uint8_t mode = static_cast<uint8_t>(IconType::Cube);
    uint8_t flags = 0;
};

// File stuff (3.5x smaller)

template <class T>
static inline void appendPod_(std::vector<uint8_t>& dst, T const& v) {
    const size_t old = dst.size();
    dst.resize(old + sizeof(T));
    std::memcpy(dst.data() + old, &v, sizeof(T));
}

template <class T>
static inline bool readPod_(const uint8_t*& cur, const uint8_t* end, T& out) {
    if ((size_t)(end - cur) < sizeof(T)) return false;
    std::memcpy(&out, cur, sizeof(T));
    cur += sizeof(T);
    return true;
}

static inline uint8_t attemptFlagsFromAttempt_(Attempt const& a) {
    uint8_t flags = 0;
    if (a.practiceAttempt) flags |= 1u;
    if (a.completed)       flags |= (1u << 1);
    if (a.hadDual)         flags |= (1u << 2);
    return flags;
}

static inline void applyAttemptFlags_(uint8_t flags, Attempt& a) {
    a.practiceAttempt = (flags & 1u) != 0;
    a.completed = (flags & (1u << 1)) != 0;
    a.hadDual = (flags & (1u << 2)) != 0;
}

static inline int32_t quantPos_(float v) {
    return static_cast<int32_t>(std::llround(static_cast<double>(v) * kAPXPosScale_));
}

static inline int32_t quantRot_(float v) {
    return static_cast<int32_t>(std::llround(static_cast<double>(v) * kAPXRotScale_));
}

static inline uint16_t quantSize_(float v) {
    long long q = std::llround(static_cast<double>(v) * kAPXSizeScale_);
    if (q < 0) q = 0;
    if (q > std::numeric_limits<uint16_t>::max()) q = std::numeric_limits<uint16_t>::max();
    return static_cast<uint16_t>(q);
}

static inline uint32_t quantTime_(double v) {
    if (v < 0.0) v = 0.0;
    unsigned long long q = static_cast<unsigned long long>(std::llround(v * kAPXTimeScale_));
    if (q > std::numeric_limits<uint32_t>::max()) q = std::numeric_limits<uint32_t>::max();
    return static_cast<uint32_t>(q);
}

static inline float dequantPos_(int32_t q) {
    return static_cast<float>(static_cast<double>(q) / kAPXPosScale_);
}

static inline float dequantRot_(int32_t q) {
    return static_cast<float>(static_cast<double>(q) / kAPXRotScale_);
}

static inline float dequantSize_(uint16_t q) {
    return static_cast<float>(static_cast<double>(q) / kAPXSizeScale_);
}

static inline double dequantTime_(uint32_t q) {
    return static_cast<double>(q) / kAPXTimeScale_;
}

static inline APXQuantState_ makeQuantState_(Frame const& f) {
    APXQuantState_ q;
    q.xQ = f.x.q;
    q.yQ = f.y.q;
    q.rotQ = f.rot.q;
    q.vehicleQ = f.vehicleSize.q;
    q.waveQ = f.waveSize.q;
    q.tQ = f.t.q;
    q.mode = static_cast<uint8_t>(f.mode);
    q.flags = flagsFromFrame_(f);
    return q;
}

static inline Frame makeFrame_(APXQuantState_ const& s) {
    Frame f{};
    f.x.q = s.xQ;
    f.y.q = s.yQ;
    f.rot.q = s.rotQ;
    f.vehicleSize.q = s.vehicleQ;
    f.waveSize.q = s.waveQ;
    f.mode = static_cast<IconType>(s.mode);
    f.t.q = s.tQ;
    frameFromFlags_(s.flags, f);
    return f;
}

static std::vector<uint8_t> encodeTrackCompact_(std::vector<Frame> const& frames) {
    std::vector<uint8_t> out;
    if (frames.empty()) return out;

    out.reserve(frames.size() * 10 + 32);

    APXQuantState_ prev{};
    bool havePrev = false;
    size_t sinceKey = 0;

    for (size_t i = 0; i < frames.size(); ++i) {
        const APXQuantState_ cur = makeQuantState_(frames[i]);

        bool needKey = !havePrev || sinceKey >= kAPXKeyEvery_;
        if (!needKey) {
            const long long dx = static_cast<long long>(cur.xQ) - static_cast<long long>(prev.xQ);
            const long long dy = static_cast<long long>(cur.yQ) - static_cast<long long>(prev.yQ);
            const long long dr = static_cast<long long>(cur.rotQ) - static_cast<long long>(prev.rotQ);
            const unsigned long long dt =
                (cur.tQ >= prev.tQ)
                    ? static_cast<unsigned long long>(cur.tQ - prev.tQ)
                    : std::numeric_limits<unsigned long long>::max();

            if (dx < std::numeric_limits<int16_t>::min() || dx > std::numeric_limits<int16_t>::max() ||
                dy < std::numeric_limits<int16_t>::min() || dy > std::numeric_limits<int16_t>::max() ||
                dr < std::numeric_limits<int16_t>::min() || dr > std::numeric_limits<int16_t>::max() ||
                dt > std::numeric_limits<uint16_t>::max()) {
                needKey = true;
            }
        }

        if (needKey) {
            const uint8_t tag = kAPXRec_Key_;

            APXFrameKeyCompact rec{};
            rec.xQ = cur.xQ;
            rec.yQ = cur.yQ;
            rec.rotQ = cur.rotQ;
            rec.vehicleQ = cur.vehicleQ;
            rec.waveQ = cur.waveQ;
            rec.tQ = cur.tQ;
            rec.mode = cur.mode;
            rec.flags = cur.flags;

            appendPod_(out, tag);
            appendPod_(out, rec);

            prev = cur;
            havePrev = true;
            sinceKey = 0;
            continue;
        }

        uint8_t tag = 0;
        if (cur.mode != prev.mode)         tag |= kAPXRec_Mode_;
        if (cur.flags != prev.flags)       tag |= kAPXRec_Flags_;
        if (cur.vehicleQ != prev.vehicleQ) tag |= kAPXRec_Vehicle_;
        if (cur.waveQ != prev.waveQ)       tag |= kAPXRec_Wave_;

        APXFrameDeltaCompact rec{};
        rec.dxQ = static_cast<int16_t>(cur.xQ - prev.xQ);
        rec.dyQ = static_cast<int16_t>(cur.yQ - prev.yQ);
        rec.drotQ = static_cast<int16_t>(cur.rotQ - prev.rotQ);
        rec.dtQ = static_cast<uint16_t>(cur.tQ - prev.tQ);

        appendPod_(out, tag);
        appendPod_(out, rec);

        if (tag & kAPXRec_Mode_)    appendPod_(out, cur.mode);
        if (tag & kAPXRec_Flags_)   appendPod_(out, cur.flags);
        if (tag & kAPXRec_Vehicle_) appendPod_(out, cur.vehicleQ);
        if (tag & kAPXRec_Wave_)    appendPod_(out, cur.waveQ);

        prev = cur;
        ++sinceKey;
    }

    return out;
}

static bool decodeTrackCompact_(
    const uint8_t* data,
    size_t byteSize,
    uint32_t frameCount,
    std::vector<Frame>& outFrames
) {
    outFrames.clear();

    if (frameCount > kAPXMaxFramesPerTrack_) return false;

    // compact stream needs at least one tag byte per frame
    if (frameCount > byteSize) return false;

    outFrames.reserve(static_cast<size_t>(frameCount));

    const uint8_t* cur = data;
    const uint8_t* end = data + byteSize;

    APXQuantState_ prev{};
    bool havePrev = false;

    for (uint32_t i = 0; i < frameCount; ++i) {
        uint8_t tag = 0;
        if (!readPod_(cur, end, tag)) return false;

        APXQuantState_ s{};

        if (tag & kAPXRec_Key_) {
            APXFrameKeyCompact rec{};
            if (!readPod_(cur, end, rec)) return false;

            s.xQ = rec.xQ;
            s.yQ = rec.yQ;
            s.rotQ = rec.rotQ;
            s.vehicleQ = rec.vehicleQ;
            s.waveQ = rec.waveQ;
            s.tQ = rec.tQ;
            s.mode = rec.mode;
            s.flags = rec.flags;
        } else {
            if (!havePrev) return false;

            APXFrameDeltaCompact rec{};
            if (!readPod_(cur, end, rec)) return false;

            s = prev;
            s.xQ += rec.dxQ;
            s.yQ += rec.dyQ;
            s.rotQ += rec.drotQ;
            s.tQ += rec.dtQ;

            if (tag & kAPXRec_Mode_) {
                if (!readPod_(cur, end, s.mode)) return false;
            }
            if (tag & kAPXRec_Flags_) {
                if (!readPod_(cur, end, s.flags)) return false;
            }
            if (tag & kAPXRec_Vehicle_) {
                if (!readPod_(cur, end, s.vehicleQ)) return false;
            }
            if (tag & kAPXRec_Wave_) {
                if (!readPod_(cur, end, s.waveQ)) return false;
            }
        }

        outFrames.push_back(makeFrame_(s));
        prev = s;
        havePrev = true;
    }

    return true;
}

static inline void finalizeLoadedAttempt_(Attempt& a) {
    if (!a.p1.empty()) {
        a.startX = static_cast<float>(a.p1.front().x);
        a.startY = static_cast<float>(a.p1.front().y);
        a.endX = static_cast<float>(a.p1.back().x);
    } else if (!a.p2.empty()) {
        a.startX = static_cast<float>(a.p2.front().x);
        a.startY = static_cast<float>(a.p2.front().y);
        a.endX = static_cast<float>(a.p2.back().x);
    } else {
        a.endX = a.startX;
    }

    if (!a.p2.empty()) a.hadDual = true;
    a.persistedOnDisk = true;
    a.recordedThisSession = false;
}

bool writeAPXPracticePath(std::ostream& out, const PracticePath& path) {
    if (!out.good()) return false;

    APXPathHeaderV2 hdr{};
    hdr.version = kAPXPathVersion_V5;  // Version 5 without saved checkpoints / active chain
    hdr.numSessions = static_cast<uint32_t>(path.sessions.size());
    hdr.activeSessionId = path.activeSessionId;
    hdr.selectedSessionId = path.selectedSessionId;
    hdr.frozen = path.frozen ? 1 : 0;

    const uint32_t type = kChunk_PATH;

    // Calculate total size
    uint32_t totalSize = sizeof(APXPathHeaderV2);
    for (const auto& session : path.sessions) {
        totalSize += sizeof(APXSessionHeaderV5_);
        totalSize += static_cast<uint32_t>(session.allAttemptSerials.size() * sizeof(int32_t));
        totalSize += static_cast<uint32_t>(session.segments.size() * sizeof(APXSegmentV5_));
    }

    out.write(reinterpret_cast<const char*>(&type), sizeof(type));
    out.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out.good()) return false;

    for (const auto& session : path.sessions) {
        APXSessionHeaderV5_ shdr{};
        shdr.sessionId = session.sessionId;
        shdr.numAttemptSerials = static_cast<uint32_t>(session.allAttemptSerials.size());
        shdr.numSegments = static_cast<uint32_t>(session.segments.size());
        shdr.startX = session.startX;
        shdr.startY = session.startY;
        shdr.endX = session.endX;
        shdr.completed = session.completed ? 1 : 0;
        shdr.frozen = session.frozen ? 1 : 0;

        out.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));

        // Write all attempt serials
        for (int32_t id : session.allAttemptSerials) {
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }

        // Write segments with time data
        for (const auto& seg : session.segments) {
            APXSegmentV5_ aseg{};
            aseg.startX = seg.startX;
            aseg.endX = seg.endX;
            aseg.ownerSerial = seg.ownerSerial;
            aseg.p1Frames = static_cast<int32_t>(seg.p1Frames);
            aseg.p2Frames = static_cast<int32_t>(seg.p2Frames);
            aseg.maxXReached = seg.maxXReached;
            aseg.baseTimeOffset = seg.baseTimeOffset;
            aseg.localStartTime = seg.localStartTime;
            aseg.localEndTime   = seg.localEndTime;

            out.write(reinterpret_cast<const char*>(&aseg), sizeof(aseg));
        }

        if (!out.good()) return false;
    }

    return true;
}

static inline bool canConsumeBytes_(uint64_t consumed, uint32_t chunkSize, uint64_t need) {
    return consumed <= static_cast<uint64_t>(chunkSize) &&
           need <= static_cast<uint64_t>(chunkSize) - consumed;
}

static inline bool addBytesChecked_(uint64_t& consumed, uint32_t chunkSize, uint64_t add) {
    if (!canConsumeBytes_(consumed, chunkSize, add)) return false;
    consumed += add;
    return true;
}

bool readAPXPracticePath(std::istream& in, uint32_t chunkSize, PracticePath& path, bool* outUsedLegacy) {
    path.clear();
    if (outUsedLegacy) *outUsedLegacy = false;

    if (chunkSize < sizeof(APXPathHeaderV2)) return false;
    if (chunkSize > kAPXMaxChunkSize_) return false;

    uint64_t consumed = 0;

    // Read header to determine version
    APXPathHeaderV2 hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    consumed += sizeof(hdr);

    if (hdr.numSessions > kAPXMaxPracticeSessions_) return false;

    path.activeSessionId = hdr.activeSessionId;
    path.selectedSessionId = hdr.selectedSessionId;
    path.frozen = (hdr.frozen != 0);

    const bool isV4 = (hdr.version == kAPXPathVersion_V4);
    const bool isV5 = (hdr.version == kAPXPathVersion_V5);

    if (!isV4 && !isV5) {
        log::warn("Unsupported Practice Session Format version {}. Please delete attempts save file", hdr.version);
        return false;
    }

    if (isV4 && outUsedLegacy) {
        *outUsedLegacy = true;
    }

    //log::info("[APX load] PATH version={}, sessions={}", hdr.version, hdr.numSessions);

    // Read sessions
    for (uint32_t s = 0; s < hdr.numSessions; ++s) {
        PracticeSession session;

        if (isV4) {
            APXSessionHeaderV4_ shdr{};
            in.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
            if (!in) return false;
            if (!addBytesChecked_(consumed, chunkSize, sizeof(shdr))) return false;

            // Bunch of safety stuff so we don't blow up
            if (shdr.numCheckpoints > kAPXMaxPracticeItemsPerSession_) return false;
            if (shdr.numActiveIDs > kAPXMaxPracticeItemsPerSession_) return false;
            if (shdr.numAttemptSerials > kAPXMaxPracticeItemsPerSession_) return false;
            if (shdr.numSegments > kAPXMaxPracticeItemsPerSession_) return false;

            const uint64_t checkpointBytes =
                static_cast<uint64_t>(shdr.numCheckpoints) * sizeof(APXCheckpointLegacy_);
            const uint64_t activeBytes =
                static_cast<uint64_t>(shdr.numActiveIDs) * sizeof(int32_t);
            const uint64_t serialBytes =
                static_cast<uint64_t>(shdr.numAttemptSerials) * sizeof(int32_t);
            const uint64_t segmentBytes =
                static_cast<uint64_t>(shdr.numSegments) * sizeof(APXSegmentV4_);

            const uint64_t sessionPayloadBytes =
                checkpointBytes + activeBytes + serialBytes + segmentBytes;

            if (!addBytesChecked_(consumed, chunkSize, sessionPayloadBytes)) return false;

            session.sessionId = shdr.sessionId;
            session.startX = shdr.startX;
            session.startY = shdr.startY;
            session.endX = shdr.endX;
            session.completed = (shdr.completed != 0);
            session.frozen = (shdr.frozen != 0);

            session.allAttemptSerials.reserve(static_cast<size_t>(shdr.numAttemptSerials));
            session.segments.reserve(static_cast<size_t>(shdr.numSegments));

            // Read and discard checkpoints (legacy)
            for (uint32_t i = 0; i < shdr.numCheckpoints; ++i) {
                APXCheckpointLegacy_ acheckpoint{};
                in.read(reinterpret_cast<char*>(&acheckpoint), sizeof(acheckpoint));
                if (!in) return false;
            }

            // Read and discard active chain (legacy)
            for (uint32_t i = 0; i < shdr.numActiveIDs; ++i) {
                int32_t id = 0;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
            }

            // Read all attempt serials
            for (uint32_t i = 0; i < shdr.numAttemptSerials; ++i) {
                int32_t id = 0;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.allAttemptSerials.push_back(id);
            }

            // Read segments
            for (uint32_t i = 0; i < shdr.numSegments; ++i) {
                APXSegmentV4_ aseg{};
                in.read(reinterpret_cast<char*>(&aseg), sizeof(aseg));
                if (!in) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;
                seg.p1Frames = static_cast<size_t>(std::max<int32_t>(0, aseg.p1Frames));
                seg.p2Frames = static_cast<size_t>(std::max<int32_t>(0, aseg.p2Frames));
                seg.maxXReached = aseg.maxXReached;
                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        } else { // V5
            APXSessionHeaderV5_ shdr{};
            in.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
            if (!in) return false;
            if (!addBytesChecked_(consumed, chunkSize, sizeof(shdr))) return false;

            // Bunch of safety stuff so we don't blow up
            if (shdr.numAttemptSerials > kAPXMaxPracticeItemsPerSession_) return false;
            if (shdr.numSegments > kAPXMaxPracticeItemsPerSession_) return false;

            const uint64_t serialBytes =
                static_cast<uint64_t>(shdr.numAttemptSerials) * sizeof(int32_t);
            const uint64_t segmentBytes =
                static_cast<uint64_t>(shdr.numSegments) * sizeof(APXSegmentV5_);

            const uint64_t sessionPayloadBytes =
                serialBytes + segmentBytes;

            if (!addBytesChecked_(consumed, chunkSize, sessionPayloadBytes)) return false;

            session.sessionId = shdr.sessionId;
            session.startX = shdr.startX;
            session.startY = shdr.startY;
            session.endX = shdr.endX;
            session.completed = (shdr.completed != 0);
            session.frozen = (shdr.frozen != 0);

            session.allAttemptSerials.reserve(static_cast<size_t>(shdr.numAttemptSerials));
            session.segments.reserve(static_cast<size_t>(shdr.numSegments));

            // Read all attempt serials
            for (uint32_t i = 0; i < shdr.numAttemptSerials; ++i) {
                int32_t id = 0;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.allAttemptSerials.push_back(id);
            }

            // Read segments
            for (uint32_t i = 0; i < shdr.numSegments; ++i) {
                APXSegmentV5_ aseg{};
                in.read(reinterpret_cast<char*>(&aseg), sizeof(aseg));
                if (!in) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;
                seg.p1Frames = static_cast<size_t>(std::max<int32_t>(0, aseg.p1Frames));
                seg.p2Frames = static_cast<size_t>(std::max<int32_t>(0, aseg.p2Frames));
                seg.maxXReached = aseg.maxXReached;
                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        }

        session.updateSpan();
        path.sessions.push_back(session);
    }

    // Skip any remaining bytes in chunk
    if (consumed != chunkSize) {
        if (consumed > chunkSize) return false;
        in.seekg(static_cast<std::streamoff>(chunkSize - consumed), std::ios::cur);
        if (!in) return false;
    }

    //log::info("[APX load] PATH loaded: {} sessions, version {}", path.sessions.size(), hdr.version);
    return true;
}

static bool skipChunkPayload_(std::istream& in, uint32_t sz) {
    if (sz == 0) return true;
    in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
    return static_cast<bool>(in);
}

static bool writeAPXFileCurrent_(
    std::filesystem::path const& path,
    std::vector<Attempt> const& attempts,
    PracticePath const& practicePath
) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        log::warn(
            "[SAVE FILE] failed to open file for write: {}",
            geode::utils::string::pathToString(path)
        );
        return false;
    }

    APXHeader h{};
    std::memcpy(h.magic, "APX2", 4);
    h.version = kAPXVersion;

    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (!out.good()) return false;

    for (auto const& attempt : attempts) {
        if (!writeAPXAttemptCompact(out, attempt)) {
            log::warn("[SAVE FILE] failed writing ATT3 chunk");
            return false;
        }
    }

    if (!writeAPXPracticePath(out, practicePath)) {
        log::warn("[SAVE FILE] failed writing PATH chunk");
        return false;
    }

    out.flush();
    return out.good();
}

static bool replaceFileAtomically_(
    std::filesystem::path const& tmpPath,
    std::filesystem::path const& finalPath
) {
    std::error_code ec;

    ec.clear();
    std::filesystem::rename(tmpPath, finalPath, ec);
    if (!ec) return true;

    // remove + rename fallback
    ec.clear();
    std::filesystem::remove(finalPath, ec);

    ec.clear();
    std::filesystem::rename(tmpPath, finalPath, ec);
    if (!ec) return true;

    log::warn(
        "[SAVE FILE] atomic replace failed: {} -> {} ({})",
        geode::utils::string::pathToString(tmpPath),
        geode::utils::string::pathToString(finalPath),
        ec.message()
    );
    return false;
}

static bool rewriteAPXFileToCurrent_(
    std::filesystem::path const& finalPath,
    std::vector<Attempt> const& attempts,
    PracticePath const& practicePath
) {
    auto tmpPath = finalPath;
    tmpPath += ".tmp";

    {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
    }

    if (!writeAPXFileCurrent_(tmpPath, attempts, practicePath)) {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    if (!replaceFileAtomically_(tmpPath, finalPath)) {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    return true;
}

bool loadAPXFileWithMigration(
    std::filesystem::path const& path,
    std::vector<Attempt>& attemptsOut,
    PracticePath& practicePathOut
) {
    attemptsOut.clear();
    practicePathOut.clear();

    std::error_code ec;
    ec.clear();
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        log::warn(
            "[SAVE FILE] exists failed for {}: {}",
            geode::utils::string::pathToString(path),
            ec.message()
        );
        return false;
    }

    if (!exists) {
        return true; // no file yet (none created)
    }

    ec.clear();
    const uintmax_t fsz = std::filesystem::file_size(path, ec);
    if (ec) {
        log::warn(
            "[SAVE FILE] file_size failed for {}: {}",
            geode::utils::string::pathToString(path),
            ec.message()
        );
        return false;
    }

    if (fsz < sizeof(APXHeader)) {
        log::warn(
            "[SAVE FILE] file too small to contain APX header: {}",
            geode::utils::string::pathToString(path)
        );
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        log::warn(
            "[SAVE FILE] failed to open file for read: {}",
            geode::utils::string::pathToString(path)
        );
        return false;
    }

    APXHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) {
        log::warn("[SAVE FILE] failed reading APX header");
        return false;
    }

    if (std::memcmp(hdr.magic, "APX2", 4) != 0) {
        log::warn(
            "[SAVE FILE] invalid APX header magic in {}",
            geode::utils::string::pathToString(path)
        );
        return false;
    }

    bool loadedLegacy = false;
    if (hdr.version < kAPXVersion) {
        loadedLegacy = true;
    } else if (hdr.version > kAPXVersion) {
        log::warn(
            "[SAVE FILE] unsupported future APX version {} in {}",
            hdr.version,
            geode::utils::string::pathToString(path)
        );
        return false;
    }

    bool sawPathChunk = false;

    while (true) {
        uint32_t chunkType = 0;
        uint32_t chunkSize = 0;

        in.read(reinterpret_cast<char*>(&chunkType), sizeof(chunkType));
        if (!in) {
            if (in.eof()) break;
            return false;
        }

        in.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (!in) return false;

        if (chunkSize > kAPXMaxChunkSize_) {
            log::warn("[SAVE FILE] chunk size too large: {}", chunkSize);
            return false;
        }

        switch (chunkType) {
            case kChunk_ATT3: {
                Attempt a;
                bool usedLegacy = false;
                if (!readAPXAttemptCompact(in, chunkSize, a, &usedLegacy)) {
                    log::warn("[SAVE FILE] failed reading ATT3 chunk");
                    return false;
                }
                loadedLegacy = loadedLegacy || usedLegacy;
                attemptsOut.push_back(std::move(a));
                break;
            }

            case kChunk_PATH: {
                PracticePath tmpPath;
                bool usedLegacy = false;
                if (!readAPXPracticePath(in, chunkSize, tmpPath, &usedLegacy)) {
                    log::warn("[SAVE FILE] failed reading PATH chunk");
                    return false;
                }
                loadedLegacy = loadedLegacy || usedLegacy;
                practicePathOut = std::move(tmpPath);
                sawPathChunk = true;
                break;
            }

            case kChunk_CheckpointZ2: {
                // Legacy checkpoint chain chunk
                loadedLegacy = true;
                if (!skipChunkPayload_(in, chunkSize)) return false;
                break;
            }

            case kChunk_SEGZ: {
                // Legacy segment chunk
                loadedLegacy = true;
                if (!skipChunkPayload_(in, chunkSize)) return false;
                break;
            }

            case kChunk_ATTZ: {
                // Legacy attempt chunk
                loadedLegacy = true;
                if (!skipChunkPayload_(in, chunkSize)) return false;
                break;
            }

            default: {
                // Unknown chunk
                if (!skipChunkPayload_(in, chunkSize)) return false;
                break;
            }
        }
    }

    if (!sawPathChunk) {
        practicePathOut.clear();
    }

    if (loadedLegacy) {
        if (!rewriteAPXFileToCurrent_(path, attemptsOut, practicePathOut)) {
            log::warn(
                "[SAVE FILE] loaded legacy file but failed to rewrite it to current format: {}",
                geode::utils::string::pathToString(path)
            );
        }
    }

    return true;
}

bool saveAPXFileCurrent(
    std::filesystem::path const& path,
    std::vector<Attempt> const& attempts,
    PracticePath const& practicePath
) {
    std::error_code ec;
    const auto dir = path.parent_path();

    if (!dir.empty()) {
        ec.clear();
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            log::warn(
                "[SAVE FILE] create_directories failed: {} ({})",
                geode::utils::string::pathToString(dir),
                ec.message()
            );
            return false;
        }
    }

    return rewriteAPXFileToCurrent_(path, attempts, practicePath);
}

bool writeAPXAttemptCompact(std::ostream& out, Attempt const& attempt) {
    if (!out.good()) return false;

    const std::vector<uint8_t> p1Bytes = encodeTrackCompact_(attempt.p1);
    const std::vector<uint8_t> p2Bytes = encodeTrackCompact_(attempt.p2);

    APXMetaCompactPrefix_ prefix{};
    prefix.serial = static_cast<uint32_t>(attempt.serial);
    prefix.startPercent = attempt.startPercent;
    prefix.endPercent = attempt.endPercent;
    prefix.flags = attemptFlagsFromAttempt_(attempt);
    prefix.streamVersion = kAPXCompactAttemptVersion_V2;
    prefix.p1Count = static_cast<uint32_t>(attempt.p1.size());
    prefix.p2Count = static_cast<uint32_t>(attempt.p2.size());
    prefix.startX = attempt.startX;
    prefix.startY = attempt.startY;

    APXMetaCompactV2Tail_ tail{};
    tail.baseTimeOffset = attempt.baseTimeOffset;
    tail.seed = static_cast<uint64_t>(attempt.seed);

    APXTrackHeaderCompact p1Hdr{};
    p1Hdr.byteSize = static_cast<uint32_t>(p1Bytes.size());

    APXTrackHeaderCompact p2Hdr{};
    p2Hdr.byteSize = static_cast<uint32_t>(p2Bytes.size());

    const uint32_t payloadSz =
        static_cast<uint32_t>(sizeof(prefix)) +
        static_cast<uint32_t>(sizeof(tail)) +
        static_cast<uint32_t>(sizeof(APXTrackHeaderCompact)) + p1Hdr.byteSize +
        static_cast<uint32_t>(sizeof(APXTrackHeaderCompact)) + p2Hdr.byteSize;

    const uint32_t type = kChunk_ATT3;

    out.write(reinterpret_cast<const char*>(&type), sizeof(type));
    out.write(reinterpret_cast<const char*>(&payloadSz), sizeof(payloadSz));
    out.write(reinterpret_cast<const char*>(&prefix), sizeof(prefix));
    out.write(reinterpret_cast<const char*>(&tail), sizeof(tail));
    out.write(reinterpret_cast<const char*>(&p1Hdr), sizeof(p1Hdr));
    if (!p1Bytes.empty()) {
        out.write(reinterpret_cast<const char*>(p1Bytes.data()), static_cast<std::streamsize>(p1Bytes.size()));
    }
    out.write(reinterpret_cast<const char*>(&p2Hdr), sizeof(p2Hdr));
    if (!p2Bytes.empty()) {
        out.write(reinterpret_cast<const char*>(p2Bytes.data()), static_cast<std::streamsize>(p2Bytes.size()));
    }

    return out.good();
}

bool readAPXAttemptCompact(std::istream& in, uint32_t chunkSize, Attempt& attempt, bool* outUsedLegacy) {
    attempt = Attempt{};
    if (outUsedLegacy) *outUsedLegacy = false;

    if (chunkSize < sizeof(APXMetaCompactPrefix_) + sizeof(APXTrackHeaderCompact) + sizeof(APXTrackHeaderCompact)) {
        return false;
    }
    if (chunkSize > kAPXMaxChunkSize_) {
        return false;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(chunkSize));
    if (chunkSize != 0) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!in) return false;
    }

    const uint8_t* cur = buf.data();
    const uint8_t* end = buf.data() + buf.size();

    APXMetaCompactPrefix_ prefix{};
    if (!readPod_(cur, end, prefix)) return false;

    if (prefix.p1Count > kAPXMaxFramesPerTrack_ || prefix.p2Count > kAPXMaxFramesPerTrack_) {
        return false;
    }

    attempt.serial = static_cast<int>(prefix.serial);
    attempt.startPercent = prefix.startPercent;
    attempt.endPercent = prefix.endPercent;
    attempt.startX = prefix.startX;
    attempt.startY = prefix.startY;
    applyAttemptFlags_(prefix.flags, attempt);

    if (prefix.streamVersion == kAPXCompactAttemptVersion_V1) {
        APXMetaCompactV1Tail_ tail{};
        if (!readPod_(cur, end, tail)) return false;

        attempt.baseTimeOffset = tail.baseTimeOffset;
        attempt.seed = static_cast<uintptr_t>(tail.seed);

        if (outUsedLegacy) *outUsedLegacy = true;
    } else if (prefix.streamVersion == kAPXCompactAttemptVersion_V2) {
        APXMetaCompactV2Tail_ tail{};
        if (!readPod_(cur, end, tail)) return false;

        attempt.baseTimeOffset = tail.baseTimeOffset;
        attempt.seed = static_cast<uintptr_t>(tail.seed);
    } else {
        return false;
    }

    APXTrackHeaderCompact p1Hdr{};
    if (!readPod_(cur, end, p1Hdr)) return false;
    if (p1Hdr.byteSize > static_cast<uint32_t>(end - cur)) return false;
    if (prefix.p1Count > p1Hdr.byteSize) return false; // at least 1 tag byte per frame
    if (!decodeTrackCompact_(cur, p1Hdr.byteSize, prefix.p1Count, attempt.p1)) return false;
    cur += p1Hdr.byteSize;

    APXTrackHeaderCompact p2Hdr{};
    if (!readPod_(cur, end, p2Hdr)) return false;
    if (p2Hdr.byteSize > static_cast<uint32_t>(end - cur)) return false;
    if (prefix.p2Count > p2Hdr.byteSize) return false; // at least 1 tag byte per frame
    if (!decodeTrackCompact_(cur, p2Hdr.byteSize, prefix.p2Count, attempt.p2)) return false;
    cur += p2Hdr.byteSize;

    // catch evil dubious corrupted chunk sizes
    if (cur != end) return false;

    finalizeLoadedAttempt_(attempt);
    return true;
}

