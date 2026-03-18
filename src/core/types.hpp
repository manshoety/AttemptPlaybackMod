// types.hpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <optional>
#include <thread>
#include <future>
#include <atomic>
#include <Geode/DefaultInclude.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

enum class ColorMode { Random, PlayerColors, White, Black };
enum class ReplayKind : uint8_t {
    PracticeComposite = 0,
    BestSingle = 1
};

struct Frame {
    float x=0;
    float y=0;
    float rot=0;
    float vehicleSize=1.f;
    float waveSize=1.f;
    bool upsideDown=false;
    bool hold=false;
    bool holdL=false;
    bool holdR=false;
    bool isDashing=false;
    bool isVisible=true;
    IconType mode = IconType::Cube;
    size_t frame=0;
    bool stateDartSlide=false;
    double t = 0;
};

struct FrameAccelTime {
    double baseT = 0.0;
    double binW = 0.0125;
    double invBinW = 80.0;
    int bins = 0;

    struct BinRange { uint32_t lo, hi; };
    std::vector<BinRange> range;

    std::vector<uint32_t> idx;

    std::vector<double> times;  // times[i] == v[i].t

    inline int binOf(double t) const {
        if (t <= baseT) return 0;
        int b = (int)((t - baseT) * invBinW);
        if (b < 0) return 0;
        if (b >= bins) return bins - 1;
        return b;
    }

    inline bool valid() const {
        return bins > 0 && !range.empty();
    }

    inline bool hasTimes(size_t n) const {
        return times.size() == n;
    }

    inline void reset() {
        bins = 0;
        baseT = 0.0;
        binW = 0.0125;
        invBinW = 80.0;
        range.clear();
        idx.clear();
        times.clear();
    }
};

struct FrameAccel {
    float baseX = 0.f;
    float invBinW = 1.f / 64.f;
    int bins = 0;
    std::vector<uint32_t> idx;
    inline int binOf(float x) const {
        int b = (int)std::floor((x - baseX) * invBinW);
        if (b < 0) return 0;
        if (b >= bins) return bins - 1;
        return b;
    }
    inline bool valid() const {
        return bins > 0 && !idx.empty();
    }
    inline void reset() {
        bins = 0;
        idx.clear();
    }
};


enum class IconAnimationState {
    Idle = 0,
    Run = 1,
    Jump = 2,
    Fall = 3,
};

struct PoseCache {
    float x = NAN;
    float y = NAN;
    float rot = NAN;
    float vehicleSize = NAN;
    float waveSize = NAN;
    IconType mode = IconType::Cube;
    bool upsideDown = false;
    bool isDashing = false;
    bool visible = false;
    IconAnimationState robotState = IconAnimationState::Idle;
    IconAnimationState spiderState = IconAnimationState::Idle;
};

#pragma pack(push, 1)
struct APXHeader { 
    char magic[4] = {'A','P','X','2'}; 
    uint32_t version = 5;
};
struct APXMeta {
    uint32_t serial;
    float startPercent;
    float endPercent;
    uint8_t flags;
    uint8_t _pad1;
    uint32_t p1Count;
    uint32_t p2Count;
    float startX;
    float startY;
    int32_t startCheckpointId;
    double baseTimeOffset;
    uint64_t seed;
};
struct APXFrame {
    float x;
    float y;
    float rot;
    float vehicleSize;
    float waveSize;
    uint8_t mode;
    uint8_t flags;
    uint8_t _pad[2];
    double t;
};
struct APXCheckpointMeta {
    uint32_t nodeCount;
    int32_t topId;
    int32_t frozenTopId;
};
struct APXCheckpointNodePacked {
    int32_t id;
    float x;
    int32_t prevId;
    uint8_t active;
    int32_t ownerSerial;
    uint32_t ownerFrameIdx;
    float rot1;
    float rot2;
    uint8_t rot1Valid;
    uint8_t rot2Valid;
    uint8_t _pad[2]{};
};
struct APXSeg {
    float x0;
    float x1;
    int32_t ownerSerial;
    int32_t checkpointId;
};
#pragma pack(pop)

struct FrozenSeg {
    float x0;
    float x1;
    int ownerSerial;
    int checkpointId;
};

