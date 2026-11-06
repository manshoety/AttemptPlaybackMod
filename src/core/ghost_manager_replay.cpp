#include "ghost_manager.hpp"

void Ghosts::buildCompositeFromAttempt(size_t idx) {
        m_activeRoute = ActiveRoute::SingleAttempt;
        m_replayAttempt = nullptr;

        if (idx >= attempts.size()) {
            m_compOwnerIdx = SIZE_MAX;
            return;
        }

        auto& a = attempts[idx];

        m_compOwnerIdx = idx;
        m_replayOwnerSerial = a.serial;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;
        m_replayIdx1 = m_replayIdx2 = 0;

        if (botActive) {
            tagCompositeOwnersHidden_(true);
        }
    }

const Attempt* Ghosts::findBestAttemptForRange_(float x0, float x1, 
        std::optional<bool> wantPractice) const 
    {
        const Attempt* best = nullptr;
        float bestOverlap = 0.0f;
        float bestEndX = -1.0f;
        
        for (const auto& a : attempts) {
            if (!matchesPracticeFilter_(a, wantPractice)) continue;
            if (a.p1.empty()) continue;
            
            float aStart = a.p1.front().x;
            float aEnd = a.p1.back().x;
            
            float overlapStart = std::max(aStart, x0);
            float overlapEnd = std::min(aEnd, x1);
            float overlap = std::max(0.0f, overlapEnd - overlapStart);
            
            if (overlap > bestOverlap || 
                (overlap == bestOverlap && aEnd > bestEndX)) {
                best = &a;
                bestOverlap = overlap;
                bestEndX = aEnd;
            }
        }
        
        if (matchesPracticeFilter_(m_current, wantPractice) && !m_current.p1.empty()) {
            float aStart = m_current.p1.front().x;
            float aEnd = m_current.p1.back().x;
            float overlapStart = std::max(aStart, x0);
            float overlapEnd = std::min(aEnd, x1);
            float overlap = std::max(0.0f, overlapEnd - overlapStart);
            
            if (overlap > bestOverlap || 
                (overlap == bestOverlap && aEnd > bestEndX)) {
                best = &m_current;
            }
        }
        
        return best;
    }

void Ghosts::syncInputsToCurrentIndexP1_() {
        if (!m_pl->m_player1 || m_currentOwner->p1.empty()) return;
        for (size_t i = m_lastEmitIdx1 + 1; i <= m_replayIdx1; ++i) {
            if (i < m_currentOwner->p1.size()) {
                const Frame& F = m_currentOwner->p1[std::min(m_currentOwner->p1.size()-1, i+2)];
                if (F.hold  != botPrevHold1) {
                    // best method but might not work due to Click Between Frames
                    m_pl->handleButton(F.hold, /*btn=*/1, /*isP1=*/true);

                    // if didn't work
                    if (F.hold != p1Hold) {
                        if (F.hold) m_pl->m_player1->pushButton(PlayerButton::Jump);
                        else m_pl->m_player1->releaseButton(PlayerButton::Jump);
                    }
                    
                    botPrevHold1  = F.hold;
                }
                if (F.holdL != botPrevHoldL1) {
                    m_pl->handleButton(F.holdL, /*btn=*/2, /*isP1=*/true);

                    if (F.holdL != p1LHold) {
                        if (F.holdL) m_pl->m_player1->pushButton(PlayerButton::Left);
                        else m_pl->m_player1->releaseButton(PlayerButton::Left);
                    }

                    botPrevHoldL1 = F.holdL;
                }
                if (F.holdR != botPrevHoldR1) {
                    m_pl->handleButton(F.holdR, /*btn=*/3, /*isP1=*/true);

                    if (F.holdR != p1RHold) {
                        if (F.holdR) m_pl->m_player1->pushButton(PlayerButton::Right);
                        else m_pl->m_player1->releaseButton(PlayerButton::Right);
                    }

                    botPrevHoldR1 = F.holdR;
                }
            }
        }
        m_lastEmitIdx1 = m_replayIdx1;
    }

void Ghosts::syncInputsToCurrentIndexP2_() {
        if (!m_pl->m_player2 || !m_currentOwner->hadDual || m_currentOwner->p2.empty()) return;
        for (size_t i = m_lastEmitIdx2 + 1; i <= m_replayIdx2; ++i) {
            if (i < m_currentOwner->p2.size()) {
                const Frame& F2 = m_currentOwner->p2[std::min(m_currentOwner->p2.size()-1, i+2)];
                if (F2.hold  != botPrevHold2) {
                    // best method but might not work due to Click Between Frames
                    m_pl->handleButton(F2.hold, /*btn=*/1, /*isP1=*/false);

                    // if didn't work
                    if (F2.hold != p2Hold) {
                        if (F2.hold)  m_pl->m_player2->pushButton(PlayerButton::Jump);
                        else m_pl->m_player2->releaseButton(PlayerButton::Jump);
                    }
                    botPrevHold2 = F2.hold;
                }
                if (F2.holdL != botPrevHoldL2) {
                    m_pl->handleButton(F2.holdL, /*btn=*/2, /*isP1=*/false);

                    if (F2.holdL != p2LHold) {
                        if (F2.holdL) m_pl->m_player2->pushButton(PlayerButton::Left);
                        else m_pl->m_player2->releaseButton(PlayerButton::Left);
                    }
                    
                    botPrevHoldL2 = F2.holdL;
                }
                if (F2.holdR != botPrevHoldR2) {
                    m_pl->handleButton(F2.holdR, /*btn=*/3, /*isP1=*/false);

                    if (F2.holdR != p2RHold) {
                        if (F2.holdR) m_pl->m_player2->pushButton(PlayerButton::Right);
                        else m_pl->m_player2->releaseButton(PlayerButton::Right);
                    }
                    
                    botPrevHoldR2 = F2.holdR;
                }
            }
        }
        m_lastEmitIdx2 = m_replayIdx2;
    }

