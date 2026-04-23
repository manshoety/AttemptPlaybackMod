// ghost_distance_popup.hpp
#pragma once
#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/General.hpp>

#include <Geode/binding/Slider.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>

#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace geode::prelude;

class GhostDistancePopup : public Popup {
public:
    static GhostDistancePopup* create();
    bool init(float width, float height);

protected:
    void onExit() override;

private:
    cocos2d::CCMenu* m_menu = nullptr;

    cocos2d::CCLabelBMFont* m_lblTitle = nullptr;
    cocos2d::CCLabelBMFont* m_lblValue = nullptr;

    cocos2d::extension::CCScale9Sprite* m_previewCard = nullptr;

    SimplePlayer* m_player = nullptr;
    SimplePlayer* m_ghostAhead = nullptr;
    SimplePlayer* m_ghostBehind = nullptr;

    Slider* m_slider = nullptr;

    float m_distancePx = 40.f;

    float m_cubeVisualPx = 30.f;
    float m_pxPerGameX = 1.f;
    float m_minGamePx = -100.f;
    float m_maxGamePx = 174.f;
    static constexpr float kUnitsPerCube = 30.f;
    static constexpr const char* kKeyGhostDist = "ghost-distance";

    void buildUI_();
    void ensurePreview_();
    void computePreviewScale_();
    void layoutPreview_();
    void updateValueLabel_();

    void onSlide(cocos2d::CCObject*);

    static float getSavedDistance_();
    static void setSavedDistance_(float px);
};

cocos2d::CCLayer* CreateGhostDistancePopup();
