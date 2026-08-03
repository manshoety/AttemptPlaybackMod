#pragma once

#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/binding/Slider.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/draw_nodes/CCDrawNode.h>

#include "../core/ghost_manager.hpp"

#include <vector>

class GhostTextPositionPopup : public geode::Popup {
public:
    static GhostTextPositionPopup* create();
    bool init(float width, float height);

protected:
    void onEnter() override;
    void onExit() override;
    void onClose(cocos2d::CCObject* sender) override;

private:
    struct HiddenNodeState {
        cocos2d::CCNode* node = nullptr;
        bool wasVisible = false;
    };

    Slider* m_xSlider = nullptr;
    Slider* m_ySlider = nullptr;
    cocos2d::CCLabelBMFont* m_xValueLabel = nullptr;
    cocos2d::CCLabelBMFont* m_yValueLabel = nullptr;
    std::vector<HiddenNodeState> m_hiddenNodes;
    bool m_previewModeActive = false;

    void onXSlider(cocos2d::CCObject*);
    void onYSlider(cocos2d::CCObject*);
    void onDone(cocos2d::CCObject*);
    void onReset(cocos2d::CCObject*);
    void refreshPositionControls_();
    void beginPreviewMode_();
    void endPreviewMode_();
    void hideNodeForPreview_(cocos2d::CCNode* node);
};

class GhostTextPopup : public geode::Popup {
public:
    static GhostTextPopup* create();
    bool init(float width, float height);

protected:
    void update(float dt) override;

private:
    CCMenuItemToggler* m_showToggle = nullptr;
    CCMenuItemToggler* m_flashToggle = nullptr;
    geode::TextInput* m_customInput = nullptr;
    Slider* m_sizeSlider = nullptr;
    Slider* m_opacitySlider = nullptr;
    cocos2d::CCLabelBMFont* m_sizeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_opacityLabel = nullptr;
    cocos2d::CCLabelBMFont* m_previewLabel = nullptr;
    cocos2d::CCMenu* m_rootMenu = nullptr;

    void onToggleShow(cocos2d::CCObject*);
    void onToggleFlash(cocos2d::CCObject*);
    void onModeAlive(cocos2d::CCObject*);
    void onModeDead(cocos2d::CCObject*);
    void onModeCustom(cocos2d::CCObject*);
    void onVariables(cocos2d::CCObject*);
    void onResetText(cocos2d::CCObject*);
    void onConfigurePosition(cocos2d::CCObject*);
    void onSizeSlider(cocos2d::CCObject*);
    void onOpacitySlider(cocos2d::CCObject*);

    void refreshModeButtons_();
    void refreshPreview_();
    void refreshLabels_();
    void setModeVariantVisible_(const char* id, bool visible);
};

class DeathMarkerSettingsPopup : public geode::Popup {
public:
    static DeathMarkerSettingsPopup* create();
    bool init(float width, float height);

protected:
    void update(float dt) override;

private:
    CCMenuItemToggler* m_showToggle = nullptr;
    Slider* m_sizeSlider = nullptr;
    Slider* m_thicknessSlider = nullptr;
    Slider* m_opacitySlider = nullptr;
    cocos2d::CCLabelBMFont* m_sizeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_thicknessLabel = nullptr;
    cocos2d::CCLabelBMFont* m_opacityLabel = nullptr;
    cocos2d::CCDrawNode* m_preview = nullptr;

    void onToggleShow(cocos2d::CCObject*);
    void onSizeSlider(cocos2d::CCObject*);
    void onThicknessSlider(cocos2d::CCObject*);
    void onOpacitySlider(cocos2d::CCObject*);
    void refreshPreview_();
    void refreshLabels_();
};

class GhostSettingsPopup : public geode::Popup {
public:
    static GhostSettingsPopup* create();
    bool init(float width, float height);

protected:
    void update(float dt) override;

private:
    CCMenuItemToggler* m_ghostTextToggle = nullptr;
    CCMenuItemToggler* m_deathMarkersToggle = nullptr;
    cocos2d::CCLabelBMFont* m_distanceValue = nullptr;

    void onOpenColorSelector(cocos2d::CCObject*);
    void onOpenGhostDistance(cocos2d::CCObject*);
    void onToggleGhostText(cocos2d::CCObject*);
    void onConfigureGhostText(cocos2d::CCObject*);
    void onToggleDeathMarkers(cocos2d::CCObject*);
    void onConfigureDeathMarkers(cocos2d::CCObject*);
    void refresh_();
};
