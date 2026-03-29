// playback_mod_menu_popup.hpp
#pragma once
#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/Slider.hpp>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include "../core/ghost_manager.hpp"

#include <vector>
#include <chrono>

class PreloadAttemptsPopup : public geode::Popup {
public:
    static PreloadAttemptsPopup* create(ReplayKind kind);
    bool init(float width, float height);

protected:
    void update(float dt) override;
    void onClose(cocos2d::CCObject* sender) override;

private:
    ReplayKind m_kind;

    cocos2d::CCLabelBMFont* m_numAttemptsLoadingLabel = nullptr;
    cocos2d::CCLabelBMFont* m_estimatedRamLabel = nullptr;
    geode::TextInput* m_numAttemptsToLoad = nullptr;
    cocos2d::CCLabelBMFont* m_alreadyPreloadedLabel = nullptr;

    CCMenuItemSpriteExtra* m_loadBtn = nullptr;
    CCMenuItemSpriteExtra* m_maxBtn = nullptr;

    int  m_totalAttempts = 0;
    int  m_lastClampedValue = -1;
    bool m_startedLoading = false;
    bool m_replayOn = false;

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
    void onDeleteSaveFile(CCObject*);
    void onToggleRecording(cocos2d::CCObject*);
    void onToggleInterpolation(cocos2d::CCObject*);
    void onToggleRandomIcons(cocos2d::CCObject*);
    void onToggleGhostsExplode(cocos2d::CCObject*);
    void onToggleGhostsExplodeSFX(cocos2d::CCObject*);
    void onToggleBlockRecording(cocos2d::CCObject*);
    void onToggleReplayPreventCompletion(cocos2d::CCObject*);
    void onCycleGhostColors(cocos2d::CCObject*);
    void onOpenColorSelector(cocos2d::CCObject*);
    void onOpenGhostDistance(cocos2d::CCObject*);
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
