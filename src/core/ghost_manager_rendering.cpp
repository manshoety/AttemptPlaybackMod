#include "ghost_manager.hpp"

void Ghosts::postUpdate() {
        if (!modEnabled || !m_pl || m_is_quitting) { 
            ++m_frameCounter; 
            return; 
        }
                
        if (m_pendingPracticeStartPosInit) {
            if (!botActive || !m_pl->m_player1 || !m_replayStartPosObject) {
                m_pendingPracticeStartPosInit = false;
                m_pendingPracticeStartPosFrames = 0;
            }
            else {
                ++m_pendingPracticeStartPosFrames;

                const cocos2d::CCPoint playerPos = m_pl->m_player1->getPosition();
                const float dist = ccpDistance(m_currentReplayStartPos, playerPos);

                if (dist > kReplayStartTolerance && m_pendingPracticeStartPosFrames < 3) {
                    ++m_frameCounter;
                    return;
                }

                if (dist > kReplayStartTolerance) {
                    log::warn(
                        "[Practice replay] start pos did not place correctly: expected=({:.2f}, {:.2f}), actual=({:.2f}, {:.2f})",
                        m_currentReplayStartPos.x,
                        m_currentReplayStartPos.y,
                        playerPos.x,
                        playerPos.y
                    );
                }

                initBotAfterReset_(true);
                m_pendingPracticeStartPosInit = false;
                m_pendingPracticeStartPosFrames = 0;
                m_playbackArmed = true;

                ++m_frameCounter;
                return;
            }
        }

        updateMirrorVisuals_();

        //double tStart = getTimeMs();

        //log::error("[postUpdate] playerx: {}", m_pl->m_player1->m_positionX);

        const double dt = m_currentSessionTime - m_prevSessionTime;
        m_prevSessionTime = m_currentSessionTime;

        if (isPureRecordingMode_() || isPreloadingAttempts()) {
            ++m_frameCounter;
            if (m_autosaveEnabled && hasCurrentPersistenceTarget_()) {
                m_autosaveAccum += static_cast<float>(dt);
                const float every = std::max(1, m_autosaveMinutes) * 60.0f;
                if (m_autosaveAccum >= every) {
                    m_autosaveAccum = 0.f;
                    saveNewAttemptsForCurrentLevel();
                }
            }
            return;
        }

        if (m_pauseGameAtTheEndOfReplayBot && botActive && 
            m_pl->m_endXPosition > 0 && 
            playerX_() + 750 >= m_pl->m_endXPosition && 
            safeMode_enabled) {
            m_pl->pauseGame(false);
        }

        m_p2Moving = (m_px2 != m_prevpx2);

        if (opacity == 0) {
            if (!m_zeroOpacityLatched) {
                for (auto& a : attempts) {
                    if (a.g1) { a.setP1Visible(false); }
                    if (a.g2) { a.setP2Visible(false); }
                    a.primedP1 = a.primedP2 = false;
                }
                m_zeroOpacityLatched = true;
            }
            ++m_frameCounter;
            return;
        }
        m_zeroOpacityLatched = false;

        const bool drawGhosts = (playback || showWhilePlaying || botActive);

        //log::info("[TIME TAKEN] postupdate initial {:.9f}", getTimeMs() - tStart);
        
        if (drawGhosts && !m_1PreloadedSoDontShowGhosts) {
            m_ghostUpdateAccum += static_cast<float>(dt);
            
            // Check if ghost offset changed
            if (m_randomDistPx != m_randomDistPxOld) {
                m_randomDistPxOld = m_randomDistPx;
                applyRandomOffsetsAll_();
            }

            // Determine if we should do a ghost tick
            float dtGhost = 0.f;
            bool doGhostTick = false;
            int GhostsVisibleThisFrame = 0;
            
            if (m_perfUnlimited) {
                doGhostTick = true;
                dtGhost = static_cast<float>(dt);
                m_ghostUpdateAccum = 0.f;
            } else if (m_ghostUpdateAccum >= m_ghostUpdateInterval) {
                doGhostTick = true;
                dtGhost = std::min(m_ghostUpdateAccum, 2.f * m_ghostUpdateInterval);
                m_ghostUpdateAccum = std::fmod(m_ghostUpdateAccum, m_ghostUpdateInterval);
            }

            if (doGhostTick) {
                const size_t stepFrames = (m_lastGhostTickFrame == 0) ? 1 : 
                    std::max<size_t>(1, m_frameCounter - m_lastGhostTickFrame);
                m_lastGhostTickFrame = m_frameCounter;
                
                const bool lockGhostX = shouldLockGhostX_();
                const float px = lockGhostX && m_pl->m_player1
                    ? m_pl->m_player1->getPositionX() : playerX_();
                const float px2 = lockGhostX && m_pl->m_player2 && m_gl && m_gl->m_gameState.m_isDualMode
                    ? m_pl->m_player2->getPositionX() : playerX2_();

                // Pre-compute screen culling bounds
                auto* director = cocos2d::CCDirector::sharedDirector();
                auto* worldNode = m_pl->m_objectLayer;
                
                cocos2d::CCPoint tlScreen{director->m_fScreenLeft, director->m_fScreenTop};
                cocos2d::CCPoint brScreen{director->m_fScreenRight, director->m_fScreenBottom};
                
                cocos2d::CCPoint tl = worldNode->convertToNodeSpace(tlScreen);
                cocos2d::CCPoint br = worldNode->convertToNodeSpace(brScreen);

                // Handle mirror portal coordinate inversion
                if (tl.x > br.x) std::swap(tl.x, br.x);
                if (br.y > tl.y) std::swap(br.y, tl.y);
                
                // Add margins
                tl.x -= 30.f; tl.y += 30.f;
                br.x += 30.f; br.y -= 30.f;


                const bool skip_m_grid = false;
                if (skip_m_grid) {
                    m_cachedCandidates = m_preloadedIndices;
                }
                else {
                    if (m_replayKind == ReplayKind::BestSingle && m_onlyShowGhostsThatPassedPercent) {
                        // Get view bounds and candidates, cached by bin
                        const int wantL = XBatchGrid::binOf(tl.x, m_gridThreshold.binW);
                        const int wantR = XBatchGrid::binOf(br.x, m_gridThreshold.binW);

                        if (wantL != m_cachedBinL || wantR != m_cachedBinR) {
                            m_gridThreshold.candidates_u32(tl.x, br.x, m_cachedCandidates);
                            m_cachedBinL = wantL;
                            m_cachedBinR = wantR;
                        }
                    }
                    else {
                        // Get view bounds and candidates, cached by bin
                        const int wantL = XBatchGrid::binOf(tl.x, m_grid.binW);
                        const int wantR = XBatchGrid::binOf(br.x, m_grid.binW);

                        if (wantL != m_cachedBinL || wantR != m_cachedBinR) {
                            m_grid.candidates_u32(tl.x, br.x, m_cachedCandidates);
                            m_cachedBinL = wantL;
                            m_cachedBinR = wantR;
                        }
                    }
                }

                // log::info("[SCREEN] tl.x: {}, br.x: {}, tl.y: {}, br.y: {}", tl.x, br.x, tl.y, br.y);
                // log::info("[BIN] m_cachedBinL: {}, m_cachedBinR: {}", m_cachedBinL, m_cachedBinR);

                // Pre-compute filter settings
                const std::optional<bool> wantPractice = botActive ? 
                    practiceFilterForGhosts_() : 
                    std::optional<bool>(m_pl && m_pl->m_isPracticeMode);
                
                const size_t bestIdx = onlyBestGhost ? pickWinningOrBestIndex_(wantPractice) : SIZE_MAX;
                const uint32_t candidateCount = m_cachedCandidates.size();
                
                // Pre-allocate with estimated capacity
                std::vector<size_t> showable;
                //showable.reserve(m_limitVisibleGhosts ? static_cast<size_t>(m_maxVisibleGhosts) : candidateCount);
                showable.reserve(candidateCount);

                //log::info("candidateCount: {}", candidateCount);

                // Prime any ones we wanted to prime but didn't have player objects for
                const int maxPreloadPerFrame = 9999;
                size_t currentPreloaded = 0;
                size_t numCurrentlyDead = 0;
                
                for (auto it = m_wantToPrimeIndices.begin(); 
                    it != m_wantToPrimeIndices.end(); ) {
                    if (currentPreloaded >= maxPreloadPerFrame) break;
                    if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) break;

                    size_t ai = *it;
                    Attempt& a = attempts[ai];

                    // false when the ghost would start already dead
                    if (primeGhostToPX_(a, ai, px, px2)) {
                        if (m_primedSet.insert(ai).second) m_primedIndices.push_back(ai);

                        if (!a.colorsAssigned) {
                            refreshAttemptColors_(a);
                        }
                        if (a.opacity != opacity) {
                            updateOpacityForAttempt(a, opacity);
                        }

                        showable.push_back(ai);
                        GhostsVisibleThisFrame ++;
                        currentPreloaded++;
                    }
                    else if (!a.completed) countGhostTextDeathOnce_(a);
                    it = m_wantToPrimeIndices.erase(it);

                }

                // Main candidate processing loop
                for (size_t cidx = 0; cidx < candidateCount; ++cidx) {
                    const size_t ai = m_cachedCandidates[cidx];
                    //log::info("cidx: {}", cidx);
                    //log::info("ai: {}", ai);
                    if (UNLIKELY(ai >= attempts.size())) continue;

                    if (m_limitVisibleGhosts && GhostsVisibleThisFrame >= m_maxVisibleGhosts) continue;
                    
                    Attempt& a = attempts[ai];

                    if (a.p1.empty()) continue;
                    if (onlyBestGhost && ai != bestIdx) continue;
                    //if (!matchesPracticeFilter_(a, wantPractice)) continue;
                    //if (!matchesStartPositionFilter_(a)) continue;

                    // Owner check
                    if (isOwnerAnywhereInRoute_(a)) {
                        if (a.g1) a.setP1Visible(false);
                        if (a.g2) a.setP2Visible(false);
                        a.primedP1 = a.primedP2 = false;
                        // log::info("[OWNER REJECT] ai: {}", ai);
                        continue;
                    }

                    const double ghostTime = std::max(0.0, m_currentSessionTime + a.ghostOffsetTime);
                    const double firstTimeP1 = a.p1.front().t;

                    // log::info("[OWNER] ai: {}", ai);
                    
                    if (ghostTime < firstTimeP1 - 0.0001) {
                        a.setP1Visible(false);
                        a.setP2Visible(false);
                        a.primedP1 = a.primedP2 = false;
                        continue;
                    }
                    // log::info("[TIME] ai: {}", ai);


                    if (a.eolFrozenP1) {
                        numCurrentlyDead++;
                        // remove on death (if ghosts explode, meaning they disapear so I can remove them early)
                        // or remove them if I need player objects freed up
                        if (a.completed || m_ghostsExplode || needToFreePlayerObjectsForWaitingQueue(numCurrentlyDead)) {
                            a.setP1Visible(false);
                                a.setP2Visible(false);
                                // pool release player objects
                                if (a.g1Idx >= 0) {
                                    m_playerObjectPool.releaseIndex(a.g1Idx);
                                    a.g1Idx = -1;
                                    a.g1 = nullptr;
                                }
                                if (a.g2Idx >= 0) {
                                    m_playerObjectPool.releaseIndex(a.g2Idx);
                                    a.g2Idx = -1;
                                    a.g2 = nullptr;
                                }
                                continue;
                        }

                        // Screen culling (dead and offscreen)
                        if (a.d1 < a.p1.size()) {
                            const float sx = a.p1[a.d1].x;
                            const float sy = a.p1[a.d1].y;
                            if (sx < tl.x || sx > br.x || sy < br.y || sy > tl.y) {
                                a.setP1Visible(false);
                                a.setP2Visible(false);
                                // pool release player objects
                                if (a.g1Idx >= 0) {
                                    m_playerObjectPool.releaseIndex(a.g1Idx);
                                    a.g1Idx = -1;
                                    a.g1 = nullptr;
                                }
                                if (a.g2Idx >= 0) {
                                    m_playerObjectPool.releaseIndex(a.g2Idx);
                                    a.g2Idx = -1;
                                    a.g2 = nullptr;
                                }
                                continue;
                            }
                        }
                    }

                    // log::info("[EOL] ai: {}", ai);

                    const bool showP2 = a.hadDual && !a.p2.empty() &&
                        (ghostTime >= a.p2.front().t - 0.0001);

                    if (!a.primedP1 || (showP2 && !a.primedP2)) {
                        if ((m_playerObjectPool.inUseCount() < m_playerObjectPool.capacity() - 2)
                    || a.g1 || a.g2) {
                            // false when the ghost would start already dead
                            if (primeGhostToPX_(a, ai, px, px2)) {
                                if (m_primedSet.insert(ai).second) m_primedIndices.push_back(ai);
                            }
                            else if (!a.completed) countGhostTextDeathOnce_(a);
                        }
                        else {
                            if (m_wantToPrimeSet.insert(ai).second) {
                                m_wantToPrimeIndices.push_back(ai);
                            }
                            continue;
                        }
                    }

                    // log::info("[PRIMED] ai: {}", ai);

                    if (!a.colorsAssigned) {
                        refreshAttemptColors_(a);
                    }
                    if (a.opacity != opacity) {
                        updateOpacityForAttempt(a, opacity);
                    }

                    showable.push_back(ai);
                    GhostsVisibleThisFrame ++;
                }

                // Update visible ghosts
                const int total = static_cast<int>(showable.size());
                if (total > 0) {
                    m_skipHardStreakCheck = true;
                    if (m_perfUnlimited) {
                        for (const size_t ai : showable) {
                            updateGhostForAttempt_(attempts[ai], px, px2, ai, 
                                dtGhost, stepFrames, m_IHateMirrorPortalsSoRewind);
                        }
                        m_rrPhase = 0;
                    } else {
                        const int batchSize = std::min(m_budgetPerFrame, total);
                        const int stride = std::max(1, (total + batchSize - 1) / batchSize);
                        
                        int updated = 0;
                        for (int j = 0; j < total && updated < batchSize; ++j) {
                            if ((j % stride) == m_rrPhase) {
                                updateGhostForAttempt_(attempts[showable[j]], px, px2, 
                                    showable[j], dtGhost, stepFrames, m_IHateMirrorPortalsSoRewind);
                                ++updated;
                            }
                        }
                        m_rrPhase = (m_rrPhase + 1) % stride;
                    }
                    m_skipHardStreakCheck = false;
                }
            }
        }

        m_IHateMirrorPortalsSoRewind = false;
        ++m_frameCounter;

        // Autosave check
        if (m_autosaveEnabled && hasCurrentPersistenceTarget_()) {
            m_autosaveAccum += static_cast<float>(dt);
            const float every = std::max(1, m_autosaveMinutes) * 60.0f;
            if (m_autosaveAccum >= every) {
                m_autosaveAccum = 0.f;
                saveNewAttemptsForCurrentLevel();
            }
        }

        // text goop
        updatePlayLayerGhostTextLabel_();
    }

