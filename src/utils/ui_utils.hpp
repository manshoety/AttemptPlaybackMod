#pragma once
#include <Geode/DefaultInclude.hpp>
#include <Geode/cocos/base_nodes/CCNode.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

cocos2d::CCPoint screenToMenu(cocos2d::CCMenu* menu, cocos2d::CCPoint screen);
cocos2d::CCMenu* findLargestMenu(cocos2d::CCNode* root);
CCMenuItemSpriteExtra* makeButton(const char* text, cocos2d::CCObject* target,
                                      cocos2d::SEL_MenuHandler cb, float scale = 1.0f);
cocos2d::CCLabelBMFont* findLabelRecursive(cocos2d::CCNode* n);
CCMenuItemSpriteExtra* findDefaultCloseButton(cocos2d::CCNode* root);

