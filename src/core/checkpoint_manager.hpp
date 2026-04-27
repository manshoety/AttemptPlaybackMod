// checkpoint_manager.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_set>
#include <vector>
#include "types.hpp"


class CheckpointManager {
public:
    CheckpointManager() = default;

    bool noValidSessionForStartX() const { return m_noValidSessionForStartX; }
    void clearNoValidSessionForStartX() { m_noValidSessionForStartX = false; }
    
    void restorePath(const PracticePath& path) {
        m_path = path;
        m_dirty = false;
        m_noValidSessionForStartX = false;

        // Find max session ID
        m_nextSessionId = 1;
        for (const auto& session : m_path.sessions) {
            if (session.sessionId >= m_nextSessionId) {
                m_nextSessionId = session.sessionId + 1;
            }
        }

        currentFurthestSegment = PracticeSegment{};
    }

    static float sessionStartX_(PracticeSession const& s) {
        if (s.segments.empty()) return NAN;
        return s.segments.front().startX;
    }

    static double sessionAbsEnd_(PracticeSession const& s) {
        double best = 0.0;
        for (auto const& seg : s.segments) {
            best = std::max(best, seg.absEnd());
        }
        return best;
    }

    // Filter by start position, rank by time
    int pickBestSessionIdForStartX_RankByTime(float curX, float xTol) const {
        int bestId = -1;
        double bestEndT = -1.0;

        // Walk newest -> oldest so ties prefer the most recent session
        for (size_t i = m_path.sessions.size(); i-- > 0;) {
            auto const& s = m_path.sessions[i];
            if (s.segments.empty()) continue;

            const float sx = sessionStartX_(s);
            if (!std::isfinite(sx)) continue;

            if (std::fabs(sx - curX) > xTol) {
                continue;
            }

            const double endT = sessionAbsEnd_(s);

            if (endT > bestEndT) {
                bestEndT = endT;
                bestId = s.sessionId;
            }
        }

        return bestId;
    }

    int selectBestSessionForStartX_RankByTime(float curX, float xTol) {
        const int best = pickBestSessionIdForStartX_RankByTime(curX, xTol);
        currentFurthestSegment = PracticeSegment{};

        if (best > 0) {
            if (auto* s = sessionById_(best)) {
                m_noValidSessionForStartX = false;
                m_path.selectedSessionId = best;
                m_path.activeSessionId = best;
                return best;
            }

            m_noValidSessionForStartX = true;
            m_path.selectedSessionId = 0;
            m_path.activeSessionId = 0;
            return -1;
        }

        m_noValidSessionForStartX = true;
        m_path.selectedSessionId = 0;
        m_path.activeSessionId = 0;
        return -1;
    }



    std::vector<int> getPracticeSerialsMatchingStartX_AllSessions(float targetX, int xTol) const {
        if (!std::isfinite(targetX)) return {};

        std::vector<PracticeSession const*> matches;
        matches.reserve(m_path.sessions.size());

        for (auto const& s : m_path.sessions) {
            if (s.segments.empty()) continue;

            const float sx = sessionStartX_(s);
            if (!std::isfinite(sx)) continue;

            if (std::fabs(sx - targetX) <= static_cast<float>(xTol)) {
                matches.push_back(&s);
            }
        }

        // Most-progress and longest replay sessions first
        std::sort(matches.begin(), matches.end(),
            [](PracticeSession const* a, PracticeSession const* b) {
                const double ae = sessionAbsEnd_(*a);
                const double be = sessionAbsEnd_(*b);
                if (ae != be) return ae > be;
                return a->sessionId > b->sessionId;
            }
        );

        std::unordered_set<int> seen;
        std::vector<int> out;
        out.reserve(512);

        for (auto const* s : matches) {
            // Include every serial saved in the session
            for (int serial : s->allAttemptSerials) {
                if (serial <= 0) continue;
                if (seen.insert(serial).second) {
                    out.push_back(serial);
                }
            }

            // Also include segment owners
            for (auto const& seg : s->segments) {
                if (seg.ownerSerial <= 0) continue;
                if (seen.insert(seg.ownerSerial).second) {
                    out.push_back(seg.ownerSerial);
                }
            }
        }

        return out;
    }

