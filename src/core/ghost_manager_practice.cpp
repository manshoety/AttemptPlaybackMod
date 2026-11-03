#include "ghost_manager.hpp"

bool Ghosts::getCurrentPracticeStartLocation(float& startX, float& startPercent) const {
        if (!m_pl) return false;

        float x = NAN;
        float fallbackPercent = 0.f;

        if (!m_current.p1.empty()) {
            x = static_cast<float>(m_current.p1.front().x);
            fallbackPercent = m_current.startPercent;
        }
        else if (m_pl->m_startPosObject) {
            x = m_pl->m_startPosObject->getPositionX();
            fallbackPercent = m_pl->getCurrentPercent();
        }
        else if (std::isfinite(m_current.startX)) {
            x = m_current.startX;
            fallbackPercent = m_current.startPercent;
        }
        else if (m_pl->m_player1) {
            x = m_pl->m_player1->getPositionX();
            fallbackPercent = m_pl->getCurrentPercent();
        }

        if (!std::isfinite(x)) return false;

        startX = x;
        startPercent = getAccuratePercentForX(x, fallbackPercent);
        return true;
    }

void Ghosts::setPracticeReplaySessionSelection(std::vector<int> const& sessionIds) {
        m_requestedPracticeSessionIds.clear();
        m_requestedPracticeSessionIds.reserve(sessionIds.size());

        for (int id : sessionIds) {
            if (id > 0) m_requestedPracticeSessionIds.insert(id);
        }

        invalidateAttemptCounts();
    }

bool Ghosts::deletePracticeAttemptsBySerials(std::vector<int> const& serials) {
        std::unordered_set<int> deleteSet;
        deleteSet.reserve(serials.size());

        for (int serial : serials) {
            if (serial > 0) deleteSet.insert(serial);
        }

        return deletePracticeAttemptsBySerialSet_(deleteSet, false);
    }

bool Ghosts::isPracticeSessionBeingRecorded(int sessionId) const {
        if (sessionId <= 0) return false;
        if (!m_pl || !m_pl->m_isPracticeMode) return false;
        if (!recording || recordingBlocked || !recordInPractice) return false;

        auto const& path = m_checkpointMgr.getPath();
        if (path.frozen || path.activeSessionId != sessionId) return false;

        auto const* session = path.activeSession();
        return session && !session->frozen;
    }

bool Ghosts::deletePracticeSessionById(int sessionId) {
        if (sessionId <= 0) return false;
        if (!hasCurrentPersistenceTarget_()) return false;

        if (isPracticeSessionBeingRecorded(sessionId)) {
            log::warn(
                "[APX delete] refusing to delete active practice session {} while it is being recorded",
                sessionId
            );
            return false;
        }

        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return false;
        }

        const PracticeSession* targetSession = nullptr;
        for (auto const& session : m_attemptCatalogPracticePath.sessions) {
            if (session.sessionId == sessionId) {
                targetSession = &session;
                break;
            }
        }

        if (!targetSession) return false;

        std::unordered_set<int> deleteSet;
        for (int serial : targetSession->allAttemptSerials) {
            if (serial > 0) deleteSet.insert(serial);
        }
        for (auto const& seg : targetSession->segments) {
            if (seg.ownerSerial > 0) deleteSet.insert(seg.ownerSerial);
        }

        if (deleteSet.empty()) {
            PracticePath keptPath = m_attemptCatalogPracticePath;
            keptPath.sessions.erase(
                std::remove_if(
                    keptPath.sessions.begin(),
                    keptPath.sessions.end(),
                    [&](PracticeSession const& session) {
                        return session.sessionId == sessionId;
                    }
                ),
                keptPath.sessions.end()
            );
            return rewriteAttemptsAndPracticePathAfterDeletion_(deleteSet, keptPath, "practice-session");
        }

        PracticePath keptPath = m_attemptCatalogPracticePath;
        keptPath.sessions.erase(
            std::remove_if(
                keptPath.sessions.begin(),
                keptPath.sessions.end(),
                [&](PracticeSession const& session) {
                    return session.sessionId == sessionId;
                }
            ),
            keptPath.sessions.end()
        );

        return rewriteAttemptsAndPracticePathAfterDeletion_(deleteSet, keptPath, "practice-session");
    }

