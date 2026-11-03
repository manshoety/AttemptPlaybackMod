#include "ghost_manager.hpp"
#include <chrono>

void Ghosts::flushPendingSaves_() {
        if (hasCurrentPersistenceTarget_()) {
            //saveCurrentAttemptNow();
            int n = saveNewAttemptsForLevel_(m_levelIDOnAttach);
            //log::info("[Ghosts] flushPendingSaves_: wrote {} attempt(s) for level {}", n, m_levelIDOnAttach);
        }
    }

void Ghosts::setModEnabled(bool on) {
        modEnabled = on;
        Mod::get()->setSavedValue("mod-enabled", on);
        if (!modEnabled) {
            flushPendingSaves_();
            stopReplay();
            clearAllGhostNodes();
            attempts.clear();
            m_loadedAttemptKeys.clear();
            m_loadedLevelID = 0;
            m_serialCacheDirty = true;
            m_spans.clear();
            m_grid.clear();
            m_gridThreshold.clear();
            m_cachedCandidates.clear();
            clearAttemptCatalog_();
            invalidateAttemptPointerCaches_();
            m_preloadedIndices.clear();
            m_primedIndices.clear();
            m_wantToPrimeIndices.clear();
            m_wantToPrimeSet.clear();
            recording = false;
            playback = false;
        } else {
            recording = true;
            playback = false;
        }
        updateGhostVisibility();
    }

void Ghosts::abandonStoredPlayLayerAttachment() {
        clearPlayLayerGhostTextLabel();

        m_pl = nullptr;
        m_gl = nullptr;

        m_ghostRoot = nullptr;
        m_deathMarkerDrawNode = nullptr;
    }

void Ghosts::setPerfBudgetFromUI(int v) {
        m_budgetPerFrame = std::max(1, v);
        m_perfUnlimited = (m_budgetPerFrame >= kPerfBudgetInfiniteSentinel);
        if (m_perfUnlimited) {
            m_maxVisibleGhosts = std::numeric_limits<int>::max() / 4;
        }
    }

