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

        for (auto const& s : m_path.sessions) {
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
        double localStartTime, double localEndTime,
        float startX, float endX,
        size_t p1Frames, size_t p2Frames
    ) {
        if (!(localEndTime > localStartTime)) return;

        PracticeSession* session = m_path.activeSession();
        if (!session) {
            createNewSession();
            session = m_path.activeSession();
        }
        if (!session) return;

        if (m_path.frozen || session->frozen) return;

        if (session->allAttemptSerials.empty() || session->allAttemptSerials.back() != serial) {
            session->allAttemptSerials.push_back(serial);
        }

        auto& segs = session->segments;

        const double absStartTime = baseTimeOffset + localStartTime;
        const double absEndTime   = baseTimeOffset + localEndTime;
        if (!(absEndTime > absStartTime)) return;

        const size_t segsBefore = segs.size();

        PracticeSegment newSeg;
        newSeg.baseTimeOffset  = baseTimeOffset;
        newSeg.localStartTime  = localStartTime;
        newSeg.localEndTime    = localEndTime;
        newSeg.startX          = startX;
        newSeg.endX            = endX;
        newSeg.ownerSerial     = serial;
        newSeg.p1Frames        = p1Frames;
        newSeg.p2Frames        = p2Frames;
        newSeg.maxXReached     = std::max(startX, endX);
        newSeg.checkpointIdEnd = -1;

        constexpr double kEps = 1e-9;
        if (currentFurthestSegment.ownerSerial <= 0 ||
            std::fabs(currentFurthestSegment.baseTimeOffset - newSeg.baseTimeOffset) > kEps ||
            newSeg.localEndTime > currentFurthestSegment.localEndTime) {
            currentFurthestSegment = newSeg;
        }

        // Fast path: empty timeline
        if (segs.empty()) {
            segs.push_back(newSeg);
            session->updateSpan();
            m_dirty = true;

            /*
            geode::log::info(
                "overwrite: serial={} base={} local=[{:.6f},{:.6f}] abs=[{:.6f},{:.6f}] startX={:.3f} endX={:.3f} segs(before)={} segs(after)={}",
                serial, baseTimeOffset, localStartTime, localEndTime,
                absStartTime, absEndTime, startX, endX,
                (int)segsBefore, (int)segs.size()
            );*/
            return;
        }

        // Fast append path: strictly after tail
        {
            const PracticeSegment& tail = segs.back();
            const double tailEnd = tail.absEnd();

            if (absStartTime >= tailEnd - kEps) {
                // Merge directly into tail if same owner and touching/overlapping at boundary
                if (tail.ownerSerial == serial && absStartTime <= tailEnd + 1e-5) {
                    PracticeSegment& back = segs.back();
                    back.localEndTime = std::max(back.localEndTime, absEndTime - back.baseTimeOffset);
                    back.endX = std::max(back.endX, endX);
                    back.maxXReached = std::max(back.maxXReached, std::max(startX, endX));
                    back.p1Frames = std::max(back.p1Frames, p1Frames);
                    back.p2Frames = std::max(back.p2Frames, p2Frames);
                } else {
                    segs.push_back(newSeg);
                }

                session->updateSpan();
                m_dirty = true;

                /*
                geode::log::info(
                    "overwrite: serial={} base={} local=[{:.6f},{:.6f}] abs=[{:.6f},{:.6f}] startX={:.3f} endX={:.3f} segs(before)={} segs(after)={}",
                    serial, baseTimeOffset, localStartTime, localEndTime,
                    absStartTime, absEndTime, startX, endX,
                    (int)segsBefore, (int)segs.size()
                );*/
                return;
            }
        }

        // Find first segment whose absEnd() > absStartTime
        auto itFirst = std::lower_bound(
            segs.begin(), segs.end(), absStartTime,
            [](PracticeSegment const& s, double t) {
                return s.absEnd() <= t;
            }
        );

        // Find first segment whose absStart() >= absEndTime
        auto itLast = std::lower_bound(
            segs.begin(), segs.end(), absEndTime,
            [](PracticeSegment const& s, double t) {
                return s.absStart() < t;
            }
        );

        const size_t iFirst = static_cast<size_t>(itFirst - segs.begin());
        const size_t iLast  = static_cast<size_t>(itLast  - segs.begin());

        std::vector<PracticeSegment> replacement;
        replacement.reserve(3);

        // Left remainder from first overlapped segment
        if (iFirst < segs.size()) {
            const PracticeSegment& s = segs[iFirst];
            const double s0 = s.absStart();
            const double s1 = s.absEnd();

            if (s0 < absStartTime && s1 > absStartTime + kEps) {
                PracticeSegment left = s;
                left.localEndTime = absStartTime - left.baseTimeOffset;

                if (left.localEndTime > left.localStartTime + kEps) {
                    // Conservative X trimming
                    left.endX = std::min(left.endX, startX);
                    if (left.endX < left.startX) left.endX = left.startX;

                    left.maxXReached = std::min(left.maxXReached, left.endX);
                    if (left.maxXReached < left.endX) left.maxXReached = left.endX;

                    replacement.push_back(left);
                }
            }
        }

        // New segment owns entire overwrite interval
        replacement.push_back(newSeg);

        // Right remainder from last overlapped segment
        if (iLast > 0 && (iLast - 1) < segs.size()) {
            const PracticeSegment& s = segs[iLast - 1];
            const double s0 = s.absStart();
            const double s1 = s.absEnd();

            if (s0 < absEndTime - kEps && s1 > absEndTime) {
                PracticeSegment right = s;
                right.localStartTime = absEndTime - right.baseTimeOffset;
                if (right.localStartTime < 0.0) right.localStartTime = 0.0;

                if (right.localEndTime > right.localStartTime + kEps) {
                    // Conservative X trimming
                    right.startX = std::max(right.startX, endX);
                    if (right.endX < right.startX) right.endX = right.startX;
                    if (right.maxXReached < right.endX) right.maxXReached = right.endX;

                    replacement.push_back(right);
                }
            }
        }

        // Replace overlapped window [iFirst, iLast)
        segs.erase(segs.begin() + iFirst, segs.begin() + iLast);
        segs.insert(segs.begin() + iFirst, replacement.begin(), replacement.end());

        // Local merge only near changed area
        auto mergeAt = [&](size_t idx) -> bool {
            if (idx == 0 || idx >= segs.size()) return false;

            PracticeSegment& a = segs[idx - 1];
            PracticeSegment& b = segs[idx];

            if (a.ownerSerial == b.ownerSerial &&
                b.absStart() <= a.absEnd() + 1e-5) {
                const double newAbsEnd = std::max(a.absEnd(), b.absEnd());
                a.localEndTime = newAbsEnd - a.baseTimeOffset;
                a.endX = std::max(a.endX, b.endX);
                a.maxXReached = std::max(a.maxXReached, b.maxXReached);
                a.p1Frames = std::max(a.p1Frames, b.p1Frames);
                a.p2Frames = std::max(a.p2Frames, b.p2Frames);
                segs.erase(segs.begin() + idx);
                return true;
            }

            return false;
        };

        size_t mergePos = (iFirst > 0 ? iFirst : 1);

        while (mergePos < segs.size() && mergeAt(mergePos)) {
            if (mergePos > 1) --mergePos;
        }
        while (mergePos + 1 < segs.size() && mergeAt(mergePos + 1)) {
            // keep merging forward if needed
        }

        session->updateSpan();
        m_dirty = true;

        /*
        geode::log::info(
            "overwrite: serial={} base={} local=[{:.6f},{:.6f}] abs=[{:.6f},{:.6f}] startX={:.3f} endX={:.3f} segs(before)={} segs(after)={}",
            serial, baseTimeOffset, localStartTime, localEndTime,
            absStartTime, absEndTime, startX, endX,
            (int)segsBefore, (int)segs.size()
        );*/
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
                [](const PracticeSegment& seg) {
                    return seg.ownerSerial <= 0 || !(seg.absEnd() > seg.absStart());
                }),
            segs.end()
        );

        std::sort(segs.begin(), segs.end(),
            [](const PracticeSegment& a, const PracticeSegment& b) {
                if (a.absStart() != b.absStart()) return a.absStart() < b.absStart();
                if (a.absEnd()   != b.absEnd())   return a.absEnd()   < b.absEnd();
                return a.ownerSerial < b.ownerSerial;
            });

        constexpr double kEps = 1e-5;

        std::vector<PracticeSegment> repaired;
        repaired.reserve(segs.size());

        for (const auto& s : segs) {
            if (!(s.absEnd() > s.absStart())) continue;

            if (repaired.empty()) {
                repaired.push_back(s);
                continue;
            }

            PracticeSegment& back = repaired.back();

            // Merge touching/overlapping same-owner segments
            if (back.ownerSerial == s.ownerSerial &&
                s.absStart() <= back.absEnd() + kEps) {
                const double newAbsEnd = std::max(back.absEnd(), s.absEnd());
                back.localEndTime = newAbsEnd - back.baseTimeOffset;
                back.endX = std::max(back.endX, s.endX);
                back.maxXReached = std::max(back.maxXReached, s.maxXReached);
                back.p1Frames = std::max(back.p1Frames, s.p1Frames);
                back.p2Frames = std::max(back.p2Frames, s.p2Frames);
                continue;
            }

            // No overlap
            if (s.absStart() >= back.absEnd() - kEps) {
                repaired.push_back(s);
                continue;
            }

            // Overlap with previous different-owner segment:
            // keep only the tail after the previous segment ends.
            if (s.absEnd() > back.absEnd() + kEps) {
                PracticeSegment trimmed = s;
                trimmed.localStartTime = back.absEnd() - trimmed.baseTimeOffset;
                if (trimmed.localStartTime < 0.0) trimmed.localStartTime = 0.0;

                if (trimmed.localEndTime > trimmed.localStartTime) {
                    trimmed.startX = std::max(trimmed.startX, back.endX);
                    if (trimmed.endX < trimmed.startX) trimmed.endX = trimmed.startX;
                    if (trimmed.maxXReached < trimmed.endX) trimmed.maxXReached = trimmed.endX;
                    repaired.push_back(trimmed);
                }
            }
        }

        segs.swap(repaired);
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
};