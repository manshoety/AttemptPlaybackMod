#pragma once

#include <Geode/Geode.hpp>

#include <algorithm>
#include <optional>
#include <vector>
#include <iosfwd>
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
#include <cctype>

#include "../core/dashing_args.hpp"
#include "../core/apx_io.hpp"
#include "../core/apx_format.hpp"
#include "./globals.hpp"
#include <legowiifun.cheat_api/include/cheatAPI.hpp>
#ifdef MY_MOD_ID
#undef MY_MOD_ID
#endif
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#ifdef MY_MOD_ID
#undef MY_MOD_ID
#endif
#include "../core/random_color_ids.hpp"
#include "../core/types.hpp"
#include "../core/ghost_pool.hpp"
#include "../core/sfx_handling.hpp"
#include "../core/checkpoint_manager.hpp"
#include "../core/player_object_pool.hpp"
#include "../core/preload_memory_estimator.hpp"
// #include "../core/seed_utils.hpp"

// If I make platformer mode officially supported I need to fix matchesStartPositionFilter_ and make checkpoints in normal mode work (probably use the practice mode thing for all sessions in platformer regardless of practice mode)

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

static FORCE_INLINE void syncPlayerSize(PlayerObject* p, float vehicleSize) {
    if (!p) return;
    const bool wasMini = p->m_vehicleSize < 0.9f;
    const bool isMini = vehicleSize < 0.9f;
    if (wasMini != isMini) p->togglePlayerScale(!isMini, true);
    if (p->m_vehicleSize != vehicleSize) {
        p->m_vehicleSize = vehicleSize;
        p->updatePlayerScale();
    }
}