    std::vector<int> getPracticeSerialsMatchingCurrentStartPos_AllSessions(int xTol) const {
        const PracticeSession* cur = m_path.selectedSession();
        if (!cur) cur = m_path.activeSession();
        if (!cur) return {};

        if (cur->segments.empty()) return {};

        const float targetX = sessionStartX_(*cur);
        if (!std::isfinite(targetX)) return {};

        return getPracticeSerialsMatchingStartX_AllSessions(targetX, xTol);
    }
        
    const PracticePath& getPath() const { return m_path; }
    PracticePath& getPath() { return m_path; }
    
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }
    void markDirty() { m_dirty = true; }
    
    void onAttemptCompleted() {
        PracticeSession* session = m_path.activeSession();
        if (session) {
            session->completed = true;
            session->updateSpan();
        }
        m_dirty = true;
    }

    void gatherSessionOwnerSerials(std::unordered_set<int>& out) const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) return;
        
        for (const auto& seg : session->segments) {
            if (seg.ownerSerial > 0) out.insert(seg.ownerSerial);
        }
    }

    void onExitEarly() {
        if (currentFurthestSegment.ownerSerial <= 0) return;
        if (!(currentFurthestSegment.localEndTime > currentFurthestSegment.localStartTime)) return;

        overwriteSegmentForAttempt(
            currentFurthestSegment.ownerSerial,
            currentFurthestSegment.baseTimeOffset,
            currentFurthestSegment.localStartTime,
            currentFurthestSegment.localEndTime,
            currentFurthestSegment.startX,
            currentFurthestSegment.endX,
            currentFurthestSegment.p1Frames,
            currentFurthestSegment.p2Frames
        );
    }
    
    void overwriteSegmentForAttempt(
        int serial,
        double baseTimeOffset,
        double localStartTime,
        double localEndTime,
        float startX,
        float endX,
        size_t p1Frames,
        size_t p2Frames
    ) {
        if (!(localEndTime > localStartTime)) return;

        PracticeSession* session = m_path.activeSession();
        if (!session) {
            createNewSession();
            session = m_path.activeSession();
        }
        if (!session) return;

        if (m_path.frozen || session->frozen) return;

        //log::info(
        //    "[SESSION WRITE] activeSessionId={} sessionPtr={} serial={} base={} start={} end={}",
        //    m_path.activeSessionId,
        //    static_cast<void*>(session),
        //    serial,
        //    baseTimeOffset,
        //    localStartTime,
        //    localEndTime
        //);

        addAttemptSerialIfMissing_(*session, serial);

        PracticeSegment newSeg = makeSegment_(
            serial,
            baseTimeOffset,
            localStartTime,
            localEndTime,
            startX,
            endX,
            p1Frames,
            p2Frames
        );

        updateCurrentFurthestSegment_(newSeg);

        bool beganAtStartOfLevel = baseTimeOffset == 0.f;

        overwriteIntoTimeline_(session->segments, newSeg, beganAtStartOfLevel);

        session->updateSpan();
        m_dirty = true;
    }
    
    int findOwnerSerialForTime(double t) const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session || session->segments.empty()) return -1;

        const auto& segs = session->segments;

        if (t < segs.front().absStart()) return segs.front().ownerSerial;

        auto it = std::lower_bound(
            segs.begin(), segs.end(), t,
            [](const PracticeSegment& seg, double value) {
                return seg.absEnd() <= value;
            }
        );

        if (it != segs.end() && t >= it->absStart() && t < it->absEnd()) {
            return it->ownerSerial;
        }

        if (it == segs.begin()) {
            return segs.front().ownerSerial;
        }

        return std::prev(it)->ownerSerial;
    }
    
    double replayEndTime() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session || session->segments.empty()) return 0.0;
        
        double maxEnd = 0.0;
        for (const auto& seg : session->segments) {
            maxEnd = std::max(maxEnd, seg.absEnd());
        }
        return maxEnd;
    }
    
    std::vector<PracticeSegment> getReplaySequence() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) return {};

        return session->segments;
    }

    std::vector<int> getPracticeSerialsInCurrentSession() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) return {};

        return session->allAttemptSerials;
    }
    
    void freezeForReplay() {
        m_path.frozen = true;
        PracticeSession* session = m_path.activeSession();
        if (session) session->frozen = true;
    }
    
    void unfreezeForRecording() {
        m_path.frozen = false;
        PracticeSession* session = m_path.activeSession();
        if (session) session->frozen = false;
    }

    void createNewSession() {
        PracticeSession session;
        session.sessionId = m_nextSessionId++;
        session.startX = 0.f;
        session.startY = 0.f;
        session.endX = 0.f;
        session.completed = false;
        session.frozen = false;
        m_path.frozen = false;
        
        m_path.sessions.push_back(session);
        m_path.activeSessionId = session.sessionId;
        m_path.selectedSessionId = session.sessionId;

        m_dirty = true;
        m_noValidSessionForStartX = false;
        currentFurthestSegment = PracticeSegment{};
        //log::info("[SESSION] createNewSession id={} total={}", session.sessionId, m_path.sessions.size() + 1);
    }
    