void Ghosts::attach(PlayLayer* pl) {
        const auto attachStart = std::chrono::steady_clock::now();
        clearPlayLayerGhostTextLabel();
        setdisablePlayerMove(false);

        abandonStoredPlayLayerAttachment();
        onQuit();

        for (auto& a : attempts) {
            a.g1 = nullptr;
            a.g2 = nullptr;
        }

        m_current.g1 = nullptr;
        m_current.g2 = nullptr;
        m_replayOwnerIndex = -1;
        m_1PreloadedSoDontShowGhosts = false;

        m_pl = pl;
        m_gl = GJBaseGameLayer::get();

        m_isTwoPlayer =
            m_pl &&
            m_pl->m_levelSettings &&
            m_pl->m_levelSettings->m_twoPlayerMode;

        m_ghostRoot = cocos2d::CCNode::create();
        m_ghostRoot->ignoreAnchorPointForPosition(true);
        m_ghostRoot->setPosition({0, 0});

        cocos2d::CCNode* objectLayer =
            (m_pl && m_pl->m_player1)
                ? m_pl->m_player1->getParent()
                : m_pl;

        if (objectLayer) {
            objectLayer->addChild(m_ghostRoot, 5);
        }

        ensureDeathMarkerLayer_();
        redrawDeathMarkers_();

        m_ghostPool.initialize(pl, m_ghostRoot, 1);

        m_fmodEngine = FMODAudioEngine::sharedEngine();

        clearGhostBatches_();
        getOrCreateBatch_();
        updateMirrorVisuals_();

        customLastWrite = {};
        m_lastCustomSfxRefreshTick = 0;

        if (m_useCustomExplodeSounds) refreshAndPreloadCustomExplodeSfx_(true);
        else customSfx.clear();

        clearAllGhostNodes();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();

        m_grid.clear();
        m_gridThreshold.clear();
        m_spans.clear();
        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
        
        forceSetPosP1.x = 0.f;
        forceSetPosP1.y = 105.f;
        forceSetPosP2.x = 0.f;
        forceSetPosP2.y = 105.f;

        auto* mod = Mod::get();
        syncGhostUiSettingsFromSaved_();

        int numRealPlayerObjects = getSettingIntOrDefault_(mod, "real-player-objects", 1000);

        m_playerObjectPool.init(&m_ghostPool, numRealPlayerObjects);

        m_currentOwner = nullptr;
        m_playbackArmed = false;
        botActive = false;
        m_runUsedBot = false;
        m_didInitialWarp = false;
        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;
        m_lastGhostTickFrame = m_frameCounter;

        practiceAttemptReplayIndex.clear();

        m_checkpointTopId = -1;
        m_nextCheckpointId = 1;
        m_checkpointUndoStack.clear();
        m_nextAttemptSerial = 1;
        m_frozenCheckpointTopId = -1;

        m_tickId = 0;
        m_lastWorkTickId = 0;

        if (!modEnabled || (m_pl->m_isPlatformer && !m_allow_platformer)) {
            clearAllGhostNodes();
            attempts.clear();
            m_loadedAttemptKeys.clear();
            clearAttemptCatalog_();
            invalidateAttemptPointerCaches_();
            recording = false;
            playback = false;
            //log::info("[Ghosts] attach() mod not enabled");
            return;
        }

        recordInPractice = getSettingBoolOrDefault_(mod, "record-in-practice", true);
        m_accurateHighTPSReplay = getSettingBoolOrDefault_(mod, "accurate-high-tps-replay", false);
        // showWhilePlaying = getSettingBoolOrDefault_(mod, "show-while-playing", false);
        showWhilePlaying = false;
        // onlyBestGhost = getSettingBoolOrDefault_(mod, "only-best-ghost", false);
        onlyBestGhost = false;
        // m_ghostsExplode = getSettingBoolOrDefault_(mod, "ghosts-explode", false);
        // m_ghostsExplode = true;
        m_ghostsExplode = mod->getSavedValue<bool>("ghosts-explode");
        m_ghostsExplodeSFX = mod->getSavedValue<bool>("ghosts-explode-sfx");
        m_useCustomExplodeSounds = mod->getSavedValue<bool>("custom-ghosts-explode-sfx");
        recordingBlocked = mod->getSavedValue<bool>("recording-blocked");
        m_recordingBlockedOnNoclip = mod->getSavedValue<bool>("recording-blocked-on-noclip");

        //m_useInterpolation = mod->getSavedValue<bool>("interp-enabled");
        m_useInterpolation = false;
        Mod::get()->setSavedValue("interp-enabled", false);
        Mod::get()->setSettingValue("interp-enabled", false);
        m_randomIcons = mod->getSavedValue<bool>("randomIcons-enabled");
        m_pauseGameAtTheEndOfReplayBot = mod->getSavedValue<bool>("ReplayPreventCompletion-enabled");


        // if (!mod->hasSavedValue("interp-enabled")) m_useInterpolation = true;
        if (!mod->hasSavedValue("ghosts-explode")) m_ghostsExplode = false;
        if (!mod->hasSavedValue("ghosts-explode-sfx")) m_ghostsExplodeSFX = true;
        if (!mod->hasSavedValue("custom-ghosts-explode-sfx")) m_useCustomExplodeSounds = false;
        if (!mod->hasSavedValue("randomIcons-enabled")) m_randomIcons = false;
        if (!mod->hasSavedValue("recording-blocked")) recordingBlocked = false;
        if (!mod->hasSavedValue("recording-blocked-on-noclip")) m_recordingBlockedOnNoclip = false;
        if (!mod->hasSavedValue("ReplayPreventCompletion-enabled")) m_pauseGameAtTheEndOfReplayBot = false;
        m_pauseGameAtTheEndOfReplayBot = false;

        if (!mod->hasSavedValue("ghost-distance")) mod->setSavedValue("ghost-distance", 45.f);

        if (mod->hasSavedValue("ghost-distance")) {
            int distance = static_cast<int>(mod->getSavedValue<int64_t>("ghost-distance"));
            m_randomDistPx = std::clamp(distance, 0, 400);
        }
        else m_randomDistPx = 45.f;


        int64_t op = mod->hasSavedValue("ghost-opacity");
        if (!mod->hasSavedValue("ghost-opacity")) op = 255;
        //setOpacity(clampU8_(op));
        setOpacityVar(clampU8_(op));

        
        if (!mod->hasSavedValue("ghost-colors")) mod->setSavedValue("ghost-colors", std::string("PlayerColors"));
        std::string colorMode = mod->getSavedValue<std::string>("ghost-colors");

        std::string colorModeLower = colorMode;
        std::transform(colorModeLower.begin(), colorModeLower.end(), colorModeLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (colorModeLower == "playercolors") {
            colors = ColorMode::PlayerColors;
            if (colorMode != "PlayerColors") mod->setSavedValue("ghost-colors", std::string("PlayerColors"));
        } else if (colorModeLower == "white") {
            colors = ColorMode::White;
            if (colorMode != "White") mod->setSavedValue("ghost-colors", std::string("White"));
        } else if (colorModeLower == "black") {
            colors = ColorMode::Black;
            if (colorMode != "Black") mod->setSavedValue("ghost-colors", std::string("Black"));
        } else {
            colors = ColorMode::Random;
            if (colorMode != "Random") mod->setSavedValue("ghost-colors", std::string("Random"));
        }

        m_autosaveEnabled = getSettingBoolOrDefault_(mod, "autosave-enabled", true);
        m_autosaveMinutes = getSettingIntOrDefault_(mod, "autosave-minutes", 5);
        m_autosaveAccum = 0.f;

        m_allow_platformer = getSettingBoolOrDefault_(mod, "allow-platformer", true);

        //m_CBFwaveFixEnabled = getSettingBoolOrDefault_(mod, "cbf-wave-fix", false);

        
        // Icon colors:
        if (!mod->hasSavedValue(kGhostRandomColorsMaskKey)) mod->setSavedValue(kGhostRandomColorsMaskKey, std::string(kDefaultRandomColorMask));

        /*
        const int kUnlimitedSliderMax = 1000;
        const int budgetSlider = getSettingIntOrDefault_(mod, "perf-budget-per-frame", 1000);
        if (budgetSlider >= kUnlimitedSliderMax) {
            m_perfUnlimited  = true;
            m_budgetPerFrame = kPerfBudgetInfiniteSentinel;
        } else {
            m_perfUnlimited  = false;
            m_budgetPerFrame = std::max(1, budgetSlider);
        }*/
        m_perfUnlimited = true;
        m_budgetPerFrame = 999999999;

        m_preloadQ.clear();
        m_isPreloaded.clear();

        waveTrailGhost = getSettingBoolOrDefault_(mod, "wave-trail-ghost", true);

        //int fps = std::clamp(getSettingIntOrDefault_(mod, "ghost-update-fps", 480), 1, 480);
        //setGhostUpdateFPS(fps);
        setGhostUpdateFPS(480);

        //maxGhosts = std::max(1, getSettingIntOrDefault_(mod, "max-ghosts", INT64_MAX));
        // log::info("[Ghosts] max stored attempts cap = {}", maxGhosts);
        maxGhosts = INT32_MAX;

        if (onlyBestGhost) showBestGhostOnly();
        else showAllGhosts();

        //log::info("[Ghosts] attach() right before thingy");

        recording = true;
        playback = false;

        p1Hold = p2Hold = false;
        p1LHold = p2LHold = false;
        p1RHold = p2RHold = false;
        m_frameCounter = 0;
        m_currentAttemptStart = 0;
        m_playbackStartTick = 0;
        p1WavePointThisFrame = false;
        p2WavePointThisFrame = false;

        m_prevPractice = (m_pl && m_pl->m_isPracticeMode);
        m_pendingRearm = false;

        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;

        m_replayIdx1 = m_replayIdx2 = 0;
        m_replayOwnerSerial = -1;
        m_replayAttempt = nullptr;
        m_prevBotPx = 0.f;
        m_nullOwnerSpanSkips = 0;
        m_allowWaveHook = true;

        //log::info("[Ghosts] attach: botActive={}, recording={}, playback={}", botActive, recording, playback);

        if (m_pl && m_pl->m_level) {
            m_levelIDOnAttach = m_pl->m_level->m_levelID;
            refreshLevelName(m_levelIDOnAttach);

            LoadFilter f;
            loadFromDisk_(m_levelIDOnAttach, f);
        }

        startNewAttempt();

        m_lastRecordedX = 0.f;
        updateGhostVisibility();

        clearHeldInputs_();
        forcePlayersVisible_();
        invalidateAttemptCounts();

        // getPlaybackLimitVisibleGhostsEnabled();
        m_limitVisibleGhosts = false;
        // getPlaybackMaxVisibleGhosts();
        m_maxVisibleGhosts = INT_MAX;
        // Making this default off when opening a level since people will absolutely forget its on and complain that the mod isn't working corrrectly
        setPlaybackOnlyPastPercentEnabled(false);
        getPlaybackOnlyPastPercentEnabled();
        getPlaybackOnlyPastPercentThreshold();
        buildGhostThatPassedPercentSerialList();
        getPreloadSortMode();
        setPreloadSortMode(m_preloadSortMode);
        log::info(
            "[APX lifecycle] attach total={} ms catalog={} resident={}",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - attachStart).count(),
            m_attemptCatalog.size(),
            attempts.size()
        );
    }