bool Ghosts::startReplayPractice() {
        clearDeathMarkers_();
        const bool explicitSelection = !m_requestedPracticeSessionIds.empty();
        clearReplayStartPosOverride_();
        if (!explicitSelection) restartLevel();

        m_freezePlayerXAtEnd = false;
        m_chainAuthoritative = true;
        m_hasWavePointData = false;
        invalidateAttemptCounts();

        m_filterByStartPosition = true;

        if (!m_pl || !m_pl->m_player1) {
            log::warn("[Bot] startReplayPractice aborted: pl={}, p1={}",
                (bool)m_pl, m_pl && m_pl->m_player1);
            m_filterByStartPosition = false;
            stopReplay();
            return false;
        }

        if (hasCurrentPersistenceTarget_()) {
            clearAttemptCatalog_();

            if (scanAttemptCatalogForLevel_(m_levelIDOnAttach)) {
                m_checkpointMgr.restorePath(m_attemptCatalogPracticePath);
            }
        }

        if (explicitSelection) m_pl->m_currentCheckpoint = nullptr;
        if (isPracticeMode()) togglePracticeMode(false);

        int selectedSession = -1;

        m_checkpointMgr.clearReplayOverrideSegments();

        if (explicitSelection) {
            if (!buildSelectedPracticeReplayRoute_()) {
                log::warn("[Bot] startReplayPractice: selected practice runs could not make a replay route");
                m_filterByStartPosition = false;
                stopReplay();
                return false;
            }

            selectedSession = m_practiceReplayRouteStartSessionId;

            // Use startpos to spawn player in the right spot
            if (!installReplayStartPosForSelectedPracticeSession_()) {
                m_filterByStartPosition = false;
                stopReplay();
                return false;
            }
        }

        if (!explicitSelection) {
            restartLevel();
            m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
            m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;
            selectedSession = m_checkpointMgr.selectBestSessionForStartX_RankByTime(m_currentReplayStartPos.x, kReplayStartTolerance);
        }

        if (selectedSession <= 0 || m_checkpointMgr.noValidSessionForStartX()) {
            log::warn("[Bot] startReplayPractice: no valid session for startX={}", m_currentReplayStartPos.x);
            m_filterByStartPosition = false;
            stopReplay();
            return false;
        }

        m_replayKind = ReplayKind::PracticeComposite;
        
        if (explicitSelection) {
            auto const replaySegs = m_checkpointMgr.getReplaySequence();
            if (!replaySegs.empty()) {
                m_baseTime = replaySegs.front().absStart();
                m_currentSessionTime = m_baseTime;
                m_prevSessionTime = m_baseTime;
            }
        }
        else if (m_baseTime != 0.0) {
            m_checkpointMgr.offsetSelectedSessionBaseTimeIfZero(m_baseTime);
        }

        m_replayOwnerSerial = m_checkpointMgr.findOwnerSerialForTime(m_baseTime);
        m_activeOwnerSerial = m_replayOwnerSerial > 0 ? m_replayOwnerSerial : -1;

        if (m_activeOwnerSerial > 0) {
            Attempt* chosen = ensurePracticeReplayOwnerLoaded_(
                m_activeOwnerSerial,
                "active-owner"
            );

            if (!chosen) {
                log::warn(
                    "[Bot] startReplayPractice aborted: active owner serial={} could not be loaded",
                    m_activeOwnerSerial
                );

                m_filterByStartPosition = false;
                stopReplay();
                return false;
            }
        }

        rebuildRouteOwnerCache_();
        
        std::vector<PracticeSegment> replaySegs = m_checkpointMgr.getReplaySequence();
        if (replaySegs.empty()) {
            log::warn("[Bot] startReplayPractice: no replay segments");
            m_filterByStartPosition = false;
            stopReplay();
            return false;
        }

        std::unordered_set<int> routeOwners;
        routeOwners.reserve(replaySegs.size());

        for (auto const& seg : replaySegs) {
            if (seg.ownerSerial > 0) {
                routeOwners.insert(seg.ownerSerial);
            }
        }

        for (int serial : routeOwners) {
            if (!ensurePracticeReplayOwnerLoaded_(serial, "route-owner")) {
                log::warn(
                    "[Bot] startReplayPractice aborted: replay path owner serial={} is missing",
                    serial
                );

                m_filterByStartPosition = false;
                stopReplay();
                return false;
            }
        }
        
        //if (!firstOwner || firstOwner->p1.empty()) {
        //    log::warn("[Bot] startReplayPractice: no valid owner found");
        //    return;
        //}

        //if (firstOwner->seed != 0) {
        //    seedutils::applyAttemptSeed(firstOwner->seed, m_pl);
            //log::info("[Seed] Applied seed {} from first owner {} at replay start", firstOwner->seed, firstOwner->serial);
        //}

        tagCompositeOwnersHidden_(true);
        clearHeldInputs_();

        playback = true;
        m_playbackArmed = true;
        botActive = true;
        m_justStartedBot = true;
        recording = false;
        applyBotSafety_(true);

        m_didInitialWarp = false;
        m_replayIdx1 = m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        m_prevBotPx = m_currentReplayStartPos.x;

        forcePlayersVisible_();

        updateGhostVisibility();

        //if (m_pl && m_pl->m_player1) {
        //    m_pl->m_player1->setPositionX(m_currentReplayStartPos.x);
        //    if (m_pl->m_player2) m_pl->m_player2->setPositionX(m_currentReplayStartPos.x);
        //    m_didInitialWarp = true;
        //}

        m_replayEndTime = m_checkpointMgr.replayEndTime();
        
        if (m_replayEndTime > 0.1) {
            int tailSerial = m_checkpointMgr.findOwnerSerialForTime(m_replayEndTime);

            if (tailSerial > 0) {
                Attempt* tailOwner = ensurePracticeReplayOwnerLoaded_(
                    tailSerial,
                    "tail-owner"
                );

                if (!tailOwner) {
                    log::warn(
                        "[Bot] startReplayPractice: missing tail owner serial={} for endTime={:.3f}",
                        tailSerial,
                        m_replayEndTime
                    );

                    m_freezePlayerX = 0.f;
                    m_freezePlayerY = 0.f;
                    m_freezePlayerXP2 = 0.f;
                    m_freezePlayerYP2 = 0.f;
                    m_freezePlayerRotationP2 = 0.f;
                }
                else {
                    // Find the frame closest to endTime
                    size_t endIdx = idxForTime(tailOwner->acc1Time, tailOwner->p1, m_replayEndTime);
                    if (endIdx < tailOwner->p1.size()) {
                        m_freezePlayerX = tailOwner->p1[endIdx].x;
                        m_freezePlayerY = tailOwner->p1[endIdx].y;
                        m_freezePlayerRotation = tailOwner->p1[endIdx].rot;
                    } else {
                        m_freezePlayerX = tailOwner->p1.back().x;
                        m_freezePlayerY = tailOwner->p1.back().y;
                        m_freezePlayerRotation = tailOwner->p1.back().rot;
                    }
                    
                    if (!tailOwner->p2.empty()) {
                        size_t endIdx2 = idxForTime(tailOwner->acc2Time, tailOwner->p2, m_replayEndTime);
                        if (endIdx2 < tailOwner->p2.size()) {
                            m_freezePlayerXP2 = tailOwner->p2[endIdx2].x;
                            m_freezePlayerYP2 = tailOwner->p2[endIdx2].y;
                            m_freezePlayerRotationP2 = tailOwner->p2[endIdx2].rot;
                        } else {
                            m_freezePlayerXP2 = tailOwner->p2.back().x;
                            m_freezePlayerYP2 = tailOwner->p2.back().y;
                            m_freezePlayerRotationP2 = tailOwner->p2.back().rot;
                        }
                    } else {
                        m_freezePlayerXP2 = 0.f;
                        m_freezePlayerYP2 = 0.f;
                        m_freezePlayerRotationP2 = 0.f;
                    }
                    
                    // log::info("[Bot] Set freeze position for endTime={:.3f}: ({:.1f}, {:.1f})", 
                    //     endTime, m_freezePlayerX, m_freezePlayerY);
                }
            }
        } else {
            log::warn("[Bot] No valid end time data (endTime={:.3f}), freeze disabled", m_replayEndTime);
            m_freezePlayerX = 0.f;
            m_freezePlayerY = 0.f;
            m_freezePlayerXP2 = 0.f;
            m_freezePlayerYP2 = 0.f;
            m_freezePlayerRotationP2 = 0.f;
        }

        if (explicitSelection) {
            m_pl->m_currentCheckpoint = nullptr;
            m_pendingPracticeStartPosInit = true;
            m_pendingPracticeStartPosFrames = 0;
        }
        
        restartLevel();
        if (explicitSelection) m_pl->startMusic();
        return true;
    }

