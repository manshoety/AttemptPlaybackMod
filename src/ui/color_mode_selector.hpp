#pragma once

#include "../core/ghost_manager.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace attemptplayback::ui {

struct GhostColorOption {
    ColorMode mode;
    const char* caption;
    const char* savedValue;
};
inline constexpr std::array<GhostColorOption, 4> kGhostColorOptions{{
    {ColorMode::PlayerColors, "Player", "PlayerColors"},
    {ColorMode::Random, "Random", "Random"},
    {ColorMode::White, "White", "White"},
    {ColorMode::Black, "Black", "Black"},
}};

class ColorModeSelector {
public:
    void build(cocos2d::CCMenu* menu, cocos2d::CCObject* owner,
               cocos2d::SEL_MenuHandler callback, cocos2d::CCPoint center,
               float width, float height, float gap) {
        using namespace cocos2d;
        using namespace cocos2d::extension;
        if (!menu) return;
        m_initialized = false;
        constexpr float textureScale = 2.f;
        float fontScale = .32f;
        for (auto const& option : kGhostColorOptions) {
            if (auto* label = CCLabelBMFont::create(option.caption, "bigFont.fnt")) {
                const auto size = label->getContentSize();
                fontScale = std::min({fontScale,
                    (width - 8.f) / std::max(1.f, size.width),
                    (height - 5.f) / std::max(1.f, size.height)});
            }
        }
        for (std::size_t i = 0; i < kGhostColorOptions.size(); ++i) {
            const float x = center.x + (static_cast<float>(i) - 1.5f) * (width + gap);
            for (int active = 0; active <= 1; ++active) {
                auto* bg = CCScale9Sprite::create(active ? "GJ_button_01.png" : "GJ_button_04.png");
                if (!bg) continue;
                bg->setContentSize({width * textureScale, height * textureScale});
                bg->setScale(1.f / textureScale);
                bg->setAnchorPoint({.5f, .5f});
                bg->setPosition({width * .5f, height * .5f});
                auto* label = CCLabelBMFont::create(kGhostColorOptions[i].caption, "bigFont.fnt");
                if (!label) continue;
                label->setScale(fontScale * textureScale);
                label->setPosition({width * textureScale * .5f, height * textureScale * .5f});
                bg->addChild(label);
                auto* visual = CCNode::create();
                visual->setContentSize({width, height});
                visual->addChild(bg);
                auto* item = CCMenuItemSpriteExtra::create(visual, owner, callback);
                if (!item) continue;
                item->setTag(static_cast<int>(i));
                item->setID(std::string("ghost-color-mode-") + kGhostColorOptions[i].caption
                    + (active ? "-selected" : "-base"));
                item->ignoreAnchorPointForPosition(false);
                item->setAnchorPoint({.5f, .5f});
                item->setPosition({x, center.y});
                item->setSizeMult(1.05f);
                menu->addChild(item);
                m_buttons[i][active] = item;
            }
        }
        refresh();
    }

    void refresh() {
        const auto mode = Ghosts::I().colors;
        if (m_initialized && mode == m_lastMode) return;
        m_initialized = true;
        m_lastMode = mode;
        for (std::size_t i = 0; i < kGhostColorOptions.size(); ++i) {
            const bool selected = kGhostColorOptions[i].mode == mode;
            for (int active = 0; active <= 1; ++active) {
                if (auto* button = m_buttons[i][active]) {
                    const bool visible = (active != 0) == selected;
                    button->setVisible(visible);
                    button->setEnabled(visible);
                }
            }
        }
    }

    void select(cocos2d::CCObject* sender) {
        using namespace geode::prelude;
        auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(sender);
        if (!item) return;
        const int index = item->getTag();
        if (index < 0 || static_cast<std::size_t>(index) >= kGhostColorOptions.size()) return;
        auto const& option = kGhostColorOptions[static_cast<std::size_t>(index)];
        auto& ghosts = Ghosts::I();
        if (ghosts.colors != option.mode) {
            geode::Mod::get()->setSavedValue("ghost-colors", std::string(option.savedValue));
            ghosts.setColorMode(option.mode);
        }
        refresh();
    }

private:
    std::array<std::array<CCMenuItemSpriteExtra*, 2>, 4> m_buttons{};
    ColorMode m_lastMode = ColorMode::PlayerColors;
    bool m_initialized = false;
};

} 