void Ghosts::initBotAfterReset_(bool naturalPracticeStart) {
        if (!botActive || !m_pl || !m_pl->m_player1) return;

        m_replayIdx1 = m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;

        const Attempt* owner = nullptr;
        if (naturalPracticeStart && m_replayKind == ReplayKind::PracticeComposite) {
            auto replaySegs = m_checkpointMgr.getReplaySequence();
            m_baseTime = replaySegs.empty() ? getStartTime() : replaySegs.front().absStart();
        }
        else m_baseTime = getStartTime();
        
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                owner = ensureAttemptLoadedBySerialAndPractice_(m_replayOwnerSerial, false, false);
            }

            if (!owner && m_compOwnerIdx < attempts.size() &&
                !attempts[m_compOwnerIdx].practiceAttempt) {
                owner = &attempts[m_compOwnerIdx];
                m_replayOwnerSerial = owner->serial;
            }

            m_replayAttempt = nullptr;
        } else {
            int serial = m_checkpointMgr.findOwnerSerialForTime(m_baseTime);
            if (serial > 0) {
                owner = ensurePracticeReplayOwnerLoaded_(serial, "reset-owner");
                if (owner) m_replayOwnerSerial = serial;
            }
            m_replayAttempt = nullptr;
            rebuildRouteOwnerCache_();
        }

        if (!owner || owner->p1.empty()) {
            log::warn("[Bot] initBotAfterReset: no owner data (owner={}, p1.size={})", 
                (bool)owner, owner ? owner->p1.size() : 0);
            return;
        }

        // I'll try getting seed stuff working later
        //if (owner->seed != 0) {
        //    seedutils::applyAttemptSeed(owner->seed, m_pl);
            //log::info("[Seed] Applied seed {} from owner attempt {} during init", owner->seed, owner->serial);
        //}

        m_currentOwner = owner;
        m_currentSessionTime = m_baseTime;
        m_prevSessionTime = m_baseTime;
        seekReplayCursorsToTimeExact_(*owner, m_currentSessionTime);

        m_lastEmitIdx1 = (!owner->p1.empty() && m_replayIdx1 > 0)
            ? m_replayIdx1 - 1
            : kNoEmitIdx;
        m_lastEmitIdx2 = (owner->hadDual && !owner->p2.empty() && m_replayIdx2 > 0)
            ? m_replayIdx2 - 1
            : kNoEmitIdx;
        m_lastPoseIdx1 = m_lastEmitIdx1;
        m_lastPoseIdx2 = m_lastEmitIdx2;

        const Frame& f1 = owner->p1[m_replayIdx1];
        if (!naturalPracticeStart) {
            applyPoseHard(m_pl->m_player1, f1, m_pl);

            if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) {
                const Frame& f2 = owner->p2[m_replayIdx2];
                applyPoseHard(m_pl->m_player2, f2, m_pl);
            }
         }

        m_prevBotPx = f1.x;
        m_didInitialWarp = true;

        m_justStartedBot = false;
    }

