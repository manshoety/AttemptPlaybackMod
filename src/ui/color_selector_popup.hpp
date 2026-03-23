// color_selector_popup.hpp
#pragma once
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <bitset>
#include <fstream>
#include <sstream>
#include <ctime>

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

#include "../core/ghost_manager.hpp"
#include <Geode/ui/BasedButtonSprite.hpp>
#include <UIBuilder.hpp>
#include "../core/random_color_ids.hpp"
#include "../utils/ui_utils.hpp"

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace {
    constexpr float kColorPopupW = 600.f;
    constexpr float kColorPopupH = 340.f;

    constexpr float kColorBtnVisualScale = 0.65f;
    constexpr float kPressSizeMult = 1.f;
    constexpr float kPopupButtonSizeMult = 1.15f;

    static constexpr std::array<float, 16> kRowX16 = {
        0.f, 24.f, 48.f, 72.f,
        108.f, 132.f, 156.f, 180.f,
        216.f, 240.f, 264.f, 288.f,
        324.f, 348.f, 372.f, 396.f
    };

    static constexpr std::array<float, 4> kRowX4 = { 108.f, 132.f, 156.f, 180.f };

    static constexpr std::array<float, 7> kRowX8 = {
        252.f, 276.f, 300.f, 324.f, 348.f, 372.f, 396.f
    };

    inline CCSprite* makeColorBtnSprite_() {
        if (auto* s = CCSprite::createWithSpriteFrameName("GJ_colorBtn_001.png")) return s;
        if (auto* s = CCSprite::create("GJ_colorBtn_001.png")) return s;
        return CCSprite::create();
    }
}

