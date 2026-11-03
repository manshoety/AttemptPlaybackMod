#pragma once

#include <Geode/Geode.hpp>
#include "ghost_preview_appearance.hpp"
#include "../utils/attempt_scroll.hpp"

#include <array>
#include <vector>

class TabbedGhostSettingsPopup : public geode::Popup {
public:
    static TabbedGhostSettingsPopup* create();
    bool init(float width, float height);

protected:
    void update(float dt) override;
    void onClose(cocos2d::CCObject* sender) override;

private:
    cocos2d::CCNode* m_page = nullptr;
    attemptplayback::ui::AttemptScrollView* m_scroll = nullptr;
    cocos2d::CCMenu* m_tabs = nullptr;
    int m_tab = 0;
    int m_colorPlayer = 0;
    Slider* m_opacity = nullptr;
    Slider* m_distance = nullptr;
    Slider* m_textSize = nullptr;
    Slider* m_textOpacity = nullptr;
    Slider* m_markerSize = nullptr;
    Slider* m_markerThickness = nullptr;
    cocos2d::CCDrawNode* m_markerDraw = nullptr;
    Slider* m_markerOpacity = nullptr;
    geode::TextInput* m_textInput = nullptr;
    std::string m_textOriginal;
    cocos2d::CCLabelBMFont* m_distanceLabel = nullptr;
    cocos2d::CCLabelBMFont* m_opacityLabel = nullptr;
    cocos2d::CCLabelBMFont* m_textSizeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_textOpacityLabel = nullptr;
    cocos2d::CCLabelBMFont* m_markerSizeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_markerThicknessLabel = nullptr;
    cocos2d::CCLabelBMFont* m_markerOpacityLabel = nullptr;
    attemptplayback::ui::GhostPreviewAppearanceState m_previewAppearance;
    cocos2d::CCNode* m_previewWorld = nullptr;
    SimplePlayer* m_player = nullptr;
    SimplePlayer* m_behind = nullptr;
    SimplePlayer* m_ahead = nullptr;
    cocos2d::CCLabelBMFont* m_colorModeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_textPreviewLabel = nullptr;
    std::vector<cocos2d::CCLabelBMFont*> m_colorMarks;
    std::vector<CCMenuItemToggler*> m_switches;

    void onTab(cocos2d::CCObject*);
    void applyQueuedTab(float);
    void applyQueuedPage(float);
    void onCycleColor(cocos2d::CCObject*);
    void onOpacity(cocos2d::CCObject*);
    void onDistance(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*);
    void onColorPlayer(cocos2d::CCObject*);
    void onColorSlot(cocos2d::CCObject*);
    void onAllColors(cocos2d::CCObject*);
    void onOpenPresets(cocos2d::CCObject*);
    void onOpenText(cocos2d::CCObject*);
    void onOpenMarkers(cocos2d::CCObject*);
    void onOpenEffects(cocos2d::CCObject*);
    void onTextMode(cocos2d::CCObject*);
    void onTextSize(cocos2d::CCObject*);
    void onTextOpacity(cocos2d::CCObject*);
    void onMarkerSize(cocos2d::CCObject*);
    void onMarkerThickness(cocos2d::CCObject*);
    void onMarkerOpacity(cocos2d::CCObject*);
    void commitText();
    void refreshMarkerPreview();
    void refreshSliderLabels();
    void refreshPreview();
    void refreshTextPreview();
    void refreshColorMarks();
    void buildTabs();
    void buildPage();
};

class TabbedPlaybackSettingsPopup : public geode::Popup {
public:
    static TabbedPlaybackSettingsPopup* create();
    bool init(float width, float height);

protected:
    void onClose(cocos2d::CCObject* sender) override;

private:
    cocos2d::CCNode* m_page = nullptr;
    attemptplayback::ui::AttemptScrollView* m_scroll = nullptr;
    cocos2d::CCMenu* m_tabs = nullptr;
    int m_tab = 0;
    geode::TextInput* m_percent = nullptr;
    geode::TextInput* m_autosaveMinutes = nullptr;
    geode::TextInput* m_maxPlayerObjects = nullptr;
    cocos2d::CCLabelBMFont* m_capacityStatus = nullptr;

    void onTab(cocos2d::CCObject*);
    void applyQueuedTab(float);
    void onToggle(cocos2d::CCObject*);
    void buildTabs();
    void buildPage();
    void commitInputs();
    void stopReplayForFilterChange_();
    void applyPercentInput_(std::string const& value);
    void applyAutosaveInput_(std::string const& value);
    void applyPlayerObjectInput_(std::string const& value);
};