void Ghosts::onDeath() {
        if (!m_pl) return;
        if (m_justDied) return;
        m_justDied = true;

        if (m_pl->m_isPracticeMode) {
            if (recording && !recordingBlocked && recordInPractice && !m_current.p1.empty()) {
                double startTime = m_current.baseTimeOffset;
                float startX = m_current.p1.front().x;
                float endX = m_current.p1.back().x;

                //log::info("x: {}, y: {}", m_current.p1.front().x, m_current.p1.front().y);

                
                //log::info("[onDeath] m_currentSessionTime: {}, playlayerTime: {}", m_currentSessionTime, m_pl->m_attemptTime);

                
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
            }

            if (recording && !recordingBlocked && recordInPractice && !m_current.p1.empty() && !block_attempt_push_on_recording_start) {
                pushAttempt(std::move(m_current));
            }

            return;
        }

        if (recording && !recordingBlocked && m_current.p1.size() > 1 && !block_attempt_push_on_recording_start) {
            pushAttempt(std::move(m_current));
        }
    }

void Ghosts::onReset() {
        m_justDied = false;
        m_playingEndAnimation = false;
        if (!botActive) m_runUsedBot = false;

        if (m_forceFreshRecordingAfterDelete) {
            m_forceFreshRecordingAfterDelete = false;

            recording = true;
            recordingBlocked = false;
            block_attempt_push_on_recording_start = false;

            m_current = Attempt{};
        }
        else if (block_attempt_push_on_recording_start) return;
        forceSetPosP1.x = 0.f;
        forceSetPosP1.y = 105.f;
        forceSetPosP2.x = 0.f;
        forceSetPosP2.y = 105.f;
        m_noP2Check = false;
        m_haveLastSnappedWavePointP1 = false;
        m_haveLastSnappedWavePointP2 = false;
        m_wavePointLastFrameP1 = false;
        m_wavePointLastFrameP2 = false;
        m_explicitWavePointAddedPrevPoseP1 = false;
        m_explicitWavePointAddedPrevPoseP2 = false;
        m_suppressNextExplicitWavePointP1 = false;
        m_suppressNextExplicitWavePointP2 = false;

        // log::info("onReset");

        //log::info("player x: {}, y: {}", m_pl->m_player1->m_positionX, m_pl->m_player1->m_positionY);
        //log::info("playerV2 x: {}", playerX_());

        if (m_pl && m_pl->m_isPracticeMode && recording && !recordingBlocked) {
            // Check for start position switch (end recording if so)
            // PLAYER X IS WEIRD AND IS THE DEFAULT START NOT THE CORRECT PLAYER START (maybe delay 1 frame?)
            //Checkpoint* currentCheckpoint = m_checkpointMgr.getCurrentCheckpoint();
            //if (currentCheckpoint && ccpDistance({currentCheckpoint->x, currentCheckpoint->y}, {(float)m_pl->m_player1->m_positionX, (float)m_pl->m_player1->m_positionY}) > kReplayStartTolerance) {
            //    log::info("AAAAAAAA");
            //    log::info("player x: {}, y: {}", m_pl->m_player1->m_positionX, m_pl->m_player1->m_positionY);
            //    log::info("checkpoint x: {}, y: {}", currentCheckpoint->x, currentCheckpoint->y);
            //    m_checkpointMgr.onExitEarly();
            //    togglePracticeMode(false);
            //    if (!isRecording()) toggleRecording();
            //}
            //else 
            if (!m_current.p1.empty()) {
                // Only runs when we don't use OnDeath
                double startTime = m_current.baseTimeOffset;
                float startX = m_current.p1.front().x;
                float endX = m_current.p1.back().x;
                float endtime = m_current.p1.back().t - m_current.baseTimeOffset;

                //log::info("[onReset] m_currentSessionTime: {}, playlayerTime: {}", m_currentSessionTime, m_pl->m_attemptTime);
                    
                m_checkpointMgr.overwriteSegmentForAttempt(
                    m_current.serial,
                    startTime,
                    0,
                    endtime,
                    startX,
                    endX,
                    m_current.p1.size(),
                    m_current.p2.size()
                );

                if (recordInPractice) {
                    pushAttempt(std::move(m_current));
                }
                else {
                    m_current = Attempt{};
                }
            }
        }
        
        // Will still push on the attempt you switched start pos on
        if (recording && !recordingBlocked && m_current.p1.size() > 1) {
            pushAttempt(std::move(m_current));
        }

        startNewAttempt();

        if (m_pendingPracticeStartPosInit && m_replayKind == ReplayKind::PracticeComposite) {
            auto replaySegs = m_checkpointMgr.getReplaySequence();
            m_currentSessionTime = replaySegs.empty() ? getStartTime() : replaySegs.front().absStart();
        }
        else m_currentSessionTime = getStartTime();
        m_prevSessionTime = m_currentSessionTime;
        m_baseTime = m_currentSessionTime;

        m_frameCounter = 0;
        m_lastGhostTickFrame = 0;
        m_lastRecordedX = 0.f;
        m_ghostUpdateAccum = 0.f;
        m_freezePlayerXAtEnd = false;
        if (!(botActive && m_replayKind == ReplayKind::PracticeComposite)) {
            m_freezePlayerX = 0.f;
            m_freezePlayerY = 0.f;
        }
        m_playbackArmed = false;
        m_pendingRearm = false;
        m_activeOwnerSerial = -1;
        m_mirrorVisualWarmupFrames = 2;
        m_px = 0.f;
        m_px2 = 0.f;
        m_prevpx2 = 0.f;

        forcePlayersVisible_();

        if (isPureRecordingMode_()) {
            return;
        }

        if (!botActive) {
            playback = showWhilePlaying;
            m_playbackArmed = playback;
        }

        //if (m_useCheckpointsRoute && m_replayKind == ReplayKind::PracticeComposite && routeIsComposite_()) {
        //    if (botActive || showWhilePlaying) {
        //        //buildCompositePracticePath();
        //        rebuildRouteOwnerCache_();
        //    }
        //    if (botActive) tagCompositeOwnersHidden_(true);
        //}

        if (botActive) {
            m_playerPrevTeleportedP1 = false;
            m_playerPrevTeleportedP2 = false;
            m_p2JustSpawned = false;
            m_prevHadP2 = false;
            if (m_pendingPracticeStartPosInit) {
                m_justStartedBot = true;
                m_replayIdx1 = m_replayIdx2 = 0;
                m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
                m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;
                m_replayOwnerSerial = -1;
            } // Check for start position switch
            else if (ccpDistance(m_currentReplayStartPos, {(float)m_pl->m_player1->m_positionX, (float)m_pl->m_player1->m_positionY}) > kReplayStartTolerance) {
                if (!isRecording()) toggleRecording();
                setActiveGhostsInvisible();
            }
            else {
                m_justStartedBot = true;
                m_replayIdx1 = m_replayIdx2 = 0;
                m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
                m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;
                if (m_replayKind != ReplayKind::BestSingle) {
                    m_replayOwnerSerial = -1;
                }

                initBotAfterReset_();
            }
            updateGhostTextOnDeath(true);
        }

        if (botActive || showWhilePlaying) {
            invalidatePrimedPlayerObjectRefs();
            InitialGamemodeSet();
        }
        
        if (m_ghostRoot) {
            m_ghostRoot->setScaleX(1.f);
            m_ghostRoot->setPositionX(0.f);
        }

        resetPlayerMirrorVisualOffsets_();
        resetMirrorVisualState_();
        updateMirrorVisuals_();

        m_movementDirectionP1 = MovementDirection::Flat;
        m_movementDirectionP2 = MovementDirection::Flat;

        // log::info("inUseCount: {}, activeAttempts: {}, currentlyHiddenSerials: {}", m_playerObjectPool.inUseCount(), m_playerObjectPool.getActiveAttempts().size(), m_currentlyHiddenSerials.size());
    }