bool Ghosts::rewriteAttemptsAndPracticePathAfterDeletion_(
        std::unordered_set<int> const& deleteSet,
        PracticePath const& keptPath,
        char const* label) {
        if (!hasCurrentPersistenceTarget_()) return false;
        if (m_isSaving) return false;

        m_isSaving = true;
        struct SaveGuard {
            bool& flag;
            ~SaveGuard() { flag = false; }
        } guard{ m_isSaving };

        clearRuntimeReplayStateAfterAttemptDeletion_();

        const auto path = fileForLevel_(m_levelIDOnAttach);

        std::vector<Attempt> keptAttempts;
        keptAttempts.reserve(m_attemptCatalog.size());

        for (auto const& entry : m_attemptCatalog) {
            if (deleteSet.count(entry.serial) != 0) {
                continue;
            }

            Attempt loaded{};
            bool usedLegacy = false;

            if (!loadAPXAttemptByCatalogEntry(path, entry, loaded, &usedLegacy)) {
                log::warn(
                    "[APX delete] failed to lazy-load kept attempt serial={} from {}",
                    entry.serial,
                    geode::utils::string::pathToString(path)
                );
                return false;
            }

            loaded.persistedOnDisk = true;
            loaded.recordedThisSession = false;
            keptAttempts.push_back(std::move(loaded));
        }

        if (!saveAPXFileCurrent(path, keptAttempts, keptPath)) {
            log::warn(
                "[APX delete] failed to rewrite APX file after {} deletion: {}",
                label ? label : "attempt",
                geode::utils::string::pathToString(path)
            );
            return false;
        }

        m_checkpointMgr.restorePath(keptPath);
        removeDeletedAttemptsFromRuntime_(deleteSet);
        clearRuntimeReplayStateAfterAttemptDeletion_();
        clearAttemptCatalog_();
        scanAttemptCatalogForLevel_(m_levelIDOnAttach);

        armFreshRecordingAfterDeleteRestart_();
        restartLevel();

        return true;
    }

bool Ghosts::deletePracticeAttemptsBySerialSet_(
        std::unordered_set<int> const& deleteSet,
        bool allowReplayPathSerials) {
        if (deleteSet.empty()) return false;
        if (!hasCurrentPersistenceTarget_()) return false;

        if (m_pl && m_pl->m_isPracticeMode && recording && !recordingBlocked && recordInPractice) {
            auto const& path = m_checkpointMgr.getPath();
            auto const* active = path.activeSession();

            if (active && !path.frozen && !active->frozen) {
                bool touchesActiveSession = deleteSet.count(m_current.serial) != 0;

                if (!touchesActiveSession) {
                    for (int serial : active->allAttemptSerials) {
                        if (deleteSet.count(serial) != 0) {
                            touchesActiveSession = true;
                            break;
                        }
                    }
                }

                if (!touchesActiveSession) {
                    for (auto const& seg : active->segments) {
                        if (deleteSet.count(seg.ownerSerial) != 0) {
                            touchesActiveSession = true;
                            break;
                        }
                    }
                }

                if (touchesActiveSession) {
                    log::warn(
                        "[APX delete] refusing to delete attempts from active practice session {} while it is being recorded",
                        active->sessionId
                    );
                    return false;
                }
            }
        }

        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return false;
        }

        bool foundAny = false;

        for (auto const& entry : m_attemptCatalog) {
            if (deleteSet.count(entry.serial) == 0) continue;

            if (!entry.practiceAttempt) {
                log::warn(
                    "[APX delete] refusing practice-attempt delete because serial={} is normal",
                    entry.serial
                );
                return false;
            }

            foundAny = true;
        }

        if (!foundAny) return false;

        if (!allowReplayPathSerials) {
            for (auto const& session : m_attemptCatalogPracticePath.sessions) {
                for (auto const& seg : session.segments) {
                    if (seg.ownerSerial > 0 && deleteSet.count(seg.ownerSerial) != 0) {
                        log::warn(
                            "[APX delete] refusing individual practice delete for replay-path serial={}",
                            seg.ownerSerial
                        );
                        return false;
                    }
                }
            }
        }

        PracticePath keptPath = m_attemptCatalogPracticePath;
        prunePracticePathSerials_(keptPath, deleteSet);

        return rewriteAttemptsAndPracticePathAfterDeletion_(deleteSet, keptPath, "practice-attempt");
    }