void Ghosts::forceShowPlayerVisuals(PlayerObject* p) {
        if (!p) return;
        p->m_isDead = false;
        p->m_isHidden = false;
        //p->setVisible(true);
        p->setOpacity(opacity);
    }

bool Ghosts::primeGhostToPX_(Attempt& a, size_t attemptIdx, float pxw, float px2w) {
        //log::info("Priming");
        prepareAttemptVisualData_(a);
        const bool isHiddenOwner = isOwnerAnywhereInRoute_(a);
        const bool isRouteOwner = isRouteOwnerForOffset_(a);

        if (!a.offsetApplied) {
            if (isRouteOwner) {
                clearGhostOffset_(a);
            }
            else {
                applyRandomOffsetSingle(a, false);
                a.offsetApplied = true;
            }
        }

        double ghostTime = m_currentSessionTime + a.ghostOffsetTime;
        if (ghostTime < 0.0) ghostTime = 0.0;

        constexpr double kPrimeEolTolerance = 0.02;
        if (!a.p1.empty() && ghostTime > static_cast<double>(a.p1.back().t) + kPrimeEolTolerance) {
            return false;
        }
        
        if (!a.p1.empty() && !a.primedP1) {
            //log::info("p1 not created");
            if (!a.g1 && a.g1Idx < 0) {
                //log::info("allocating p1");
                auto h = m_playerObjectPool.acquireForOwner(a.serial, static_cast<uint32_t>(attemptIdx), false);
                if (h) {
                    //log::info("allocated correctly");
                    a.g1Idx = h.index;
                    a.g1 = h.po;
                }
                //else log::info("allocated failed");
                // Set initial mode
                if (a.g1 && !a.p1.empty()) {
                    //log::info("g1 and not empty");
                    a.g1CurMode = a.p1.front().mode;
                    setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
                }
                //else log::info("NOT g1 or empty");
            }
            
            a.i1 = idxForTime(a.acc1Time, a.p1, ghostTime);
            a.d1 = std::min(a.i1, a.p1.size() - 1);

            const Frame& f = a.p1[a.d1];

            if (a.g1) {
                if (UNLIKELY(a.g1CurMode != f.mode)) {
                    setPOFrameForIcon(a.g1, f.mode, a, m_randomIcons);
                    a.g1CurMode = f.mode;
                }
                //if (currentMode(a.g1, false) != a.g1CurMode) {
                //    log::info("Current mode not correct: {} should be {}", (int)currentMode(a.g1, false), (int)a.g1CurMode);
                //}
                PoseCache& pc = a.last1;
                applyPoseHard(a.g1, f, m_pl);
                // setPOFrameForIcon(a.g1, f.mode, a, m_randomIcons);
                refreshAttemptColors_(a);
                const bool lockToPlayerX = shouldLockGhostX_() && a.ghostOffsetPx == 0.f;
                if (lockToPlayerX) a.g1->setPositionX(pxw);
                pc.x = lockToPlayerX ? pxw : static_cast<float>(f.x);
                pc.y = static_cast<float>(f.y);
                pc.rot = static_cast<float>(f.rot);
                pc.vehicleSize = static_cast<float>(f.vehicleSize);
                pc.upsideDown = f.upsideDown;
                a.g1->stopDashing();
                pc.isDashing = false;
                
                // hardResetWaveTrail(a.g1);
                a.g1->m_waveTrail->reset();
                a.trailactive1 = false;
                forceShowPlayerVisuals(a.g1);
                a.setP1Visible(f.isVisible, true);
            }

            a.previouslyHolding1 = f.hold;
            a.previouslyHoldingL1 = f.holdL;
            a.previouslyHoldingR1 = f.holdR;
            a.primedP1 = true;
        }

        const bool p2HasStarted =
            a.hadDual &&
            !a.p2.empty() &&
            ghostTime >= static_cast<double>(a.p2.front().t) - 0.0001;

        if (!p2HasStarted) {
            if (a.g2) {
                a.setP2Visible(false, true);
                if (a.g2->m_waveTrail) {
                    a.g2->m_waveTrail->reset();
                    a.g2->m_waveTrail->setVisible(false);
                }
            }
            a.trailactive2 = false;
            a.prevTeleported2 = false;
        }
        else if (!a.primedP2) {
            if (!a.g2 && a.g2Idx < 0) {
                auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000, static_cast<uint32_t>(attemptIdx), true);
                if (h2) {
                    a.g2Idx = h2.index;
                    a.g2 = h2.po;
                }
                if (a.g2 && !a.p2.empty()) {
                    a.g2CurMode = a.p2.front().mode;
                    setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
                }
            }
            a.i2 = idxForTime(a.acc2Time, a.p2, ghostTime);
            a.d2 = std::min(a.i2, a.p2.size() - 1);

            const Frame& f2 = a.p2[a.d2];

            if (a.g2) {
                if (UNLIKELY(a.g2CurMode != f2.mode)) {
                    setPOFrameForIcon(a.g2, f2.mode, a, m_randomIcons);
                    a.g2CurMode = f2.mode;
                }
                PoseCache& pc2 = a.last2;
                applyPoseHard(a.g2, f2, m_pl);
                const bool lockToPlayerX = shouldLockGhostX_() && a.ghostOffsetPx == 0.f &&
                    m_gl && m_gl->m_gameState.m_isDualMode;
                if (lockToPlayerX) a.g2->setPositionX(px2w);
                refreshAttemptColors_(a);
                pc2.x = lockToPlayerX ? px2w : static_cast<float>(f2.x);
                pc2.y = static_cast<float>(f2.y);
                pc2.rot = static_cast<float>(f2.rot);
                pc2.vehicleSize = static_cast<float>(f2.vehicleSize);
                pc2.upsideDown = f2.upsideDown;

                a.g2->stopDashing();
                pc2.isDashing = false;
                
                // hardResetWaveTrail(a.g2);
                a.g2->m_waveTrail->reset();
                a.trailactive2 = false;
                forceShowPlayerVisuals(a.g2);
                a.setP2Visible(f2.isVisible, true);
            }

            a.previouslyHolding2 = f2.hold;
            a.previouslyHoldingL2 = f2.holdL;
            a.previouslyHoldingR2 = f2.holdR;
            a.primedP2 = true;

            refreshAttemptColors_(a);
        }
        return true;
    }