static FORCE_INLINE void forceMode(PlayerObject* p, IconType mode, bool isRealPlayer = false) {
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

    bool shouldIgnoreNoclipDetection_() const {
        // can go through spikes during end animation but that shouldn't count
        return m_playingEndAnimation || (m_pl && m_pl->m_hasCompletedLevel);
    }
    void beginPlayerDestroyedCheck() {
        if (shouldIgnoreNoclipDetection_()) return;
        setFalseIfPlayerWasDestroyedCheck = true;
    }
    void endPlayerDestroyedCheck(bool playerDied);
    bool didAttemptUseNoclip() {
        if (shouldIgnoreNoclipDetection_()) return m_currentDidNoclip;
        return (setFalseIfPlayerWasDestroyedCheck || isNoclipDetected);
     }
    void resetNoclipDetectedFlag() { 
        isNoclipDetected = false;
        setFalseIfPlayerWasDestroyedCheck = false;
    }
    void updateCurrentAttemptNoclipState() {
        if (shouldIgnoreNoclipDetection_()) return;
        if (!m_currentDidNoclip) m_currentDidNoclip = didAttemptUseNoclip();
    }
    void armFreshRecordingAfterDeleteRestart_();

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
    bool m_allowWaveHook = false;

    void setP1Hold(bool h) { p1Hold = h; }
    void setP2Hold(bool h) { p2Hold = h; }
    void setP1LHold(bool h) { p1LHold = h; }
    void setP2LHold(bool h) { p2LHold = h; }
    void setP1RHold(bool h) { p1RHold = h; }
    void setP2RHold(bool h) { p2RHold = h; }
    void markWavePointThisFrameP1() { p1WavePointThisFrame = true; }
    void markWavePointThisFrameP2() { p2WavePointThisFrame = true; }
    bool allowWaveHook() const { return m_allowWaveHook; }

    float getStartPosTolerance() const { return kReplayStartTolerance; }

    float getAccuratePercentForX(float x, float fallbackPercent = 0.f) const;

    bool getCurrentPracticeStartLocation(float& startX, float& startPercent) const;

    const std::vector<APXAttemptDiskInfo>& getAttemptCatalog() {
        if (hasCurrentPersistenceTarget_()) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }
        return m_attemptCatalog;
    }

    const PracticePath& getPracticePathCatalog() {
        if (hasCurrentPersistenceTarget_()) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }
        return m_attemptCatalogPracticePath;
    }

    bool refreshAttemptCatalogForCurrentLevel(bool pushCurrentAttempt = true) {
        return refreshAttemptCatalogForCurrentLevel_(true, pushCurrentAttempt);
    }

    void setPracticeReplaySessionSelection(std::vector<int> const& sessionIds);

    void clearPracticeReplaySessionSelection() {
        m_requestedPracticeSessionIds.clear();
        invalidateAttemptCounts();
    }

    bool hasPracticeReplaySessionSelection() const {
        return !m_requestedPracticeSessionIds.empty();
    }

    bool deleteAttemptBySerial(int serial) {
        if (serial <= 0) return false;
        return deleteAttemptsBySerials({ serial });
    }

    bool deleteAttemptsBySerials(std::vector<int> const& serials);

    bool deletePracticeAttemptsBySerials(std::vector<int> const& serials);

    bool isPracticeSessionBeingRecorded(int sessionId) const;

    bool deleteAttemptsByEndPercent(float minPercent, float maxPercent);

    bool deletePracticeSessionById(int sessionId);

    bool rewriteAttemptsAndPracticePathAfterDeletion_(
        std::unordered_set<int> const& deleteSet,
        PracticePath const& keptPath,
        char const* label
    );

    bool deletePracticeAttemptsBySerialSet_(
        std::unordered_set<int> const& deleteSet,
        bool allowReplayPathSerials
    );

    // Ghost text
    geode::Ref<cocos2d::CCLabelBMFont> m_playLayerGhostTextLabel = nullptr;

    bool m_showPlayLayerGhostText = false;
    std::string m_playLayerGhostText = "";
    std::string m_ghostTextCustomFormat = "{alive}/{total} attempts remain";
    cocos2d::CCPoint m_playLayerGhostTextPos = { 2.f, 310.f };
    float m_playLayerGhostTextScale = 0.45f;
    uint8_t m_playLayerGhostTextOpacity = 180;
    bool m_ghostTextFlashOnDeath = true;
    size_t m_numDeadGhosts = 0;
    size_t m_numAliveGhosts = 0;
    size_t m_numTotalGhosts = 0;
    int m_ghostTextFlashActionTag = 812345;
    GhostTextPreset m_ghostTextMode = GhostTextPreset::DeadAttempts;

    std::string m_lastAppliedPlayLayerGhostText = "";
    bool m_lastAppliedPlayLayerGhostTextVisible = false;
    bool m_ghostTextPositionPreview = false;

    std::unordered_set<int> m_ghostTextCountedDeadSerials;

    bool m_showDeathMarkers = false;
    float m_deathMarkerSize = 25.f;
    float m_deathMarkerThickness = 5.f;
    uint8_t m_deathMarkerOpacity = 255;
    struct DeathMarkerPositions {
        cocos2d::CCPoint p1{};
        cocos2d::CCPoint p2{};
        bool hasP2 = false;
    };
    geode::Ref<cocos2d::CCDrawNode> m_deathMarkerDrawNode = nullptr;
    std::unordered_map<int, DeathMarkerPositions> m_deathMarkerPositions;

    static void replaceAllGhostTextToken_(std::string& value, std::string const& token, std::string const& replacement);

    static std::string formatGhostPercentage_(size_t count, size_t total, int precision);

    static void replaceGhostPercentageTokens_(std::string& value, std::string const& name, size_t count, size_t total);

    static std::string formatGhostTextForCounts_(
        std::string out,
        size_t alive,
        size_t dead,
        size_t total
    );
 
    std::string formatCustomGhostText_() const;

    void rebuildGhostTextString_();

    void syncGhostUiSettingsFromSaved_();

    void setPlayLayerGhostTextLabel(cocos2d::CCLabelBMFont* label) {
        m_playLayerGhostTextLabel = label;
        m_lastAppliedPlayLayerGhostText.clear();
        m_lastAppliedPlayLayerGhostTextVisible = false;
        updatePlayLayerGhostTextLabel_();
    }

    void clearPlayLayerGhostTextLabel() {
        m_playLayerGhostTextLabel = nullptr;
        m_lastAppliedPlayLayerGhostText.clear();
        m_lastAppliedPlayLayerGhostTextVisible = false;
    }

    void setPlayLayerGhostTextEnabled(bool enabled) {
        m_showPlayLayerGhostText = enabled;
        Mod::get()->setSavedValue("ghost-text-enabled", enabled);
        updatePlayLayerGhostTextLabel_();
    }

    bool isPlayLayerGhostTextEnabled() const { return m_showPlayLayerGhostText; }

    void setGhostTextPositionPreview(bool enabled) {
        m_ghostTextPositionPreview = enabled;
        m_lastAppliedPlayLayerGhostText.clear();
        m_lastAppliedPlayLayerGhostTextVisible = !enabled;
        updatePlayLayerGhostTextLabel_();
    }

    bool isGhostTextPositionPreviewActive() const {
        return m_ghostTextPositionPreview;
    }

    void setGhostTextMode(GhostTextPreset mode) {
        m_ghostTextMode = mode;
        Mod::get()->setSavedValue("ghost-text-mode", static_cast<int64_t>(mode));
        rebuildGhostTextString_();
        updatePlayLayerGhostTextLabel_();
    }
    GhostTextPreset getGhostTextMode() const { return m_ghostTextMode; }

    void setGhostTextCustomFormat(std::string text);
    std::string const& getGhostTextCustomFormat() const { return m_ghostTextCustomFormat; }

    void setPlayLayerGhostText(std::string text) {
        m_playLayerGhostText = std::move(text);
        m_lastAppliedPlayLayerGhostText.clear();
    }

    void setPlayLayerGhostTextPos(cocos2d::CCPoint pos) {
        m_playLayerGhostTextPos = pos;
        Mod::get()->setSavedValue("ghost-text-x", static_cast<int64_t>(std::lround(pos.x)));
        Mod::get()->setSavedValue("ghost-text-y", static_cast<int64_t>(std::lround(pos.y)));
        updatePlayLayerGhostTextLabel_();
    }
    cocos2d::CCPoint getPlayLayerGhostTextPos() const { return m_playLayerGhostTextPos; }

    void setPlayLayerGhostTextScale(float scale) {
        m_playLayerGhostTextScale = std::clamp(scale, 0.15f, 1.25f);
        Mod::get()->setSavedValue("ghost-text-scale-percent", static_cast<int64_t>(std::lround(m_playLayerGhostTextScale * 100.f)));
        updatePlayLayerGhostTextLabel_();
    }
    float getPlayLayerGhostTextScale() const { return m_playLayerGhostTextScale; }

    void setPlayLayerGhostTextOpacity(int opacityValue) {
        m_playLayerGhostTextOpacity = static_cast<uint8_t>(std::clamp(opacityValue, 0, 255));
        Mod::get()->setSavedValue("ghost-text-opacity", static_cast<int64_t>(m_playLayerGhostTextOpacity));
        updatePlayLayerGhostTextLabel_();
     }
    int getPlayLayerGhostTextOpacity() const { return static_cast<int>(m_playLayerGhostTextOpacity); }

    void setGhostTextFlashOnDeath(bool enabled) {
        m_ghostTextFlashOnDeath = enabled;
        Mod::get()->setSavedValue("ghost-text-flash-on-death", enabled);
    }
    bool isGhostTextFlashOnDeath() const { return m_ghostTextFlashOnDeath; }

    size_t getGhostTextAliveCount() const { return m_numAliveGhosts; }
    size_t getGhostTextDeadCount() const { return m_numDeadGhosts; }
    size_t getGhostTextTotalCount() const { return m_numTotalGhosts; }
    std::string getFormattedGhostText() const { return m_playLayerGhostText; }
        
    void updatePlayLayerGhostTextLabel_();

    void forceHidePlayLayerGhostTextLabelOnly();

    void ensureDeathMarkerLayer_();

    void drawDeathMarkerAt_(cocos2d::CCPoint const& p);

    static bool hasDistinctP2DeathMarker_(DeathMarkerPositions const& positions);

    void drawDeathMarkersForAttempt_(DeathMarkerPositions const& positions) {
        drawDeathMarkerAt_(positions.p1);
        if (hasDistinctP2DeathMarker_(positions)) {
            drawDeathMarkerAt_(positions.p2);
        }
    }

    void redrawDeathMarkers_();

    void clearDeathMarkers_() {
        m_deathMarkerPositions.clear();
        if (m_deathMarkerDrawNode) m_deathMarkerDrawNode->clear();
    }

    void recordDeathMarker_(Attempt const& a);

    void setDeathMarkersEnabled(bool enabled) {
        m_showDeathMarkers = enabled;
        Mod::get()->setSavedValue("death-markers-enabled", enabled);
        redrawDeathMarkers_();
    }
    bool areDeathMarkersEnabled() const { return m_showDeathMarkers; }

    void setDeathMarkerSize(float size) {
        m_deathMarkerSize = std::clamp(size, 4.f, 40.f);
        Mod::get()->setSavedValue("death-marker-size", static_cast<int64_t>(std::lround(m_deathMarkerSize)));
        redrawDeathMarkers_();
    }
    float getDeathMarkerSize() const { return m_deathMarkerSize; }

    void setDeathMarkerThickness(float thickness);
    float getDeathMarkerThickness() const { return m_deathMarkerThickness; }

    void setDeathMarkerOpacity(int opacityValue) {
        m_deathMarkerOpacity = static_cast<uint8_t>(std::clamp(opacityValue, 0, 255));
        Mod::get()->setSavedValue("death-marker-opacity", static_cast<int64_t>(m_deathMarkerOpacity));
        redrawDeathMarkers_();
    }
    int getDeathMarkerOpacity() const { return static_cast<int>(m_deathMarkerOpacity); }

    bool countGhostTextDeathOnceBySerial_(int serial);

    bool countGhostTextDeathOnce_(Attempt const& a) {
        const bool counted = countGhostTextDeathOnceBySerial_(a.serial);
        if (counted) recordDeathMarker_(a);
        return counted;
    }

    void updateGhostTextOnDeath(bool reset = false);

    void updateDeadAttemptsText(bool reset = false);

    void updateAliveAttemptsText(bool reset = false) {
        if (reset) m_numAliveGhosts = m_numTotalGhosts;
        rebuildGhostTextString_();
     }

    void flashPlayLayerGhostTextRed(double fadeSeconds = 0.2);

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
    std::array<uint8_t, kRandomColorCount> m_randomColorAllowedP1{};
    std::array<uint8_t, kRandomColorCount> m_randomColorAllowedP2{};
    std::vector<int> m_randomColorAllowedListP1;
    std::vector<int> m_randomColorAllowedListP2;

    PreloadSortMode m_preloadSortMode = PreloadSortMode::Best;

    void refreshCustomDeathSfxNow(bool resetGhostObjects);

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

    float getPlaybackOnlyPastPercentThreshold();

    void setPlaybackLimitVisibleGhostsEnabled(bool on) {
        m_limitVisibleGhosts = on;
        Mod::get()->setSavedValue("limit-visible-ghosts", on);
    }

    void setPlaybackOnlyPastPercentEnabled(bool on);

    void setPlaybackMaxVisibleGhosts(int maxVisible) {
        m_maxVisibleGhosts = maxVisible;
        Mod::get()->setSavedValue("max-visible-ghosts", maxVisible);
    }

    void setPlaybackOnlyPastPercentThreshold(float percent);

    void buildGhostThatPassedPercentSerialList();

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

    bool hasPersistenceTargetForLevel_(int levelID) const {
        if (levelID != 0) return true;
        return !m_customSaveId.empty();
    }

    bool hasCurrentPersistenceTarget_() const {
        return hasPersistenceTargetForLevel_(m_levelIDOnAttach);
    }

    bool deleteCurrentLevelSaveFile();

    std::string getCurrentLevelSaveFileName() const {
        auto path = fileForLevel_(m_levelIDOnAttach);
        return geode::utils::string::pathToString(path.filename());
    }

    enum class AttemptFileExportMode : uint8_t {
        FullFile = 0,
        BestNormalAttempt = 1,
        BestPracticePath = 2,
    };

    enum class AttemptFileImportMode : uint8_t {
        ReplaceCurrent = 0,
        MergeWithCurrent = 1,
    };

    struct AttemptFileImportPreview {
        bool valid = false;
        std::string error;
        size_t incomingAttempts = 0;
        size_t incomingPracticeSessions = 0;
        size_t currentAttempts = 0;
        size_t currentPracticeSessions = 0;
        size_t attemptSerialConflicts = 0;
        size_t practiceSessionIdConflicts = 0;
        bool incomingContainsNormal = false;
        bool incomingContainsPractice = false;
        bool incomingIsLegacy = false;
    };

    std::filesystem::path getCurrentLevelSavePath() const {
        if (!hasCurrentPersistenceTarget_()) return {};
        return fileForLevel_(m_levelIDOnAttach);
    }

    std::filesystem::path getAttemptExportsDir() const;

    bool exportCurrentLevelAttempts(
        std::filesystem::path const& destination,
        AttemptFileExportMode mode,
        std::string* outError = nullptr
    );

    AttemptFileImportPreview inspectAttemptImportFile(std::filesystem::path const& source);

    bool importAttemptFile(
        std::filesystem::path const& source,
        AttemptFileImportMode mode,
        std::string* outError = nullptr
    );

    static bool validRandomColorMaskString_(std::string const& s) {
        if (s.size() != static_cast<size_t>(kRandomColorCount)) return false;
        return std::all_of(s.begin(), s.end(), [](char ch) { return ch == '0' || ch == '1'; });
    }

    static void randomColorArrayFromString_(
        std::array<uint8_t, kRandomColorCount>& out,
        std::string const& s
    );

    static std::string randomColorArrayToString_(
        std::array<uint8_t, kRandomColorCount> const& mask
    );

    void ensureRandomColorMaskLoaded_();

    void rebuildRandomColorAllowedLists_();

    void saveRandomColorMasks_();

    bool isRandomGhostColorAllowed(int slot, bool player1) {
        ensureRandomColorMaskLoaded_();

        if (slot < 0 || slot >= kRandomColorCount) return false;
        return player1 ? (m_randomColorAllowedP1[slot] != 0) : (m_randomColorAllowedP2[slot] != 0);
    }

    void setRandomGhostColorAllowed(int slot, bool allowed, bool player1);

    void setAllRandomGhostColorsAllowed(bool allowed, bool player1);

    int pickRandomAllowedGhostColorIdx(uint32_t seed, bool player1);

    int getNumAttemptsPreloadedTotal() { return m_attemptsPreloadedTotal; }

    struct CandidateInfo {
            int index;
            float endX;
        };
    std::vector<CandidateInfo> candidates;

    void buildInitialAttemptsFromPreloadOrder(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial
    );

    void buildPreloadOrderForCount_(int targetCount, std::vector<uint32_t>& outOrder) const;

    void beginPreloadAttempts(int targetCount);

    void prioritizeInitialAttemptsInPreloadOrder_(int targetCount);

    void preloadStep(int maxPerTick);

    void InitialGamemodeSetProcessAttemptIdx(size_t attemptIdx);

    void prepareAttemptVisualData_(Attempt& a);

    void InitialGamemodeSetProcessBatch(
        const std::vector<uint32_t>* orderList,
        int batchSize,
        int startIndex = 0
    );

    // Please can you be the same as preloading? It's slightly different for some reason
    void InitialGamemodeSet() {
        m_playerObjectPool.clearOwners(true);
        InitialGamemodeSetProcessBatch(&m_preloadOrder, m_playerObjectPool.capacity());
    }

    bool isPreloadingAttempts() const { return m_preloadActive; }
    int getPreloadLoadedCount() const { return m_preloadLoaded; }
    int getPreloadTargetCount() const { return m_preloadTarget; }
    bool isPlayerFrozen() const { return m_freezePlayerXAtEnd; }

    void resetAttemptPreloadState_(Attempt& a, int idx);

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

    void cancelPreloadAttempts(bool ignoreStopPlayback);

    bool hasAnyReplayDataForCurrentLevel(ReplayKind kind);

    int getTotalAttemptsCount();

    preload_memory::PreloadMemoryEstimate estimatePreloadMemoryUsage(
        int targetCount,
        int realPlayerObjectAttempts,
        size_t bytesPerPlayerObject
    );

    void flushPendingSaves_();

    void setUseCheckpointsRoute(bool on) { m_useCheckpointsRoute = on; }
    bool useCheckpointsRoute() const { return m_useCheckpointsRoute; }
    
    void setGhostDistance(int maxPx) { m_randomDistPx = maxPx; }
    int getGhostDistance() const { return m_randomDistPx; }

    void setModEnabled(bool on);
    bool isModEnabledForPlayLayer(PlayLayer* pl) const {
        if (!modEnabled) return false;

        const bool isPlatformer = pl && pl->m_isPlatformer;
        return !isPlatformer || m_allow_platformer;
    }

    bool isModEnabled() const {
        auto* pl = getPlayLayer();
        return isModEnabledForPlayLayer(pl);
    }

    bool hasModAttachedToLevel() const {
        return getPlayLayer() != nullptr;
    }

    bool hasStoredPlayLayerAttachment() const {
        return m_pl != nullptr;
    }

    bool isStoredPlayLayer(PlayLayer* pl) const {
        return pl && pl == m_pl;
    }

    void abandonStoredPlayLayerAttachment();

    bool isAttachedPlayLayer(PlayLayer* pl) const {
        if (!pl || pl != m_pl) return false;
        auto* active = GJBaseGameLayer::get();
        return active && active == static_cast<GJBaseGameLayer*>(pl);
    }

    bool shouldHandlePlayLayer(PlayLayer* pl) const {
        return isAttachedPlayLayer(pl) && isModEnabledForPlayLayer(pl);
    }

    bool shouldHandleActivePlayLayer() const {
        return shouldHandlePlayLayer(getPlayLayer());
    }

    bool isUpdateAttemptCountBlocked() const { return m_blockAttemptCount; }

    void updateModEnabled() {
        auto* mod = Mod::get();
        bool temp_modEnabled = getSettingBoolOrDefault_(mod, "mod-enabled", true);
        m_allow_platformer = getSettingBoolOrDefault_(mod, "allow-platformer", false);
        if (temp_modEnabled != modEnabled) setModEnabled(temp_modEnabled);
    }

    void setPerfBudgetFromUI(int v);

    std::filesystem::path attemptsDir_() const;

    std::filesystem::path fileForLevel_(int levelID) const;

    bool loadLevelFileWithMigration_(int levelID, std::vector<Attempt>& attemptsOut, PracticePath& practicePathOut) {
        return loadAPXFileWithMigration(fileForLevel_(levelID), attemptsOut, practicePathOut);
    }

    bool saveLevelFileCurrent_(int levelID, std::vector<Attempt> const& attempts, PracticePath const& practicePath) {
        const auto path = fileForLevel_(levelID);
        ensureHeader_(path);
        return saveAPXFileCurrent(path, attempts, practicePath);
    }

    void ensureHeader_(std::filesystem::path const& path);

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

    void migrateLegacyCustomLevelFiles_(
        int levelID,
        std::string const& legacyNameHash,
        std::string const& canonicalSaveId
    );

    void refreshLevelName(int levelID);

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

    bool rewriteWholeFileToCurrent_(std::filesystem::path const& path);

    int saveNewAttemptsForLevel_(int levelID);


    int loadFromDisk_(int levelID, LoadFilter const& flt);

    int loadAttemptsForLevel(int levelID, LoadFilter const& flt = {}) {
        return loadFromDisk_(levelID, flt);
    }

    int saveNewAttemptsForCurrentLevel();

    void setOpacityVar(GLubyte o) { opacity = o; }

    void refreshOpacityForAttemptNoWaveTrail(Attempt& a) {
        if (a.g1) a.g1->setOpacity(a.opacity);
        if (a.g2) a.g2->setOpacity(a.opacity);
    }

    void updateOpacityForAttempt(Attempt& a, GLubyte o, bool force=false);

    void setOpacityAll(GLubyte o);
    void updateOpacityAll();


    void setBudgetPerFrame(int v) { m_budgetPerFrame = std::max(1, v); }
    void setMaxVisibleGhosts(int v) { m_maxVisibleGhosts = std::max(1, v); }
    void setRecordInPractice(bool v) { recordInPractice = v; }

    void setAccurateHighTPSReplay(bool v) { m_accurateHighTPSReplay = v; }
    void setAutosaveEnabled(bool v) { m_autosaveEnabled = v; m_autosaveAccum = 0.f; }
    void setAutosaveMinutes(int v) { m_autosaveMinutes = std::clamp(v, 1, 120); m_autosaveAccum = 0.f; }
    bool resizeRealPlayerObjects(int count) {
        if (!m_playerObjectPool.isInitialized()) return true; // loaded on next attach
        return m_playerObjectPool.resizeCapacity(std::clamp(count, 250, 5000));
    }
    void setColorMode(ColorMode m);
    void updateAllColors();
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
    PlayLayer* getPlayLayer() const;
    bool isGhostsExplodeSFXEnabled() const { return m_ghostsExplodeSFX; }
    void setGhostsExplodeSFXEnabled(bool on) { m_ghostsExplodeSFX = on; }
    bool getPlaybackCustomDeathSoundEnabled() const { return m_useCustomExplodeSounds; }
    void setPlaybackCustomDeathSoundEnabled(bool on) {
        Mod::get()->setSavedValue("custom-ghosts-explode-sfx", on);
        m_useCustomExplodeSounds = on;
        if (on) refreshAndPreloadCustomExplodeSfx_(true);
    }
    bool isRandomIconsEnabled() const { return m_randomIcons; }
    void setRandomIconsEnabled(bool on);
    bool isReplayPreventCompletionEnabled() const { return m_pauseGameAtTheEndOfReplayBot; }
    void setReplayPreventCompletionEnabled(bool on) { m_pauseGameAtTheEndOfReplayBot= on; }

    bool practiceFilterEvenWhenBotNotOn() const;

    std::optional<bool> practiceFilterForGhosts_() const;

    void updateGhostVisibility();

    void attach(PlayLayer* pl);

    void initBotAfterReset_(bool naturalPracticeStart = false);

    void onDeath();

    void onReset();

    void invalidatePrimedPlayerObjectRefs();

    void setActiveGhostsInvisible();

    void onQuit();

    void onComplete();

    void saveCurrentAttemptNow();

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

    void toggleRecording();
    bool isRecording() const { return recording; }
    bool getShowWhilePlaying() const { return showWhilePlaying; }

    bool isPlaybackEnabled() const { return playback; }
    void startPlayback();
    void stopPlayback();

    void clearGhosts();
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
        syncPlayerSize(po, vehicleSize);
        po->setRotation(rot);
        po->setPosition({ x, y });
    }

    void buildCompositeFromAttempt(size_t idx);
    
    const Attempt* findBestAttemptForRange_(float x0, float x1, 
        std::optional<bool> wantPractice) const;

    void syncInputsToCurrentIndexP1_();

    void syncInputsToCurrentIndexP2_();

    bool startReplayPractice();


    bool startReplayBest();

    int chooseReplayOwnerFromFinishedPreload_();

    bool replayBestSetOwner();

    void stopReplay();

    void restartLevel();

    bool isBotActive() const { return botActive; }
    bool didUseBotThisAttempt() const { return m_runUsedBot; }
    bool isReplaying() const { return playback; }
    bool isResetting() const { return resetting; }
    void setResetting(bool on) { resetting = on; }
    bool isdisablePlayerMove() const { return disablePlayerMove && m_setRealPlayerPosition && !m_freezePlayerXAtEnd; }
    void setdisablePlayerMove(bool on) { disablePlayerMove = on; }
    void setAllowWavePointAdding(bool on ) { m_allowWavePointAdding = on; }
    bool isAllowingWavePointAdding() const { return m_allowWavePointAdding; }
    bool skipHardStreakCheck() const { return m_skipHardStreakCheck; }
    bool shouldOwnWaveTrailDuringPlayback() const { return botActive && m_hasWavePointData; }

    double getStartTime();

    void togglePracticeMode(bool enabled) {
        if (!m_pl) return;
        m_pl->togglePracticeMode(enabled);
    }

    bool isPracticeMode() {
        if (!m_pl) return false;
        return m_pl->m_isPracticeMode;
    }

    void onPracticeToggle(bool enabled);

    void maybeInitialWarpToReplayStart_();

    bool refreshCurrentOwnerPointer_();

    void updateClickState(bool isPlayer1);

    void setFirstNonzeroPosP2();

    void preUpdateP2();

    void hasReachedEndOfLevel();

    void preUpdate();

    void checkIfPlayerIsDonePlaybackThingMaybePerhapsYeah();

    void updateFreezePlayerX_();

    void applySegmentBasedReplay_(bool isPlayer1);

    void recordUpdate(bool isPlayer1, bool isPostWavePointUpdate=false);

    bool removeReverseOrDuplicateFrames(std::vector<Frame>& frames, double t);

    void insertWaveStateIntoLastFrame(bool isPlayer1);
    

    void recordCurrentFrame_(bool isPlayer1);

    void seedInitialAttemptFrame_();
    
    bool isOwnerGhost_(size_t ai, Attempt const& a) const;

    void postUpdate();

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
    bool m_noP2Check = false;
    bool m_ghostsExplode = false;
    bool m_ghostsExplodeSFX = false;
    bool m_randomIcons = true;
    bool m_IHateMirrorPortalsSoRewind = false;
    bool m_separatePlayerInputs = false;
    bool shouldReplayP2Inputs_() const {
        return m_separatePlayerInputs && m_gl && m_gl->m_gameState.m_isDualMode;
    }
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
    MovementDirection m_movementDirectionP1 = MovementDirection::Flat;
    MovementDirection m_movementDirectionP2 = MovementDirection::Flat;
    bool m_explicitWavePointAddedPrevPoseP1 = false;
    bool m_explicitWavePointAddedPrevPoseP2 = false;
    bool m_suppressNextExplicitWavePointP1 = false;
    bool m_suppressNextExplicitWavePointP2 = false;
    bool m_wavePointLastFrameP1 = false;
    bool m_wavePointLastFrameP2 = false;
    cocos2d::CCPoint m_lastWavePointP1;
    cocos2d::CCPoint m_lastWavePointP2;

    double finalTime = 0.f;

    bool overrideWaveSize = true;

    inline static SfxLimiter g_explodeLimiter{20};
    FMODAudioEngine* m_fmodEngine = nullptr;
    bool m_useCustomExplodeSounds = false;

    PlayLayer* m_pl = nullptr;
    GJBaseGameLayer* m_gl = nullptr;

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
    bool m_runUsedBot = false;
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

    size_t m_lastEmitIdx1 = kNoEmitIdx;
    size_t m_lastEmitIdx2 = kNoEmitIdx;
    size_t m_lastPoseIdx1 = kNoEmitIdx;
    size_t m_lastPoseIdx2 = kNoEmitIdx;
    static constexpr size_t kNoEmitIdx = std::numeric_limits<size_t>::max();
    
    int m_bridgeOwnerSerial = -1;

    bool m_bridgePendingP1 = false;
    bool m_bridgePendingP2 = false;

    size_t m_bridgeIdx1 = kNoEmitIdx;
    size_t m_bridgeIdx2 = kNoEmitIdx;

    bool recording = true;
    bool recordingBlocked = false;
    bool m_recordingBlockedOnNoclip = false;
    bool playback = false;
    bool m_playbackArmed = false;
    bool block_attempt_push_on_recording_start = false;
    bool m_forceFreshRecordingAfterDelete = false;

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
    std::unordered_set<uint64_t> m_loadedAttemptKeys;
    int m_loadedLevelID = 0;
    bool m_needsMigrationRewrite = false;
    bool m_loadedFileWasLegacyAttempts = false;
    uint64_t m_lazyLoadedAttempts = 0;
    uint64_t m_lazyLoadFailures = 0;
    uint64_t m_lazyDecodeTimeUs = 0;
    uint64_t m_preloadPrepareTimeUs = 0;
    std::vector<APXAttemptDiskInfo> m_attemptCatalog;
    std::unordered_map<int, size_t> m_attemptCatalogBySerial;
    std::unordered_map<uint64_t, size_t> m_attemptCatalogByIdentity;
    int m_attemptCatalogLevelID = 0;
    bool m_attemptCatalogScanned = false;
    bool m_attemptCatalogCurrentFormat = false;
    std::string m_attemptCatalogCustomSaveId;

    bool m_allow_platformer = false;

    static constexpr int kPerfBudgetInfiniteSentinel = 1000;
    bool m_perfUnlimited = false;

    int m_randomDistPx = 0;
    int m_randomDistPxOld = 0;
    size_t m_lastGhostDistPoll = 0;

    bool m_justStartedBot = false;
    cocos2d::CCPoint m_currentReplayStartPos{0.f, 0.f};
    bool m_pendingPracticeStartPosInit = false;
    int m_pendingPracticeStartPosFrames = 0;

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
    bool m_playingEndAnimation = false;
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
    std::unordered_set<int> m_requestedPracticeSessionIds;
    int m_practiceReplayRouteStartSessionId = 0;

    geode::Ref<StartPosObject> m_replayStartPosObject = nullptr;
    geode::Ref<StartPosObject> m_savedStartPosObject = nullptr;
    geode::Ref<LevelSettingsObject> m_replayStartSettings = nullptr;
    std::vector<int> m_preloadableIndexesInAttemptsList;
    std::vector<uint32_t> m_preloadableSerials;

    geode::Ref<cocos2d::CCNode> m_ghostRoot = nullptr;

    std::unordered_set<int> m_routeOwnerSerials;
    std::unordered_set<int> m_currentlyHiddenSerials;
    std::vector<uint8_t> m_routeOwnerMask;
    bool m_hideAllOwnersDuringPracticeReplay = true;

    CheckpointManager m_checkpointMgr;

    mutable std::unordered_map<int, size_t> m_serialToIdx;
    mutable std::unordered_map<uint64_t, size_t> m_attemptToIdxByIdentity;
    mutable bool m_serialCacheDirty = true;

    static constexpr float kReplayStartTolerance = 30.0f;
    static constexpr float kPracticeStitchXOverlapTolerance = 2.0f;
    static constexpr float kPracticeStitchMaxPositionDelta = 8.0f;
    static constexpr float kPracticeStitchMaxMotionDelta = 3.0f;
    static constexpr float kPracticeStitchMinExtension = 0.5f;
    static constexpr float kTolSq = kReplayStartTolerance * kReplayStartTolerance;
    static constexpr float kWaveTeleportedTolerance = 30.0f;
    static constexpr float kNoDualTimeGap = 0.1f;
    static constexpr float kYEqualEps = 0.1f;
    static constexpr double kP1ContinuesPastP2Threshold = 0.5;
    bool m_playerPrevTeleportedP1 = false;
    bool m_playerPrevTeleportedP2 = false;
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

    PracticePath m_attemptCatalogPracticePath;

    bool m_allowWorkThisTick = false;
    bool m_allowSetPlayerPos = false;
    bool m_allowSetPlayerClickState = false;
    bool m_accurateHighTPSReplay = false;
    bool m_allowClearOldPlayerObjectsWhenMoreNeeded = false;
    size_t minKeepDead = 50;
    uint64_t m_tickId = 0;
    uint64_t m_lastWorkTickId = 0;

    cocos2d::CCPoint m_playerMirrorVisualOffsetP1 = {0.f, 0.f};
    cocos2d::CCPoint m_playerMirrorVisualOffsetP2 = {0.f, 0.f};

    float m_lastMirrorFlipT = 0.f;
    int m_mirrorAnchorSide = 0; // 0: normal, 1: mirror
    bool m_haveMirrorFlipSample = false;
    bool m_haveMirrorAnchorSide = false;

    int m_mirrorEndpointHoldFrames = 0;
    int m_pendingMirrorAnchorSide = -1;
    float m_lastMirrorVisualT = 0.f;

    int m_playerMirrorAnchorSide = 0; // 0: normal, 1: mirror
    float m_lastPlayerMirrorFlipT = 0.f;
    bool m_havePlayerMirrorFlipSample = false;
    bool m_havePlayerMirrorAnchorSide = false;
    int m_mirrorVisualWarmupFrames = 0;

    static constexpr float kMirrorEndpointEps_ = 0.001f;

    bool mirrorEndpointSide_(float t, int& sideOut) const;

    void syncGhostMirrorEndpointState_(int side);

    void syncPlayerMirrorEndpointState_(int side) {
        m_playerMirrorAnchorSide = side;
        m_havePlayerMirrorAnchorSide = true;
        m_lastPlayerMirrorFlipT = side ? 1.f : 0.f;
        m_havePlayerMirrorFlipSample = true;
    }

    float mirrorFlipT_() const;

    void updatePlayerMirrorAnchorSide_(float t);

    float playerMirrorProgress_(float rawT) const {
        return m_playerMirrorAnchorSide == 0
            ? rawT
            : 1.f - rawT;
    }

    void updateMirrorAnchorSide_(float t);

    bool mirrorEndpointVisualHold_(float rawT, float& visualTOut);

    float mirrorVisualT_(float rawT) const {
        return m_mirrorAnchorSide == 0
            ? rawT
            : 1.f - rawT;
    }

    float screenCenterXInNode_(cocos2d::CCNode* node) const;

    cocos2d::CCPoint mirrorDisplayPointInParentAtT_(
        cocos2d::CCPoint p,
        cocos2d::CCNode* parent,
        float t
    ) const;

    cocos2d::CCPoint parentVectorToPlayerLocal_(
        PlayerObject* player,
        cocos2d::CCPoint deltaParent
    ) const;

    void applyMirrorVisualTransformToNode_(cocos2d::CCNode* node);

    void resetMirrorVisualState_();

    void updateMirrorVisuals_();

    void applyPlayerVisualChildOffset_(
        PlayerObject* player,
        cocos2d::CCPoint wantedOffset
    );

    cocos2d::CCPoint playerMirrorVisualOffsetLocal_(PlayerObject* player);

    void updatePlayerMirrorVisuals_();

    void resetPlayerMirrorVisualOffsets_();

    // Cache the directory scan (updates when folder changes)
    std::vector<std::string> customSfx;
    std::filesystem::file_time_type customLastWrite{};
    uint64_t m_lastCustomSfxRefreshTick = 0;

    bool needToFreePlayerObjectsForWaitingQueue(size_t numCurrentlyDead);

    static constexpr uint64_t kCustomSfxRefreshIntervalTicks = 120;

    void refreshAndPreloadCustomExplodeSfx_(bool force = false);

    void invalidateAttemptPointerCaches_();

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

    bool matchesStartPositionFilter_(const Attempt& a) const;

    bool refreshAttemptCatalogForCurrentLevel_(bool forceRescan, bool pushCurrentAttempt);

    static void prunePracticePathSerials_(
        PracticePath& path,
        std::unordered_set<int> const& deleteSet
    );

    void clearRuntimeReplayStateAfterAttemptDeletion_();

    void removeDeletedAttemptsFromRuntime_(
    std::unordered_set<int> const& deleteSet
    );

    bool deleteAttemptsBySerialSet_(
        std::unordered_set<int> const& deleteSet
    );

    void clearAttemptCatalog_();

    bool scanAttemptCatalogForLevel_(int levelID);

    static uint64_t attemptIdentityKey_(int serial, bool practiceAttempt) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(serial)) << 1u) |
            (practiceAttempt ? 1ull : 0ull);
    }

    const APXAttemptDiskInfo* findCatalogEntryBySerialAndPractice_(
        int serial,
        bool practiceAttempt
    ) const;

    const APXAttemptDiskInfo* findCatalogEntryBySerial_(int serial) const {
        auto it = m_attemptCatalogBySerial.find(serial);
        if (it == m_attemptCatalogBySerial.end()) return nullptr;
        if (it->second >= m_attemptCatalog.size()) return nullptr;
        return &m_attemptCatalog[it->second];
    }

    size_t findLoadedAttemptIndexBySerialAndPractice_(
        int serial,
        bool practiceAttempt
    ) const;

    size_t findLoadedAttemptIndexBySerial_(int serial) const;

    size_t findLoadedAttemptIndexBySerialFiltered_(int serial, std::optional<bool> wantPractice) const {
        if (wantPractice.has_value()) {
            return findLoadedAttemptIndexBySerialAndPractice_(serial, wantPractice.value());
        }
        return findLoadedAttemptIndexBySerial_(serial);
    }

    Attempt* findLoadedAttemptBySerialAndPractice_(int serial, bool practiceAttempt) {
        const size_t idx = findLoadedAttemptIndexBySerialAndPractice_(serial, practiceAttempt);
        if (idx == SIZE_MAX) return nullptr;
        return &attempts[idx];
    }

    const Attempt* findLoadedAttemptBySerialAndPractice_(int serial, bool practiceAttempt) const {
        const size_t idx = findLoadedAttemptIndexBySerialAndPractice_(serial, practiceAttempt);
        if (idx == SIZE_MAX) return nullptr;
        return &attempts[idx];
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

    bool getPreloadSortInfoForSerial_(int serial, float& endX, double& endT) const;

    void buildInitialAttemptsFromPreloadSerialOrder_(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial
    );

    Attempt* ensureAttemptLoadedBySerialFiltered_(int serial, std::optional<bool> wantPractice, bool spawnNow = false, std::istream* batchStream = nullptr);

    Attempt* ensureAttemptLoadedBySerial_(int serial, bool spawnNow = false) {
        return ensureAttemptLoadedBySerialFiltered_(serial,std::nullopt,spawnNow);
    }

    Attempt* ensureAttemptLoadedBySerialAndPractice_(int serial,bool practiceAttempt,bool spawnNow = false) {
        return ensureAttemptLoadedBySerialFiltered_(serial,std::optional<bool>(practiceAttempt),spawnNow);
    }

    Attempt* ensurePracticeReplayOwnerLoaded_(
        int serial,
        char const* reason
    );

    void rebuildSerialCache_() const;

    inline Attempt* findAttemptBySerialAndPractice_(int serial,bool practiceAttempt) {
        const size_t idx = findLoadedAttemptIndexBySerialAndPractice_(serial,practiceAttempt);
        if (idx == SIZE_MAX) return nullptr;

        m_replayOwnerIndex = idx;
        return &attempts[idx];
    }

    static GLubyte clampU8_(int64_t v) {
        if (v < 0) return (GLubyte)0;
        if (v > 255) return (GLubyte)255;
        return (GLubyte)v;
    }

    static bool getSettingBoolOrDefault_(geode::Mod* mod, const char* key, bool def);

    static int getSettingIntOrDefault_(geode::Mod* mod, const char* key, int def);

    static std::string getSettingStrOrDefault_(geode::Mod* mod, const char* key, const char* def);

    cocos2d::ccColor3B randomGameColor(uint32_t seed, bool player1) {
        auto* gm = GameManager::sharedState();
        int paletteIdx = pickRandomAllowedGhostColorIdx(seed, player1);
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

    void rebuildRouteOwnerCache_();

    FORCE_INLINE bool isRouteOwnerForOffset_(Attempt const& a) const {
        if (!botActive) return false;
        if (!practiceRouteOwnerHidingEnabled_()) return false;
        if (a.serial <= 0) return false;

        return m_routeOwnerSerials.find(a.serial) != m_routeOwnerSerials.end();
    }

    FORCE_INLINE bool isOwnerAnywhereInRoute_(Attempt const& a) const {
        if (a.ignorePlayback) return true;

        if (!botActive) return false;

        if (!practiceRouteOwnerHidingEnabled_())
            return false;

        if (a.serial <= 0)
            return false;

        return m_currentlyHiddenSerials.find(a.serial) != m_currentlyHiddenSerials.end();
    }

    FORCE_INLINE bool isCurrentOwner_(size_t ai, const Attempt& a) const {
        if (!routeOwnersActive_()) return false;
        return (a.serial == m_activeOwnerSerial);
    }

    static void buildAccelTime(const std::vector<Frame>& v,
                           FrameAccelTime& a,
                           uint32_t binWQ = 600,
                           bool buildTimes = true);

    static size_t idxForTime(const FrameAccelTime& acc, const std::vector<Frame>& v, double t);

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

    void rebuildAttemptCountsIfNeeded();

    static int startModeForReplayFrame_(IconType mode);

    struct PracticeReplayRoute_ {
        std::vector<PracticeSegment> segments;
        int startSessionId = 0;
        float startX = 0.f;
        float endX = 0.f;
        int stitches = 0;
    };

    struct PracticeSplice_ {
        bool valid = false;
        double time = 0.0;
        float score = std::numeric_limits<float>::max();
    };

    static float practiceSessionEndX_(PracticeSession const& session);

    static float practiceSpliceScore_(Frame const& a, Frame const& b);

    static float practiceMotionScore_(
        std::vector<Frame> const& a,
        size_t ai,
        std::vector<Frame> const& b,
        size_t bi
    );

    static bool practiceDualAtTime_(Attempt const& attempt, double time) {
        if (!attempt.hadDual || attempt.p2.empty()) return false;
        const double first = static_cast<double>(attempt.p2.front().t);
        const double last = static_cast<double>(attempt.p2.back().t);
        return time >= first - 0.01 && time <= last + 0.01;
    }

    PracticeSplice_ findPracticeSplice_(
        std::vector<PracticeSegment> const& route,
        PracticeSession const& nextSession
    );

    static void trimPracticeRouteAfter_(std::vector<PracticeSegment>& route, double time);

    static void appendPracticeSessionFrom_(
        std::vector<PracticeSegment>& route,
        PracticeSession const& session,
        double time
    );

    bool buildSelectedPracticeReplayRoute_();

    void clearReplayStartPosOverride_();

    bool installReplayStartPosForSelectedPracticeSession_();

    float practicePreloadTargetStartX_() const;

    bool rebuildPracticePreloadSerialSetForCurrentStart_();

    void applyBotSafety_(bool on);

    static inline float lastXOf_(const std::vector<Frame>& v) {
        return v.empty() ? 0.f : static_cast<float>(v.back().x);
    }

    float attemptSpanX_(Attempt const& a) const;

    static uint32_t mix32_(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
    }

    static float signedUnit_(uint32_t seed) {
        uint32_t h = mix32_(seed);
        // [0,1) -> (-1,1)
        return ( (h / 4294967296.0f) * 2.f ) - 1.f;
    }

    void clearGhostOffset_(Attempt& a) {
        a.ghostOffsetPx = 0.f;
        a.ghostOffsetTime = 0.0;
        a.offsetApplied = true;
    }

    void applyRandomOffsetSingle(Attempt& a, bool forceNegative);

    void applyRandomOffsetsAll_();
    
    void refreshAttemptColors_(Attempt& a);

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

    void preloadAttempt_(Attempt& a, size_t attemptIdx = SIZE_MAX, bool force=false, bool prepareVisuals=true);

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

    void tagCompositeOwnersHidden_(bool hide);

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

    void forceShowPlayerVisuals(PlayerObject* p);

    bool primeGhostToPX_(Attempt& a, size_t attemptIdx, float pxw, float px2w);

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
    ) const;

    size_t pickWinningOrBestIndex_(std::optional<bool> wantPractice) const;

    float playerX_() const { return m_px; }
    float playerX2_() const { return m_px2; }
    bool shouldLockGhostX_() const {
        return m_randomDistPx == 0 && !botActive && m_pl &&
            !m_pl->m_isPlatformer && !m_pl->m_isPracticeMode;
    }

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

    void seekReplayCursorsToTimeExact_(const Attempt& owner, double sessionTime);

    void advanceReplayCursorsLockstep_(double sessionTime, bool wentBack);

    void adjustReplayCursorToTime_(double sessionTime);

    const Attempt* getReplayOwner_(double sessionTime);

    void updateGhostForAttempt_(Attempt& a, float px, float px2, size_t ai,
                            float dt, size_t stepFrames, bool mustRewind);

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

    size_t ptrToIndex_(const Attempt* p) const;

    void startNewAttempt();

    void sortBestToFront();

    static inline void considerByXThenSerial_(const Attempt& a, float& bestX, int& bestSerial, const Attempt*& bestPtr) {
        if (a.p1.empty()) return;
        float x = lastXOf_(a.p1);
        if (x > bestX || (x == bestX && a.serial > bestSerial)) {
            bestX = x;
            bestSerial = a.serial;
            bestPtr = &a;
        }
    }

    cocos2d::ccColor3B chooseColor1(bool isP1);
    cocos2d::ccColor3B chooseColor2(bool isP1);
    void hasGlowActive(bool isP1, bool& hasGlow) {
        if (isP1 && m_pl->m_player1) hasGlow = m_pl->m_player1->m_hasGlow;
        else if (m_pl->m_player2) hasGlow = m_pl->m_player2->m_hasGlow;
    }
    cocos2d::ccColor3B chooseGlowColor(bool isP1, bool& hasGlow);

    static PlayerObject* spawnGhostPO(PlayLayer* pl, cocos2d::CCNode* parent, GLubyte opacity);

    static void applyOpacityTree(cocos2d::CCNode* n, GLubyte o);

    GhostBatch& getOrCreateBatch_();

    void clearGhostBatches_();

    void rebuildGridThreasholdSet_();

    void rebuildGridPreloadedSet_();

    void rebuildGrid_();

    void pushAttempt(Attempt&& a, bool spawnNow = true);

    void clearAllGhostNodes();

    void forcePlayersVisible_();

    void clearHeldInputs_();

    inline bool waveTrailAddPointToPlayer(
        HardStreak* waveTrail,
        cocos2d::CCPoint point,
        bool isP1,
        bool m_skip,
        std::optional<bool> goingLeftOverride = std::nullopt
    ) {
        if (!waveTrail || !waveTrail->m_pointArray) {
            return false;
        }

        auto* arr = waveTrail->m_pointArray;

        if (arr->count() > 0) {
            auto* lastNode = static_cast<PointNode*>(
                arr->lastObject()
            );

            if (!lastNode) {
                return false;
            }

            const auto previousPoint = lastNode->m_point;

            // Exact duplicate (now in hardstreak itself)
            //if (
            //    previousPoint.x == point.x &&
            //    previousPoint.y == point.y
            //) {
            //    return false;
            //}

            bool goingLeft = false;

            if (goingLeftOverride.has_value()) {
                goingLeft = *goingLeftOverride;
            }
            else if (m_pl) {
                auto* player = isP1
                    ? m_pl->m_player1
                    : m_pl->m_player2;

                if (player) {
                    goingLeft = player->m_isGoingLeft;
                }
            }

            // Reject tiny playback jitter
            if (!goingLeft && point.x < previousPoint.x) {
                return false;
            }

            if (goingLeft && point.x > previousPoint.x) {
                return false;
            }
        }

        const bool oldAllow = m_allowWavePointAdding;
        m_allowWavePointAdding = true;

        waveTrail->addPoint(point);

        m_allowWavePointAdding = oldAllow;

        /*
        log::info(
            "Wave trail point array: {} points",
            arr->count()
        );

        for (unsigned int i = 0; i < arr->count(); ++i) {
            auto* node = static_cast<PointNode*>(
                arr->objectAtIndex(i)
            );

            if (!node) {
                log::info("  [{}] NULL", i);
                continue;
            }

            const auto& p = node->m_point;

            log::info(
                "  [{}] x={}, y={}",
                i,
                p.x,
                p.y
            );
        }*/

        return true;
    }

    cocos2d::CCPoint m_lastSnappedWavePointP1;
    cocos2d::CCPoint m_lastSnappedWavePointP2;

    bool m_haveLastSnappedWavePointP1 = false;
    bool m_haveLastSnappedWavePointP2 = false;

    bool m_lastSnappedWaveMiniP1 = false;
    bool m_lastSnappedWaveMiniP2 = false;
    bool m_lastSnappedWaveDirP1 = false;
    bool m_lastSnappedWaveDirP2 = false;
    bool m_CBFwaveFixEnabled = false;

    // CBF is evil and requires me to go through pain and suffering to "fix" wave trails without recording extra data
    std::optional<cocos2d::CCPoint> snapWavePointSimple_(
        cocos2d::CCPoint raw,
        bool isMini,
        bool isP1
    );

    inline void setMovementDirection(float previousY, float currentY, MovementDirection& movementDirectionVar) {
        if (previousY < currentY) movementDirectionVar = MovementDirection::Up;
        else if (previousY > currentY) movementDirectionVar = MovementDirection::Down;
        else movementDirectionVar = MovementDirection::Flat;
    }

    inline void markWavePoint(const cocos2d::CCPoint& pt, bool isP1) {
        bool& hasWavePoint = isP1 ? m_wavePointLastFrameP1 : m_wavePointLastFrameP2;
        cocos2d::CCPoint&  lastPoint = isP1 ? m_lastWavePointP1 : m_lastWavePointP2;

        hasWavePoint = true;
        lastPoint = pt;
    }

    void applyFrameAndClickToPlayerOverRange(bool isP1 = true);

    void forcePlayerHoldState_(PlayerObject* p, bool holdJump, bool holdL, bool holdR);
};

// Now this file isn't like 20k lines, yippie