void Ghosts::onPracticeToggle(bool enabled) {
        // log::info("[onPracticeToggle] enabled={}", enabled);

        if (recording && !recordingBlocked && recordInPractice && !enabled) {
            if (!m_current.p1.empty()) {
                double startTime = m_current.baseTimeOffset;
                float startX = m_current.p1.front().x;
                float endX = m_current.p1.back().x;
                
                m_checkpointMgr.overwriteSegmentForAttempt(
                    m_current.serial,
                    m_current.baseTimeOffset,
                    0,
                    m_current.p1.back().t - m_current.baseTimeOffset,
                    startX,
                    endX,
                    m_current.p1.size(),
                    m_current.p2.size()
                );
                pushAttempt(std::move(m_current));
            }
            m_checkpointMgr.onExitEarly();
        }

        if (playback) setActiveGhostsInvisible();
        if (!isRecording()) toggleRecording();

        practiceAttemptReplayIndex.clear();

        forcePlayersVisible_();
        clearHeldInputs_();

        if (!recordingBlocked) {
            if (enabled) {
                m_practiceCompositeOwnerSerial = -1;
                renumberCurrentAttemptIfFresh();
                m_current.practiceAttempt = true;

                m_checkpointMgr.createNewSession();
                m_pendingRearm = false;
            }
            else {
                m_checkpointMgr.freezeForReplay();

                saveCurrentAttemptNow();
                flushPendingSaves_();

                m_pendingRearm = true;
            }
        }

        m_prevPractice = enabled;
    }

int Ghosts::startModeForReplayFrame_(IconType mode) {
        switch (mode) {
            case IconType::Ship:    return 1;
            case IconType::Ball:    return 2;
            case IconType::Ufo:     return 3;
            case IconType::Wave:    return 4;
            case IconType::Robot:   return 5;
            case IconType::Spider:  return 6;
            case IconType::Swing:   return 7;
            case IconType::Jetpack: return 1;
            case IconType::Cube:
            default:                return 0;
        }
    }

float Ghosts::practiceSessionEndX_(PracticeSession const& session) {
        float endX = session.endX;
        for (auto const& seg : session.segments) {
            endX = std::max(endX, seg.endX);
            endX = std::max(endX, seg.maxXReached);
        }
        return endX;
    }

float Ghosts::practiceSpliceScore_(Frame const& a, Frame const& b) {
        const bool miniA = static_cast<float>(a.vehicleSize) < 0.9f;
        const bool miniB = static_cast<float>(b.vehicleSize) < 0.9f;

        if (a.mode != b.mode || miniA != miniB || a.upsideDown != b.upsideDown) {
            return std::numeric_limits<float>::max();
        }

        const float dx = static_cast<float>(a.x) - static_cast<float>(b.x);
        const float dy = static_cast<float>(a.y) - static_cast<float>(b.y);

        if (std::fabs(dx) > kPracticeStitchMaxPositionDelta ||
            std::fabs(dy) > kPracticeStitchMaxPositionDelta) {
            return std::numeric_limits<float>::max();
        }

        float rotDelta = std::fabs(static_cast<float>(a.rot) - static_cast<float>(b.rot));
        while (rotDelta >= 360.f) rotDelta -= 360.f;
        if (rotDelta > 180.f) rotDelta = 360.f - rotDelta;

        float score = dx * dx + dy * dy;
        score += rotDelta * 0.01f;
        if (a.hold != b.hold) score += 4.f;
        if (a.holdL != b.holdL) score += 4.f;
        if (a.holdR != b.holdR) score += 4.f;
        if (a.isDashing != b.isDashing) score += 8.f;
        return score;
    }

float Ghosts::practiceMotionScore_(
        std::vector<Frame> const& a,
        size_t ai,
        std::vector<Frame> const& b,
        size_t bi) {
        if (ai == 0 || bi == 0 || ai >= a.size() || bi >= b.size()) return 0.f;

        const float adx = static_cast<float>(a[ai].x) - static_cast<float>(a[ai - 1].x);
        const float ady = static_cast<float>(a[ai].y) - static_cast<float>(a[ai - 1].y);
        const float bdx = static_cast<float>(b[bi].x) - static_cast<float>(b[bi - 1].x);
        const float bdy = static_cast<float>(b[bi].y) - static_cast<float>(b[bi - 1].y);
        const float dx = adx - bdx;
        const float dy = ady - bdy;

        if (std::fabs(dx) > kPracticeStitchMaxMotionDelta ||
            std::fabs(dy) > kPracticeStitchMaxMotionDelta) {
            return std::numeric_limits<float>::max();
        }

        return (dx * dx + dy * dy) * 2.f;
    }

