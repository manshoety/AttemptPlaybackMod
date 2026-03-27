// ghost_distance_popup.cpp
#include "ghost_distance_popup.hpp"
#include <Geode/ui/BasedButtonSprite.hpp>
#include <UIBuilder.hpp>
#include <Geode/loader/Mod.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace {
    constexpr float kW = 420.f;
    constexpr float kH = 260.f;
}

GhostDistancePopup* GhostDistancePopup::create() {
    auto ret = new GhostDistancePopup();
    if (ret && ret->init(kW, kH)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool GhostDistancePopup::init(float width, float height) {
    if (!Popup::init(width, height))
            return false;
    this->setID("ghostDistance-popup"_spr);
    setTitle("Ghost Distance");

    m_mainLayer->setLayout(AnchorLayout::create());

    m_distancePx = getSavedDistance_();

    buildUI_();
    ensurePreview_();
    computePreviewScale_();

    const float t = std::clamp(m_distancePx / m_maxGamePx, 0.f, 1.f);
    if (m_slider) m_slider->setValue(t);

    updateValueLabel_();
    layoutPreview_();

    return true;
}

void GhostDistancePopup::updateValueLabel_() {
    if (!m_lblValue) return;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Distance (px): %.0f", m_distancePx);
    m_lblValue->setString(buf);
}

void GhostDistancePopup::onExit() {
    this->unscheduleAllSelectors();
    Popup::onExit();
}

void GhostDistancePopup::buildUI_() {
    // Center root container
    auto* root = CCNode::create();
    root->setContentSize({0.f, 0.f});
    root->setAnchorPoint({0.5f, 0.5f});
    root->setPosition(m_mainLayer->getContentSize() * 0.5f);
    m_mainLayer->addChild(root);

    // preview card
    m_previewCard = CCScale9Sprite::create("square02b_001.png");
    m_previewCard->setColor({0, 0, 0});
    m_previewCard->setOpacity(78);
    m_previewCard->setContentSize({ kW - 32.f, 120.f });
    m_previewCard->setPosition({ 0.f, 17.f });
    root->addChild(m_previewCard);

    // title
    m_lblTitle = CCLabelBMFont::create("Max distance from player", "goldFont.fnt");
    m_lblTitle->setScale(0.55f);
    m_lblTitle->setPosition({ 0.f, 92.f });
    root->addChild(m_lblTitle);

    // slider
    m_menu = CCMenu::create();
    m_menu->setPosition({0.f, 0.f});
    root->addChild(m_menu);

    m_slider = Slider::create(this, menu_selector(GhostDistancePopup::onSlide), 0.9f);
    m_slider->setID("ghost-distance-limit"_spr);
    m_slider->setScale(0.85f);
    m_slider->setPosition({ 0.f, -10.f });
    m_slider->setAnchorPoint({0.0f, 0.0f});
    m_menu->addChild(m_slider);

    // value label
    m_lblValue = CCLabelBMFont::create("Distance (px): --", "goldFont.fnt");
    m_lblValue->setScale(0.38f);
    m_lblValue->setPosition({ 0.f, -30.f });
    root->addChild(m_lblValue);
}

void GhostDistancePopup::ensurePreview_() {
    auto gm = GameManager::sharedState();
    ccColor3B c1 = {255,255,255}, c2 = {255,255,255};
    if (gm) {
        c1 = gm->colorForIdx(gm->getPlayerColor());
        c2 = gm->colorForIdx(gm->getPlayerColor2());
    }

    if (!m_player) {
        m_player = SimplePlayer::create(1);
        m_player->setSecondColor(c2);
        m_player->setColor(c1);
        m_player->setScale(0.8f);
        m_previewCard->addChild(m_player);
    }
    if (!m_ghostAhead) {
        m_ghostAhead = SimplePlayer::create(1);
        m_ghostAhead->setSecondColor(c2);
        m_ghostAhead->setColor(c1);
        m_ghostAhead->setScale(0.8f);
        m_ghostAhead->setOpacity(128);
        m_previewCard->addChild(m_ghostAhead);
    }
    if (!m_ghostBehind) {
        m_ghostBehind = SimplePlayer::create(1);
        m_ghostBehind->setSecondColor(c2);
        m_ghostBehind->setColor(c1);
        m_ghostBehind->setScale(0.8f);
        m_ghostBehind->setOpacity(128);
        m_previewCard->addChild(m_ghostBehind);
    }
}

void GhostDistancePopup::computePreviewScale_() {
    if (!m_previewCard) return;

    float measured = 30.f;
    if (m_player) {
        auto bb = m_player->boundingBox();
        measured = bb.size.width > 1.f ? bb.size.width : measured;
    }
    m_cubeVisualPx = measured;

    m_pxPerGameX = m_cubeVisualPx / kUnitsPerCube;

    const float innerW = m_previewCard->getContentSize().width - 40.f;
    const float halfW = std::max(0.f, innerW * 0.5f);
    m_maxGamePx = std::max(1.f, halfW / std::max(1.f, m_pxPerGameX));
}

void GhostDistancePopup::layoutPreview_() {
    if (!m_previewCard) return;

    const float innerW = m_previewCard->getContentSize().width - 40.f;
    const float innerH = m_previewCard->getContentSize().height - 20.f;

    const float cx = 20.f + innerW * 0.5f;
    const float cy = 10.f + innerH * 0.5f;

    const float clampedGame = std::clamp(m_distancePx, 0.f, m_maxGamePx);
    const float dx = clampedGame * m_pxPerGameX;

    if (m_player)      m_player->setPosition({ cx, cy });
    if (m_ghostAhead)  m_ghostAhead->setPosition({ cx + dx, cy });
    if (m_ghostBehind) m_ghostBehind->setPosition({ cx - dx, cy });
}

void GhostDistancePopup::onSlide(CCObject* obj) {
    auto* n = typeinfo_cast<cocos2d::CCNode*>(obj);
    while (n && !typeinfo_cast<Slider*>(n)) n = n->getParent();
    if (!n) return;

    auto* s = static_cast<Slider*>(n);
    const float t = std::clamp(s->getValue(), 0.f, 1.f);

    m_distancePx = std::round(m_maxGamePx * t);
    setSavedDistance_(m_distancePx);

    updateValueLabel_();
    layoutPreview_();
}

float GhostDistancePopup::getSavedDistance_() {
    if (Mod::get()->hasSavedValue(kKeyGhostDist)) {
        return static_cast<float>(Mod::get()->getSavedValue<int64_t>(kKeyGhostDist));
    }
    return 45.f;
}
void GhostDistancePopup::setSavedDistance_(float px) {
    const int dist = std::max(0, (int)std::lround(px));
    Mod::get()->setSavedValue(kKeyGhostDist, (int64_t)dist);
    Ghosts::I().setGhostDistance(dist);
    
}

cocos2d::CCLayer* CreateGhostDistancePopup() {
    auto* p = GhostDistancePopup::create();
    return p;
}
