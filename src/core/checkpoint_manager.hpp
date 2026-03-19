// checkpoint_manager.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <unordered_set>
#include <vector>
#include <Geode/binding/PlayLayer.hpp>
#include "types.hpp"


class CheckpointManager {
public:
    CheckpointManager() = default;

    int getCurrentCheckpointId(bool preferSelected = true) const {
        const PracticeSession* s = preferSelected ? m_path.selectedSession() : nullptr;
        if (!s) s = m_path.activeSession();
        if (!s || s->activeChain.empty()) return -1;
        return s->activeChain.back();
    }

    double getCurrentCheckpointBaseTime(bool preferSelected = true) const {
        const PracticeSession* s = preferSelected ? m_path.selectedSession() : nullptr;
        if (!s) s = m_path.activeSession();
        if (!s || s->activeChain.empty()) return 0.0;

        const int topId = s->activeChain.back();
        if (auto const* checkpoint = s->findCheckpoint(topId)) return checkpoint->checkpointTime;

        // Fallback (shouldn't happen if data is consistent)
        return 0.0;
    }

    Checkpoint* getCurrentCheckpoint(bool preferSelected = true) {
        PracticeSession* s = preferSelected ? m_path.selectedSession() : nullptr;
        if (!s) s = m_path.activeSession();
        if (!s || s->activeChain.empty()) return nullptr;

        int topId = s->activeChain.back();
        if (Checkpoint* checkpoint = s->findCheckpoint(topId)) return checkpoint;
        return nullptr;
    }

    // If explicitly want the active session (ignores selected)
    double getCurrentCheckpointBaseTimeActive() const {
        return getCurrentCheckpointBaseTime(false);
    }

    bool noValidSessionForStartX() const { return m_noValidSessionForStartX; }
    void clearNoValidSessionForStartX() { m_noValidSessionForStartX = false; }
    
    void attach(PlayLayer* pl) {
        m_pl = pl;
        m_noValidSessionForStartX = false;
        if (auto* s = m_path.activeSession()) {
            rebuildCheckpointBaseTimesFromSession_(*s);
        }
    }
    
    void restorePath(const PracticePath& path) {
        m_path = path;
        m_dirty = false;
        m_noValidSessionForStartX = false;

        // Find max checkpoint ID to avoid collisions
        m_nextCheckpointId = 1;
        for (const auto& session : m_path.sessions) {
            for (const auto& checkpoint : session.checkpoints) {
                if (checkpoint.id >= m_nextCheckpointId) {
                    m_nextCheckpointId = checkpoint.id + 1;
                }
            }
        }

        // Find max session ID
        m_nextSessionId = 1;
        for (const auto& session : m_path.sessions) {
            if (session.sessionId >= m_nextSessionId) {
                m_nextSessionId = session.sessionId + 1;
            }
        }

        // Repair loaded session segment timelines once
        //for (auto& session : m_path.sessions) {
        //    validateAndRepairSegments_(session);
        //}

        baseTimeForCheckpoints.clear();
        if (auto* s = m_path.activeSession()) {
            rebuildCheckpointBaseTimesFromSession_(*s);
        }
        currentFurthestSegment = PracticeSegment{};
    }

    static float sessionStartX_(PracticeSession const& s) {
        if (s.segments.empty()) return NAN;
        return s.segments.front().startX;
    }

    static double sessionAbsEnd_(PracticeSession const& s) {
        double best = 0.0;
        for (auto const& seg : s.segments) best = std::max(best, seg.absEnd());
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
                //geode::log::info("session not good x: {} x: {} curx: {}", s.sessionId, sx, curX);
                continue;   // FILTER ONLY
            }

            const double endT = sessionAbsEnd_(s);       // RANK BY TIME
            //geode::log::info("session: {} endT: {}", s.sessionId, endT);

