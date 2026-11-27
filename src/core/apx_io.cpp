#include <filesystem>
#include <system_error>
#include <cstring>
#include <cstdint>
#include <climits>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#include <array>
#include "apx_catalog_cache.hpp"
#include "types.hpp"
#include "apx_format.hpp"
#include "apx_io.hpp"

static constexpr double kAPXPosScale_  = 64.0;   // 1/64 px
static constexpr double kAPXRotScale_  = 16.0;   // 1/16 deg
static constexpr double kAPXSizeScale_ = 256.0;  // 1/256
static constexpr double kAPXTimeScale_ = 48000;  // up to 48k Hz (so any large weird physics update people set won't look jittery)
static constexpr size_t kAPXKeyEvery_  = 64;
static constexpr uint32_t kAPXMaxChunkSize_ = apxio::kMaxChunkBytes;
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

// Stuff for only loading light thingies for each attempt and loading the full attempt on preload
struct APXTrackSummary_ {
    bool hasFrames = false;
    int32_t firstXQ = 0;
    int32_t lastXQ = 0;
    uint32_t lastTQ = 0;
};

class ChunkReader_ {
    apxio::Reader& in;
    uint64_t unread;
    std::array<uint8_t, 32 * 1024> buffer{};
    size_t cursor = 0, filled = 0;
public:
    ChunkReader_(apxio::Reader& source, uint32_t bytes) : in(source), unread(bytes) {}

    uint64_t remaining() const { return unread + filled - cursor; }

    template<class T> bool read(T& value) {
        if (sizeof(T) > remaining()) return false;
        auto* dest = reinterpret_cast<uint8_t*>(&value);
        size_t left = sizeof(T);
        while (left) {
            if (cursor == filled) {
                filled = static_cast<size_t>(std::min<uint64_t>(unread, buffer.size()));
                cursor = 0;
                if (!filled) return false;
                in.read(reinterpret_cast<char*>(buffer.data()), filled);
                if (!in) return false;
                unread -= filled;
            }

            const size_t n = std::min(left, filled - cursor);
            std::memcpy(dest, buffer.data() + cursor, n);
            dest += n;
            cursor += n;
            left -= n;
        }
        return true;
    }
};

static bool validMode_(uint8_t mode) {
    switch (static_cast<IconType>(mode)) {
        case IconType::Cube: case IconType::Ship: case IconType::Ball:
        case IconType::Ufo: case IconType::Wave: case IconType::Robot:
        case IconType::Spider: case IconType::Swing: case IconType::Jetpack: return true;
        default: return false;
    }
}

static bool validMetadata_(APXMetaCompactPrefix_ const& p, double base) {
    return p.serial < uint32_t(INT_MAX) && (p.flags & ~7u) == 0 &&
        std::isfinite(p.startPercent) && std::isfinite(p.endPercent) &&
        std::isfinite(p.startX) && std::isfinite(p.startY) && std::isfinite(base);
}

static bool addDelta_(int32_t& target, int16_t delta) {
    const int64_t n = int64_t(target) + delta;
    if (n < INT32_MIN || n > INT32_MAX) return false;
    target = static_cast<int32_t>(n);
    return true;
}

static bool readTrack_(ChunkReader_& in, uint32_t count, APXTrackSummary_& summary, std::vector<Frame>* frames) {
    APXTrackHeaderCompact hdr{};
    if (!in.read(hdr) || hdr.byteSize > in.remaining() || uint64_t(count) * (1 + sizeof(APXFrameDeltaCompact)) > hdr.byteSize) return false;
    uint64_t left = hdr.byteSize;

    auto read = [&](auto& v) {
        if (sizeof(v) > left || !in.read(v)) return false;
        left -= sizeof(v);
        return true;
    };

    if (frames) frames->reserve(count);

    APXQuantState_ s{};

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t tag{};
        if (!read(tag) || (tag & ~uint8_t(31))) return false;
        if (tag & kAPXRec_Key_) {
            if (tag != kAPXRec_Key_) return false;
            APXFrameKeyCompact rec{};
            if (!read(rec)) return false;
            s.xQ = rec.xQ;
            s.yQ = rec.yQ;
            s.rotQ = rec.rotQ;
            s.vehicleQ = rec.vehicleQ;
            s.waveQ = rec.waveQ;
            s.tQ = rec.tQ;
            s.mode = rec.mode;
            s.flags = rec.flags;
        } else {
            APXFrameDeltaCompact rec{};
            if (!i || !read(rec) || !addDelta_(s.xQ, rec.dxQ) || !addDelta_(s.yQ, rec.dyQ) || !addDelta_(s.rotQ, rec.drotQ) || rec.dtQ > UINT32_MAX - s.tQ) return false;
            s.tQ += rec.dtQ;
            if ((tag & kAPXRec_Mode_) && !read(s.mode)) return false;
            if ((tag & kAPXRec_Flags_) && !read(s.flags)) return false;
            if ((tag & kAPXRec_Vehicle_) && !read(s.vehicleQ)) return false;
            if ((tag & kAPXRec_Wave_) && !read(s.waveQ)) return false;
        }

        if (!validMode_(s.mode) || (s.flags & 0x80u)) return false;

        if (!i) {
            summary.hasFrames = true;
            summary.firstXQ = s.xQ;
        }

        summary.lastXQ = s.xQ;
        summary.lastTQ = s.tQ;
        if (frames) frames->push_back(makeFrame_(s));
    }
    return left == 0;
}

