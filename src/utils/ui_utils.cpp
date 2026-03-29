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

void setChildrenInvisible(cocos2d::CCNode* root) {
    if (!root) return;
    for (auto* child : root->getChildrenExt()) {
        child->setVisible(false);
    }
}

CCSprite* createFullscreenGradient_() {
    ccColor3B color = { 69, 60, 177 };
    CCSprite* gradient = CCSprite::create("GJ_gradientBG.png");
    if (!gradient) gradient = CCSprite::create("GJ_gradientBG-hd.png");

    if (!gradient) {
        log::warn("Failed to load GJ_gradientBG.png / GJ_gradientBG-hd.png. Using empty sprite.");
        gradient = CCSprite::create();
    }

    gradient->setColor(color);
    gradient->setAnchorPoint({ 0.5f, 0.5f });

    const CCSize win = CCDirector::sharedDirector()->getWinSize();
    const CCSize tex = gradient->getContentSize();

    if (tex.width > 0.f && tex.height > 0.f) {
        gradient->setScaleX((win.width + 32.f) / tex.width);
        gradient->setScaleY((win.height + 32.f) / tex.height);
    }
    else {
        gradient->setScaleX(64.f);
        gradient->setScaleY(64.f);
    }
        return gradient;
}

float kPopupMinW = 260.f;
float kPopupMinH = 180.f;

cocos2d::CCSize fitPopupToWindow_(float designW, float designH) {
    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();

    float maxW = std::max(kPopupMinW, win.width);
    float maxH = std::max(kPopupMinH, win.height);

    return {
        std::min(designW, maxW),
        std::min(designH, maxH)
    };
}

float computeFitScale_(float actualW, float actualH, float designW, float designH) {
    if (designW <= 0.f || designH <= 0.f) return 1.f;

    float sx = actualW / designW;
    float sy = actualH / designH;
    float s = std::min({ 1.f, sx, sy });
    s *= 1.06f; // Fit properly

    // log::info("Scale: actual: {} {} normal: {} {}", actualW, actualH, designW, designH);
    return s;
}





