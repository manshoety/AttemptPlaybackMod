#pragma once

#include "attempt_manager_popup.hpp"

#include <Geode/Geode.hpp>

#include <UIBuilder.hpp>
#include "../utils/ui_utils.hpp"
#include "color_selector_popup.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <limits>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace {
    int currentPopupTouchPriority_() {
        auto* director = CCDirector::sharedDirector();
        auto* dispatcher = director ? director->getTouchDispatcher() : nullptr;
        return dispatcher ? dispatcher->getTargetPrio() : -500;
    }

    constexpr float kPopupW = 700.f;
    constexpr float kPopupH = 380.f;

    constexpr int kRowsPerPage = 20;

    // Main panel layout
    constexpr float kLeftPanelX = -70.f;
    constexpr float kRightPanelX = 244.f;

    constexpr float kLeftPanelW = 435.f;
    constexpr float kLeftPanelH = 238.f;
    constexpr float kRightPanelW = 185.f;
    constexpr float kRightPanelH = 184.f;
    constexpr float kPanelY = -18.f;
    constexpr float kRightPanelY = 9.f;

    // Scroll viewport: centered in the left panel
    constexpr float kRowsViewW = kLeftPanelW - 18.f;
    constexpr float kRowsViewH = 176.f;
    constexpr float kRowsViewX = -75.f; 
    constexpr float kRowsViewY = -32.f;

    // Normal row size inside the scroll page
    constexpr float kRowH = 22.f;
    constexpr float kRowBgH = 60.f;
    constexpr float kRowBgScaleY = 0.325f;

    constexpr float kRowsContentPadTop = 11.f;
    constexpr float kRowsContentPadBottom = 8.f;

    constexpr float kRowsCheckboxX = 13.f;
    constexpr float kRowsStartPercentX = 42.f;
    constexpr float kRowsEndPercentX = 128.f;
    constexpr float kRowsDurationX = 232.f;
    constexpr float kRowsDeleteX = 397.f;
    constexpr float kRowsInfoX = 365.f;
    constexpr float kListRowHitX = 170.f;
    constexpr float kListRowHitW = 300.f;
    constexpr float kRowsLabelScale = 0.22f;

    constexpr float kTopControlsY = 90.f;
    constexpr float kDeselectBtnX = -76.f;
    constexpr float kRefreshBtnX = 136.f;
    constexpr float kInfoBtnX = kRefreshBtnX + 28.f;

    CCLabelBMFont* makeLabel_(std::string const& text, float x, float y, float scale, CCNode* parent,
        ccColor3B color = {255, 255, 255}, CCPoint anchor = {0.5f, 0.5f}) {
        auto* label = CCLabelBMFont::create(text.empty() ? " " : text.c_str(), "bigFont.fnt");
        label->setAnchorPoint(anchor);
        label->setScale(scale);
        label->setColor(color);
        label->setPosition({x, y});
        parent->addChild(label);
        return label;
    }

    CCScale9Sprite* makePanel_(float x, float y, float w, float h, CCNode* parent, int opacity = 78) {
        auto* panel = CCScale9Sprite::create("square02b_001.png");
        if (!panel) return nullptr;
        panel->setContentSize({w, h});
        panel->setPosition({x, y});
        panel->setColor({0, 0, 0});
        panel->setOpacity(opacity);
        parent->addChild(panel);
        return panel;
    }

    CCSprite* makeBlueGameBackdrop_() {
        auto* bg = createFullscreenGradient_();
        if (!bg) return nullptr;
        auto const win = CCDirector::sharedDirector()->getWinSize();
        const float uiScale = std::min({
            1.f,
            std::max(0.01f, win.width - 22.f) / kPopupW,
            std::max(0.01f, win.height - 18.f) / kPopupH
        });
        if (uiScale > 0.f) {
            bg->setScaleX((win.width + 30.f) / (std::max(1.f, bg->getContentSize().width) * uiScale));
            bg->setScaleY((win.height + 30.f) / (std::max(1.f, bg->getContentSize().height) * uiScale));
        }
        bg->setColor({40, 84, 220});
        bg->setPosition({0.f, 0.f});
        if (auto* frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("game_bg_01_001.png")) {
            auto* texture = CCSprite::createWithSpriteFrame(frame);
            const auto size = texture->getContentSize();
            if (size.width > 0.f && size.height > 0.f) {
                texture->setScaleX(bg->getContentSize().width / size.width);
                texture->setScaleY(bg->getContentSize().height / size.height);
                texture->setPosition(bg->getContentSize() * .5f);
                texture->setColor({75, 150, 255});
                texture->setOpacity(60);
                bg->addChild(texture);
            }
        }
        return bg;
    }

    CCScale9Sprite* makeSmallTextButtonSprite_(
        std::string const& text,
        char const* texture,
        float w,
        float h,
        float scale = 1.f,
        CCLabelBMFont** outLabel = nullptr
    ) {
        auto* bg = CCScale9Sprite::create(texture);
        bg->setContentSize({ w, h });
        bg->setAnchorPoint({ 0.5f, 0.5f });
        bg->setScale(scale);

        auto* label = CCLabelBMFont::create(text.empty() ? " " : text.c_str(), "bigFont.fnt");
        if (outLabel) *outLabel = label;
        label->setAnchorPoint({ 0.5f, 0.5f });
        label->setPosition({ w * 0.5f, h * 0.5f });
        label->setScale(std::min(0.23f, std::max(0.20f, h / 88.f)));
        bg->addChild(label);

        return bg;
    }

    CCMenuItemSpriteExtra* makeSmallTextButton_(CCObject* target, SEL_MenuHandler selector,
        std::string const& text, char const* texture, float x, float y, float w, float h,
        char const* id, float scale = 1.f, CCLabelBMFont** outLabel = nullptr) {
        
        auto* bg = makeSmallTextButtonSprite_(
            text,
            texture,
            w,
            h,
            scale,
            outLabel
        );

        auto* item = CCMenuItemSpriteExtra::create(bg, target, selector);
        item->setPosition({ x, y });
        item->setID(id);
        item->setSizeMult(0.75f);
        return item;
    }

    CCMenuItemSpriteExtra* makeTabButton_(CCObject* target, SEL_MenuHandler selector,
        std::string const& text, int tab, float x, float y, float w, float h, char const* id) {
        auto* bg = CCScale9Sprite::create("GJ_button_01.png");
        bg->setContentSize({w, h});
        bg->setAnchorPoint({0.5f, 0.5f});
        auto* label = CCLabelBMFont::create(text.empty() ? " " : text.c_str(), "bigFont.fnt");
        label->setAnchorPoint({0.5f, 0.5f});
        label->setPosition({w * 0.5f, h * 0.52f});
        label->setScale(0.40f);
        bg->addChild(label);

        auto* item = CCMenuItemSpriteExtra::create(bg, target, selector);
        item->setTag(tab);
        item->setID(id);
        item->setPosition({x, y});
        item->setSizeMult(kPopupButtonSizeMult);
        return item;
    }

    CCMenuItemToggler* makeCheckbox_(CCObject* target, SEL_MenuHandler selector,
        int serial, float x, float y, bool selected, char const* id) {
        auto* toggler = CCMenuItemToggler::createWithStandardSprites(target, selector, 0.45f);
        toggler->setPosition({ x, y });
        toggler->setTag(serial);
        toggler->setID(id);

        // Keep the visual checkbox size, shrink interactive area so clicks don't hit other stuff
        toggler->setAnchorPoint({ 0.5f, 0.5f });
        toggler->setContentSize({ 18.f, 18.f });
        toggler->setSizeMult(0.38f);
        toggler->toggle(selected);
        return toggler;
    }

    CCMenuItemSpriteExtra* makeTrashButton_(CCObject* target, SEL_MenuHandler selector,
        int serial, float x, float y, char const* id) {
        auto* icon = CCSprite::createWithSpriteFrameName("edit_delBtn_001.png");
        auto* spr = CircleButtonSprite::create(icon, CircleBaseColor::Red, CircleBaseSize::Small);
        spr->setScale(0.42f);

        auto* item = CCMenuItemSpriteExtra::create(spr, target, selector);
        item->setPosition({x, y});
        item->setTag(serial);
        item->setID(id);
        item->setSizeMult(0.48f);
        return item;
    }

    CCMenuItemSpriteExtra* makeInfoButton_(CCObject* target, SEL_MenuHandler selector,
        int tag, float x, float y, char const* id) {
        auto* icon = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        icon->setScale(0.58f);

        auto* item = CCMenuItemSpriteExtra::create(icon, target, selector);
        item->setPosition({x, y});
        item->setTag(tag);
        item->setID(id);
        item->setSizeMult(0.65f);
        return item;
    }

    CCMenuItemSpriteExtra* makeSpriteButton_(
        CCObject* target,
        SEL_MenuHandler selector,
        char const* spriteFrame,
        float x,
        float y,
        char const* id,
        float scale = 1.f,
        float sizeMult = 0.55f
    ) {
        CCNode* normal = nullptr;

        if (auto* spr = CCSprite::createWithSpriteFrameName(spriteFrame)) {
            spr->setScale(scale);
            normal = spr;
        }
        else if (auto* spr = CCSprite::create(spriteFrame)) {
            spr->setScale(scale);
            normal = spr;
        }
        else {
            auto* fallback = CCScale9Sprite::create("GJ_button_01.png");
            fallback->setContentSize({20.f, 20.f});
            fallback->setScale(scale);
            normal = fallback;
        }

        auto* item = CCMenuItemSpriteExtra::create(normal, target, selector);
        item->setPosition({x, y});
        item->setID(id);
        item->setSizeMult(sizeMult);
        return item;
    }

    CCMenuItemSpriteExtra* makeHitButton_(CCObject* target, SEL_MenuHandler selector,
        int tag, float x, float y, float w, float h, char const* id) {
        auto* bg = CCScale9Sprite::create("square02b_001.png");
        bg->setContentSize({w, h});
        bg->setOpacity(0);

        auto* item = CCMenuItemSpriteExtra::create(bg, target, selector);
        item->setPosition({x, y});
        item->setTag(tag);
        item->setID(id);
        item->setSizeMult(0.65f);
        return item;
    }

    void parkMenuItem_(CCMenuItem* item) {
        if (!item) return;

        item->setVisible(false);
        item->setEnabled(false);
        item->setTag(-1);
        item->setPosition({ -10000.f, -10000.f });
    }

    void showMenuItem_(CCMenuItem* item, float x, float y, int tag) {
        if (!item) return;

        item->setPosition({ x, y });
        item->setTag(tag);
        item->setVisible(true);
        item->setEnabled(true);
    }

    geode::utils::file::FilePickOptions attemptFilePickOptions_(std::filesystem::path const& defaultPath) {
        geode::utils::file::FilePickOptions opt;
        opt.defaultPath = defaultPath;

        geode::utils::file::FilePickOptions::Filter apx;
        apx.description = "AttemptPlayback Files (*.apx)";
        apx.files = { "*.apx" };

        geode::utils::file::FilePickOptions::Filter all;
        all.description = "All files (*.*)";
        all.files = { "*.*" };

        opt.filters = { apx, all };
        return opt;
    }

    std::filesystem::path ensureApxExtension_(std::filesystem::path path) {
        std::string ext = geode::utils::string::pathToString(path.extension());
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext != ".apx") path.replace_extension(".apx");
        return path;
    }

    std::string importFileName_(std::filesystem::path const& path) {
        const auto filename = geode::utils::string::pathToString(path.filename());
        return filename.empty() ? std::string("selected APX file") : filename;
    }
}


static void safeSetString_(CCLabelBMFont* label, std::string const& text) {
    if (!label) return;

    const std::string safeText = text.empty() ? " " : text;
    const char* current = label->getString();

    if (current && safeText == current) return;

    label->setString(safeText.c_str());
}

static void setPageVariantVisible_(
    CCMenuItemSpriteExtra* activeBtn,
    CCMenuItemSpriteExtra* endBtn,
    bool useEndTexture,
    bool canUse
) {
    if (activeBtn) {
        activeBtn->setVisible(!useEndTexture);
        activeBtn->setEnabled(canUse && !useEndTexture);
    }

    if (endBtn) {
        endBtn->setVisible(useEndTexture);
        endBtn->setEnabled(canUse && useEndTexture);
    }
}