int Ghosts::pickWinningOrBestSerialFromPosition_(
        std::optional<bool> wantPractice,
        float startPosX,
        float startPosY,
        float tolerance) const {
        int bestWinSerial = -1;
        float bestWinX = -1.f;

        int bestAnySerial = -1;
        float bestAnyX = -1.f;

        const float tolSq = tolerance * tolerance;
        std::unordered_set<int> seenLoaded;
        seenLoaded.reserve(attempts.size());

        // L bozo platformer mode this is being wacky for initial teleports in levels due to y offset
        auto matchesStart = [&](float attemptStartX, float attemptStartY) -> bool {
            const float dx = attemptStartX - startPosX;
            //const float dy = attemptStartY - startPosY;
            //log::info("attemptStartX: {}, attemptStartY: {}", attemptStartX, attemptStartY);
            //log::info("startPosX: {}, startPosY: {}", startPosX, startPosY);

            //return (dx * dx + dy * dy) <= tolSq;
            return fabs(dx) <= tolerance;
        };

        //auto matchesStart = [&](float attemptStartX) -> bool {
        //    return std::fabs(attemptStartX - startPosX) <= tolerance;
        //};

        auto consider = [&](int serial,
                            bool completed,
                            bool practiceAttempt,
                            float attemptStartX,
                            float attemptStartY,
                            float endX,
                            bool hasP1Data) {
            if (!hasP1Data) return;
            //log::info("hasP1Data");
            if (!matchesPracticeValue_(practiceAttempt, wantPractice)) return;
            //log::info("matchesPracticeValue_");
            if (!matchesStart(attemptStartX, attemptStartY)) return;
            //log::info("matchesStart");
            // if (!matchesStart(attemptStartX)) return;

            if (completed) {
                if (endX > bestWinX || (endX == bestWinX && serial > bestWinSerial)) {
                    bestWinX = endX;
                    bestWinSerial = serial;
                }
            }

            if (endX > bestAnyX || (endX == bestAnyX && serial > bestAnySerial)) {
                bestAnyX = endX;
                bestAnySerial = serial;
            }
        };

        for (const auto& a : attempts) {
            seenLoaded.insert(a.serial);

            if (a.p1.empty()) continue;
            consider(
                a.serial,
                a.completed,
                a.practiceAttempt,
                static_cast<float>(a.p1.front().x),
                a.startY,
                static_cast<float>(a.p1.back().x),
                true
            );
        }

        for (const auto& entry : m_attemptCatalog) {
            if (seenLoaded.count(entry.serial) != 0) continue;

            consider(
                entry.serial,
                entry.completed,
                entry.practiceAttempt,
                entry.startX,
                entry.startY,
                entry.endX,
                entry.p1Count > 0
            );
        }

        if (bestWinSerial > 0) return bestWinSerial;
        return bestAnySerial;
    }

