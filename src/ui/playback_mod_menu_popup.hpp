// playback_mod_menu_popup.hpp
#pragma once
#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/Slider.hpp>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <cue/RadioLogic.hpp>

#include "../core/ghost_manager.hpp"

#include <vector>
#include <unordered_set>
#include <chrono>

class PracticeRunSelectPopup : public geode::Popup {
public:
    static PracticeRunSelectPopup* create();
    bool init(float width, float height);

protected:
    void onClose(cocos2d::CCObject* sender) override;

private:
    enum class SortMode { Recent, Furthest, Attempts };

    struct RunInfo {
        int sessionId = 0;
        int attempts = 0;
        float startPercent = 0.f;
        float endPercent = 0.f;
        float startX = 0.f;
        float endX = 0.f;
        double replayEndTime = 0.0;
        bool completed = false;
    };

    std::vector<RunInfo> m_allRuns;
    std::vector<RunInfo> m_runs;
    std::unordered_set<int> m_selectedSessionIds;
    SortMode m_sortMode = SortMode::Recent;
    int m_page = 0;
    bool m_continuing = false;
    bool m_onlyCurrentStart = false;
    bool m_haveCurrentStart = false;
    float m_currentStartX = 0.f;
    float m_currentStartPercent = 0.f;

    cocos2d::CCNode* m_rowsRoot = nullptr;
    cocos2d::CCLabelBMFont* m_sortValueLabel = nullptr;
    cocos2d::CCLabelBMFont* m_selectionLabel = nullptr;
    CCMenuItemSpriteExtra* m_continueBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevEndBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextEndBtn = nullptr;
    CCMenuItemToggler* m_currentStartToggle = nullptr;
    cocos2d::CCLabelBMFont* m_currentStartLabel = nullptr;

private:
    void loadRuns_();
    void applyCurrentStartFilter_();
    void sortRuns_();
    void rebuildRows_();
    void refreshFooter_();
    int maxPage_() const;
    bool selectedRunsHaveGap_(float& gapEndPercent, float& nextStartPercent) const;

    void onToggleRun(cocos2d::CCObject* sender);
    void onToggleCurrentStart(cocos2d::CCObject* sender);
    void onCycleSort(cocos2d::CCObject* sender);
    void onPrevPage(cocos2d::CCObject* sender);
    void onNextPage(cocos2d::CCObject* sender);
    void onContinue(cocos2d::CCObject* sender);
};

class PreloadAttemptsPopup : public geode::Popup {
public:
    static PreloadAttemptsPopup* create(ReplayKind kind);
    bool init(float width, float height);

protected:
    void update(float dt) override;
    void onClose(cocos2d::CCObject* sender) override;

private:
    ReplayKind m_kind;

    cocos2d::CCLabelBMFont* m_percentageLimitEnabledLabel = nullptr;
    cocos2d::CCLabelBMFont* m_sortByLabel = nullptr;
    cocos2d::CCLabelBMFont* m_numAttemptsLoadingLabel = nullptr;
    cocos2d::CCLabelBMFont* m_estimatedRamLabel = nullptr;
    geode::TextInput* m_numAttemptsToLoad = nullptr;
    cocos2d::CCLabelBMFont* m_alreadyPreloadedLabel = nullptr;
    cocos2d::CCMenu* m_sortMenu = nullptr;

    CCMenuItemSpriteExtra* m_loadBtn = nullptr;
    CCMenuItemSpriteExtra* m_maxBtn = nullptr;

    int  m_totalAttempts = 0;
    int  m_lastClampedValue = -1;
    bool m_startedLoading = false;
    bool m_replayOn = false;
    bool m_isReplayBest = true;

    cue::RadioLogic<size_t> m_sortRadio;
    PreloadSortMode m_sortMode = PreloadSortMode::Best;

private:
    void onPressLoad(cocos2d::CCObject*);
    void onPressMax(cocos2d::CCObject*);

    int  sanitizeParseClamp_();
    void refreshInfoLabels_(int clampedN);
    void setInputValue_(int v);
    void setPreloadValue_();

    void beginLoading_(int clampedN);
    void tickLoading_();
    void finishAndStartReplay_();
};

class PlaybackModMenu : public geode::Popup {
public:
    static PlaybackModMenu* create();
    void syncUIFromRuntime();
    bool init(float width, float height);

protected:
    void update(float dt) override;

private:
    CCMenuItemToggler* m_tgRecording = nullptr;
    CCMenuItemToggler* m_tgInterp = nullptr;
    CCMenuItemToggler* m_tgRandomIcons = nullptr;
    CCMenuItemToggler* m_tgGhostsExplode = nullptr;
    CCMenuItemToggler* m_tgGhostsExplodeSFX = nullptr;
    CCMenuItemToggler* m_tgBlockRecording = nullptr;
    CCMenuItemToggler* m_tgBlockRecordingOnNoclip = nullptr;
    CCMenuItemToggler* m_tgReplayPreventCompletion    = nullptr;