Ghosts::PracticeSplice_ Ghosts::findPracticeSplice_(
        std::vector<PracticeSegment> const& route,
        PracticeSession const& nextSession) {
        PracticeSplice_ best;

        for (auto const& routeSeg : route) {
            if (routeSeg.ownerSerial <= 0) continue;

            for (auto const& nextSeg : nextSession.segments) {
                if (nextSeg.ownerSerial <= 0) continue;

                const double overlapStart = std::max(routeSeg.absStart(), nextSeg.absStart());
                const double overlapEnd = std::min(routeSeg.absEnd(), nextSeg.absEnd());
                if (overlapEnd + 1e-5 < overlapStart) continue;

                if (!ensureAttemptLoadedBySerialAndPractice_(routeSeg.ownerSerial, true, false)) continue;
                if (!ensureAttemptLoadedBySerialAndPractice_(nextSeg.ownerSerial, true, false)) continue;

                auto const* routeOwner = findLoadedAttemptBySerialAndPractice_(routeSeg.ownerSerial, true);
                auto const* nextOwner = findLoadedAttemptBySerialAndPractice_(nextSeg.ownerSerial, true);
                if (!routeOwner || !nextOwner || routeOwner->p1.empty() || nextOwner->p1.empty()) continue;

                constexpr int samples = 32;
                for (int sample = 0; sample <= samples; ++sample) {
                    const double alpha = samples == 0 ? 0.0 : static_cast<double>(sample) / samples;
                    const double time = overlapStart + (overlapEnd - overlapStart) * alpha;

                    const size_t routeIdx = idxForTime(routeOwner->acc1Time, routeOwner->p1, time);
                    const size_t nextIdx = idxForTime(nextOwner->acc1Time, nextOwner->p1, time);
                    if (routeIdx >= routeOwner->p1.size() || nextIdx >= nextOwner->p1.size()) continue;

                    float score = practiceSpliceScore_(
                        routeOwner->p1[routeIdx],
                        nextOwner->p1[nextIdx]
                    );
                    if (!std::isfinite(score)) continue;

                    const float motionScore = practiceMotionScore_(
                        routeOwner->p1,
                        routeIdx,
                        nextOwner->p1,
                        nextIdx
                    );
                    if (!std::isfinite(motionScore)) continue;
                    score += motionScore;

                    const bool routeDual = practiceDualAtTime_(*routeOwner, time);
                    const bool nextDual = practiceDualAtTime_(*nextOwner, time);
                    if (routeDual != nextDual) continue;

                    if (routeDual) {
                        const size_t routeIdx2 = idxForTime(routeOwner->acc2Time, routeOwner->p2, time);
                        const size_t nextIdx2 = idxForTime(nextOwner->acc2Time, nextOwner->p2, time);
                        if (routeIdx2 >= routeOwner->p2.size() || nextIdx2 >= nextOwner->p2.size()) continue;

                        const float p2Score = practiceSpliceScore_(
                            routeOwner->p2[routeIdx2],
                            nextOwner->p2[nextIdx2]
                        );
                        if (!std::isfinite(p2Score)) continue;

                        const float p2MotionScore = practiceMotionScore_(
                            routeOwner->p2,
                            routeIdx2,
                            nextOwner->p2,
                            nextIdx2
                        );
                        if (!std::isfinite(p2MotionScore)) continue;

                        score += p2Score + p2MotionScore;
                    }

                    if (!best.valid || score < best.score - 0.001f ||
                        (std::fabs(score - best.score) <= 0.001f && time > best.time)) {
                        best.valid = true;
                        best.time = time;
                        best.score = score;
                    }
                }
            }
        }

        return best;
    }

void Ghosts::trimPracticeRouteAfter_(std::vector<PracticeSegment>& route, double time) {
        std::vector<PracticeSegment> trimmed;
        trimmed.reserve(route.size());

        for (auto seg : route) {
            if (seg.absStart() >= time - 1e-6) break;

            if (seg.absEnd() > time) {
                seg.localEndTime = time - seg.baseTimeOffset;
            }

            if (seg.ownerSerial > 0 && seg.absEnd() > seg.absStart() + 1e-6) {
                trimmed.push_back(seg);
            }
        }

        route.swap(trimmed);
    }