class ColorSelectorPopup : public geode::Popup {
public:
    static ColorSelectorPopup* create() {
        auto ret = new ColorSelectorPopup();
        if (ret && ret->init(kColorPopupW, kColorPopupH)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

protected:
    bool init(float width, float height) {
        if (!Popup::init(width, height))
            return false;
        this->setID("color-selector-popup_spr");
        setTitle("");

        // Murder the default close button
        if (CCMenuItemSpriteExtra* close = findDefaultCloseButton(m_mainLayer)) {
            close->stopAllActions();
            close->setVisible(false);
            close->setEnabled(false);
        }

        m_mainLayer->setLayout(AnchorLayout::create());

        loadMask_();

        CCSprite* gradient = createFullscreenGradient_();

        Build<CCMenu>::create()
            .id("ColorSelectorRoot")
            .contentSize(0.f, 0.f)
            .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
            .children(
                // BG gradient
                Build<CCSprite>(gradient)
                    .color(100, 86, 255)
                    .anchorPoint(0.5f, 0.5f)
                    .zOrder(-1)
                    .scaleX(38.f).scaleY(2.f),

                // title
                Build<CCLabelBMFont>::create("Color Selector", "bigFont.fnt")
                    .pos(0.f, 140.f)
                    .anchorPoint(0.5f, 0.5f),

                // exit button
                Build(CircleButtonSprite::createWithSpriteFrameName(
                    "geode.loader/close.png",
                    0.85f,
                    CircleBaseColor::Green
                ))
                    .intoMenuItem(this, menu_selector(ColorSelectorPopup::onExit_))
                    .ignoreAnchorPointForPos(false)
                    .anchorPoint(0.f, 1.f)
                    .id("ExitButton")
                    .pos(-283.f, 158.f)
                    .scale(0.75f),

                // enable all button
                Build<CCScale9Sprite>::create("GJ_button_01.png")
                    .contentSize(164.f, 40.f)
                    .scale(0.55f)
                    .intoMenuItem(this, menu_selector(ColorSelectorPopup::onEnableAll_))
                    .ignoreAnchorPointForPos(false)
                    .anchorPoint(0.5f, 0.5f)
                    .id("enable-all-button")
                    .pos(-234.f, -119.f)
                    .scaleMult(kPopupButtonSizeMult)
                    .layout(Build<AnchorLayout>::create())
                    .children(
                        Build<CCLabelBMFont>::create("Enable all", "bigFont.fnt")
                            .id("EnableAllLabel")
                            .anchorPoint(0.5f, 0.5f)
                            .scale(0.4f)
                            .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
                    )
                    .updateLayout(),

                // disable all button
                Build<CCScale9Sprite>::create("GJ_button_06.png")
                    .contentSize(164.f, 40.f)
                    .scale(0.55f)
                    .intoMenuItem(this, menu_selector(ColorSelectorPopup::onDisableAll_))
                    .ignoreAnchorPointForPos(false)
                    .anchorPoint(0.5f, 0.5f)
                    .id("disable-all-button")
                    .pos(-234.f, -144.f)
                    .scaleMult(kPopupButtonSizeMult)
                    .layout(Build<AnchorLayout>::create())
                    .children(
                        Build<CCLabelBMFont>::create("Disable all", "bigFont.fnt")
                            .id("DisableAllLabel")
                            .anchorPoint(0.5f, 0.5f)
                            .scale(0.4f)
                            .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
                    )
                    .updateLayout(),

                Build<CCScale9Sprite>::create("GJ_button_02.png")
                    .contentSize(164.f, 40.f)
                    .scale(0.55f)
                    .intoMenuItem(this, menu_selector(ColorSelectorPopup::onSaveColorPreset_))
                    .ignoreAnchorPointForPos(false)
                    .anchorPoint(0.5f, 0.5f)
                    .id("save-color-preset-button")
                    .pos(-140.f, -119.f)
                    .scaleMult(kPopupButtonSizeMult)
                    .layout(Build<AnchorLayout>::create())
                    .children(
                        Build<CCLabelBMFont>::create("Save Preset", "bigFont.fnt")
                            .id("SavePresetLabel")
                            .anchorPoint(0.5f, 0.5f)
                            .scale(0.4f)
                            .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
                    )
                    .updateLayout(),

                Build<CCScale9Sprite>::create("GJ_button_02.png")
                    .contentSize(164.f, 40.f)
                    .scale(0.55f)
                    .intoMenuItem(this, menu_selector(ColorSelectorPopup::onLoadColorPreset_))
                    .ignoreAnchorPointForPos(false)
                    .anchorPoint(0.5f, 0.5f)
                    .id("load-color-preset-button")
                    .pos(-140.f, -144.f)
                    .scaleMult(kPopupButtonSizeMult)
                    .layout(Build<AnchorLayout>::create())
                    .children(
                        Build<CCLabelBMFont>::create("Load Preset", "bigFont.fnt")
                            .id("LoadPresetLabel")
                            .anchorPoint(0.5f, 0.5f)
                            .scale(0.4f)
                            .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
                    )
                    .updateLayout(),

                Build<CCLabelBMFont>::create("Colors used when Color Mode: Random", "bigFont.fnt")
                    .pos(113.f, -116.f)
                    .anchorPoint(0.5f, 0.5f)
                    .scale(0.325f),

                Build<CCLabelBMFont>::create("Click to enable/disable a color", "bigFont.fnt")
                    .pos(100.f, -130.f)
                    .anchorPoint(0.5f, 0.5f)
                    .scale(0.325f),

                Build<CCNode>::create()
                    .id("ColorButtonsNode")
                    .pos(-216.f, 48.f)
                    .scale(1.1f)
                    .children(
                        Build<CCNode>::create().id("colorButtonsRow1").pos(0.f,  48.f),
                        Build<CCNode>::create().id("colorButtonsRow2").pos(0.f,  24.f),
                        Build<CCNode>::create().id("colorButtonsRow3").pos(0.f,   0.f),
                        Build<CCNode>::create().id("colorButtonsRow4").pos(0.f, -36.f),
                        Build<CCNode>::create().id("colorButtonsRow5").pos(0.f, -60.f),
                        Build<CCNode>::create().id("colorButtonsRow6").pos(0.f, -84.f),
                        Build<CCNode>::create().id("colorButtonsRow7").pos(0.f,-108.f),
                        Build<CCNode>::create().id("colorButtonsRow8").pos(0.f,-120.f)
                    )
            )
            .updateLayout()
            .ignoreAnchorPointForPos(false)
            .anchorPoint(0.5f, 0.5f)
            .parentAtPos(m_mainLayer, Anchor::Center)
            .store(m_uiRoot)
            .collect();

        buildColorButtons_();
        refreshAllX_();

        m_mainLayer->updateLayout();
        return true;
    }

    void onClose(CCObject* sender) override {
        saveMask_();
        Ghosts::I().m_randomColorMaskLoaded = false;
        Ghosts::I().updateAllColors();
        Popup::onClose(sender);
    }

private:
    CCMenu* m_uiRoot = nullptr;

    std::bitset<kRandomColorSlots> m_allowed{};
    std::array<CCLabelBMFont*, kRandomColorSlots> m_xMark{};

    void onExit_(CCObject*) { this->onClose(nullptr); }

    void onEnableAll_(CCObject*) {
        m_allowed.set();
        refreshAllX_();
        saveMask_();
    }

    void onDisableAll_(CCObject*) {
        m_allowed.reset();
        refreshAllX_();
        saveMask_();
    }

    void onPressColor_(CCObject* sender) {
        auto* item = typeinfo_cast<CCMenuItem*>(sender);
        if (!item) return;

        int slot = item->getTag();
        if (slot < 0 || slot >= kRandomColorSlots) return;

        m_allowed.flip((size_t)slot);
        updateOneX_(slot);
        saveMask_();
    }

    void loadMask_() {
        std::string s;

        if (Mod::get()->hasSavedValue(kGhostRandomColorsMaskKey)) {
            s = Mod::get()->getSavedValue<std::string>(kGhostRandomColorsMaskKey);
            normalizeMaskString_(s);
        } else {
            s = kDefaultRandomColorMask;
            Mod::get()->setSavedValue(kGhostRandomColorsMaskKey, s);
        }

        if ((int)s.size() != kRandomColorSlots) {
            log::warn("[ColorSelectorPopup] saved mask invalid length (got {}, expected {}), using built-in default", s.size(), kRandomColorSlots);
            s = kDefaultRandomColorMask;
        }

        m_allowed.reset();
        for (int slot = 0; slot < kRandomColorSlots; ++slot) {
            if (s[slot] == '1') m_allowed.set((size_t)slot);
        }
    }

    void saveMask_() {
        std::string s(kRandomColorSlots, '0');
        for (int slot = 0; slot < kRandomColorSlots; ++slot) {
            s[slot] = m_allowed.test((size_t)slot) ? '1' : '0';
        }
        Mod::get()->setSavedValue(kGhostRandomColorsMaskKey, s);
    }

    void refreshAllX_() {
        for (int slot = 0; slot < kRandomColorSlots; ++slot) updateOneX_(slot);
    }

    void updateOneX_(int slot) {
        if (slot < 0 || slot >= kRandomColorSlots) return;
        if (!m_xMark[slot]) return;
        m_xMark[slot]->setVisible(!m_allowed.test((size_t)slot));
    }

    void buildColorButtons_() {
        if (!m_uiRoot) return;

        auto* buttonsNode = m_uiRoot->getChildByIDRecursive("ColorButtonsNode");
        if (!buttonsNode) return;

        int slot = 0;
        slot = fillRow_(buttonsNode, "colorButtonsRow1", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow2", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow3", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow4", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow5", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow6", slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow7", slot, kRowX4,  false);
        slot = fillRow_(buttonsNode, "colorButtonsRow8", slot, kRowX8,  false);

        if (slot != kRandomColorSlots) {
            log::warn("[ColorSelectorPopup] Filled {} slots but expected {}", slot, kRandomColorSlots);
        }
    }

    template <size_t N>
    int fillRow_(CCNode* buttonsNode, const char* rowId, int startSlot, const std::array<float, N>& xs, bool row8Special) {
        auto* row = buttonsNode->getChildByIDRecursive(rowId);
        if (!row) return startSlot;

        auto* menu = CCMenu::create();
        menu->setID(std::string(rowId) + "-menu");
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({0.f, 0.f});
        menu->setPosition({0.f, 0.f});
        row->addChild(menu);

        int slot = startSlot;
        for (size_t i = 0; i < xs.size() && slot < kRandomColorSlots; ++i, ++slot) {
            bool useCircleTiny = row8Special && (i < 3);

            auto* item = makeColorItemForSlot_(slot, useCircleTiny);
            item->setPosition({xs[i], 0.f});
            menu->addChild(item);
        }

        return slot;
    }

    CCMenuItemSpriteExtra* makeColorItemForSlot_(int slot, bool circleTiny) {
        auto* gm = GameManager::sharedState();
        int paletteIdx = kRandomColorIDs[slot];

        ccColor3B c{255, 255, 255};
        if (gm) c = gm->colorForIdx(paletteIdx);

        CCNode* visual = nullptr;
        if (!circleTiny) {
            auto* spr = makeColorBtnSprite_();
            spr->setColor(c);
            spr->setScale(kColorBtnVisualScale);
            visual = spr;
        } else {
            auto* icon = makeColorBtnSprite_();
            icon->setColor(c);
            auto* circle = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::Tiny);
            circle->setScale(kColorBtnVisualScale);
            visual = circle;
        }

        auto* x = CCLabelBMFont::create("X", "bigFont.fnt");
        x->setColor({255, 0, 0});
        x->setPosition({18.f, 19.f});
        x->setAnchorPoint({0.5f, 0.5f});
        x->setZOrder(1);
        x->setScale(0.55f);
        visual->addChild(x);

        auto* item = CCMenuItemSpriteExtra::create(visual, this, menu_selector(ColorSelectorPopup::onPressColor_));
        item->setTag(slot);
        item->setID(fmt::format("color-slot-{}", slot));

        item->setSizeMult(kPressSizeMult);

        m_xMark[slot] = x;
        x->setVisible(!m_allowed.test((size_t)slot));

        return item;
    }


    static constexpr const char* kPresetExt = ".apxcolors";
    static constexpr const char* kDefaultPresetName = "random-colors.apxcolors";

    std::filesystem::path presetsDir_() const {
        auto dir = Mod::get()->getSaveDir() / "color-presets";

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            log::warn(
                "[ColorPreset] create_directories failed: {} ({})",
                geode::utils::string::pathToString(dir),
                ec.message()
            );
        }

        return dir;
    }

