// ghost_manager.hpp
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/DefaultInclude.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/loader/Dirs.hpp>

#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayerCheckpoint.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/HardStreak.hpp>

#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/cocoa/CCArray.h>
#include <Geode/cocos/misc_nodes/CCMotionStreak.h>

#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/ui/General.hpp>
#include <Geode/ui/Popup.hpp>

#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <random>
#include <deque>
#include <limits>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>

#include "../core/dashing_args.hpp"
#include "../core/apx_io.hpp"
#include "../core/apx_format.hpp"
#include "./globals.hpp"
#include <legowiifun.cheat_api/include/cheatAPI.hpp>
#include "../core/random_color_ids.hpp"
#include "../core/types.hpp"
#include "../core/ghost_pool.hpp"
#include "../core/sfx_handling.hpp"
#include "../core/checkpoint_manager.hpp"
#include "../core/player_object_pool.hpp"
#include "../core/preload_memory_estimator.hpp"
// #include "../core/seed_utils.hpp"

// Current visual bugs:
// Wave trail wackyness when turning off (only show past percent) when replaying with waves at the start

// Features I'll (maybe) add:
// Ability to only replay one practice session or multiple (maybe even name them and save names as a different file). This would be it's own UI. Also have ability to delete certain sessions and maybe even export and import sessions (practice and normal). It would be cool to be able to export your best attempt, and be able to import the best attempt of several people and replay the at the same time, and have the icons maybe exported and imported too? A lot of work but would be cool to have eventually.
// Simple first addition would be to allow replay of all start positions (it would just load all normal mode attempts and skip the start pos check)
// Platformer mode isn't implimented when there are checkpoints yet (practice mode works but not normal)
// Ability to load and export attempt files so you can send them to people
// Add back the limit ghost updates per frame feature for low end devices
// Once dead, allow new allocation to remove the ghost early
// Make glow work on ghosts
// Make the mod work with mods that add extra icons
// Massively optimize Attempts class to be way smaller (lot of uneeded bools from the early versions of this mod)
// Camera pans when teleporting large y (sort of partially fixed by speeding up the camera when teleporting, but still buggy)

// Fix:
// wave trail color when reverse color selected
// Player frame when dual has separate icons (I did the colors and glow already)
// If wave trail larger maybe set the ghost trail larger, but be careful of weird values that aren't reflecting the actual wave trail people see on screen
// Wave trail sometimes vanishes for some reason on real player, so ensure that is visible (player 2 teleported which stopped and started p2 wave trail, then p2 teleported and had no wave trail)
// Ensure this doesn't conflict with the wave trail draw fix mod on Geode
// Player rotation is set by the game after I set it, so slightly wrong (ensure overriding this doesn't break follow player rotation objects)
// On every ghost death it turns on and off auto-deafen

using namespace geode::prelude;

#if defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#else
  #define FORCE_INLINE inline __attribute__((always_inline))
#endif

#if defined(__GNUC__) || defined(__clang__)
  #define LIKELY(x)   (__builtin_expect(!!(x), 1))
  #define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
  #define LIKELY(x)   (x)
  #define UNLIKELY(x) (x)
#endif

// ATTENTION GEODE DEV: To the Geode dev looking through this file, I'm sorry this file is so long. I did not intend to tourment you. Have fun!

static FORCE_INLINE IconType currentMode(PlayerObject* p, bool isPlatformer) {
    // log::info("isShip: {} isPlatformer: {}", p->m_isShip, isPlatformer);
    if (!p) return IconType::Cube;
    if (p->m_isShip) {
        if (isPlatformer) return IconType::Jetpack;
        return IconType::Ship;
    }
    if (p->m_isBird) return IconType::Ufo;
    if (p->m_isBall) return IconType::Ball;
    if (p->m_isDart) return IconType::Wave;
    if (p->m_isRobot) return IconType::Robot;
    if (p->m_isSpider) return IconType::Spider;
    if (p->m_isSwing) return IconType::Swing;
    return IconType::Cube;
}

static FORCE_INLINE int frameForIcon(IconType t) {
    auto gm = GameManager::sharedState();
    if (!gm) return 1;
    switch (t) {
        case IconType::Cube: return gm->getPlayerFrame();
        case IconType::Ship: return gm->getPlayerShip();
        case IconType::Ball: return gm->getPlayerBall();
        case IconType::Ufo: return gm->getPlayerBird();
        case IconType::Wave: return gm->getPlayerDart();
        case IconType::Robot: return gm->getPlayerRobot();
        case IconType::Spider: return gm->getPlayerSpider();
        case IconType::Swing: return gm->getPlayerSwing();
        case IconType::Jetpack: return gm->getPlayerJetpack();
        default: return gm->getPlayerFrame();
    }
}

static int randomIconFrame(IconType t, uint32_t seed) {
    auto* gm = GameManager::sharedState();
    std::mt19937 rng(seed);
    switch (t) {
        case IconType::Cube: return rng() % 485;
        case IconType::Ship: return rng() % 169;
        case IconType::Ball: return rng() % 118;
        case IconType::Ufo: return rng() % 149;
        case IconType::Wave: return rng() % 96;
        case IconType::Robot: return rng() % 68;
        //case IconType::Spider: return rng() % 69;
        case IconType::Spider: return rng() % 30; // ROBTOP WHY CAN'T I SET A FRAME HIGHER THAN 30 FOR SPIDER???? (I'm probably dumb and doing something wrong)
        case IconType::Swing: return rng() % 43;
        case IconType::Jetpack: return rng() % 8;
        default: return 1;
    }
}

static FORCE_INLINE void forceMode(PlayerObject* p, IconType mode, bool isRealPlayer=false) {
    if (!p) return;
    if (!isRealPlayer) g_disableUpdate = true;
    p->toggleFlyMode(false, false);
    p->toggleBirdMode(false, false);
    p->toggleRollMode(false, false);
    p->toggleDartMode(false, false);
    p->toggleRobotMode(false, false);
    p->toggleSpiderMode(false, false);
    p->toggleSwingMode(false, false);
    
    switch (mode) {
        case IconType::Jetpack: p->toggleFlyMode(true, false); break;
        case IconType::Ship: p->toggleFlyMode(true, false); break;
        case IconType::Ufo: p->toggleBirdMode(true, false); break;
        case IconType::Ball: p->toggleRollMode(true, false); break;
        case IconType::Wave: p->toggleDartMode(true, false); break;
        case IconType::Robot: p->toggleRobotMode(true, false); break;
        case IconType::Spider: p->toggleSpiderMode(true, false); break;
        case IconType::Swing: p->toggleSwingMode(true, false); break;
        default: break; // cube
    }
    if (!isRealPlayer) g_disableUpdate = false;
}

static FORCE_INLINE void setPOFrameForIcon(PlayerObject* p, IconType m) {
    if (!p) return;
    // if (currentMode(p) != m) forceMode(p, m);
    forceMode(p, m);
    // log::info("IconType: {}", (int)m);
    //log::info("IconType: {}", (int)m);
    switch (m) {
        case IconType::Jetpack: p->updatePlayerJetpackFrame (frameForIcon(IconType::Jetpack)); p->updatePlayerFrame (frameForIcon(IconType::Cube)); break;
        case IconType::Ship: p->updatePlayerShipFrame (frameForIcon(IconType::Ship)); p->updatePlayerFrame (frameForIcon(IconType::Cube)); break;
        case IconType::Ufo: p->updatePlayerBirdFrame (frameForIcon(IconType::Ufo)); p->updatePlayerFrame (frameForIcon(IconType::Cube)); break;
        case IconType::Ball: p->updatePlayerRollFrame (frameForIcon(IconType::Ball)); break;
        case IconType::Wave: p->updatePlayerDartFrame (frameForIcon(IconType::Wave)); break;
        case IconType::Robot: p->updatePlayerRobotFrame (frameForIcon(IconType::Robot)); break;
        case IconType::Spider: p->updatePlayerSpiderFrame(frameForIcon(IconType::Spider)); break;
        case IconType::Swing: p->updatePlayerSwingFrame (frameForIcon(IconType::Swing)); break;
        default: p->updatePlayerFrame (frameForIcon(IconType::Cube)); break;
    }
}


static FORCE_INLINE void setPOFrameForIcon(PlayerObject* p, IconType m, std::array<int,9> frames) {
    if (!p) return;
    // if (currentMode(p) != m) forceMode(p, m);
    forceMode(p, m);
    //log::info("IconType: {}, RandomID: {}", (int)m, frames[(int)m]);
    switch (m) {
        case IconType::Jetpack: p->updatePlayerJetpackFrame(frames[(int)IconType::Jetpack]); p->updatePlayerFrame(frames[(int)IconType::Cube]); break;
        case IconType::Ship: p->updatePlayerShipFrame(frames[(int)IconType::Ship]); p->updatePlayerFrame(frames[(int)IconType::Cube]); break;
        case IconType::Ufo: p->updatePlayerBirdFrame(frames[(int)IconType::Ufo]); p->updatePlayerFrame(frames[(int)IconType::Cube]); break;
        case IconType::Ball: p->updatePlayerRollFrame(frames[(int)IconType::Ball]); break;
        case IconType::Wave: p->updatePlayerDartFrame(frames[(int)IconType::Wave]); break;
        case IconType::Robot: p->updatePlayerRobotFrame(frames[(int)IconType::Robot]); break;
        case IconType::Spider: p->updatePlayerSpiderFrame(frames[(int)IconType::Spider]); break;
        case IconType::Swing: p->updatePlayerSwingFrame(frames[(int)IconType::Swing]); break;
        default: p->updatePlayerFrame(frames[(int)IconType::Cube]); break;
    }
}

static FORCE_INLINE void setPOFrameForIcon(PlayerObject* p, IconType m,
    const Attempt& a, bool randomFlag) {
    //log::info("randomFlag: {}, iconsAssigned: {}", randomFlag, a.iconsAssigned);
    if (randomFlag && a.iconsAssigned)
        setPOFrameForIcon(p, m, a.randomFrame);
    else
        setPOFrameForIcon(p, m);
    if (a.m_isPlatformer) p->togglePlatformerMode(true);
}

class Ghosts final {
public:
    static Ghosts& I() {
        static Ghosts g;
        return g;
    }

    bool isNoclipDetected = false;
    bool setFalseIfPlayerWasDestroyedCheck = false;

    void beginPlayerDestroyedCheck() { setFalseIfPlayerWasDestroyedCheck = true; }
    void endPlayerDestroyedCheck(bool playerDied) {
        if (!isNoclipDetected) {
            if (setFalseIfPlayerWasDestroyedCheck) isNoclipDetected = !playerDied;
        }
    }
    bool didAttemptUseNoclip() {
        return (setFalseIfPlayerWasDestroyedCheck || isNoclipDetected);
    }
    void resetNoclipDetectedFlag() { 
        isNoclipDetected = false;
        setFalseIfPlayerWasDestroyedCheck = false;
    }
    void updateCurrentAttemptNoclipState() {
        if (!m_currentDidNoclip) m_currentDidNoclip = didAttemptUseNoclip();
    }

    cocos2d::CCPoint forceSetPosP1{0.f, 0.f};
    cocos2d::CCPoint forceSetPosP2{0.f, 0.f};

    bool m_playerSetPositionEnabled = false;

    bool recordInPractice = true;
    bool showWhilePlaying = false;
    bool waveTrailGhost = true;
    int64_t maxGhosts = INT64_MAX;
    GLubyte opacity = 255;
    ColorMode colors = ColorMode::Random;
    bool onlyBestGhost = false;
    float m_waveTrailOpacityPct = 100.f;

    void setP1Hold(bool h) { p1Hold = h; }
    void setP2Hold(bool h) { p2Hold = h; }
    void setP1LHold(bool h) { p1LHold = h; }
    void setP2LHold(bool h) { p2LHold = h; }
    void setP1RHold(bool h) { p1RHold = h; }
    void setP2RHold(bool h) { p2RHold = h; }
    void markWavePointThisFrameP1() { p1WavePointThisFrame = true; }
    void markWavePointThisFrameP2() { p2WavePointThisFrame = true; }


    int m_levelIDOnAttach = 0;

    bool noclip_enabled = false;
    bool safeMode_enabled = false;

    bool m_preloadActive = false;
    int  m_preloadTarget = 0;
    int  m_preloadLoaded = 0;
    size_t m_preloadCursor = 0;

    std::vector<uint32_t> m_preloadOrder;
    std::vector<uint32_t> m_initialAttemptsToSet;

    CheckpointManager& checkpointMgr() { return m_checkpointMgr; }
    const CheckpointManager& checkpointMgr() const { return m_checkpointMgr; }

    static constexpr int kRandomColorCount = kRandomColorSlots;

    bool m_randomColorMaskLoaded = false;
    std::array<uint8_t, kRandomColorCount> m_randomColorAllowed{};
    std::vector<int> m_randomColorAllowedList;

    PreloadSortMode m_preloadSortMode = PreloadSortMode::Best;

    void refreshCustomDeathSfxNow(bool resetGhostObjects) {
        customLastWrite = {};
        refreshCustomExplodeSfx_(customSfx, customLastWrite);

        if (resetGhostObjects) {
            invalidatePrimedPlayerObjectRefs();
            InitialGamemodeSet();
        }
    }

    void setPreloadSortMode(PreloadSortMode sortMode) { 
        m_preloadSortMode = sortMode;
        savePreloadSortMode(m_preloadSortMode);
    }
    PreloadSortMode getPreloadSortMode() {
        m_preloadSortMode = preloadSortModeFromSaved();
        return m_preloadSortMode;
    }