void Ghosts::appendPracticeSessionFrom_(
        std::vector<PracticeSegment>& route,
        PracticeSession const& session,
        double time) {
        for (auto seg : session.segments) {
            if (seg.absEnd() <= time + 1e-6) continue;

            if (seg.absStart() < time) {
                seg.localStartTime = time - seg.baseTimeOffset;
            }

            if (seg.ownerSerial > 0 && seg.absEnd() > seg.absStart() + 1e-6) {
                route.push_back(seg);
            }
        }
    }

bool Ghosts::buildSelectedPracticeReplayRoute_() {
        m_checkpointMgr.clearReplayOverrideSegments();
        m_practiceReplayRouteStartSessionId = 0;

        std::vector<PracticeSession const*> selected;
        selected.reserve(m_requestedPracticeSessionIds.size());
 
         for (auto const& session : m_checkpointMgr.getPath().sessions) {
            if (!m_requestedPracticeSessionIds.contains(session.sessionId)) continue;
            if (session.segments.empty()) continue;
            selected.push_back(&session);
        }

        if (selected.empty()) return false;

        std::sort(selected.begin(), selected.end(), [](PracticeSession const* a, PracticeSession const* b) {
            const float ax = CheckpointManager::sessionStartX_(*a);
            const float bx = CheckpointManager::sessionStartX_(*b);
            if (ax != bx) return ax < bx;
            return practiceSessionEndX_(*a) > practiceSessionEndX_(*b);
        });

        float selectedStartX = CheckpointManager::sessionStartX_(*selected.front());
        float selectedEndX = practiceSessionEndX_(*selected.front());

        if (!std::isfinite(selectedStartX) || !std::isfinite(selectedEndX)) return false;

        for (size_t i = 1; i < selected.size(); ++i) {
            const float startX = CheckpointManager::sessionStartX_(*selected[i]);
            const float endX = practiceSessionEndX_(*selected[i]);
            if (!std::isfinite(startX) || !std::isfinite(endX)) return false;

            if (startX > selectedEndX + kPracticeStitchXOverlapTolerance) {
                log::warn(
                    "[Practice replay] selected runs have a gap from x={:.2f} to x={:.2f}",
                    selectedEndX,
                    startX
                );
                return false;
            }

            selectedEndX = std::max(selectedEndX, endX);
        }

        PracticeReplayRoute_ bestRoute;
        bool haveBest = false;

        for (auto const* seed : selected) {
            PracticeReplayRoute_ route;
            route.segments = seed->segments;
            route.startSessionId = seed->sessionId;
            route.startX = CheckpointManager::sessionStartX_(*seed);
            route.endX = practiceSessionEndX_(*seed);

            std::unordered_set<int> used;
            used.insert(seed->sessionId);

            while (true) {
                PracticeSession const* bestNext = nullptr;
                PracticeSplice_ bestSplice;
                float bestNextEndX = route.endX;

                for (auto const* candidate : selected) {
                    if (used.contains(candidate->sessionId)) continue;

                    const float candidateStartX = CheckpointManager::sessionStartX_(*candidate);
                    const float candidateEndX = practiceSessionEndX_(*candidate);

                    if (!std::isfinite(candidateStartX) || !std::isfinite(candidateEndX)) continue;
                    if (candidateEndX <= route.endX + kPracticeStitchMinExtension) continue;
                    if (candidateStartX > route.endX + kPracticeStitchXOverlapTolerance) continue;

                    PracticeSplice_ splice = findPracticeSplice_(route.segments, *candidate);
                    if (!splice.valid) continue;

                    if (!bestNext ||
                        candidateEndX > bestNextEndX + kPracticeStitchMinExtension ||
                        (std::fabs(candidateEndX - bestNextEndX) <= kPracticeStitchMinExtension &&
                            splice.score < bestSplice.score - 0.001f) ||
                        (std::fabs(candidateEndX - bestNextEndX) <= kPracticeStitchMinExtension &&
                            std::fabs(splice.score - bestSplice.score) <= 0.001f &&
                            candidate->sessionId > bestNext->sessionId)) {
                        bestNext = candidate;
                        bestSplice = splice;
                        bestNextEndX = candidateEndX;
                    }
                }

                if (!bestNext) break;

                trimPracticeRouteAfter_(route.segments, bestSplice.time);
                appendPracticeSessionFrom_(route.segments, *bestNext, bestSplice.time);
                route.endX = bestNextEndX;
                route.stitches++;
                used.insert(bestNext->sessionId);
            }

            if (!haveBest ||
                route.endX > bestRoute.endX + kPracticeStitchMinExtension ||
                (std::fabs(route.endX - bestRoute.endX) <= kPracticeStitchMinExtension &&
                    route.startX < bestRoute.startX - kPracticeStitchMinExtension) ||
                (std::fabs(route.endX - bestRoute.endX) <= kPracticeStitchMinExtension &&
                    std::fabs(route.startX - bestRoute.startX) <= kPracticeStitchMinExtension &&
                    route.stitches > bestRoute.stitches) ||
                (std::fabs(route.endX - bestRoute.endX) <= kPracticeStitchMinExtension &&
                    std::fabs(route.startX - bestRoute.startX) <= kPracticeStitchMinExtension &&
                    route.stitches == bestRoute.stitches &&
                    route.startSessionId > bestRoute.startSessionId)) {
                bestRoute = std::move(route);
                haveBest = true;
             }
         }
 
        if (!haveBest || bestRoute.segments.empty() || bestRoute.startSessionId <= 0) {
            return false;
        }

        if (
            bestRoute.startX > selectedStartX + kPracticeStitchXOverlapTolerance ||
            bestRoute.endX < selectedEndX - kPracticeStitchMinExtension
        ) {
            log::warn(
                "[Practice replay] selected runs overlap but no clean splice covers x={:.2f} to x={:.2f}",
                selectedStartX,
                selectedEndX
            );
            return false;
        }

        if (m_checkpointMgr.selectSessionById(bestRoute.startSessionId) <= 0) {
            return false;
        }

        m_practiceReplayRouteStartSessionId = bestRoute.startSessionId;
        m_checkpointMgr.setReplayOverrideSegments(std::move(bestRoute.segments));

        log::info(
            "[Practice replay] selected route startSession={} startX={:.2f} endX={:.2f} stitches={}",
            m_practiceReplayRouteStartSessionId,
            bestRoute.startX,
            bestRoute.endX,
            bestRoute.stitches
        );
        return true;
     }

