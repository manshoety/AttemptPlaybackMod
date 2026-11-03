#pragma once

#include <Geode/Geode.hpp>

#include <UIBuilder.hpp>

using namespace geode::prelude;

cocos2d::CCPoint screenToMenu(cocos2d::CCMenu* menu, cocos2d::CCPoint screen);
cocos2d::CCMenu* findLargestMenu(cocos2d::CCNode* root);
CCMenuItemSpriteExtra* makeButton(const char* text, cocos2d::CCObject* target,
                                      cocos2d::SEL_MenuHandler cb, float scale = 1.0f);
cocos2d::CCLabelBMFont* findLabelRecursive(cocos2d::CCNode* n);
CCMenuItemSpriteExtra* findDefaultCloseButton(cocos2d::CCNode* root);
void setChildrenInvisible(cocos2d::CCNode* root);
cocos2d::CCSprite* createFullscreenGradient_();
cocos2d::CCSize fitPopupToWindow_(float designW, float designH);
float computeFitScale_(float actualW, float actualH, float designW, float designH);

template <class PopupT>
auto createTextButton_(
    PopupT* self,
    const char* spriteFrame,
    const char* text,
    cocos2d::SEL_MenuHandler callback,
    std::string const& node_id,
    cocos2d::CCPoint pos,
    float width = 164.f,
    float height = 40.f,
    float spriteScale = 0.55f,
    float labelScale = 0.4f,
    float sizeMult = 1.15f,
    float textOffsetX = 0.f
) {
    return Build<cocos2d::extension::CCScale9Sprite>::create(spriteFrame)
        .contentSize(width, height)
        .scale(spriteScale)
        .intoMenuItem(self, callback)
        .ignoreAnchorPointForPos(false)
        .anchorPoint(0.5f, 0.5f)
        .id(node_id)
        .pos(pos.x, pos.y)
        .scaleMult(sizeMult)
        .layout(Build<AnchorLayout>::create())
        .children(
            Build<cocos2d::CCLabelBMFont>::create(text, "bigFont.fnt")
                .id(fmt::format("{}-label", node_id))
                .anchorPoint(0.5f, 0.5f)
                .scale(labelScale)
                .setAnchorOpts(Anchor::Center, {textOffsetX, 0.f})
        )
        .updateLayout();
}