struct LoadFilter {
    std::optional<int> minPct;
    std::optional<int> maxPct;
    std::optional<bool> practice;
    size_t maxNonPractice = 0;
};
struct Span {
    double startX = 0.0;
    double endX   = 0.0;
    int ownerSerial = 0;
    size_t p1Count = 0;
    size_t p2Count = 0;
};
struct PracticeRun {
    int serial = 0;
    double startX = 0.0;
    double spanX  = 0.0;
    size_t framesP1 = 0;
    size_t framesP2 = 0;
    bool completed = false;
    bool recordedThisSession = false;
};
struct AttemptSpan {
    union {
        struct {
            double startX;
            double endX;
        };
        struct {
            double firstX;
            double lastX;
        };
    };

    int ownerSerial = 0;
    size_t p1Count = 0;
    size_t p2Count = 0;

    AttemptSpan() : startX(0.0), endX(0.0) {}

    AttemptSpan(double s, double e, int owner = 0, int p1 = 0, int p2 = 0)
        : startX(s), endX(e), ownerSerial(owner), p1Count(p1), p2Count(p2) {}

    AttemptSpan(float s, float e, int owner = 0, int p1 = 0, int p2 = 0)
        : startX(static_cast<double>(s)),
          endX(static_cast<double>(e)),
          ownerSerial(owner), p1Count(p1), p2Count(p2) {}

    AttemptSpan(const Span& other)
        : startX(other.startX), endX(other.endX),
          ownerSerial(other.ownerSerial), p1Count(other.p1Count), p2Count(other.p2Count) {}

    AttemptSpan& operator=(const Span& other) {
        startX = other.startX; endX = other.endX;
        ownerSerial = other.ownerSerial; p1Count = other.p1Count; p2Count = other.p2Count;
        return *this;
    }

    operator Span() const {
        Span s;
        s.startX = startX;
        s.endX = endX;
        s.ownerSerial = ownerSerial;
        s.p1Count = p1Count;
        s.p2Count = p2Count;
        return s;
    }
};

struct Segment {
    float startX;
    float endX;
    int ownerSerial;
};

struct Checkpoint {
    int id = -1;
    float x = 0.f;
    float y = 0.f;
    float rot1 = 0.f;
    float rot2 = 0.f;
    bool rot1Valid = false;
    bool rot2Valid = false;
    double checkpointTime = 0.0;
};

struct PracticeSegment {
    float startX = 0.f;
    float endX = 0.f;
    int ownerSerial = -1;
    int checkpointIdEnd = -1;
    size_t p1Frames = 0;
    size_t p2Frames = 0;
    float maxXReached = 0.f;
    double baseTimeOffset = 0.0;   // checkpointTime at attempt start (respawn)
    double localStartTime = 0.0;   // seconds since respawn at attempt start
    double localEndTime   = 0.0;   // seconds since respawn at attempt end

    inline double absStart() const { return baseTimeOffset + localStartTime; }
    inline double absEnd()   const { return baseTimeOffset + localEndTime;   }
};

struct PracticeSession {
    int sessionId = 0;
    std::vector<Checkpoint> checkpoints;
    std::vector<int> activeChain;  // IDs of active checkpoints in order
    std::vector<int> allAttemptSerials;
    // Invariant for segments:
    // - kept sorted by absStart()
    // - kept non-overlapping in absolute time
    // - overwriteSegmentForAttempt() preserves this
    std::vector<PracticeSegment> segments;
    float startX = 0.f;
    float startY = 0.f;
    float endX = 0.f;
    bool completed = false;
    bool frozen = false;
    
    void updateSpan() {
        if (segments.empty()) {
            startX = 0.f;
            endX = 0.f;
            return;
        }

        // segments are kept sorted by absStart(), so earliest segment is front()
        startX = segments.front().startX;

        // EndX = furthest reached among all segments
        float bestEnd = 0.f;
        for (const auto& seg : segments) {
            bestEnd = std::max(bestEnd, seg.endX);
            bestEnd = std::max(bestEnd, seg.maxXReached);
        }
        endX = bestEnd;
    }
    
    const Checkpoint* findCheckpoint(int id) const {
        for (const auto& checkpoint: checkpoints) {
            if (checkpoint.id == id) return &checkpoint;
        }
        return nullptr;
    }
    
    Checkpoint* findCheckpoint(int id) {
        for (auto& checkpoint: checkpoints) {
            if (checkpoint.id == id) return &checkpoint;
        }
        return nullptr;
    }
};

struct PracticePath {
    std::vector<PracticeSession> sessions;
    int activeSessionId = 0;
    int selectedSessionId = 0;
    bool frozen = false;
    
    void clear() {
        sessions.clear();
        activeSessionId = 0;
        selectedSessionId = 0;
        frozen = false;
    }
    