void Ghosts::invalidatePrimedPlayerObjectRefs() {
        for (const auto& use : m_playerObjectPool.getActiveAttempts()) {
            if (use.attemptIdx >= attempts.size()) continue;

            Attempt& a = attempts[use.attemptIdx];

            if (use.hasP1) {
                if (a.g1) {
                    a.setP1Visible(false, true);
                    if (a.g1->m_waveTrail) a.g1->m_waveTrail->setVisible(false);
                }
                a.g1 = nullptr;
                a.g1Idx = -1;
                a.primedP1 = false;
            }

            if (use.hasP2) {
                if (a.g2) {
                    a.setP2Visible(false, true);
                    if (a.g2->m_waveTrail) a.g2->m_waveTrail->setVisible(false);
                }
                a.g2 = nullptr;
                a.g2Idx = -1;
                a.primedP2 = false;
            }
        }

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                attempts[idx].resetPlayback();
            }
        }

        if (m_current.g1) {
            m_current.setP1Visible(false, true);
            if (m_current.g1->m_waveTrail) m_current.g1->m_waveTrail->setVisible(false);
        }
        if (m_current.g2) {
            m_current.setP2Visible(false, true);
            if (m_current.g2->m_waveTrail) m_current.g2->m_waveTrail->setVisible(false);
        }
        m_current.g1 = nullptr;
        m_current.g2 = nullptr;
        m_current.g1Idx = -1;
        m_current.g2Idx = -1;

        m_primedIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeIndices.clear();
        m_wantToPrimeSet.clear();
        if (!routeOwnersActive_()) {
            m_currentlyHiddenSerials.clear();
        }
    }