    Slider* m_opacitySlider = nullptr;
    cocos2d::CCLabelBMFont* m_opacityLabel  = nullptr;

    cocos2d::CCLabelBMFont* m_colorModeLabel = nullptr;

    cocos2d::CCMenu* m_playbackButtonsRow = nullptr;
    cocos2d::CCMenuItem* m_stopReplayBtn = nullptr;
    bool m_lastReplayingState = false;
    int m_suppressReplayPollFrames = 0;

private:
    void buildTemplateUI_();
    void scaleUIForThatOneTabletUser(float designWidth, float designHeight);

    void onReplayBest(cocos2d::CCObject*);
    void onReplayPractice(cocos2d::CCObject*);
    void onOpenAttemptManager(cocos2d::CCObject*);
    void onToggleRecording(cocos2d::CCObject*);
    void onToggleInterpolation(cocos2d::CCObject*);
    void onToggleRandomIcons(cocos2d::CCObject*);
    void onToggleGhostsExplode(cocos2d::CCObject*);
    void onToggleGhostsExplodeSFX(cocos2d::CCObject*);
    void onToggleBlockRecording(cocos2d::CCObject*);
    void onToggleBlockRecordingOnNoclip(cocos2d::CCObject*);
    void onToggleReplayPreventCompletion(cocos2d::CCObject*);
    void onCycleGhostColors(cocos2d::CCObject*);
    void onOpenColorSelector(cocos2d::CCObject*);
    void onOpenGhostDistance(cocos2d::CCObject*);
    void onOpenGhostSettings(cocos2d::CCObject*);
    void onOpenPlaybackSettings(cocos2d::CCObject*);
    void onFreeRobux(cocos2d::CCObject*);
    void onExitButton(cocos2d::CCObject*);

    void onSliderInt(cocos2d::CCObject*);
    
    void onStopReplay(cocos2d::CCObject*);
    void cacheReplayButtons_();
    bool queryIsReplaying_() const;
    void refreshReplayButtons_();

    void refreshToggles_();
    void refreshColorsLabel_();
    static std::pair<std::string, float> sliderKeyAndValue_(cocos2d::CCObject* obj);
};

class PlaybackSettingsPopup : public geode::Popup {
public:
    static PlaybackSettingsPopup* create();
    bool init(float width, float height);

protected:
    void update(float dt) override;
    void onClose(cocos2d::CCObject* sender) override;

private:
    CCMenuItemToggler* m_tgCustomDeathSound = nullptr;
    cocos2d::CCMenu* m_customDeathSoundMenu = nullptr;
    CCMenuItemSpriteExtra* m_openCustomDeathSoundFolderBtn = nullptr;
    CCMenuItemSpriteExtra* m_downloadMemeSoundsBtn = nullptr;
    bool m_customDeathSoundOn = false;

    CCMenuItemToggler* m_tgLimitVisible = nullptr;
    geode::TextInput* m_maxVisibleInput = nullptr;
    cocos2d::CCLabelBMFont* m_maxVisibleLabel = nullptr;

    CCMenuItemToggler* m_tgOnlyPastPercent = nullptr;
    geode::TextInput* m_percentInput = nullptr;
    cocos2d::CCLabelBMFont* m_percentDynamicLabel = nullptr;

    int m_lastMaxVisible = -1;
    float m_lastPercent = -1.f;
    bool m_limitVisibleOn = false;
    bool m_onlyPastOn = false;
    bool m_initialOnlyPastOn = false;
    float m_initialPercent = 0.f;

private:
    void onToggleLimitVisible(cocos2d::CCObject*);
    void onToggleOnlyPast(cocos2d::CCObject*);
    float sanitizeParseClampPercent_();
    void setPercentInput_(float v);

    void onToggleCustomDeathSound(cocos2d::CCObject*);
    void onOpenCustomDeathSoundFolder(cocos2d::CCObject*);
    void onDownloadMemeSounds(cocos2d::CCObject*);
    void pushCustomDeathSoundToRuntime_();

    int sanitizeParseClampMaxVisible_();
    void setMaxVisibleInput_(int v);
    void refreshDependentVisibility_();
    void refreshPercentLabel_(float percent);

    void pullFromRuntime_();
    void pushLimitVisibleToRuntime_();
    void pushOnlyPastToRuntime_();
    void pushMaxVisibleToRuntime_(int v);
    void pushPercentToRuntime_(float percent);
};