void Ghosts::clearReplayStartPosOverride_() {
        if (m_pl && m_replayStartPosObject) {
            m_pl->setStartPosObject(m_savedStartPosObject);
        }

        m_pendingPracticeStartPosInit = false;
        m_pendingPracticeStartPosFrames = 0;
        m_replayStartPosObject = nullptr;
        m_replayStartSettings = nullptr;
        m_savedStartPosObject = nullptr;
    }

bool Ghosts::installReplayStartPosForSelectedPracticeSession_() {
        if (m_requestedPracticeSessionIds.empty()) return true;
        if (!m_pl) return false;

        auto replaySegments = m_checkpointMgr.getReplaySequence();
        if (replaySegments.empty()) {
            log::warn("[Practice replay] selected route has no segments");
            return false;
        }

        auto const& firstSegment = replaySegments.front();
        if (firstSegment.ownerSerial <= 0) {
            log::warn("[Practice replay] selected route has no first owner serial");
            return false;
        }

        Attempt* owner = ensureAttemptLoadedBySerialAndPractice_(firstSegment.ownerSerial, true, false);
        if (!owner || owner->p1.empty()) {
            log::warn("[Practice replay] could not load first owner {} for selected primary session", firstSegment.ownerSerial);
            return false;
        }

        size_t frameIndex = idxForTime(owner->acc1Time, owner->p1, firstSegment.absStart());
        if (frameIndex >= owner->p1.size()) frameIndex = 0;
        auto const& frame = owner->p1[frameIndex];

        LevelSettingsObject* settings = nullptr;
        if (m_pl->m_levelSettings) {
            settings = LevelSettingsObject::objectFromString(m_pl->m_levelSettings->getSaveString());
        }
        if (!settings) settings = LevelSettingsObject::create();

        auto* startPos = StartPosObject::create();
        if (!settings || !startPos) {
            log::warn("[Practice replay] failed to create temporary start-position objects");
            return false;
        }

        settings->m_level = m_pl->m_level;
        settings->m_startMode = startModeForReplayFrame_(frame.mode);
        settings->m_startMini = static_cast<float>(frame.vehicleSize) < 0.9f;
        const uint32_t startTQ = runtimeQuantTimeQ_(firstSegment.absStart());

        const bool dualAtReplayStart = owner->hadDual && !owner->p2.empty() && owner->p2.front().t.q <= startTQ;

        settings->m_startDual = dualAtReplayStart;
        settings->m_isFlipped = frame.upsideDown;
        settings->m_startsWithStartPos = true;
        settings->m_disableStartPos = false;
        settings->m_resetCamera = false;

        const cocos2d::CCPoint startPoint = {
            static_cast<float>(frame.x),
            static_cast<float>(frame.y)
        };

        startPos->m_objectID = 31;
        startPos->setPosition(startPoint);
        startPos->setStartPos(startPoint);
        startPos->updateStartValues();
        startPos->setSettings(settings);

        clearReplayStartPosOverride_();
        m_savedStartPosObject = m_pl->m_startPosObject;
        m_replayStartSettings = settings;
        m_replayStartPosObject = startPos;
        m_currentReplayStartPos = startPoint;
        m_pl->m_currentCheckpoint = nullptr;
        m_pl->setStartPosObject(startPos);

        log::info(
            "[Practice replay] temporary start pos placed for route session {} at ({:.2f}, {:.2f}), mode={}, mini={}",
            m_practiceReplayRouteStartSessionId,
            static_cast<float>(frame.x),
            static_cast<float>(frame.y),
            settings->m_startMode,
            settings->m_startMini
        );
        return true;
    }

