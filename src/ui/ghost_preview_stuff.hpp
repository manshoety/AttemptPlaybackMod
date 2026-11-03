#pragma once

#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>

namespace attemptplayback::ui {
    inline cocos2d::CCSprite* gameSprite(const char* const* names, size_t count) {
        auto* cache = cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache();
        for (size_t i = 0; i < count; ++i) {
            if (auto* frame = cache->spriteFrameByName(names[i])) {
                return cocos2d::CCSprite::createWithSpriteFrame(frame);
            }
        }
        return nullptr;
    }

    inline void fitSprite(cocos2d::CCSprite* sprite, float width, float height) {
        if (!sprite) return;
        auto size = sprite->getContentSize();
        if (size.width < 1.f || size.height < 1.f) return;
        sprite->setScale(std::min(width / size.width, height / size.height));
    }

    inline cocos2d::CCSprite* gdTextureSprite(const char* name) {
        return name ? cocos2d::CCSprite::create(name) : nullptr;
    }

    inline void addGDPreviewBackground(cocos2d::CCNode* scene, float width, float height) {
        if (!scene) return;
        auto* gm = GameManager::sharedState();
        auto* bg = gm ? cocos2d::CCSprite::create(gm->getBGTexture(1)) : nullptr;
        if (!bg) bg = gdTextureSprite("game_bg_01_001.png");
        if (!bg) return;
        const auto size = bg->getContentSize();
        if (size.width <= 0.f || size.height <= 0.f) return;
        bg->setAnchorPoint({0.f, 0.f});
        bg->setPosition({0.f, 0.f});
        // BG fit
        bg->setScaleX(width / size.width);
        bg->setScaleY(height / size.height);
        bg->setColor({40, 126, 255});
        scene->addChild(bg, -4);
    }

    inline void addGDPreviewGround(cocos2d::CCNode* scene, float width) {
        if (!scene) return;
        constexpr float groundTop = 15.f; // Matches positionGhostsInInset
        auto* probe = gdTextureSprite("groundSquare_01_001.png");
        if (probe) {
            const auto size = probe->getContentSize();
            if (size.width > 0.f && size.height > 0.f) {
                constexpr float visibleGroundHeight = 14.f;
                const float scaleY = visibleGroundHeight / size.height;
                const float tileWidth = std::max(24.f, size.width * scaleY);
                const float scaleX = tileWidth / size.width;
                const int count = std::min(128, static_cast<int>(std::ceil(width / tileWidth)) + 1);
                for (int i = 0; i < count; ++i) {
                    auto* tile = i == 0 ? probe : gdTextureSprite("groundSquare_01_001.png");
                    if (!tile) break;
                    tile->setAnchorPoint({0.f, 0.f});
                    tile->setPosition({i * tileWidth, 0.f});
                    tile->setScaleX(scaleX);
                    tile->setScaleY(scaleY);
                    tile->setColor({80, 152, 240});
                    scene->addChild(tile, 1);
                }
            }
        }
        // Thin ground line
        auto* edge = cocos2d::CCLayerColor::create(
            cocos2d::ccc4(165, 215, 255, 235), width, 1.f);
        if (edge) {
            edge->setPosition({0.f, groundTop - 1.f});
            scene->addChild(edge, 2);
        }
    }

    inline void addGhostScene(cocos2d::CCNode* scene, float width, float height, bool drawBackdrop = true) {
        if (!scene) return;
        if (drawBackdrop) addGDPreviewBackground(scene, width, height);
        addGDPreviewGround(scene, width);
    }

    inline cocos2d::CCSprite* addRealBlock(cocos2d::CCNode* scene, float x, float y, float size) {
        const char* names[] = {"block_01_001.png", "square_01_001.png", "groundSquare_01_001.png"};
        auto* sprite = gameSprite(names, 3);
        if (!sprite) return nullptr;
        fitSprite(sprite, size, size);
        sprite->setPosition({x, y});
        scene->addChild(sprite, 2);
        return sprite;
    }

    inline cocos2d::CCSprite* addRealSpike(cocos2d::CCNode* scene, float x, float y, float size) {
        const char* names[] = {"spike_01_001.png", "gdh_spike_01_001.png", "spike_01_g_001.png"};
        auto* sprite = gameSprite(names, 3);
        if (!sprite) return nullptr;
        fitSprite(sprite, size, size);
        sprite->setPosition({x, y});
        scene->addChild(sprite, 3);
        return sprite;
    }

    inline void placeSlider(Slider* slider, cocos2d::CCNode* parent, float x, float y, float scale) {
        if (!slider || !parent) return;
        slider->setAnchorPoint({0.f, 0.f});
        slider->setScale(scale);
        slider->setPosition({x, y});
        parent->addChild(slider, 9);
    }
}