static bool readCompactRecord_(apxio::Reader& source, uint32_t size,
                               APXAttemptDiskInfo& entry, Attempt* decoded, bool* legacy) {
    if (legacy) *legacy = false;
    if (size > source.remaining() || size > kAPXMaxChunkSize_) return false;
    ChunkReader_ in(source, size);
    APXMetaCompactPrefix_ prefix{};
    APXMetaCompactV2Tail_ tail{};

    if (!in.read(prefix) || prefix.p1Count > kAPXMaxFramesPerTrack_ || prefix.p2Count > kAPXMaxFramesPerTrack_) return false;

    if (prefix.streamVersion == kAPXCompactAttemptVersion_V1) {
        APXMetaCompactV1Tail_ old{};
        if (!in.read(old)) return false;
        tail.baseTimeOffset = old.baseTimeOffset;
        tail.seed = old.seed;
        if (legacy) *legacy = true;
    } else if (prefix.streamVersion != kAPXCompactAttemptVersion_V2 || !in.read(tail)) return false;

    if (!validMetadata_(prefix, tail.baseTimeOffset)) return false;
    if ((uint64_t(prefix.p1Count) + prefix.p2Count) * sizeof(Frame) > apxio::kMaxDecodedFrameBytes) return false;

    APXTrackSummary_ p1{}, p2{};

    if (!readTrack_(in, prefix.p1Count, p1, decoded ? &decoded->p1 : nullptr) ||
        !readTrack_(in, prefix.p2Count, p2, decoded ? &decoded->p2 : nullptr) || in.remaining()) return false;
    
    entry.serial = static_cast<int>(prefix.serial);
    entry.startPercent = prefix.startPercent;
    entry.endPercent = prefix.endPercent;
    entry.startX = prefix.startX;
    entry.startY = prefix.startY;
    entry.practiceAttempt = (prefix.flags & 1) != 0;
    entry.completed = (prefix.flags & 2) != 0;
    entry.hadDual = (prefix.flags & 4) != 0;
    entry.baseTimeOffset = tail.baseTimeOffset;
    entry.seed = tail.seed;
    entry.p1Count = prefix.p1Count;
    entry.p2Count = prefix.p2Count;

    const auto& summary = p1.hasFrames ? p1 : p2;
    entry.endX = summary.hasFrames ? dequantPos_(summary.lastXQ) : entry.startX;
    entry.endTime = summary.hasFrames ? dequantTime_(summary.lastTQ) : 0.0;

    if (decoded) {
        decoded->serial = entry.serial;
        decoded->startPercent = entry.startPercent;
        decoded->endPercent = entry.endPercent;
        decoded->startX = entry.startX;
        decoded->startY = entry.startY;
        decoded->baseTimeOffset = entry.baseTimeOffset;
        decoded->seed = entry.seed;
        applyAttemptFlags_(prefix.flags, *decoded);
    }
    return true;
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

void clearAPXWavePointThisFrameBits(Attempt& attempt) {
    for (auto& f : attempt.p1) {
        f.wavePointThisFrame = false;
    }

    for (auto& f : attempt.p2) {
        f.wavePointThisFrame = false;
    }
}

void clearAPXWavePointThisFrameBits(std::vector<Attempt>& attempts) {
    for (auto& attempt : attempts) {
        clearAPXWavePointThisFrameBits(attempt);
    }
}

static inline void clearWavePointThisFrameBits_(Attempt& a) {
    for (auto& f : a.p1) {
        f.wavePointThisFrame = false;
    }

    for (auto& f : a.p2) {
        f.wavePointThisFrame = false;
    }
}

static inline void clearWavePointThisFrameBits_(std::vector<Attempt>& attempts) {
    for (auto& a : attempts) {
        clearWavePointThisFrameBits_(a);
    }
}

bool writeAPXPracticePath(apxio::Writer& out, const PracticePath& path) {
    if (!out.good()) return false;

    APXPathHeaderV2 hdr{};
    hdr.version = kAPXPathVersion_V5;
    hdr.numSessions = static_cast<uint32_t>(path.sessions.size());
    hdr.activeSessionId = path.activeSessionId;
    hdr.selectedSessionId = path.selectedSessionId;
    hdr.frozen = path.frozen ? 1 : 0;

    const uint32_t type = kChunk_PATH;

    // Calculate total size
    if (path.sessions.size() > kAPXMaxPracticeSessions_) return false;
    uint64_t totalSize64 = sizeof(APXPathHeaderV2);

    for (const auto& session : path.sessions) {
        if (session.sessionId < 0 || session.sessionId >= INT_MAX || !std::isfinite(session.startX) || !std::isfinite(session.startY) || !std::isfinite(session.endX) || session.allAttemptSerials.size() > kAPXMaxPracticeItemsPerSession_ || session.segments.size() > kAPXMaxPracticeItemsPerSession_) return false;

        totalSize64 += sizeof(APXSessionHeaderV5_) +
            uint64_t(session.allAttemptSerials.size()) * sizeof(int32_t) +
            uint64_t(session.segments.size()) * sizeof(APXSegmentV5_);
        
        if (totalSize64 > kAPXMaxChunkSize_) return false;
    }

    const uint32_t totalSize = static_cast<uint32_t>(totalSize64);
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
            if (seg.p1Frames > INT32_MAX || seg.p2Frames > INT32_MAX ||
                !std::isfinite(seg.startX) || !std::isfinite(seg.endX) || !std::isfinite(seg.maxXReached) ||
                !std::isfinite(seg.baseTimeOffset) || !std::isfinite(seg.localStartTime) || !std::isfinite(seg.localEndTime)) return false;
            
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

static bool readAPXPracticePathImpl_(apxio::Reader& in, uint32_t chunkSize, PracticePath& path, bool* outUsedLegacy) {
    path.clear();
    if (outUsedLegacy) *outUsedLegacy = false;

    if (chunkSize < sizeof(APXPathHeaderV2)) return false;
    if (chunkSize > kAPXMaxChunkSize_ || chunkSize > in.remaining()) return false;

    uint64_t consumed = 0;

    // Read header to determine version
    APXPathHeaderV2 hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    consumed += sizeof(hdr);

    if (hdr.numSessions > kAPXMaxPracticeSessions_ || hdr.numSessions > (chunkSize - sizeof(hdr)) / sizeof(APXSessionHeaderV5_)) return false;

    path.activeSessionId = hdr.activeSessionId;
    path.selectedSessionId = hdr.selectedSessionId;
    path.frozen = (hdr.frozen != 0);

    const bool isV4 = (hdr.version == kAPXPathVersion_V4);
    const bool isV5 = (hdr.version == kAPXPathVersion_V5);

    if (!isV4 && !isV5) {
        log::warn("Unsupported Practice Session Format version {}. Original recording preserved", hdr.version);
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
            if (!canConsumeBytes_(consumed, chunkSize, sizeof(shdr))) return false;
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

            if (shdr.sessionId < 0 || shdr.sessionId >= INT_MAX || !std::isfinite(shdr.startX) || !std::isfinite(shdr.startY) || !std::isfinite(shdr.endX)) return false;

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

                if (!std::isfinite(aseg.startX) || !std::isfinite(aseg.endX) || !std::isfinite(aseg.maxXReached) || !std::isfinite(aseg.baseTimeOffset) || !std::isfinite(aseg.localStartTime) || !std::isfinite(aseg.localEndTime)) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;

                seg.p1Frames = static_cast<size_t>(std::max<int32_t>(0, int32_t(aseg.p1Frames)));
                seg.p2Frames = static_cast<size_t>(std::max<int32_t>(0, int32_t(aseg.p2Frames)));

                seg.maxXReached = aseg.maxXReached;
                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        } else { // V5
            APXSessionHeaderV5_ shdr{};
            if (!canConsumeBytes_(consumed, chunkSize, sizeof(shdr))) return false;
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

            if (shdr.sessionId < 0 || shdr.sessionId >= INT_MAX || !std::isfinite(shdr.startX) || !std::isfinite(shdr.startY) || !std::isfinite(shdr.endX)) return false;

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

                if (!std::isfinite(aseg.startX) || !std::isfinite(aseg.endX) || !std::isfinite(aseg.maxXReached) || !std::isfinite(aseg.baseTimeOffset) || !std::isfinite(aseg.localStartTime) || !std::isfinite(aseg.localEndTime)) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;

                seg.p1Frames = static_cast<size_t>(std::max<int32_t>(0, int32_t(aseg.p1Frames)));
                seg.p2Frames = static_cast<size_t>(std::max<int32_t>(0, int32_t(aseg.p2Frames)));

                seg.maxXReached = aseg.maxXReached;
                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        }

        session.updateSpan();
        path.sessions.push_back(std::move(session));
    }

    if (consumed != chunkSize) return false;

    //log::info("[APX load] PATH loaded: {} sessions, version {}", path.sessions.size(), hdr.version);
    return true;
}

bool readAPXPracticePath(apxio::Reader& in, uint32_t size, PracticePath& path, bool* legacy) {
    PracticePath decoded;
    bool old = false;
    if (!readAPXPracticePathImpl_(in, size, decoded, &old)) return false;
    path = std::move(decoded);
    if (legacy) *legacy = old;
    return true;
}

static bool skipChunkPayload_(apxio::Reader& in, uint32_t sz) {
    if (sz == 0) return true;
    in.seekg(static_cast<int64_t>(sz), apxio::Origin::Current);
    return static_cast<bool>(in);
}

bool replaceAPXFileSafely_(
    std::filesystem::path const& tmpPath,
    std::filesystem::path const& finalPath
) {
    std::error_code ec;
    std::filesystem::rename(tmpPath, finalPath, ec);
    if (ec) {
        log::error("[APX replace] {} -> {}: {}",
            attemptplayback::pathcompat::toUtf8(tmpPath),
            attemptplayback::pathcompat::toUtf8(finalPath), ec.message());
        return false;
    }
    auto index = finalPath;
    index += ".idx";
    std::filesystem::remove(index, ec);
    return true;
}

static bool writeAPXFileCurrent_(
    std::filesystem::path const& path,
    std::vector<Attempt> const& attempts,
    PracticePath const& practicePath
) {
    apxio::Writer out(path, apxio::Mode::Truncate);
    if (!out) {
        log::error("[APX write] could not open temporary recording {}", attemptplayback::pathcompat::toUtf8(path));
        return false;
    }

    APXHeader h{};
    h.version = kAPXVersion;
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    for (auto const& attempt : attempts) {
        if (!writeAPXAttemptCompact(out, attempt)) return false;
    }
    return writeAPXPracticePath(out, practicePath);
}


static bool readLegacyAttempt_(apxio::Reader& in, uint32_t size, Attempt& attempt) {
    APXMeta meta{};
    if (size < sizeof(meta)) return false;
    in.read(reinterpret_cast<char*>(&meta), sizeof(meta));
    const uint64_t count = uint64_t(meta.p1Count) + meta.p2Count;
    if (!in || meta.serial >= uint32_t(INT_MAX) ||
        count * sizeof(APXFrame) > size - sizeof(meta) ||
        count * sizeof(Frame) > apxio::kMaxDecodedFrameBytes) return false;
    if (!std::isfinite(meta.startX) || !std::isfinite(meta.startY) ||
        !std::isfinite(meta.startPercent) || !std::isfinite(meta.endPercent) ||
        !std::isfinite(meta.baseTimeOffset)) return false;

    attempt.serial = static_cast<int>(meta.serial);
    attempt.startPercent = meta.startPercent;
    attempt.endPercent = meta.endPercent;
    attempt.startX = meta.startX;
    attempt.startY = meta.startY;
    attempt.baseTimeOffset = meta.baseTimeOffset;
    attempt.seed = meta.seed;
    applyAttemptFlags_(meta.flags, attempt);

    auto readFrames = [&](std::vector<Frame>& frames, uint32_t n) {
        frames.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            APXFrame stored{};
            in.read(reinterpret_cast<char*>(&stored), sizeof(stored));
            if (!in || !validMode_(stored.mode) || !std::isfinite(stored.x) ||
                !std::isfinite(stored.y) || !std::isfinite(stored.rot) ||
                !std::isfinite(stored.vehicleSize) || !std::isfinite(stored.waveSize) ||
                !std::isfinite(stored.t) || std::abs(double(stored.x)) * kAPXPosScale_ > INT32_MAX ||
                std::abs(double(stored.y)) * kAPXPosScale_ > INT32_MAX ||
                std::abs(double(stored.rot)) * kAPXRotScale_ > INT32_MAX ||
                stored.t > UINT32_MAX / kAPXTimeScale_ || stored.t < 0) return false;
            Frame frame{};
            frame.x = stored.x;
            frame.y = stored.y;
            frame.rot = stored.rot;
            frame.vehicleSize = stored.vehicleSize;
            frame.waveSize = stored.waveSize;
            frame.mode = static_cast<IconType>(stored.mode);
            frame.t = stored.t;
            frameFromFlags_(stored, frame);
            frames.push_back(frame);
        }
        return true;
    };
    if (!readFrames(attempt.p1, meta.p1Count) || !readFrames(attempt.p2, meta.p2Count)) return false;
    if (!skipChunkPayload_(in, size - sizeof(meta) - count * sizeof(APXFrame))) return false;
    finalizeLoadedAttempt_(attempt);
    return true;
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

    if (!replaceAPXFileSafely_(tmpPath, finalPath)) {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    return true;
}

bool loadAPXFileWithMigration(
    std::filesystem::path const& path,
    std::vector<Attempt>& attemptsOut,
    PracticePath& practicePathOut,
    bool rewriteLegacy
) {
    attemptsOut.clear();
    practicePathOut.clear();
    if (path.empty()) return false;

    std::error_code ec;
    ec.clear();
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        log::warn(
            "[SAVE FILE] exists failed for {}: {}",
            attemptplayback::pathcompat::toUtf8(path),
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
            attemptplayback::pathcompat::toUtf8(path),
            ec.message()
        );
        return false;
    }

    if (fsz < sizeof(APXHeader)) {
        log::warn(
            "[SAVE FILE] file too small to contain APX header: {}",
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    apxio::Reader in(path, apxio::Mode::Read);
    if (!in) {
        log::warn(
            "[SAVE FILE] failed to open file for read: {}",
            attemptplayback::pathcompat::toUtf8(path)
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
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    bool loadedLegacy = false;

    const bool oldWavePointBitMeaning =
        hdr.version < kAPXVersion_WavePointThisFrame;

    if (hdr.version < kAPXVersion) {
        loadedLegacy = true;
    } else if (hdr.version > kAPXVersion) {
        log::warn(
            "[SAVE FILE] unsupported future APX version {} in {}",
            hdr.version,
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    bool sawPathChunk = false;
    uint64_t frameBytes = 0;

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

        if (chunkSize > kAPXMaxChunkSize_ || chunkSize > in.remaining()) {
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
                frameBytes += (uint64_t(a.p1.size()) + a.p2.size()) * sizeof(Frame);
                if (frameBytes > apxio::kMaxBulkFrameBytes) {
                    log::error("[APX load] full-file operation exceeds memory limit");
                    return false;
                }
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
                Attempt a;
                if (!readLegacyAttempt_(in, chunkSize, a)) {
                    log::error("[APX load] invalid legacy ATTZ chunk");
                    return false;
                }
                loadedLegacy = true;
                frameBytes += (uint64_t(a.p1.size()) + a.p2.size()) * sizeof(Frame);
                if (frameBytes > apxio::kMaxBulkFrameBytes) return false;
                attemptsOut.push_back(std::move(a));
                break;
            }

            default: {
                log::error("[APX load] unsupported chunk type {}; file left unchanged", chunkType);
                return false;
            }
        }
    }

    if (!sawPathChunk) {
        practicePathOut.clear();
    }

    if (oldWavePointBitMeaning) {
        clearWavePointThisFrameBits_(attemptsOut);
        loadedLegacy = true;
    }

    in.close();
    if (loadedLegacy && rewriteLegacy) {
        if (!rewriteAPXFileToCurrent_(path, attemptsOut, practicePathOut)) {
            log::warn(
                "[SAVE FILE] loaded legacy file but failed to rewrite it to current format: {}",
                attemptplayback::pathcompat::toUtf8(path)
            );
            return false;
        }
    }

    return true;
}

static bool serializeAPXCatalogIndex_(APXCatalogScanResult const& scan, std::vector<uint8_t>& payload) {
    if (scan.attempts.size() > apxio::kMaxCatalogEntries) return false;

    payload.clear();
    const uint32_t version = 1;
    const uint32_t count = static_cast<uint32_t>(scan.attempts.size());
    appendPod_(payload, version);
    appendPod_(payload, count);
    appendPod_(payload, scan.maxSerialSeen);

    for (auto const& a : scan.attempts) {
        const int32_t serial = a.serial;
        const uint8_t flags = (a.practiceAttempt ? 1 : 0) |
            (a.completed ? 2 : 0) | (a.hadDual ? 4 : 0);
        const uint64_t seed = static_cast<uint64_t>(a.seed);
        appendPod_(payload, serial);
        appendPod_(payload, a.startPercent);
        appendPod_(payload, a.endPercent);
        appendPod_(payload, flags);
        appendPod_(payload, a.startX);
        appendPod_(payload, a.startY);
        appendPod_(payload, a.endX);
        appendPod_(payload, a.endTime);
        appendPod_(payload, a.baseTimeOffset);
        appendPod_(payload, seed);
        appendPod_(payload, a.p1Count);
        appendPod_(payload, a.p2Count);
        appendPod_(payload, a.chunkFileOffset);
        appendPod_(payload, a.chunkSize);
    }

    apxio::Writer stream(payload, apxio::kMaxCatalogPayload);
    if (!writeAPXPracticePath(stream, scan.practicePath)) return false;
    return true;
}

static bool deserializeAPXCatalogIndex_(
    std::vector<uint8_t> const& payload,
    uint64_t apxSize,
    APXCatalogScanResult& out
) {
    if (payload.size() < 20) return false;
    const uint8_t* cur = payload.data();
    const uint8_t* end = cur + payload.size();

    uint32_t version = 0, count = 0, maxSerial = 0;
    if (!readPod_(cur, end, version) || version != 1 ||
        !readPod_(cur, end, count) || count > apxio::kMaxCatalogEntries ||
        !readPod_(cur, end, maxSerial) || count > payload.size() / 64) return false;

    APXCatalogScanResult decoded{};
    decoded.attempts.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        APXAttemptDiskInfo a{};
        int32_t serial = 0;
        uint8_t flags = 0;
        uint64_t seed = 0;
        if (!readPod_(cur, end, serial) ||
            !readPod_(cur, end, a.startPercent) ||
            !readPod_(cur, end, a.endPercent) ||
            !readPod_(cur, end, flags) || (flags & ~7u) ||
            !readPod_(cur, end, a.startX) ||
            !readPod_(cur, end, a.startY) ||
            !readPod_(cur, end, a.endX) ||
            !readPod_(cur, end, a.endTime) ||
            !readPod_(cur, end, a.baseTimeOffset) ||
            !readPod_(cur, end, seed) ||
            !readPod_(cur, end, a.p1Count) ||
            !readPod_(cur, end, a.p2Count) ||
            !readPod_(cur, end, a.chunkFileOffset) ||
            !readPod_(cur, end, a.chunkSize)) return false;

        a.serial = serial;
        a.practiceAttempt = (flags & 1u) != 0;
        a.completed = (flags & 2u) != 0;
        a.hadDual = (flags & 4u) != 0;
        a.seed = seed;
        if (a.serial < 0 || a.serial >= INT_MAX || maxSerial >= static_cast<uint32_t>(INT_MAX) ||
            a.chunkSize > kAPXMaxChunkSize_ ||
            a.p1Count > kAPXMaxFramesPerTrack_ || a.p2Count > kAPXMaxFramesPerTrack_ ||
            (uint64_t(a.p1Count) + a.p2Count) * sizeof(Frame) > apxio::kMaxDecodedFrameBytes ||
            a.chunkFileOffset < sizeof(APXHeader) ||
            a.chunkFileOffset > apxSize ||
            apxSize - a.chunkFileOffset < sizeof(uint32_t) * 2 ||
            a.chunkSize > apxSize - a.chunkFileOffset - sizeof(uint32_t) * 2) return false;
        decoded.attempts.push_back(a);
    }

    const std::span<const uint8_t> remaining(cur, end);
    apxio::Reader stream(remaining);
    uint32_t type = 0, size = 0;
    stream.read(reinterpret_cast<char*>(&type), sizeof(type));
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!stream || type != kChunk_PATH ||
        size != remaining.size() - sizeof(type) - sizeof(size) ||
        !readAPXPracticePath(stream, size, decoded.practicePath)) return false;

    decoded.maxSerialSeen = maxSerial;
    out = std::move(decoded);
    return true;
}

bool scanAPXFileCatalog(
    std::filesystem::path const& path,
    APXCatalogScanResult& out
) {
    out = APXCatalogScanResult{};
    if (path.empty()) return false;

    const auto scanStart = std::chrono::steady_clock::now();
    std::vector<uint8_t> cachedPayload;
    if (loadAPXCatalogCache(path, cachedPayload)) {
        std::error_code cacheEc;
        const auto apxSize = std::filesystem::file_size(path, cacheEc);
        if (!cacheEc && deserializeAPXCatalogIndex_(cachedPayload, apxSize, out)) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - scanStart).count();
            log::info("[APX catalog] index hit: {} attempts in {} ms", out.attempts.size(), elapsed);
            return true;
        }
    }

    std::error_code ec;
    ec.clear();
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        log::warn(
            "[SAVE FILE] exists failed for {}: {}",
            attemptplayback::pathcompat::toUtf8(path),
            ec.message()
        );
        return false;
    }

    if (!exists) {
        return true;
    }

    ec.clear();
    const uintmax_t fsz = std::filesystem::file_size(path, ec);
    if (ec) {
        log::warn(
            "[SAVE FILE] file_size failed for {}: {}",
            attemptplayback::pathcompat::toUtf8(path),
            ec.message()
        );
        return false;
    }

    if (fsz < sizeof(APXHeader)) {
        log::warn(
            "[SAVE FILE] file too small to contain APX header: {}",
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    log::debug("[APX catalog] scanning {} ({} bytes, cache miss)", attemptplayback::pathcompat::toUtf8(path), fsz);
    APXCatalogFileIdentity scanIdentity{};
    const bool haveScanIdentity = getAPXCatalogFileIdentity(path, scanIdentity);

    apxio::Reader in(path, apxio::Mode::Read);
    if (!in) {
        log::warn(
            "[SAVE FILE] failed to open file for read: {}",
            attemptplayback::pathcompat::toUtf8(path)
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
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    if (hdr.version < kAPXVersion) {
        out.loadedLegacy = true;
    } else if (hdr.version > kAPXVersion) {
        log::warn(
            "[SAVE FILE] unsupported future APX version {} in {}",
            hdr.version,
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }

    while (true) {
        uint32_t chunkType = 0;
        uint32_t chunkSize = 0;

        const int64_t chunkOffset = in.tellg();

        in.read(reinterpret_cast<char*>(&chunkType), sizeof(chunkType));
        if (!in) {
            if (in.eof()) break;
            return false;
        }

        in.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (!in) return false;

        if (chunkSize > kAPXMaxChunkSize_ || chunkSize > in.remaining()) {
            log::warn("[SAVE FILE] chunk size too large: {}", chunkSize);
            return false;
        }

        switch (chunkType) {
            case kChunk_ATT3: {
                APXAttemptDiskInfo entry{};
                bool usedLegacy = false;
                if (!readCompactRecord_(in, chunkSize, entry, nullptr, &usedLegacy)) {
                    log::error("[APX catalog] invalid ATT3 at offset={} size={}", chunkOffset, chunkSize);
                    return false;
                }
                entry.chunkFileOffset = static_cast<uint64_t>(chunkOffset);
                entry.chunkSize = chunkSize;
                if (out.attempts.size() >= apxio::kMaxCatalogEntries) return false;
                out.loadedLegacy = out.loadedLegacy || usedLegacy;
                out.maxSerialSeen = std::max(out.maxSerialSeen, static_cast<uint32_t>(entry.serial));
                out.attempts.push_back(std::move(entry));
                break;
            }

            case kChunk_PATH: {
                PracticePath tmpPath;
                bool usedLegacy = false;
                if (!readAPXPracticePath(in, chunkSize, tmpPath, &usedLegacy)) {
                    log::warn("[SAVE FILE] failed reading PATH chunk during catalog scan");
                    return false;
                }
                out.loadedLegacy = out.loadedLegacy || usedLegacy;
                out.practicePath = std::move(tmpPath);
                break;
            }

            case kChunk_ATTZ:
            case kChunk_CheckpointZ2:
            case kChunk_SEGZ: {
                out.loadedLegacy = true;
                if (chunkType == kChunk_ATTZ) {
                    out.sawLegacyAttemptChunks = true;
                }
                if (!skipChunkPayload_(in, chunkSize)) return false;
                break;
            }

            default: {
                log::error("[APX catalog] unsupported chunk type {} at offset={}", chunkType, chunkOffset);
                return false;
            }
        }
    }

    if (!out.loadedLegacy && !out.sawLegacyAttemptChunks && haveScanIdentity) {
        std::vector<uint8_t> indexPayload;
        if (serializeAPXCatalogIndex_(out, indexPayload)) {
            saveAPXCatalogCache(path, indexPayload, scanIdentity);
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - scanStart).count();
    log::info("[APX catalog] disk scan: {} attempts in {} ms", out.attempts.size(), elapsed);
    return true;
}

bool loadAPXAttemptByCatalogEntry(
    apxio::Reader& in,
    APXAttemptDiskInfo const& entry,
    Attempt& attemptOut,
    bool* outUsedLegacy
) {
    attemptOut = Attempt{};
    if (outUsedLegacy) *outUsedLegacy = false;

    in.clear();

    in.seekg(static_cast<int64_t>(entry.chunkFileOffset), apxio::Origin::Begin);
    if (!in) return false;

    uint32_t chunkType = 0;
    uint32_t chunkSize = 0;
    in.read(reinterpret_cast<char*>(&chunkType), sizeof(chunkType));
    in.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
    if (!in) return false;

    if (chunkType != kChunk_ATT3) return false;
    if (chunkSize != entry.chunkSize) return false;

    return readAPXAttemptCompact(in, chunkSize, attemptOut, outUsedLegacy) &&
        attemptOut.serial == entry.serial && attemptOut.practiceAttempt == entry.practiceAttempt;
}

bool loadAPXAttemptByCatalogEntry(
    std::filesystem::path const& path,
    APXAttemptDiskInfo const& entry,
    Attempt& attemptOut,
    bool* outUsedLegacy
) {
    apxio::Reader in(path, apxio::Mode::Read);
    if (!in) {
        log::warn(
            "[SAVE FILE] failed to open file for ATT3 lazy load: {}",
            attemptplayback::pathcompat::toUtf8(path)
        );
        return false;
    }
    return loadAPXAttemptByCatalogEntry(in, entry, attemptOut, outUsedLegacy);
 }

bool saveAPXFileCurrent(
    std::filesystem::path const& path,
    std::vector<Attempt> const& attempts,
    PracticePath const& practicePath
) {
    std::error_code ec;
    if (path.empty()) return false;
    const auto dir = path.parent_path();

    if (!dir.empty()) {
        ec.clear();
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            log::warn(
                "[SAVE FILE] create_directories failed: {} ({})",
                attemptplayback::pathcompat::toUtf8(dir),
                ec.message()
            );
            return false;
        }
    }

    return rewriteAPXFileToCurrent_(path, attempts, practicePath);
}

bool writeAPXAttemptCompact(apxio::Writer& out, Attempt const& attempt) {
    if (!out.good()) return false;

    if (attempt.p1.size() > kAPXMaxFramesPerTrack_ || attempt.p2.size() > kAPXMaxFramesPerTrack_ ||
        (uint64_t(attempt.p1.size()) + attempt.p2.size()) * sizeof(Frame) > apxio::kMaxDecodedFrameBytes) return false;

    for (auto const& frame : attempt.p1) if (!validMode_(static_cast<uint8_t>(frame.mode))) return false;
    for (auto const& frame : attempt.p2) if (!validMode_(static_cast<uint8_t>(frame.mode))) return false;

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

    if (!validMetadata_(prefix, attempt.baseTimeOffset)) return false;

    APXMetaCompactV2Tail_ tail{};
    tail.baseTimeOffset = attempt.baseTimeOffset;
    tail.seed = static_cast<uint64_t>(attempt.seed);

    if (uint64_t(p1Bytes.size()) + p2Bytes.size() + sizeof(prefix) + sizeof(tail) +
        2 * sizeof(APXTrackHeaderCompact) > kAPXMaxChunkSize_) return false;

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
        out.write(reinterpret_cast<const char*>(p1Bytes.data()), static_cast<size_t>(p1Bytes.size()));
    }
    out.write(reinterpret_cast<const char*>(&p2Hdr), sizeof(p2Hdr));
    if (!p2Bytes.empty()) {
        out.write(reinterpret_cast<const char*>(p2Bytes.data()), static_cast<size_t>(p2Bytes.size()));
    }

    return out.good();
}

bool readAPXAttemptCompact(apxio::Reader& in, uint32_t chunkSize, Attempt& attempt, bool* outUsedLegacy) {
    Attempt decoded;
    APXAttemptDiskInfo entry{};
    if (!readCompactRecord_(in, chunkSize, entry, &decoded, outUsedLegacy)) return false;
    finalizeLoadedAttempt_(decoded);
    attempt = std::move(decoded);
    return true;
}