size_t Ghosts::pickWinningOrBestIndex_(std::optional<bool> wantPractice) const {
        if (m_lastWinSerial != -1) {
            for (size_t i = 0; i < attempts.size(); ++i) {
                const auto& a = attempts[i];
                if (a.serial == m_lastWinSerial && !a.p1.empty() && matchesPracticeFilter_(a, wantPractice)) {
                    return i;
                }
            }
        }

        // best completed
        size_t bestWin = SIZE_MAX;
        float  bestWinX = -1.f;
        int    bestWinSerial = -1;
        for (size_t i = 0; i < attempts.size(); ++i) {
            const auto& a = attempts[i];
            if (!a.completed || a.p1.empty()) continue;
            if (!matchesPracticeFilter_(a, wantPractice)) continue;
            float x = lastXOf_(a.p1);
            if (x > bestWinX || (x == bestWinX && a.serial > bestWinSerial)) {
                bestWinX = x;
                bestWinSerial = a.serial;
                bestWin = i;
            }
        }
        if (bestWin != SIZE_MAX) return bestWin;

        // any best
        size_t bestAny = SIZE_MAX;
        float  bestAnyX = -1.f;
        int    bestAnySerial = -1;
        for (size_t i = 0; i < attempts.size(); ++i) {
            const auto& a = attempts[i];
            if (a.p1.empty()) continue;
            if (!matchesPracticeFilter_(a, wantPractice)) continue;
            float x = lastXOf_(a.p1);
            if (x > bestAnyX || (x == bestAnyX && a.serial > bestAnySerial)) {
                bestAnyX = x;
                bestAnySerial = a.serial;
                bestAny = i;
            }
        }
        return bestAny;
    }

const Attempt* Ghosts::getReplayOwner_(double sessionTime) {
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                m_currentOwner = ensureAttemptLoadedBySerialAndPractice_(m_replayOwnerSerial,false,false);
                return m_currentOwner;
            }

            if (m_compOwnerIdx < attempts.size() &&
                !attempts[m_compOwnerIdx].practiceAttempt) {
                m_replayOwnerSerial = attempts[m_compOwnerIdx].serial;
                m_currentOwner = &attempts[m_compOwnerIdx];
                return m_currentOwner;
            }

            m_currentOwner = nullptr;
            return nullptr;
        }

        constexpr double kPhysicsTick = 1.0 / 240.0;
        const int serial = m_checkpointMgr.findOwnerSerialForTimeWithBridge(sessionTime,kPhysicsTick);

        if (serial <= 0) {
            m_currentOwner = nullptr;
            return nullptr;
        }

        Attempt* owner = ensurePracticeReplayOwnerLoaded_(serial, "owner-transition");
        if (!owner) {
            m_currentOwner = nullptr;
            return nullptr;
        }

        const bool ownerChanged = serial != m_replayOwnerSerial || m_currentOwner != owner;

        m_replayOwnerSerial = serial;
        m_currentOwner = owner;

        if (ownerChanged) {
            seekReplayCursorsToTimeExact_(*owner, sessionTime);

            m_lastEmitIdx1 = (!owner->p1.empty() && m_replayIdx1 > 0)
                     ? m_replayIdx1 - 1
                     : kNoEmitIdx;

            m_lastEmitIdx2 = (owner->hadDual && !owner->p2.empty() && m_replayIdx2 > 0)
                     ? m_replayIdx2 - 1
                     : kNoEmitIdx;

            m_lastPoseIdx1 = m_lastEmitIdx1;
            m_lastPoseIdx2 = m_lastEmitIdx2;

            m_explicitWavePointAddedPrevPoseP1 = false;
            m_explicitWavePointAddedPrevPoseP2 = false;
            m_suppressNextExplicitWavePointP1 = false;
            m_suppressNextExplicitWavePointP2 = false;
        }
        return m_currentOwner;
    }