void Ghosts::setActiveGhostsInvisible() {
        for (size_t idx : m_primedIndices) {
            if (idx >= attempts.size()) continue;
            Attempt& a = attempts[idx];
            
            a.resetPlayback();
            if (a.g1) a.setP1Visible(false, true);
            if (a.g2) a.setP2Visible(false, true);
        }
        m_primedIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeIndices.clear();
        m_wantToPrimeSet.clear();
    }

void Ghosts::onQuit() {
        const auto quitStart = std::chrono::steady_clock::now();
        const size_t attemptsAtQuit = attempts.size();
        m_is_quitting = true;
        g_disableUpdate = false;
        m_justDied = false;

        clearReplayStartPosOverride_();
        clearPracticeReplaySessionSelection();
        clearPlayLayerGhostTextLabel();

        m_preloadActive = false;
        m_preloadTarget = 0;
        m_preloadLoaded = 0;
        m_preloadCursor = 0;
        m_preloadOrder.clear();
        m_initialAttemptsToSet.clear();
        m_preloadQ.clear();
        m_isPreloaded.clear();

        death_sound_preloaded = false;
        m_allowWaveHook = false;

        m_current = Attempt{};
        
        flushPendingSaves_();
        const auto quitAfterSave = std::chrono::steady_clock::now();
        
        botActive = false;
        m_runUsedBot = false;
        playback = false;
        recording = false;
        m_hasWavePointData = false;
        m_filterByStartPosition = false;
        applyBotSafety_(false);

        m_playerPrevTeleportedP1 = false;
        m_playerPrevTeleportedP2 = false;
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;
        
        //log::info("Nulling ghost references in {} attempts...", attempts.size());
        for (auto& a : attempts) {
            a.g1 = nullptr;
            a.g2 = nullptr;
            a.preloaded = false;
            a.primedP1 = false;
            a.primedP2 = false;
        }
        m_current.g1 = nullptr;
        m_current.g2 = nullptr;

        //log::info("Cleaning up ghost pool...");
        m_ghostPool.cleanup();

        //log::info("Clearing ghost batches...");
        clearGhostBatches_();
        resetPlayerMirrorVisualOffsets_();
        resetMirrorVisualState_();
        m_mirrorVisualWarmupFrames = 0;

        if (m_deathMarkerDrawNode) {
            if (m_deathMarkerDrawNode->getParent()) m_deathMarkerDrawNode->removeFromParent();
            m_deathMarkerDrawNode = nullptr;
        }
        m_deathMarkerPositions.clear();
        
        if (m_ghostRoot) {
            m_ghostRoot->setScaleX(1.f);
            m_ghostRoot->setPositionX(0.f);
            //log::info("Removing ghost root from scene...");
            if (m_ghostRoot->getParent()) {
                m_ghostRoot->removeFromParent();
            }
            m_ghostRoot = nullptr;
        }

        const auto quitAfterVisuals = std::chrono::steady_clock::now();
        attempts.clear();
        invalidateAttemptPointerCaches_();
        const auto quitAfterAttempts = std::chrono::steady_clock::now();

        m_activeOwnerSerial = -1;
        m_didInitialWarp = false;
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;
        m_pl = nullptr;
        m_gl = nullptr;
        m_currentOwner = nullptr;
        recording = true;
        playback = false;
        onlyBestGhost = false;
        m_frameCounter = 0;
        m_currentAttemptStart = 0;
        m_playbackStartTick = 0;
        p1Hold = p2Hold = false;
        p1LHold = p2LHold = false;
        p1RHold = p2RHold = false;
        p1WavePointThisFrame = false;
        p2WavePointThisFrame = false;
        m_lastWinSerial = -1;
        m_filterByStartPosition = false;
        m_currentReplayStartPos.x = 0.f;
        m_currentReplayStartPos.y = 0.f;

        practiceAttemptReplayIndex.clear();

        m_prevPractice = false;
        m_pendingRearm = false;

        m_checkpointTopId = -1;
        m_nextCheckpointId = 1;
        m_checkpointUndoStack.clear();

        m_frozenCheckpointTopId = -1;

        m_replayIdx1 = m_replayIdx2 = 0;
        m_replayOwnerSerial = -1;
        m_replayAttempt = nullptr;
        
        m_prevBotPx = 0.f;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;
        m_lastGhostTickFrame = 0;

        m_ghostUpdateAccum = 0.f;
        m_loadedAttemptKeys.clear();
        m_loadedLevelID = 0;
        clearAttemptCatalog_();
        m_practiceCompositeOwnerSerial = -1;
        m_activeOwnerSerial = -1;
        m_activeRoute = ActiveRoute::CompositeCheckpoint;
        m_attemptsPreloadedTotal = 0;

        m_is_quitting = false;

        m_playerObjectPool.clearAfterGhostPoolCleared();
        setdisablePlayerMove(false);
        m_blockAttemptCount = false;

        const auto quitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - quitStart).count();
        log::info("[APX lifecycle] onQuit cleaned {} attempts in {} ms", attemptsAtQuit, quitMs);
        log::info(
            "[APX lifecycle] phases save={} ms visuals={} ms attempts={} ms remainder={} ms; lazyLoads={} failures={} decode={} ms preloadPrepare={} ms",
            std::chrono::duration_cast<std::chrono::milliseconds>(quitAfterSave - quitStart).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(quitAfterVisuals - quitAfterSave).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(quitAfterAttempts - quitAfterVisuals).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - quitAfterAttempts).count(),
            m_lazyLoadedAttempts,
            m_lazyLoadFailures,
            m_lazyDecodeTimeUs / 1000,
            m_preloadPrepareTimeUs / 1000
        );
        m_lazyLoadedAttempts = 0;
        m_lazyLoadFailures = 0;
        m_lazyDecodeTimeUs = 0;
        m_preloadPrepareTimeUs = 0;
    }