    PracticeSession* activeSession() {
        for (auto& s : sessions) {
            if (s.sessionId == activeSessionId) return &s;
        }
        return nullptr;
    }
    
    const PracticeSession* activeSession() const {
        for (const auto& s : sessions) {
            if (s.sessionId == activeSessionId) return &s;
        }
        return nullptr;
    }
    
    PracticeSession* selectedSession() {
        for (auto& s : sessions) {
            if (s.sessionId == selectedSessionId) return &s;
        }
        return nullptr;
    }
    
    const PracticeSession* selectedSession() const {
        for (const auto& s : sessions) {
            if (s.sessionId == selectedSessionId) return &s;
        }
        return nullptr;
    }
};

struct CheckpointNode {
    int id = -1;
    float x = 0.f;
    int ownerSerial = -1;
    size_t ownerAttemptIdx = SIZE_MAX;
    size_t ownerFrameIdx = 0;
    int prevId = -1;
    bool active = true;
    float rot1 = 0.f;
    float rot2 = 0.f;
    bool rot1Valid = false;
    bool rot2Valid = false;
};


struct XBatchGrid {
    float binW = 1024.f;

    int minBin = INT_MAX;
    int maxBin = INT_MIN;
    std::vector<std::vector<uint32_t>> bins;

    static constexpr int kMaxBins = 200000;
    static constexpr int kMaxSpanBins = 20000;

    mutable std::vector<uint32_t> seenStamp;
    mutable uint32_t curStamp = 1;

    uint32_t maxIdPlus1 = 0;

    void clear() {
        bins.clear();
        minBin = INT_MAX;
        maxBin = INT_MIN;
        seenStamp.clear();
        curStamp = 1;
        maxIdPlus1 = 0;
    }

    static inline int binOf(float x, float w) {
        if (!(w > 0.f) || !std::isfinite(x)) return 0;
        const double b = std::floor((double)x / (double)w);
        if (b < (double)INT_MIN) return INT_MIN;
        if (b > (double)INT_MAX) return INT_MAX;
        return (int)b;
    }

    bool ensureRange(int b0, int b1) {
        if (b0 > b1) return true;

        // compute prospective range using 64-bit to avoid overflow
        int64_t newMin = (minBin == INT_MAX) ? (int64_t)b0 : std::min<int64_t>(minBin, b0);
        int64_t newMax = (minBin == INT_MAX) ? (int64_t)b1 : std::max<int64_t>(maxBin, b1);
        int64_t newCount = newMax - newMin + 1;

        if (newCount <= 0 || newCount > kMaxBins) {
            // refuse insane allocation
            return false;
        }

        if (minBin == INT_MAX) {
            minBin = (int)newMin;
            maxBin = (int)newMax;
            bins.clear();
            bins.resize((size_t)newCount);
            return true;
        }

        if (b0 < minBin) {
            size_t grow = (size_t)(minBin - b0);
            std::vector<std::vector<uint32_t>> nb;
            nb.resize(grow + bins.size());
            std::move(bins.begin(), bins.end(), nb.begin() + (ptrdiff_t)grow);
            bins.swap(nb);
            minBin = b0;
        }

        if (b1 > maxBin) {
            bins.resize((size_t)(b1 - minBin + 1));
            maxBin = b1;
        }

        return true;
    }

    void insert(size_t attemptIdx, float firstX, float lastX) {
        if (!std::isfinite(firstX) || !std::isfinite(lastX)) return;
        if (attemptIdx > UINT32_MAX) return;

        if (lastX < firstX) std::swap(firstX, lastX);

        const int b0 = binOf(firstX, binW);
        const int b1 = binOf(lastX,  binW);
        if ((int64_t)b1 - (int64_t)b0 > kMaxSpanBins) return;
        if (!ensureRange(b0, b1)) return;

        const uint32_t id = (uint32_t)attemptIdx;

        if (id + 1u > maxIdPlus1) maxIdPlus1 = id + 1u;

        for (int b = b0; b <= b1; ++b) {
            bins[(size_t)(b - minBin)].push_back(id);
        }
    }