float Ghosts::practicePreloadTargetStartX_() const {
        if (m_filterByStartPosition && std::isfinite(m_currentReplayStartPos.x)) {
            return m_currentReplayStartPos.x;
        }

        if (m_pl && m_pl->m_player1) return m_pl->m_player1->m_positionX;

        if (auto const* s = m_checkpointMgr.getPath().selectedSession()) {
            const float sx = CheckpointManager::sessionStartX_(*s);
            if (std::isfinite(sx)) return sx;
        }

        if (auto const* s = m_checkpointMgr.getPath().activeSession()) {
            const float sx = CheckpointManager::sessionStartX_(*s);
            if (std::isfinite(sx)) return sx;
        }

        return NAN;
    }

bool Ghosts::rebuildPracticePreloadSerialSetForCurrentStart_() {
        m_practiceSerialSet.clear();

        if (!m_requestedPracticeSessionIds.empty()) {
            size_t reserveCount = 0;
            for (auto const& session : m_checkpointMgr.getPath().sessions) {
                if (!m_requestedPracticeSessionIds.contains(session.sessionId)) continue;
                reserveCount += session.allAttemptSerials.size() + session.segments.size();
            }
            m_practiceSerialSet.reserve(reserveCount);

            for (auto const& session : m_checkpointMgr.getPath().sessions) {
                if (!m_requestedPracticeSessionIds.contains(session.sessionId)) continue;

                for (int serial : session.allAttemptSerials) {
                    if (serial > 0) m_practiceSerialSet.insert(serial);
                }
                for (auto const& seg : session.segments) {
                    if (seg.ownerSerial > 0) m_practiceSerialSet.insert(seg.ownerSerial);
                }
            }

            return !m_practiceSerialSet.empty();
        }

        const float targetX = practicePreloadTargetStartX_();
        if (!std::isfinite(targetX)) {
            log::warn("[Practice preload] no finite target start X");
            return false;
        }

        const int selectedSession =
            m_checkpointMgr.selectBestSessionForStartX_RankByTime(targetX, kReplayStartTolerance);

        if (selectedSession <= 0 || m_checkpointMgr.noValidSessionForStartX()) {
            log::warn("[Practice preload] no valid session for targetX={} xTol={}", targetX, kReplayStartTolerance);
            return false;
        }

        const auto serials =
            m_checkpointMgr.getPracticeSerialsMatchingStartX_AllSessions(targetX, static_cast<int>(kReplayStartTolerance));

        m_practiceSerialSet.reserve(serials.size());

        for (int serial : serials) {
            if (serial > 0) m_practiceSerialSet.insert(serial);
        }

        return !m_practiceSerialSet.empty();
    }

void Ghosts::applyBotSafety_(bool on) {
        safeMode_enabled = on;
        noclip_enabled = on;
        if (on) m_runUsedBot = true;
        if (on) cheatAPI::setCheat();
        else cheatAPI::endCheat();
    }