    bool getPlaybackLimitVisibleGhostsEnabled() {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue("limit-visible-ghosts")) setPlaybackLimitVisibleGhostsEnabled(false);
        else m_limitVisibleGhosts = mod->getSavedValue<bool>("limit-visible-ghosts");
        return m_limitVisibleGhosts;
    }

    bool getPlaybackOnlyPastPercentEnabled() {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue("only-show-ghosts-that-passed-percent")) setPlaybackOnlyPastPercentEnabled(false);
        else m_onlyShowGhostsThatPassedPercent = mod->getSavedValue<bool>("only-show-ghosts-that-passed-percent");
        return m_onlyShowGhostsThatPassedPercent;
    }

    int getPlaybackMaxVisibleGhosts() {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue("max-visible-ghosts")) setPlaybackMaxVisibleGhosts(100);
        else m_maxVisibleGhosts = mod->getSavedValue<int>("max-visible-ghosts");
        return m_maxVisibleGhosts;
    }

    float getPlaybackOnlyPastPercentThreshold() {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue("ghosts-passed-percent-threshold")) setPlaybackOnlyPastPercentThreshold(20);
        else {
            m_GhostsPassedPercentThreshold = mod->getSavedValue<float>("ghosts-passed-percent-threshold");
        }
        return m_GhostsPassedPercentThreshold;
    }

    void setPlaybackLimitVisibleGhostsEnabled(bool on) {
        m_limitVisibleGhosts = on;
        Mod::get()->setSavedValue("limit-visible-ghosts", on);
    }

    void setPlaybackOnlyPastPercentEnabled(bool on) {
        m_onlyShowGhostsThatPassedPercent = on;
        Mod::get()->setSavedValue("only-show-ghosts-that-passed-percent", on);

        m_attemptCountsDirty = true;
        buildGhostThatPassedPercentSerialList();
        rebuildGridThreasholdSet_();
    }

    void setPlaybackMaxVisibleGhosts(int maxVisible) {
        m_maxVisibleGhosts = maxVisible;
        Mod::get()->setSavedValue("max-visible-ghosts", maxVisible);
    }

    void setPlaybackOnlyPastPercentThreshold(float percent) {
        m_GhostsPassedPercentThreshold = percent;
        Mod::get()->setSavedValue("ghosts-passed-percent-threshold", percent);

        m_attemptCountsDirty = true;
        buildGhostThatPassedPercentSerialList();
        rebuildGridThreasholdSet_();
    }

    void buildGhostThatPassedPercentSerialList() {
        m_ghostsThatPassThePercentThreshold.clear();
        m_playerObjectPool.clearOwners(true);

        for (size_t i = 0; i < attempts.size(); ++i) {
            Attempt& a = attempts[i];

            if (!a.practiceAttempt && a.endPercent >= m_GhostsPassedPercentThreshold) {
                m_ghostsThatPassThePercentThreshold.push_back(static_cast<uint32_t>(i)); // loaded attempt indices
            }
            else {
                a.setP1Visible(false, true);
                a.setP2Visible(false, true);
            }
        }

        m_initialAttemptsToSet.clear();
        m_attemptCountsDirty = true;
    }

    void beginPostUpdateTick_() {
        m_allowWorkThisTick = true;
        m_allowSetPlayerPos = true;
        m_allowSetPlayerClickState = true;
        ++m_tickId;
    }

    void endPostUpdateTick_() {
        m_allowWorkThisTick = false;
    }

    bool shouldRunWorkThisTick_() {
        if (!m_allowWorkThisTick) return false;
        if (m_lastWorkTickId == m_tickId) return false;
        m_lastWorkTickId = m_tickId;
        return true;
    }

    bool deleteCurrentLevelSaveFile() {

        if (isPracticeMode()) togglePracticeMode(false);
        
        flushPendingSaves_();
        
        auto path = fileForLevel_(m_levelIDOnAttach);
        std::error_code ec;
        
        if (!std::filesystem::exists(path, ec)) {
            log::info("deleteCurrentLevelSaveFile: file does not exist at {}", geode::utils::string::pathToString(path));
            return false;
        }

        stopReplay();
        stopPlayback();
        
        bool success = std::filesystem::remove(path, ec);
        
        if (success) {
            //log::info("Deleted save file: {}", geode::utils::string::pathToString(path));
            
            //log::info("[Delete] Clearing {} attempts from memory...", attempts.size());
            
            clearAllGhostNodes();
            attempts.clear();
            invalidateAttemptPointerCaches_();
            attempts.shrink_to_fit();
            
            m_current = Attempt{};
            m_currentDidNoclip = false;
            m_loadedSerials.clear();
            clearAttemptCatalog_();
            m_loadedLevelID = 0;
            m_nextAttemptSerial = 1;
            m_spans.clear();
            m_grid.clear();
            m_gridThreshold.clear();
            m_cachedCandidates.clear();
            m_preloadedIndices.clear();
            m_preloadedSet.clear();
            m_primedIndices.clear();
            m_wantToPrimeIndices.clear();
            m_primedSet.clear();
            m_wantToPrimeSet.clear();
            m_serialCacheDirty = true;
            
            m_checkpointMgr = CheckpointManager();
            invalidateAttemptCounts();
            stopPlayback();
            stopReplay();
            
            recording = false;
            toggleRecording();
            
            restartLevel();
            
            //log::info("[Delete] After clear: attempts.size()={}", attempts.size());
        } else {
            log::warn("Failed to delete save file: {} (error: {})", 
                    geode::utils::string::pathToString(path), ec.message());
        }
        
        return success;
    }

    std::string getCurrentLevelSaveFileName() const {
        auto path = fileForLevel_(m_levelIDOnAttach);
        return geode::utils::string::pathToString(path.filename());
    }

    void ensureRandomColorMaskLoaded_() {
        if (m_randomColorMaskLoaded) return;
        m_randomColorMaskLoaded = true;
        m_randomColorAllowed.fill(1);

        auto* mod = Mod::get();
        const bool has = mod->hasSavedValue(kGhostRandomColorsMaskKey);
        
        if (has) {
            auto s = mod->getSavedValue<std::string>(kGhostRandomColorsMaskKey);

            if (s.size() == (size_t)kRandomColorCount) {
                int enabled = 0;
                for (int slot = 0; slot < kRandomColorCount; ++slot) {
                    const bool on = (s[slot] != '0');
                    m_randomColorAllowed[slot] = on ? 1 : 0;
                    enabled += on ? 1 : 0;
                }
                //log::info("[RandomColors] parsed mask ok: enabledSlots={}/{}", enabled, kRandomColorCount);
            } else {
                log::warn("[RandomColors] saved mask wrong length (got {}, expected {}), using default(all enabled)",
                        s.size(), (size_t)kRandomColorCount);
            }
        }

        rebuildRandomColorAllowedList_();
    }

    void rebuildRandomColorAllowedList_() {
        m_randomColorAllowedList.clear();
        m_randomColorAllowedList.reserve(kRandomColorCount);

        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            if (m_randomColorAllowed[slot]) m_randomColorAllowedList.push_back(kRandomColorIDs[slot]);
        }
    }

    void saveRandomColorMask_() {
        std::string s;
        s.reserve(kRandomColorCount);

        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            const bool on = (m_randomColorAllowed[slot] != 0);
            s.push_back(on ? '1' : '0');
        }

        Mod::get()->setSavedValue(kGhostRandomColorsMaskKey, s);
    }

    bool isRandomGhostColorAllowed(int slot) {
        ensureRandomColorMaskLoaded_();

        if (slot < 0 || slot >= kRandomColorCount) {
            // log::info("[RandomColors] isRandomGhostColorAllowed: slot out of range: {}", slot);
            return false;
        }

        const bool allowed = (m_randomColorAllowed[slot] != 0);
        return allowed;
    }

    void setRandomGhostColorAllowed(int slot, bool allowed) {
        ensureRandomColorMaskLoaded_();

        if (slot < 0 || slot >= kRandomColorCount) return;
        m_randomColorAllowed[slot] = allowed ? 1 : 0;

        rebuildRandomColorAllowedList_();
        saveRandomColorMask_();
    }

    void setAllRandomGhostColorsAllowed(bool allowed) {
        ensureRandomColorMaskLoaded_();

        m_randomColorAllowed.fill(allowed ? 1 : 0);
        rebuildRandomColorAllowedList_();
        saveRandomColorMask_();
    }

    int pickRandomAllowedGhostColorIdx(uint32_t seed) {
        ensureRandomColorMaskLoaded_();

        if (m_randomColorAllowedList.empty()) return kRandomColorIDs[0];

        std::mt19937 rng(seed);
        std::uniform_int_distribution<size_t> pick(0, m_randomColorAllowedList.size() - 1);
        const size_t idx = pick(rng);
        const int paletteIdx = m_randomColorAllowedList[idx];

        return paletteIdx;
    }

    int getNumAttemptsPreloadedTotal() { return m_attemptsPreloadedTotal; }

    struct CandidateInfo {
            int index;
            float endX;
        };
    std::vector<CandidateInfo> candidates;

    void buildInitialAttemptsFromPreloadOrder(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial
    ) {
        std::vector<uint32_t> sorted = preloadOrder;

        if (m_replayKind == ReplayKind::PracticeComposite) {
            auto getEndT = [&](uint32_t serial) -> double {
                const Attempt* a = findLoadedAttemptBySerialOnly_(static_cast<int>(serial));
                if (a && !a->p1.empty()) {
                    return static_cast<double>(a->p1.back().t);
                }

                const APXAttemptDiskInfo* entry = findCatalogEntryBySerial_(static_cast<int>(serial));
                if (entry) {
                    return 0.0;
                }

                return 0.0;
            };

            std::sort(sorted.begin(), sorted.end(),
                [&](uint32_t lhs, uint32_t rhs) {
                    const double tL = getEndT(lhs);
                    const double tR = getEndT(rhs);
                    if (tL != tR) return tL < tR;
                    return lhs < rhs;
                });
        }

        const size_t initialCount = std::min(
            static_cast<size_t>(m_playerObjectPool.capacity()),
            sorted.size()
        );

        outInitial.assign(sorted.begin(), sorted.begin() + initialCount);
    }

    void buildPreloadOrderForCount_(int targetCount, std::vector<uint32_t>& outOrder) const {
        outOrder.clear();

        if (targetCount <= 0) return;

        const auto wantPractice = practiceFilterEvenWhenBotNotOn();
        const int totalMatching = wantPractice ? m_cachedPracticeAttempts : m_cachedNormalAttempts;
        const int takeCount = std::min(targetCount, totalMatching);

        if (takeCount <= 0) return;

        std::vector<uint32_t> candidateSerials = m_preloadableSerials;
        if (candidateSerials.empty()) return;

        auto getEndT = [&](uint32_t serial) -> double {
            float endX = 0.0f;
            double endT = 0.0;
            getPreloadSortInfoForSerial_(static_cast<int>(serial), endX, endT);
            return endT;
        };

        auto getEndX = [&](uint32_t serial) -> float {
            float endX = 0.0f;
            double endT = 0.0;
            getPreloadSortInfoForSerial_(static_cast<int>(serial), endX, endT);
            return endX;
        };

        outOrder.reserve(std::min(takeCount, static_cast<int>(candidateSerials.size())));

        PreloadSortMode sortMode = m_preloadSortMode;
        if (m_replayKind == ReplayKind::PracticeComposite) {
            sortMode = PreloadSortMode::Random;
        }

        switch (sortMode) {
            case PreloadSortMode::Best: {
                std::sort(candidateSerials.begin(), candidateSerials.end(),
                    [&](int lhs, int rhs) {
                        const float xL = getEndX(lhs);
                        const float xR = getEndX(rhs);
                        if (xL != xR) return xL > xR;

                        const double tL = getEndT(lhs);
                        const double tR = getEndT(rhs);
                        if (tL != tR) return tL > tR;

                        return lhs > rhs;
                    });

                outOrder.insert(
                    outOrder.end(),
                    candidateSerials.begin(),
                    candidateSerials.begin() + std::min(takeCount, static_cast<int>(candidateSerials.size()))
                );
                break;
            }

            case PreloadSortMode::Recent: {
                std::sort(candidateSerials.begin(), candidateSerials.end(),
                    [&](int lhs, int rhs) {
                        return lhs > rhs;
                    });

                outOrder.insert(
                    outOrder.end(),
                    candidateSerials.begin(),
                    candidateSerials.begin() + std::min(takeCount, static_cast<int>(candidateSerials.size()))
                );
                break;
            }

            case PreloadSortMode::Random:
            default: {
                // Sort candidates by endT first
                std::sort(candidateSerials.begin(), candidateSerials.end(),
                    [&](int lhs, int rhs) {
                        const double tL = getEndT(lhs);
                        const double tR = getEndT(rhs);
                        if (tL != tR) return tL < tR;
                        return lhs < rhs;
                    });
                
                // Use quantile buckets by sorted rank, not value-width buckets
                // That keeps bucket sizes balanced even if endT is heavily skewed
                const int desiredBuckets = std::max(
                    4,
                    std::min(
                        64,
                        std::max(
                            takeCount / 2,
                            static_cast<int>(std::sqrt(static_cast<float>(candidateSerials.size())))
                        )
                    )
                );

                const int numBuckets = std::max(
                    1,
                    std::min(desiredBuckets, static_cast<int>(candidateSerials.size()))
                );

                std::vector<std::vector<int>> buckets(numBuckets);

                for (size_t i = 0; i < candidateSerials.size(); ++i) {
                    const int bucket = std::min(
                        numBuckets - 1,
                        static_cast<int>((i * static_cast<size_t>(numBuckets)) / candidateSerials.size())
                    );
                    buckets[bucket].push_back(candidateSerials[i]);
                }

                std::mt19937 rng(static_cast<uint32_t>(m_levelIDOnAttach) ^ 0x9E3779B9u);
                // Shuffle within each endT bucket so preload isn't too predictable

                for (auto& bucket : buckets) {
                    std::shuffle(bucket.begin(), bucket.end(), rng);
                }

                // Build a randomized round-robin bucket visitation order
                std::vector<int> bucketVisitOrder(numBuckets);
                std::iota(bucketVisitOrder.begin(), bucketVisitOrder.end(), 0);
                std::shuffle(bucketVisitOrder.begin(), bucketVisitOrder.end(), rng);

                std::vector<size_t> bucketCursors(numBuckets, 0);

                while (static_cast<int>(outOrder.size()) < takeCount) {
                    bool anyRemaining = false;

                    for (int b : bucketVisitOrder) {
                        if (bucketCursors[b] < buckets[b].size()) {
                            outOrder.push_back(static_cast<uint32_t>(buckets[b][bucketCursors[b]++]));
                            anyRemaining = true;

                            if (static_cast<int>(outOrder.size()) >= takeCount) {
                                break;
                            }
                        }
                    }

                    if (!anyRemaining) break;

                    // Rotate the visit order each round so the same bucket doesn't keep
                    // getting first pick on every pass
                    if (bucketVisitOrder.size() > 1) {
                        std::rotate(
                            bucketVisitOrder.begin(),
                            bucketVisitOrder.begin() + 1,
                            bucketVisitOrder.end()
                        );
                    }
                }

                break;
            }
        }
    }

    void beginPreloadAttempts(int targetCount) {
        if (isPureRecordingMode_()) {
            m_preloadActive = false;
            m_initialAttemptsToSet.clear();
            return;
        }

        cancelPreloadAttempts(true);
        clearAttemptPreloadStateForCurrentSelection_();
        m_preloadCursor = 0;
        m_preloadLoaded = 0;
        m_preloadTarget = 0;
        m_preloadActive = false;

        if (targetCount <= 0) return;

        m_1PreloadedSoDontShowGhosts = (targetCount == 1);

        m_attemptCountsDirty = true;
        rebuildAttemptCountsIfNeeded();
        if (botActive && practiceRouteOwnerHidingEnabled_()) {
            rebuildRouteOwnerCache_();
        }

        const auto wantPractice = practiceFilterEvenWhenBotNotOn();

        int readyNow = 0;
        int totalMatching = wantPractice ? m_cachedPracticeAttempts : m_cachedNormalAttempts;

        m_preloadTarget = std::min(targetCount, totalMatching);
        m_preloadLoaded = std::min(readyNow, m_preloadTarget);

        const int remainingNeeded = std::max(0, m_preloadTarget - m_preloadLoaded);

        if (remainingNeeded <= 0) {
            m_preloadActive = false;
            return;
        }

        buildPreloadOrderForCount_(remainingNeeded, m_preloadOrder);

        if (m_preloadOrder.empty()) {
            m_preloadActive = false;
            return;
        }

        // Reverse so the earliest deaths have real player objects (only normal mode or max load)
        if (!wantPractice && (m_preloadSortMode == PreloadSortMode::Best || targetCount == m_cachedNormalAttempts)) {
            std::reverse(m_preloadOrder.begin(), m_preloadOrder.end());
        }

        // Build the smaller real player object initial set list from the first N
        // preloadOrder entries, then sort that list by endT
        buildInitialAttemptsFromPreloadOrder(m_preloadOrder, m_initialAttemptsToSet);
        // do the ones I set the icon for first
        prioritizeInitialAttemptsInPreloadOrder_(targetCount);

        m_preloadCursor = 0;
        m_preloadActive = (m_preloadLoaded < m_preloadTarget) && !m_preloadOrder.empty();
    }

    void prioritizeInitialAttemptsInPreloadOrder_(int targetCount) {
        if (m_preloadOrder.empty()) return;

        std::unordered_set<uint32_t> validCandidates(
            m_preloadOrder.begin(),
            m_preloadOrder.end()
        );

        std::unordered_set<uint32_t> initialSet(
            m_initialAttemptsToSet.begin(),
            m_initialAttemptsToSet.end()
        );

        std::unordered_set<uint32_t> added;
        std::vector<uint32_t> reordered;
        reordered.reserve(m_preloadOrder.size() + 1);

        auto pushIfValid = [&](uint32_t serial) {
            if (validCandidates.find(serial) == validCandidates.end()) return;
            if (added.insert(serial).second) {
                reordered.push_back(serial);
            }
        };

        // Force route owner if it is actually part of this preload candidate set
        if (targetCount > 1) {
            if (m_activeOwnerSerial > 0) {
                pushIfValid(static_cast<uint32_t>(m_activeOwnerSerial));
            } else if (m_replayOwnerSerial > 0) {
                pushIfValid(static_cast<uint32_t>(m_replayOwnerSerial));
            }
        }

        // Force initially assigned PlayerObject attempts
        for (uint32_t serial : m_initialAttemptsToSet) {
            pushIfValid(serial);
        }

        // Append the rest
        for (uint32_t serial : m_preloadOrder) {
            pushIfValid(serial);
        }

        if (!reordered.empty()) {
            m_preloadOrder = std::move(reordered);
        }
    }

    void preloadStep(int maxPerTick) {
        if (isPureRecordingMode_()) {
            m_preloadActive = false;
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
            return;
        }

        if (!m_preloadActive || maxPerTick <= 0) return;

        int work = 0;
        
        while (work < maxPerTick &&
            m_preloadLoaded < m_preloadTarget &&
            m_preloadCursor < m_preloadOrder.size())
        {
            const int serial = static_cast<int>(m_preloadOrder[m_preloadCursor]);

            Attempt* loaded = ensureAttemptLoadedBySerial_(serial, false);
            if (!loaded) {
                ++m_preloadCursor;
                continue;
            }

            const size_t idx = findLoadedAttemptIndexBySerial_(serial);
            if (idx == SIZE_MAX || idx >= attempts.size()) {
                ++m_preloadCursor;
                continue;
            }

            Attempt& a = attempts[idx];
            preloadAttempt_(a, idx, true);
            InitialGamemodeSetProcessAttemptIdx(idx);

            if (a.preloaded) {
                ++m_preloadLoaded;
                ++work;
            }

            ++m_preloadCursor;
        }

        if (m_preloadLoaded >= m_preloadTarget || m_preloadCursor >= m_preloadOrder.size()) {
            m_preloadActive = false;
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
        }
    }

    void InitialGamemodeSetProcessAttemptIdx(size_t attemptIdx) {
        if (attemptIdx >= attempts.size()) return;
        if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) return;

        Attempt& a = attempts[attemptIdx];

        auto h = m_playerObjectPool.acquireForOwner(a.serial, static_cast<uint32_t>(attemptIdx), false);
        if (h) { a.g1Idx = h.index; a.g1 = h.po; }

        if (a.hadDual) {
            auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000, static_cast<uint32_t>(attemptIdx), true);
            if (h2) { a.g2Idx = h2.index; a.g2 = h2.po; }
        }

        if (a.g1 && !a.p1.empty()) {
            a.g1CurMode = a.p1.front().mode;
            setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
            a.setP1Visible(false, true);
        }
        if (a.g2 && !a.p2.empty()) {
            a.g2CurMode = a.p2.front().mode;
            setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
            a.setP2Visible(false, true);
        }
    }


    void InitialGamemodeSetProcessBatch(
        const std::vector<uint32_t>* orderList,
        int batchSize,
        int startIndex = 0
    ) {
        if (!orderList || orderList->empty()) return;

        const auto& order = *orderList;
        const int end = std::min(startIndex + batchSize, static_cast<int>(order.size()));

        for (int i = startIndex; i < end; ++i) {
            const int serial = static_cast<int>(order[i]);

            Attempt* loaded = ensureAttemptLoadedBySerial_(serial, false);
            if (!loaded) {
                continue;
            }

            const size_t attemptIdx = findLoadedAttemptIndexBySerial_(serial);
            if (attemptIdx == SIZE_MAX || attemptIdx >= attempts.size()) {
                continue;
            }

            if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) {
                return;
            }

            Attempt& a = attempts[attemptIdx];

            auto h = m_playerObjectPool.acquireForOwner(a.serial, static_cast<uint32_t>(attemptIdx), false);
            if (h) {
                a.g1Idx = h.index;
                a.g1 = h.po;
            }

            if (a.hadDual) {
                auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000, static_cast<uint32_t>(attemptIdx), true);
                if (h2) {
                    a.g2Idx = h2.index;
                    a.g2 = h2.po;
                }
            }

            if (a.g1 && !a.p1.empty()) {
                a.g1CurMode = a.p1.front().mode;
                setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
                a.setP1Visible(false, true);
            }

            if (a.g2 && !a.p2.empty()) {
                a.g2CurMode = a.p2.front().mode;
                setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
                a.setP2Visible(false, true);
            }
        }
    }

    // Please can you be the same as preloading? It's slightly different for some reason
    void InitialGamemodeSet() {
        m_playerObjectPool.clearOwners(true);
        InitialGamemodeSetProcessBatch(&m_preloadOrder, m_playerObjectPool.capacity());
    }

    bool isPreloadingAttempts() const { return m_preloadActive; }
    int getPreloadLoadedCount() const { return m_preloadLoaded; }
    int getPreloadTargetCount() const { return m_preloadTarget; }
    bool isPlayerFrozen() const { return m_freezePlayerXAtEnd; }

    void resetAttemptPreloadState_(Attempt& a, int idx) {
        (void)idx;

        if (a.g1) a.setP1Visible(false, true);
        if (a.g2) a.setP2Visible(false, true);

        a.preloaded = false;

        a.resetPlayback();

        a.p1Visible = false;
        a.p2Visible = false;
        a.last1 = PoseCache{};
        a.last2 = PoseCache{};

        a.g1CurMode = IconType::Cube;
        a.g2CurMode = IconType::Cube;

        a.g1 = nullptr;
        a.g2 = nullptr;
        a.g1Idx = -1;
        a.g2Idx = -1;
    }

    void clearAllAttemptPreloadState_() {
        for (size_t i = 0; i < attempts.size(); ++i) {
            resetAttemptPreloadState_(attempts[i], static_cast<int>(i));
        }
    }

    void clearAttemptPreloadStateForCurrentSelection_() {
        for (int idx : m_preloadableIndexesInAttemptsList) {
            if (idx < 0 || idx >= static_cast<int>(attempts.size())) continue;
            resetAttemptPreloadState_(attempts[idx], idx);
        }
    }

    void cancelPreloadAttempts(bool ignoreStopPlayback) {
        //m_ghostPool.logStats("CancelPreload");
        m_preloadActive = false;
        m_preloadTarget = 0;
        m_preloadLoaded = 0;
        m_preloadCursor = 0;
        m_preloadOrder.clear();
        m_initialAttemptsToSet.clear();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        practiceAttemptReplayIndex.clear();
        m_attemptsPreloadedTotal = 0;

        m_playerObjectPool.clearOwners(true);
        clearAllAttemptPreloadState_();

        m_primedIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeIndices.clear();
        m_wantToPrimeSet.clear();
        m_practiceSerialSet.clear();

        const bool keepRouteOwnerCache =
            ignoreStopPlayback &&
            botActive &&
            practiceRouteOwnerHidingEnabled_();

        if (!keepRouteOwnerCache) {
            m_routeOwnerSerials.clear();
            m_currentlyHiddenSerials.clear();
        }

        if (!ignoreStopPlayback) {
            stopReplay();
            playback = false;
            recording = true;

            forcePlayersVisible_();
            clearHeldInputs_();
        }
    }

    int getTotalAttemptsCount() {
        // No valid session for start pos for practice mode
        if (m_replayKind == ReplayKind::PracticeComposite && m_checkpointMgr.noValidSessionForStartX()) return 0;

        m_attemptCountsDirty = true; // Fix later, but for now always rebuild
        rebuildAttemptCountsIfNeeded();

        if (practiceFilterEvenWhenBotNotOn()) return m_cachedPracticeAttempts;
        else return m_cachedNormalAttempts;
    }

    preload_memory::PreloadMemoryEstimate estimatePreloadMemoryUsage(
        int targetCount,
        int realPlayerObjectAttempts,
        size_t bytesPerPlayerObject
    ) {
        if (targetCount <= 0) {
            return {};
        }

        m_attemptCountsDirty = true;
        rebuildAttemptCountsIfNeeded();
        rebuildSerialCache_();

        if (m_levelIDOnAttach != 0) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }

        std::vector<uint32_t> order;
        buildPreloadOrderForCount_(targetCount, order);

        return preload_memory::estimateForSerialOrder(
            order,
            targetCount,
            std::max(0, realPlayerObjectAttempts),
            bytesPerPlayerObject,
            attempts,
            m_serialToIdx,
            m_attemptCatalog,
            m_attemptCatalogBySerial
        );
    }

    bool hasModAttachedToLevel() { return m_pl != nullptr; }

    void flushPendingSaves_() {
        if (m_levelIDOnAttach != 0) {
            //saveCurrentAttemptNow();
            int n = saveNewAttemptsForLevel_(m_levelIDOnAttach);
            //log::info("[Ghosts] flushPendingSaves_: wrote {} attempt(s) for level {}", n, m_levelIDOnAttach);
        }
    }

    void setUseCheckpointsRoute(bool on) { m_useCheckpointsRoute = on; }
    bool useCheckpointsRoute() const { return m_useCheckpointsRoute; }
    
    void setGhostDistance(int maxPx) { m_randomDistPx = maxPx; }

    void setModEnabled(bool on) {
        modEnabled = on;
        Mod::get()->setSavedValue("mod-enabled", on);
        if (!modEnabled) {
            flushPendingSaves_();
            stopReplay();
            clearAllGhostNodes();
            attempts.clear();
            m_loadedSerials.clear();
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
    bool isModEnabled() const {
        bool isPlatformer = false;
        if (m_pl) isPlatformer = m_pl->m_isPlatformer;
        if (isPlatformer && m_allow_platformer) return modEnabled;
        return modEnabled && !isPlatformer; 
    }

    bool isUpdateAttemptCountBlocked() const { return m_blockAttemptCount; }

    void updateModEnabled() {
        auto* mod = Mod::get();
        bool temp_modEnabled = getSettingBoolOrDefault_(mod, "mod-enabled", true);
        m_allow_platformer = getSettingBoolOrDefault_(mod, "allow-platformer", false);
        if (temp_modEnabled != modEnabled) setModEnabled(temp_modEnabled);
    }

    void setPerfBudgetFromUI(int v) {
        m_budgetPerFrame = std::max(1, v);
        m_perfUnlimited = (m_budgetPerFrame >= kPerfBudgetInfiniteSentinel);
        if (m_perfUnlimited) {
            //log::info("[Perf] Unlimited mode active: budget=∞, visible=∞, updates=realtime");
            m_maxVisibleGhosts = std::numeric_limits<int>::max() / 4;
        }
    }

    std::filesystem::path attemptsDir_() const {
        if (m_attemptsDir.empty()) {
            auto base = Mod::get()->getSaveDir();
            m_attemptsDir = base / "attempts";

            std::error_code ec;
            std::filesystem::create_directories(m_attemptsDir, ec);
            if (ec) {
                log::warn(
                    "[SAVE FILE] create_directories failed: {} ({})",
                    geode::utils::string::pathToString(m_attemptsDir),
                    ec.message()
                );
            }
        }
        return m_attemptsDir;
    }

    std::filesystem::path fileForLevel_(int levelID) const {
        auto dir = attemptsDir_();

        if (levelID < 120 && !m_customSaveId.empty()) {
            return dir / (m_customSaveId + "_attempts.apx");
        }

        return dir / (std::to_string(levelID) + "_attempts.apx");
    }

    bool loadLevelFileWithMigration_(int levelID, std::vector<Attempt>& attemptsOut, PracticePath& practicePathOut) {
        return loadAPXFileWithMigration(fileForLevel_(levelID), attemptsOut, practicePathOut);
    }

    bool saveLevelFileCurrent_(int levelID, std::vector<Attempt> const& attempts, PracticePath const& practicePath) {
        const auto path = fileForLevel_(levelID);
        ensureHeader_(path);
        return saveAPXFileCurrent(path, attempts, practicePath);
    }

    void ensureHeader_(std::filesystem::path const& path) {
        std::error_code ec;

        ec.clear();
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            log::warn(
                "[SAVE FILE] exists failed for {}: {}",
                geode::utils::string::pathToString(path),
                ec.message()
            );
            return;
        }

        uintmax_t fsz = 0;
        if (exists) {
            ec.clear();
            fsz = std::filesystem::file_size(path, ec);
            if (ec) {
                log::warn(
                    "[SAVE FILE] file_size failed for {}: {}",
                    geode::utils::string::pathToString(path),
                    ec.message()
                );
                return;
            }
        }

        if (exists && fsz >= sizeof(APXHeader)) {
            std::ifstream checkFile(path, std::ios::binary);
            if (checkFile) {
                APXHeader existing{};
                checkFile.read(reinterpret_cast<char*>(&existing), sizeof(existing));
                if (checkFile && std::string(existing.magic, 4) == "APX2") {
                    return;
                }
            }
        }

        std::ofstream hout(path, std::ios::binary | std::ios::trunc);
        if (!hout) {
            log::warn(
                "[SAVE FILE] failed to open header file for write: {}",
                geode::utils::string::pathToString(path)
            );
            return;
        }

        APXHeader h{};
        std::memcpy(h.magic, "APX2", 4);
        h.version = kAPXVersion;
        hout.write(reinterpret_cast<const char*>(&h), sizeof(h));
        hout.flush();
    }

    // 16 char hex hash of current level name
    // FNV-1a 64-bit
    static inline std::string customLevelSaveId(const std::string& name) {
        constexpr uint64_t kOffset = 0xcbf29ce484222325ULL;
        constexpr uint64_t kPrime = 0x100000001b3ULL;
        uint64_t h = kOffset;
        for (unsigned char c : name) {
            h ^= static_cast<uint64_t>(c);
            h *= kPrime;
        }
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << h;
        return oss.str(); // ex: "4f82d0e62887c3fa"
    }

    void refreshLevelName(int levelID) {
        //log::info("[refreshLevelName] levelID < 120 {}",levelID < 120);
        //log::info("[refreshLevelName] m_pl {}",m_pl!=nullptr);
        //log::info("[refreshLevelName] m_pl->m_level {}",m_pl!=nullptr && m_pl->m_level!=nullptr);
        
        if (levelID < 120 && m_pl && m_pl->m_level) {
            const std::string lvlName = m_pl->m_level->m_levelName;
            //log::info("[lvlName] {}",lvlName);
            if (lvlName.length() == 0) m_customSaveId.clear();
            else m_customSaveId = customLevelSaveId(lvlName);
        } else {
            m_customSaveId.clear();
        }
    }

    void prepareLevelPersistence(int levelID, PlayLayer* pl) {
        m_pl = pl;
        m_levelIDOnAttach = levelID;
        refreshLevelName(levelID);
    }

    void renumberCurrentAttemptIfFresh() {
        if (m_current.p1.empty() && m_current.p2.empty()) {
            m_current.serial = m_nextAttemptSerial++;
        }
    }

    bool rewriteWholeFileToCurrent_(std::filesystem::path const& path) {
        const size_t validAttempts = countAttemptsWithData_(attempts);

        if (validAttempts == 0) {
            log::error(
                "[APX save] ABORTING migration rewrite for {}: 0 valid attempts in memory. "
                "Refusing to overwrite existing file.",
                geode::utils::string::pathToString(path)
            );
            return false;
        }
        
        auto tmp = path;
        tmp += ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                log::warn(
                    "[APX save] failed to open temp migration file: {}",
                    geode::utils::string::pathToString(tmp)
                );
                return false;
            }

            APXHeader h{};
            std::memcpy(h.magic, "APX2", 4);
            h.version = kAPXVersion;
            out.write(reinterpret_cast<const char*>(&h), sizeof(h));
            if (!out.good()) return false;

            size_t writtenAttempts = 0;
            for (auto const& a : attempts) {
                if (!attemptHasData_(a)) continue;
                if (!writeAPXAttemptCompact(out, a)) return false;
                ++writtenAttempts;
            }

            if (writtenAttempts == 0) {
                log::error(
                    "[APX save] ABORTING migration rewrite for {}: wrote 0 attempts unexpectedly.",
                    geode::utils::string::pathToString(path)
                );
                return false;
            }

            if (!writeAPXPracticePath(out, m_checkpointMgr.getPath())) {
                return false;
            }

            out.flush();
            if (!out.good()) return false;
        }

        std::error_code ec;
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            log::warn(
                "[APX save] migration rename failed {} -> {}: {}",
                geode::utils::string::pathToString(tmp),
                geode::utils::string::pathToString(path),
                ec.message()
            );
            std::error_code ec2;
            std::filesystem::remove(tmp, ec2);
            return false;
        }

        for (auto& a : attempts) {
            if (attemptHasData_(a)) a.persistedOnDisk = true;
        }

        m_needsMigrationRewrite = false;
        m_loadedFileWasLegacyAttempts = false;
        return true;
    }

    int saveNewAttemptsForLevel_(int levelID) {
        if (!m_pl) return 0;
        if (m_isSaving) return 0;
        m_isSaving = true;

        auto path = fileForLevel_(levelID);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            log::warn(
                "[APX save] create_directories failed: {} ({})",
                geode::utils::string::pathToString(path.parent_path()),
                ec.message()
            );
            m_isSaving = false;
            return 0;
        }

        const size_t validAttempts = countAttemptsWithData_(attempts);

        int unsavedValidAttempts = 0;
        for (auto const& a : attempts) {
            if (!attemptHasData_(a)) continue;
            if (!a.recordedThisSession) continue;
            if (a.persistedOnDisk) continue;
            ++unsavedValidAttempts;
        }

        if (validAttempts == 0) {
            m_isSaving = false;
            return 0;
        }

        if (!m_needsMigrationRewrite && unsavedValidAttempts == 0 && !m_checkpointMgr.isDirty()) {
            m_isSaving = false;
            return 0;
        }


        int written = 0;

        if (m_needsMigrationRewrite) {
            if (!rewriteWholeFileToCurrent_(path)) {
                m_isSaving = false;
                return 0;
            }
        } else {
            ensureHeader_(path);
        }

        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) {
            m_isSaving = false;
            return 0;
        }

        for (auto& a : attempts) {
            if (!attemptHasData_(a)) continue;
            if (!a.recordedThisSession) continue;
            if (a.persistedOnDisk) continue;

            if (!writeAPXAttemptCompact(out, a)) {
                m_isSaving = false;
                return written;
            }
            a.persistedOnDisk = true;
            ++written;
        }

        if (m_checkpointMgr.isDirty()) {
            if (writeAPXPracticePath(out, m_checkpointMgr.getPath())) {
                m_checkpointMgr.clearDirty();
            } else {
                log::warn("[APX save] PATH write failed (stream bad)");
            }
        }

        out.flush();

        m_isSaving = false;
        return written;
    }


    int loadFromDisk_(int levelID, LoadFilter const& flt) {
        if (m_loadedLevelID == levelID && !attempts.empty()) {
            scanAttemptCatalogForLevel_(levelID);
            return 0;
        }

        if (m_loadedLevelID != levelID) {
            m_checkpointMgr = CheckpointManager();
            m_needsMigrationRewrite = false;
            m_loadedFileWasLegacyAttempts = false;
            clearAttemptCatalog_();
        }

        scanAttemptCatalogForLevel_(levelID);

        auto path = fileForLevel_(levelID);
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return 0;
        }

        APXHeader h{};
        in.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!in || std::memcmp(h.magic, "APX2", 4) != 0) {
            log::warn("[APX load] bad header for {}", geode::utils::string::pathToString(path));
            return 0;
        }

        const bool headerIsLegacy = (h.version < kAPXVersion);
        const bool oldWavePointBitMeaning = h.version < kAPXVersion_WavePointThisFrame;

        const bool forceFullMigrationLoad = headerIsLegacy || oldWavePointBitMeaning;

        if (forceFullMigrationLoad) {
            m_needsMigrationRewrite = true;
        }

        size_t loaded = 0;
        size_t nonPracticeLoaded = 0;
        uint32_t maxSerialSeen = 0;

        while (in) {
            uint32_t type = 0, sz = 0;
            in.read(reinterpret_cast<char*>(&type), sizeof(type));
            in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
            if (!in) break;

            std::streampos chunkStart = in.tellg();

            if (type == kChunk_ATT3) {
                Attempt a{};
                if (!readAPXAttemptCompact(in, sz, a)) {
                    break;
                }

                if (oldWavePointBitMeaning) {
                    clearAPXWavePointThisFrameBits(a);
                }

                if (m_loadedSerials.count(a.serial)) {
                    continue;
                }

                maxSerialSeen = std::max(maxSerialSeen, static_cast<uint32_t>(a.serial));

                const bool isPractice = a.practiceAttempt;
                bool loadThis = true;

                if (!(forceFullMigrationLoad)) {
                    if (flt.practice.has_value() && flt.practice.value() != isPractice) loadThis = false;
                    if (flt.minPct.has_value() && a.startPercent < flt.minPct.value()) loadThis = false;
                    if (flt.maxPct.has_value() && a.startPercent > flt.maxPct.value()) loadThis = false;
                    if (loadThis && !isPractice && flt.maxNonPractice > 0
                        && nonPracticeLoaded >= flt.maxNonPractice) loadThis = false;
                }

                if (!loadThis) {
                    continue;
                }

                m_loadedSerials.insert(a.serial);
                pushAttempt(std::move(a), false);

                ++loaded;
                if (!isPractice) ++nonPracticeLoaded;
            }
            else if (type == kChunk_ATTZ) {
                m_loadedFileWasLegacyAttempts = true;
                m_needsMigrationRewrite = true;

                std::streampos attzChunkStart = in.tellg();

                APXMeta m{};
                in.read(reinterpret_cast<char*>(&m), sizeof(m));
                if (!in) break;

                if (m_loadedSerials.count((int)m.serial)) {
                    auto now = in.tellg();
                    auto readSoFar = (std::streamoff)(now - attzChunkStart);
                    auto remain = (std::streamoff)sz - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                    continue;
                }

                maxSerialSeen = std::max(maxSerialSeen, m.serial);

                const bool isPractice = (m.flags & 1u) != 0;
                bool loadThis = true;

                if (!forceFullMigrationLoad) {
                    if (flt.practice.has_value() && flt.practice.value() != isPractice) loadThis = false;
                    if (flt.minPct.has_value() && m.startPercent < flt.minPct.value()) loadThis = false;
                    if (flt.maxPct.has_value() && m.startPercent > flt.maxPct.value()) loadThis = false;
                    if (loadThis && !isPractice && flt.maxNonPractice > 0
                        && nonPracticeLoaded >= flt.maxNonPractice) loadThis = false;
                }

                auto skipToEndOfChunk = [&]() {
                    auto now = in.tellg();
                    auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                    auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                };

                auto readFrames = [&](std::vector<Frame>& dst, uint32_t n) {
                    dst.resize(n);
                    for (uint32_t i = 0; i < n; ++i) {
                        APXFrame s{};
                        in.read(reinterpret_cast<char*>(&s), sizeof(s));

                        Frame f{};
                        f.x = s.x;
                        f.y = s.y;
                        f.rot = s.rot;
                        f.vehicleSize = s.vehicleSize;
                        f.waveSize = s.waveSize;
                        f.mode = static_cast<IconType>(s.mode);
                        f.t = s.t;
                        frameFromFlags_(s, f);

                        dst[i] = f;
                    }
                };

                if (!loadThis) {
                    skipToEndOfChunk();
                    continue;
                }

                Attempt a{};
                a.serial = static_cast<int>(m.serial);
                a.startPercent = m.startPercent;
                a.endPercent = m.endPercent;
                a.practiceAttempt = isPractice;
                a.completed = (m.flags & (1u << 1)) != 0;
                a.hadDual = (m.flags & (1u << 2)) != 0;
                a.startX = m.startX;
                a.startY = m.startY;
                a.baseTimeOffset = m.baseTimeOffset;
                a.seed = static_cast<uintptr_t>(m.seed);

                readFrames(a.p1, m.p1Count);
                readFrames(a.p2, m.p2Count);

                if (oldWavePointBitMeaning) {
                    clearAPXWavePointThisFrameBits(a);
                }

                if (!a.p1.empty()) {
                    a.startX = static_cast<float>(a.p1.front().x);
                    a.startY = static_cast<float>(a.p1.front().y);
                    a.endX = static_cast<float>(a.p1.back().x);
                } else if (!a.p2.empty()) {
                    a.startX = static_cast<float>(a.p2.front().x);
                    a.startY = static_cast<float>(a.p2.front().y);
                    a.endX = static_cast<float>(a.p2.back().x);
                }

                a.persistedOnDisk = true;
                a.recordedThisSession = false;

                m_loadedSerials.insert(a.serial);
                pushAttempt(std::move(a), false);

                ++loaded;
                if (!isPractice) ++nonPracticeLoaded;

                auto now = in.tellg();
                auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                if (remain > 0) in.seekg(remain, std::ios::cur);
            }
            else if (type == kChunk_PATH) {
                PracticePath loadedPath;

                if (readAPXPracticePath(in, sz, loadedPath)) {
                    m_checkpointMgr.restorePath(loadedPath);
                } else {
                    auto now = in.tellg();
                    auto readSoFar = static_cast<std::streamoff>(now - chunkStart);
                    auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                }
            }
            else if (type == kChunk_CheckpointZ2 || type == kChunk_SEGZ) {
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            } else {
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            }
        }

        m_loadedLevelID = levelID;

        if (m_nextAttemptSerial <= static_cast<int>(maxSerialSeen)) {
            m_nextAttemptSerial = static_cast<int>(maxSerialSeen) + 1;
        }

        log::info(
            "[APX load] Loaded {} attempts from {} (version {})",
            loaded,
            geode::utils::string::pathToString(path),
            h.version
        );

        log::info(
            "[APX load] migration={}",
            m_needsMigrationRewrite ? "yes" : "no"
        );

        in.close();

        if (m_needsMigrationRewrite) {
            if (oldWavePointBitMeaning) {
                log::info(
                    "[APX migration] clearing old wavePointThisFrame/sliding bit and rewriting {} to APX v{}",
                    geode::utils::string::pathToString(path),
                    kAPXVersion
                );
            } else {
                log::info(
                    "[APX migration] rewriting {} to APX v{}",
                    geode::utils::string::pathToString(path),
                    kAPXVersion
                );
            }

            if (!rewriteWholeFileToCurrent_(path)) {
                log::warn(
                    "[APX migration] failed to rewrite migration file: {}",
                    geode::utils::string::pathToString(path)
                );
            }
        }

        return static_cast<int>(loaded);
    }

    int loadAttemptsForLevel(int levelID, LoadFilter const& flt = {}) {
        return loadFromDisk_(levelID, flt);
    }

    int saveNewAttemptsForCurrentLevel() {
        if (!m_pl || !m_pl->m_level) return 0;
        return saveNewAttemptsForLevel_(m_levelIDOnAttach);
    }

    void setOpacityVar(GLubyte o) { opacity = o; }

    void refreshOpacityForAttemptNoWaveTrail(Attempt& a) {
        if (a.g1) a.g1->setOpacity(a.opacity);
        if (a.g2) a.g2->setOpacity(a.opacity);
    }

    void updateOpacityForAttempt(Attempt& a, GLubyte o, bool force=false) {
        m_skipGhostWork = (o == 0);
        if (!force && a.opacity == o) return;
        auto applyAll = [&](PlayerObject* po){
                if (!po) return;
                po->setOpacity(o);
                applyOpacityTree(po, o);
                if (po->m_waveTrail) {
                    po->m_waveTrail->setOpacity(
                        (GLubyte)std::clamp<int>(int(o * (m_waveTrailOpacityPct / 100.f)), 0, 255));
                    po->m_waveTrail->setVisible(o > 0);
                }
            };
        applyAll(a.g1);
        applyAll(a.g2);

        if (m_skipGhostWork) {
            if (a.g1) { a.setP1Visible(false); }
            if (a.g2) { a.setP2Visible(false); }
            a.primedP1 = a.primedP2 = false;
        }
        a.opacity = o;
    }

    void setOpacityAll(GLubyte o) {
        m_skipGhostWork = (o == 0);

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                updateOpacityForAttempt(attempts[idx], o);
            }
        }

        m_zeroOpacityLatched = m_skipGhostWork;
    }
    void updateOpacityAll() {
        m_skipGhostWork = (opacity == 0);

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                updateOpacityForAttempt(attempts[idx], opacity);
            }
        }

        m_zeroOpacityLatched = m_skipGhostWork;
    }


    void setBudgetPerFrame(int v) { m_budgetPerFrame = std::max(1, v); }
    void setMaxVisibleGhosts(int v) { m_maxVisibleGhosts = std::max(1, v); }
    void setRecordInPractice(bool v) { recordInPractice = v; }
    void setColorMode(ColorMode m) {
        colors = m;
        
        m_current.colorsAssigned = false;
        refreshAttemptColors_(m_current);
        
        for (auto& a : attempts) {
            a.colorsAssigned = false;
            if (a.g1 || a.g2) { 
                refreshAttemptColors_(a);
            }
        }
    }
    void updateAllColors() {
        m_current.colorsAssigned = false;
        refreshAttemptColors_(m_current);
        for (auto& a : attempts) {
            a.colorsAssigned = false;
            if (a.g1 || a.g2) refreshAttemptColors_(a);
        }
    }
    void setRecordingEnabled(bool on) { if (on != recording) toggleRecording(); }
    void setRecordingBlocked(bool on) { 
        recordingBlocked = on; 
        m_current = Attempt{};
        m_currentDidNoclip = false;
        restartLevel();
    }
    bool isRecordingBlocked() const { return recordingBlocked; }
    bool isRecordingBlockedOnNoclip() const { return m_recordingBlockedOnNoclip; }
    void setRecordingBlockedOnNoclip(bool on) { m_recordingBlockedOnNoclip = on; }
    bool isInterpolationEnabled() const { return m_useInterpolation; }
    void setInterpolationEnabled(bool on) { m_useInterpolation = on; }
    bool isGhostsExplodeEnabled() const { return m_ghostsExplode; }
    void setGhostsExplodeEnabled(bool on) { m_ghostsExplode = on; }
    PlayLayer* getPlayLayer() { return m_pl; }
    bool isGhostsExplodeSFXEnabled() const { return m_ghostsExplodeSFX; }
    void setGhostsExplodeSFXEnabled(bool on) { m_ghostsExplodeSFX = on; }
    bool getPlaybackCustomDeathSoundEnabled() const { return m_useCustomExplodeSounds; }
    void setPlaybackCustomDeathSoundEnabled(bool on) {
        Mod::get()->setSavedValue("custom-ghosts-explode-sfx", on);
        m_useCustomExplodeSounds = on;
        if (on) refreshAndPreloadCustomExplodeSfx_(true);
    }
    bool isRandomIconsEnabled() const { return m_randomIcons; }
    void setRandomIconsEnabled(bool on) {
        if (m_randomIcons == on) return;
        m_randomIcons = on;

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                auto& a = attempts[idx];
                if (a.g1) setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
                if (a.g2) setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
            }
        }
    }
    bool isReplayPreventCompletionEnabled() const { return m_pauseGameAtTheEndOfReplayBot; }
    void setReplayPreventCompletionEnabled(bool on) { m_pauseGameAtTheEndOfReplayBot= on; }

    bool practiceFilterEvenWhenBotNotOn() const {
        switch (m_replayKind) {
            case ReplayKind::PracticeComposite: return true;
            case ReplayKind::BestSingle: return false;
            default: return false;
        }
    }

    std::optional<bool> practiceFilterForGhosts_() const {
        if (botActive) {
            switch (m_replayKind) {
                case ReplayKind::PracticeComposite: return std::optional<bool>(true);
                case ReplayKind::BestSingle: return std::optional<bool>(false);
                default: return std::nullopt;
            }
        }

        if (playback) return std::nullopt;
        
        const bool inPractice = (m_pl && m_pl->m_isPracticeMode);
        return std::optional<bool>(inPractice);
    }

    void updateGhostVisibility() {
        if (opacity == 0) {
            for (size_t idx : m_primedIndices) {
                if (idx < attempts.size()) {
                    auto& a = attempts[idx];
                    if (a.g1) a.setP1Visible(false);
                    if (a.g2) a.setP2Visible(false);
                    a.primedP1 = a.primedP2 = false;
                }
            }
            return;
        }
    }

    void attach(PlayLayer* pl) {
        //log::info("[Ghosts] attach(entry)");

        setdisablePlayerMove(false);

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

        m_isTwoPlayer = m_pl && m_pl->m_levelSettings && m_pl->m_levelSettings->m_twoPlayerMode;
        // log::info("is two player mode: {}", m_isTwoPlayer);

        m_ghostRoot = cocos2d::CCNode::create();
        m_ghostRoot->ignoreAnchorPointForPosition(true);
        m_ghostRoot->setPosition({0, 0});
        
        cocos2d::CCNode* objectLayer = (m_pl && m_pl->m_player1) 
            ? m_pl->m_player1->getParent() : m_pl;
        
        if (objectLayer) {
            objectLayer->addChild(m_ghostRoot, 5);
        }
        
        m_ghostPool.initialize(pl, m_ghostRoot, 1);
        m_fmodEngine = FMODAudioEngine::sharedEngine();

        clearGhostBatches_();
        getOrCreateBatch_();

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

        int numRealPlayerObjects = getSettingIntOrDefault_(mod, "real-player-objects", 1000);

        m_playerObjectPool.init(&m_ghostPool, numRealPlayerObjects);

        m_currentOwner = nullptr;
        m_playbackArmed = false;
        botActive = false;
        m_didInitialWarp = false;
        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;
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
            m_loadedSerials.clear();
            clearAttemptCatalog_();
            invalidateAttemptPointerCaches_();
            recording = false;
            playback = false;
            //log::info("[Ghosts] attach() mod not enabled");
            return;
        }

        recordInPractice = getSettingBoolOrDefault_(mod, "record-in-practice", true);
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

        if (colorMode == "PlayerColors") colors = ColorMode::PlayerColors;
        else if (colorMode == "White") colors = ColorMode::White;
        else if (colorMode == "Black") colors = ColorMode::Black;
        else colors = ColorMode::Random;

        m_autosaveEnabled = getSettingBoolOrDefault_(mod, "autosave-enabled", true);
        m_autosaveMinutes = getSettingIntOrDefault_(mod, "autosave-minutes", 5);
        m_autosaveAccum = 0.f;

        m_allow_platformer = getSettingBoolOrDefault_(mod, "allow-platformer", true);

        
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

        //log::info("[Ghosts] attach: botActive={}, recording={}, playback={}", botActive, recording, playback);

        if (m_pl && m_pl->m_level) {
            m_levelIDOnAttach = m_pl->m_level->m_levelID;
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
    }

    void initBotAfterReset_() {
        if (!botActive || !m_pl || !m_pl->m_player1) return;

        m_replayIdx1 = m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;

        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;

        const Attempt* owner = nullptr;
        m_baseTime = getStartTime();
        
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                owner = ensureAttemptLoadedBySerial_(m_replayOwnerSerial, false);
            }

            if (!owner && m_compOwnerIdx < attempts.size()) {
                owner = &attempts[m_compOwnerIdx];
                m_replayOwnerSerial = owner->serial;
            }

            m_replayAttempt = nullptr;
        } else {
            int serial = m_checkpointMgr.findOwnerSerialForTime(m_baseTime);
            if (serial > 0) {
                owner = ensureAttemptLoadedBySerial_(serial, false);
                m_replayOwnerSerial = serial;
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

        const Frame& f1 = owner->p1.front();
        applyPoseHard(m_pl->m_player1, f1, m_pl);

        if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) {
            const Frame& f2 = owner->p2.front();
            applyPoseHard(m_pl->m_player2, f2, m_pl);
        }

        m_prevBotPx = f1.x;
        m_didInitialWarp = true;

        // Set starting time
        m_currentSessionTime = m_baseTime;
        m_prevSessionTime = m_baseTime;
        
        adjustReplayCursorToTime_(m_currentSessionTime);

        m_justStartedBot = false;
    }

    void onDeath() {
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

        if (recording && !recordingBlocked && !m_current.p1.empty() && !block_attempt_push_on_recording_start) {
            pushAttempt(std::move(m_current));
        }
    }

    void onReset() {
        m_justDied = false;
        if (block_attempt_push_on_recording_start) return;
        forceSetPosP1.x = 0.f;
        forceSetPosP1.y = 105.f;
        forceSetPosP2.x = 0.f;
        forceSetPosP2.y = 105.f;
        prevHold = false;

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
        if (recording && !recordingBlocked && !m_current.p1.empty()) {
            pushAttempt(std::move(m_current));
        }

        startNewAttempt();

        m_currentSessionTime = getStartTime();
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
            m_playerPrevTeleported = false;
            m_p2JustSpawned = false;
            m_prevHadP2 = false;
            // Check for start position switch
            if (ccpDistance(m_currentReplayStartPos, {(float)m_pl->m_player1->m_positionX, (float)m_pl->m_player1->m_positionY}) > kReplayStartTolerance) {
                if (!isRecording()) toggleRecording();
                setActiveGhostsInvisible();
            }
            else {
                m_justStartedBot = true;
                m_replayIdx1 = m_replayIdx2 = 0;
                m_lastEmitIdx1 = m_lastEmitIdx2 = 0;
                m_replayOwnerSerial = -1;
                initBotAfterReset_();
            }
        }

        if (botActive || showWhilePlaying) {
            invalidatePrimedPlayerObjectRefs();
            InitialGamemodeSet();
        }

        m_movementDirection = MovementDirection::Flat;

        // log::info("inUseCount: {}, activeAttempts: {}, currentlyHiddenSerials: {}", m_playerObjectPool.inUseCount(), m_playerObjectPool.getActiveAttempts().size(), m_currentlyHiddenSerials.size());
    }

    void invalidatePrimedPlayerObjectRefs() {
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
        m_currentlyHiddenSerials.clear();
    }

    void setActiveGhostsInvisible() {
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

    void onQuit() {
        m_is_quitting = true;

        death_sound_preloaded = false;

        m_current = Attempt{};
        
        flushPendingSaves_();
        
        botActive = false;
        playback = false;
        recording = false;
        m_hasWavePointData = false;
        m_filterByStartPosition = false;
        applyBotSafety_(false);

        m_playerPrevTeleported = false;
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
        
        if (m_ghostRoot) {
            //log::info("Removing ghost root from scene...");
            if (m_ghostRoot->getParent()) {
                m_ghostRoot->removeFromParent();
            }
            m_ghostRoot = nullptr;
        }

        attempts.clear();
        invalidateAttemptPointerCaches_();

        m_activeOwnerSerial = -1;
        m_didInitialWarp = false;
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;
        m_pl = nullptr;
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
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;
        m_lastGhostTickFrame = 0;

        m_ghostUpdateAccum = 0.f;
        m_loadedSerials.clear();
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
    }

    void onComplete() {
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

    void saveCurrentAttemptNow() {
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

    /*
    void toggleShowWhilePlaying() {
        showWhilePlaying = !showWhilePlaying;
        Mod::get()->setSettingValue("show-while-playing", showWhilePlaying);

        if (showWhilePlaying) {
            if (!botActive && !attempts.empty()) {
                startPlayback();
            } else {
                updateGhostVisibility();
            }
        } else {
            if (!botActive && playback) {
                stopPlayback();
            } else {
                updateGhostVisibility();
            }
        }
    }*/

    void toggleRecording() {
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
                block_attempt_push_on_recording_start = true;
                restartLevel();
                block_attempt_push_on_recording_start = false;
            }
        }

        updateGhostVisibility();
    }
    bool isRecording() const { return recording; }
    bool getShowWhilePlaying() const { return showWhilePlaying; }

    bool isPlaybackEnabled() const { return playback; }
    void startPlayback()   {
        if (botActive) stopReplay();
        playback = true;
        m_playbackArmed = true;
        m_playbackStartTick = m_frameCounter;
        recording = false;
        updateGhostVisibility(); 
        for (auto& a : attempts) a.resetPlayback();
    }
    void stopPlayback() {
        playback = false;
        updateGhostVisibility();
    }

    void clearGhosts() {
        clearAllGhostNodes();
        attempts.clear();
        m_loadedSerials.clear();
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
    void showBestGhostOnly() {
        if (attempts.empty()) return;
        onlyBestGhost = true;
        sortBestToFront();
        updateGhostVisibility();
    }
    void showAllGhosts() {
        onlyBestGhost = false;
        updateGhostVisibility();
    }

    static FORCE_INLINE void applyPoseHard(PlayerObject* po, const Frame& f, PlayLayer* pl) {
        if (!po) return;

        const float vehicleSize = f.vehicleSize;
        const float rot = f.rot;
        const float x = f.x;
        const float y = f.y;

        syncGhostGravity(po, f.upsideDown, pl);
        if (po->m_vehicleSize != vehicleSize) {
            po->m_vehicleSize = vehicleSize;
            po->updatePlayerScale();
        }
        po->setRotation(rot);
        po->setPosition({ x, y });
    }

    void buildCompositeFromAttempt(size_t idx) {
        m_activeRoute = ActiveRoute::SingleAttempt;
        m_replayAttempt = nullptr;

        if (idx >= attempts.size()) {
            m_compOwnerIdx = SIZE_MAX;
            return;
        }

        auto& a = attempts[idx];

        m_compOwnerIdx = idx;
        m_replayOwnerSerial = a.serial;
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;
        m_replayIdx1 = m_replayIdx2 = 0;

        if (botActive) {
            tagCompositeOwnersHidden_(true);
        }
    }
    
    const Attempt* findBestAttemptForRange_(float x0, float x1, 
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

    void syncInputsToCurrentIndexP1_() {
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

    void syncInputsToCurrentIndexP2_() {
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

    bool startReplayPractice() {
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

        if (isPracticeMode()) togglePracticeMode(false);

        block_attempt_push_on_recording_start = true;
        restartLevel();
        block_attempt_push_on_recording_start = false;

        m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
        m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;

        const int selectedSession = m_checkpointMgr.selectBestSessionForStartX_RankByTime(
            m_currentReplayStartPos.x,
            kReplayStartTolerance
        );

        if (selectedSession <= 0 || m_checkpointMgr.noValidSessionForStartX()) {
            log::warn(
                "[Bot] startReplayPractice: no valid session for startX={}",
                m_currentReplayStartPos.x
            );
            m_filterByStartPosition = false;
            stopReplay();
            return false;
        }

        m_replayKind = ReplayKind::PracticeComposite;

        //buildCompositePracticePath(std::optional<bool>(true));

        if (m_baseTime != 0.0) {
            m_checkpointMgr.offsetSelectedSessionBaseTimeIfZero(m_baseTime);
        }

        m_replayOwnerSerial = m_checkpointMgr.findOwnerSerialForTime(m_baseTime);
        m_activeOwnerSerial = m_replayOwnerSerial > 0 ? m_replayOwnerSerial : -1;
        if (m_activeOwnerSerial > 0) {
            Attempt* chosen = ensureAttemptLoadedBySerial_(m_activeOwnerSerial, false);
            // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
            if (m_baseTime != 0.f && chosen->baseTimeOffset == 0.f) {
                chosen->baseTimeOffset = m_baseTime;
                const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
                offsetAttemptTimesFast(*chosen, offsetQ);
            }
        }

        rebuildRouteOwnerCache_();
        rebuildRouteOwnerCache_();
        
        std::vector<PracticeSegment> replaySegs = m_checkpointMgr.getReplaySequence();
        if (replaySegs.empty()) {
            log::warn("[Bot] startReplayPractice: no replay segments");
            m_filterByStartPosition = false;
            stopReplay();
            return false;
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
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;

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
                Attempt* tailOwner = findAttemptBySerial_(tailSerial);
                // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
                if (m_baseTime != 0.f && tailOwner->baseTimeOffset == 0.f) {
                    tailOwner->baseTimeOffset = m_baseTime;
                    const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
                    offsetAttemptTimesFast(*tailOwner, offsetQ);
                }
                if (tailOwner && !tailOwner->p1.empty()) {
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
        
        restartLevel();
        return true;
    }


    bool startReplayBest() {
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

        block_attempt_push_on_recording_start = true;
        botActive = false;
        playback = false;
        
        restartLevel();

        m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
        m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;

        m_filterByStartPosition = true;
        
        block_attempt_push_on_recording_start = false;
        
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

        const int serial = pickWinningOrBestSerialFromPosition_(
            std::optional<bool>(false),
            m_currentReplayStartPos.x,
            m_currentReplayStartPos.y,
            kReplayStartTolerance
        );

        if (serial <= 0) {
            log::warn("[Bot] startReplayBest: no non-practice attempts to replay for current start pos");
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        Attempt* chosen = ensureAttemptLoadedBySerial_(serial, false);
        if (!chosen || chosen->p1.empty()) {
            log::warn("[Bot] startReplayBest: failed to load chosen attempt serial={}", serial);
            m_replayOwnerSerial = -1;
            m_replayOwnerIndex = -1;
            m_currentOwner = nullptr;
            playback = false;
            stopReplay();
            return false;
        }

        // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
        if (m_baseTime != 0.f && chosen->baseTimeOffset == 0.f) {
            chosen->baseTimeOffset = m_baseTime;
            const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
            offsetAttemptTimesFast(*chosen, offsetQ);
        }

        const size_t idx = findLoadedAttemptIndexBySerial_(serial);
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
            log::warn("[Bot] startReplayBest: no data (p1 empty) after filtering. m_compOwnerIdx={}", m_compOwnerIdx);
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
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;

        forcePlayersVisible_();

        updateGhostVisibility();
        m_justStartedBot = true;

        //log::info("[Bot] startReplayBest: calling final resetLevel()");
        restartLevel();
        //log::info("[Bot] startReplayBest: complete, botActive={}", botActive);
        return true;
    }

    void stopReplay() {
        if (!botActive) {
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
        m_playerPrevTeleported = false;
        invalidatePrimedPlayerObjectRefs();
        m_initialAttemptsToSet.clear();
        m_preloadOrder.clear();
        m_1PreloadedSoDontShowGhosts = false;
    }

    void restartLevel() { 
        if (m_pl) {
            m_blockAttemptCount = true;
            m_pl->resetLevel(); 
            m_blockAttemptCount = false;
        }
    }

    bool isBotActive() const { return botActive; }
    bool isReplaying() const { return playback; }
    bool isResetting() const { return resetting; }
    void setResetting(bool on) { resetting = on; }
    bool isdisablePlayerMove() const { return disablePlayerMove && m_setRealPlayerPosition; }
    void setdisablePlayerMove(bool on) { disablePlayerMove = on; }
    bool shouldDisableHardStreakAddPoint() const { return !m_allowWavePointAdding && botActive && m_hasWavePointData; }
    void setAllowWavePointAdding(bool on ) { m_allowWavePointAdding = on; }
    bool skipHardStreakCheck() const { return m_skipHardStreakCheck; }

    double getStartTime() {
        if (isPracticeMode() && m_pl->getLastCheckpoint()) {
            return m_pl->getLastCheckpoint()->m_gameState.m_levelTime;
        }
        else {
            auto gl = GJBaseGameLayer::get();
            if (gl) return gl->m_gameState.m_levelTime;
        }
        return 0.f;
    }

    void togglePracticeMode(bool enabled) {
        if (!m_pl) return;
        m_pl->togglePracticeMode(enabled);
    }

    bool isPracticeMode() {
        if (!m_pl) return false;
        return m_pl->m_isPracticeMode;
    }

    void onPracticeToggle(bool enabled) {
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

    void maybeInitialWarpToReplayStart_() {
        if (!m_pl || !m_pl->m_player1) return;
        if (m_replayKind != ReplayKind::PracticeComposite) return;
        if (m_didInitialWarp) return;

        // Use time 0 to get the first owner
        const Attempt* owner = getReplayOwner_(getStartTime());
        if (!owner || owner->p1.empty()) return;

        const float px = m_pl->m_player1->getPositionX();
        const float f1x = owner->p1.front().x;

        if (px + 5.f >= f1x) return;

        forceMode(m_pl->m_player1, IconType::Cube);
        if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) 
            forceMode(m_pl->m_player2, IconType::Cube);

        const Frame& f = owner->p1.front();
        m_pl->m_player1->setPosition({ f.x, f.y });
        m_pl->m_player1->setRotation(f.rot);

        if (m_pl->m_player2 && owner->hadDual && !owner->p2.empty()) {
            const Frame& f2 = owner->p2.front();
            m_pl->m_player2->setPosition({ f2.x, f2.y });
            m_pl->m_player2->setRotation(f2.rot);
            forceMode(m_pl->m_player2, f2.mode);
            syncGhostGravity(m_pl->m_player2, f2.upsideDown, m_pl);
        }

        forceMode(m_pl->m_player1, f.mode);
        syncGhostGravity(m_pl->m_player1, f.upsideDown, m_pl);

        m_didInitialWarp = true;
    }

    bool refreshCurrentOwnerPointer_() {
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                if (const Attempt* bySerial = findLoadedAttemptBySerialOnly_(m_replayOwnerSerial)) {
                    m_currentOwner = bySerial;
                    return true;
                }

                m_currentOwner = ensureAttemptLoadedBySerial_(m_replayOwnerSerial, false);
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
            if (const Attempt* bySerial = findLoadedAttemptBySerialOnly_(m_replayOwnerSerial)) {
                m_currentOwner = bySerial;
                return true;
            }

            m_currentOwner = ensureAttemptLoadedBySerial_(m_replayOwnerSerial, false);
            return m_currentOwner != nullptr;
        }

        m_currentOwner = nullptr;
        return false;
    }

    void updateClickState(bool isPlayer1) {
        if (!botActive) return;
        if (!m_pl || m_is_quitting) return;
        refreshCurrentOwnerPointer_();
        if (!m_allowSetPlayerClickState) return;
        if (!m_currentOwner || m_currentOwner->p1.empty()) return;

        if (m_justStartedBot) {
            m_justStartedBot = false;
        }

        if (isPlayer1) syncInputsToCurrentIndexP1_();
        else if (m_isTwoPlayer) syncInputsToCurrentIndexP2_();
        else m_lastEmitIdx2 = m_replayIdx2;

        m_allowSetPlayerClickState = false;

    }

    void preUpdateP2() {
        // if (!m_allowWorkThisTick || m_is_quitting || !m_pl) return;
        if (m_is_quitting || !m_pl || !m_pl->m_player2) return;
        m_prevpx2 = m_px2;
        const float p2pos = m_pl->m_player2->getPositionX();
        const bool hasP2Now = (p2pos != 0.f);
        m_px2 = hasP2Now ? p2pos : m_px;
        if (hasP2Now && !m_prevHadP2) m_p2JustSpawned = true;
        // m_p2JustSpawned = (hasP2Now && !m_prevHadP2); Now set this in applyFrameToPlayer_Only_
        m_prevHadP2 = hasP2Now;
    }

    void preUpdate() {
        //log::info("Preupdate");
        // log::info("didAttemptUseNoclip: {}", didAttemptUseNoclip());
        // if (!m_allowWorkThisTick || m_is_quitting || !m_pl) return;
        if (m_is_quitting || !m_pl || !m_pl->m_player1) return;

        m_px = m_pl->m_player1->getPositionX();
        m_currentSessionTime = m_baseTime + m_pl->m_attemptTime;
        updateCurrentAttemptNoclipState();

        if (!botActive) return;

        // Check when not dual
        if (m_prevHadP2) {
            float p2pos = 0.f;
            if (m_pl->m_player2) p2pos = m_pl->m_player2->getPositionX();
            if (p2pos == 0.f) m_prevHadP2 = false;
            // log::info("p2 pos: {}", p2pos);
        }
        else m_px2 = m_px;
        
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
        
        if (newOwnerSerial > 0 && previousOwnerSerial != newOwnerSerial && 
            m_replayKind == ReplayKind::PracticeComposite) {
            
            if (previousOwnerSerial > 0) {
                m_currentlyHiddenSerials.erase(previousOwnerSerial);
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

    void checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah() {
        if (m_replayKind == ReplayKind::PracticeComposite) finalTime = m_replayEndTime;
        else finalTime = m_currentOwner->p1.back().t;

        if (finalTime <= 0.1) {
            return;
        }

        if (!m_freezePlayerXAtEnd && m_currentSessionTime >= finalTime) {
            m_freezePlayerXAtEnd = true;
            if (m_currentOwner->completed) return;
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
        }

        if (m_freezePlayerXAtEnd && !m_currentOwner->completed) {
            m_pl->m_player1->setPosition({m_freezePlayerX, m_freezePlayerY});
            m_pl->m_player1->setRotation(m_freezePlayerRotation);
            if (m_pl->m_player2 && m_pl->m_player2->isVisible()) {
                m_pl->m_player2->setPosition({m_freezePlayerXP2, m_freezePlayerYP2});
                m_pl->m_player2->setRotation(m_freezePlayerRotationP2);
            }
                
        }
    }

    void updateFreezePlayerX_() {
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

    void applySegmentBasedReplay_(bool isPlayer1) {
        if (!botActive) return;
        if (!m_allowSetPlayerPos) return;
        if (!m_pl || m_is_quitting) return;

        const float px = playerX_();
        const float px2 = playerX2_();

        // geode::log::warn("[applySegmentBasedReplay_] updateFreezeX");
        
        if (m_replayKind == ReplayKind::BestSingle) {
            // geode::log::warn("[applySegmentBasedReplay_] ReplayKind::BestSingle");
            if (m_compOwnerIdx < attempts.size()) {
                m_currentOwner = &attempts[m_compOwnerIdx];
            }
            if (!m_currentOwner || m_currentOwner->p1.empty()) {
                // geode::log::warn("[applySegmentBasedReplay_] No owner");
                return;
            }
            
            // Use the maintained index instead of recalculating
            size_t idx1 = m_replayIdx1 + 2;
            if (idx1 >= m_currentOwner->p1.size()) idx1 = m_currentOwner->p1.size() - 1;
            
            if (isPlayer1) applyFrameToPlayerOverRange(m_pl->m_player1, m_currentOwner->p1, idx1, m_currentSessionTime, true);
            else {
                if (m_currentOwner->hadDual && m_pl->m_player2 && !m_currentOwner->p2.empty()) {
                    size_t idx2 = m_replayIdx2 + 2;
                    if (idx2 >= m_currentOwner->p2.size()) idx2 = m_currentOwner->p2.size() - 1;
                    applyFrameToPlayerOverRange(m_pl->m_player2, m_currentOwner->p2, idx2, m_currentSessionTime, false);
                }
            }
            checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah();
            return;
        }

        // geode::log::warn("[applySegmentBasedReplay_] (!m_currentOwner) {}", (!m_currentOwner));

        if (!m_currentOwner) {
            geode::log::warn("NO CURRENT OWNER");
            return;
        }

        // geode::log::warn("[applySegmentBasedReplay_] (m_currentOwner->serial <= 0) {}", (m_currentOwner->serial <= 0));
        if (m_currentOwner->serial <= 0) {
            geode::log::warn("NEGATIVE OWNER SERIAL");
            return;
        }

        // geode::log::warn("[applySegmentBasedReplay_] (m_currentOwner->p1.empty()) {}", (m_currentOwner->p1.empty()));
        if (m_currentOwner->p1.empty()) {
            geode::log::warn("EMPTY OWNER DATA");
            return;
        }

        // Use maintained indices for PracticeComposite as well
        size_t idx1 = m_replayIdx1 + 2;
        if (idx1 >= m_currentOwner->p1.size()) idx1 = m_currentOwner->p1.size() - 1;
        
        if (isPlayer1) applyFrameToPlayerOverRange(m_pl->m_player1, m_currentOwner->p1, idx1, m_currentSessionTime, true);
        else {
            if (m_currentOwner->hadDual && m_pl->m_player2 && !m_currentOwner->p2.empty()) {
                size_t idx2 = m_replayIdx2 + 2;
                if (idx2 >= m_currentOwner->p2.size()) idx2 = m_currentOwner->p2.size() - 1;
                applyFrameToPlayerOverRange(m_pl->m_player2, m_currentOwner->p2, idx2, m_currentSessionTime, false);
            }
        }

        checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah();
    }

    void recordUpdate(bool isPlayer1) {
        // if (!shouldRunWorkThisTick_()) return;
        if (!m_pl || m_is_quitting) return;
        const bool isPractice = m_pl->m_isPracticeMode;
        if (isPractice && !recordInPractice) return;
        if (!recording || recordingBlocked) return;

        recordCurrentFrame_(isPlayer1);
    }

    bool removeReverseOrDuplicateFrames(std::vector<Frame>& frames, double t) {
        // Reverse frames
        while (!frames.empty() && t < frames.back().t) {
            frames.pop_back();
        }

        // Duplicate frame
        if (!frames.empty() && t == frames.back().t) return false;

        return true;
    }
    

    void recordCurrentFrame_(bool isPlayer1) {
        if (m_justDied) return;
        
        const double absoluteTime = m_pl->m_attemptTime + m_current.baseTimeOffset;
        
        if (m_pl->m_player1 && isPlayer1) {
            if (!removeReverseOrDuplicateFrames(m_current.p1, absoluteTime)) return;

            auto* p1 = m_pl->m_player1;
            Frame f;
            f.t = absoluteTime;
            cocos2d::CCPoint pos = p1->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p1->getRotation();
            f.vehicleSize = p1->m_vehicleSize;
            f.waveSize = p1->m_waveTrail ? p1->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p1->m_isUpsideDown;
            f.isDashing = p1->m_isDashing;
            f.isVisible = p1->isVisible();
            f.wavePointThisFrame = p1WavePointThisFrame;

            // Moved to runtime to reduce recording delay
            //if (!m_current.p1.empty()) {
            //    const float prevY = static_cast<float>(m_current.p1.back().y);
            //    f.stateDartSlide = std::fabs(static_cast<float>(f.y) - prevY) <= kYEqualEps;
            //} else {
            //    f.stateDartSlide = false;
            //}

            f.mode = currentMode(p1, m_pl->m_isPlatformer);
            f.hold = p1Hold;
            f.holdL = p1LHold;
            f.holdR = p1RHold;

            m_current.p1.push_back(f);
            m_lastRecordedX = static_cast<float>(f.x);
            const float currentPercent = m_pl->getCurrentPercent();
            if (currentPercent > m_current.endPercent) m_current.endPercent = currentPercent;
            p1WavePointThisFrame = false;
        }
        
        if (m_pl->m_player2 && m_prevHadP2 && !isPlayer1) {
            if (!removeReverseOrDuplicateFrames(m_current.p2, absoluteTime)) return;
            m_current.hadDual = true;
            auto* p2 = m_pl->m_player2;
            Frame f;
            f.t = absoluteTime;
            cocos2d::CCPoint pos = p2->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p2->getRotation();
            f.vehicleSize = p2->m_vehicleSize;
            f.waveSize = p2->m_waveTrail ? p2->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p2->m_isUpsideDown;
            f.isDashing = p2->m_isDashing;
            f.isVisible = p2->isVisible();
            f.wavePointThisFrame = p2WavePointThisFrame;

            // Moved to runtime to reduce recording delay
            //if (!m_current.p2.empty()) {
            //    const float prevY = static_cast<float>(m_current.p2.back().y);
            //    f.stateDartSlide = std::fabs(static_cast<float>(f.y) - prevY) <= kYEqualEps;
            //} else {
            //    f.stateDartSlide = false;
            //}

            f.mode = currentMode(p2, m_pl->m_isPlatformer);
            f.hold = p2Hold;
            f.holdL = p2LHold;
            f.holdR = p2RHold;
            m_current.p2.push_back(f);
            p2WavePointThisFrame = false;
        }
    }
    
    bool isOwnerGhost_(size_t ai, Attempt const& a) const {
        if (!botActive) return false;
        
        if (a.serial == m_activeOwnerSerial)
            return true;
        
        if (!m_useCheckpointsRoute && ai == m_compOwnerIdx)
            return true;
        
        return false;
    }

    void postUpdate() {
        if (!modEnabled || !m_pl || m_is_quitting) { 
            ++m_frameCounter; 
            return; 
        }

        //double tStart = getTimeMs();

        //log::error("[postUpdate] playerx: {}", m_pl->m_player1->m_positionX);

        const double dt = m_currentSessionTime - m_prevSessionTime;
        m_prevSessionTime = m_currentSessionTime;

        if (isPureRecordingMode_() || isPreloadingAttempts()) {
            ++m_frameCounter;
            if (m_autosaveEnabled && m_levelIDOnAttach != 0) {
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
                
                const float px = playerX_();
                const float px2 = playerX2_();

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
                int currentPreloaded = 0;
                
                for (auto it = m_wantToPrimeIndices.begin(); 
                    it != m_wantToPrimeIndices.end(); ) {
                    if (currentPreloaded >= maxPreloadPerFrame) break;
                    if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) {
                        break;
                    }

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
                    it = m_wantToPrimeIndices.erase(it);

                }

                // Main candidate processing loop
                for (size_t cidx = 0; cidx < candidateCount; ++cidx) {
                    const size_t ai = m_cachedCandidates[cidx];
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
                        // remove on death (if ghosts explode, meaning they disapear so I can remove them early)
                        if (m_ghostsExplode) {
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
        if (m_autosaveEnabled && m_levelIDOnAttach != 0) {
            m_autosaveAccum += static_cast<float>(dt);
            const float every = std::max(1, m_autosaveMinutes) * 60.0f;
            if (m_autosaveAccum >= every) {
                m_autosaveAccum = 0.f;
                saveNewAttemptsForCurrentLevel();
            }
        }
    }

    void setGhostUpdateFPS(int fps) {
        fps = std::max(1, std::min(480, fps));
        m_ghostUpdateFPS = fps;
        m_ghostUpdateInterval = 1.f / static_cast<float>(fps);
        m_ghostUpdateAccum = 0.f;
    }

private:
    bool modEnabled = true;
    bool m_is_quitting = false;
    bool m_blockAttemptCount = false;
    bool death_sound_preloaded = false;
    bool m_p2JustSpawned = false;
    bool m_prevHadP2 = false;
    bool m_ghostsExplode = false;
    bool m_ghostsExplodeSFX = false;
    bool m_randomIcons = true;
    bool m_IHateMirrorPortalsSoRewind = false;
    bool m_isTwoPlayer = false;
    float m_px = 0.f;
    float m_px2 = 0.f;
    float m_prevpx2 = 0.f;
    bool m_p2Moving = false;
    double m_currentSessionTime = 0.0;
    double m_prevSessionTime = 0.0;
    double m_baseTime = 0.0f;
    double m_offsetUnitsPerSecond = 311.f;
    bool m_1PreloadedSoDontShowGhosts = false;
    bool m_hasWavePointData = false;
    bool m_allowWavePointAdding = false;
    bool m_skipHardStreakCheck = false;
    MovementDirection m_movementDirection = MovementDirection::Flat;
    bool prevHold = false;

    double finalTime = 0.f;

    bool overrideWaveSize = true;

    inline static SfxLimiter g_explodeLimiter{20};
    FMODAudioEngine* m_fmodEngine = nullptr;
    bool m_useCustomExplodeSounds = false;

    PlayLayer* m_pl = nullptr;

    bool m_justDied = false;
    const Attempt* m_currentOwner = nullptr;

    bool m_skipGhostWork = false;
    bool m_zeroOpacityLatched = false;

    size_t m_lastGhostTickFrame = 0;
    size_t m_speedUpCameraAnimationAfterTeleportForNFrames = 0;
    size_t m_fixWaveTrailAfterSpawnP2ForNFrames = 0;

    std::vector<Attempt> attempts;
    Attempt m_current;
    bool m_currentDidNoclip = false;

    bool botActive = false;
    bool resetting = false;
    bool disablePlayerMove = false;
    bool m_didInitialWarp = false;
    bool m_setRealPlayerPosition = true;
    bool botPrevHold1 = false;
    bool botPrevHold2 = false;
    bool botPrevHoldL1 = false;
    bool botPrevHoldL2 = false;
    bool botPrevHoldR1 = false;
    bool botPrevHoldR2 = false;

    float  m_prevBotPx = 0.f;
    size_t m_replayIdx1 = 0; // current index in replay owner's p1
    size_t m_replayIdx2 = 0; // current index in replay owner's p2
    int m_replayOwnerSerial = -1; // serial of current replay owner
    size_t m_replayOwnerIndex = -1;
    const Attempt* m_replayAttempt = nullptr; // cached pointer to replay attempt (for BestSingle)

    size_t m_lastEmitIdx1 = 0;
    size_t m_lastEmitIdx2 = 0;

    bool recording = true;
    bool recordingBlocked = false;
    bool m_recordingBlockedOnNoclip = false;
    bool playback = false;
    bool m_playbackArmed = false;
    bool block_attempt_push_on_recording_start = false;

    bool m_useInterpolation = true;
    bool m_animateRobot = true;
    bool m_animateSpider = true;
    bool m_pauseGameAtTheEndOfReplayBot = false;

    int m_ghostUpdateFPS = 480;
    float m_ghostUpdateInterval = 1.f / 480.f;
    float m_ghostUpdateAccum = 0.f;

    bool p1Hold = false;
    bool p2Hold = false;
    bool p1LHold = false;
    bool p2LHold = false;
    bool p1RHold = false;
    bool p2RHold = false;
    bool p1WavePointThisFrame = false;
    bool p2WavePointThisFrame = false;

    size_t m_frameCounter = 0;
    size_t m_currentAttemptStart = 0;
    size_t m_playbackStartTick = 0;

    std::vector<size_t> practiceAttemptReplayIndex;

    bool m_prevPractice = false;
    bool m_pendingRearm = false;

    int m_checkpointTopId = -1;
    int m_nextCheckpointId = 1;
    int m_nextAttemptSerial = 1;
    int m_lastWinSerial = -1;

    int m_frozenCheckpointTopId = -1;

    static constexpr float kCheckpointEpsilon = 1.0f;

    std::vector<int> m_checkpointUndoStack;

    float m_lastRecordedX = 0.f;

    int m_lastLoggedOwnerIdx = -2;


    bool m_limitVisibleGhosts = false;
    int m_maxVisibleGhosts = 10000;
    bool m_onlyShowGhostsThatPassedPercent = false;
    float m_GhostsPassedPercentThreshold = 20;
    std::vector<uint32_t> m_ghostsThatPassThePercentThreshold;

    int m_budgetPerFrame = 1000;
    
    size_t m_lastAttachCheck = 0;

    std::deque<size_t> m_preloadQ;
    std::vector<uint8_t> m_isPreloaded;

    size_t m_compOwnerIdx = SIZE_MAX;

    int m_rrPhase = 0;

    std::vector<AttemptSpan> m_spans;

    XBatchGrid m_grid;
    XBatchGrid m_gridThreshold;
    std::vector<uint32_t> m_cachedCandidates;
    int m_cachedBinL = INT_MAX;
    int m_cachedBinR = INT_MIN;

    size_t m_nullOwnerSpanSkips = 0;

    std::vector<FrozenSeg> m_frozenSegs;
    bool m_pathFrozen = false;
    bool m_allowCrossModeOwners = true;

    mutable std::filesystem::path m_attemptsDir;
    bool m_autosaveEnabled = true;
    bool m_isSaving = false;
    int m_autosaveMinutes = 5;
    float m_autosaveAccum = 0.f;
    std::unordered_set<int> m_loadedSerials;
    int m_loadedLevelID = 0;
    bool m_needsMigrationRewrite = false;
    bool m_loadedFileWasLegacyAttempts = false;
    std::vector<APXAttemptDiskInfo> m_attemptCatalog;
    std::unordered_map<int, size_t> m_attemptCatalogBySerial;
    int m_attemptCatalogLevelID = 0;
    bool m_attemptCatalogScanned = false;

    bool m_allow_platformer = false;

    static constexpr int kPerfBudgetInfiniteSentinel = 1000;
    bool m_perfUnlimited = false;

    int m_randomDistPx = 0;
    int m_randomDistPxOld = 0;
    size_t m_lastGhostDistPoll = 0;

    bool m_justStartedBot = false;
    cocos2d::CCPoint m_currentReplayStartPos{0.f, 0.f};

    bool m_chainAuthoritative = false;
    bool m_checkpointDirty = false;

    bool m_useCheckpointsRoute = true;
    ReplayKind m_replayKind = ReplayKind::BestSingle;

    enum class ActiveRoute { CompositeCheckpoint, SingleAttempt };
    ActiveRoute m_activeRoute = ActiveRoute::CompositeCheckpoint;
    bool routeIsComposite_() const { return m_replayKind == ReplayKind::PracticeComposite; }
    int m_activeOwnerSerial = -1;
    int m_previousActiveOwnerSerial = -1;

    bool m_freezePlayerXAtEnd = false;
    float m_freezePlayerX = 0.f;
    float m_freezePlayerY = 0.f;
    float m_freezePlayerXP2 = 0.f;
    float m_freezePlayerYP2 = 0.f;
    double m_replayEndTime = 0.f;
    float m_freezePlayerRotation = 0.f;
    float m_freezePlayerRotationP2 = 0.f;

    int m_practiceCompositeOwnerSerial = -1;
    std::string m_customSaveId;

    mutable bool m_attemptCountsDirty = true;
    mutable int m_cachedNormalAttempts = 0;
    mutable int m_cachedPracticeAttempts = 0;
    mutable int m_attemptsPreloadedTotal = 0;

    std::unordered_set<int> m_practiceSerialSet;
    std::vector<int> m_preloadableIndexesInAttemptsList;
    std::vector<uint32_t> m_preloadableSerials;

    geode::Ref<cocos2d::CCNode> m_ghostRoot = nullptr;

    std::unordered_set<int> m_routeOwnerSerials;
    std::unordered_set<int> m_currentlyHiddenSerials;
    std::vector<uint8_t> m_routeOwnerMask;
    bool m_hideAllOwnersDuringPracticeReplay = true;

    CheckpointManager m_checkpointMgr;

    mutable std::unordered_map<int, size_t> m_serialToIdx;
    mutable bool m_serialCacheDirty = true;

    static constexpr float kReplayStartTolerance = 30.0f;
    static constexpr float kTolSq = kReplayStartTolerance * kReplayStartTolerance;
    static constexpr float kWaveTeleportedTolerance = 30.0f;
    static constexpr float kNoDualTimeGap = 0.1f;
    static constexpr float kYEqualEps = 0.1f;
    static constexpr double kP1ContinuesPastP2Threshold = 0.5;
    bool m_playerPrevTeleported = false;
    bool m_filterByStartPosition = false;

    std::vector<uint32_t> m_preloadedIndices;
    std::unordered_set<size_t> m_preloadedSet;
    std::vector<size_t> m_primedIndices;
    std::unordered_set<size_t> m_primedSet;
    std::vector<size_t> m_wantToPrimeIndices;
    std::unordered_set<size_t> m_wantToPrimeSet;

    static constexpr size_t kGhostBatchSize = 500;
    std::vector<GhostBatch> m_ghostBatches;
    size_t m_currentBatchIdx = 0;

    bool m_isInPureRecordingMode = false; // when recording without bot/playback

    ProfiledGhostPool m_ghostPool;
    GhostContainer* m_ghostContainer = nullptr;
    GhostContainer* m_trailContainer = nullptr;
    PlayerObjectPool m_playerObjectPool;

    bool m_allowWorkThisTick = false;
    bool m_allowSetPlayerPos = false;
    bool m_allowSetPlayerClickState = false;
    uint64_t m_tickId = 0;
    uint64_t m_lastWorkTickId = 0;

    // Cache the directory scan (updates when folder changes)
    std::vector<std::string> customSfx;
    std::filesystem::file_time_type customLastWrite{};
    uint64_t m_lastCustomSfxRefreshTick = 0;

    static constexpr uint64_t kCustomSfxRefreshIntervalTicks = 120;

    void refreshAndPreloadCustomExplodeSfx_(bool force = false) {
        if (!m_useCustomExplodeSounds)
            return;

        if (!force) {
            const uint64_t elapsed = m_tickId - m_lastCustomSfxRefreshTick;
            if (elapsed < kCustomSfxRefreshIntervalTicks)
                return;
        }

        m_lastCustomSfxRefreshTick = m_tickId;

        if (force) {
            customLastWrite = {};
        }

        refreshCustomExplodeSfx_(customSfx, customLastWrite);

        if (!customSfx.empty()) {
            g_explodeLimiter.preloadAll(customSfx);
        }
    }

    void invalidateAttemptPointerCaches_() {
        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerIndex = SIZE_MAX;
        m_compOwnerIdx = SIZE_MAX;
        m_serialCacheDirty = true;
    }

    static inline float lerp_(float a, float b, float t) {
        return a + (b - a) * t;
    }
    static inline double clamp01_(double x) {
        return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
    }
    static inline float lerpAngleDeg_(float a, float b, float t) {
        // shortest-path degrees (handles wraparound cleanly)
        float d = std::fmod(b - a, 360.f);
        if (d > 180.f) d -= 360.f;
        if (d < -180.f) d += 360.f;
        return a + d * t;
    }


    FORCE_INLINE bool isPureRecordingMode_() const {
        return recording && !botActive && !playback && !showWhilePlaying;
    }

    bool matchesStartPositionFilter_(const Attempt& a) const {
        if (!m_filterByStartPosition || a.m_isPlatformer || a.practiceAttempt) return true;
        if (a.p1.empty()) return false;

        // L bozo platformer mode this is being wacky for initial teleports in levels due to y offset
        
        float attemptStartX = a.p1.front().x;
        float attemptStartY = a.startY;

        float dx = m_currentReplayStartPos.x - attemptStartX;
        float dy = m_currentReplayStartPos.y - attemptStartY;
        
        return ((dx * dx + dy * dy) < kTolSq);

        // return std::fabs(attemptStartX - m_currentReplayStartPos.x) <= kReplayStartTolerance;
    }

    void clearAttemptCatalog_() {
        m_attemptCatalog.clear();
        m_attemptCatalogBySerial.clear();
        m_attemptCatalogLevelID = 0;
        m_attemptCatalogScanned = false;
    }

    bool scanAttemptCatalogForLevel_(int levelID) {
        if (m_attemptCatalogScanned && m_attemptCatalogLevelID == levelID) {
            return true;
        }

        clearAttemptCatalog_();

        APXCatalogScanResult scan{};
        if (!scanAPXFileCatalog(fileForLevel_(levelID), scan)) {
            return false;
        }

        m_attemptCatalog = std::move(scan.attempts);
        m_attemptCatalogBySerial.reserve(m_attemptCatalog.size());

        for (size_t i = 0; i < m_attemptCatalog.size(); ++i) {
            m_attemptCatalogBySerial[m_attemptCatalog[i].serial] = i;
        }

        if (m_nextAttemptSerial <= static_cast<int>(scan.maxSerialSeen)) {
            m_nextAttemptSerial = static_cast<int>(scan.maxSerialSeen) + 1;
        }

        m_attemptCatalogLevelID = levelID;
        m_attemptCatalogScanned = true;
        return true;
    }

    const APXAttemptDiskInfo* findCatalogEntryBySerial_(int serial) const {
        auto it = m_attemptCatalogBySerial.find(serial);
        if (it == m_attemptCatalogBySerial.end()) return nullptr;
        if (it->second >= m_attemptCatalog.size()) return nullptr;
        return &m_attemptCatalog[it->second];
    }

    size_t findLoadedAttemptIndexBySerial_(int serial) const {
        rebuildSerialCache_();
        auto it = m_serialToIdx.find(serial);
        if (it == m_serialToIdx.end()) return SIZE_MAX;
        if (it->second >= attempts.size()) return SIZE_MAX;
        if (attempts[it->second].serial != serial) return SIZE_MAX;
        return it->second;
    }

    Attempt* findLoadedAttemptBySerialOnly_(int serial) {
        const size_t idx = findLoadedAttemptIndexBySerial_(serial);
        if (idx == SIZE_MAX) return nullptr;
        return &attempts[idx];
    }

    const Attempt* findLoadedAttemptBySerialOnly_(int serial) const {
        const size_t idx = findLoadedAttemptIndexBySerial_(serial);
        if (idx == SIZE_MAX) return nullptr;
        return &attempts[idx];
    }

    bool matchesStartPositionFilterCatalogEntry_(const APXAttemptDiskInfo& entry) const {
        if (!m_filterByStartPosition || entry.practiceAttempt) return true;
        return std::fabs(entry.startX - m_currentReplayStartPos.x) <= kReplayStartTolerance;
    }

    bool getPreloadSortInfoForSerial_(int serial, float& endX, double& endT) const {
        if (const Attempt* a = findLoadedAttemptBySerialOnly_(serial)) {
            if (!a->p1.empty()) {
                endX = static_cast<float>(a->p1.back().x);
                endT = static_cast<double>(a->p1.back().t);
                return true;
            }
            if (a->hadDual && !a->p2.empty()) {
                endX = static_cast<float>(a->p2.back().x);
                endT = static_cast<double>(a->p2.back().t);
                return true;
            }
        }

        if (const APXAttemptDiskInfo* entry = findCatalogEntryBySerial_(serial)) {
            if (entry->p1Count <= 0) return false;
            endX = entry->endX;
            endT = 0.0;
            return true;
        }

        endX = 0.0f;
        endT = 0.0;
        return false;
    }

    void buildInitialAttemptsFromPreloadSerialOrder_(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial
    ) {
        std::vector<uint32_t> sorted = preloadOrder;

        if (m_replayKind == ReplayKind::PracticeComposite) {
            auto getEndT = [&](uint32_t serial) -> double {
                float endX = 0.0f;
                double endT = 0.0;
                getPreloadSortInfoForSerial_(static_cast<int>(serial), endX, endT);
                return endT;
            };

            std::sort(sorted.begin(), sorted.end(),
                [&](uint32_t lhs, uint32_t rhs) {
                    const double tL = getEndT(lhs);
                    const double tR = getEndT(rhs);
                    if (tL != tR) return tL < tR;
                    return lhs < rhs;
                });
        }

        const size_t initialCount = std::min(
            static_cast<size_t>(m_playerObjectPool.capacity()),
            sorted.size()
        );

        outInitial.assign(sorted.begin(), sorted.begin() + initialCount);
    }

    Attempt* ensureAttemptLoadedBySerial_(int serial, bool spawnNow = false) {
        if (serial <= 0) return nullptr;

        if (Attempt* already = findLoadedAttemptBySerialOnly_(serial)) {
            return already;
        }

        if (!m_pl || m_levelIDOnAttach == 0) return nullptr;

        const auto path = fileForLevel_(m_levelIDOnAttach);

        if (!scanAttemptCatalogForLevel_(m_levelIDOnAttach)) return nullptr;

        const APXAttemptDiskInfo* entry = findCatalogEntryBySerial_(serial);
        if (!entry) return nullptr;

        Attempt loaded{};
        bool usedLegacy = false;

        if (!loadAPXAttemptByCatalogEntry(path, *entry, loaded, &usedLegacy)) return nullptr;

        if (m_loadedSerials.count(loaded.serial) != 0) {
            Attempt* existing = findLoadedAttemptBySerialOnly_(loaded.serial);
            if (existing) return existing;

            m_loadedSerials.erase(loaded.serial);
        }

        const int loadedSerial = loaded.serial;
        m_loadedSerials.insert(loadedSerial);
        pushAttempt(std::move(loaded), spawnNow);

        Attempt* result = findLoadedAttemptBySerialOnly_(serial);
        if (!result && loadedSerial != serial) {
            result = findLoadedAttemptBySerialOnly_(loadedSerial);
        }

        if (!result) {
            m_loadedSerials.erase(loadedSerial);
        }

        return result;
    }

    void rebuildSerialCache_() const {
        if (!m_serialCacheDirty) return;
        m_serialToIdx.clear();
        m_serialToIdx.reserve(attempts.size());
        for (size_t i = 0; i < attempts.size(); ++i) {
            m_serialToIdx[attempts[i].serial] = i;
        }
        m_serialCacheDirty = false;
    }

    inline Attempt* findAttemptBySerial_(int serial) {
        const size_t idx = findLoadedAttemptIndexBySerial_(serial);
        if (idx == SIZE_MAX) return nullptr;

        m_replayOwnerIndex = idx;
        return &attempts[idx];
    }

    static GLubyte clampU8_(int64_t v) {
        if (v < 0) return (GLubyte)0;
        if (v > 255) return (GLubyte)255;
        return (GLubyte)v;
    }

    static bool getSettingBoolOrDefault_(geode::Mod* mod, const char* key, bool def) {
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", def);
            return def;
        }
        return mod->getSettingValue<bool>(key);
    }

    static int getSettingIntOrDefault_(geode::Mod* mod, const char* key, int def) {
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", def);
            return def;
        }
        return static_cast<int>(mod->getSettingValue<int64_t>(key));
    }

    static std::string getSettingStrOrDefault_(geode::Mod* mod, const char* key, const char* def) {
        const char* fallback = def ? def : "";
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", fallback);
            return std::string(fallback);
        }
        return mod->getSettingValue<std::string>(key);
    }

    cocos2d::ccColor3B randomGameColor(uint32_t seed) {
        auto* gm = GameManager::sharedState();
        int paletteIdx = pickRandomAllowedGhostColorIdx(seed);
        return gm->colorForIdx(paletteIdx);
    }

    FORCE_INLINE bool practiceRouteOwnerHidingEnabled_() const {
        return m_hideAllOwnersDuringPracticeReplay
            && (m_replayKind == ReplayKind::PracticeComposite)
            && routeIsComposite_();
    }

    FORCE_INLINE bool routeOwnersActive_() const {
        return botActive && practiceRouteOwnerHidingEnabled_();
    }

    void rebuildRouteOwnerCache_() {
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

    FORCE_INLINE bool isOwnerAnywhereInRoute_(Attempt const& a) const {
        if (a.ignorePlayback) return true;

        if (!botActive) return false;

        if (!practiceRouteOwnerHidingEnabled_())
            return false;

        if (a.serial <= 0)
            return false;

        if (a.serial == m_activeOwnerSerial)
            return true;

        return m_routeOwnerSerials.find(a.serial) != m_routeOwnerSerials.end();
    }

    FORCE_INLINE bool isCurrentOwner_(size_t ai, const Attempt& a) const {
        if (!routeOwnersActive_()) return false;
        return (a.serial == m_activeOwnerSerial);
    }

    static void buildAccelTime(const std::vector<Frame>& v,
                           FrameAccelTime& a,
                           uint32_t binWQ = 600,
                           bool buildTimes = true) {
        a.reset();
        const size_t n = v.size();
        if (n == 0) return;

        const uint32_t minTQ = v.front().t.q;
        const uint32_t maxTQ = v.back().t.q;

        if (maxTQ < minTQ) {
            log::warn("[buildAccelTime] Invalid time data: minTQ={}, maxTQ={}", minTQ, maxTQ);
            return;
        }
        if (maxTQ == minTQ) {
            return;
        }

        a.baseTQ = minTQ;
        a.binWQ = std::max<uint32_t>(1u, binWQ);

        const uint64_t spanQ = static_cast<uint64_t>(maxTQ) - static_cast<uint64_t>(minTQ);
        int bins = static_cast<int>((spanQ + a.binWQ - 1) / a.binWQ) + 1;

        if (bins <= 0 || bins > 10000000) {
            log::warn("[buildAccelTime] Invalid bin count: {}, minTQ={}, maxTQ={}, binWQ={}",
                    bins, minTQ, maxTQ, a.binWQ);
            a.reset();
            return;
        }

        if (bins > 100000) {
            a.binWQ = std::max<uint32_t>(1u, static_cast<uint32_t>((spanQ + 99998ull) / 99999ull));
            bins = 100000;
        }

        a.bins = bins;
        a.range.resize(static_cast<size_t>(bins));
        a.idx.resize(static_cast<size_t>(bins));

        if (buildTimes) {
            a.timesQ.resize(n);
            for (size_t i = 0; i < n; ++i) a.timesQ[i] = v[i].t.q;
        } else {
            a.timesQ.clear();
        }

        const uint32_t* tarr = buildTimes ? a.timesQ.data() : nullptr;
        auto getTQ = [&](size_t i) -> uint32_t {
            return tarr ? tarr[i] : v[i].t.q;
        };

        size_t iHi = 0;
        uint32_t binEndQ = minTQ + a.binWQ;

        for (int b = 0; b < bins; ++b) {
            const size_t iLo = iHi;

            while (iHi + 1 < n && getTQ(iHi + 1) < binEndQ) ++iHi;

            a.range[static_cast<size_t>(b)] = FrameAccelTime::BinRange{
                static_cast<uint32_t>(iLo),
                static_cast<uint32_t>(iHi)
            };
            a.idx[static_cast<size_t>(b)] = static_cast<uint32_t>(iLo);

            if (UINT32_MAX - binEndQ < a.binWQ) {
                binEndQ = UINT32_MAX;
            } else {
                binEndQ += a.binWQ;
            }
        }
    }

    static size_t idxForTime(const FrameAccelTime& acc, const std::vector<Frame>& v, double t) {
        if (v.empty()) return 0;

        const uint32_t wantTQ = runtimeQuantTimeQ_(t);
        size_t idx = 0;

        if (acc.valid()) {
            idx = acc.idx[static_cast<size_t>(acc.binOfQ(wantTQ))];
            if (idx >= v.size()) idx = v.size() - 1;

            while (idx + 1 < v.size() && v[idx + 1].t.q <= wantTQ) ++idx;
            while (idx > 0 && v[idx].t.q > wantTQ) --idx;
        } else {
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                const size_t mid = (lo + hi) >> 1;
                if (v[mid].t.q < wantTQ) lo = mid + 1;
                else hi = mid;
            }
            idx = lo ? lo - 1 : 0;
        }

        return idx;
    }

    static FORCE_INLINE size_t idxForTimeBounded_(const std::vector<Frame>& v,
                                              const FrameAccelTime& a,
                                              double t) {
        const size_t n = v.size();
        if (n == 0) return 0;

        const uint32_t wantTQ  = runtimeQuantTimeQ_(t);
        const uint32_t frontTQ = v.front().t.q;
        const uint32_t backTQ  = v.back().t.q;

        if (wantTQ <= frontTQ) return 0;
        if (wantTQ >= backTQ)  return n - 1;

        const bool useTimes = a.hasTimes(n);
        const uint32_t* timesQ = useTimes ? a.timesQ.data() : nullptr;

        auto getTQ = [&](size_t i) -> uint32_t {
            return timesQ ? timesQ[i] : v[i].t.q;
        };

        if (!a.valid()) {
            size_t lo = 0, hi = n;
            while (lo < hi) {
                const size_t mid = (lo + hi) >> 1;
                if (getTQ(mid) <= wantTQ) lo = mid + 1;
                else hi = mid;
            }
            return lo - 1;
        }

        const int b = a.binOfQ(wantTQ);
        size_t L = static_cast<size_t>(a.range[static_cast<size_t>(b)].lo);
        size_t R = static_cast<size_t>(a.range[static_cast<size_t>(b)].hi);

        if (R >= n) R = n - 1;
        if (L > R) L = R;

        size_t lo = L, hi = R + 1;
        while (lo < hi) {
            const size_t mid = (lo + hi) >> 1;
            if (getTQ(mid) <= wantTQ) lo = mid + 1;
            else hi = mid;
        }
        return lo - 1;
    }

    static FORCE_INLINE void advanceUsingAccelTime_(const std::vector<Frame>& v,
                                                const FrameAccelTime& a,
                                                size_t& idx,
                                                double t) {
        const size_t n = v.size();
        if (n == 0) return;

        const uint32_t wantTQ = runtimeQuantTimeQ_(t);

        const bool useTimes = a.hasTimes(n);
        const uint32_t* timesQ = useTimes ? a.timesQ.data() : nullptr;

        auto getTQ = [&](size_t i) -> uint32_t {
            return timesQ ? timesQ[i] : v[i].t.q;
        };

        if (LIKELY(a.valid())) {
            const int b = a.binOfQ(wantTQ);
            idx = static_cast<size_t>(a.idx[static_cast<size_t>(b)]);
            if (idx >= n) idx = n - 1;
        }

        while (idx + 1 < n && getTQ(idx + 1) <= wantTQ) ++idx;
        while (idx > 0 && getTQ(idx) > wantTQ) --idx;
    }

    static FORCE_INLINE void rewindUsingAccelTime_(const std::vector<Frame>& v,
                                               const FrameAccelTime& a,
                                               size_t& idx,
                                               double t) {
        const size_t n = v.size();
        if (n == 0) { idx = 0; return; }

        const uint32_t wantTQ = runtimeQuantTimeQ_(t);

        const bool useTimes = a.hasTimes(n);
        const uint32_t* timesQ = useTimes ? a.timesQ.data() : nullptr;

        auto getTQ = [&](size_t i) -> uint32_t {
            return timesQ ? timesQ[i] : v[i].t.q;
        };

        if (a.valid()) {
            const int b = a.binOfQ(wantTQ);
            size_t hint = static_cast<size_t>(a.idx[static_cast<size_t>(b)]);
            if (hint < idx) idx = hint;
        }

        while (idx > 0 && getTQ(idx) > wantTQ) --idx;
    }

    FORCE_INLINE bool isAttemptPreloaded_(Attempt const& a) const { return a.preloaded; }

    void invalidateAttemptCounts() {
        m_attemptCountsDirty = true;
    }

    
    FORCE_INLINE bool isAttemptInCurrentPracticeSession_(Attempt const& a) const {
        if (m_replayKind != ReplayKind::PracticeComposite) return true;
        return m_practiceSerialSet.contains(a.serial);
        // return m_practiceSerialSet.find(a.serial) != m_practiceSerialSet.end();
    }

    void rebuildAttemptCountsIfNeeded() {
        if (!m_attemptCountsDirty) return;

        int normal = 0;
        int practice = 0;
        m_cachedNormalAttempts = 0;
        m_cachedPracticeAttempts = 0;

        m_preloadableIndexesInAttemptsList.clear();
        m_preloadableSerials.clear();

        std::unordered_set<int> loadedSeen;
        loadedSeen.reserve(attempts.size());
        for (const auto& a : attempts) {
            loadedSeen.insert(a.serial);
        }

        if (m_levelIDOnAttach != 0) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }

        if (m_replayKind == ReplayKind::PracticeComposite) {
            if (!rebuildPracticePreloadSerialSetForCurrentStart_()) {
                m_cachedPracticeAttempts = 0;
                m_cachedNormalAttempts = 0;
                m_attemptCountsDirty = false;
                return;
            }

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];

                if (!a.practiceAttempt) continue;
                if (m_practiceSerialSet.find(a.serial) == m_practiceSerialSet.end()) continue;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) continue;

                m_preloadableIndexesInAttemptsList.push_back(static_cast<int>(i));
                m_preloadableSerials.push_back(static_cast<uint32_t>(a.serial));
                practice++;
            }

            for (const auto& entry : m_attemptCatalog) {
                if (loadedSeen.find(entry.serial) != loadedSeen.end()) continue;

                if (!entry.practiceAttempt) continue;
                if (m_practiceSerialSet.find(entry.serial) == m_practiceSerialSet.end()) continue;
                if (entry.p1Count <= 0) continue;

                m_preloadableSerials.push_back(static_cast<uint32_t>(entry.serial));
                practice++;
            }

            m_cachedPracticeAttempts = practice;
            m_cachedNormalAttempts = 0;
        }
        else {
            auto loadedMatchesNormalReplayFilter = [&](const Attempt& a) -> bool {
                if (a.practiceAttempt) return false;
                if (!matchesStartPositionFilter_(a)) return false;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) return false;

                if (m_onlyShowGhostsThatPassedPercent &&
                    a.endPercent < m_GhostsPassedPercentThreshold) {
                    return false;
                }

                return true;
            };

            auto catalogMatchesNormalReplayFilter = [&](const APXAttemptDiskInfo& entry) -> bool {
                if (entry.practiceAttempt) return false;
                if (!matchesStartPositionFilterCatalogEntry_(entry)) return false;
                if (entry.p1Count <= 0) return false;

                if (m_onlyShowGhostsThatPassedPercent &&
                    entry.endPercent < m_GhostsPassedPercentThreshold) {
                    return false;
                }

                return true;
            };

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];
                if (!loadedMatchesNormalReplayFilter(a)) continue;

                m_preloadableIndexesInAttemptsList.push_back(static_cast<int>(i));
                m_preloadableSerials.push_back(static_cast<uint32_t>(a.serial));
                normal++;
            }

            for (const auto& entry : m_attemptCatalog) {
                if (loadedSeen.find(entry.serial) != loadedSeen.end()) continue;
                if (!catalogMatchesNormalReplayFilter(entry)) continue;

                m_preloadableSerials.push_back(static_cast<uint32_t>(entry.serial));
                normal++;
            }

            m_cachedNormalAttempts = normal;
        }

        m_attemptCountsDirty = false;
    }

    float practicePreloadTargetStartX_() const {
        if (m_filterByStartPosition && std::isfinite(m_currentReplayStartPos.x)) {
            return m_currentReplayStartPos.x;
        }

        if (m_pl && m_pl->m_player1) {
            return m_pl->m_player1->m_positionX;
        }

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

    bool rebuildPracticePreloadSerialSetForCurrentStart_() {
        m_practiceSerialSet.clear();

        const float targetX = practicePreloadTargetStartX_();
        if (!std::isfinite(targetX)) {
            log::warn("[Practice preload] no finite target start X");
            return false;
        }

        const int selectedSession =
            m_checkpointMgr.selectBestSessionForStartX_RankByTime(
                targetX,
                kReplayStartTolerance
            );

        if (selectedSession <= 0 || m_checkpointMgr.noValidSessionForStartX()) {
            log::warn(
                "[Practice preload] no valid session for targetX={} xTol={}",
                targetX,
                kReplayStartTolerance
            );
            return false;
        }

        const auto serials =
            m_checkpointMgr.getPracticeSerialsMatchingStartX_AllSessions(
                targetX,
                static_cast<int>(kReplayStartTolerance)
            );

        m_practiceSerialSet.reserve(serials.size());

        for (int serial : serials) {
            if (serial > 0) {
                m_practiceSerialSet.insert(serial);
            }
        }

        return !m_practiceSerialSet.empty();
    }

    void applyBotSafety_(bool on) {
        safeMode_enabled = on;
        noclip_enabled = on;
        if (on) cheatAPI::setCheat();
        else cheatAPI::endCheat();
    }

    static inline float lastXOf_(const std::vector<Frame>& v) {
        return v.empty() ? 0.f : static_cast<float>(v.back().x);
    }

    float attemptSpanX_(Attempt const& a) const
    {
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();

        if (!a.p1.empty()) {
            minX = std::min<float>(minX, static_cast<float>(a.p1.front().x));
            maxX = std::max<float>(maxX, static_cast<float>(a.p1.back().x));
        }
        if (a.hadDual && !a.p2.empty()) {
            minX = std::min<float>(minX, static_cast<float>(a.p2.front().x));
            maxX = std::max<float>(maxX, static_cast<float>(a.p2.back().x));
        }

        if (maxX <= minX)
            return 0.f;

        return maxX - minX;
    }

    static uint32_t mix32_(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
    }

    static float signedUnit_(uint32_t seed) {
        uint32_t h = mix32_(seed);
        // [0,1) -> (-1,1)
        return ( (h / 4294967296.0f) * 2.f ) - 1.f;
    }

    void applyRandomOffsetSingle(Attempt& a, bool forceNegative) {
        if (m_randomDistPx == 0) {
            a.ghostOffsetPx = 0.f;
            a.ghostOffsetTime = 0.0;
            return;
        }

        auto lastT = [](const std::vector<Frame>& v) -> double {
            return v.empty() ? 0.0 : static_cast<double>(v.back().t);
        };
        const double endT = std::max(lastT(a.p1), lastT(a.p2));

        // Deterministic signed random in [-1, 1]
        const float u = signedUnit_(
            static_cast<uint32_t>(a.serial) ^
            static_cast<uint32_t>(m_levelIDOnAttach) ^
            0x9E3779B9u
        );

        float offsetPx = u * static_cast<float>(m_randomDistPx);

        constexpr float kMinOffsetPx = -100.0f;
        constexpr float kMaxOffsetPx = 174.0f;
        constexpr float kMinMagPx = 10.0f;
        constexpr double kEndGapSec = 0.5;

        const double ups = std::max(1e-6, m_offsetUnitsPerSecond);

        // Positive limit:
        const double maxForwardTime = std::max(0.0, endT - kEndGapSec);
        const double maxForwardPxD  = maxForwardTime * ups;
        const float usableMaxPx = std::min(
            kMaxOffsetPx,
            static_cast<float>(std::max(0.0, maxForwardPxD))
        );

        // Negative limit with wrapping:
        auto wrapIntoWindow = [](float x, float lo, float hi) -> float {
            if (hi <= lo) return lo;
            if (x >= lo && x <= hi) return x;

            const double span = static_cast<double>(hi) - static_cast<double>(lo);
            double y = static_cast<double>(x) - static_cast<double>(lo);
            y = std::fmod(y, span);
            if (y < 0.0) y += span;
            return static_cast<float>(static_cast<double>(lo) + y);
        };

        if (forceNegative) {
            offsetPx = -std::fabs(offsetPx);
            offsetPx = wrapIntoWindow(offsetPx, kMinOffsetPx, -kMinMagPx);
        } else {
            offsetPx = wrapIntoWindow(offsetPx, kMinOffsetPx, usableMaxPx);
        }

        // Don't have 0 offset to avoid weird overlap
        if (std::fabs(offsetPx) < kMinMagPx) {
            const float posCandidate = std::min(kMinMagPx, usableMaxPx);
            const float negCandidate = -kMinMagPx;

            if (offsetPx >= 0.f && posCandidate >= kMinMagPx) {
                offsetPx = posCandidate;
            } else {
                offsetPx = negCandidate;
            }
        }

        a.ghostOffsetPx   = offsetPx;
        a.ghostOffsetTime = static_cast<double>(offsetPx) / ups;
    }

    void applyRandomOffsetsAll_() {
        for (auto& a : attempts) {
            applyRandomOffsetSingle(a, isOwnerAnywhereInRoute_(a));
        }
    }
    
    void refreshAttemptColors_(Attempt& a) {
        if (!a.colorsAssigned) {
            if (colors == ColorMode::Random) {
                uint32_t seed = static_cast<uint32_t>(a.serial) ^ 0xA5339E4Du;
                a.c1p1 = randomGameColor(seed);
                a.c2p1 = randomGameColor(seed ^ 0xBADA551u);
                a.c1p2 = a.c1p1;
                a.c2p2 = a.c2p1;
                hasGlowActive(true, a.hasGlowP1);
                hasGlowActive(false, a.hasGlowP2);
                if (a.hasGlowP1) a.cgp1 = randomGameColor(seed ^ 0x6C8E9CF5u);
                if (a.hasGlowP2) a.cgp2 = randomGameColor(seed ^ 0xD1B54A35u);
            } else {
                a.c1p1 = chooseColor1(true);
                a.c2p1 = chooseColor2(true);
                a.c1p2 = chooseColor1(false);
                a.c2p2 = chooseColor2(false);
                a.cgp1 = chooseGlowColor(true, a.hasGlowP1);
                a.cgp2 = chooseGlowColor(false, a.hasGlowP2);
            }
            a.colorsAssigned = true;
        }

        if (a.g1) {
            a.g1->setColor(a.c1p1);
            a.g1->setSecondColor(a.c2p1);
            if (a.g1->m_waveTrail) {
                if (m_pl->m_player1->m_switchWaveTrailColor) a.g1->m_waveTrail->setColor(a.c2p1);
                else a.g1->m_waveTrail->setColor(a.c1p1);
            }
            if (a.hasGlowP1) {
                a.g1->m_hasGlow = true;
                a.g1->updatePlayerGlow();
                a.g1->enableCustomGlowColor(a.cgp1);
                a.g1->updateGlowColor();
            }
        }
        if (a.g2) {
            a.g2->setColor(a.c1p2);
            a.g2->setSecondColor(a.c2p2);
            if (a.g2->m_waveTrail) {
                if (m_pl->m_player2->m_switchWaveTrailColor) a.g2->m_waveTrail->setColor(a.c1p2);
                else a.g2->m_waveTrail->setColor(a.c2p2);
            }
            if (a.hasGlowP2) {
                a.g2->m_hasGlow = true;
                a.g2->updatePlayerGlow();
                a.g2->enableCustomGlowColor(a.cgp2);
                a.g2->updateGlowColor();
            }
        }
    }

    inline void moveTrailToObjectLayer(PlayerObject* po, cocos2d::CCNode* objectLayer)
    {
        if (!po || !po->m_waveTrail || !objectLayer) return;

        if (po->m_waveTrail->getParent() != objectLayer) {
            po->m_waveTrail->retain();
            po->m_waveTrail->removeFromParentAndCleanup(false);
            objectLayer->addChild(po->m_waveTrail, -1);
            po->m_waveTrail->release();
        }
    }

    static inline void offsetAttemptTimesFast(Attempt& a, uint32_t offsetQ) {
        if (offsetQ == 0) return;

        auto offsetFrames = [offsetQ](std::vector<Frame>& frames) {
            if (frames.empty()) return;

            Frame* p = frames.data();
            Frame* e = p + frames.size();

            for (; p != e; ++p) {
                p->t.q += offsetQ;
            }
        };

        offsetFrames(a.p1);
        offsetFrames(a.p2);
    }

    void preloadAttempt_(Attempt& a, size_t attemptIdx = SIZE_MAX, bool force=false) {
        if (!force && a.preloaded) return;
        if (!m_pl || !m_pl->m_player1) return;

        a.m_isPlatformer = m_pl->m_isPlatformer;

        // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
        if (m_baseTime != 0.f && a.baseTimeOffset == 0.f) {
            a.baseTimeOffset = m_baseTime;
            const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
            offsetAttemptTimesFast(a, offsetQ);
        }

        if (!a.acc1Time.valid() && !a.p1.empty()) {
            buildAccelTime(a.p1, a.acc1Time);
        }
        if (a.hadDual && !a.acc2Time.valid() && !a.p2.empty()) {
            buildAccelTime(a.p2, a.acc2Time);
        }
        

        uint32_t base = static_cast<uint32_t>(a.serial) ^ 0xDEADBEEF;
        for (int i = 0; i < 9; ++i) {
            a.randomFrame[i] = randomIconFrame(static_cast<IconType>(i), base + i * 97);
        }
        a.iconsAssigned = true;

        a.preloaded = true;
        ++m_attemptsPreloadedTotal;
        
        if (attemptIdx != SIZE_MAX && m_preloadedSet.find(attemptIdx) == m_preloadedSet.end()) {
            //log::info("push into preload set");
            m_preloadedIndices.push_back(attemptIdx);
            m_preloadedSet.insert(attemptIdx);
        }
        else {
            log::warn("Didn't push into preload set: attemptIdx: {}, m_preloadedSet.size(): {}", attemptIdx, m_preloadedSet.size());
        }
    }

    static inline AttemptSpan makeSpan_(const Attempt& a) {
        AttemptSpan s;
        if (!a.p1.empty()) {
            s.firstX = a.p1.front().x;
            s.lastX = a.p1.back().x;
            if (s.lastX < s.firstX)
                std::swap(s.lastX, s.firstX);
        } else {
            s.firstX = s.lastX = 0.0;
        }
        s.ownerSerial = a.serial;
        s.p1Count = (int)a.p1.size();
        s.p2Count = (int)a.p2.size();
        return s;
    }

    static inline void hardResetWaveTrail(PlayerObject* p) {
        if (!p || !p->m_waveTrail) return;
        // p->deactivateStreak(true);
        //p->m_waveTrail->reset();
        //p->setupStreak();
        //p->resetStreak();
        // p->m_waveTrail->setVisible(false);
        //p->m_waveTrail->setOpacity(255);
    }

    void tagCompositeOwnersHidden_(bool hide) {
        for (auto& a : attempts) a.ignorePlayback = false;
        if (!hide) return;
        if (!routeIsComposite_()) {
            if (m_compOwnerIdx != SIZE_MAX && m_compOwnerIdx < attempts.size())
                attempts[m_compOwnerIdx].ignorePlayback = true;
        }
    }

    static inline void resetTrailFlags_(Attempt& a, bool isP1) {
        if (isP1) {
            a.trailactive1 = false;
            a.previouslyHolding1 = false;
            a.previouslyHoldingL1 = false;
            a.previouslyHoldingR1 = false;
        } else {
            a.trailactive2 = false;
            a.previouslyHolding2 = false;
            a.previouslyHoldingL2 = false;
            a.previouslyHoldingR2 = false;
        }
    }

    void forceShowPlayerVisuals(PlayerObject* p) {
        if (!p) return;
        p->m_isDead = false;
        p->m_isHidden = false;
        //p->setVisible(true);
        p->setOpacity(opacity);
    }

    bool primeGhostToPX_(Attempt& a, size_t attemptIdx, float pxw, float px2w) {
        //log::info("Priming");
        double ghostTime = m_currentSessionTime + a.ghostOffsetTime;
        if (ghostTime < 0.0) ghostTime = 0.0;

        if (!a.p1.empty() && ghostTime > a.p1.back().t) return false;

        bool isAnOwner = isOwnerAnywhereInRoute_(a);
        
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
            
            // One frame late to apply
            if (!a.offsetApplied) {
                applyRandomOffsetSingle(a, isAnOwner);
                a.offsetApplied = true;
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
                pc.x = static_cast<float>(f.x);
                pc.y = static_cast<float>(f.y);
                pc.rot = static_cast<float>(f.rot);
                pc.vehicleSize = static_cast<float>(f.vehicleSize);
                pc.upsideDown = f.upsideDown;
                a.g1->stopDashing();
                pc.isDashing = false;
                
                // hardResetWaveTrail(a.g1);
                a.trailactive1 = false;
                forceShowPlayerVisuals(a.g1);
            }

            a.previouslyHolding1 = f.hold;
            a.previouslyHoldingL1 = f.holdL;
            a.previouslyHoldingR1 = f.holdR;
            a.primedP1 = true;
        }

        if (a.hadDual && !a.p2.empty() && !a.primedP2) {
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
                refreshAttemptColors_(a);
                pc2.x = static_cast<float>(f2.x);
                pc2.y = static_cast<float>(f2.y);
                pc2.rot = static_cast<float>(f2.rot);
                pc2.vehicleSize = static_cast<float>(f2.vehicleSize);
                pc2.upsideDown = f2.upsideDown;

                a.g2->stopDashing();
                pc2.isDashing = false;
                
                // hardResetWaveTrail(a.g2);
                a.trailactive2 = false;
                forceShowPlayerVisuals(a.g2);
            }

            a.previouslyHolding2 = f2.hold;
            a.previouslyHoldingL2 = f2.holdL;
            a.previouslyHoldingR2 = f2.holdR;
            a.primedP2 = true;

            refreshAttemptColors_(a);
        }
        return true;
    }

    static inline bool attemptHasData_(const Attempt& a) {
        return !a.p1.empty() || !a.p2.empty();
    }

    static inline size_t countAttemptsWithData_(std::vector<Attempt> const& v) {
        size_t n = 0;
        for (auto const& a : v) {
            if (attemptHasData_(a)) ++n;
        }
        return n;
    }

    static inline float attemptLastX_(const Attempt& a)  {
        return a.p1.empty() ? 0.f : static_cast<float>(a.p1.back().x);
    }

    static FORCE_INLINE bool matchesPracticeFilter_(const Attempt& a, std::optional<bool> wantPractice) {
        if (!wantPractice.has_value())
            return true;

        if (wantPractice.value()) {
            if (!a.practiceAttempt) return false;
            return true;
        }
        return !a.practiceAttempt;
    }

    static FORCE_INLINE bool matchesPracticeValue_(bool practiceAttempt, std::optional<bool> wantPractice) {
        if (!wantPractice.has_value()) return true;
        return practiceAttempt == wantPractice.value();
    }

    int pickWinningOrBestSerialFromPosition_(
        std::optional<bool> wantPractice,
        float startPosX,
        float startPosY,
        float tolerance
    ) const {
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
            const float dy = attemptStartY - startPosY;
            //log::info("attemptStartX: {}, attemptStartY: {}", attemptStartX, attemptStartY);
            //log::info("startPosX: {}, startPosY: {}", startPosX, startPosY);
            return (dx * dx + dy * dy) <= tolSq;
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

    size_t pickWinningOrBestIndex_(std::optional<bool> wantPractice) const {
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

    float playerX_() const { return m_px; }
    float playerX2_() const { return m_px2; }

    static inline size_t clampWithOffsetMonotonicThrottled_(size_t base, int offset, size_t cap, size_t lastDraw, size_t maxStep = 1) {
        long long want = (long long)base + (long long)offset;
        if (want < 0) want = 0;
        if ((size_t)want > cap) want = (long long)cap;

        size_t target = (size_t)want;
        if (target < lastDraw) target = lastDraw;

        size_t delta = target > lastDraw ? (target - lastDraw) : 0;
        if (delta > maxStep) target = lastDraw + maxStep;
        
        return target;
    }

    static inline void syncGhostGravity(PlayerObject* po, bool wantUpside, PlayLayer* pl) {
        if (!po) return;
        po->m_stateFlipGravity = 0;
        if (po->m_isUpsideDown != wantUpside) {
            po->flipGravity(wantUpside, true);
        }
    }

    void adjustReplayCursorToTime_(double sessionTime) {
        m_currentOwner = getReplayOwner_(sessionTime);
        if (!m_currentOwner) return;

        
        //log::info("[adjustReplayCursorToTime_] curret owner size: {} idx {}", m_currentOwner->p1.size(), m_replayIdx1);
        
        const bool wentBack = (sessionTime < m_prevSessionTime - 0.05);

        //log::info("[adjustReplayCursorToTime_] wentBack {} m_prevSessionTime {} sessionTime {}", wentBack, m_prevSessionTime, sessionTime);

        
        
        if (!m_currentOwner->p1.empty()) {
            //m_replayIdx1 = idxForTimeBounded_(m_currentOwner->p1, m_currentOwner->acc1Time, sessionTime); WAS USING THIS BEFORE advanceFromPrevIdx_
            //if (wentBack) {
            //    rewindUsingAccelTime_(m_currentOwner->p1, m_currentOwner->acc1Time, m_replayIdx1, sessionTime);
            //}
            //advanceUsingAccelTime_(m_currentOwner->p1, m_currentOwner->acc1Time, m_replayIdx1, sessionTime);

            if (LIKELY(!wentBack)) {
                advanceFromPrevIdx_(m_currentOwner->p1, m_replayIdx1, sessionTime, /*mustRewind=*/false);
            } else {
                m_replayIdx1 = idxForTimeBounded_(m_currentOwner->p1, m_currentOwner->acc1Time, sessionTime);
            }


            
            // Clamp to valid range
            if (m_replayIdx1 >= m_currentOwner->p1.size()) {
                m_replayIdx1 = m_currentOwner->p1.size() - 1;
            }
            // log::info("m_currentOwner serial: {}", m_currentOwner->serial);
        }
        
        if (m_currentOwner->hadDual && !m_currentOwner->p2.empty()) {
            if (LIKELY(!wentBack)) {
                advanceFromPrevIdx_(m_currentOwner->p2, m_replayIdx2, sessionTime, /*mustRewind=*/false);
            } else {
                m_replayIdx2 = idxForTimeBounded_(m_currentOwner->p2, m_currentOwner->acc2Time, sessionTime);
            }

            if (m_replayIdx2 >= m_currentOwner->p2.size())
                m_replayIdx2 = m_currentOwner->p2.size() - 1;
        }
        
        m_prevBotPx = playerX_();
    }

    const Attempt* getReplayOwner_(double sessionTime) {
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_replayOwnerSerial > 0) {
                m_currentOwner = ensureAttemptLoadedBySerial_(m_replayOwnerSerial, false);
                return m_currentOwner;
            }

            if (m_compOwnerIdx < attempts.size()) {
                m_replayOwnerSerial = attempts[m_compOwnerIdx].serial;
                m_currentOwner = &attempts[m_compOwnerIdx];
                return m_currentOwner;
            }

            m_currentOwner = nullptr;
            return nullptr;
        }

        int serial = m_checkpointMgr.findOwnerSerialForTime(sessionTime);

        if (serial != m_replayOwnerSerial && serial > 0) {
            m_replayOwnerSerial = serial;
            m_replayIdx1 = 0;
            m_replayIdx2 = 0;
            m_lastEmitIdx1 = 0;
            m_lastEmitIdx2 = 0;
        }

        if (serial <= 0) {
            m_currentOwner = nullptr;
            return nullptr;
        }

        m_currentOwner = ensureAttemptLoadedBySerial_(serial, false);
        return m_currentOwner;
    }

    void updateGhostForAttempt_(Attempt& a, float px, float px2, size_t ai,
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
            bool isP2,
            float playerX
        ) {
            if (frames.empty() || !primed) return;
            // if (isP2 && !m_p2Moving) return;

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
                }
                frameIdx = std::min(frameIdx+2, frames.size()-1);
                drawIdx = std::min(frameIdx, lastIdx);

                const bool atLastFrame = (drawIdx >= lastIdx);
                const bool atEndTime   = (ghostTime >= lastT - kEolTimeTolerance);

                // EOL freeze check
                if ((atLastFrame || atEndTime) && !isP2) {
                    if (ghost) {
                        ghost->stopDashing();
                        if (m_ghostsExplode) {
                            // ghost->playerDestroyed(false);
                            ghost->playDeathEffect();
                            // ghost->setOpacity(0);
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

                    }
                        
                    //ghost->m_spiderSprite->stopAnimations();
                    //ghost->m_robotSprite->stopAnimations();
                    // Need another name for these?
                    eolFrozen = true;
                }

                // Make P2 invisible when there's a P2 data gap (not in dual mode)
                if (isP2 && ghost) {
                    bool hideForGap = false;
                    if (drawIdx < lastIdx) {
                        const double t0 = frames[drawIdx].t;
                        const double t1 = frames[drawIdx+1].t;
                        const double gap = t1 - t0;

                        if (gap > kNoDualTimeGap && ghostTime > t0 && ghostTime < t1) {
                            hideForGap = true;
                        }
                    }
                    // When they both die, don't be invisible
                    // Only be invisible when p2 ended and p1 has more data
                    bool hideForP2EndedWhileP1Continues = false;
                    if (!hideForGap && !eolFrozen && (atLastFrame || atEndTime)) {
                        const double p2LastT = lastT;
                        const double p1LastT = a.p1.empty()
                            ? p2LastT
                            : static_cast<double>(a.p1.back().t);

                        if (p1LastT - p2LastT > kP1ContinuesPastP2Threshold) {
                            hideForP2EndedWhileP1Continues = true;
                        }
                    }

                    if (hideForGap || hideForP2EndedWhileP1Continues) {
                        a.setP2Visible(false, true);
                        return;
                    }
                }

                if (frameIdx == previousIDX) return;

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
                const float firstX = frames.front().x;
                if (playerX + 0.0001f < firstX) {
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
            ghost->setPosition({ix, iy});
            ghost->setRotation(irot);

            //log::info("[TIME TAKEN] updateghost posupdate {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            // Vehicle size update
            if (pc.vehicleSize != iveh) {
                ghost->m_vehicleSize = iveh;
                ghost->updatePlayerScale();
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

            if (m_animateSpider && f.mode == IconType::Spider) { // Tween for smoother transitions
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

            if (m_animateRobot && f.mode == IconType::Robot) { //  Tween for smoother transitions between states (but extra compute)
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
                        // hardResetWaveTrail(ghost);
                        trail->setVisible(false);
                        trail->setOpacity(0);
                        trailActive = false;
                    } else if (trail) {
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
                            trail->addPoint({ix, iy});
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

                                    if (wi == drawIdx) {
                                        trail->addPoint({ix, iy});
                                    } else {
                                        trail->addPoint({wf.x, wf.y});
                                    }
                                }
                            }
                        } else if (f.wavePointThisFrame) {
                            hasWavePointData = true;
                            trail->addPoint({ix, iy});
                        }
                        if (!hasWavePointData) {
                            if (f.hold != prevHolding) {
                                trail->addPoint({ix, iy});
                            }
                            else if (fNext.vehicleSize != f.vehicleSize) {
                                trail->addPoint({ix, iy});
                            }
                            else if (pc.wasMovingUp != isMovingUp) {    
                                trail->addPoint({ix, iy});
                            }
                            else {
                                const bool stateDartSlide = (std::fabs(fNext.y - f.y) <= kYEqualEps);
                                if (stateDartSlide != prevDartSlide) {
                                    prevDartSlide = stateDartSlide;
                                    trail->addPoint({ix, iy});
                                }
                            }
                        }
                        
                        prevHolding = f.hold;
                        pc.wasMovingUp = isMovingUp;
                        
                        // Teleport visual wacky stuff
                        if (prevTeleported) {
                            trail->resumeStroke();
                            trail->addPoint({ix, iy});
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
                a.trailactive1, a.previouslyHolding1, a.prevStateDartSlide1, a.prevTeleported1, a.hasWavePointData,
                false, px
            );
        }

        // log::info("a.hadDual: {}, a.p2.empty(): {}", a.hadDual, a.p2.empty());

        // Process P2
        if (a.hadDual && !a.p2.empty() && a.primedP2) {
            processPlayer(
                a.p2, a.acc2Time, a.g2, a.last2, a.g2CurMode,
                a.i2, a.d2, a.primedP2, a.eolFrozenP2,
                a.trailactive2, a.previouslyHolding2, a.prevStateDartSlide2, a.prevTeleported2, a.hasWavePointData,
                true, px2 + a.ghostOffsetPx
            );
        }
        //log::info("[TIME TAKEN] updateghost {:.8f}", getTimeMs() - tStart2);
    }

    static FORCE_INLINE void advanceFromPrevIdx_(
        const std::vector<Frame>& v,
        size_t& idx,
        double t,
        bool mustRewind
    ) {
        const size_t n = v.size();
        if (n == 0) { idx = 0; return; }
        const size_t last = n - 1;
        if (idx > last) idx = last;

        const uint32_t wantTQ  = runtimeQuantTimeQ_(t);
        const uint32_t frontTQ = v.front().t.q;
        const uint32_t backTQ  = v.back().t.q;

        if (wantTQ <= frontTQ) { idx = 0; return; }
        if (wantTQ >= backTQ)  { idx = last; return; }

        if (UNLIKELY(mustRewind) || UNLIKELY(v[idx].t.q > wantTQ)) {
            while (idx > 0 && v[idx].t.q > wantTQ) --idx;
            while (idx + 1 <= last && v[idx + 1].t.q <= wantTQ) ++idx;
            return;
        }

        if (LIKELY(idx + 1 <= last) && v[idx + 1].t.q <= wantTQ) {
            do { ++idx; }
            while (idx + 1 <= last && v[idx + 1].t.q <= wantTQ);
        }
    }

    size_t ptrToIndex_(const Attempt* p) const {
        if (!p) return SIZE_MAX;
        if (p == &m_current) return SIZE_MAX;
        for (size_t j = 0; j < attempts.size(); ++j) {
            if (&attempts[j] == p) return j;
        }
        return SIZE_MAX;
    }

    void startNewAttempt() {
        m_current = Attempt{};
        m_currentDidNoclip = false;
        m_current.serial = m_nextAttemptSerial++;
        m_currentAttemptStart = m_frameCounter;
        
        float startX = 0.f;
        float startY = 0.f;
        m_current.endPercent = 0.f;
        
        if (m_pl && m_pl->m_player1) {
            startX = m_pl->m_player1->getPositionX();
            startY = m_pl->m_player1->getPositionY();
        }
        
        m_current.startX = startX;
        m_current.startY = startY;

        m_baseTime = getStartTime();
        m_current.baseTimeOffset = m_baseTime;

        // log::info("[startNewAttempt] baseTime: {}", m_baseTime);

        //if (recording) {
            //m_current.seed = seedutils::getCurrentSeed();
            //if (m_current.seed == 0) {
            //    m_current.seed = seedutils::generateFreshSeed();
            //    seedutils::setRawSeed(m_current.seed);
            //}
            //log::info("[Seed] Captured seed {} for attempt {}", m_current.seed, m_current.serial);
        //}

        //log::info("[startNewAttempt] Attempt created with time offset {}", m_current.baseTimeOffset);
        
        m_current.practiceAttempt = (m_pl && m_pl->m_isPracticeMode);
        m_current.ignorePlayback = false;
        m_lastRecordedX = 0.f;
        
        if (m_pl) {
            m_current.startPercent = m_pl->getCurrentPercent();
        } else {
            m_current.startPercent = 0;
        }
        m_current.recordedThisSession = true;
    }

    void sortBestToFront() {
        float best=-1.f;
        size_t bestIdx=0;
        for (size_t i=0; i<attempts.size(); ++i) {
            float lastX = !attempts[i].p1.empty() ? static_cast<float>(attempts[i].p1.back().x) : 0.f;
            if (lastX > best) {
                best=lastX;
                bestIdx=i;
            }
        }
        if (bestIdx!=0) {
            std::swap(attempts[0], attempts[bestIdx]);
        }
    }

    static inline void considerByXThenSerial_(const Attempt& a, float& bestX, int& bestSerial, const Attempt*& bestPtr) {
        if (a.p1.empty()) return;
        float x = lastXOf_(a.p1);
        if (x > bestX || (x == bestX && a.serial > bestSerial)) {
            bestX = x;
            bestSerial = a.serial;
            bestPtr = &a;
        }
    }

    cocos2d::ccColor3B chooseColor1(bool isP1) {
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
    cocos2d::ccColor3B chooseColor2(bool isP1) {
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
    void hasGlowActive(bool isP1, bool& hasGlow) {
        if (isP1 && m_pl->m_player1) hasGlow = m_pl->m_player1->m_hasGlow;
        else if (m_pl->m_player2) hasGlow = m_pl->m_player2->m_hasGlow;
    }
    cocos2d::ccColor3B chooseGlowColor(bool isP1, bool& hasGlow) {
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

    static PlayerObject* spawnGhostPO(PlayLayer* pl, cocos2d::CCNode* parent, GLubyte opacity) {
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

    static void applyOpacityTree(cocos2d::CCNode* n, GLubyte o) {
        if (!n) return;
        if (auto* rgba = typeinfo_cast<cocos2d::CCRGBAProtocol*>(n)) rgba->setOpacity(o);
        if (auto* kids = n->getChildren()) {
            for (unsigned i = 0; i < kids->count(); ++i) {
                applyOpacityTree(static_cast<cocos2d::CCNode*>(kids->objectAtIndex(i)), o);
            }
        }
    }

    GhostBatch& getOrCreateBatch_() {
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

    void clearGhostBatches_() {
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

    void rebuildGridThreasholdSet_() {
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

    void rebuildGridPreloadedSet_() {
        m_grid.clear();

        // throw away any prior preload build state
        m_spans.clear();
        m_spans.reserve(m_preloadedIndices.size());

        for (size_t k = 0; k < m_preloadedIndices.size(); ++k) {
            const size_t attemptIdx = (size_t)m_preloadedIndices[k];
            if (attemptIdx >= attempts.size()) continue;

            // Always recompute
            AttemptSpan sp = makeSpan_(attempts[attemptIdx]);
            m_spans.push_back(sp);

            if (sp.lastX > sp.firstX) {
                // Store attemptIdx so downstream can do attempts[candidate]
                m_grid.insert((int)attemptIdx, (float)sp.firstX, (float)sp.lastX);
            }
        }

        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
    }

    void rebuildGrid_() {
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

    void pushAttempt(Attempt&& a, bool spawnNow = true) {
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

        //geode::log::info("[pushAttempt] serial {} endPercent {}", a.serial, a.endPercent);

        if (isPureRecordingMode_()) {
            a.preloaded = false;
            a.g1 = nullptr;
            a.g2 = nullptr;
            
            attempts.push_back(std::move(a));
            m_serialCacheDirty = true;
            
            while ((int64_t)attempts.size() > maxGhosts) {
                invalidateAttemptPointerCaches_();
                m_loadedSerials.erase(attempts.front().serial);
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
        attempts.push_back(std::move(a));
        m_serialCacheDirty = true;

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

            m_loadedSerials.erase(oldest.serial);

            attempts.erase(attempts.begin());
            m_spans.erase(m_spans.begin());
            m_serialCacheDirty = true;
            rebuildGrid_();
        }
        
        invalidateAttemptCounts();
    }

    void clearAllGhostNodes() {
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

    void forcePlayersVisible_() {
        if (!m_pl) return;
        if (m_pl->m_player1) {
            m_pl->m_player1->setVisible(true);
            m_pl->m_player1->setOpacity(255);
        } if (m_pl->m_player2) {
            m_pl->m_player2->setVisible(true);
            m_pl->m_player2->setOpacity(255);
        }
    }

    void clearHeldInputs_() {
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

    void waveTrailAddPointToPlayer(HardStreak* m_waveTrail, cocos2d::CCPoint point, bool isP1, bool m_skip) {
        if (m_skip) return;
        m_allowWavePointAdding = true;
        m_waveTrail->addPoint(point);
        m_allowWavePointAdding = false;
        // if (!isP1) log::info("px: {}, py: {}", point.x, point);
    }

    void applyFrameToPlayerOverRange(
        PlayerObject* p,
        const std::vector<Frame>& v,
        size_t baseIdx,
        double sessionTime,
        bool isP1 = true) {
        
        if (!m_setRealPlayerPosition) return;
        if (!p || v.empty()) return;
        if (m_freezePlayerXAtEnd) return;

        const double firstT = v.front().t;
        const double lastT  = v.back().t;
        const bool inside  = (sessionTime >= firstT && sessionTime <= lastT);
        size_t& lastApplyIdx = isP1 ? m_lastEmitIdx1 : m_lastEmitIdx2;

        const size_t endIdx = std::min(baseIdx, v.size() - 1);

        size_t startIdx = endIdx;

        if (lastApplyIdx != std::numeric_limits<size_t>::max() &&
            lastApplyIdx < endIdx &&
            lastApplyIdx < v.size()) {
            startIdx = lastApplyIdx + 1;
        }

        log::info("p1: {} from {} to {}", isP1, startIdx, endIdx);

        for (size_t i = startIdx; i <= endIdx; ++i) {
            if (i >= v.size()) i = v.size() - 1;
            const Frame& a = v[i];
            const bool haveNext = (i + 1 < v.size());
            const Frame* b = haveNext ? &v[i + 1] : nullptr;

            if (p->m_isDashing != a.isDashing) {
                if (a.isDashing) p->startDashing(attemptplayback::p0d());
                else p->stopDashing();
            }

            if (p->m_isUpsideDown != a.upsideDown)
                p->flipGravity(a.upsideDown, true);

            if (currentMode(p, m_pl->m_isPlatformer) != a.mode) {
                forceMode(p, a.mode, /*isRealPlayer*/ true);
            }

            const bool isWave = (currentMode(p, m_pl->m_isPlatformer) == IconType::Wave);
            const float ix = a.x;
            const float iy = a.y;
            float prevY = p->getPositionY();
            float prevX = p->getPositionX();

            bool addedWavePoint = false;
            if (isWave && a.wavePointThisFrame && p->m_waveTrail) {
                m_hasWavePointData = true;
                waveTrailAddPointToPlayer(p->m_waveTrail, {ix, iy}, isP1, m_p2JustSpawned);
                addedWavePoint = true;
            }
            else if (isWave && !addedWavePoint && prevHold != a.hold) {
                waveTrailAddPointToPlayer(p->m_waveTrail, {prevX, prevY}, isP1, m_p2JustSpawned);
                addedWavePoint = true;
            }
            prevHold = a.hold;

            if (isWave && p->m_waveTrail) {
                p->m_waveTrail->setVisible(true);
                p->m_waveTrail->setOpacity(255);
                p->m_waveTrail->resumeStroke();
            }

            // Wave teleport goop
            if (m_playerPrevTeleported) {
                if (isWave && p->m_waveTrail) {
                    p->m_waveTrail->reset();
                    p->m_waveTrail->resumeStroke();
                    if (!addedWavePoint) {
                        waveTrailAddPointToPlayer(p->m_waveTrail, {ix, iy}, isP1, m_p2JustSpawned);
                        addedWavePoint = true;
                    }
                }
                auto gl = GJBaseGameLayer::get();
                m_playerPrevTeleported = false;
            }
            bool teleported = (b && std::fabs(static_cast<float>(b->y) - iy) > kWaveTeleportedTolerance);
            if (teleported) {
                if (isWave && p->m_waveTrail) p->m_waveTrail->stopStroke();
                m_playerPrevTeleported = true;
            }

            p->setVisible(true);
            p->setOpacity(255);

            p->setPosition({ a.x, a.y });

            if (std::fabs(iy - prevY) > kWaveTeleportedTolerance*8) m_speedUpCameraAnimationAfterTeleportForNFrames = 5;

            if (m_speedUpCameraAnimationAfterTeleportForNFrames > 0) {
                auto gl = GJBaseGameLayer::get();
                gl->updateCamera(20.0f);
                m_speedUpCameraAnimationAfterTeleportForNFrames--;
            }

            if (isWave) {
                p->setRotation(a.rot);

                //if (m_fixWaveTrailAfterSpawnP2ForNFrames > 0) {
                //    p->m_waveTrail->stopStroke();
                //    p->m_waveTrail->reset();
                //    p->m_waveTrail->resumeStroke();
                //    if (!addedWavePoint) {
                //        waveTrailAddPointToPlayer(p->m_waveTrail, {ix, iy}, isP1, false);
                //        addedWavePoint = true;
                //    }
                //    m_fixWaveTrailAfterSpawnP2ForNFrames--;
                //}
                
                // Fix player 2 wave trail visual bug when spawning in
                if (m_p2JustSpawned && p->m_waveTrail) {
                    //m_fixWaveTrailAfterSpawnP2ForNFrames = 2;
                    p->m_waveTrail->reset();
                    p->m_waveTrail->resumeStroke();
                    if (!addedWavePoint) {
                        waveTrailAddPointToPlayer(p->m_waveTrail, {ix, iy}, isP1, false);
                        addedWavePoint = true;
                    }
                    p->m_waveTrail->setVisible(true);
                    p->m_waveTrail->setOpacity(255);
                    m_p2JustSpawned = false;
                }
            }
            else p->setRotation(a.rot);
        }
    }

    static void emitTransitionsOverRange_(const std::vector<Frame>& v,
                                          size_t fromIdx,
                                          size_t toIdx,
                                          PlayerObject* p,
                                          bool& prevJump,
                                          bool& prevLeft,
                                          bool& prevRight,
                                        PlayLayer* pl,
                                    bool isP1) {
        if (!p || v.empty()) return;
        if (toIdx >= v.size()) toIdx = v.size() - 1;
        if (fromIdx >= v.size()) fromIdx = toIdx;
        // auto bl = GJBaseGameLayer::get();
        for (size_t k = fromIdx + 1; k <= toIdx; ++k) {
            const Frame& F = v[k];
            if (F.hold != prevJump) {
                //if (F.hold) p->pushButton(PlayerButton::Jump);
                //else p->releaseButton(PlayerButton::Jump);
                //m_bl->handleButton(F.hold, 1, isplayer1);
                //GJBaseGameLayer::get()->handleButton(F.hold, 1, isplayer1);
                //if (isplayer1) {
                //    if (F.hold) Ghosts::I().setGJH1(F.hold);
                //    else Ghosts::I().setGJR1(F.hold);
                //} else {
                //    if (F.hold) Ghosts::I().setGJH2(F.hold);
                //    else Ghosts::I().setGJR2(F.hold);
                //}
                //log::info("prevJump: {}, F.hold: {}, p1Hold: {}", prevJump, F.hold, Ghosts::I().p1Hold);

                if (pl) pl->handleButton(F.hold, /*btn=*/1, /*isP1=*/isP1);

                bool clickStateChanged = false;
                if (isP1) clickStateChanged = (F.hold != Ghosts::I().p1Hold);
                else clickStateChanged = (F.hold != Ghosts::I().p2Hold);

                if (clickStateChanged) {
                    if (F.hold) p->pushButton(PlayerButton::Jump);
                    else p->releaseButton(PlayerButton::Jump);
                }

                //if (F.hold) {
                //    p->pushButton(PlayerButton::Jump);
                //    if (currentMode(p)==IconType::Spider) p->releaseButton(PlayerButton::Jump);
                //}
                //else p->releaseButton(PlayerButton::Jump);
                
                prevJump = F.hold;
            }
            if (F.holdL != prevLeft) {
                if (F.holdL) p->pushButton(PlayerButton::Left);
                else p->releaseButton(PlayerButton::Left);
                prevLeft = F.holdL;
            }
            if (F.holdR != prevRight) {
                if (F.holdR) p->pushButton(PlayerButton::Right);
                else p->releaseButton(PlayerButton::Right);
                prevRight = F.holdR;
            }
        }
    }

    void forcePlayerHoldState_(PlayerObject* p, bool holdJump, bool holdL, bool holdR) {
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
};

// Hey you survived looking through this file! Now go drink water
