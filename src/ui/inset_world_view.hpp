#pragma once

#include <Geode/Geode.hpp>
#include <algorithm>

namespace attemptplayback::ui {

struct InsetWorldView {
    cocos2d::CCNode* root = nullptr;
    cocos2d::CCNode* world = nullptr;
    cocos2d::CCNode* actors = nullptr;
    cocos2d::CCNode* backupGround = nullptr;
};

inline InsetWorldView createInsetWorldView(cocos2d::CCSize size) {
    using namespace cocos2d;

    auto* root = CCNode::create();
    root->setContentSize(size);
    root->setAnchorPoint({0.f, 0.f});
    root->ignoreAnchorPointForPosition(false);

    // Fixed background
    auto* background = CCLayerColor::create(ccc4(28, 82, 185, 255), size.width, size.height);
    background->setAnchorPoint({0.f, 0.f});
    background->ignoreAnchorPointForPosition(false);
    root->addChild(background, -4);

    auto* backupGround = CCNode::create();
    backupGround->setContentSize(size);
    backupGround->setAnchorPoint({0.f, 0.f});
    backupGround->ignoreAnchorPointForPosition(false);
    root->addChild(backupGround, -3);

    auto* stencil = CCLayerColor::create(ccc4(255, 255, 255, 255), size.width, size.height);
    stencil->setAnchorPoint({0.f, 0.f});
    stencil->ignoreAnchorPointForPosition(false);

    auto* clip = CCClippingNode::create(stencil);

    clip->setAlphaThreshold(1.f);
    clip->setContentSize(size);
    clip->setAnchorPoint({0.f, 0.f});
    clip->ignoreAnchorPointForPosition(false);
    root->addChild(clip, 0);

    auto* world = CCNode::create();
    world->setContentSize(size);
    world->setAnchorPoint({0.f, 0.f});
    world->ignoreAnchorPointForPosition(false);
    clip->addChild(world, 1);

    auto* actors = CCNode::create();
    actors->setContentSize(size);
    actors->setAnchorPoint({0.f, 0.f});
    actors->ignoreAnchorPointForPosition(false);
    root->addChild(actors, 2);

    return {root, world, actors, backupGround};
}

inline void positionInsetWorld(cocos2d::CCNode* world,
                              cocos2d::CCPoint worldFocus,
                              cocos2d::CCPoint screenAnchor, float zoom) {
    if (!world) return;
    world->setScale(zoom);
    world->setPosition({screenAnchor.x - worldFocus.x * zoom,
                        screenAnchor.y - worldFocus.y * zoom});
}


inline void positionGhostsInInset(cocos2d::CCNode* world,
                                 cocos2d::CCNode* middle,
                                 cocos2d::CCNode* behind,
                                 cocos2d::CCNode* ahead,
                                 cocos2d::CCSize viewport,
                                 float screenPlayerX, float distancePx,
                                 float maxZoom = 1.f) {
    if (!world || !middle) return;
    constexpr float worldPlayerX = 450.f;
    constexpr float groundTop = 15.f;
    constexpr float playerY = 30.f;
    constexpr float padding = 6.f;
    const float left = std::min(std::max(distancePx, 0.f), 100.f);
    const float right = std::min(std::max(distancePx, 0.f), 174.f);

    middle->setPosition({worldPlayerX, playerY});
    if (behind) behind->setPosition({worldPlayerX - left, playerY});
    if (ahead) ahead->setPosition({worldPlayerX + right, playerY});

    const float halfIcon = std::max(12.f, middle->getContentSize().width * .5f);
    const float leftAvailable = std::max(1.f, screenPlayerX - padding);
    const float rightAvailable = std::max(1.f, viewport.width - screenPlayerX - padding);
    const float zoom = std::min({maxZoom,
        leftAvailable / std::max(1.f, left + halfIcon),
        rightAvailable / std::max(1.f, right + halfIcon)});
    positionInsetWorld(world, {worldPlayerX, groundTop}, {screenPlayerX, groundTop},
                       std::max(0.01f, zoom));
}

}
