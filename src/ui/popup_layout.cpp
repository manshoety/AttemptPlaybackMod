#include "popup_layout.hpp"

#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Layout.hpp>

using namespace geode::prelude;
using namespace cocos2d;

namespace attemptplayback::ui {

bool LayoutPopup::init(float width, float height) {
    m_design = {width, height};
    if (!popup_layout::validSize(m_design) || !Popup::init(width, height)) return false;

    m_noElasticity = true;

    if (m_closeBtn) {
        if (auto* sprite = CircleButtonSprite::createWithSpriteFrameName(
                "geode.loader/close.png", .85f, CircleBaseColor::Green)) {
            const auto size = sprite->getContentSize();
            const float diameter = std::max(size.width, size.height);
            if (diameter > 0.f) {
                setCloseButtonSpr(sprite, popup_layout::CloseDiameter / diameter);
            }
        }
        const float inset = popup_layout::CloseCenterInset;
        m_closeBtn->ignoreAnchorPointForPosition(false);
        m_closeBtn->setAnchorPoint({.5f, .5f});
        m_closeBtn->setLayoutOptions(AnchorLayoutOptions::create()
            ->setAnchor(Anchor::TopLeft)->setOffset({inset, -inset}));
        m_closeBtn->setPosition({inset, height - inset});
        m_closeBtn->setZOrder(1000);
    }

    if (m_bgSprite) m_bgSprite->setPosition({width * .5f, height * .5f});
    refreshLayout(0.f);
    return true;
}

void LayoutPopup::onEnter() {
    Popup::onEnter();
    m_lastViewport = {0.f, 0.f};
    refreshLayout(0.f);
    schedule(schedule_selector(LayoutPopup::refreshLayout), 0.f);
}

void LayoutPopup::onExit() {
    unschedule(schedule_selector(LayoutPopup::refreshLayout));
    Popup::onExit();
}

void LayoutPopup::useFullscreenGradient(ccColor3B color) {
    if (!m_fullscreenGradient) {
        auto* gradient = CCSprite::create("GJ_gradientBG.png");
        if (!gradient) gradient = CCSprite::create("GJ_gradientBG-hd.png");
        if (!gradient || gradient->getContentSize().width <= 0.f
            || gradient->getContentSize().height <= 0.f) {
            log::warn("[AttemptPlayback] Missing fullscreen popup gradient");
            return;
        }
        m_fullscreenGradient = gradient;
        m_fullscreenGradient->setAnchorPoint({.5f, .5f});
        m_fullscreenGradient->setID("attempt-playback-fullscreen-gradient");
        addChild(m_fullscreenGradient, 0);
        reorderChild(m_mainLayer, 1);
    }
    m_fullscreenGradient->setColor(color);
    if (m_bgSprite) m_bgSprite->setVisible(false);
    m_lastViewport = {0.f, 0.f};
    refreshLayout(0.f);
}

void LayoutPopup::useTransparentPreview() {
    if (m_bgSprite) m_bgSprite->setVisible(false);
    if (m_title) m_title->setVisible(false);
    if (m_closeBtn) {
        m_closeBtn->setVisible(false);
        m_closeBtn->setEnabled(false);
    }
    setOpacity(0);
}

void LayoutPopup::refreshLayout(float) {
    if (!m_mainLayer) return;
    const auto win = CCDirector::sharedDirector()->getWinSize();
    const popup_layout::Size viewport{win.width, win.height};
    const float scale = popup_layout::fitScale(viewport, m_design);
    if (!(scale > 0.f)) return;

    const bool resized = viewport.width != m_lastViewport.width
        || viewport.height != m_lastViewport.height;
    if (resized) {
        m_lastViewport = viewport;
        setContentSize(win);
        m_mainLayer->ignoreAnchorPointForPosition(false);
        m_mainLayer->setAnchorPoint({.5f, .5f});
        m_mainLayer->setPosition({win.width * .5f, win.height * .5f});
        if (m_fullscreenGradient) {
            const auto texture = m_fullscreenGradient->getContentSize();
            m_fullscreenGradient->setPosition({win.width * .5f, win.height * .5f});
            if (texture.width > 0.f && texture.height > 0.f) {
                m_fullscreenGradient->setScaleX((win.width + 2.f) / texture.width);
                m_fullscreenGradient->setScaleY((win.height + 2.f) / texture.height);
            }
        }
    }
    if (m_mainLayer->getScaleX() != scale || m_mainLayer->getScaleY() != scale) {
        m_mainLayer->setScale(scale);
    }
}

}