            if (endT > bestEndT) {
                bestEndT = endT;
                bestId = s.sessionId;
            }
        }

        return bestId;
    }

    int selectBestSessionForStartX_RankByTime(float curX, float xTol) {
        //geode::log::info("m_path.selectedSessionId: {}", m_path.selectedSessionId);

        const int best = pickBestSessionIdForStartX_RankByTime(curX, xTol);
        currentFurthestSegment = PracticeSegment{};
        //geode::log::info("best: {}", best);

        if (best > 0) {
            if (auto* s = sessionById_(best)) {
                m_noValidSessionForStartX = false;

                m_path.selectedSessionId = best;
                m_path.activeSessionId = best;

                // keep basetime stack synced when session changes
                rebuildCheckpointBaseTimesFromSession_(*s);

                //geode::log::info(
                //    "BEST selectedSessionId={}, activeSessionId={}, numAttempts={}",
                //    m_path.selectedSessionId,
                //    m_path.activeSessionId,
                //    s->allAttemptSerials.size()
                //);
                return best;
            }

            m_noValidSessionForStartX = true;
            m_path.selectedSessionId = 0;
            //geode::log::info("No valid session object for selected id={} (stale)", best);
            return -1;
        }

        m_noValidSessionForStartX = true;
        m_path.selectedSessionId = 0;

        //geode::log::info("No valid session for startX={} tol={}", curX, xTol);
        return -1;
    }
        
    const PracticePath& getPath() const { return m_path; }
    PracticePath& getPath() { return m_path; }
    
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }
    void markDirty() { m_dirty = true; }
    
    // Get the top (most recent) checkpoint
    const Checkpoint* topCheckpoint() const {
        PracticeSession* session = const_cast<CheckpointManager*>(this)->m_path.activeSession();
        if (!session || session->activeChain.empty()) return nullptr;
        int topId = session->activeChain.back();
        return session->findCheckpoint(topId);
    }
    
    void onCheckpointPlaced(float x, float y, int attemptSerial,
                    float rot1, float rot2,
                    size_t p1Count, size_t p2Count,
                    double absCheckpointTime,
                    double attemptBaseTimeOffset,
                    int attemptStartCheckpointId) {
        PracticeSession* session = m_path.activeSession();
        if (!session) {
            createNewSession();
            session = m_path.activeSession();
        }
        if (!session) return;

        if (m_path.frozen || session->frozen) {
            //geode::log::info(
            //    "[CPM] CheckPointplace IGNORED (frozen). sessId={} pathFrozen={} sessFrozen={} attemptSerial={}",
            //    session->sessionId, (int)m_path.frozen, (int)session->frozen, attemptSerial
            //);
            return;
        }

        const size_t chainBefore = session->activeChain.size();
        const size_t cpsBefore   = session->checkpoints.size();
        const size_t btBefore    = baseTimeForCheckpoints.size();

        ensureCheckpointBaseTimesSynced_(*session);

        const size_t btAfterSync = baseTimeForCheckpoints.size();
        const double currentChainBase =
            baseTimeForCheckpoints.empty() ? 0.0 : baseTimeForCheckpoints.back();

        /*

        // validate this checkpoint against the immutable base of the attempt that is currently alive
        const double deltaFromAttemptBase = absCheckpointTime - attemptBaseTimeOffset;
        const double deltaFromCurrentChain = absCheckpointTime - currentChainBase;

        geode::log::info(
            "[CPM] CheckPointplace: sessId={} attemptSerial={} startCpId={} x={:.3f} y={:.3f} p1={} p2={} "
            "absT={:.6f} attemptBase={:.6f} deltaFromAttemptBase={:.6f} "
            "currentChainBase={:.6f} deltaFromCurrentChain={:.6f} "
            "sizes: chain={} cps={} baseTimes(beforeSync={} afterSync={})",
            session->sessionId, attemptSerial, attemptStartCheckpointId,
            x, y, (int)p1Count, (int)p2Count,
            absCheckpointTime,
            attemptBaseTimeOffset, deltaFromAttemptBase,
            currentChainBase, deltaFromCurrentChain,
            (int)chainBefore, (int)cpsBefore, (int)btBefore, (int)btAfterSync
        );

        if (chainBefore != btAfterSync) {
            geode::log::warn(
                "[CPM] CheckPointplace: baseTime stack mismatch after sync! sessId={} chain={} baseTimes={}",
                session->sessionId, (int)chainBefore, (int)btAfterSync
            );
        }

        if (std::fabs(deltaFromAttemptBase) <= 1e-6) {
            geode::log::warn(
                "[CPM] CheckPointplace: TIME COLLISION vs attempt base. sessId={} attemptSerial={} absT={:.6f} attemptBase={:.6f}",
                session->sessionId, attemptSerial, absCheckpointTime, attemptBaseTimeOffset
            );
        } else if (deltaFromAttemptBase < -1e-6) {
            geode::log::warn(
                "[CPM] CheckPointplace: TIME WENT BACKWARDS vs attempt base. sessId={} attemptSerial={} absT={:.6f} attemptBase={:.6f}",
                session->sessionId, attemptSerial, absCheckpointTime, attemptBaseTimeOffset
            );
        }*/

        Checkpoint checkpoint;
        checkpoint.id = m_nextCheckpointId++;
        checkpoint.x = x;
        checkpoint.y = y;
        checkpoint.rot1 = rot1;
        checkpoint.rot2 = rot2;
        checkpoint.rot1Valid = true;
        checkpoint.rot2Valid = (m_pl && m_pl->m_player2);
        checkpoint.checkpointTime = absCheckpointTime;

        session->checkpoints.push_back(checkpoint);
        session->activeChain.push_back(checkpoint.id);
        baseTimeForCheckpoints.push_back(absCheckpointTime);

        /*
        geode::log::info(
            "[CPM] CheckPointplaced: sessId={} checkpointId={} absT={:.6f} sizes(after): chain={} cps={} baseTimes={}",
            session->sessionId, checkpoint.id, checkpoint.checkpointTime,
            (int)session->activeChain.size(),
            (int)session->checkpoints.size(),
            (int)baseTimeForCheckpoints.size()
        );*/

        currentFurthestSegment = PracticeSegment{};
        m_dirty = true;
    }

    void onCheckpointRemoved() {
        PracticeSession* session = m_path.activeSession();
        if (!session || session->activeChain.empty()) return;

        if (m_path.frozen || session->frozen) {
            //geode::log::info(
            //    "[CPM] CheckPointremove IGNORED (frozen). sessId={} pathFrozen={} sessFrozen={}",
            //    session->sessionId, (int)m_path.frozen, (int)session->frozen
            //);
            return;
        }

        const size_t chainBefore = session->activeChain.size();
        const size_t cpsBefore   = session->checkpoints.size();
        const size_t btBefore    = baseTimeForCheckpoints.size();

        ensureCheckpointBaseTimesSynced_(*session);

        const int removedId = session->activeChain.back();
        const Checkpoint* removedCheckpoint= session->findCheckpoint(removedId);
        const bool hadCheckpoint= (removedCheckpoint!= nullptr);
        const double removedT = hadCheckpoint? removedCheckpoint->checkpointTime : 0.0;

        //geode::log::info(
        //    "[CPM] CheckPointremove: sessId={} removedId={} hadCheckpoint={} removedAbsT={:.6f} "
        //    "sizes(before): chain={} cps={} baseTimes={}",
        //    session->sessionId, removedId, (int)hadCheckpoint, removedT,
        //    (int)chainBefore, (int)cpsBefore, (int)btBefore
        //);

        session->activeChain.pop_back();

        bool erased = false;
        auto& cps = session->checkpoints;
        for (auto it = cps.rbegin(); it != cps.rend(); ++it) {
            if (it->id == removedId) {
                cps.erase(std::next(it).base());
                erased = true;
                break;
            }
        }

        /*

        if (!erased) {
            geode::log::warn(
                "[CPM] CheckpointRemove: could not find checkpoint object for removedId={} (sessId={})",
                removedId, session->sessionId
            );
        }*/

        if (!baseTimeForCheckpoints.empty()) {
            baseTimeForCheckpoints.pop_back();
        } //else {
        //    geode::log::warn("[CPM] CheckpointRemove: baseTimeForCheckpoints already empty (sessId={})", session->sessionId);
        //}

        /*

        geode::log::info(
            "[CPM] CheckpointRemoved: sessId={} removedId={} erasedObj={} sizes(after): chain={} cps={} baseTimes={}",
            session->sessionId, removedId, (int)erased,
            (int)session->activeChain.size(),
            (int)session->checkpoints.size(),
            (int)baseTimeForCheckpoints.size()
        );

        if (session->activeChain.size() != baseTimeForCheckpoints.size()) {
            geode::log::warn(
                "[CPM] CheckpointRemove: chain/baseTimes size mismatch! sessId={} chain={} baseTimes={}",
                session->sessionId,
                (int)session->activeChain.size(),
                (int)baseTimeForCheckpoints.size()
            );
        }*/
        currentFurthestSegment = PracticeSegment{};
        m_dirty = true;
    }
    
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
        if (!session) session = m_path.activeSession();
        if (!session) return;
        
        for (const auto& seg : session->segments) {
            if (seg.ownerSerial > 0) out.insert(seg.ownerSerial);
        }
    }

    bool isAtLevelStartNoCheckpoints() const {
        const PracticeSession* s = m_path.activeSession();
        if (!s) return true;
        return s->activeChain.empty();
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
            currentFurthestSegment.p2Frames,
            false
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
        size_t p2Frames,
        bool beganAtStartOfLevel
    ) {
        if (!(localEndTime > localStartTime)) return;

        PracticeSession* session = m_path.activeSession();
        if (!session) {
            createNewSession();
            session = m_path.activeSession();
        }
        if (!session) return;

        if (m_path.frozen || session->frozen) return;

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

        overwriteIntoTimeline_(session->segments, newSeg, beganAtStartOfLevel);

        session->updateSpan();
        m_dirty = true;
    }
    
    int findOwnerSerialForTime(double t) const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) session = m_path.activeSession();
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
    
    // Get the end time of the replay
    // CACHE THIS AND OTHER STUFF
    double replayEndTime() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) session = m_path.activeSession();
        if (!session || session->segments.empty()) return 0.0;
        
        double maxEnd = 0.0;
        for (const auto& seg : session->segments) {
            maxEnd = std::max(maxEnd, seg.absEnd());
        }
        return maxEnd;
    }
    
    // Get replay sequence (all segments in order)
    std::vector<PracticeSegment> getReplaySequence() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) session = m_path.activeSession();
        if (!session) return {};

        return session->segments;
    }

    std::vector<int> getPracticeSerialsInCurrentSession() const {
        const PracticeSession* session = m_path.selectedSession();
        if (!session) session = m_path.activeSession();
        if (!session) return {};

        return session->allAttemptSerials;
    }

    std::vector<int> getPracticeSerialsMatchingCurrentStartPos_AllSessions(int xTol) const {
        const PracticeSession* cur = m_path.selectedSession();
        if (!cur) cur = m_path.activeSession();
        if (!cur) return {};

        if (cur->segments.empty()) return {};

        const float targetX = sessionStartX_(*cur);
        if (!std::isfinite(targetX)) return {};

        // Collect sessions whose derived startX is within tolerance of the current session's startX
        std::vector<PracticeSession const*> matches;
        matches.reserve(m_path.sessions.size());

        for (auto const& s : m_path.sessions) {
            if (s.segments.empty()) continue;

            const float sx = sessionStartX_(s);
            if (!std::isfinite(sx)) continue;

            if (std::fabs(sx - targetX) <= (float)xTol) {
                matches.push_back(&s);
            }
        }

        // Rank by most recent end time
        std::sort(matches.begin(), matches.end(),
            [&](PracticeSession const* a, PracticeSession const* b) {
                return sessionAbsEnd_(*a) > sessionAbsEnd_(*b);
            });

        // Merge serials in ranked session order, de-dupe while preserving order
        std::unordered_set<int> seen;
        std::vector<int> out;
        out.reserve(512);

        for (auto const* s : matches) {
            for (int serial : s->allAttemptSerials) {
                if (serial <= 0) continue;
                if (seen.insert(serial).second) out.push_back(serial);
            }
        }

        return out;
    }
    
    // Freeze for replay (stop recording changes)
    void freezeForReplay() {
        m_path.frozen = true;
        PracticeSession* session = m_path.activeSession();
        if (session) session->frozen = true;
    }
    
    // Unfreeze for recording
    void unfreezeForRecording() {
        m_path.frozen = false;
        PracticeSession* session = m_path.activeSession();
        if (session) session->frozen = false;
    }
    
    // Validation helpers
    void validateAndRepairSegments_() {
        PracticeSession* session = m_path.activeSession();
        if (!session) return;
        validateAndRepairSegments_(*session);
    }

    void validateAndRepairSegments_(PracticeSession& session) {
        auto& segs = session.segments;

        segs.erase(
            std::remove_if(segs.begin(), segs.end(),
                [](PracticeSegment const& s) {
                    return !validSegment_(s);
                }),
            segs.end()
        );

        std::sort(segs.begin(), segs.end(),
            [](PracticeSegment const& a, PracticeSegment const& b) {
                if (a.absStart() != b.absStart()) return a.absStart() < b.absStart();
                if (a.absEnd()   != b.absEnd())   return a.absEnd()   < b.absEnd();
                return a.ownerSerial < b.ownerSerial;
            });

        // Different-owner overlaps: trim the later segment so the timeline stays non-overlapping.
        std::vector<PracticeSegment> out;
        out.reserve(segs.size());

        for (auto s : segs) {
            if (!validSegment_(s)) continue;

            if (out.empty()) {
                out.push_back(s);
                continue;
            }

            PracticeSegment& back = out.back();

            if (canMerge_(back, s)) {
                mergeIntoLeft_(back, s);
                continue;
            }

            if (s.absStart() < back.absEnd() - kSegEps_) {
                s.localStartTime = back.absEnd() - s.baseTimeOffset;
                if (s.localStartTime < 0.0) s.localStartTime = 0.0;
                if (!validSegment_(s)) continue;
                s.startX = std::max(s.startX, back.endX);
                clampSegmentX_(s);
            }

            out.push_back(s);
        }

        mergeAdjacentSegments_(out);
        segs.swap(out);
        session.updateSpan();
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
        baseTimeForCheckpoints.clear();
        currentFurthestSegment = PracticeSegment{};
    }
    