    static geode::utils::file::FilePickOptions presetPickOptions_(
        std::filesystem::path const& defaultPath,
        bool includeFilename
    ) {
        geode::utils::file::FilePickOptions opt;

        if (includeFilename) opt.defaultPath = defaultPath;
        else opt.defaultPath = defaultPath;

        geode::utils::file::FilePickOptions::Filter f1;
        f1.description = "AttemptPlayback Color Presets (*.apxcolors)";
        f1.files = { "*.apxcolors" };

        geode::utils::file::FilePickOptions::Filter f2;
        f2.description = "All files (*.*)";
        f2.files = { "*.*" };

        opt.filters = { f1, f2 };
        return opt;
    }

    std::string buildMaskString_() const {
        std::string s(kRandomColorSlots, '0');
        for (int i = 0; i < kRandomColorSlots; ++i) {
            s[i] = m_allowed.test((size_t)i) ? '1' : '0';
        }
        return s;
    }

    static void normalizeMaskString_(std::string& s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char ch) {
            return ch != '0' && ch != '1';
        }), s.end());
    }

    bool applyMaskString_(std::string s) {
        normalizeMaskString_(s);

        if ((int)s.size() != kRandomColorSlots) {
            log::warn("[ColorPreset] invalid mask length: got {}, expected {}", s.size(), kRandomColorSlots);
            return false;
        }

        m_allowed.reset();
        for (int i = 0; i < kRandomColorSlots; ++i) {
            if (s[i] == '1') m_allowed.set((size_t)i);
        }

        refreshAllX_();
        saveMask_();

        Ghosts::I().m_randomColorMaskLoaded = false;
        Ghosts::I().updateAllColors();

        return true;
    }

    void onSaveColorPreset_(cocos2d::CCObject*) {
        saveMask_();

        const std::string data = buildMaskString_();
        const auto dir = presetsDir_();
        const auto suggested = dir / kDefaultPresetName;

        auto opt = presetPickOptions_(suggested, /*includeFilename=*/true);

        async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::SaveFile, opt),
            [this, data](geode::Result<std::optional<std::filesystem::path>> result) {

                if (result.isErr()) {
                    log::info("[ColorPreset] Save cancelled / failed: {}", result.unwrapErr());
                    return;
                }

                auto optPath = result.unwrap();

                if (!optPath)
                    return;

                std::filesystem::path path = optPath.value();

                if (path.empty())
                    return;

                if (!path.has_extension()) {
                    path.replace_extension(kPresetExt);
                }

                std::error_code ec;
                auto parent = path.parent_path();
                if (!parent.empty()) {
                    std::filesystem::create_directories(parent, ec);

                    if (ec) {
                        log::warn(
                            "[ColorPreset] create_directories failed: {} ({})",
                            geode::utils::string::pathToString(parent),
                            ec.message()
                        );
                    }
                }

                auto wr = geode::utils::file::writeStringSafe(path, data);

                if (wr.isErr()) {
                    log::error("[ColorPreset] Failed to write {}: {}", geode::utils::string::pathToString(path), wr.unwrapErr());
                    return;
                }

                //log::info("[ColorPreset] Saved preset to {}", geode::utils::string::pathToString(path));
            }
        );
    }

    void onLoadColorPreset_(cocos2d::CCObject*) {
        const auto dir = presetsDir_();
        auto opt = presetPickOptions_(dir, false);

        auto* self = this;
        self->retain();

        async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, opt),
            [self](geode::Result<std::optional<std::filesystem::path>> result) {

                if (result.isErr()) {
                    log::info("[ColorPreset] Load cancelled / failed: {}", result.unwrapErr());
                    self->release();
                    return;
                }

                auto optPath = result.unwrap();
                if (!optPath || optPath->empty()) {
                    self->release();
                    return;
                }

                auto rd = geode::utils::file::readString(*optPath);
                if (rd.isErr()) {
                    log::error("[ColorPreset] Failed to read {}: {}",
                        geode::utils::string::pathToString(*optPath), rd.unwrapErr());
                    self->release();
                    return;
                }

                std::string s = rd.unwrap();
                self->applyMaskString_(std::move(s));
                self->release();
            }
        );
    }
};