void Ghosts::onComplete() {
        m_justDied = true;

        if (!m_current.p1.empty()) {
            const int serial = m_current.serial;
            const float endX = m_current.p1.back().x;
            const float endY = m_current.p1.back().y;
            const size_t p1Count = m_current.p1.size();
            const size_t p2Count = m_current.p2.size();

            if (m_pl && m_pl->m_isPracticeMode && recording && !recordingBlocked && recordInPractice) {
                double startTime = m_current.baseTimeOffset;
                float startX = m_current.p1.front().x;

                //log::info("[onComplete] m_currentSessionTime: {}, playlayerTime: {}", m_currentSessionTime, m_pl->m_attemptTime);
                
                m_checkpointMgr.overwriteSegmentForAttempt(
                    serial,
                    m_current.baseTimeOffset,
                    0,
                    m_current.p1.back().t - m_current.baseTimeOffset,
                    startX,
                    endX,
                    p1Count,
                    p2Count
                );
            }

            m_checkpointMgr.onAttemptCompleted();

            m_current.completed = true;
            m_lastWinSerial = serial;
            m_current.endPercent = 100.f;
            pushAttempt(std::move(m_current));
        }
    }

void Ghosts::saveCurrentAttemptNow() {
        if (botActive || playback) return;
        if (!recording || recordingBlocked) return;

        // Don't save practice attempts unless explicitly enabled.
        if (m_pl && m_pl->m_isPracticeMode && !recordInPractice) {
            return;
        }

        if (m_current.p1.size() > 2) {
            //geode::log::warn("PUSH saveCurrentAttemptNow");
            pushAttempt(std::move(m_current));
            startNewAttempt();
        }
    }