private:
    
    PlayLayer* m_pl = nullptr;
    PracticePath m_path;
    bool m_dirty = false;
    int m_nextCheckpointId = 1;
    int m_nextSessionId = 1;
    bool m_noValidSessionForStartX = false;
    std::vector<double> baseTimeForCheckpoints;
    PracticeSegment currentFurthestSegment; // For when you exit practice mode before finishing the level
    static constexpr double kSegEps_ = 1e-6;

    void rebuildCheckpointBaseTimesFromSession_(PracticeSession const& session) {
        baseTimeForCheckpoints.clear();
        baseTimeForCheckpoints.reserve(session.activeChain.size());

        double last = 0.0;
        for (int id : session.activeChain) {
            if (auto const* checkpoint= session.findCheckpoint(id)) {
                last = checkpoint->checkpointTime;
            }
            baseTimeForCheckpoints.push_back(last);
        }
    }

    void ensureCheckpointBaseTimesSynced_(PracticeSession const& session) {
        if (baseTimeForCheckpoints.size() != session.activeChain.size()) {
            rebuildCheckpointBaseTimesFromSession_(session);
            return;
        }

        // Keep top consistent (cheap sanity sync)
        if (!session.activeChain.empty()) {
            const int topId = session.activeChain.back();
            if (auto const* checkpoint= session.findCheckpoint(topId)) {
                if (!baseTimeForCheckpoints.empty()) {
                    baseTimeForCheckpoints.back() = checkpoint->checkpointTime;
                }
            }
        }
    }

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
        seg.checkpointIdEnd = -1;
        seg.p1Frames = p1Frames;
        seg.p2Frames = p2Frames;
        seg.maxXReached = std::max(startX, endX);
        seg.baseTimeOffset = baseTimeOffset;
        seg.localStartTime = localStartTime;
        seg.localEndTime = localEndTime;
        return seg;
    }

    void updateCurrentFurthestSegment_(PracticeSegment const& seg) {
        if (currentFurthestSegment.ownerSerial <= 0) {
            currentFurthestSegment = seg;
            return;
        }

        if (std::fabs(currentFurthestSegment.baseTimeOffset - seg.baseTimeOffset) > kSegEps_ ||
            seg.localEndTime > currentFurthestSegment.localEndTime) {
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

            // If this was a true start of level attempt, the new segment owns the full left side
            const bool oldEntirelyBeforeNew = beganAtStartOfLevel ? false : (oldEnd <= newAbsStart + kSegEps_);

            const bool oldEntirelyAfterNew = oldStart >= newAbsEnd - kSegEps_;

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