// hook_menu_layer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/binding/MenuLayer.hpp>

using namespace geode::prelude;

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        /*
        auto myButton = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"),
            this, menu_selector(MyMenuLayer::onMyButton)
        );
        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(myButton);
            myButton->setID("my-button"_spr);
            menu->updateLayout();
        }
            */
        return true;
    }
    /*
    void onMyButton(CCObject*) {
        FLAlertLayer::create("Message from manshoety", "This mod might blow up your PC. Good luck!", "Plese Don't")->show();
    }
    */
};
