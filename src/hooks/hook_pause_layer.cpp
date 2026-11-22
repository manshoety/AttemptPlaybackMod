#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/utils/NodeIDs.hpp>

#include "../ui/playback_mod_menu_popup.hpp"
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        NodeIDs::provideFor(this);
        if (!Ghosts::I().isModEnabled()) return;

        auto* mainLayer = this->getChildByID("main-layer");
        auto* leftMenu = mainLayer
            ? typeinfo_cast<CCMenu*>(mainLayer->getChildByID("left-button-menu"))
            : nullptr;
        if (!leftMenu) {
            log::warn("Attempt Playback: PauseLayer/main-layer/left-button-menu not found");
            return;
        }

        if (leftMenu->getChildByID("attemptplayback-button"_spr)) return;

        auto* icon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png");
        if (!icon) {
            log::warn("Attempt Playback: GJ_sRecentIcon_001.png is unavailable");
            return;
        }

        auto* btnSprite = CircleButtonSprite::create(
            icon, CircleBaseColor::Green, CircleBaseSize::Small
        );
        if (!btnSprite) return;

        auto* btn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(MyPauseLayer::onAttemptPlaybackPopup)
        );
        if (!btn) return;
        btn->setID("attemptplayback-button"_spr);

        // Match the other side buttons
        constexpr float kButtonWidth = 27.f;
        constexpr float kButtonHeight = 28.5f;
        const CCSize buttonSize{kButtonWidth, kButtonHeight};
        btn->setContentSize(buttonSize);

        const auto spriteSize = btnSprite->getContentSize();
        if (spriteSize.width > 0.f && spriteSize.height > 0.f) {
            const float widthScale = kButtonWidth / spriteSize.width;
            const float heightScale = kButtonHeight / spriteSize.height;
            btnSprite->setScale(widthScale < heightScale ? widthScale : heightScale);
        }

        btnSprite->setAnchorPoint({0.5f, 0.5f});
        btnSprite->setPosition({kButtonWidth * 0.5f, kButtonHeight * 0.5f});

        leftMenu->addChild(btn);
        leftMenu->updateLayout();
    }

    void onAttemptPlaybackPopup(CCObject*) {
        if (!Ghosts::I().isModEnabled()) return;
        if (auto* popup = PlaybackModMenu::create()) {
            popup->m_noElasticity = true;
            popup->show();
        }
    }
};
