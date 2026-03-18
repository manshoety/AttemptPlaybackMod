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

// #include <cvolton.level-id-api/include/EditorIDs.hpp>
// #include "../core/seed_utils.hpp"

// Current visual bugs:
// Wave trail wackyness when turning off (only show past percent) when replaying with waves at the start
// Replay practice session, then record a practice session entering by pressing practice mode during replay, then replay and icons are the wrong gamemode 

// Features I'll (maybe) add:
// Ability to only replay one practice session or multiple (maybe even name them and save names as a different file). This would be it's own UI. Also have ability to delete certain sessions and maybe even export and import sessions (practice and normal). It would be cool to be able to export your best attempt, and be able to import the best attempt of several people and replay the at the same time, and have the icons maybe exported and imported too? A lot of work but would be cool to have eventually.
// Platformer mode isn't implimented when there are checkpoints yet (practice mode works but not normal)

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

    int getPlaybackOnlyPastPercentThreshold() {
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
        restartLevel();
    }

    void setPlaybackMaxVisibleGhosts(int maxVisible) {
        m_maxVisibleGhosts = maxVisible;
        Mod::get()->setSavedValue("max-visible-ghosts", maxVisible);
    }

    void setPlaybackOnlyPastPercentThreshold(float percent) {
        m_GhostsPassedPercentThreshold = percent;
        Mod::get()->setSavedValue("ghosts-passed-percent-threshold", percent);
        buildGhostThatPassedPercentSerialList();
        rebuildGridThreasholdSet_();
        //log::info("setPlaybackOnlyPastPercentThreshold: {}", percent);
    }

    void buildGhostThatPassedPercentSerialList() {
        m_ghostsThatPassThePercentThreshold.clear();
        m_playerObjectPool.clearOwners(true);
        for (size_t i = 0; i < attempts.size(); ++i) {
            if (!attempts[i].practiceAttempt && attempts[i].endPercent >= m_GhostsPassedPercentThreshold) {
                m_ghostsThatPassThePercentThreshold.push_back(i);
            } else {
                attempts[i].setP1Visible(false, true);
                attempts[i].setP2Visible(false, true);
            }
        }
        m_initialAttemptsToSet.clear();
        buildInitialAttemptsFromPreloadOrder(m_ghostsThatPassThePercentThreshold, m_initialAttemptsToSet);
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
            m_loadedSerials.clear();
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
            m_checkpointMgr.attach(m_pl);
            invalidateAttemptCounts();
            stopPlayback();
            stopReplay();
            
            recording = false;
            toggleRecording();
            m_pl->resetLevel();
            
            //log::info("[Delete] After clear: attempts.size()={}", attempts.size());
        } else {
            log::warn("Failed to delete save file: {} (error: {})", 
                    geode::utils::string::pathToString(path), ec.message());
        }
        
        return success;
    }

    std::string getCurrentLevelSaveFileName() const {
        auto path = fileForLevel_(m_levelIDOnAttach);
        return path.filename().string();
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
        auto getEndT = [&](int attemptIndex) -> double {
            const Attempt& a = attempts[attemptIndex];
            return (!a.p1.empty()) ? a.p1.back().t : 0.0;
        };

        std::vector<uint32_t> sorted = preloadOrder;

        std::sort(sorted.begin(), sorted.end(),
            [&](int lhs, int rhs) {
                const double tL = getEndT(lhs);
                const double tR = getEndT(rhs);
                if (tL != tR) return tL < tR;
                return lhs < rhs;
            });

        const size_t initialCount = std::min(
            static_cast<size_t>(m_playerObjectPool.capacity()),
            sorted.size()
        );

        outInitial.assign(sorted.begin(), sorted.begin() + initialCount);
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

        const auto wantPractice = practiceFilterEvenWhenBotNotOn();

        int readyNow = 0;
        int emptySkipped = 0;
        int totalMatching = wantPractice ? m_cachedPracticeAttempts : m_cachedNormalAttempts;

        std::vector<int> candidateIds;
        candidateIds.reserve(totalMatching);

        auto getEndT = [&](int attemptIndex) -> double {
            const Attempt& a = attempts[attemptIndex];
            return (!a.p1.empty()) ? a.p1.back().t : 0.0;
        };

        candidateIds = m_preloadableIndexesInAttemptsList;
        /*

        for (int attemptIndex : m_preloadableIndexesInAttemptsList) {
            Attempt& a = attempts[attemptIndex];

            //if (isAttemptPreloaded_(a)) {
            //    ++readyNow;
            //} else {
            candidateIds.push_back(attemptIndex);
            if (a.p1.empty()) ++emptySkipped;
            //}
        }*/

        m_preloadTarget = std::min(targetCount, totalMatching);
        m_preloadLoaded = std::min(readyNow, m_preloadTarget);

        const int remainingNeeded = std::max(0, m_preloadTarget - m_preloadLoaded);

        if (candidateIds.empty() || remainingNeeded <= 0) {
            m_preloadActive = false;
            return;
        }

        // Sort candidates by endT first
        std::sort(candidateIds.begin(), candidateIds.end(),
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
                    targetCount / 2,
                    static_cast<int>(std::sqrt(static_cast<float>(candidateIds.size())))
                )
            )
        );

        const int numBuckets = std::max(
            1,
            std::min(desiredBuckets, static_cast<int>(candidateIds.size()))
        );

        std::vector<std::vector<int>> buckets(numBuckets);

        for (size_t i = 0; i < candidateIds.size(); ++i) {
            const int bucket = std::min(
                numBuckets - 1,
                static_cast<int>((i * static_cast<size_t>(numBuckets)) / candidateIds.size())
            );
            buckets[bucket].push_back(candidateIds[i]);
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

        m_preloadOrder.reserve(std::min<int>(remainingNeeded, static_cast<int>(candidateIds.size())));
        std::vector<size_t> bucketCursors(numBuckets, 0);

        while (static_cast<int>(m_preloadOrder.size()) < remainingNeeded) {
            bool anyRemaining = false;

            for (int b : bucketVisitOrder) {
                if (bucketCursors[b] < buckets[b].size()) {
                    m_preloadOrder.push_back(buckets[b][bucketCursors[b]++]);
                    anyRemaining = true;

                    if (static_cast<int>(m_preloadOrder.size()) >= remainingNeeded) {
                        break;
                    }
                }
            }

            if (!anyRemaining) break;

            // Rotate the visit order each round so the same bucket doesn't keep getting "first pick" on every pass
            if (bucketVisitOrder.size() > 1) {
                std::rotate(bucketVisitOrder.begin(),
                            bucketVisitOrder.begin() + 1,
                            bucketVisitOrder.end());
            }
        }

        // Build the smaller real player object initial set list from the first N
        // preloadOrder entries, then sort that list by endT
        buildInitialAttemptsFromPreloadOrder(m_preloadOrder, m_initialAttemptsToSet);
        // do the ones I set the icon for first
        prioritizeInitialAttemptsInPreloadOrder_();

        m_preloadCursor = 0;
        m_preloadActive = (m_preloadLoaded < m_preloadTarget) && !m_preloadOrder.empty();
    }

    void prioritizeInitialAttemptsInPreloadOrder_() {
        if (m_initialAttemptsToSet.empty() || m_preloadOrder.empty()) return;

        std::unordered_set<uint32_t> initialSet(
            m_initialAttemptsToSet.begin(),
            m_initialAttemptsToSet.end()
        );

        std::unordered_set<uint32_t> added;
        std::vector<uint32_t> reordered;
        reordered.reserve(m_preloadOrder.size());

        // First: force the initial list to the front, in its own order.
        for (int idx : m_initialAttemptsToSet) {
            if (added.insert(idx).second) {
                reordered.push_back(idx);
            }
        }

        // Then: append the rest of the preload order, skipping anything already added.
        for (int idx : m_preloadOrder) {
            if (added.insert(idx).second) {
                reordered.push_back(idx);
            }
        }

        m_preloadOrder = std::move(reordered);
    }

    void preloadStep(int maxPerTick) {
        if (isPureRecordingMode_()) {
            m_preloadActive = false;
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
            return;
        }

        if (!m_preloadActive || maxPerTick <= 0) return;

        const auto wantPractice = practiceFilterEvenWhenBotNotOn();

        int work = 0;
        int skippedAlreadyLoaded = 0;
        int skippedNoData = 0;
        
        while (work < maxPerTick &&
            m_preloadLoaded < m_preloadTarget &&
            m_preloadCursor < m_preloadOrder.size())
        {
            const size_t idx = m_preloadOrder[m_preloadCursor];
            Attempt& a = attempts[idx];
            //log::info("preload idx: {}, already preloaded: {}", idx, a.preloaded);
            preloadAttempt_(a, idx);
            //log::info("preloaded: {}, iconsAssigned: {}", a.preloaded, a.iconsAssigned);
            if (m_replayKind == ReplayKind::PracticeComposite) InitialGamemodeSetProcessAttemptIdx(idx);
            if (a.preloaded) {
                ++m_preloadLoaded;
                ++work;
            }
            m_preloadCursor++;
        }

        if (m_preloadLoaded >= m_preloadTarget || m_preloadCursor >= m_preloadOrder.size()) {
            m_preloadActive = false;
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
        }
    }

    void InitialGamemodeSetProcessAttemptIdx(size_t attemptIdx) {
        if (attemptIdx >= attempts.size()) return;
        if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 1) return;

        Attempt& a = attempts[attemptIdx];

        auto h = m_playerObjectPool.acquireForOwner(a.serial);
        if (h) { a.g1Idx = h.index; a.g1 = h.po; }

        if (a.hadDual) {
            auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000);
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
        if (!orderList || orderList->empty() || attempts.empty()) return;

        const auto& order = *orderList;
        const int end = std::min(startIndex + batchSize, static_cast<int>(order.size()));

        for (int i = startIndex; i < end; ++i) {
            const int attemptIdx = static_cast<int>(order[i]);

            if (attemptIdx < 0 || attemptIdx >= static_cast<int>(attempts.size()))
                continue;

            Attempt& a = attempts[attemptIdx];

            if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 1) {
                return;
            }

            auto h = m_playerObjectPool.acquireForOwner(a.serial);
            if (h) {
                a.g1Idx = h.index;
                a.g1 = h.po;
            }

            if (a.hadDual) {
                auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000);
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

    void InitialGamemodeSet() {
        m_playerObjectPool.clearOwners(true);
        InitialGamemodeSetProcessBatch(&m_initialAttemptsToSet, m_playerObjectPool.capacity());
    }

    bool isPreloadingAttempts() const { return m_preloadActive; }
    int getPreloadLoadedCount() const { return m_preloadLoaded; }
    int getPreloadTargetCount() const { return m_preloadTarget; }
    bool isPlayerFrozen() const { return m_freezePlayerXAtEnd; }

    void resetAttemptPreloadState_(Attempt& a, int idx) {
        a.preloaded = false;
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
        m_playerObjectPool.clearOwners(true);
            attemptBuffer.clear();
            m_primedIndices.clear();
            m_primedSet.clear();
            m_wantToPrimeIndices.clear();
            m_wantToPrimeSet.clear();
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
        //LoadFilter f;
        //loadFromDisk_(levelID, f);
    }

    void renumberCurrentAttemptIfFresh() {
        if (m_current.p1.empty() && m_current.p2.empty()) {
            m_current.serial = m_nextAttemptSerial++;
        }
    }

    void appendAttemptChunk_(std::ofstream& out, Attempt const& a) {
        const uint32_t p1N = static_cast<uint32_t>(a.p1.size());
        const uint32_t p2N = static_cast<uint32_t>(a.p2.size());

        APXMeta m{};
        m.serial = static_cast<uint32_t>(a.serial);
        m.startPercent = a.startPercent;
        m.endPercent = a.endPercent;
        m.flags = 0;
        if (a.practiceAttempt) m.flags |= 1u;
        if (a.completed) m.flags |= (1u << 1);
        if (a.hadDual) m.flags |= (1u << 2);
        m.p1Count = p1N;
        m.p2Count = p2N;
        m.startX = a.startX;
        m.startY = a.startY;
        m.startCheckpointId = a.startCheckpointId;
        m.baseTimeOffset = a.baseTimeOffset;
        m.seed = static_cast<uint64_t>(a.seed);

        const uint32_t payloadSz =
            static_cast<uint32_t>(sizeof(APXMeta)) +
            p1N * static_cast<uint32_t>(sizeof(APXFrame)) +
            p2N * static_cast<uint32_t>(sizeof(APXFrame));

        const uint32_t type = kChunk_ATTZ;

        out.write(reinterpret_cast<const char*>(&type), sizeof(type));
        out.write(reinterpret_cast<const char*>(&payloadSz), sizeof(payloadSz));
        out.write(reinterpret_cast<const char*>(&m), sizeof(m));

        // Write frames
        auto writeFrames = [&](std::vector<Frame> const& src) {
            for (auto const& f : src) {
                APXFrame s{};
                s.x = f.x;
                s.y = f.y;
                s.rot = f.rot;
                s.vehicleSize = f.vehicleSize;
                s.waveSize = f.waveSize;
                s.mode = static_cast<uint8_t>(f.mode);
                s.t = f.t;
                flagsFromFrame_(f, s);
                out.write(reinterpret_cast<const char*>(&s), sizeof(s));
            }
        };
        writeFrames(a.p1);
        writeFrames(a.p2);
    }


    int saveNewAttemptsForLevel_(int levelID) {
        if (!m_pl) return 0;
        if (m_isSaving) return 0;
        m_isSaving = true;
        
        auto path = fileForLevel_(levelID);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        ensureHeader_(path);

        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) {
            m_isSaving = false;
            return 0;
        }

        int written = 0;
        
        for (auto& a : attempts) {
            if (!attemptHasData_(a)) continue;
            if (!a.recordedThisSession) continue;
            if (a.persistedOnDisk) continue;

            appendAttemptChunk_(out, a);
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
        //log::info("[APX save] Saved {} attempt(s) to {}", written, geode::utils::string::pathToString(path));
        
        m_isSaving = false;
        return written;
    }


    int loadFromDisk_(int levelID, LoadFilter const& flt) {
        if (m_loadedLevelID == levelID && !attempts.empty()) {
            return 0;
        }

        if (m_loadedLevelID != levelID) {
            m_checkpointMgr = CheckpointManager();
            if (m_pl) m_checkpointMgr.attach(m_pl);
        }

        if (m_loadedLevelID == levelID && !attempts.empty()) {
            return 0;
        }
        
        auto path = fileForLevel_(levelID);
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return 0;
        }

        // Validate header
        APXHeader h{};
        in.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!in || std::string(h.magic, h.magic + 4) != "APX2") {
            log::warn("[APX load] bad header for {}", geode::utils::string::pathToString(path));
            return 0;
        }

        const bool isVersion5 = (h.version >= 5);
        const bool isVersion4 = (h.version == 4);
        const bool isVersion3 = (h.version == 3);
        const bool isVersion2 = (h.version == 2);
        // log::info("[APX load] File version {}", h.version);

        size_t loaded = 0;
        size_t nonPracticeLoaded = 0;
        uint32_t maxSerialSeen = 0;

        while (in) {
            uint32_t type = 0, sz = 0;
            in.read(reinterpret_cast<char*>(&type), sizeof(type));
            in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
            if (!in) break;

            std::streampos chunkStart = in.tellg();

            if (type == kChunk_ATTZ) {
                std::streampos attzChunkStart = in.tellg();

                if (isVersion5) {
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

                    if (flt.practice.has_value() && flt.practice.value() != isPractice) loadThis = false;
                    if (flt.minPct.has_value() && m.startPercent < flt.minPct.value()) loadThis = false;
                    if (flt.maxPct.has_value() && m.startPercent > flt.maxPct.value()) loadThis = false;
                    if (loadThis && !isPractice && flt.maxNonPractice > 0
                        && nonPracticeLoaded >= flt.maxNonPractice) loadThis = false;

                    auto skipToEndOfChunk = [&]() {
                        auto now = in.tellg();
                        auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                        auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                        if (remain > 0) in.seekg(remain, std::ios::cur);
                    };

                    // Read frames
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
                            f.frame = i;
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
                    a.startCheckpointId = m.startCheckpointId;
                    a.baseTimeOffset = m.baseTimeOffset;
                    a.seed = static_cast<uintptr_t>(m.seed);

                    readFrames(a.p1, m.p1Count);
                    readFrames(a.p2, m.p2Count);

                    if (!a.p1.empty()) {
                        a.startX = a.p1.front().x;
                        a.startY = a.p1.front().y;
                        a.endX = a.p1.back().x;
                    } else if (!a.p2.empty()) {
                        a.startX = a.p2.front().x;
                        a.startY = a.p2.front().y;
                        a.endX = a.p2.back().x;
                    }

                    a.persistedOnDisk = true;
                    a.recordedThisSession = false;

                    m_loadedSerials.insert(a.serial);
                    pushAttempt(std::move(a), /*spawnNow=*/false);

                    ++loaded;
                    if (!isPractice) ++nonPracticeLoaded;

                    auto now = in.tellg();
                    auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                    auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                }

            } else if (type == kChunk_PATH) {
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
                // Legacy chunks - skip
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            } else {
                // Unknown chunk - skip
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            }
        }

        m_loadedLevelID = levelID;

        if (m_nextAttemptSerial <= static_cast<int>(maxSerialSeen)) {
            m_nextAttemptSerial = static_cast<int>(maxSerialSeen) + 1;
        }

        log::info("[APX load] Loaded {} attempts from {} (version {})", loaded, geode::utils::string::pathToString(path), h.version);
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

    void updateOpacityForAttempt(Attempt& a, GLubyte o) {
        m_skipGhostWork = (o == 0);
        if (a.opacity == o) return;
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

        onQuit();

        for (auto& a : attempts) {
            a.g1 = nullptr;
            a.g2 = nullptr;
        }
        m_current.g1 = nullptr;
        m_current.g2 = nullptr;
        m_replayOwnerIndex = -1;

        m_pl = pl;
        m_checkpointMgr.attach(pl);

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

        attemptBuffer.clear();
        practiceAttemptReplayIndex.clear();
        currentAttemptImmune = false;

        m_checkpointNodes.clear();
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

        getPlaybackLimitVisibleGhostsEnabled();
        getPlaybackMaxVisibleGhosts();
        getPlaybackOnlyPastPercentEnabled();
        getPlaybackOnlyPastPercentThreshold();
        buildGhostThatPassedPercentSerialList();
    }

    void initBotAfterReset_() {
        if (!botActive || !m_pl || !m_pl->m_player1) return;

        m_replayIdx1 = m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = 0;

        botPrevHold1 = botPrevHold2 = false;
        botPrevHoldL1 = botPrevHoldL2 = false;
        botPrevHoldR1 = botPrevHoldR2 = false;

        const Attempt* owner = nullptr;
        
        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_compOwnerIdx < attempts.size()) {
                owner = &attempts[m_compOwnerIdx];
                m_replayAttempt = owner;
                m_replayOwnerSerial = owner->serial;
            }
        } else {
            double startTime = m_checkpointBaseTime;
            int serial = m_checkpointMgr.findOwnerSerialForTime(startTime);
            if (serial > 0) {
                owner = findAttemptBySerial_(serial);
                m_replayOwnerSerial = serial;
            }
            m_replayAttempt = nullptr;
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
        double startTime = m_checkpointBaseTime;
        m_currentSessionTime = startTime;
        m_lastAttemptTime = startTime;
        m_prevSessionTime = startTime;
        
        adjustReplayCursorToTime_(m_lastAttemptTime);

        m_justStartedBot = false;
    }

    void onDeath() {
        if (!m_pl) return;
        if (m_justDied) return;
        m_justDied = true;

        if (m_pl->m_isPracticeMode) {
            if (recording && recordInPractice && !m_current.p1.empty()) {
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

            if (recording && recordInPractice && !m_current.p1.empty() && !block_attempt_push_on_recording_start) {
                pushAttempt(std::move(m_current));
            }

            return;
        }

        if (recording && !m_current.p1.empty() && !block_attempt_push_on_recording_start) {
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

        // log::info("onReset");

        m_freezeRemoveCheckpointDuringCompletionCleanup = false;

        //log::info("player x: {}, y: {}", m_pl->m_player1->m_positionX, m_pl->m_player1->m_positionY);
        //log::info("playerV2 x: {}", playerX_());

        if (m_pl && m_pl->m_isPracticeMode && recording) {
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
        if (recording && !m_current.p1.empty()) {
            pushAttempt(std::move(m_current));
        }

        startNewAttempt();

        m_currentSessionTime = 0.0;
        m_prevSessionTime = 0.0;

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

        if (m_pl && m_pl->m_isPracticeMode) {
            const Checkpoint* topCheckpoint = m_checkpointMgr.topCheckpoint();
            if (topCheckpoint) {
                m_checkpointBaseTime = topCheckpoint->checkpointTime;
            } else {
                m_checkpointBaseTime = 0.0;
            }
        } else {
            m_checkpointBaseTime = 0.0;
        }
        
        // Only set current session time when bot is not active
        //if (!botActive) {
        //    m_currentSessionTime = m_checkpointBaseTime;
        //}

        if (isPureRecordingMode_()) {
            return;
        }

        if (!botActive) {
            playback = showWhilePlaying;
            m_playbackArmed = playback;
        }

        if (m_useCheckpointsRoute && m_replayKind == ReplayKind::PracticeComposite && routeIsComposite_()) {
            if (botActive || showWhilePlaying) {
                buildCompositePracticePath();
                rebuildRouteOwnerCache_();
            }
            if (botActive) tagCompositeOwnersHidden_(true);
        }

        if (botActive) {
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
            for (size_t idx : m_primedIndices) {
                if (idx < attempts.size()) {
                    Attempt& a = attempts[idx];
                    a.resetPlayback();
                    if (a.g1) {
                        a.setP1Visible(false, true);
                        //a.g1->m_waveTrail->setVisible(false);
                        //a.g1->m_waveTrail->setOpacity(0);
                        //a.g1->m_waveTrail->reset();
                    }
                    if (a.g2) {
                        a.setP2Visible(false, true);
                        //a.g2->m_waveTrail->setVisible(false);
                        //a.g2->m_waveTrail->setOpacity(0);
                        //a.g2->m_waveTrail->reset();
                    }
                }
            }
            m_primedIndices.clear();
            m_primedSet.clear();
            m_wantToPrimeIndices.clear();
            m_wantToPrimeSet.clear();
            //m_playerObjectPool.clearOwners(true);
            // Reset practice mode player objects
            InitialGamemodeSet();
        }
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
        
        flushPendingSaves_();
        
        botActive = false;
        playback = false;
        recording = false;
        m_filterByStartPosition = false;
        applyBotSafety_(false);

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
        m_lastWinSerial = -1;
        m_filterByStartPosition = false;
        m_currentReplayStartPos.x = 0.f;
        m_currentReplayStartPos.y = 0.f;

        attemptBuffer.clear();
        practiceAttemptReplayIndex.clear();
        currentAttemptImmune = false;

        m_prevPractice = false;
        m_pendingRearm = false;

        m_checkpointNodes.clear();
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
        m_practiceCompositeOwnerSerial = -1;
        m_activeOwnerSerial = -1;
        m_activeRoute = ActiveRoute::CompositeCheckpoint;
        m_attemptsPreloadedTotal = 0;

        m_is_quitting = false;

        m_playerObjectPool.clearAfterGhostPoolCleared();
    }

    void onComplete() {
        m_justDied = true;
        m_freezeRemoveCheckpointDuringCompletionCleanup = true;

        if (!m_current.p1.empty()) {
            const int serial = m_current.serial;
            const float endX = m_current.p1.back().x;
            const float endY = m_current.p1.back().y;
            const size_t p1Count = m_current.p1.size();
            const size_t p2Count = m_current.p2.size();

            if (m_pl && m_pl->m_isPracticeMode && recording && recordInPractice) {
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

        resolvePendingCheckpointOwners_();
    }

    void saveCurrentAttemptNow() {
        if (botActive || playback) return;
        if (!recording) return;

        // Don't save practice attempts unless explicitly enabled.
        if (m_pl && m_pl->m_isPracticeMode && !recordInPractice) {
            return;
        }

        if (m_current.p1.size() > 2) {
            //geode::log::warn("PUSH saveCurrentAttemptNow");
            pushAttempt(std::move(m_current));
            resolvePendingCheckpointOwners_();
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
                m_pl->resetLevel();
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
        invalidateAttemptPointerCaches_();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        onlyBestGhost = false;
        updateGhostVisibility();
        attemptBuffer.clear();
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
        // setPOFrameForIcon(po, f.mode, a, m_randomIcons);
        syncGhostGravity(po, f.upsideDown, pl);
        if (po->m_vehicleSize != f.vehicleSize) {
            po->m_vehicleSize = f.vehicleSize;
            po->updatePlayerScale();
        }
        po->setRotation(f.rot);
        po->setPosition({ f.x, f.y });
    }

    void buildCompositeFromAttempt(size_t idx) {
        m_activeRoute = ActiveRoute::SingleAttempt;
        m_replayAttempt = nullptr;
        
        if (idx >= attempts.size()) {
            m_compOwnerIdx = SIZE_MAX;
            return;
        }
        
        auto& a = attempts[idx];
        
        //if (!a.acc1.valid() && !a.p1.empty()) {
        //    buildAccel(a.p1, a.acc1, 64.f);
        //}
        //if (!a.acc1Time.valid() && !a.p1.empty()) {
        //    buildAccelTime(a.p1, a.acc1Time);
        //}
        //if (a.hadDual && !a.acc2.valid() && !a.p2.empty()) {
        //    buildAccel(a.p2, a.acc2, 64.f);
        //}
        //if (a.hadDual && !a.acc2Time.valid() && !a.p2.empty()) {
        //    buildAccelTime(a.p2, a.acc2Time);
        //}
        
        m_compOwnerIdx = idx;
        m_replayAttempt = &a;
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

    void buildCompositePracticePath(std::optional<bool> wantPractice = std::optional<bool>(true)) {
        m_activeRoute = ActiveRoute::CompositeCheckpoint;
        m_compOwnerIdx = SIZE_MAX;
        m_replayAttempt = nullptr;
        m_replayOwnerSerial = -1;
        
        m_nullOwnerSpanSkips = 0;
        m_spans.clear();

        std::vector<PracticeSegment> replaySegs = m_checkpointMgr.getReplaySequence();
        
        if (replaySegs.empty()) {
            log::warn("[Practice Composite] No segments in selected session");
            return;
        }
        
        std::string dbg = "[Practice Composite] Building spans from segments:";
        
        // Build spans for visibility culling and owner tracking
        for (size_t i = 0; i < replaySegs.size(); ++i) {
            const auto& seg = replaySegs[i];
            
            if (seg.ownerSerial <= 0) {
                dbg += fmt::format("\n  [{:.1f}, {:.1f}) -> NO OWNER (skipped)", seg.startX, seg.endX);
                ++m_nullOwnerSpanSkips;
                continue;
            }
            
            // Verify owner exists
            const Attempt* owner = findAttemptBySerial_(seg.ownerSerial);
            if (!owner || owner->p1.empty()) {
                owner = findBestAttemptForRange_(seg.startX,
                    seg.checkpointIdEnd == -1 ? seg.maxXReached : seg.endX,
                    wantPractice);
            }
            
            if (!owner || owner->p1.empty()) {
                dbg += fmt::format("\n  [{:.1f}, {:.1f}) -> NO VALID OWNER (skipped)", seg.startX, seg.endX);
                ++m_nullOwnerSpanSkips;
                continue;
            }
            
            float effectiveEnd = (seg.checkpointIdEnd == -1) ? seg.maxXReached : seg.endX;
            
            m_spans.push_back(Span{
                double(seg.startX), 
                double(effectiveEnd),
                owner->serial,
                seg.p1Frames,
                seg.p2Frames
            });
            
            dbg += fmt::format("\n  [{:.1f}, {:.1f}) -> serial={}", 
                seg.startX, effectiveEnd, owner->serial);
        }
        
        if (m_nullOwnerSpanSkips > 0) {
            dbg += fmt::format("\n  WARNING: {} span(s) had no owner", m_nullOwnerSpanSkips);
        }

        // log::info("[Practice Composite] {}", dbg);
        
        if (botActive) tagCompositeOwnersHidden_(true);
    }

    void syncInputsToCurrentIndexP1_() {
        if (!m_pl) return;
        if (!m_currentOwner) return;
        if (m_replayOwnerIndex==-1) return;

        if (m_pl->m_player1 && !m_currentOwner->p1.empty() && m_replayIdx1 < m_currentOwner->p1.size()) {
            const Frame& F = m_currentOwner->p1[m_replayIdx1];
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
        m_lastEmitIdx1 = m_replayIdx1;
    }

    void syncInputsToCurrentIndexP2_() {
        if (!m_pl) return;
        if (!m_currentOwner) return;
        if (m_replayOwnerIndex==-1) return;

        if (m_pl->m_player2 && m_currentOwner->hadDual && !m_currentOwner->p2.empty() && m_replayIdx2  < m_currentOwner->p2.size()) {
            const Frame& F2 = m_currentOwner->p2[m_replayIdx2];
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
        m_lastEmitIdx2 = m_replayIdx2;
    }

    void startReplayPractice() {
        m_freezePlayerXAtEnd = false;
        m_chainAuthoritative = true;
        resolvePendingCheckpointOwners_();
        invalidateAttemptCounts();

        m_filterByStartPosition = true;

        if (!m_pl || !m_pl->m_player1) {
            log::warn("[Bot] startReplayPractice aborted: pl={}, p1={}",
                (bool)m_pl, m_pl && m_pl->m_player1);
            m_filterByStartPosition = false;
            return;
        }

        if (isPracticeMode()) togglePracticeMode(false);

        block_attempt_push_on_recording_start = true;
        m_pl->resetLevel();
        block_attempt_push_on_recording_start = false;

        m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
        m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;

        m_checkpointMgr.selectBestSessionForStartX_RankByTime(
            m_currentReplayStartPos.x,
            kReplayStartTolerance
        );

        m_replayKind = ReplayKind::PracticeComposite;

        buildCompositePracticePath(std::optional<bool>(true));

        rebuildRouteOwnerCache_();
        
         std::vector<PracticeSegment> replaySegs = m_checkpointMgr.getReplaySequence();
        if (replaySegs.empty()) {
            log::warn("[Bot] startReplayPractice: no replay segments");
            m_filterByStartPosition = false;
            return;
        }

        const double sessionT0 = replaySegs.front().absStart();
        m_checkpointBaseTime = sessionT0;
        
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

        if (m_replayOwnerSerial > 0) {
            m_activeOwnerSerial = m_replayOwnerSerial;
        }

        double endTime = m_checkpointMgr.replayEndTime();
        
        if (endTime > 0.1) {
            int tailSerial = m_checkpointMgr.findOwnerSerialForTime(endTime);
            
            if (tailSerial > 0) {
                const Attempt* tailOwner = findAttemptBySerial_(tailSerial);
                if (tailOwner && !tailOwner->p1.empty()) {
                    // Find the frame closest to endTime
                    size_t endIdx = idxForTime(tailOwner->acc1Time, tailOwner->p1, endTime);
                    if (endIdx < tailOwner->p1.size()) {
                        m_freezePlayerX = tailOwner->p1[endIdx].x;
                        m_freezePlayerY = tailOwner->p1[endIdx].y;
                    } else {
                        m_freezePlayerX = tailOwner->p1.back().x;
                        m_freezePlayerY = tailOwner->p1.back().y;
                    }
                    
                    if (!tailOwner->p2.empty()) {
                        size_t endIdx2 = idxForTime(tailOwner->acc2Time, tailOwner->p2, endTime);
                        if (endIdx2 < tailOwner->p2.size()) {
                            m_freezePlayerXP2 = tailOwner->p2[endIdx2].x;
                            m_freezePlayerYP2 = tailOwner->p2[endIdx2].y;
                        } else {
                            m_freezePlayerXP2 = tailOwner->p2.back().x;
                            m_freezePlayerYP2 = tailOwner->p2.back().y;
                        }
                    } else {
                        m_freezePlayerXP2 = 0.f;
                        m_freezePlayerYP2 = 0.f;
                    }
                    
                    // log::info("[Bot] Set freeze position for endTime={:.3f}: ({:.1f}, {:.1f})", 
                    //     endTime, m_freezePlayerX, m_freezePlayerY);
                }
            }
        } else {
            log::warn("[Bot] No valid end time data (endTime={:.3f}), freeze disabled", endTime);
            m_freezePlayerX = 0.f;
            m_freezePlayerY = 0.f;
            m_freezePlayerXP2 = 0.f;
            m_freezePlayerYP2 = 0.f;
        }
        
        m_pl->resetLevel();
    }

    void startReplayBest() {
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;

        resolvePendingCheckpointOwners_();

        if (!m_pl || !m_pl->m_player1 || attempts.empty()) {
            log::warn("[Bot] startReplayBest aborted: pl={}, p1={}, attempts={}",
                    (bool)m_pl, m_pl && m_pl->m_player1, attempts.size());
            return;
        }

        if (isPracticeMode()) togglePracticeMode(false);

        //log::info("[Bot] startReplayBest: {} total attempts", attempts.size());

        m_replayKind = ReplayKind::BestSingle;

        block_attempt_push_on_recording_start = true;
        botActive = false;
        playback = false;
        
        m_pl->resetLevel();

        m_currentReplayStartPos.x = m_pl->m_player1->m_positionX;
        m_currentReplayStartPos.y = m_pl->m_player1->m_positionY;

        m_filterByStartPosition = true;
        
        block_attempt_push_on_recording_start = false;
        
        size_t idx = pickWinningOrBestIndexFromPosition_(std::optional<bool>(false), m_currentReplayStartPos.x, kReplayStartTolerance);

        m_replayOwnerIndex = idx;

        //log::info("[Bot] startReplayBest: pickWinningOrBestIndexFromPosition returned idx={}", 
        //    idx == SIZE_MAX ? "SIZE_MAX" : std::to_string(idx));

        if (idx == SIZE_MAX) {
            log::warn("[Bot] startReplayBest: no non-practice attempts to replay");
            m_filterByStartPosition = false;
            return;
        }

        //if (idx < attempts.size()) {
        //    const Attempt& dbgAttempt = attempts[idx];
            //log::info("[Bot] startReplayBest: selected attempt serial={}, p1.size={}, startX={:.2f}, endX={:.2f}, completed={}",
            //    dbgAttempt.serial, dbgAttempt.p1.size(), 
            //    dbgAttempt.p1.empty() ? -1.f : dbgAttempt.p1.front().x,
            //    dbgAttempt.p1.empty() ? -1.f : dbgAttempt.p1.back().x,
            //    dbgAttempt.completed);
        //}

        buildCompositeFromAttempt(idx);

        if (m_compOwnerIdx >= attempts.size() || attempts[m_compOwnerIdx].p1.empty()) {
            log::warn("[Bot] startReplayBest: no data (p1 empty) after filtering. m_compOwnerIdx={}", m_compOwnerIdx);
            m_filterByStartPosition = false;
            return;
        }
        
        const Attempt& selectedAttempt = attempts[m_compOwnerIdx];

        //if (selectedAttempt.seed != 0) {
        //    seedutils::applyAttemptSeed(selectedAttempt.seed, m_pl);
            //log::info("[Seed] Applied seed {} from best attempt {} at replay start", selectedAttempt.seed, selectedAttempt.serial);
        //}
        
        //log::info("[Bot] startReplayBest: FINAL selection - serial={}, frames={}, startX={:.2f}, endX={:.2f}",
        //    selectedAttempt.serial,
        //    selectedAttempt.p1.size(),
        //    selectedAttempt.p1.front().x,
        //    selectedAttempt.p1.back().x);

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
        m_pl->resetLevel();
        //log::info("[Bot] startReplayBest: complete, botActive={}", botActive);
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
    }

    void restartLevel() { if (m_pl) m_pl->resetLevel(); }

    bool isBotActive() const { return botActive; }
    bool isReplaying() const { return playback; }
    bool isResetting() const { return resetting; }
    void setResetting(bool on) { resetting = on; }
    bool isdisablePlayerMove() const { return disablePlayerMove; }
    void setdisablePlayerMove(bool on) { disablePlayerMove = on; }

    size_t m_dbgCheckpointCall = 0;
    double s_lastAbsTime = -1.0;
    int    s_lastSerial  = -1;
    float  s_lastX       = NAN;
    size_t s_lastFrame = 0;

    void onPracticeCheckpoint() {
        if (!m_pl || !m_pl->m_player1) return;
        
        const float px = playerX_();
        const float py = m_pl->m_player1->getPositionY();
        float rot1 = 0.f, rot2 = 0.f;
        
        if (m_pl->m_player1) rot1 = m_pl->m_player1->getRotation();
        if (m_pl->m_player2) rot2 = m_pl->m_player2->getRotation();
        
        size_t p1Count = m_current.p1.size();
        size_t p2Count = m_current.p2.size();
        int attemptSerial = m_current.serial;
        
        double absTime = m_current.baseTimeOffset + m_pl->m_attemptTime;

        ++m_dbgCheckpointCall;
        /*
        geode::log::info(
            "[Bot] onPracticeCheckpoint call={} serial={} frame={} attemptTime={:.6f} baseOff={:.6f} absTime={:.6f} lastFrameT={:.6f} x={:.3f}",
            (long long)m_dbgCheckpointCall, attemptSerial, (long long)m_frameCounter,
            (double)m_pl->m_attemptTime, (double)m_current.baseTimeOffset, (double)absTime,
            (double)(m_current.p1.empty() ? -1.0 : m_current.p1.back().t),
            px
        );*/

        if (attemptSerial == s_lastSerial &&
            std::fabs(absTime - s_lastAbsTime) < 1e-4 &&
            std::fabs(px - s_lastX) < 0.5f &&
            m_frameCounter == s_lastFrame) {
            // geode::log::warn("[Bot] Duplicate practice checkpoint suppressed");
            return;
        }

        s_lastSerial = attemptSerial;
        s_lastAbsTime = absTime;
        s_lastX = px;
        s_lastFrame = m_frameCounter;

        m_checkpointMgr.onCheckpointPlaced(px, py, attemptSerial, rot1, rot2, 
                                        p1Count, p2Count, absTime, m_current.baseTimeOffset, m_current.startCheckpointId);
        
        m_current.immuneToIgnorePractice = true;
        m_current.ignoreInPractice = false;
        
        const Checkpoint* topCheckpoint = m_checkpointMgr.topCheckpoint();
        if (topCheckpoint) {
            m_current.startCheckpointId = topCheckpoint->id;
        }
        
        currentAttemptImmune = true;
        attemptBuffer.clear();
        
        if (botActive && m_useCheckpointsRoute) {
            buildCompositePracticePath();
            rebuildRouteOwnerCache_();
        }
        if (botActive) tagCompositeOwnersHidden_(true);
    }

    void onPracticeUndoCheckpoint() {
        if (!m_freezeRemoveCheckpointDuringCompletionCleanup) {
            m_checkpointMgr.onCheckpointRemoved();
            
            const Checkpoint* topCheckpoint = m_checkpointMgr.topCheckpoint();
            m_current.startCheckpointId = topCheckpoint ? topCheckpoint->id : -1;
            
            if (botActive && m_useCheckpointsRoute) {
                buildCompositePracticePath();
                rebuildRouteOwnerCache_();
                m_replayIdx1 = m_replayIdx2 = 0;
                m_didInitialWarp = false;
                m_lastEmitIdx1 = m_lastEmitIdx2 = 0;
                m_replayOwnerSerial = -1;  // Force re-lookup
            }
        }
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
        //log::info("[onPracticeToggle] enabled={}", enabled);

        m_freezeRemoveCheckpointDuringCompletionCleanup = true;

        if (recording && recordInPractice) {
            m_checkpointMgr.onExitEarly();
        }

        if (playback) setActiveGhostsInvisible();
        if (!isRecording()) toggleRecording();

        practiceAttemptReplayIndex.clear();
        attemptBuffer.clear();

        forcePlayersVisible_();
        clearHeldInputs_();

        if (enabled) {
            m_practiceCompositeOwnerSerial = -1;
            m_current.practiceAttempt = true;

            m_checkpointMgr.createNewSession();

            /*
            if (recording && recordInPractice) {
                const PracticeSession* session = m_checkpointMgr.getPath().activeSession();
                if (session) {
                    log::info("[Practice] Started recording for new session id={}", session->sessionId);
                }
            }*/

            m_pendingRearm = false;
        }
        else {
            m_checkpointMgr.freezeForReplay();

            saveCurrentAttemptNow();
            flushPendingSaves_();

            m_pendingRearm = true;
        }

        m_prevPractice = enabled;
    }

    void maybeInitialWarpToReplayStart_() {
        if (!m_pl || !m_pl->m_player1) return;
        if (m_replayKind != ReplayKind::PracticeComposite) return;
        if (m_didInitialWarp) return;

        // Use time 0 to get the first owner
        const Attempt* owner = getReplayOwner_(0.0);
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

    void updateClickState(bool isPlayer1) {
        //log::info("[PLAYER updateClickState] holding: {}", p1Hold);
        if (!m_allowSetPlayerClickState) return;
        if (!m_pl || !botActive || m_is_quitting) return;
        if (!m_currentOwner || m_currentOwner->p1.empty()) return;

        if (m_justStartedBot) {
            m_justStartedBot = false;
        }

        if (isPlayer1) syncInputsToCurrentIndexP1_();
        else syncInputsToCurrentIndexP2_();

        m_allowSetPlayerClickState = false;

    }

    void preUpdateP2() {
        if (!m_allowWorkThisTick || m_is_quitting || !m_pl) return;
        m_prevpx2 = m_px2;
        bool hasP2Now = (m_pl->m_player2 && m_pl->m_player2->getPositionX() != 0.f);
        m_px2 = hasP2Now ? m_pl->m_player2->getPositionX() : m_px;
        m_p2JustSpawned = (hasP2Now && !m_prevHadP2);
        m_prevHadP2 = hasP2Now;
    }

    void preUpdate() {
        if (!m_allowWorkThisTick || m_is_quitting || !m_pl) return;
        //double tStart = getTimeMs();

        //log::info("[preUpdate] playerx: {}", m_pl->m_player1->m_positionX);

        //log::info("playertime: {}", m_pl->m_attemptTime);

        // log::info("player z: {}, trail z: {}", m_pl->m_player2->getZOrder(), m_pl->m_player2->m_waveTrail->getZOrder());

        m_px = m_pl->m_player1 ? m_pl->m_player1->getPositionX() : 0.f;
        
        //if (m_pl) m_currentSessionTime = m_pl->m_attemptTime + m_checkpointBaseTime;
        if (m_pl) m_lastAttemptTime = m_pl->m_attemptTime;

        // log::info("Player z order: {}", m_pl->m_player1->getZOrder()); 59

        if (!modEnabled || !botActive || !m_pl || !m_pl->m_player1) return;

        //const float px = playerX_();
        //const float px2 = playerX2_();
        
        bool timeWentBack = (m_lastAttemptTime < m_prevSessionTime - 0.05);
        m_IHateMirrorPortalsSoRewind = timeWentBack;

        int previousOwnerSerial = m_activeOwnerSerial;
        
        adjustReplayCursorToTime_(m_lastAttemptTime);
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
            lastTime = m_checkpointMgr.replayEndTime();
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
        
        if (!m_freezePlayerXAtEnd && m_lastAttemptTime >= lastTime - 0.05) {
            //log::info("[Bot] updateFreezePlayerX_: FREEZING at time={:.3f}, lastTime={:.3f}", 
            //        m_lastAttemptTime, lastTime);
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
        // geode::log::warn("[applySegmentBasedReplay_] m_allowSetPlayerPos: {}", m_allowSetPlayerPos);
        if (!m_allowSetPlayerPos) return;
        if (!m_pl || !botActive || m_is_quitting) return;

        const float px = playerX_();
        const float px2 = playerX2_();

        // geode::log::warn("[applySegmentBasedReplay_] updateFreezeX");

        updateFreezePlayerX_();
        
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
            size_t idx1 = m_replayIdx1;
            if (idx1 >= m_currentOwner->p1.size()) idx1 = m_currentOwner->p1.size() - 1;
            
            if (isPlayer1) applyFrameToPlayer_Only_(m_pl->m_player1, m_currentOwner->p1, idx1, m_lastAttemptTime, false, true);
            else {
                if (m_currentOwner->hadDual && m_pl->m_player2 && !m_currentOwner->p2.empty() && m_pl->m_player2->getPositionX() != 0.f) {
                    size_t idx2 = m_replayIdx2;
                    if (idx2 >= m_currentOwner->p2.size()) idx2 = m_currentOwner->p2.size() - 1;
                    applyFrameToPlayer_Only_(m_pl->m_player2, m_currentOwner->p2, idx2, m_lastAttemptTime, m_p2JustSpawned, false);
                }
            }
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
        size_t idx1 = m_replayIdx1;
        if (idx1 >= m_currentOwner->p1.size()) idx1 = m_currentOwner->p1.size() - 1;
        
        if (isPlayer1) applyFrameToPlayer_Only_(m_pl->m_player1, m_currentOwner->p1, idx1, px, false, true);
        else {
            if (m_currentOwner->hadDual && m_pl->m_player2 && !m_currentOwner->p2.empty()) {
                size_t idx2 = m_replayIdx2;
                if (idx2 >= m_currentOwner->p2.size()) idx2 = m_currentOwner->p2.size() - 1;
                applyFrameToPlayer_Only_(m_pl->m_player2, m_currentOwner->p2, idx2, px2, false, false);
            }
        }
    }

    void recordUpdate(bool isPlayer1) {
        if (!shouldRunWorkThisTick_()) return;
        if (!modEnabled || !m_pl || m_is_quitting) return;

        if (isPureRecordingMode_()) {
            //log::info("[PLAYER] holding {}", p1Hold);
            const bool isPractice = m_pl->m_isPracticeMode;
            if (!isPractice || recordInPractice) {
                recordCurrentFrame_(isPlayer1);
            }
            return;
        }

        const bool isPractice = m_pl->m_isPracticeMode;
        const bool shouldRecord = recording && (!isPractice || recordInPractice);

        if (shouldRecord) {
            recordCurrentFrame_(isPlayer1);
        }
    }

    void recordCurrentFrame_(bool isPlayer1) {
        if (!m_pl) return;
        if (m_justDied) return;
        
        constexpr float kYEqualEps = 0.1f;
        
        double absoluteTime = m_pl->m_attemptTime + m_current.baseTimeOffset;
        
        if (m_pl->m_player1 && isPlayer1) {
            if (!m_current.p1.empty()) {
                double last_t = m_current.p1.back().t;
                // Don't record the same frame again (frame stepper)
                if (absoluteTime == last_t) return;
                // If we moved backwards in time (some mods can do that)
                while (absoluteTime < last_t) {
                    // log::info("Delete P1 frames");
                    m_current.p1.pop_back();
                    if (m_current.p1.empty()) break;
                    last_t = m_current.p1.back().t;
                }
            }
            auto* p1 = m_pl->m_player1;
            Frame f;
            f.t = absoluteTime;
            f.x = p1->getPositionX();
            f.y = p1->getPositionY();
            f.rot = p1->getRotation();
            f.vehicleSize = p1->m_vehicleSize;
            f.waveSize = p1->m_waveTrail ? p1->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p1->m_isUpsideDown;
            f.isDashing = p1->m_isDashing;
            f.isVisible = p1->isVisible();

            if (!m_current.p1.empty()) {
                const float prevY = m_current.p1.back().y;
                f.stateDartSlide = std::fabs(f.y - prevY) <= kYEqualEps;
            } else {
                f.stateDartSlide = false;
            }

            f.mode = currentMode(p1, m_pl->m_isPlatformer);
            f.hold = p1Hold;
            f.holdL = p1LHold;
            f.holdR = p1RHold;
            f.frame = (m_frameCounter >= m_currentAttemptStart) ? 
                    (m_frameCounter - m_currentAttemptStart) : 0;

            m_current.p1.push_back(f);
            m_lastRecordedX = f.x;
            float currentPercent = m_pl->getCurrentPercent();
            if (currentPercent > m_current.endPercent) m_current.endPercent = currentPercent;
        }
        
        if (m_pl->m_player2 && m_pl->m_player2->getPositionX() != 0.f && !isPlayer1) {
            if (!m_current.p2.empty()) {
                double last_t2 = m_current.p2.back().t;
                // Don't record the same frame again (frame stepper)
                if (absoluteTime == last_t2) return;
                // If we moved backwards in time (some mods can do that)
                while (absoluteTime < last_t2) {
                    // log::info("Delete P2 frames");
                    m_current.p2.pop_back();
                    if (m_current.p2.empty()) break;
                    last_t2 = m_current.p2.back().t;
                }
            }
            m_current.hadDual = true;
            auto* p2 = m_pl->m_player2;
            Frame f;
            f.t = absoluteTime;
            f.x = p2->getPositionX();
            f.y = p2->getPositionY();
            f.rot = p2->getRotation();
            f.vehicleSize = p2->m_vehicleSize;
            f.waveSize = p2->m_waveTrail ? p2->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p2->m_isUpsideDown;
            f.isDashing = p2->m_isDashing;
            f.isVisible = p2->isVisible();

            if (!m_current.p2.empty()) {
                const float prevY = m_current.p2.back().y;
                f.stateDartSlide = std::fabs(f.y - prevY) <= kYEqualEps;
            } else {
                f.stateDartSlide = false;
            }

            f.mode = currentMode(p2, m_pl->m_isPlatformer);
            f.hold = p2Hold;
            f.holdL = p2LHold;
            f.holdR = p2RHold;
            f.frame = (m_frameCounter >= m_currentAttemptStart) ? 
                    (m_frameCounter - m_currentAttemptStart) : 0;
            m_current.p2.push_back(f);
        }
        
        if (m_current.practiceAttempt && m_current.startCheckpointId == -1) {
            m_current.startCheckpointId = m_checkpointTopId;
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

        const double dt = m_lastAttemptTime - m_prevSessionTime;
        m_prevSessionTime = m_lastAttemptTime;

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
        
        if (drawGhosts) {
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
                    if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 1) {
                        break;
                    }

                    size_t ai = *it;
                    Attempt& a = attempts[ai];

                    primeGhostToPX_(a, px, px2);

                    m_primedIndices.push_back(ai);
                    m_primedSet.insert(ai);

                    it = m_wantToPrimeIndices.erase(it);

                    if (!a.colorsAssigned) {
                        refreshAttemptColors_(a);
                    }
                    if (a.opacity != opacity) {
                        updateOpacityForAttempt(a, opacity);
                    }

                    // Set visibility
                    if (a.g1) a.setP1Visible(true);
                    // If dead keep last visible state
                    const double ghostTime = std::max(0.0, m_lastAttemptTime + a.ghostOffsetTime);
                    const bool showP2 = a.hadDual && !a.p2.empty() &&
                        (ghostTime >= a.p2.front().t - 0.0001);
                    if (a.g2 && !a.eolFrozenP2) a.setP2Visible(showP2);

                    showable.push_back(ai);
                    GhostsVisibleThisFrame ++;
                    currentPreloaded++;
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
                    if (isOwnerAnywhereInRoute_(ai, a)) {
                        if (a.g1) a.setP1Visible(false);
                        if (a.g2) a.setP2Visible(false);
                        a.primedP1 = a.primedP2 = false;
                        //log::info("[OWNER REJECT] ai: {}", ai);
                        continue;
                    }

                    //log::info("[NOT OWNER] ai: {}", ai);

                    // Time-based visibility calculation
                    const double ghostTime = std::max(0.0, m_lastAttemptTime + a.ghostOffsetTime);
                    const double firstTimeP1 = a.p1.front().t;
                    
                    if (ghostTime < firstTimeP1 - 0.0001) {
                        a.setP1Visible(false);
                        a.setP2Visible(false);
                        a.primedP1 = a.primedP2 = false;
                        //log::info("[Bad Time] ai: {}", ai);
                        continue;
                    }

                    if (a.eolFrozenP1 && a.d1 < a.p1.size()) {
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

                    // Determine P2 visibility
                    const bool showP2 = a.hadDual && !a.p2.empty() &&
                        (ghostTime >= a.p2.front().t - 0.0001);


                    // Prime if needed
                    if (!a.primedP1 || (a.hadDual && showP2 && !a.primedP2)) {
                        
                        if ((m_playerObjectPool.inUseCount() < m_playerObjectPool.capacity() - 1)
                    || a.g1 || a.g2) {
                            //log::info("Priming: {}", ai);
                            primeGhostToPX_(a, px, px2);
                            m_primedIndices.push_back(ai);
                            m_primedSet.insert(ai);
                        }
                        else {
                            //log::info("No Priming space: {}", ai);
                            if (m_wantToPrimeSet.insert(ai).second) {
                                m_wantToPrimeIndices.push_back(ai);
                            }
                            continue;
                        }
                    }
                    //else {
                    //    log::info("a.primedP1: {}, m_playerObjectPool.inUseCount(): {}, m_playerObjectPool.capacity(): {}", a.primedP1, m_playerObjectPool.inUseCount(), m_playerObjectPool.capacity());
                    //}

                    // Screen culling for unprimed ghosts
                    /*
                    if (!a.primedP1) {

                        size_t checkIdx = idxForTimeBounded_(a.p1, a.acc1Time, ghostTime);
                        float checkX = a.p1[checkIdx].x;
                        float checkY = a.p1[checkIdx].y;
                        
                        if (checkX < tl.x || checkX > br.x || 
                            checkY < br.y || checkY > tl.y) {
                            a.setP1Visible(false);
                            a.setP2Visible(false);
                            // log::info("[CULLED np] ai: {} at time-pos ({:.1f}, {:.1f})", ai, checkX, checkY);
                            continue;
                        }
                    }*/

                    // Setup colors and opacity once per visible attempt
                    if (!a.colorsAssigned) {
                        refreshAttemptColors_(a);
                    }
                    if (a.opacity != opacity) {
                        updateOpacityForAttempt(a, opacity);
                    }

                    // Set visibility
                    if (a.g1) a.setP1Visible(true);
                    // If dead keep last visible state
                    if (a.g2 && !a.eolFrozenP2) a.setP2Visible(showP2);

                    //log::info("[VISIBLE GHOST] serial: {}", ai);
                    //if (!a.g1) log::info("[VISIBLE GHOST] NO PLAYER OBJECT");
                    //if (!a.p1Visible) log::info("[VISIBLE GHOST] NOT VISIBLE");
                    //log::info("[VISIBLE GHOST] px: {} gx: {}", px, a.p1[a.d1].x);

                    showable.push_back(ai);
                    GhostsVisibleThisFrame ++;
                }

                // Update visible ghosts
                const int total = static_cast<int>(showable.size());
                if (total > 0) {
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
    bool death_sound_preloaded = false;
    bool m_p2JustSpawned = false;
    bool m_prevHadP2 = false;
    bool m_ghostsExplode = false;
    bool m_ghostsExplodeSFX = false;
    bool m_randomIcons = true;
    bool m_IHateMirrorPortalsSoRewind = false;
    float m_px = 0.f;
    float m_px2 = 0.f;
    float m_prevpx2 = 0.f;
    bool m_p2Moving = false;
    double m_currentSessionTime = 0.0;
    double m_lastAttemptTime = 0.0;
    double m_prevSessionTime = 0.0;
    double m_checkpointBaseTime = 0.0;
    double m_offsetUnitsPerSecond = 311.f;

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

    std::vector<Attempt> attempts;
    Attempt m_current;

    bool botActive = false;
    bool resetting = false;
    bool disablePlayerMove = false;
    bool m_didInitialWarp = false;
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

    size_t m_frameCounter = 0;
    size_t m_currentAttemptStart = 0;
    size_t m_playbackStartTick = 0;

    std::vector<size_t> attemptBuffer;
    std::vector<size_t> practiceAttemptReplayIndex;
    bool currentAttemptImmune = false;

    bool m_prevPractice = false;
    bool m_pendingRearm = false;

    std::vector<CheckpointNode> m_checkpointNodes;
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

    int m_practiceCompositeOwnerSerial = -1;
    std::string m_customSaveId;

    mutable bool m_attemptCountsDirty = true;
    mutable int m_cachedNormalAttempts = 0;
    mutable int m_cachedPracticeAttempts = 0;
    mutable int m_attemptsPreloadedTotal = 0;

    std::unordered_set<int> m_practiceSerialSet;
    std::vector<int> m_preloadableIndexesInAttemptsList;

    geode::Ref<cocos2d::CCNode> m_ghostRoot = nullptr;

    std::unordered_set<int> m_routeOwnerSerials;
    std::unordered_set<int> m_currentlyHiddenSerials;
    std::vector<uint8_t> m_routeOwnerMask;
    bool m_hideAllOwnersDuringPracticeReplay = true;
    bool m_freezeRemoveCheckpointDuringCompletionCleanup = false;

    CheckpointManager m_checkpointMgr;

    mutable std::unordered_map<int, size_t> m_serialToIdx;
    mutable bool m_serialCacheDirty = true;

    static constexpr float kReplayStartTolerance = 30.0f;
    static constexpr float kTolSq = kReplayStartTolerance * kReplayStartTolerance;
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

    void invalidateAttemptPointerCaches_() {
        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerIndex = SIZE_MAX;
        m_compOwnerIdx = SIZE_MAX;
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
        
        float attemptStartX = a.p1.front().x;
        float attemptStartY = a.startY;

        float dx = m_currentReplayStartPos.x - attemptStartX;
        float dy = m_currentReplayStartPos.y - attemptStartY;
        
        return ((dx * dx + dy * dy) < kTolSq);
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
        // log::info("[findAttemptBySerial_] serial: {}", serial);
        rebuildSerialCache_();
        auto it = m_serialToIdx.find(serial);
        
        if (it != m_serialToIdx.end() && it->second < attempts.size()) {
            m_replayOwnerIndex = it->second;
            return &attempts[it->second];
        }
        return nullptr;
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

    FORCE_INLINE bool routeOwnersActive_() const {
        return botActive
            && m_hideAllOwnersDuringPracticeReplay
            && (m_replayKind == ReplayKind::PracticeComposite)
            && routeIsComposite_();
    }

    void rebuildRouteOwnerCache_() {
        m_routeOwnerSerials.clear();
        m_currentlyHiddenSerials.clear();

        if (!routeOwnersActive_())
            return;

        const size_t est = m_spans.size() + 64;
        m_routeOwnerSerials.reserve(est * 2);
        m_currentlyHiddenSerials.reserve(est * 2);

        if (!m_spans.empty()) {
            for (auto const& sp : m_spans) {
                if (sp.ownerSerial > 0) {
                    m_routeOwnerSerials.insert(sp.ownerSerial);
                    m_currentlyHiddenSerials.insert(sp.ownerSerial);
                }
            }
        }

        m_checkpointMgr.gatherSessionOwnerSerials(m_routeOwnerSerials);

        m_currentlyHiddenSerials.reserve(m_currentlyHiddenSerials.size() + m_routeOwnerSerials.size());
        for (int serial : m_routeOwnerSerials) {
            m_currentlyHiddenSerials.insert(serial);
        }
    }

    FORCE_INLINE bool isOwnerAnywhereInRoute_(size_t ai, Attempt const& a) const {
        if (!botActive) return false;
        if (a.ignorePlayback) return true;

        if (m_hideAllOwnersDuringPracticeReplay && 
            (m_replayKind == ReplayKind::PracticeComposite) &&
            routeIsComposite_()) {
            return (m_currentlyHiddenSerials.count(a.serial) != 0) || (m_activeOwnerSerial == a.serial);
        }

        return false;
    }

    FORCE_INLINE bool isCurrentOwner_(size_t ai, const Attempt& a) const {
        if (!routeOwnersActive_()) return false;
        return (a.serial == m_activeOwnerSerial);
    }

    static void buildAccelTime(const std::vector<Frame>& v,
                           FrameAccelTime& a,
                           double binW = 0.0125,
                           bool buildTimes = true) {
        a.reset();
        const size_t n = v.size();
        if (n == 0) return;

        const double minT = v.front().t;
        const double maxT = v.back().t;

        if (!std::isfinite(minT) || !std::isfinite(maxT) || maxT < minT) {
            log::warn("[buildAccelTime] Invalid time data: minT={}, maxT={}", minT, maxT);
            return;
        }
        if (maxT - minT < 1e-9) {
            // log::warn("[buildAccelTime] Time range too small, skipping accel build");
            return;
        }

        a.baseT = minT;
        a.binW = binW;
        a.invBinW = 1.0 / binW;

        int bins = (int)std::ceil((maxT - minT) * a.invBinW) + 1;
        if (bins <= 0 || bins > 10000000) {
            log::warn("[buildAccelTime] Invalid bin count: {}, minT={}, maxT={}, binW={}",
                    bins, minT, maxT, binW);
            a.reset();
            return;
        }

        if (bins > 100000) {
            a.binW = (maxT - minT) / 99999.0;
            a.invBinW = 1.0 / a.binW;
            bins = 100000;
        }

        a.bins = bins;

        a.range.resize((size_t)bins);
        a.idx.resize((size_t)bins);

        if (buildTimes) {
            a.times.resize(n);
            for (size_t i = 0; i < n; ++i) a.times[i] = v[i].t;
        } else {
            a.times.clear();
        }

        const double* tarr = (buildTimes ? a.times.data() : nullptr);
        const Frame*  parr = (!buildTimes ? v.data() : nullptr);

        auto getT = [&](size_t i) -> double {
            return tarr ? tarr[i] : parr[i].t;
        };

        size_t iHi = 0;
        double binEnd = minT + a.binW;

        for (int b = 0; b < bins; ++b) {
            const size_t iLo = iHi;

            while (iHi + 1 < n && getT(iHi + 1) < binEnd) ++iHi;

            a.range[(size_t)b] = FrameAccelTime::BinRange{ (uint32_t)iLo, (uint32_t)iHi };

            a.idx[(size_t)b] = (uint32_t)iLo;

            binEnd += a.binW;
        }
    }

    static size_t idxForTime(const FrameAccelTime& acc, const std::vector<Frame>& v, double t) {
        if (v.empty()) return 0;
        
        size_t idx = 0;
        if (acc.valid()) {
            idx = acc.idx[(size_t)acc.binOf(t)];
            if (idx >= v.size()) idx = v.size() - 1;
            // Advance to find exact position
            while (idx + 1 < v.size() && v[idx + 1].t <= t) ++idx;
            // Rewind if overshot
            while (idx > 0 && v[idx].t > t) --idx;
        } else {
            // Binary search fallback
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                size_t mid = (lo + hi) >> 1;
                if (v[mid].t < t) lo = mid + 1;
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

        const double frontT = v.front().t;
        const double backT  = v.back().t;
        if (t <= frontT) return 0;
        if (t >= backT)  return n - 1;

        const bool useTimes = a.hasTimes(n);
        const double* times = useTimes ? a.times.data() : nullptr;
        const Frame*  parr  = useTimes ? nullptr : v.data();

        auto getT = [&](size_t i) -> double {
            return times ? times[i] : parr[i].t;
        };

        if (!a.valid()) {
            // binary search (upper_bound - 1)
            size_t lo = 0, hi = n;
            while (lo < hi) {
                size_t mid = (lo + hi) >> 1;
                if (getT(mid) <= t) lo = mid + 1;
                else hi = mid;
            }
            return lo - 1;
        }

        const int b = a.binOf(t);
        size_t L = (size_t)a.range[(size_t)b].lo;
        size_t R = (size_t)a.range[(size_t)b].hi;
        if (R >= n) R = n - 1;
        if (L > R) L = R;

        size_t lo = L, hi = R + 1;
        while (lo < hi) {
            size_t mid = (lo + hi) >> 1;
            if (getT(mid) <= t) lo = mid + 1;
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

        const bool useTimes = a.hasTimes(n);
        const double* times = useTimes ? a.times.data() : nullptr;
        const Frame*  parr  = useTimes ? nullptr : v.data();

        auto getT = [&](size_t i) -> double {
            return times ? times[i] : parr[i].t;
        };

        if (LIKELY(a.valid())) {
            const int b = a.binOf(t);
            idx = (size_t)a.idx[(size_t)b]; // or a.range[b].lo if dropping idx later
            if (idx >= n) idx = n - 1;
        }

        while (idx + 1 < n && getT(idx + 1) <= t) ++idx;
        while (idx > 0 && getT(idx) > t) --idx;
    }

    static FORCE_INLINE void rewindUsingAccelTime_(const std::vector<Frame>& v,
                                                const FrameAccelTime& a,
                                                size_t& idx,
                                                double t) {
        const size_t n = v.size();
        if (n == 0) { idx = 0; return; }

        const bool useTimes = a.hasTimes(n);
        const double* times = useTimes ? a.times.data() : nullptr;
        const Frame*  parr  = useTimes ? nullptr : v.data();

        auto getT = [&](size_t i) -> double {
            return times ? times[i] : parr[i].t;
        };

        if (a.valid()) {
            const int b = a.binOf(t);
            size_t hint = (size_t)a.idx[(size_t)b];
            if (hint < idx) idx = hint;
        }

        while (idx > 0 && getT(idx) > t) --idx;
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
        m_preloadableIndexesInAttemptsList.clear();

        if (m_replayKind == ReplayKind::PracticeComposite) {
            const auto practiceSerials = m_checkpointMgr.getPracticeSerialsMatchingCurrentStartPos_AllSessions(kReplayStartTolerance);
            m_practiceSerialSet.clear();
            m_practiceSerialSet.reserve(practiceSerials.size());
            for (int s : practiceSerials) m_practiceSerialSet.insert(s);

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];
                if (!isAttemptInCurrentPracticeSession_(a)) continue;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) continue;

                m_preloadableIndexesInAttemptsList.push_back(i);
                practice++;
            }
            m_cachedPracticeAttempts = practice;
        }
        else {

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];

                if (a.practiceAttempt) continue;
                if (!matchesStartPositionFilter_(a)) continue;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) continue;

                m_preloadableIndexesInAttemptsList.push_back(i);
                normal++;
            }
            m_cachedNormalAttempts = normal;
        }
        m_attemptCountsDirty = false;
    }

    void applyBotSafety_(bool on) {
        safeMode_enabled = on;
        noclip_enabled = on;
        if (on) cheatAPI::setCheat();
        else cheatAPI::endCheat();
    }

    static inline float lastXOf_(const std::vector<Frame>& v) {
        return v.empty() ? 0.f : v.back().x;
    }

    float attemptSpanX_(Attempt const& a) const
    {
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();

        if (!a.p1.empty()) {
            minX = std::min(minX, a.p1.front().x);
            maxX = std::max(maxX, a.p1.back().x);
        }
        if (a.hadDual && !a.p2.empty()) {
            minX = std::min(minX, a.p2.front().x);
            maxX = std::max(maxX, a.p2.back().x);
        }

        if (maxX <= minX)
            return 0.f;

        return maxX - minX;
    }

    inline CheckpointNode* findCheckpointNodeById_(int id) {
        if (id < 0) return nullptr;
        for (auto& n : m_checkpointNodes) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }

    inline CheckpointNode const* findCheckpointNodeById_(int id) const {
        if (id < 0) return nullptr;
        for (auto const& n : m_checkpointNodes) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }

    static uint32_t mix32_(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
    }

    static float signedUnit_(uint32_t seed) {
        uint32_t h = mix32_(seed);
        // [0,1) -> (-1,1)
        return ( (h / 4294967296.0f) * 2.f ) - 1.f;
    }

    void applyRandomOffsetSingle(Attempt& a) {
        if (m_randomDistPx == 0) {
            a.ghostOffsetPx = 0.f;
            a.ghostOffsetTime = 0.0;
            return;
        }

        auto lastT = [](const std::vector<Frame>& v) -> double {
            return v.empty() ? 0.0 : v.back().t;
        };
        const double endT = std::max(lastT(a.p1), lastT(a.p2));

        // Deterministic signed random in [-1, 1]
        float u = signedUnit_(
            (uint32_t)a.serial ^
            (uint32_t)m_levelIDOnAttach ^
            0x9E3779B9u
        );

        float offsetPx = u * static_cast<float>(m_randomDistPx);

        constexpr float kMinMagPx = 10.0f;
        if (std::fabs(offsetPx) < kMinMagPx) {
            offsetPx = (offsetPx >= 0.f ? kMinMagPx : -kMinMagPx);
            if (offsetPx == 0.f) offsetPx = kMinMagPx;
        }

        const double ups = std::max(1e-6, m_offsetUnitsPerSecond);

        // prevent forward time offset from making the ghost die too soon
        constexpr double kEndGapSec = 0.5;

        if (offsetPx > 0.f) {
            const double maxForwardTime = std::max(0.0, endT - kEndGapSec);
            const double maxForwardPxD  = maxForwardTime * ups;
            const float  maxForwardPx   = static_cast<float>(maxForwardPxD);

            if (maxForwardPx <= 0.f) {
                offsetPx = 0.f;
            } else if (offsetPx > maxForwardPx) {
                offsetPx = maxForwardPx;
            }
        }

        a.ghostOffsetPx   = offsetPx;
        a.ghostOffsetTime = static_cast<double>(offsetPx) / ups;
    }

    void applyRandomOffsetsAll_() {
        for (auto& a : attempts) {
            applyRandomOffsetSingle(a);
        }
    }

    std::filesystem::path attemptsDir_() const {
        if (m_attemptsDir.empty()) {
            auto base = Mod::get()->getSaveDir();
            m_attemptsDir = base / "attempts";
            std::error_code ec;
            std::filesystem::create_directories(m_attemptsDir, ec);
        }
        return m_attemptsDir;
    }

    std::filesystem::path fileForLevel_(int levelID) const {
        auto dir = attemptsDir_();
        if (levelID < 120 && !m_customSaveId.empty()) {
            //log::info("[SAVE FILE] {}", dir / (m_customSaveId + "_attempts.apx"));
            return dir / (m_customSaveId + "_attempts.apx");
        }
        //log::info("[SAVE FILE] {}", dir / (std::to_string(levelID) + "_attempts.apx"));
        return dir / (std::to_string(levelID) + "_attempts.apx");
    }

    void ensureHeader_(std::filesystem::path const& path) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        const uintmax_t fsz = exists ? std::filesystem::file_size(path, ec) : 0;

        if (exists && fsz >= sizeof(APXHeader)) {
            // Check version of existing file
            std::ifstream checkFile(path, std::ios::binary);
            APXHeader existing{};
            checkFile.read(reinterpret_cast<char*>(&existing), sizeof(existing));
            if (checkFile && std::string(existing.magic, 4) == "APX2") {
                // File exists with valid header - don't overwrite
                return;
            }
        }

        std::ofstream hout(path, std::ios::binary | std::ios::trunc);
        APXHeader h{};
        std::memcpy(h.magic, "APX2", 4);
        hout.write(reinterpret_cast<const char*>(&h), sizeof(h));
        hout.flush();
    }

    void appendCheckpointChainChunk_(std::ofstream& out) {
        APXCheckpointMeta meta{};
        meta.nodeCount = (uint32_t)m_checkpointNodes.size();
        meta.topId = m_checkpointTopId;
        meta.frozenTopId = m_frozenCheckpointTopId;

        const uint32_t payloadSz =
            (uint32_t)sizeof(APXCheckpointMeta) +
            (uint32_t)m_checkpointNodes.size() * (uint32_t)sizeof(APXCheckpointNodePacked);

        const uint32_t type = kChunk_CheckpointZ2;

        out.write(reinterpret_cast<const char*>(&type), sizeof(type));
        out.write(reinterpret_cast<const char*>(&payloadSz), sizeof(payloadSz));
        out.write(reinterpret_cast<const char*>(&meta), sizeof(meta));

        for (auto const& n : m_checkpointNodes) {
            APXCheckpointNodePacked p{};
            p.id            = n.id;
            p.x             = n.x;
            p.prevId        = n.prevId;
            p.active        = (uint8_t)(n.active ? 1 : 0);
            p.ownerSerial   = n.ownerSerial;
            p.ownerFrameIdx = (uint32_t)n.ownerFrameIdx;
            p.rot1          = n.rot1; p.rot2 = n.rot2;
            p.rot1Valid     = (uint8_t)(n.rot1Valid ? 1 : 0);
            p.rot2Valid     = (uint8_t)(n.rot2Valid ? 1 : 0);
            out.write(reinterpret_cast<const char*>(&p), sizeof(p));
        }
    }

    void loadCheckpointChainChunk_(std::ifstream& in, uint32_t sz) {
        std::streampos chunkStart = in.tellg();

        APXCheckpointMeta meta{};
        in.read(reinterpret_cast<char*>(&meta), sizeof(meta));
        if (!in) return;

        m_checkpointNodes.clear();
        m_checkpointNodes.reserve(meta.nodeCount);
        for (uint32_t i = 0; i < meta.nodeCount; ++i) {
            APXCheckpointNodePacked p{};
            in.read(reinterpret_cast<char*>(&p), sizeof(p));
            if (!in) break;

            CheckpointNode n{};
            n.id              = p.id;
            n.x               = p.x;
            n.prevId          = p.prevId;
            n.active          = (p.active != 0);
            n.ownerSerial     = p.ownerSerial;
            n.ownerAttemptIdx = SIZE_MAX;
            n.ownerFrameIdx   = p.ownerFrameIdx;
            n.rot1            = p.rot1; n.rot2 = p.rot2;
            n.rot1Valid       = (p.rot1Valid != 0);
            n.rot2Valid       = (p.rot2Valid != 0);
            m_checkpointNodes.push_back(n);
        }
        m_checkpointTopId = meta.topId;
        m_frozenCheckpointTopId = meta.frozenTopId;
        m_chainAuthoritative = true;

        // skip any padding if present
        auto now = in.tellg();
        auto readSoFar = (std::streamoff)(now - chunkStart);
        auto remain = (std::streamoff)sz - readSoFar;
        if (remain > 0) in.seekg(remain, std::ios::cur);
    }


    void applyRotationFromTopCheckpoint_() {
        if (!m_pl || !m_pl->m_isPracticeMode || m_checkpointTopId == -1) return;
        const CheckpointNode& checkpoint = nodeById_(m_checkpointTopId);
        if (m_pl->m_player1 && checkpoint.rot1Valid) m_pl->m_player1->setRotation(checkpoint.rot1);
        if (m_pl->m_player2 && checkpoint.rot2Valid) m_pl->m_player2->setRotation(checkpoint.rot2);
    }
    
    void refreshAttemptColors_(Attempt& a) {
        if (!a.colorsAssigned) {
            if (colors == ColorMode::Random) {
                uint32_t seed = static_cast<uint32_t>(a.serial) ^ 0xA5339E4Du;
                a.c1 = randomGameColor(seed);
                a.c2 = randomGameColor(seed ^ 0xBADA551u);
            } else {
                a.c1 = chooseColor1();
                a.c2 = chooseColor2();
            }
            a.colorsAssigned = true;
        }

        if (a.g1) {
            a.g1->setColor(a.c1);
            a.g1->setSecondColor(a.c2);
            if (a.g1->m_waveTrail) {
                if (m_pl->m_player1->m_switchWaveTrailColor) a.g1->m_waveTrail->setColor(a.c2);
                else a.g1->m_waveTrail->setColor(a.c1);
            }
        }
        if (a.g2) {
            a.g2->setColor(a.c1);
            a.g2->setSecondColor(a.c2);
            if (a.g2->m_waveTrail) {
                if (m_pl->m_player2->m_switchWaveTrailColor) a.g2->m_waveTrail->setColor(a.c1);
                else a.g2->m_waveTrail->setColor(a.c2);
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

    void preloadAttempt_(Attempt& a, size_t attemptIdx = SIZE_MAX) {
        if (a.preloaded) return;
        if (!m_pl || !m_pl->m_player1) return;
        
        //double tStart = getTimeMs();

        a.m_isPlatformer = m_pl->m_isPlatformer;
        
        // Acquire ghost objects
        //double t0 = getTimeMs();
        //if (!a.g1 && !a.p1.empty()) {
        //    a.g1 = m_ghostPool.acquire(a.serial);
        //}
        //if (a.hadDual && !a.g2 && !a.p2.empty()) {
        //    a.g2 = m_ghostPool.acquire(a.serial + 0x100000);
        //}

        

        if (m_replayKind == ReplayKind::BestSingle) {
            if (m_playerObjectPool.inUseCount() < m_playerObjectPool.capacity() - 1) {
                auto h = m_playerObjectPool.acquireForOwner(a.serial);
                if (h) {
                    a.g1Idx = h.index;
                    a.g1 = h.po;
                }
                if (a.hadDual) {
                    auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000);
                    if (h2) {
                        a.g2Idx = h2.index;
                        a.g2 = h2.po;
                    }
                }
            }
        }

        
            
        // Build time-based acceleration if needed
        if (!a.acc1Time.valid() && !a.p1.empty()) {
            buildAccelTime(a.p1, a.acc1Time);
        }
        if (a.hadDual && !a.acc2Time.valid() && !a.p2.empty()) {
            buildAccelTime(a.p2, a.acc2Time);
        }
        
        // Build X-based acceleration for spatial queries
        //if (!a.acc1.valid() && !a.p1.empty()) {
        //    buildAccel(a.p1, a.acc1, 64.f);
        //}
        //if (a.hadDual && !a.acc2.valid() && !a.p2.empty()) {
        //    buildAccel(a.p2, a.acc2, 64.f);
        //}
        
        // Assign icons
        //if (!a.iconsAssigned) {
            uint32_t base = static_cast<uint32_t>(a.serial) ^ 0xDEADBEEF;
            for (int i = 0; i < 9; ++i) {
                a.randomFrame[i] = randomIconFrame(static_cast<IconType>(i), base + i * 97);
            }
            a.iconsAssigned = true;
        //}

        
        
        // Set initial mode
        if (a.g1 && !a.p1.empty()) {
            a.g1CurMode = a.p1.front().mode;
            setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
        }
        if (a.g2 && !a.p2.empty()) {
            a.g2CurMode = a.p2.front().mode;
            setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
        }
        
        // Hide initially
        if (a.g1) a.setP1Visible(false, true);
        if (a.g2) a.setP2Visible(false, true);
        //if (a.g1) {
        //    a.g1->togglePlatformerMode(isPlatformer);
        //    a.g1->m_isPlatformer = isPlatformer;
        //}
        //if (a.g2) {
        //    a.g2->togglePlatformerMode(isPlatformer);
        //    a.g2->m_isPlatformer = isPlatformer;
        //}
        
        
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

    void primeGhostToPX_(Attempt& a, float pxw, float px2w) {
        //log::info("Priming");
        double ghostTime = m_lastAttemptTime + a.ghostOffsetTime;
        if (ghostTime < 0.0) ghostTime = 0.0;
        
        if (!a.p1.empty() && !a.primedP1) {
            //log::info("p1 not created");
            if (!a.g1 && a.g1Idx < 0) {
                //log::info("allocating p1");
                auto h = m_playerObjectPool.acquireForOwner(a.serial);
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
                applyRandomOffsetSingle(a);
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
                pc.x = f.x;
                pc.y = f.y;
                pc.rot = f.rot;
                pc.vehicleSize = f.vehicleSize;
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
                auto h = m_playerObjectPool.acquireForOwner(a.serial + 0x100000);
                if (h) {
                    a.g2Idx = h.index;
                    a.g2 = h.po;
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
                pc2.x = f2.x;
                pc2.y = f2.y;
                pc2.rot = f2.rot;
                pc2.vehicleSize = f2.vehicleSize;
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
    }

    static inline bool attemptHasData_(const Attempt& a) {
        return !a.p1.empty() || !a.p2.empty();
    }

    static inline float attemptLastX_(const Attempt& a)  {
        return a.p1.empty() ? 0.f : a.p1.back().x;
    }

    static FORCE_INLINE bool matchesPracticeFilter_(const Attempt& a, std::optional<bool> wantPractice) {
        if (!wantPractice.has_value())
            return true;

        if (wantPractice.value()) {
            if (!a.practiceAttempt) return false;
            if (a.ignoreInPractice) return false;
            return true;
        }
        return !a.practiceAttempt;
    }

    size_t pickWinningOrBestIndexFromPosition_(std::optional<bool> wantPractice, float startPosX, float tolerance) const {
        //log::info("[Bot] pickWinningOrBestIndexFromPosition_: startPosX={:.2f}, tolerance={:.2f}, wantPractice={}", 
        //    startPosX, tolerance, wantPractice.has_value() ? (wantPractice.value() ? "true" : "false") : "nullopt");
        
        size_t bestWin = SIZE_MAX;
        float bestWinX = -1.f;
        int bestWinSerial = -1;
        
        int checkedCount = 0;
        int matchedFilter = 0;
        int matchedPosition = 0;
        
        for (size_t i = 0; i < attempts.size(); ++i) {
            const auto& a = attempts[i];
            ++checkedCount;
            
            if (!a.completed || a.p1.empty()) continue;
            if (!matchesPracticeFilter_(a, wantPractice)) continue;
            ++matchedFilter;
            
            float attemptStartX = a.p1.front().x;
            if (std::fabs(attemptStartX - startPosX) > tolerance) continue;
            ++matchedPosition;
            
            float x = lastXOf_(a.p1);
            if (x > bestWinX || (x == bestWinX && a.serial > bestWinSerial)) {
                bestWinX = x;
                bestWinSerial = a.serial;
                bestWin = i;
            }
        }
        
        //log::info("[Bot] pickWinning completed pass: checked={}, matchedFilter={}, matchedPosition={}, bestWin={}", 
        //    checkedCount, matchedFilter, matchedPosition, bestWin == SIZE_MAX ? "none" : std::to_string(bestWin));
        
        if (bestWin != SIZE_MAX) {
        //    log::info("[Bot] pickWinning: found completed attempt idx={}, serial={}, endX={:.2f}", 
        //        bestWin, bestWinSerial, bestWinX);
            return bestWin;
        }

        size_t bestAny = SIZE_MAX;
        float bestAnyX = -1.f;
        int bestAnySerial = -1;
        
        int fallbackMatchedFilter = 0;
        int fallbackMatchedPosition = 0;
        
        for (size_t i = 0; i < attempts.size(); ++i) {
            const auto& a = attempts[i];
            if (a.p1.empty()) continue;
            if (!matchesPracticeFilter_(a, wantPractice)) continue;
            ++fallbackMatchedFilter;
            
            float attemptStartX = a.p1.front().x;
            if (std::fabs(attemptStartX - startPosX) > tolerance) continue;
            ++fallbackMatchedPosition;
            
            float x = lastXOf_(a.p1);
            if (x > bestAnyX || (x == bestAnyX && a.serial > bestAnySerial)) {
                bestAnyX = x;
                bestAnySerial = a.serial;
                bestAny = i;
            }
        }
        
        //log::info("[Bot] pickWinning fallback pass: matchedFilter={}, matchedPosition={}, bestAny={}", 
        //    fallbackMatchedFilter, fallbackMatchedPosition, bestAny == SIZE_MAX ? "none" : std::to_string(bestAny));
        
        //if (bestAny != SIZE_MAX) {
        //    log::info("[Bot] pickWinning: found ANY attempt idx={}, serial={}, endX={:.2f}", 
        //        bestAny, bestAnySerial, bestAnyX);
        //}
        
        return bestAny;
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
            if (m_replayAttempt) return m_replayAttempt;
            if (m_compOwnerIdx < attempts.size()) {
                m_replayAttempt = &attempts[m_compOwnerIdx];
                return m_replayAttempt;
            }
            return nullptr;
        }

        //log::info("[getReplayOwner_] findOwnerSerialForTime");
        
        // Use time-based lookup for practice composite
        int serial = m_checkpointMgr.findOwnerSerialForTime(sessionTime);

        //log::info("[getReplayOwner_] serial: {}", serial);
        
        if (serial != m_replayOwnerSerial && serial > 0) {
            m_replayOwnerSerial = serial;
            m_replayIdx1 = 0;
            m_replayIdx2 = 0;
            m_lastEmitIdx1 = 0;
            m_lastEmitIdx2 = 0;
        }
        
        
        if (serial <= 0) return nullptr;
        return findAttemptBySerial_(serial);
    }

    void updateGhostForAttempt_(Attempt& a, float px, float px2, size_t ai,
                            float dt, size_t stepFrames, bool mustRewind) {
        
        //double tStart2 = getTimeMs();     
    
        const double ghostTime = std::max(0.0, m_lastAttemptTime + a.ghostOffsetTime);
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
            bool isP2,
            float playerX
        ) {
            if (frames.empty() || !primed) return;
            // if (isP2 && !m_p2Moving) return;

            const size_t lastIdx = frames.size() - 1;
            const double lastT = frames.back().t;

            double tStart = getTimeMs();  

            if (!eolFrozen) {

                size_t previousIDX = frameIdx;
                
                // Time-based index lookup
                if (LIKELY(!mustRewind)) {
                    advanceFromPrevIdx_(frames, frameIdx, ghostTime, /*mustRewind=*/false);
                } else {
                    frameIdx = idxForTimeBounded_(frames, accTime, ghostTime);
                }
                drawIdx = std::min(frameIdx, lastIdx);

                if (frameIdx == previousIDX) return;

                //log::info("[TIME TAKEN] updateghost advance {:.8f}", getTimeMs() - tStart);
                //tStart = getTimeMs();  

                // EOL freeze check
                if (ghostTime >= lastT - kEolTimeTolerance) {
                    if (ghost) {
                        if (m_ghostsExplode) {
                            // ghost->playerDestroyed(false);
                            ghost->playDeathEffect();
                        }

                        if (m_ghostsExplodeSFX && m_fmodEngine) {
                            if (m_fmodEngine->m_sfxVolume != 0) {

                                static std::mt19937 rng(
                                    (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()
                                );

                                std::string sfxPath;

                                if (m_useCustomExplodeSounds) {
                                    refreshCustomExplodeSfx_(customSfx, customLastWrite);
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
            }

            const Frame& f = frames[drawIdx];
            const size_t nextIdx = std::min(drawIdx + 1, lastIdx);
            const Frame& fNext = frames[nextIdx];

            //log::info("[TIME TAKEN] updateghost death {:.8f}", getTimeMs() - tStart);
            //tStart = getTimeMs();  

            // Interpolation
            float ix = f.x, iy = f.y, irot = f.rot;
            float iveh = f.vehicleSize, iwave = f.waveSize;

            if (m_useInterpolation && !eolFrozen && nextIdx != drawIdx) {
                const double denom = fNext.t - f.t;
                log::info("[m_useInterpolation] denom: {}", denom);
                if (denom > 1e-9) {
                    const float tt = static_cast<float>(clamp01_((ghostTime - f.t) / denom));
                    log::info("[m_useInterpolation] ghostTime -> mid -> next: {} -> {} -> {}", ghostTime, tt, f.t);
                    ix = lerp_(f.x, fNext.x, tt);
                    iy = lerp_(f.y, fNext.y, tt);
                    irot = lerpAngleDeg_(f.rot, fNext.rot, tt);
                    iveh = lerp_(f.vehicleSize, fNext.vehicleSize, tt);
                    iwave = lerp_(f.waveSize, fNext.waveSize, tt);
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
            if (!isP2) {
                a.setP1Visible(f.isVisible);
            } else {
                a.setP2Visible(f.isVisible);
            }

            ghost->setZOrder(0);

            if (m_animateSpider && f.mode == IconType::Spider) { // Tween for smoother transitions
                if (fNext.x == f.x) {
                    if (pc.spiderState != IconAnimationState::Idle) {
                        ghost->m_spiderSprite->runAnimation("idle01");
                        pc.spiderState = IconAnimationState::Idle;
                    }
                }
                else {
                    if (pc.spiderState != IconAnimationState::Run) {
                        ghost->m_spiderSprite->runAnimation("run");
                        pc.spiderState = IconAnimationState::Run;
                    }
                }
                if (fNext.y < f.y) { // Falling
                    if (f.upsideDown) {
                        if (pc.spiderState != IconAnimationState::Jump) {
                            ghost->m_spiderSprite->runAnimation("jump_loop");
                            pc.spiderState = IconAnimationState::Jump;
                        }
                    }
                    else if (pc.spiderState != IconAnimationState::Fall) {
                        ghost->m_spiderSprite->runAnimation("fall_loop");
                        pc.spiderState = IconAnimationState::Fall;
                    }
                }
                else if (fNext.y > f.y) { // Jumping
                    if (f.upsideDown) {
                        if (pc.spiderState != IconAnimationState::Jump) {
                            ghost->m_spiderSprite->runAnimation("fall_loop");
                            pc.spiderState = IconAnimationState::Jump;
                        }
                    }
                    else if (pc.spiderState != IconAnimationState::Fall) {
                        ghost->m_spiderSprite->runAnimation("jump_loop");
                        pc.spiderState = IconAnimationState::Fall;
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
                    }
                }
                else {
                    //if (pc.robotState != RobotState::Run) 
                    //ghost->m_robotSprite->tweenToAnimation("run", 0.1f);
                    if (pc.robotState != IconAnimationState::Run) {
                        ghost->m_robotSprite->runAnimation("run");
                        pc.robotState = IconAnimationState::Run;
                    }
                }
                if (fNext.y < f.y) { // Falling
                    if (f.upsideDown) {
                        //if (pc.robotState != RobotState::Jump) 
                        //ghost->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
                        if (pc.robotState != IconAnimationState::Jump) {
                            ghost->m_robotSprite->runAnimation("jump_loop");
                            pc.robotState = IconAnimationState::Jump;
                        }
                    }
                    //else if (pc.robotState != RobotState::Fall) 
                    //else ghost->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
                    else if (pc.robotState != IconAnimationState::Fall) {
                        ghost->m_robotSprite->runAnimation("fall_loop");
                        pc.robotState = IconAnimationState::Fall;
                    }
                }
                else if (fNext.y > f.y) { // Jumping
                    if (f.upsideDown) {
                        //if (pc.robotState != RobotState::Jump) 
                        // ghost->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
                        if (pc.robotState != IconAnimationState::Jump) {
                            ghost->m_robotSprite->runAnimation("fall_loop");
                            pc.robotState = IconAnimationState::Jump;
                        }
                    }
                    //else if (pc.robotState != RobotState::Fall) 
                    //else ghost->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
                    else if (pc.robotState != IconAnimationState::Fall) {
                        ghost->m_robotSprite->runAnimation("jump_loop");
                        pc.robotState = IconAnimationState::Fall;
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
                    } //else if (trail) {
                    //    trail->setVisible(false);
                    //    trail->setOpacity(0);
                    //}
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
                        // Add points on hold/slide state changes
                        if (fNext.hold != prevHolding) {
                            prevHolding = fNext.hold;
                            trail->addPoint({ix, iy});
                        }
                        else if (fNext.stateDartSlide != prevDartSlide) {
                            prevDartSlide = fNext.stateDartSlide;
                            trail->addPoint({ix, iy});
                        }
                        else if (fNext.vehicleSize != f.vehicleSize) {
                            trail->addPoint({ix, iy});
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
                a.trailactive1, a.previouslyHolding1, a.prevStateDartSlide1,
                false, px
            );
        }

        // log::info("a.hadDual: {}, a.p2.empty(): {}", a.hadDual, a.p2.empty());

        // Process P2
        if (a.hadDual && !a.p2.empty() && a.primedP2) {
            processPlayer(
                a.p2, a.acc2Time, a.g2, a.last2, a.g2CurMode,
                a.i2, a.d2, a.primedP2, a.eolFrozenP2,
                a.trailactive2, a.previouslyHolding2, a.prevStateDartSlide2,
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

        const double frontT = v.front().t;
        const double backT  = v.back().t;
        if (t <= frontT) { idx = 0; return; }
        if (t >= backT)  { idx = last; return; }

        if (UNLIKELY(mustRewind) || UNLIKELY(v[idx].t > t)) {
            while (idx > 0 && v[idx].t > t) --idx;
            while (idx + 1 <= last && v[idx + 1].t <= t) ++idx;
            return;
        }

        if (LIKELY(idx + 1 <= last) && v[idx + 1].t <= t) {
            do { ++idx; }
            while (idx + 1 <= last && v[idx + 1].t <= t);
        }
    }
    

    static void sliceAppendByXClosedOpen_(const std::vector<Frame>& src, 
                                       float x0_incl, float x1_excl, 
                                       std::vector<Frame>& dst) {
        if (src.empty() || x1_excl <= x0_incl) return;
        
        auto it0 = std::lower_bound(src.begin(), src.end(), x0_incl,
            [](const Frame& f, float x) { return f.x < x; });
        auto it1 = std::lower_bound(it0, src.end(), x1_excl,
            [](const Frame& f, float x) { return f.x < x; });
        
        dst.reserve(dst.size() + std::distance(it0, it1));
        for (auto it = it0; it != it1; ++it) {
            dst.push_back(*it);
        }
    }

    static void sliceAppendByXClosed_(const std::vector<Frame>& src, 
                                    float x0_incl, 
                                    std::vector<Frame>& dst) {
        if (src.empty()) return;
        
        auto it = std::lower_bound(src.begin(), src.end(), x0_incl,
            [](const Frame& f, float x) { return f.x < x; });
        
        dst.reserve(dst.size() + std::distance(it, src.end()));
        dst.insert(dst.end(), it, src.end());
    }

    CheckpointNode& nodeById_(int id) {
        for (auto& n : m_checkpointNodes) {
            if (n.id == id) return n; 
        }
        static CheckpointNode dummy;
        dummy.id = -1;
        dummy.active = false;
        dummy.prevId = -1;
        dummy.x = 0.f;
        return dummy;
    }

    const CheckpointNode& nodeById_(int id) const {
        for (auto const& n : m_checkpointNodes) {
            if (n.id == id) return n;
        }
        static CheckpointNode dummy;
        return dummy;
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
        
        // Set base time offset from checkpoint if in practice
        if (m_pl && m_pl->m_isPracticeMode) {
            double time = m_checkpointMgr.getCurrentCheckpointBaseTimeActive();
            m_current.baseTimeOffset = time;
            m_checkpointBaseTime = time;
            m_current.startCheckpointId = m_checkpointMgr.getCurrentCheckpointId(false);
        } else {
            m_current.baseTimeOffset = 0.0;
            m_checkpointBaseTime = 0.0;
            m_current.startCheckpointId = -1;
        }

        //log::info("[startNewAttempt] m_current.baseTimeOffset: {}, m_pl->m_attemptTime: {}", m_currentSessionTime, m_pl->m_attemptTime);

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
        m_current.immuneToIgnorePractice = false;
        m_current.ignoreInPractice = false;
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
            float lastX = !attempts[i].p1.empty() ? attempts[i].p1.back().x : 0.f;
            if (lastX > best) {
                best=lastX;
                bestIdx=i;
            }
        }
        if (bestIdx!=0) {
            std::swap(attempts[0], attempts[bestIdx]);
            resolvePendingCheckpointOwners_();
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

    cocos2d::ccColor3B chooseColor1() {
        if (colors == ColorMode::PlayerColors) {
            if (auto gm = GameManager::sharedState()) {
                int idx = gm->getPlayerColor();
                return gm->colorForIdx(idx);
            }
            return {255,255,255};
        } else if (colors == ColorMode::Black) {
            return {0, 0, 0};
        }
        return {255,255,255};
    }
    cocos2d::ccColor3B chooseColor2() {
        if (colors == ColorMode::PlayerColors) {
            if (auto gm = GameManager::sharedState()) {
                int idx = gm->getPlayerColor2();
                return gm->colorForIdx(idx);
            }
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
        if (auto* rgba = dynamic_cast<cocos2d::CCRGBAProtocol*>(n)) rgba->setOpacity(o);
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

    static PlayerObject* buildGhostPO_NoBatch(PlayLayer* pl, cocos2d::CCNode* parent,
                           const cocos2d::ccColor3B& c1,
                           const cocos2d::ccColor3B& c2,
                           GLubyte opacity,
                           const Frame* startF) {
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
        
        IconType initMode = startF ? startF->mode : (pl && pl->m_player1 ? currentMode(pl->m_player1, pl->m_isPlatformer) : IconType::Cube);
        if (startF) applyPoseHard(po, *startF, pl);
        else setPOFrameForIcon(po, initMode);

        applyOpacityTree(po, opacity);

        if (po->m_waveTrail) {
            po->m_waveTrail->reset();
            po->m_waveTrail->setVisible(false);
        }
        
        return po;
    }

    void rebuildGridThreasholdSet_() {
        m_gridThreshold.clear();

        // throw away any prior preload build state
        m_spans.clear();
        m_spans.reserve(m_ghostsThatPassThePercentThreshold.size());

        for (size_t k = 0; k < m_ghostsThatPassThePercentThreshold.size(); ++k) {
            const size_t attemptIdx = (size_t)m_ghostsThatPassThePercentThreshold[k];
            if (attemptIdx >= attempts.size()) continue;

            // Always recompute
            AttemptSpan sp = makeSpan_(attempts[attemptIdx]);
            m_spans.push_back(sp);

            if (sp.lastX > sp.firstX) {
                // Store attemptIdx so downstream can do attempts[candidate]
                m_gridThreshold.insert((int)attemptIdx, (float)sp.firstX, (float)sp.lastX);
            }
        }

        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
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

        for (size_t i = 0; i < a.p1.size(); ++i) a.p1[i].frame = i;
        for (size_t i = 0; i < a.p2.size(); ++i) a.p2[i].frame = i;

        if (isPureRecordingMode_()) {
            a.preloaded = false;
            a.g1 = nullptr;
            a.g2 = nullptr;
            
            attempts.push_back(std::move(a));
            m_serialCacheDirty = true;
            
            while ((int64_t)attempts.size() > maxGhosts) {
                invalidateAttemptPointerCaches_();
                attempts.erase(attempts.begin());
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

        if (spawnNow && !isPureRecordingMode_()) {
            GhostBatch& batch = getOrCreateBatch_();
            
            if (!aa.g1 && !aa.p1.empty()) {
                aa.g1 = buildGhostPO_NoBatch(m_pl, batch.node, aa.c1, aa.c2, aa.opacity, &aa.p1.front());
                if (aa.g1) {
                    batch.node->addChild(aa.g1, 0);
                    batch.count++;
                    if (aa.g1->m_waveTrail && batch.trailNode) {
                        aa.g1->m_waveTrail->retain();
                        if (aa.g1->m_waveTrail->getParent()) {
                            aa.g1->m_waveTrail->removeFromParentAndCleanup(false);
                        }
                        batch.trailNode->addChild(aa.g1->m_waveTrail, 0);
                        aa.g1->m_waveTrail->release();
                    }
                    aa.g1CurMode = aa.p1.front().mode;
                }
            }
            if (aa.hadDual && !aa.g2 && !aa.p2.empty()) {
                GhostBatch& batch2 = getOrCreateBatch_();
                aa.g2 = buildGhostPO_NoBatch(m_pl, batch2.node, aa.c1, aa.c2, aa.opacity, &aa.p2.front());
                if (aa.g2) {
                    batch2.node->addChild(aa.g2, 0);
                    batch2.count++;
                    if (aa.g2->m_waveTrail && batch2.trailNode) {
                        aa.g2->m_waveTrail->retain();
                        if (aa.g2->m_waveTrail->getParent()) {
                            aa.g2->m_waveTrail->removeFromParentAndCleanup(false);
                        }
                        batch2.trailNode->addChild(aa.g2->m_waveTrail, 0);
                        aa.g2->m_waveTrail->release();
                    }
                    aa.g2CurMode = aa.p2.front().mode;
                }
            }
            refreshAttemptColors_(aa);
        }

        AttemptSpan sp = makeSpan_(aa);
        if (idx >= m_spans.size()) m_spans.resize(idx + 1);
        m_spans[idx] = sp;
        if (sp.lastX > sp.firstX) m_grid.insert(idx, (float)sp.firstX, (float)sp.lastX);

        while ((int64_t)attempts.size() > maxGhosts) {
            auto& oldest = attempts.front();
            if (oldest.g1) oldest.g1->removeFromParentAndCleanup(true);
            if (oldest.g2) oldest.g2->removeFromParentAndCleanup(true);
            attempts.erase(attempts.begin());
            m_spans.erase(m_spans.begin());
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

    void resolvePendingCheckpointOwners_() {
        for (size_t i = 0; i < attempts.size(); ++i) {
            const int serial = attempts[i].serial;
            for (auto& n : m_checkpointNodes) {
                if (n.ownerSerial == serial) n.ownerAttemptIdx = i;
            }
        }
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

    void applyFrameToPlayer_Only_(
        PlayerObject* p,
        const std::vector<Frame>& v,
        size_t baseIdx,
        double sessionTime,
        bool p2JustSpawned = false,
        bool isP1 = true
    ) {
        if (!p || v.empty()) return;

        const double firstT = v.front().t;
        const double lastT  = v.back().t;
        const bool inside  = (sessionTime >= firstT && sessionTime <= lastT);

        size_t i = baseIdx;
        if (i >= v.size()) i = v.size() - 1;

        const Frame& a = v[i];
        const bool haveNext = (i + 1 < v.size());
        const Frame* b = haveNext ? &v[i + 1] : nullptr;

        bool hasData = false;
        Frame finterp{};

        if (m_useInterpolation && inside && b &&
            sessionTime >= a.t && sessionTime <= b->t) {

            const double dt = b->t - a.t;
            const double t  = dt > 1e-9 ? (sessionTime - a.t) / dt : 0.0;

            finterp = a;

            finterp.x   = a.x   + (b->x   - a.x)   * t;
            finterp.y   = a.y   + (b->y   - a.y)   * t;
            finterp.rot = a.rot + (b->rot - a.rot) * t;

            finterp.mode        = a.mode;
            finterp.upsideDown  = a.upsideDown;
            finterp.isDashing   = a.isDashing;

            hasData = true;
            
        }
        else {
            finterp = a;
            hasData = true;
        }

        p->setVisible(true);
        p->setOpacity(255);


        if (p->m_isDashing != finterp.isDashing) {
            if (finterp.isDashing) p->startDashing(attemptplayback::p0d());
            else p->stopDashing();
        }

        if (p->m_isUpsideDown != finterp.upsideDown)
            p->flipGravity(finterp.upsideDown, true);

        if (currentMode(p, m_pl->m_isPlatformer) != finterp.mode) {
            forceMode(p, finterp.mode, /*isRealPlayer*/ true);
        }

        if (p2JustSpawned && p->m_waveTrail)
            p->m_waveTrail->reset();

        if (currentMode(p, m_pl->m_isPlatformer) == IconType::Wave)
            p->setRotation(a.rot);
        else
            p->setRotation(finterp.rot);
        p->setPosition({ finterp.x, finterp.y });
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