    void candidates_u32(float L, float R, std::vector<uint32_t>& out) const {
        out.clear();
        if (bins.empty() || R < L) return;
        if (!std::isfinite(L) || !std::isfinite(R)) return;

        const int b0 = binOf(L, binW);
        const int b1 = binOf(R, binW);
        const int lo = std::max(b0, minBin);
        const int hi = std::min(b1, maxBin);
        if (lo > hi) return;

        if (lo == hi) { out = bins[(size_t)(lo - minBin)]; return; }

        if (seenStamp.size() < (size_t)maxIdPlus1) {
            seenStamp.assign((size_t)maxIdPlus1, 0u);
            curStamp = 1u;
        } else {
            ++curStamp;
            if (curStamp == 0u) {
                std::fill(seenStamp.begin(), seenStamp.end(), 0u);
                curStamp = 1u;
            }
        }

        for (int b = lo; b <= hi; ++b) {
            const auto& v = bins[(size_t)(b - minBin)];
            for (uint32_t id : v) {
                if (seenStamp[(size_t)id] != curStamp) {
                    seenStamp[(size_t)id] = curStamp;
                    out.push_back(id);
                }
            }
        }
    }
};

struct Attempt {
    std::vector<Frame> p1;
    std::vector<Frame> p2;
    bool hadDual = false;
    size_t i1 = 0, i2 = 0;
    size_t d1 = 0, d2 = 0;
    geode::Ref<PlayerObject> g1 = nullptr;
    geode::Ref<PlayerObject> g2 = nullptr;
    int g1Idx = -1;
    int g2Idx = -1;
    bool p1Visible = false;
    bool p2Visible = false;
    bool preloaded = false;
    bool needsVisualSetup = false;
    bool m_isPlatformer = false;
    bool beganAtStartOfLevel = true;
    uintptr_t seed = 0;

    PoseCache last1{};
    PoseCache last2{};

    FrameAccel acc1;
    FrameAccel acc2;

    bool trailactive1 = false;
    bool trailactive2 = false;
    bool previouslyHolding1 = false;
    bool previouslyHolding2 = false;
    bool previouslyHoldingL1 = false;
    bool previouslyHoldingL2 = false;
    bool previouslyHoldingR1 = false;
    bool previouslyHoldingR2 = false;
    bool prevStateDartSlide1 = false;
    bool prevStateDartSlide2 = false;
    IconType g1CurMode = IconType::Cube;
    IconType g2CurMode = IconType::Cube;
    cocos2d::ccColor3B c1{255,255,255};
    cocos2d::ccColor3B c2{255,255,255};
    bool colorsAssigned = false;

    std::array<int,9> randomFrame{ };
    bool iconsAssigned = false;
    bool offsetApplied = false;

    GLubyte opacity=255;
    float ghostOffsetPx = 0.f;
    double ghostOffsetTime = 0.0;
    bool practiceAttempt = false;
    bool ignoreInPractice = false;
    bool immuneToIgnorePractice = false;
    bool ignorePlayback = false;
    float startX = 0.f;
    float endX = 0.f;
    float startPercent = 0;
    float endPercent = 0;
    int startCheckpointId = -1;
    int serial = -1;
    bool completed = false;
    bool persistedOnDisk = false;
    bool recordedThisSession = false;
    bool primedP1 = false;
    bool primedP2 = false;
    bool eolFrozenP1 = false;
    bool eolFrozenP2 = false;
    FrameAccelTime acc1Time;
    FrameAccelTime acc2Time;
    float startY = 0.f;
    double baseTimeOffset = 0.0;
    void resetPlayback() {
        i1 = i2 = d1 = d2 = 0;
        eolFrozenP1 = eolFrozenP2 = false;
        primedP1 = primedP2 = false;
        previouslyHolding1 = previouslyHolding2 = false;
        previouslyHoldingL1 = previouslyHoldingL2 = false;
        previouslyHoldingR1 = previouslyHoldingR2 = false;
        prevStateDartSlide1 = prevStateDartSlide2 = false;
        trailactive1 = trailactive2 = false;
        last1 = PoseCache{};
        last2 = PoseCache{};
        g1Idx = -1;
        g2Idx = -1;
    }
    void setP1Visible(bool v, bool force) {
        if (!force && p1Visible==v) return;
        p1Visible = v;
        last1.visible = v;
        
        if (!g1 || !g1.data()) return; // Check both Ref and internal pointer
        
        PlayerObject* po = g1.data();
        if (!po) return;
        
        //if (!force && po->isVisible() == v) return;

        po->setVisible(v);
        // if (po->m_waveTrail) po->m_waveTrail->setVisible(false);
    }