bool Ghosts::startReplayBest() {
        clearDeathMarkers_();
        clearReplayStartPosOverride_();
        clearPracticeReplaySessionSelection();
        restartLevel();
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;
        m_hasWavePointData = false;

        if (!m_pl || !m_pl->m_player1) {
            log::warn("[Bot] startReplayBest aborted: pl={}, p1={}",
                    (bool)m_pl, m_pl && m_pl->m_player1);
            stopReplay();
            return false;
        }

        if (isPracticeMode()) togglePracticeMode(false);

        //log::info("[Bot] startReplayBest: {} total attempts", attempts.size());

        m_replayKind = ReplayKind::BestSingle;

        botActive = false;
        playback = false;
        
        restartLevel();

        m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
        m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;

        m_filterByStartPosition = true;

        
        if (!scanAttemptCatalogForLevel_(m_levelIDOnAttach)) {
            log::warn("[Bot] startReplayBest: failed to scan attempt catalog");
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        if (!scanAttemptCatalogForLevel_(m_levelIDOnAttach)) {
            log::warn("[Bot] startReplayBest: failed to scan attempt catalog");
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        tagCompositeOwnersHidden_(true);

        clearHeldInputs_();
        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;

        playback = true;
        m_playbackArmed = true;
        botActive = true;
        recording = false;
        applyBotSafety_(true);
        m_didInitialWarp = false;
        m_replayIdx1 = m_replayIdx2 = 0;
        m_prevBotPx = 0.f;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        forcePlayersVisible_();

        updateGhostVisibility();
        m_justStartedBot = true;

        //log::info("[Bot] startReplayBest: calling final resetLevel()");
        m_replayOwnerSerial = -1;
        //log::info("[Bot] startReplayBest: complete, botActive={}", botActive);
        return true;
    }

int Ghosts::chooseReplayOwnerFromFinishedPreload_() {
        int bestSerial = -1;
        float bestX = -FLT_MAX;
        double bestT = -1.0;

        for (uint32_t rawSerial : m_preloadOrder) {
            //log::info("rawSerial: {}", rawSerial);
            const int serial = static_cast<int>(rawSerial);

            const size_t idx = findLoadedAttemptIndexBySerialAndPractice_(serial, false);

            if (idx == SIZE_MAX || idx >= attempts.size()) {
                log::info("bad index");
                continue;
            }

            const Attempt& a = attempts[idx];

            if (!a.preloaded) { 
                log::info("not preloaded");
                continue;
            }
            if (a.practiceAttempt) {
                log::info("practice attempt");
                continue;
            }
            if (a.p1.empty()) { 
                log::info("attempt frames empty");
                continue;
            }

            const float endX = static_cast<float>(a.p1.back().x);
            const double endT = static_cast<double>(a.p1.back().t);

            if (
                endX > bestX ||
                (endX == bestX && endT > bestT) ||
                (endX == bestX && endT == bestT && serial > bestSerial)
            ) {
                bestX = endX;
                bestT = endT;
                bestSerial = serial;
            }
        }

        //log::info("bestSerial: {}", bestSerial);

        return bestSerial;
    }

bool Ghosts::replayBestSetOwner() {
        const int serial = chooseReplayOwnerFromFinishedPreload_();

        if (serial <= 0) {
            log::warn("[Bot] startReplayBest: no non-practice attempts to replay for current start pos");
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        Attempt* chosen = ensureAttemptLoadedBySerialAndPractice_(serial, false, false);
        if (!chosen || chosen->p1.empty()) {
            log::warn("[Bot] startReplayBest: failed to load chosen attempt serial={}", serial);
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        if (m_baseTime != 0.f && chosen->baseTimeOffset == 0.f) {
            chosen->baseTimeOffset = m_baseTime;
            const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
            offsetAttemptTimesFast(*chosen, offsetQ);
        }

        const size_t idx = findLoadedAttemptIndexBySerialAndPractice_(serial, false);
        if (idx == SIZE_MAX) {
            log::warn("[Bot] startReplayBest: loaded serial {} but index lookup failed", serial);
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        m_replayOwnerSerial = serial;
        m_replayOwnerIndex = idx;

        buildCompositeFromAttempt(idx);

        if (m_compOwnerIdx >= attempts.size() || attempts[m_compOwnerIdx].p1.empty()) {
            log::warn("[Bot] startReplayBest: no data after filtering. m_compOwnerIdx={}", m_compOwnerIdx);
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        restartLevel();
        return true;
    }

void Ghosts::stopReplay() {
        clearDeathMarkers_();
        if (!botActive) {
            clearReplayStartPosOverride_();
            m_checkpointMgr.clearReplayOverrideSegments();
            m_practiceReplayRouteStartSessionId = 0;
            clearPracticeReplaySessionSelection();
            clearHeldInputs_();
            return;
        }
        //log::info("[Bot] stopReplay()");
        botActive = false;
        m_filterByStartPosition = false;
        applyBotSafety_(false);
        tagCompositeOwnersHidden_(false);
        m_activeOwnerSerial = -1;
        clearHeldInputs_();
        m_didInitialWarp = false;
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;
        m_playerPrevTeleportedP1 = false;
        m_playerPrevTeleportedP2 = false;
        invalidatePrimedPlayerObjectRefs();
        m_initialAttemptsToSet.clear();
        m_preloadOrder.clear();
        m_1PreloadedSoDontShowGhosts = false;
        forceHidePlayLayerGhostTextLabelOnly();
        clearDeathMarkers_();
        resetPlayerMirrorVisualOffsets_();
        resetMirrorVisualState_();
        clearReplayStartPosOverride_();
        m_checkpointMgr.clearReplayOverrideSegments();
        m_practiceReplayRouteStartSessionId = 0;
        clearPracticeReplaySessionSelection();
    }

void Ghosts::restartLevel() { 
        if (m_pl) {
            block_attempt_push_on_recording_start = true;
            m_blockAttemptCount = true;
            m_pl->resetLevel(); 
            m_blockAttemptCount = false;
            block_attempt_push_on_recording_start = false;
        }
    }

double Ghosts::getStartTime() {
        if (isPracticeMode() && m_pl->getLastCheckpoint()) {
            return m_pl->getLastCheckpoint()->m_gameState.m_levelTime;
        }
        else {
            if (m_gl) return m_gl->m_gameState.m_levelTime;
        }
        return 0.f;
    }

void Ghosts::maybeInitialWarpToReplayStart_() {
        if (!m_pl || !m_pl->m_player1) return;
        if (m_replayKind != ReplayKind::PracticeComposite) return;
        if (m_didInitialWarp) return;

        const double warpTime = m_currentSessionTime;
        const Attempt* owner = getReplayOwner_(warpTime);
        if (!owner || owner->p1.empty()) return;

        seekReplayCursorsToTimeExact_(*owner, warpTime);

        const float px = m_pl->m_player1->getPositionX();
        const Frame& f = owner->p1[m_replayIdx1];

        if (px + 5.f >= f.x) return;

        forceMode(m_pl->m_player1, IconType::Cube);
        if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) 
            forceMode(m_pl->m_player2, IconType::Cube);

        m_pl->m_player1->setPosition({ f.x, f.y });
        m_pl->m_player1->setRotation(f.rot);

        if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) {
            const Frame& f2 = owner->p2[m_replayIdx2];
            m_pl->m_player2->setPosition({ f2.x, f2.y });
            m_pl->m_player2->setRotation(f2.rot);
            forceMode(m_pl->m_player2, f2.mode);
            syncPlayerSize(m_pl->m_player2, f2.vehicleSize);
            syncGhostGravity(m_pl->m_player2, f2.upsideDown, m_pl);
        }

        forceMode(m_pl->m_player1, f.mode);
        syncPlayerSize(m_pl->m_player1, f.vehicleSize);
        syncGhostGravity(m_pl->m_player1, f.upsideDown, m_pl);

        m_didInitialWarp = true;
    }

bool Ghosts::refreshCurrentOwnerPointer_() {
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                if (const Attempt* bySerial = findLoadedAttemptBySerialAndPractice_(m_replayOwnerSerial, false)) {
                    m_currentOwner = bySerial;
                    return true;
                }

                m_currentOwner = ensureAttemptLoadedBySerialAndPractice_(m_replayOwnerSerial, false, false);
                return m_currentOwner != nullptr;
            }

            if (m_compOwnerIdx < attempts.size()) {
                m_currentOwner = &attempts[m_compOwnerIdx];
                if (m_currentOwner && m_currentOwner->serial > 0) {
                    m_replayOwnerSerial = m_currentOwner->serial;
                }
                return true;
            }

            m_currentOwner = nullptr;
            return false;
        }

        if (m_replayOwnerSerial > 0) {
            if (const Attempt* bySerial = findLoadedAttemptBySerialAndPractice_(m_replayOwnerSerial, true)) {
                m_currentOwner = bySerial;
                return true;
            }

            m_currentOwner = ensurePracticeReplayOwnerLoaded_(m_replayOwnerSerial, "refresh-owner");
            return m_currentOwner != nullptr;
        }

        m_currentOwner = nullptr;
        return false;
    }

void Ghosts::updateClickState(bool isPlayer1) {
        if (!botActive) return;
        if (!m_pl || m_is_quitting) return;
        refreshCurrentOwnerPointer_();
        if (!m_allowSetPlayerClickState) return;
        if (!m_currentOwner || m_currentOwner->p1.empty()) return;

        if (m_justStartedBot) {
            m_justStartedBot = false;
        }

        if (isPlayer1) syncInputsToCurrentIndexP1_();
        else if (shouldReplayP2Inputs_()) syncInputsToCurrentIndexP2_();
        else m_lastEmitIdx2 = m_replayIdx2;

        m_allowSetPlayerClickState = false;

    }

void Ghosts::setFirstNonzeroPosP2() {
        if (!m_setRealPlayerPosition) return;
        if (!botActive || !m_p2JustSpawned || !m_currentOwner || !m_pl || !m_pl->m_player2)
            return;

        const auto& p2 = m_currentOwner->p2;
        if (p2.size() <= 1)
            return;

        for (size_t i = 1; i < p2.size(); ++i) {
            const float x = p2[i].x;
            const float y = p2[i].y;

            if (x != 0.f) {
                m_pl->m_player2->setPosition({x, y});
                // log::info("setpos: {} {}", x, y);
                return;
            }
        }
    }

void Ghosts::preUpdateP2() {
        if (m_is_quitting || !m_pl || !m_pl->m_player2) return;

        // during replay don't start p2 early (start time)
        if (botActive && m_currentOwner && !m_currentOwner->p2.empty()) {
            const uint32_t nowTQ = runtimeQuantTimeQ_(m_currentSessionTime);

            const uint32_t firstP2TQ = m_currentOwner->p2.front().t.q;

            if (nowTQ < firstP2TQ) {
                m_prevHadP2 = false;
                m_p2JustSpawned = false;
                return;
            }
        }

        m_prevpx2 = m_px2;

        const float p2pos = m_pl->m_player2->getPositionX();

        m_px2 = (p2pos != 0.f)
            ? p2pos
            : m_px;

        if (!m_prevHadP2) m_p2JustSpawned = true;

        setFirstNonzeroPosP2();

        m_prevHadP2 = true;
        m_noP2Check = false;
    }

void Ghosts::hasReachedEndOfLevel() {
        if (m_playingEndAnimation) return;

        if (!m_currentDidNoclip) {
            m_currentDidNoclip = setFalseIfPlayerWasDestroyedCheck || isNoclipDetected;
        }

        m_playingEndAnimation = true;
        resetNoclipDetectedFlag();
    }

void Ghosts::preUpdate() {
        //log::info("Preupdate");
        // log::info("didAttemptUseNoclip: {}", didAttemptUseNoclip());
        // if (!m_allowWorkThisTick || m_is_quitting || !m_pl) return;
        if (m_is_quitting || !m_pl || !m_pl->m_player1) return;

        if (m_pendingPracticeStartPosInit) {
            m_px = m_pl->m_player1->getPositionX();
            m_currentSessionTime = m_baseTime;
            return;
        }

        // log::info("m_freezePlayerXAtEnd: {}, {}", m_freezePlayerXAtEnd, m_pl->m_hasCompletedLevel);

        m_px = m_pl->m_player1->getPositionX();
        m_currentSessionTime = m_baseTime + m_pl->m_attemptTime;
        updateCurrentAttemptNoclipState();

        // Check when not dual
        if (m_noP2Check) {
            m_prevHadP2 = false;
            m_p2JustSpawned = false;
        }
        m_noP2Check = true;

        // log::info("m_prevHadP2: {}", m_prevHadP2);
        // log::info("m_levelFlipping: {}", m_gl->m_gameState.m_levelFlipping);

        if (!botActive) return;
        if (!m_prevHadP2) m_px2 = m_px;

        // log::info("x: {}, y: {}", m_pl->m_player1->getPositionX(), m_pl->m_player1->getPositionY());
        
        bool timeWentBack = (m_currentSessionTime < m_prevSessionTime - 0.05);
        m_IHateMirrorPortalsSoRewind = timeWentBack;

        int previousOwnerSerial = m_activeOwnerSerial;
        
        adjustReplayCursorToTime_(m_currentSessionTime);
        maybeInitialWarpToReplayStart_();

        if (!m_currentOwner || m_currentOwner->p1.empty()) {
            return;
        }
        
        if (m_replayIdx1 >= m_currentOwner->p1.size()) {
            m_replayIdx1 = m_currentOwner->p1.size() - 1;
        }
        if (m_currentOwner->hadDual && !m_currentOwner->p2.empty() && 
            m_replayIdx2 >= m_currentOwner->p2.size()) {
            m_replayIdx2 = m_currentOwner->p2.size() - 1;
        }

        int newOwnerSerial = m_currentOwner->serial;

        // log::info("newOwnerSerial: {}, p1: {}, p2: {}", newOwnerSerial, m_currentOwner->p1.size(), m_currentOwner->p2.size());
        
        if (newOwnerSerial > 0 && previousOwnerSerial != newOwnerSerial && 
            m_replayKind == ReplayKind::PracticeComposite) {
            
            if (previousOwnerSerial > 0) {
                m_currentlyHiddenSerials.erase(previousOwnerSerial);
                if (Attempt* oldOwner = findAttemptBySerialAndPractice_(previousOwnerSerial, true)) {
                    clearGhostOffset_(*oldOwner);

                    oldOwner->primedP1 = false;
                    oldOwner->primedP2 = false;
                    oldOwner->eolFrozenP1 = false;
                    oldOwner->eolFrozenP2 = false;
                    oldOwner->setP1Visible(false, true);
                    oldOwner->setP2Visible(false, true);
                }
            }
            m_currentlyHiddenSerials.insert(newOwnerSerial);
            
            //log::info("[OWNER CHANGE] {} -> {} at time {:.3f}", 
            //        previousOwnerSerial, newOwnerSerial, m_currentSessionTime);

            //if (m_currentOwner->seed != 0) {
            //    seedutils::applyAttemptSeed(m_currentOwner->seed, m_pl);
            //    log::info("[Seed] Applied seed {} from new owner {} on owner change", 
            //            m_currentOwner->seed, newOwnerSerial);
            //}
        }
        
        m_activeOwnerSerial = newOwnerSerial;
        
        //log::info("[TIME TAKEN] preupdate {:.8f}", getTimeMs() - tStart);
    }

void Ghosts::checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah() {
        if (m_replayKind == ReplayKind::PracticeComposite) finalTime = m_replayEndTime;
        else finalTime = m_currentOwner->p1.back().t;

        if (!m_freezePlayerXAtEnd && m_currentSessionTime >= finalTime) {
            if (m_currentOwner->completed) return;
            m_freezePlayerXAtEnd = true;
            m_pl->applyTimeWarp(0.000001f);
            m_pl->updateTimeWarp(0.000001f);
            m_pl->pauseAudio();
            
            if (m_replayKind == ReplayKind::BestSingle) {
                if (m_currentOwner && !m_currentOwner->p1.empty()) {
                    m_freezePlayerX = m_currentOwner->p1.back().x;
                    m_freezePlayerY = m_currentOwner->p1.back().y;
                    m_freezePlayerRotation = m_currentOwner->p1.back().rot;
                    if (!m_currentOwner->p2.empty()) {
                        m_freezePlayerXP2 = m_currentOwner->p2.back().x;
                        m_freezePlayerYP2 = m_currentOwner->p2.back().y;
                        m_freezePlayerRotationP2 = m_currentOwner->p2.back().rot;
                    }
                }
            }
            m_pl->m_player1->stopDashing();
            if (m_ghostsExplode) m_pl->m_player1->playDeathEffect();

            if (m_pl->m_player2 && m_pl->m_player2->isVisible()) {
                m_pl->m_player2->stopDashing();
                if (m_ghostsExplode) m_pl->m_player2->playDeathEffect();
            }

            if (m_ghostsExplodeSFX && m_fmodEngine) {
                if (m_fmodEngine->m_sfxVolume != 0) {
                    static std::mt19937 rng(
                        (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()
                    );

                     std::string sfxPath;

                    if (m_useCustomExplodeSounds) {
                        refreshAndPreloadCustomExplodeSfx_(false);

                        if (!customSfx.empty()) {
                            std::uniform_int_distribution<size_t> dist(0, customSfx.size() - 1);
                            sfxPath = customSfx[dist(rng)];
                        }
                    }

                    // Fallback when custom disabled or folder empty
                    if (sfxPath.empty()) {
                        sfxPath = "explode_11.ogg"_spr;
                    }

                    g_explodeLimiter.play(sfxPath, /*volume*/ 1.f, /*pitch*/ 1.f);
                }
            }
            countGhostTextDeathOnce_(*m_currentOwner);
        }

        if (m_freezePlayerXAtEnd && !m_playingEndAnimation) {
            m_pl->m_player1->setPosition({m_freezePlayerX, m_freezePlayerY});
            m_pl->m_player1->setRotation(m_freezePlayerRotation);
            if (m_pl->m_player2 && m_pl->m_player2->isVisible()) {
                m_pl->m_player2->setPosition({m_freezePlayerXP2, m_freezePlayerYP2});
                m_pl->m_player2->setRotation(m_freezePlayerRotationP2);
            }
                
        }
    }

void Ghosts::updateFreezePlayerX_() {
        if (!botActive || !m_pl || !m_pl->m_player1)
            return;

        if (m_replayKind != ReplayKind::BestSingle &&
            m_replayKind != ReplayKind::PracticeComposite)
            return;

        if (m_currentOwner->completed) return;

        double lastTime = 0.0;
        float lastX = 0.f;
        float lastY = 0.f;
        float lastXP2 = 0.f;
        float lastYP2 = 0.f;
        
        if (m_replayKind == ReplayKind::PracticeComposite) {
            lastTime = m_replayEndTime;
            lastX = m_freezePlayerX;
            lastY = m_freezePlayerY;
            lastXP2 = m_freezePlayerXP2;
            lastYP2 = m_freezePlayerYP2;
        } else {
            if (m_currentOwner && !m_currentOwner->p1.empty()) {
                lastTime = m_currentOwner->p1.back().t;
                lastX = m_currentOwner->p1.back().x;
                lastY = m_currentOwner->p1.back().y;
                if (!m_currentOwner->p2.empty()) {
                    lastXP2 = m_currentOwner->p2.back().x;
                    lastYP2 = m_currentOwner->p2.back().y;
                }
            }
        }
        

        if (lastTime <= 0.1) {
            return;
        }
        
        if (!m_freezePlayerXAtEnd && m_currentSessionTime >= lastTime - 0.05) {
            //log::info("[Bot] updateFreezePlayerX_: FREEZING at time={:.3f}, lastTime={:.3f}", 
            //        m_currentSessionTime, lastTime);
            m_freezePlayerXAtEnd = true;
            m_freezePlayerX = lastX;
            m_freezePlayerY = lastY;
            m_freezePlayerXP2 = lastXP2;
            m_freezePlayerYP2 = lastYP2;

            m_pl->m_player1->setPosition({m_freezePlayerX, m_freezePlayerY});
            if (m_pl->m_player2 && m_pl->m_player2->isVisible())
                m_pl->m_player2->setPosition({m_freezePlayerXP2, m_freezePlayerYP2});
            m_pl->applyTimeWarp(0.000001f);
            m_pl->updateTimeWarp(0.000001f);
            m_pl->pauseAudio();
        }
        if (m_freezePlayerXAtEnd) {
            m_pl->m_player1->setPosition({m_freezePlayerX, m_freezePlayerY});
            if (m_pl->m_player2 && m_pl->m_player2->isVisible())
                m_pl->m_player2->setPosition({m_freezePlayerXP2, m_freezePlayerYP2});
        }
    }

void Ghosts::applySegmentBasedReplay_(bool isPlayer1) {
        if (!botActive) return;
        if (!m_pl || m_is_quitting) return;
        if (m_pendingPracticeStartPosInit) return;

        //const float px = playerX_();
        //const float px2 = playerX2_();

        refreshCurrentOwnerPointer_();
        if (!m_currentOwner || m_currentOwner->p1.empty()) return;

        if (m_justStartedBot) {
            m_justStartedBot = false;
        }

        applyFrameAndClickToPlayerOverRange(isPlayer1);

        checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah();
        m_allowSetPlayerClickState = false;
    }

void Ghosts::recordUpdate(bool isPlayer1, bool isPostWavePointUpdate) {
        // if (!shouldRunWorkThisTick_()) return;
        if (!m_pl || m_is_quitting) return;
        const bool isPractice = m_pl->m_isPracticeMode;
        if (isPractice && !recordInPractice) return;
        if (!recording || recordingBlocked) return;

        if (isPostWavePointUpdate) insertWaveStateIntoLastFrame(isPlayer1);
        else recordCurrentFrame_(isPlayer1);
    }

void Ghosts::rebuildRouteOwnerCache_() {
        m_routeOwnerSerials.clear();
        m_currentlyHiddenSerials.clear();

        if (!practiceRouteOwnerHidingEnabled_())
            return;

        const auto replaySeq = m_checkpointMgr.getReplaySequence();

        if (replaySeq.empty()) {
            return;
        }

        m_routeOwnerSerials.reserve(replaySeq.size() * 2 + 8);
        m_currentlyHiddenSerials.reserve(replaySeq.size() * 2 + 8);

        for (auto const& seg : replaySeq) {
            if (seg.ownerSerial > 0) {
                m_routeOwnerSerials.insert(seg.ownerSerial);
            }
        }

        if (m_activeOwnerSerial > 0) {
            m_routeOwnerSerials.insert(m_activeOwnerSerial);
        }

        for (int serial : m_routeOwnerSerials) {
            m_currentlyHiddenSerials.insert(serial);
        }
    }

void Ghosts::seekReplayCursorsToTimeExact_(const Attempt& owner, double sessionTime) {
        m_replayIdx1 = owner.p1.empty()
            ? 0
            : idxForTimeBounded_(owner.p1, owner.acc1Time, sessionTime);

        if (owner.hadDual && !owner.p2.empty()) {
            m_replayIdx2 = idxForTimeBounded_(owner.p2,owner.acc2Time,sessionTime);
        } else m_replayIdx2 = 0;
    }

void Ghosts::advanceReplayCursorsLockstep_(double sessionTime, bool wentBack) {
        if (!m_currentOwner) return;

        auto& p1 = m_currentOwner->p1;
        auto& p2 = m_currentOwner->p2;

        if (p1.empty()) return;

        const size_t oldP1 = m_replayIdx1;
        const size_t oldP2 = m_replayIdx2;

        if (!wentBack) {
            advanceFromPrevIdx_(p1, m_replayIdx1, sessionTime, false);
        } else {
            m_replayIdx1 = idxForTimeBounded_(
                p1,
                m_currentOwner->acc1Time,
                sessionTime
            );
        }

        if (m_replayIdx1 >= p1.size()) {
            m_replayIdx1 = p1.size() - 1;
        }

        size_t p1Delta = 0;
        if (m_replayIdx1 >= oldP1) {
            p1Delta = m_replayIdx1 - oldP1;
        }

        if (m_currentOwner->hadDual && !p2.empty()) {
            m_replayIdx2 = idxForTimeBounded_(
                p2,
                m_currentOwner->acc2Time,
                sessionTime
            );

            if (m_replayIdx2 >= p2.size()) {
                m_replayIdx2 = p2.size() - 1;
            }
        }
        else {
            m_replayIdx2 = 0;
        }
        /*
        log::info(
            "[lockstep] owner={} t={} oldP1={} p1={} d={} oldP2={} p2={}",
            m_currentOwner->serial,
            sessionTime,
            oldP1,
            m_replayIdx1,
            p1Delta,
            oldP2,
            m_replayIdx2
        );*/
    }

void Ghosts::adjustReplayCursorToTime_(double sessionTime) {
        m_currentOwner = getReplayOwner_(sessionTime);
        if (!m_currentOwner) return;

        const bool wentBack = (sessionTime < m_prevSessionTime - 0.05);

        advanceReplayCursorsLockstep_(sessionTime, wentBack);

        m_prevBotPx = playerX_();
    }

void Ghosts::applyFrameAndClickToPlayerOverRange(bool isP1) {
        if (m_playingEndAnimation) return;

        // log::info("applyFrameAndClickToPlayerOverRange");

        const size_t replayIdx = isP1 ? m_replayIdx1 : m_replayIdx2;

        const std::vector<Frame>& v = isP1 ? m_currentOwner->p1 : m_currentOwner->p2;
        PlayerObject* p = isP1 ? m_pl->m_player1 : m_pl->m_player2;
        if (!p || v.empty()) return;

        size_t& lastPoseIdx =
            isP1 ? m_lastPoseIdx1 : m_lastPoseIdx2;

        size_t& lastEmitIdx =
            isP1 ? m_lastEmitIdx1 : m_lastEmitIdx2;

        const size_t startIdx =
            std::min(replayIdx, v.size() - 1);

        const size_t endIdx =
            startIdx;

        bool& botPrevHold = isP1 ? botPrevHold1 : botPrevHold2;
        bool& botPrevHoldL = isP1 ? botPrevHoldL1 : botPrevHoldL2;
        bool& botPrevHoldR = isP1 ? botPrevHoldR1 : botPrevHoldR2;

        const bool setClicks =
            (isP1 || shouldReplayP2Inputs_()) &&
            m_allowSetPlayerClickState;


        if (!isP1) {
            const uint32_t nowTQ = runtimeQuantTimeQ_(m_currentSessionTime);
            const uint32_t firstP2TQ = v.front().t.q;

            // P2 does not exist in the replay yet
            if (nowTQ < firstP2TQ) {
                return;
            }
        }

        for (size_t poseI = startIdx; poseI <= endIdx; ++poseI) {
            const Frame& C = v[poseI];

            if (m_allowSetPlayerPos && m_setRealPlayerPosition) {
                // position stuff
                const Frame& a = C;

                const bool haveNext = (poseI + 1 < v.size());
                const Frame* b = haveNext ? &v[poseI + 1] : nullptr;

                const bool havePrevious = (poseI > 0);
                const Frame* prev = havePrevious ? &v[poseI - 1] : nullptr;

                const bool isWave = (a.mode == IconType::Wave);

                const bool isMini =
                    static_cast<float>(a.vehicleSize) < 0.9f;

                const float ix = a.x;
                const float iy = a.y;

                float prevY;
                float prevX;

                if (prev) {
                    prevY = prev->y;
                    prevX = prev->x;
                }
                else {
                    prevY = p->getPositionY();
                    prevX = p->getPositionX();
                }

                float fliptime = mirrorFlipT_();

                bool blockWaveAddPoint = false;

                if (
                    isWave &&
                    p->m_waveTrail &&
                    fliptime != 1 &&
                    fliptime != 0
                ) {
                    p->m_waveTrail->reset();
                    blockWaveAddPoint = true;
                }

                /*
                log::info(
                    "[replay P{}] owner={} ia={} i={} last={} replay={} size={} t={} y={} exp={}",
                    isP1 ? 1 : 2,
                    m_currentOwner ? m_currentOwner->serial : -1,
                    ia,
                    poseI,
                    lastEmitIdx,
                    replayIdx,
                    v.size(),
                    static_cast<double>(F.t),
                    static_cast<float>(F.y),
                    static_cast<int>(F.wavePointThisFrame)
                );
                */

                if (p->m_isDashing != a.isDashing) {
                    if (a.isDashing) {
                        p->startDashing(attemptplayback::p0d());
                    }
                    else {
                        p->stopDashing();
                    }
                }

                if (p->m_isUpsideDown != a.upsideDown) {
                    p->flipGravity(a.upsideDown, true);
                }

                if (currentMode(p, m_pl->m_isPlatformer) != a.mode) {
                    forceMode(
                        p,
                        a.mode,
                        /*isRealPlayer=*/true
                    );
                }
                syncPlayerSize(p, a.vehicleSize);

                bool addedWavePoint = false;
                bool addedExplicitWavePoint = false;

                const bool explicitPoint = a.wavePointThisFrame;

                const bool canAddWavePoint =
                    isWave &&
                    p->m_waveTrail &&
                    !blockWaveAddPoint;

                MovementDirection& storedMovementDir =
                    isP1
                        ? m_movementDirectionP1
                        : m_movementDirectionP2;

                bool& explicitWavePointAddedPrevPose =
                    isP1
                        ? m_explicitWavePointAddedPrevPoseP1
                        : m_explicitWavePointAddedPrevPoseP2;

                bool& suppressNextExplicitWavePoint =
                    isP1
                        ? m_suppressNextExplicitWavePointP1
                        : m_suppressNextExplicitWavePointP2;

                const bool suppressExplicitThisPose =
                    suppressNextExplicitWavePoint;

                suppressNextExplicitWavePoint = false;

                if (!isWave) {
                    storedMovementDir = MovementDirection::Flat;
                    explicitWavePointAddedPrevPose = false;
                }

                bool directionChanged = false;

                const bool prevWasWave =
                    prev &&
                    prev->mode == IconType::Wave;

                if (
                    isWave &&
                    prevWasWave &&
                    prevX != ix
                ) {
                    MovementDirection movementDir =
                        MovementDirection::Flat;

                    setMovementDirection(
                        prevY,
                        iy,
                        movementDir
                    );

                    if (
                        movementDir !=
                        MovementDirection::Flat
                    ) {
                        directionChanged =
                            storedMovementDir !=
                                MovementDirection::Flat &&
                            movementDir != storedMovementDir;

                        storedMovementDir = movementDir;
                    }
                }
                else if (
                    isWave &&
                    !prevWasWave
                ) {
                    storedMovementDir =
                        MovementDirection::Flat;
                }

                auto tryAddWavePoint =
                    [&](cocos2d::CCPoint raw) -> bool {
                        bool& snapHave =
                            isP1
                                ? m_haveLastSnappedWavePointP1
                                : m_haveLastSnappedWavePointP2;

                        cocos2d::CCPoint& snapLast =
                            isP1
                                ? m_lastSnappedWavePointP1
                                : m_lastSnappedWavePointP2;

                        bool& snapLastMini =
                            isP1
                                ? m_lastSnappedWaveMiniP1
                                : m_lastSnappedWaveMiniP2;

                        bool& snapLastDir =
                            isP1
                                ? m_lastSnappedWaveDirP1
                                : m_lastSnappedWaveDirP2;

                        const bool oldSnapHave =
                            snapHave;

                        const cocos2d::CCPoint oldSnapLast =
                            snapLast;

                        const bool oldSnapLastMini =
                            snapLastMini;

                        const bool oldSnapLastDir =
                            snapLastDir;

                        auto snapped =
                            snapWavePointSimple_(
                                raw,
                                isMini,
                                isP1
                            );

                        if (!snapped) {
                            return false;
                        }

                        if (
                            waveTrailAddPointToPlayer(
                                p->m_waveTrail,
                                *snapped,
                                isP1,
                                m_p2JustSpawned
                            )
                        ) {
                            markWavePoint(
                                *snapped,
                                isP1
                            );

                            return true;
                        }

                        snapHave = oldSnapHave;
                        snapLast = oldSnapLast;
                        snapLastMini = oldSnapLastMini;
                        snapLastDir = oldSnapLastDir;

                        return false;
                    };

                if (
                    canAddWavePoint &&
                    explicitPoint &&
                    !suppressExplicitThisPose
                ) {
                    m_hasWavePointData = true;

                    addedExplicitWavePoint =
                        tryAddWavePoint({
                            ix,
                            iy
                        });

                    addedWavePoint =
                        addedExplicitWavePoint;
                }

                if (
                    canAddWavePoint &&
                    directionChanged &&
                    !addedWavePoint &&
                    !explicitWavePointAddedPrevPose
                ) {
                    m_hasWavePointData = true;

                    if (
                        tryAddWavePoint({
                            prevX,
                            prevY
                        })
                    ) {
                        addedWavePoint = true;
                        suppressNextExplicitWavePoint = true;
                    }
                }

                explicitWavePointAddedPrevPose =
                    addedExplicitWavePoint;

                if (canAddWavePoint) {
                    p->m_waveTrail->setVisible(true);
                    p->m_waveTrail->setOpacity(255);
                    p->m_waveTrail->resumeStroke();
                }

                // Teleport goop
                bool& playerPrevTeleported =
                    isP1
                        ? m_playerPrevTeleportedP1
                        : m_playerPrevTeleportedP2;

                if (playerPrevTeleported) {
                    if (canAddWavePoint) {
                        p->m_waveTrail->reset();
                        p->m_waveTrail->resumeStroke();

                        if (!addedWavePoint) {
                            addedWavePoint =
                                waveTrailAddPointToPlayer(
                                    p->m_waveTrail,
                                    {
                                        ix,
                                        iy
                                    },
                                    isP1,
                                    !isP1 &&
                                        m_p2JustSpawned
                                    // ix < prevX
                                );
                        }
                    }

                    playerPrevTeleported = false;
                }

                bool teleported =
                    b &&
                    std::fabs(
                        static_cast<float>(b->y) -
                        iy
                    ) >
                        kWaveTeleportedTolerance;

                if (teleported) {
                    if (
                        isWave &&
                        p->m_waveTrail
                    ) {
                        p->m_waveTrail->stopStroke();
                    }

                    playerPrevTeleported = true;
                }

                p->setVisible(a.isVisible);

                if (a.isVisible) {
                    p->setOpacity(255);
                }

                const cocos2d::CCPoint replayPosition = {
                    a.x,
                    a.y
                };

                /*
                log::info(
                    "setpos P{} owner={} t={} idx={} pos=({}, {})",
                    isP1 ? 1 : 2,
                    m_currentOwner ? m_currentOwner->serial : -1,
                    m_currentSessionTime,
                    replayIdx,
                    replayPosition.x,
                    replayPosition.y
                );*/
                

                p->setPosition(replayPosition);
                p->m_position = replayPosition;

                if (prev) {
                    p->m_lastPosition.x = prev->x;
                    p->m_lastPosition.y = prev->y;
                }
                else {
                    p->m_lastPosition = replayPosition;
                }

                // const float recordedDY = static_cast<float>(b->y) - static_cast<float>(a.y);

                //p->m_yVelocity = recordedDY;

                // log::info("recordedDY: {}", recordedDY);

                if (
                    std::fabs(iy - prevY) >
                    kWaveTeleportedTolerance * 8
                ) {
                    m_speedUpCameraAnimationAfterTeleportForNFrames =
                        5;
                }

                if (
                    m_speedUpCameraAnimationAfterTeleportForNFrames >
                    0
                ) {
                    if (m_gl) {
                        m_gl->updateCamera(20.0f);
                    }

                    m_speedUpCameraAnimationAfterTeleportForNFrames--;
                }

                p->setRotation(a.rot);

                if (m_p2JustSpawned) {
                    m_p2JustSpawned = false;
                }
            }
        }

            if (setClicks) {
                const size_t clickEnd = endIdx;

                // Default mode
                size_t clickStart = clickEnd;

                if (m_accurateHighTPSReplay) {
                    // Accurate mode
                    if (lastEmitIdx != kNoEmitIdx) {
                        if (lastEmitIdx < clickEnd) {
                            clickStart = lastEmitIdx + 1;
                        }
                        else if (lastEmitIdx == clickEnd) {
                            clickStart = clickEnd + 1;
                        }
                        else {
                            // Replay time moved backwards
                            clickStart = clickEnd;
                        }
                    }
                }

                constexpr size_t kClickLeadFrames = 1;

                for (size_t i = clickStart; i <= clickEnd && i < v.size(); ++i) {
                const size_t clickI = std::min(
                    v.size() - 1,
                    i + kClickLeadFrames
                );

                const Frame& F = v[clickI];
                if (F.hold != botPrevHold) {
                    // best method but might not work due to Click Between Frames
                    m_pl->handleButton(
                        F.hold,
                        /*btn=*/1,
                        /*isP1=*/isP1
                    );

                    // if didn't work
                    const bool actualHold = isP1 ? p1Hold : p2Hold;

                    if (F.hold != actualHold) {
                        if (F.hold) p->pushButton(PlayerButton::Jump);
                        else p->releaseButton(PlayerButton::Jump);
                    }

                    botPrevHold = F.hold;
                }

                if (F.holdL != botPrevHoldL) {
                    m_pl->handleButton(
                        F.holdL,
                        /*btn=*/2,
                        /*isP1=*/isP1
                    );

                    const bool actualHoldL = isP1 ? p1LHold : p2LHold;

                    if (F.holdL != actualHoldL) {
                        if (F.holdL) p->pushButton(PlayerButton::Left);
                        else p->releaseButton(PlayerButton::Left);
                    }

                    botPrevHoldL = F.holdL;
                }

                if (F.holdR != botPrevHoldR) {
                    m_pl->handleButton(
                        F.holdR,
                        /*btn=*/3,
                        /*isP1=*/isP1
                    );

                    const bool actualHoldR = isP1 ? p1RHold : p2RHold;

                    if (F.holdR != actualHoldR) {
                        if (F.holdR) p->pushButton(PlayerButton::Right);
                        else p->releaseButton(PlayerButton::Right);
                    }

                    botPrevHoldR = F.holdR;
                }
            }
            lastEmitIdx = clickEnd;
        }

        lastPoseIdx = endIdx;
    }

void Ghosts::forcePlayerHoldState_(PlayerObject* p, bool holdJump, bool holdL, bool holdR) {
        if (!p) return;
        
        if (holdJump) {
            p->pushButton(PlayerButton::Jump);
        } else {
            p->releaseButton(PlayerButton::Jump);
        }
        
        if (holdL) {
            p->pushButton(PlayerButton::Left);
            p->m_holdingLeft = true;
        } else {
            p->releaseButton(PlayerButton::Left);
            p->m_holdingLeft = false;
        }
        
        if (holdR) {
            p->pushButton(PlayerButton::Right);
            p->m_holdingRight = true;
        } else {
            p->releaseButton(PlayerButton::Right);
            p->m_holdingRight = false;
        }
    }