private:
    PracticePath m_path;
    bool m_dirty = false;
    int m_nextSessionId = 1;
    bool m_noValidSessionForStartX = false;
    PracticeSegment currentFurthestSegment; // For when you exit practice mode before finishing the level
    static constexpr double kSegEps_ = 1e-6;

    PracticeSession* sessionById_(int id) {
        for (auto& s : m_path.sessions) {
            if (s.sessionId == id) return &s;
        }
        return nullptr;
    }

    const PracticeSession* sessionById_(int id) const {
        for (auto const& s : m_path.sessions) {
            if (s.sessionId == id) return &s;
        }
        return nullptr;
    }

    void addAttemptSerialIfMissing_(PracticeSession& session, int serial) {
        if (serial <= 0) return;
        if (session.allAttemptSerials.empty() || session.allAttemptSerials.back() != serial) {
            session.allAttemptSerials.push_back(serial);
        }
    }

    PracticeSegment makeSegment_(
        int serial,
        double baseTimeOffset,
        double localStartTime,
        double localEndTime,
        float startX,
        float endX,
        size_t p1Frames,
        size_t p2Frames
    ) const {
        PracticeSegment seg;
        seg.startX = startX;
        seg.endX = endX;
        seg.ownerSerial = serial;
        seg.p1Frames = p1Frames;
        seg.p2Frames = p2Frames;
        seg.maxXReached = std::max(startX, endX);
        seg.baseTimeOffset = baseTimeOffset;
        seg.localStartTime = localStartTime;
        seg.localEndTime = localEndTime;
        return seg;
    }

    void updateCurrentFurthestSegment_(PracticeSegment const& seg) {
        if (!validSegment_(seg)) return;

        if (!validSegment_(currentFurthestSegment)) {
            currentFurthestSegment = seg;
            return;
        }

        if (seg.absEnd() > currentFurthestSegment.absEnd() + kSegEps_) {
            currentFurthestSegment = seg;
        }
    }

    static bool validSegment_(PracticeSegment const& s) {
        return s.ownerSerial > 0 && (s.absEnd() > s.absStart() + kSegEps_);
    }

    static void clampSegmentX_(PracticeSegment& s) {
        if (s.endX < s.startX) s.endX = s.startX;
        if (s.maxXReached < s.endX) s.maxXReached = s.endX;
    }

    static bool canMerge_(PracticeSegment const& a, PracticeSegment const& b) {
        return a.ownerSerial == b.ownerSerial &&
            b.absStart() <= a.absEnd() + 1e-5;
    }

    static void mergeIntoLeft_(PracticeSegment& a, PracticeSegment const& b) {
        const double newAbsEnd = std::max(a.absEnd(), b.absEnd());
        a.localEndTime = newAbsEnd - a.baseTimeOffset;
        a.endX = std::max(a.endX, b.endX);
        a.maxXReached = std::max(a.maxXReached, b.maxXReached);
        a.p1Frames = std::max(a.p1Frames, b.p1Frames);
        a.p2Frames = std::max(a.p2Frames, b.p2Frames);
    }

    static void mergeAdjacentSegments_(std::vector<PracticeSegment>& segs) {
        if (segs.empty()) return;

        std::vector<PracticeSegment> merged;
        merged.reserve(segs.size());

        for (auto const& s : segs) {
            if (!validSegment_(s)) continue;

            if (merged.empty()) {
                merged.push_back(s);
                continue;
            }

            PracticeSegment& back = merged.back();
            if (canMerge_(back, s)) {
                mergeIntoLeft_(back, s);
            } else {
                merged.push_back(s);
            }
        }

        segs.swap(merged);
    }

    void overwriteIntoTimeline_(
        std::vector<PracticeSegment>& segs,
        PracticeSegment const& newSeg,
        bool beganAtStartOfLevel
    ) {
        const double newAbsStart = newSeg.absStart();
        const double newAbsEnd   = newSeg.absEnd();

        if (!(newAbsEnd > newAbsStart + kSegEps_)) return;

        if (segs.empty()) {
            segs.push_back(newSeg);
            return;
        }

        std::vector<PracticeSegment> out;
        out.reserve(segs.size() + 1);

        bool inserted = false;

        for (auto const& old : segs) {
            const double oldStart = old.absStart();
            const double oldEnd   = old.absEnd();

            const bool oldEntirelyBeforeNew =
                beganAtStartOfLevel ? false : (oldEnd <= newAbsStart + kSegEps_);

            const bool oldEntirelyAfterNew =
                oldStart >= newAbsEnd - kSegEps_;

            if (oldEntirelyBeforeNew) {
                out.push_back(old);
                continue;
            }

            if (oldEntirelyAfterNew) {
                if (!inserted) {
                    out.push_back(newSeg);
                    inserted = true;
                }
                out.push_back(old);
                continue;
            }

            // Overlap case:
            // maybe preserve left remainder (only for non start of level attempts)
            if (!beganAtStartOfLevel && oldStart < newAbsStart - kSegEps_) {
                PracticeSegment left = old;
                left.localEndTime = newAbsStart - left.baseTimeOffset;
                if (validSegment_(left)) {
                    left.endX = std::min(left.endX, newSeg.startX);
                    clampSegmentX_(left);
                    out.push_back(left);
                }
            }

            if (!inserted) {
                out.push_back(newSeg);
                inserted = true;
            }

            // maybe preserve right remainder
            if (oldEnd > newAbsEnd + kSegEps_) {
                PracticeSegment right = old;
                right.localStartTime = newAbsEnd - right.baseTimeOffset;
                if (right.localStartTime < 0.0) right.localStartTime = 0.0;

                if (validSegment_(right)) {
                    right.startX = std::max(right.startX, newSeg.endX);
                    clampSegmentX_(right);
                    out.push_back(right);
                }
            }
        }

        if (!inserted) {
            out.push_back(newSeg);
        }

        std::sort(out.begin(), out.end(),
            [](PracticeSegment const& a, PracticeSegment const& b) {
                if (a.absStart() != b.absStart()) return a.absStart() < b.absStart();
                if (a.absEnd() != b.absEnd()) return a.absEnd() < b.absEnd();
                return a.ownerSerial < b.ownerSerial;
            });

        mergeAdjacentSegments_(out);
        segs.swap(out);
    }
};