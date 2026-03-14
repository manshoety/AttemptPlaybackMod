#include "ui_utils.hpp"
#include <Geode/cocos/CCDirector.h>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;
cocos2d::CCPoint screenToMenu(cocos2d::CCMenu* menu, cocos2d::CCPoint screen) {
    auto* director = cocos2d::CCDirector::sharedDirector();
    auto* running = director->getRunningScene();
    auto world = running->convertToNodeSpace(screen);
    return menu->convertToNodeSpace(running->convertToWorldSpace(world));
}

cocos2d::CCMenu* findLargestMenu(cocos2d::CCNode* root) {
    cocos2d::CCMenu* best=nullptr; int bestCount=-1;
    if (!root) return nullptr;
    auto* children = root->getChildren();
    if (children) {
        for (unsigned i=0;i<children->count();++i) {
            auto* node = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
            if (auto* menu = typeinfo_cast<cocos2d::CCMenu*>(node)) {
                int cnt = menu->getChildren() ? static_cast<int>(menu->getChildren()->count()) : 0;
                if (cnt > bestCount) { bestCount = cnt; best = menu; }
            }
            if (auto* sub = findLargestMenu(node)) {
                int cnt = sub->getChildren() ? static_cast<int>(sub->getChildren()->count()) : 0;
                if (cnt > bestCount) { bestCount = cnt; best = sub; }
            }
        }
    }
    return best;
}

CCMenuItemSpriteExtra* makeButton(const char* text, CCObject* target,
                                  cocos2d::SEL_MenuHandler cb, float scale) {
    auto bg = cocos2d::CCSprite::create("GJ_button_01.png");
    if (!bg) bg = cocos2d::CCSprite::create();
    bg->setScale(scale);
    auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
    label->setScale(0.5f);
    auto sz = bg->getContentSize();
    label->setPosition({ sz.width * 0.5f, sz.height * 0.5f });
    bg->addChild(label);
    return CCMenuItemSpriteExtra::create(bg, target, cb);
}

cocos2d::CCLabelBMFont* findLabelRecursive(cocos2d::CCNode* n) {
    if (!n) return nullptr;
    auto arr = n->getChildren();
    if (!arr) return nullptr;
    for (unsigned i=0; i<arr->count(); ++i) {
        auto child = static_cast<CCNode*>(arr->objectAtIndex(i));
        if (auto lbl = typeinfo_cast<CCLabelBMFont*>(child)) return lbl;
        if (auto nested = typeinfo_cast<CCNode*>(child)) {
            if (auto lbl2 = findLabelRecursive(nested)) return lbl2;
        }
    }
    return nullptr;
}

CCMenuItemSpriteExtra* findDefaultCloseButton(cocos2d::CCNode* root) {
    if (!root) return nullptr;

    for (auto* child : root->getChildrenExt()) {
        if (auto* menu = typeinfo_cast<CCMenu*>(child)) {
            if (menu->getChildrenCount() == 1) {
                if (auto* btn = typeinfo_cast<CCMenuItemSpriteExtra*>(
                        menu->getChildren()->objectAtIndex(0)
                    )) {
                    return btn;
                }
            }
        }
    }
    return nullptr;
}

