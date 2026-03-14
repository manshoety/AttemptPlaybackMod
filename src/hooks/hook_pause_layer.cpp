// hook_pause_layer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/cocos/CCDirector.h>

#include "../utils/ui_utils.hpp"
#include "../ui/playback_mod_menu_popup.hpp"
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;
using namespace attemptplayback;

class $modify(MyPauseLayer, PauseLayer) {
    static void onModify(auto& self) {
        if (auto res = self.setHookPriorityPost("PauseLayer::customSetup", Priority::Last); !res) {
            geode::log::warn("Failed to set hook priority: {}", res.unwrapErr());
        }
    }

    void customSetup() {
        PauseLayer::customSetup();

        if (!Ghosts::I().isModEnabled())
            return;

        auto* leftMenu = typeinfo_cast<CCMenu*>(this->getChildByID("left-button-menu"));
        if (!leftMenu) return;

        // Prevent duplicates
        if (leftMenu->getChildByID("attemptplayback-button")) return;

        // Create spriteframe icon
        auto* spr = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png");
        if (!spr) {
            geode::log::warn("Failed to load sprite frame GJ_sRecentIcon_001.png");
            return;
        }

        // Circular background like the pause menu buttons
        auto* btnSprite = CircleButtonSprite::create(
            spr, CircleBaseColor::Green, CircleBaseSize::Small
        );
        if (!btnSprite) return;

        // Create button
        auto* btn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(MyPauseLayer::onAttemptPlaybackPopup)
        );
        btn->setID("attemptplayback-button");

        // Add to pause menu + relayout
        leftMenu->addChild(btn);
        leftMenu->updateLayout();
    }

    void onAttemptPlaybackPopup(CCObject*) {
        if (!Ghosts::I().isModEnabled())
            return;

        PlaybackModMenu::create()->show();
    }

private:
    void addAttemptPlaybackButton_(float) {
        if (!Ghosts::I().isModEnabled()) return;
        if (!this->getParent()) return;

        CCMenu* host = nullptr;

        if (auto n = this->getChildByIDRecursive("button-menu")) {
            host = typeinfo_cast<CCMenu*>(n);
        }
        if (!host) {
            host = findLargestMenu(this);
        }

        if (!host) {
            host = CCMenu::create();
            host->setPosition({0.f, 0.f});
            this->addChild(host, 9000);
        }

        if (!host->getChildByID("attemptplayback-btn")) {
            auto* btn = makeButton("PM", this, menu_selector(MyPauseLayer::openMenu), 0.75f);
            btn->setID("attemptplayback-btn");

            btn->setPosition({ 268.5f, -113.f });

            host->addChild(btn, 9001);
        }
    }

    void openMenu(CCObject*) {
        if (!Ghosts::I().isModEnabled())
            return;

        PlaybackModMenu::create()->show();
    }
};