    void setP1Visible(bool p1) { setP1Visible(p1, false); }
    void setP2Visible(bool v, bool force) {
        if (!force && p2Visible==v) return;
        p2Visible = v;
        last2.visible = v;
        
        if (!g2 || !g2.data()) return;
        
        PlayerObject* po = g2.data();
        if (!po) return;
        
        //if (!force && po->isVisible() == v) return;

        po->setVisible(v);
        // if (po->m_waveTrail) po->m_waveTrail->setVisible(false);
    }
    void setP2Visible(bool p2) { setP2Visible(p2, false); }
};

#pragma pack(push, 1)
struct APXPathHeader {
    uint32_t numCheckpoints;
    uint32_t numActiveIDs;
    uint32_t numSegments;
    uint8_t frozen;
    uint8_t _pad[3];
};
struct APXCheckpoint {
    int32_t id;
    float x;
    float y;
    float rot1;
    float rot2;
    uint8_t rot1Valid;
    uint8_t rot2Valid;
    uint8_t padding[2];
    double checkpointTime;
};
struct APXSegment {
    float startX;
    float endX;
    int32_t ownerSerial;
    int32_t checkpointIdEnd;
    int32_t p1Frames;
    int32_t p2Frames;
    float maxXReached;
    double baseTimeOffset;
    double localStartTime;
    double localEndTime;
};

#pragma pack(pop)

#pragma pack(push, 1)
struct APXSessionHeader {
    int32_t sessionId;
    uint32_t numCheckpoints;
    uint32_t numActiveIDs;
    uint32_t numAttemptSerials;
    uint32_t numSegments;
    float startX;
    float startY;
    float endX;
    uint8_t completed;
    uint8_t frozen;
    uint8_t padding[2];
};

struct APXMetaV3 {
    uint32_t serial;
    float startPercent;
    uint8_t flags;
    uint8_t _pad1;
    uint32_t p1Count;
    uint32_t p2Count;
    float startX;
    float startY;
    int32_t startCheckpointId;
    double baseTimeOffset;
};

struct APXPathHeaderV2 {
    uint32_t version;
    uint32_t numSessions;
    int32_t activeSessionId;
    int32_t selectedSessionId;
    uint8_t frozen;
    uint8_t padding[3];
};

struct APXMetaV2 {
    uint32_t serial;
    float startPercent;
    uint8_t flags;
    uint32_t p1Count;
    uint32_t p2Count;
    float startX;
    int32_t startCheckpointId;
};

struct APXFrameV2 {
    float x;
    float y;
    float rot;
    float vehicleSize;
    float waveSize;
    uint8_t mode;
    uint8_t flags;
};

struct APXCheckpointV2 {
    int32_t id;
    float x;
    float rot1;
    float rot2;
    uint8_t rot1Valid;
    uint8_t rot2Valid;
    uint8_t padding[2];
};

struct APXSegmentV2 {
    float startX;
    float endX;
    int32_t ownerSerial;
    int32_t checkpointIdEnd;
    int32_t p1Frames;
    int32_t p2Frames;
    float maxXReached;
};

struct APXSessionHeaderV2 {
    int32_t sessionId;
    uint32_t numCheckpoints;
    uint32_t numActiveIDs;
    uint32_t numSegments;
    float startX;
    float endX;
    uint8_t completed;
    uint8_t frozen;
    uint8_t padding[2];
};
#pragma pack(pop)

struct GhostBatch {
    geode::Ref<cocos2d::CCNode> node = nullptr;
    geode::Ref<cocos2d::CCNode> trailNode = nullptr;
    size_t count = 0;
};

// For threading if I ever dare to do that optiization
struct GhostComputeResult {
    size_t attemptIdx;
    
    // P1 computed state
    float p1_x, p1_y, p1_rot;
    float p1_vehicleSize, p1_waveSize;
    IconType p1_mode;
    bool p1_visible;
    bool p1_upsideDown;
    bool p1_isDashing;
    bool p1_eolFrozen;
    size_t p1_frameIdx;
    size_t p1_nextFrameIdx;
    bool p1_valid;
    
    // P2 computed state  
    float p2_x, p2_y, p2_rot;
    float p2_vehicleSize, p2_waveSize;
    IconType p2_mode;
    bool p2_visible;
    bool p2_upsideDown;
    bool p2_isDashing;
    bool p2_eolFrozen;
    size_t p2_frameIdx;
    size_t p2_nextFrameIdx;
    bool p2_valid;
    
    // Wave trail data
    bool p1_trailNeedsPoint;
    bool p2_trailNeedsPoint;
    float p1_trailX, p1_trailY;
    float p2_trailX, p2_trailY;
};