void Ghosts::toggleRecording() {
        bool wasOn = recording;
        recording = !recording;

        if (playback) stopPlayback();
        if (botActive) stopReplay();

        if (!recording && wasOn) {
            saveCurrentAttemptNow();
            flushPendingSaves_();
        }

        if (recording) {
            if (m_pl) {
                restartLevel();
            }
        }

        updateGhostVisibility();
    }

void Ghosts::startPlayback()   {
        if (botActive) stopReplay();
        playback = true;
        m_playbackArmed = true;
        m_playbackStartTick = m_frameCounter;
        recording = false;
        updateGhostTextOnDeath(true);
        updateGhostVisibility(); 
        for (auto& a : attempts) a.resetPlayback();
    }

void Ghosts::stopPlayback() {
        playback = false;
        clearDeathMarkers_();
        updateGhostVisibility();
        resetPlayerMirrorVisualOffsets_();
        resetMirrorVisualState_();
    }

void Ghosts::clearGhosts() {
        clearAllGhostNodes();
        clearDeathMarkers_();
        attempts.clear();
        m_loadedAttemptKeys.clear();
        m_serialCacheDirty = true;
        m_spans.clear();
        m_grid.clear();
        m_gridThreshold.clear();
        m_cachedCandidates.clear();
        invalidateAttemptPointerCaches_();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        onlyBestGhost = false;
        updateGhostVisibility();
        practiceAttemptReplayIndex.clear();
    }

bool Ghosts::matchesStartPositionFilter_(const Attempt& a) const {
        if (!m_filterByStartPosition || a.m_isPlatformer || a.practiceAttempt) return true;
        if (a.p1.empty()) return false;

        // L bozo platformer mode this is being wacky for initial teleports in levels due to y offset
        
        float attemptStartX = a.p1.front().x;
        // float attemptStartY = a.startY;

        //float dx = m_currentReplayStartPos.x - attemptStartX;
        //float dy = m_currentReplayStartPos.y - attemptStartY;
        
        //return ((dx * dx + dy * dy) < kTolSq);

        return std::fabs(attemptStartX - m_currentReplayStartPos.x) <= kReplayStartTolerance;
    }

