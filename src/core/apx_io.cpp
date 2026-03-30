// apx_io.cpp
#include <ostream>
#include <istream>
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
    outFrames.reserve(frameCount);

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
    hdr.version = 4;  // Version 4 with saved session stuff
    hdr.numSessions = static_cast<uint32_t>(path.sessions.size());
    hdr.activeSessionId = path.activeSessionId;
    hdr.selectedSessionId = path.selectedSessionId;
    hdr.frozen = path.frozen ? 1 : 0;

    const uint32_t type = kChunk_PATH;

    // Calculate total size
    uint32_t totalSize = sizeof(APXPathHeaderV2);
    for (const auto& session : path.sessions) {
        totalSize += sizeof(APXSessionHeader);
        totalSize += static_cast<uint32_t>(session.checkpoints.size() * sizeof(APXCheckpoint));
        totalSize += static_cast<uint32_t>(session.activeChain.size() * sizeof(int32_t));
        totalSize += static_cast<uint32_t>(session.allAttemptSerials.size() * sizeof(int32_t));
        totalSize += static_cast<uint32_t>(session.segments.size() * sizeof(APXSegment));
    }

    out.write(reinterpret_cast<const char*>(&type), sizeof(type));
    out.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out.good()) return false;

    for (const auto& session : path.sessions) {
        APXSessionHeader shdr{};
        shdr.sessionId = session.sessionId;
        shdr.numCheckpoints = static_cast<uint32_t>(session.checkpoints.size());
        shdr.numActiveIDs = static_cast<uint32_t>(session.activeChain.size());
        shdr.numAttemptSerials = static_cast<uint32_t>(session.allAttemptSerials.size());
        shdr.numSegments = static_cast<uint32_t>(session.segments.size());
        shdr.startX = session.startX;
        shdr.startY = session.startY;
        shdr.endX = session.endX;
        shdr.completed = session.completed ? 1 : 0;
        shdr.frozen = session.frozen ? 1 : 0;

        out.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));

        // Write checkpoints with time data
        for (const auto& checkpoint : session.checkpoints) {
            APXCheckpoint acheckpoint{};
            acheckpoint.id = checkpoint.id;
            acheckpoint.x = checkpoint.x;
            acheckpoint.y = checkpoint.y;
            acheckpoint.rot1 = checkpoint.rot1;
            acheckpoint.rot2 = checkpoint.rot2;
            acheckpoint.rot1Valid = checkpoint.rot1Valid ? 1 : 0;
            acheckpoint.rot2Valid = checkpoint.rot2Valid ? 1 : 0;
            acheckpoint.checkpointTime = checkpoint.checkpointTime;
            out.write(reinterpret_cast<const char*>(&acheckpoint), sizeof(acheckpoint));
        }

        // Write active chain
        for (int32_t id : session.activeChain) {
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }

        // Write all attempt serials
        for (int32_t id : session.allAttemptSerials) {
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }

        // Write segments with time data
        for (const auto& seg : session.segments) {
            APXSegment aseg{};
            aseg.startX = seg.startX;
            aseg.endX = seg.endX;
            aseg.ownerSerial = seg.ownerSerial;
            aseg.checkpointIdEnd = seg.checkpointIdEnd;
            aseg.p1Frames = (int32_t)seg.p1Frames;
            aseg.p2Frames = (int32_t)seg.p2Frames;
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

bool readAPXPracticePath(std::istream& in, uint32_t chunkSize, PracticePath& path) {
    path.clear();
    
    std::streampos startPos = in.tellg();
    
    // Read header to determine version
    APXPathHeaderV2 hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    
    path.activeSessionId = hdr.activeSessionId;
    path.selectedSessionId = hdr.selectedSessionId;
    path.frozen = (hdr.frozen != 0);
    
    const bool isVersion4 = (hdr.version >= 4);
    
    //log::info("[APX load] PATH version={}, sessions={}", hdr.version, hdr.numSessions);
    
    // Read sessions
    for (uint32_t s = 0; s < hdr.numSessions; ++s) {
        PracticeSession session;
        
        if (isVersion4) {
            APXSessionHeader shdr{};
            in.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
            if (!in) return false;
            
            session.sessionId = shdr.sessionId;
            session.startX = shdr.startX;
            session.startY = shdr.startY;
            session.endX = shdr.endX;
            session.completed = (shdr.completed != 0);
            session.frozen = (shdr.frozen != 0);
            
            // Read checkpoints
            for (uint32_t i = 0; i < shdr.numCheckpoints; ++i) {
                APXCheckpoint acheckpoint{};
                in.read(reinterpret_cast<char*>(&acheckpoint), sizeof(acheckpoint));
                if (!in) return false;
                
                Checkpoint checkpoint;
                checkpoint.id = acheckpoint.id;
                checkpoint.x = acheckpoint.x;
                checkpoint.y = acheckpoint.y;
                checkpoint.rot1 = acheckpoint.rot1;
                checkpoint.rot2 = acheckpoint.rot2;
                checkpoint.rot1Valid = (acheckpoint.rot1Valid != 0);
                checkpoint.rot2Valid = (acheckpoint.rot2Valid != 0);
                checkpoint.checkpointTime = acheckpoint.checkpointTime;
                session.checkpoints.push_back(checkpoint);
            }
            
            // Read active chain
            for (uint32_t i = 0; i < shdr.numActiveIDs; ++i) {
                int32_t id;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.activeChain.push_back(id);
            }

            // Read all attempt serials
            for (uint32_t i = 0; i < shdr.numAttemptSerials; ++i) {
                int32_t id;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.allAttemptSerials.push_back(id);
            }
            
            // Read segments
            for (uint32_t i = 0; i < shdr.numSegments; ++i) {
                APXSegment aseg{};
                in.read(reinterpret_cast<char*>(&aseg), sizeof(aseg));
                if (!in) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;
                seg.checkpointIdEnd = aseg.checkpointIdEnd;
                seg.p1Frames = aseg.p1Frames;
                seg.p2Frames = aseg.p2Frames;
                seg.maxXReached = aseg.maxXReached;

                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        } else {
            geode::log::warn("Outdated Practice Session Format. Please delete attempts save file");
        }
        
        session.updateSpan();
        path.sessions.push_back(session);
    }
    
    // Skip any remaining bytes in chunk
    auto endPos = in.tellg();
    if (endPos == std::streampos(-1)) return false;

    std::streamoff bytesConsumed = endPos - startPos;
    if (bytesConsumed < 0) return false;

    if ((uint32_t)bytesConsumed < chunkSize) {
        in.seekg(static_cast<std::streamoff>(chunkSize) - bytesConsumed, std::ios::cur);
    }
    
    //log::info("[APX load] PATH loaded: {} sessions, version {}", path.sessions.size(), hdr.version);
    return true;
}

bool writeAPXAttemptCompact(std::ostream& out, Attempt const& attempt) {
    if (!out.good()) return false;

    const std::vector<uint8_t> p1Bytes = encodeTrackCompact_(attempt.p1);
    const std::vector<uint8_t> p2Bytes = encodeTrackCompact_(attempt.p2);

    APXMetaCompact meta{};
    meta.serial = static_cast<uint32_t>(attempt.serial);
    meta.startPercent = attempt.startPercent;
    meta.endPercent = attempt.endPercent;
    meta.flags = attemptFlagsFromAttempt_(attempt);
    meta.streamVersion = kAPXCompactAttemptVersion;
    meta.p1Count = static_cast<uint32_t>(attempt.p1.size());
    meta.p2Count = static_cast<uint32_t>(attempt.p2.size());
    meta.startX = attempt.startX;
    meta.startY = attempt.startY;
    meta.startCheckpointId = attempt.startCheckpointId;
    meta.baseTimeOffset = attempt.baseTimeOffset;
    meta.seed = static_cast<uint64_t>(attempt.seed);

    APXTrackHeaderCompact p1Hdr{};
    p1Hdr.byteSize = static_cast<uint32_t>(p1Bytes.size());

    APXTrackHeaderCompact p2Hdr{};
    p2Hdr.byteSize = static_cast<uint32_t>(p2Bytes.size());

    const uint32_t payloadSz =
        static_cast<uint32_t>(sizeof(APXMetaCompact)) +
        static_cast<uint32_t>(sizeof(APXTrackHeaderCompact)) + p1Hdr.byteSize +
        static_cast<uint32_t>(sizeof(APXTrackHeaderCompact)) + p2Hdr.byteSize;

    const uint32_t type = kChunk_ATT3;

    out.write(reinterpret_cast<const char*>(&type), sizeof(type));
    out.write(reinterpret_cast<const char*>(&payloadSz), sizeof(payloadSz));
    out.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
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

bool readAPXAttemptCompact(std::istream& in, uint32_t chunkSize, Attempt& attempt) {
    attempt = Attempt{};

    std::vector<uint8_t> buf(chunkSize);
    if (chunkSize != 0) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!in) return false;
    }

    const uint8_t* cur = buf.data();
    const uint8_t* end = buf.data() + buf.size();

    APXMetaCompact meta{};
    if (!readPod_(cur, end, meta)) return false;
    if (meta.streamVersion != kAPXCompactAttemptVersion) return false;

    attempt.serial = static_cast<int>(meta.serial);
    attempt.startPercent = meta.startPercent;
    attempt.endPercent = meta.endPercent;
    attempt.startX = meta.startX;
    attempt.startY = meta.startY;
    attempt.startCheckpointId = meta.startCheckpointId;
    attempt.baseTimeOffset = meta.baseTimeOffset;
    attempt.seed = static_cast<uintptr_t>(meta.seed);
    applyAttemptFlags_(meta.flags, attempt);

    APXTrackHeaderCompact p1Hdr{};
    if (!readPod_(cur, end, p1Hdr)) return false;
    if ((size_t)(end - cur) < p1Hdr.byteSize) return false;
    if (!decodeTrackCompact_(cur, p1Hdr.byteSize, meta.p1Count, attempt.p1)) return false;
    cur += p1Hdr.byteSize;

    APXTrackHeaderCompact p2Hdr{};
    if (!readPod_(cur, end, p2Hdr)) return false;
    if ((size_t)(end - cur) < p2Hdr.byteSize) return false;
    if (!decodeTrackCompact_(cur, p2Hdr.byteSize, meta.p2Count, attempt.p2)) return false;
    cur += p2Hdr.byteSize;

    finalizeLoadedAttempt_(attempt);
    return true;
}