void Ghosts::updateGhostForAttempt_(Attempt& a, float px, float px2, size_t ai,
                            float dt, size_t stepFrames, bool mustRewind) {
        
        //double tStart2 = getTimeMs();     
    
        const double ghostTime = std::max(0.0, m_currentSessionTime + a.ghostOffsetTime);
        constexpr double kEolTimeTolerance = 0.02;

        // Unified player processing lambda
        auto processPlayer = [&](
            std::vector<Frame>& frames,
            FrameAccelTime& accTime,
            geode::Ref<PlayerObject>& ghost,
            PoseCache& pc,
            IconType& curMode,
            size_t& frameIdx,
            size_t& drawIdx,
            bool& primed,
            bool& eolFrozen,
            bool& trailActive,
            bool& prevHolding,
            bool& prevDartSlide,
            bool& prevTeleported,
            bool& hasWavePointData,
            bool completedLevel,
            bool isP2,
            float playerX
        ) {
            if (frames.empty() || !primed) return;
            // if (isP2 && !m_p2Moving) return;
            const bool lockToPlayerX = shouldLockGhostX_() && a.ghostOffsetPx == 0.f &&
                (!isP2 || (m_gl && m_gl->m_gameState.m_isDualMode));

            const size_t lastIdx = frames.size() - 1;
            const double lastT = frames.back().t;

            double tStart = getTimeMs();  
            size_t previousIDXForWavePoints = std::min(frameIdx, lastIdx);
            if (!eolFrozen) {
                
                size_t previousIDX = frameIdx;
                
                // Time-based index lookup
                if (LIKELY(!mustRewind)) {
                    advanceFromPrevIdx_(frames, frameIdx, ghostTime, /*mustRewind=*/false);
                } else {
                    frameIdx = idxForTimeBounded_(frames, accTime, ghostTime);
                    if (ghost && ghost->m_waveTrail) ghost->m_waveTrail->reset();
                }

                const size_t timeIdx = std::min(frameIdx, lastIdx);
                drawIdx = timeIdx;

                if (!isP2) {
                    drawIdx = std::min(timeIdx + 2, lastIdx);
                }

                const bool atLastFrame = (drawIdx >= lastIdx);
                const bool atEndTime   = (ghostTime >= lastT - kEolTimeTolerance);

                // EOL freeze check
                if ((atLastFrame || atEndTime) && !isP2) {
                    eolFrozen = true;

                    if (completedLevel) {
                        a.eolFrozenP1 = true;
                        a.eolFrozenP2 = true;

                        a.setP1Visible(false, true);
                        a.setP2Visible(false, true);

                        if (a.g1) {
                            a.g1->stopDashing();

                            if (a.g1->m_waveTrail) {
                                a.g1->m_waveTrail->reset();
                                a.g1->m_waveTrail->setVisible(false);
                                a.g1->m_waveTrail->setOpacity(0);
                            }
                        }

                        if (a.g2) {
                            a.g2->stopDashing();

                            if (a.g2->m_waveTrail) {
                                a.g2->m_waveTrail->reset();
                                a.g2->m_waveTrail->setVisible(false);
                                a.g2->m_waveTrail->setOpacity(0);
                            }
                        }

                        a.trailactive1 = false;
                        a.trailactive2 = false;
                        a.prevTeleported1 = false;
                        a.prevTeleported2 = false;
                        a.hasWavePointData = false;

                        return;
                    }

                    // Non-completed ghosts
                    a.eolFrozenP2 = true;
                    if (ghost) {
                        ghost->stopDashing();

                        if (m_ghostsExplode) {
                            ghost->playDeathEffect();
                            if (a.g2 && a.g2 != ghost && a.g2->isVisible()) {
                                a.g2->stopDashing();
                                a.g2->playDeathEffect();
                            }
                        }

                        if (m_ghostsExplodeSFX && m_fmodEngine) {
                            if (m_fmodEngine->m_sfxVolume != 0) {
                                static std::mt19937 rng(
                                    static_cast<uint32_t>(
                                        std::chrono::high_resolution_clock::now()
                                            .time_since_epoch()
                                            .count()
                                    )
                                );

                                std::string sfxPath;

                                if (m_useCustomExplodeSounds) {
                                    refreshAndPreloadCustomExplodeSfx_(false);

                                    if (!customSfx.empty()) {
                                        std::uniform_int_distribution<size_t> dist(0, customSfx.size() - 1);
                                        sfxPath = customSfx[dist(rng)];
                                    }
                                }

                                if (sfxPath.empty()) {
                                    sfxPath = "explode_11.ogg"_spr;
                                }

                                g_explodeLimiter.play(sfxPath, 1.f, 1.f);
                            }
                        }
                    }

                    countGhostTextDeathOnce_(a);
                }

                // Make P2 invisible when there's a P2 data gap (not in dual mode)
                if (isP2 && ghost) {
                    bool hideForGap = false;

                    if (timeIdx < lastIdx) {
                        const double t0 = frames[timeIdx].t;
                        const double t1 = frames[timeIdx + 1].t;
                        const double gap = t1 - t0;

                        if (gap > kNoDualTimeGap && ghostTime > t0 && ghostTime < t1) {
                            hideForGap = true;
                        }
                    }

                    bool hideForP2EndedWhileP1Continues = false;
                    if (!hideForGap && !eolFrozen) {
                        const bool atLastFrameByTime = timeIdx >= lastIdx;
                        const bool atEndTimeByTime = ghostTime >= lastT - kEolTimeTolerance;

                        if (atLastFrameByTime || atEndTimeByTime) {
                            const double p2LastT = lastT;
                            const double p1LastT = a.p1.empty()
                                ? p2LastT
                                : static_cast<double>(a.p1.back().t);

                            if (p1LastT - p2LastT > kP1ContinuesPastP2Threshold) {
                                hideForP2EndedWhileP1Continues = true;
                            }
                        }
                    }

                    if (hideForGap || hideForP2EndedWhileP1Continues) {
                        a.setP2Visible(false, true);

                        if (ghost->m_waveTrail) {
                            ghost->m_waveTrail->reset();
                            ghost->m_waveTrail->setVisible(false);
                            ghost->m_waveTrail->setOpacity(0);
                        }

                        trailActive = false;
                        prevTeleported = false;
                        hasWavePointData = false;

                        return;
                    }
                }

                if (frameIdx == previousIDX) {
                    if (lockToPlayerX && ghost) {
                        ghost->setPositionX(playerX);
                        pc.x = playerX;
                    }
                    return;
                }

                //log::info("[TIME TAKEN] updateghost advance {:.8f}", getTimeMs() - tStart);
                //tStart = getTimeMs();  

                
            }
            //else {
            //    if (m_ghostsExplode) ghost->setOpacity(0); //  Ensure invisible if dead and exploded
            //}

            const Frame& f = frames[drawIdx];
            const size_t nextIdx = std::min(drawIdx + 1, lastIdx);
            const Frame& fNext = frames[nextIdx];

            const float fx = f.x;
            const float fy = f.y;
            const float fr = f.rot;
            const float fv = f.vehicleSize;
            const float fw = f.waveSize;
            const double ft = f.t;

            const float nx = fNext.x;
            const float ny = fNext.y;
            const float nr = fNext.rot;
            const float nv = fNext.vehicleSize;
            const float nw = fNext.waveSize;
            const double nt = fNext.t;

            bool recordedGoingLeft = false;
            if (nextIdx != drawIdx) {
                recordedGoingLeft = nx < fx;
            } else if (drawIdx > 0) {
                recordedGoingLeft = fx < static_cast<float>(frames[drawIdx - 1].x);
            }

            const float lasty = pc.y;

            float ix = fx, iy = fy, irot = fr;
            float iveh = fv, iwave = fw;

            if (m_useInterpolation && !eolFrozen && nextIdx != drawIdx) {
                const double denom = nt - ft;
                if (denom > 1e-9) {
                    const float tt = static_cast<float>(clamp01_((ghostTime - ft) / denom));
                    ix = lerp_(fx, nx, tt);
                    iy = lerp_(fy, ny, tt);
                    irot = lerpAngleDeg_(fr, nr, tt);
                    iveh = lerp_(fv, nv, tt);
                    iwave = lerp_(fw, nw, tt);
                }
            }

            //log::info("[TIME TAKEN] updateghost interp {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            if (!ghost) return;

            // For P2, check if started
            if (isP2) {
                const double firstT = static_cast<double>(frames.front().t);
                if (ghostTime + 0.0001 < firstT) {
                    a.setP2Visible(false, true);
                    return;
                }
            }

            // Mode change
            if (UNLIKELY(curMode != f.mode)) {
                setPOFrameForIcon(ghost, f.mode, a, m_randomIcons);
                curMode = f.mode;
                pc.mode = f.mode;
                refreshAttemptColors_(a);
            }

            if (UNLIKELY(a.opacity != opacity)) {
                updateOpacityForAttempt(a, opacity);
            }

            //log::info("[TIME TAKEN] updateghost modechange {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            // Apply pose
            if (lockToPlayerX) ix = playerX;
            ghost->setPosition({ix, iy});
            ghost->setRotation(irot);

            //log::info("[TIME TAKEN] updateghost posupdate {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            // Vehicle size update
            if (pc.vehicleSize != iveh || ghost->m_vehicleSize != iveh) {
                syncPlayerSize(ghost, iveh);
                pc.vehicleSize = iveh;
            }

            pc.x = ix;
            pc.y = iy;
            pc.rot = irot;

            // Gravity sync
            if (pc.upsideDown != f.upsideDown) {
                syncGhostGravity(ghost, f.upsideDown, m_pl);
                pc.upsideDown = f.upsideDown;
            }

            // Dashing state
            if (pc.isDashing != f.isDashing) {
                if (f.isDashing) {
                    ghost->startDashing(isP2 ? attemptplayback::p1d() : attemptplayback::p0d());
                } else {
                    ghost->stopDashing();
                }
                pc.isDashing = f.isDashing;
            }
            
            //if (f.isDashing) {
            //    ghost->updateDashAnimation();
            //    ghost->updateDashArt();
            //}
            // if (f.mode == IconType::Spider) ghost->playDynamicSpiderRun();

            // Visibility
            if (!isP2) a.setP1Visible(f.isVisible && (!eolFrozen || !m_ghostsExplode));
            else a.setP2Visible(f.isVisible && (!eolFrozen || !m_ghostsExplode));

            ghost->setZOrder(0);

            if (m_animateSpider && !f.isDashing && f.mode == IconType::Spider) { // Tween for smoother transitions
                if (fNext.x == f.x) {
                    if (pc.spiderState != IconAnimationState::Idle) {
                        ghost->m_spiderSprite->runAnimation("idle01");
                        pc.spiderState = IconAnimationState::Idle;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                else {
                    if (pc.spiderState != IconAnimationState::Run) {
                        ghost->m_spiderSprite->runAnimation("run");
                        pc.spiderState = IconAnimationState::Run;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                if (fNext.y < f.y) { // Falling
                    if (f.upsideDown) {
                        if (pc.spiderState != IconAnimationState::Jump) {
                            ghost->m_spiderSprite->runAnimation("jump_loop");
                            pc.spiderState = IconAnimationState::Jump;
                            refreshOpacityForAttemptNoWaveTrail(a);
                        }
                    }
                    else if (pc.spiderState != IconAnimationState::Fall) {
                        ghost->m_spiderSprite->runAnimation("fall_loop");
                        pc.spiderState = IconAnimationState::Fall;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                else if (fNext.y > f.y) { // Jumping
                    if (f.upsideDown) {
                        if (pc.spiderState != IconAnimationState::Jump) {
                            ghost->m_spiderSprite->runAnimation("fall_loop");
                            pc.spiderState = IconAnimationState::Jump;
                            refreshOpacityForAttemptNoWaveTrail(a);
                        }
                    }
                    else if (pc.spiderState != IconAnimationState::Fall) {
                        ghost->m_spiderSprite->runAnimation("jump_loop");
                        pc.spiderState = IconAnimationState::Fall;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
            }

            if (m_animateRobot && !f.isDashing && f.mode == IconType::Robot) { //  Tween for smoother transitions between states (but extra compute)
                if (fNext.x == f.x) {
                    //if (pc.robotState != RobotState::Idle) 
                    // ghost->m_robotSprite->tweenToAnimation("idle01", 0.1f);
                    if (pc.robotState != IconAnimationState::Idle) {
                        ghost->m_robotSprite->runAnimation("idle01");
                        pc.robotState = IconAnimationState::Idle;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                else {
                    //if (pc.robotState != RobotState::Run) 
                    //ghost->m_robotSprite->tweenToAnimation("run", 0.1f);
                    if (pc.robotState != IconAnimationState::Run) {
                        ghost->m_robotSprite->runAnimation("run");
                        pc.robotState = IconAnimationState::Run;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                if (fNext.y < f.y) { // Falling
                    if (f.upsideDown) {
                        //if (pc.robotState != RobotState::Jump) 
                        //ghost->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
                        if (pc.robotState != IconAnimationState::Jump) {
                            ghost->m_robotSprite->runAnimation("jump_loop");
                            pc.robotState = IconAnimationState::Jump;
                            refreshOpacityForAttemptNoWaveTrail(a);
                        }
                    }
                    //else if (pc.robotState != RobotState::Fall) 
                    //else ghost->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
                    else if (pc.robotState != IconAnimationState::Fall) {
                        ghost->m_robotSprite->runAnimation("fall_loop");
                        pc.robotState = IconAnimationState::Fall;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
                else if (fNext.y > f.y) { // Jumping
                    if (f.upsideDown) {
                        //if (pc.robotState != RobotState::Jump) 
                        // ghost->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
                        if (pc.robotState != IconAnimationState::Jump) {
                            ghost->m_robotSprite->runAnimation("fall_loop");
                            pc.robotState = IconAnimationState::Jump;
                            refreshOpacityForAttemptNoWaveTrail(a);
                        }
                    }
                    //else if (pc.robotState != RobotState::Fall) 
                    //else ghost->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
                    else if (pc.robotState != IconAnimationState::Fall) {
                        ghost->m_robotSprite->runAnimation("jump_loop");
                        pc.robotState = IconAnimationState::Fall;
                        refreshOpacityForAttemptNoWaveTrail(a);
                    }
                }
            }

            //log::info("[TIME TAKEN] updateghost other updates {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            // Wave trail update - only if in wave mode and visible
            if (waveTrailGhost) {
                const bool inWave = (f.mode == IconType::Wave);
                auto* trail = ghost->m_waveTrail;

                if (!inWave) {
                    if (trailActive) {
                        if (trail) {
                            trail->reset();
                            trail->setVisible(false);
                            trail->setOpacity(0);
                        }
                        trailActive = false;
                    } else if (trail && (trail->isVisible() || trail->getOpacity() != 0)) {
                        trail->reset();
                        trail->setVisible(false);
                        trail->setOpacity(0);
                    }
                } else {
                    if (!trailActive) {
                        // hardResetWaveTrail(ghost);
                        // ghost->activateStreak();
                        trailActive = true;
                        if (trail) {
                            trail->reset();
                            trail->setOpacity((GLubyte)std::clamp<int>(
                        int(opacity * (m_waveTrailOpacityPct / 100.f)), 0, 255));
                                    
                            trail->setZOrder(-3);
                            trail->setVisible(true);
                            //log::info("a");
                            waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                        }
                        else log::info("NO GHOST TRAIL");
                    }

                    if (trailActive && trail) {
                        const bool isMovingUp = (iy > lasty);
                        bool addedWavePoint = false;
                        // Add points on hold/slide state changes
                        if (drawIdx > previousIDXForWavePoints) {
                            const size_t waveStart = std::min(previousIDXForWavePoints + 1, lastIdx);
                            const size_t waveEnd = std::min(drawIdx, lastIdx);

                            for (size_t wi = waveStart; wi <= waveEnd; ++wi) {
                                const Frame& wf = frames[wi];

                                if (wf.wavePointThisFrame) {
                                    hasWavePointData = true;

                                    // log::info("slide x: {}, y: {}", static_cast<float>(wf.x), static_cast<float>(wf.y));

                                    if (wi == drawIdx) {
                                        //log::info("b");
                                        waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                                    } else {
                                        //log::info("c");
                                        waveTrailAddPointToPlayer(trail, {wf.x, wf.y}, !isP2, false, recordedGoingLeft);
                                    }
                                }
                            }
                        } else if (f.wavePointThisFrame) {
                            hasWavePointData = true;
                            //log::info("d");
                            // log::info("wavePointThisFrame x: {}, y: {}", ix, iy);
                            waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                        }
                        if (!hasWavePointData) {
                            if (f.hold != prevHolding) {
                                //log::info("e");
                                waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                            }
                            else if (fNext.vehicleSize != f.vehicleSize) {
                                //log::info("f");
                                waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                            }
                            else if (pc.wasMovingUp != isMovingUp) {   
                                //log::info("g"); 
                                waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                            }
                            else {
                                const bool stateDartSlide = (std::fabs(fNext.y - f.y) <= kYEqualEps);
                                if (stateDartSlide != prevDartSlide) {
                                    prevDartSlide = stateDartSlide;
                                    //log::info("h");
                                    waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false);
                                }
                            }
                        }
                        
                        prevHolding = f.hold;
                        pc.wasMovingUp = isMovingUp;
                        
                        // Teleport visual wacky stuff
                        if (prevTeleported) {
                            trail->reset();
                            trail->resumeStroke();
                            //log::info("i");
                            waveTrailAddPointToPlayer(trail, {ix, iy}, !isP2, false, recordedGoingLeft);
                            prevTeleported = false;
                        }
                        if (std::fabs(fNext.y - f.y) > kWaveTeleportedTolerance) {
                            trail->stopStroke();
                            prevTeleported = true;
                        }
                        
                        // Wave size update
                        const float targetSize = overrideWaveSize ? fNext.vehicleSize : fNext.waveSize;
                        if (trail->m_waveSize != targetSize) {
                            trail->m_waveSize = targetSize;
                        }
                        trail->updateStroke(dt);
                        // trail->setVisible(true);
                        // log::info("Trail update {}", a.serial);

                        /*
                        log::info("trail parent={} isBatch={} vis={} op={} z={} pos=({}, {}) draw={}",
                            fmt::ptr(trail->getParent()),
                            (bool)typeinfo_cast<cocos2d::CCSpriteBatchNode*>(trail->getParent()),
                            trail->isVisible(),
                            (int)trail->getOpacity(),
                            trail->getZOrder(),
                            trail->getPositionX(), trail->getPositionY(),
                            (int)trail->m_drawStreak
                        );*/
                    }
                    // else log::info("Trail not active");
                }
            }
            //log::info("[TIME TAKEN] updateghost wave trail {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  
        };

        // Process P1
        if (!a.p1.empty() && a.primedP1) {
            processPlayer(
                a.p1, a.acc1Time, a.g1, a.last1, a.g1CurMode,
                a.i1, a.d1, a.primedP1, a.eolFrozenP1,
                a.trailactive1, a.previouslyHolding1, a.prevStateDartSlide1, a.prevTeleported1, a.hasWavePointData, a.completed,
                false, px
            );
        }

        // log::info("a.hadDual: {}, a.p2.empty(): {}", a.hadDual, a.p2.empty());

        // Process P2
        if (a.hadDual && !a.p2.empty() && a.primedP2) {
            processPlayer(
                a.p2, a.acc2Time, a.g2, a.last2, a.g2CurMode,
                a.i2, a.d2, a.primedP2, a.eolFrozenP2,
                a.trailactive2, a.previouslyHolding2, a.prevStateDartSlide2, a.prevTeleported2, a.hasWavePointData, a.completed,
                true, px2 + a.ghostOffsetPx
            );
        }
        //log::info("[TIME TAKEN] updateghost {:.8f}", getTimeMs() - tStart2);
    }

cocos2d::ccColor3B Ghosts::chooseColor1(bool isP1) {
        if (colors == ColorMode::PlayerColors) {
            //if (auto gm = GameManager::sharedState()) {
            //    int idx = gm->getPlayerColor();
            //    return gm->colorForIdx(idx);
            //}
            if (isP1) {
                if (m_pl->m_player1) return m_pl->m_player1->m_originalMainColor;
            }
            else if (m_pl->m_player2) return m_pl->m_player2->m_originalMainColor;
            return {255,255,255};
        } else if (colors == ColorMode::Black) {
            return {0, 0, 0};
        }
        return {255,255,255};
    }

cocos2d::ccColor3B Ghosts::chooseColor2(bool isP1) {
        if (colors == ColorMode::PlayerColors) {
            //if (auto gm = GameManager::sharedState()) {
            //    int idx = gm->getPlayerColor2();
            //    return gm->colorForIdx(idx);
            //}
            if (isP1) {
                if (m_pl->m_player1) return m_pl->m_player1->m_originalSecondColor;
            }
            else if (m_pl->m_player2) return m_pl->m_player2->m_originalSecondColor;
            return {255,255,255};
        } else if (colors == ColorMode::Black) {
            return {0, 0, 0};
        }
        return {255,255,255};
    }

cocos2d::ccColor3B Ghosts::chooseGlowColor(bool isP1, bool& hasGlow) {
        hasGlowActive(isP1, hasGlow);
        if (colors == ColorMode::PlayerColors) {
            if (isP1) {
                if (m_pl->m_player1 && m_pl->m_player1->m_hasCustomGlowColor) return m_pl->m_player1->m_glowColor;
            }
            else if (m_pl->m_player2 && m_pl->m_player2->m_hasCustomGlowColor) return m_pl->m_player2->m_glowColor;
            return {255,255,255};
        } else if (colors == ColorMode::Black) {
            return {0, 0, 0};
        }
        return {255,255,255};
    }

PlayerObject* Ghosts::spawnGhostPO(PlayLayer* pl, cocos2d::CCNode* parent, GLubyte opacity) {
        auto gm = GameManager::sharedState();
        int playerId = gm->getPlayerFrame();
        int shipId = gm->getPlayerShip();
        cocos2d::CCLayer* layer = nullptr;
        if (parent) layer = typeinfo_cast<cocos2d::CCLayer*>(parent); 
        if (!layer) layer = pl;
        auto* po = PlayerObject::create(playerId, shipId, pl, layer, false);
        if (!po) return nullptr;
        po->disablePlayerControls();
        po->setVisible(false);
        po->setOpacity(opacity);
        po->setZOrder(5);
        po->m_playEffects = true;
        if (parent) parent->addChild(po);
        else pl->addChild(po);
        return po;
    }

void Ghosts::applyOpacityTree(cocos2d::CCNode* n, GLubyte o) {
        if (!n) return;
        if (auto* rgba = typeinfo_cast<cocos2d::CCRGBAProtocol*>(n)) rgba->setOpacity(o);
        if (auto* kids = n->getChildren()) {
            for (unsigned i = 0; i < kids->count(); ++i) {
                applyOpacityTree(static_cast<cocos2d::CCNode*>(kids->objectAtIndex(i)), o);
            }
        }
    }

GhostBatch& Ghosts::getOrCreateBatch_() {
        // find a batch with room
        for (auto& batch : m_ghostBatches) {
            if (batch.count < kGhostBatchSize) {
                return batch;
            }
        }
        
        // all batches full, create new one
        GhostBatch newBatch;
        newBatch.node = cocos2d::CCNode::create();
        newBatch.node->setPosition({0, 0});
        newBatch.node->ignoreAnchorPointForPosition(true);
        
        newBatch.trailNode = cocos2d::CCNode::create();
        newBatch.trailNode->setPosition({0, 0});
        newBatch.trailNode->ignoreAnchorPointForPosition(true);
        
        // add batch nodes to ghost root
        if (m_ghostRoot) {
            m_ghostRoot->addChild(newBatch.node, 5);
            m_ghostRoot->addChild(newBatch.trailNode, -1);
        }
        
        newBatch.count = 0;
        m_ghostBatches.push_back(newBatch);
        
        return m_ghostBatches.back();
    }

void Ghosts::clearGhostBatches_() {
        //log::info("[Ghosts] Clearing {} ghost batches...", m_ghostBatches.size());
        
        for (auto& batch : m_ghostBatches) {
            if (batch.node && batch.node->getParent()) {
                batch.node->removeFromParent();
            }
            if (batch.trailNode && batch.trailNode->getParent()) {
                batch.trailNode->removeFromParent();
            }
            
            batch.node = nullptr;
            batch.trailNode = nullptr;
            batch.count = 0;
        }
        
        m_ghostBatches.clear();
        m_ghostBatches.shrink_to_fit();
        m_currentBatchIdx = 0;
    }

void Ghosts::rebuildGridThreasholdSet_() {
        m_gridThreshold.clear();

        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;

        for (size_t k = 0; k < m_preloadedIndices.size(); ++k) {
            const size_t attemptIdx = static_cast<size_t>(m_preloadedIndices[k]);
            if (attemptIdx >= attempts.size()) continue;

            Attempt& a = attempts[attemptIdx];

            if (a.practiceAttempt) continue;
            if (a.endPercent < m_GhostsPassedPercentThreshold) continue;
            if (!a.preloaded) continue;

            AttemptSpan sp = makeSpan_(a);
            if (sp.lastX > sp.firstX) {
                m_gridThreshold.insert(static_cast<int>(attemptIdx),
                                    static_cast<float>(sp.firstX),
                                    static_cast<float>(sp.lastX));
            }
        }
    }

void Ghosts::rebuildGridPreloadedSet_() {
        m_grid.clear();

        // throw away any prior preload build state
        m_spans.clear();
        m_spans.resize(attempts.size());

        for (size_t k = 0; k < m_preloadedIndices.size(); ++k) {
            const size_t attemptIdx = (size_t)m_preloadedIndices[k];
            if (attemptIdx >= attempts.size()) continue;

            // Always recompute
            AttemptSpan sp = makeSpan_(attempts[attemptIdx]);
            m_spans[attemptIdx] = sp;

            if (sp.lastX > sp.firstX) {
                // Store attemptIdx so downstream can do attempts[candidate]
                m_grid.insert((int)attemptIdx, (float)sp.firstX, (float)sp.lastX);
            }
        }

        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
    }

void Ghosts::rebuildGrid_() {
        m_grid.clear();

        if (m_spans.size() < attempts.size()) m_spans.resize(attempts.size());

        for (size_t i = 0; i < attempts.size(); ++i) {
            AttemptSpan sp = (i < m_spans.size()) ? m_spans[i] : makeSpan_(attempts[i]);

            if (i >= m_spans.size()) m_spans.resize(i + 1);
            m_spans[i] = sp;

            if (sp.lastX > sp.firstX) m_grid.insert(i, (float)sp.firstX, (float)sp.lastX);
        }

        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
    }

void Ghosts::pushAttempt(Attempt&& a, bool spawnNow) {
        if (spawnNow && !botActive && m_currentDidNoclip && m_recordingBlockedOnNoclip && !isPracticeMode()) return;
        if (!a.p1.empty()) {
            a.startX = a.p1.front().x;
            a.startY = a.p1.front().y;
            a.endX = a.p1.back().x;
        } else if (a.hadDual && !a.p2.empty()) {
            a.startX = a.p2.front().x;
            a.startY = a.p2.front().y;
            a.endX = a.p2.back().x;
        } else {
            a.startX = 0.f;
            a.startY = 0.f;
            a.endX = 0.f;
        }

        if (a.practiceAttempt) {
            a.startPercent = getAccuratePercentForX(a.startX, a.startPercent);
            if (a.completed) a.endPercent = 100.f;
            else a.endPercent = getAccuratePercentForX(a.endX, a.endPercent);
        }

        //geode::log::info("[pushAttempt] serial {} endPercent {}", a.serial, a.endPercent);

        if (isPureRecordingMode_()) {
            a.preloaded = false;
            a.g1 = nullptr;
            a.g2 = nullptr;
            
            if (attempts.size() == attempts.capacity()) invalidateAttemptPointerCaches_();
            attempts.push_back(std::move(a));
            m_serialCacheDirty = true;
            
            while ((int64_t)attempts.size() > maxGhosts) {
                invalidateAttemptPointerCaches_();
                m_loadedAttemptKeys.erase(attemptIdentityKey_(attempts.front().serial,attempts.front().practiceAttempt));
                attempts.erase(attempts.begin());
                m_serialCacheDirty = true;
            }
            
            invalidateAttemptCounts();
            return;
        }

        //buildAccel(a.p1, a.acc1, 64.f);
        //buildAccelTime(a.p1, a.acc1Time);
        //if (a.hadDual) {
            //buildAccel(a.p2, a.acc2, 64.f);
        //    buildAccelTime(a.p2, a.acc2Time);
        //}

        const size_t idx = attempts.size();
        const bool canExtendSerialCache = !m_serialCacheDirty;
        if (attempts.size() == attempts.capacity()) invalidateAttemptPointerCaches_();
        const int addedSerial = a.serial;
        const bool addedPractice = a.practiceAttempt;
        attempts.push_back(std::move(a));
        if (canExtendSerialCache) {
            m_serialCacheDirty = false;
            m_serialToIdx[addedSerial] = idx;
            m_attemptToIdxByIdentity[attemptIdentityKey_(addedSerial, addedPractice)] = idx;
        } else m_serialCacheDirty = true;

        auto& aa = attempts[idx];

        AttemptSpan sp = makeSpan_(aa);
        if (idx >= m_spans.size()) m_spans.resize(idx + 1);
        m_spans[idx] = sp;
        if (sp.lastX > sp.firstX) m_grid.insert(idx, (float)sp.firstX, (float)sp.lastX);

        while ((int64_t)attempts.size() > maxGhosts) {
            invalidateAttemptPointerCaches_();

            auto& oldest = attempts.front();
            if (oldest.g1) oldest.g1->removeFromParentAndCleanup(true);
            if (oldest.g2) oldest.g2->removeFromParentAndCleanup(true);

            m_loadedAttemptKeys.erase(attemptIdentityKey_(oldest.serial,oldest.practiceAttempt));

            attempts.erase(attempts.begin());
            m_spans.erase(m_spans.begin());
            m_serialCacheDirty = true;
            rebuildGrid_();
        }
        
        invalidateAttemptCounts();
    }

void Ghosts::clearAllGhostNodes() {
        for (auto& a : attempts) {
            a.g1 = nullptr;
            a.g2 = nullptr;
            a.preloaded = false;
            a.primedP1 = false;
            a.primedP2 = false;
        }

        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;

        m_ghostPool.resetStats();
    }

void Ghosts::forcePlayersVisible_() {
        if (!m_pl) return;
        if (m_pl->m_player1) {
            m_pl->m_player1->setVisible(true);
            m_pl->m_player1->setOpacity(255);
        } if (m_pl->m_player2) {
            m_pl->m_player2->setVisible(true);
            m_pl->m_player2->setOpacity(255);
        }
    }

void Ghosts::clearHeldInputs_() {
        if (m_pl && m_pl->m_player1) {
            m_pl->m_player1->releaseButton(PlayerButton::Jump);
            m_pl->m_player1->releaseButton(PlayerButton::Left);
            m_pl->m_player1->releaseButton(PlayerButton::Right);
            //m_pl->m_player1->releaseAllButtons();
            //m_pl->m_player1->m_jumpBuffered = false;
            //m_pl->m_player1->m_holdingLeft = false;
            //m_pl->m_player1->m_holdingRight = false;
        }
        if (m_pl && m_pl->m_player2) {
            m_pl->m_player2->releaseButton(PlayerButton::Jump);
            m_pl->m_player2->releaseButton(PlayerButton::Left);
            m_pl->m_player2->releaseButton(PlayerButton::Right);
            //m_pl->m_player2->releaseAllButtons();
            //m_pl->m_player2->m_jumpBuffered = false;
            //m_pl->m_player2->m_holdingLeft = false;
            //m_pl->m_player2->m_holdingRight = false;
        }
        //botPrevHold1 = botPrevHold2 = false;
        //botPrevHoldL1= botPrevHoldL2 = false;
        //botPrevHoldR1 = botPrevHoldR2 = false;
        //p1Hold = p2Hold = false;
        //p1LHold = p2LHold = false;
        //p1RHold = p2RHold = false;
    }

