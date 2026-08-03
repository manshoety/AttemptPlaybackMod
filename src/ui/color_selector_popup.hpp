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
    constexpr float kPopupBigButtonSizeMult = 1.05f;

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
        this->setID("color-selector-popup"_spr);
        setTitle("");

        setChildrenInvisible(m_mainLayer);

        // Murder the default close button
        if (CCMenuItemSpriteExtra* close = findDefaultCloseButton(m_mainLayer)) {
            close->stopAllActions();
            close->setVisible(false);
            close->setEnabled(false);
        }

        m_mainLayer->setLayout(AnchorLayout::create());

        loadMasks_();

        CCSprite* gradient = createFullscreenGradient_();

        Build<CCMenu>::create()
            .id("ColorSelectorRoot"_spr)
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
                    .id("ExitButton"_spr)
                    .pos(-283.f, 158.f)
                    .scale(0.75f)
                    .scaleMult(0.8),

                createTextButton_(
                    this,
                    "GJ_button_04.png",
                    "Player 1",
                    menu_selector(ColorSelectorPopup::onPlayer1Tab_),
                    "player1-color-tab-base"_spr,
                    {-65.f, 116.f},
                    124.f,
                    34.f,
                    0.55f,
                    0.29f,
                    kPopupButtonSizeMult
                ),

                createTextButton_(
                    this,
                    "GJ_button_01.png",
                    "Player 1",
                    menu_selector(ColorSelectorPopup::onPlayer1Tab_),
                    "player1-color-tab-selected"_spr,
                    {-65.f, 112.f},
                    124.f,
                    34.f,
                    0.55f,
                    0.29f,
                    kPopupButtonSizeMult
                ),

                createTextButton_(
                    this,
                    "GJ_button_04.png",
                    "Player 2",
                    menu_selector(ColorSelectorPopup::onPlayer2Tab_),
                    "player2-color-tab-base"_spr,
                    {65.f, 112.f},
                    124.f,
                    34.f,
                    0.55f,
                    0.29f,
                    kPopupButtonSizeMult
                ),

                createTextButton_(
                    this,
                    "GJ_button_01.png",
                    "Player 2",
                    menu_selector(ColorSelectorPopup::onPlayer2Tab_),
                    "player2-color-tab-selected"_spr,
                    {65.f, 112.f},
                    124.f,
                    34.f,
                    0.55f,
                    0.29f,
                    kPopupButtonSizeMult
                ),

                // I'll reorganize to something like this when I have time, less messy

                createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Enable all",
                    /*callback*/menu_selector(ColorSelectorPopup::onEnableAll_),
                    /*node_id*/"enable-all-button"_spr,
                    /*pos*/{-234.f, -119.f},
                    /*width*/164.f,
                    /*height*/40.f,
                    /*spriteScale*/0.55f,
                    /*labelScale*/0.4f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ),

                createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_06.png",
                    /*text*/"Disable all",
                    /*callback*/menu_selector(ColorSelectorPopup::onDisableAll_),
                    /*node_id*/"disable-all-button"_spr,
                    /*pos*/{-234.f, -144.f},
                    /*width*/164.f,
                    /*height*/40.f,
                    /*spriteScale*/0.55f,
                    /*labelScale*/0.4f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ),


                createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_02.png",
                    /*text*/"Save Preset",
                    /*callback*/menu_selector(ColorSelectorPopup::onSaveColorPreset_),
                    /*node_id*/"save-color-preset-button"_spr,
                    /*pos*/{-140.f, -119.f},
                    /*width*/164.f,
                    /*height*/40.f,
                    /*spriteScale*/0.55f,
                    /*labelScale*/0.4f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ),

                createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_02.png",
                    /*text*/"Load Preset",
                    /*callback*/menu_selector(ColorSelectorPopup::onLoadColorPreset_),
                    /*node_id*/"load-color-preset-button"_spr,
                    /*pos*/{-140.f, -144.f},
                    /*width*/164.f,
                    /*height*/40.f,
                    /*spriteScale*/0.55f,
                    /*labelScale*/0.4f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ),

                Build<CCLabelBMFont>::create("Colors used when Color Mode: Random", "bigFont.fnt")
                    .pos(113.f, -128.f)
                    .anchorPoint(0.5f, 0.5f)
                    .scale(0.325f),

                Build<CCLabelBMFont>::create("Click to enable/disable a color", "bigFont.fnt")
                    .pos(100.f, -142.f)
                    .anchorPoint(0.5f, 0.5f)
                    .scale(0.325f),

                Build<CCNode>::create()
                    .id("ColorButtonsNode"_spr)
                    .pos(-216.f, 33.f)
                    .scale(1.1f)
                    .children(
                        Build<CCNode>::create().id("colorButtonsRow1"_spr).pos(0.f,  48.f),
                        Build<CCNode>::create().id("colorButtonsRow2"_spr).pos(0.f,  24.f),
                        Build<CCNode>::create().id("colorButtonsRow3"_spr).pos(0.f,   0.f),
                        Build<CCNode>::create().id("colorButtonsRow4"_spr).pos(0.f, -36.f),
                        Build<CCNode>::create().id("colorButtonsRow5"_spr).pos(0.f, -60.f),
                        Build<CCNode>::create().id("colorButtonsRow6"_spr).pos(0.f, -84.f),
                        Build<CCNode>::create().id("colorButtonsRow7"_spr).pos(0.f,-108.f),
                        Build<CCNode>::create().id("colorButtonsRow8"_spr).pos(0.f,-120.f)
                    )
            )
            .updateLayout()
            .ignoreAnchorPointForPos(false)
            .anchorPoint(0.5f, 0.5f)
            .parentAtPos(m_mainLayer, Anchor::Center)
            .store(m_uiRoot)
            .collect();

        scaleUIForThatOneTabletUser(kColorPopupW, kColorPopupH);

        buildColorButtons_();
        refreshAllX_();
        refreshPlayerTabs_();

        m_mainLayer->updateLayout();
        return true;
    }

    void onClose(CCObject* sender) override {
        saveMasks_();
        Ghosts::I().m_randomColorMaskLoaded = false;
        Ghosts::I().updateAllColors();
        Popup::onClose(sender);
    }

private:
    CCMenu* m_uiRoot = nullptr;

    std::bitset<kRandomColorSlots> m_allowedP1{};
    std::bitset<kRandomColorSlots> m_allowedP2{};
    bool m_editingPlayer1 = true;
    std::array<CCLabelBMFont*, kRandomColorSlots> m_xMark{};

    void scaleUIForThatOneTabletUser(float designWidth, float designHeight) {
        if (!m_mainLayer) return;

        const auto size = fitPopupToWindow_(designWidth, designHeight);
        const float scale = computeFitScale_(size.width, size.height, designWidth, designHeight);

        m_mainLayer->setScale(scale);
    }

    void onExit_(CCObject*) { this->onClose(nullptr); }

    std::bitset<kRandomColorSlots>& activeMask_() {
        return m_editingPlayer1 ? m_allowedP1 : m_allowedP2;
    }

    std::bitset<kRandomColorSlots> const& activeMask_() const {
        return m_editingPlayer1 ? m_allowedP1 : m_allowedP2;
    }

    void onPlayer1Tab_(CCObject*) {
        m_editingPlayer1 = true;
        refreshAllX_();
        refreshPlayerTabs_();
    }

    void onPlayer2Tab_(CCObject*) {
        m_editingPlayer1 = false;
        refreshAllX_();
        refreshPlayerTabs_();
    }

    void setTabVariantVisible_(const char* nodeId, bool visible) {
        if (!m_uiRoot) return;

        auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(
            m_uiRoot->getChildByIDRecursive(nodeId)
        );
        if (!item) return;

        item->setVisible(visible);
        item->setEnabled(visible);
    }

    void refreshPlayerTabs_() {
        setTabVariantVisible_("player1-color-tab-base"_spr, !m_editingPlayer1);
        setTabVariantVisible_("player1-color-tab-selected"_spr, m_editingPlayer1);
        setTabVariantVisible_("player2-color-tab-base"_spr, m_editingPlayer1);
        setTabVariantVisible_("player2-color-tab-selected"_spr, !m_editingPlayer1);
    }

    void onEnableAll_(CCObject*) {
        activeMask_().set();
        refreshAllX_();
        saveMasks_();
    }

    void onDisableAll_(CCObject*) {
        activeMask_().reset();
        refreshAllX_();
        saveMasks_();
    }

    void onPressColor_(CCObject* sender) {
        auto* item = typeinfo_cast<CCMenuItem*>(sender);
        if (!item) return;

        int slot = item->getTag();
        if (slot < 0 || slot >= kRandomColorSlots) return;

        activeMask_().flip((size_t)slot);
        updateOneX_(slot);
        saveMasks_();
    }

    static bool isValidMaskString_(std::string const& s) {
        if ((int)s.size() != kRandomColorSlots) return false;
        return std::all_of(s.begin(), s.end(), [](char ch) { return ch == '0' || ch == '1'; });
    }

    static std::string readValidSavedMask_(const char* key) {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue(key)) return {};
        auto s = mod->getSavedValue<std::string>(key);
        return isValidMaskString_(s) ? s : std::string{};
    }

    static void maskFromString_(std::bitset<kRandomColorSlots>& mask, std::string const& s) {
        mask.reset();
        for (int slot = 0; slot < kRandomColorSlots; ++slot) {
            if (s[slot] == '1') mask.set((size_t)slot);
        }
    }

    static std::string maskToString_(std::bitset<kRandomColorSlots> const& mask) {
        std::string s(kRandomColorSlots, '0');
        for (int slot = 0; slot < kRandomColorSlots; ++slot) {
            s[slot] = mask.test((size_t)slot) ? '1' : '0';
        }
        return s;
    }

    void loadMasks_() {
        const std::string legacy = readValidSavedMask_(kGhostRandomColorsMaskKey);
        std::string p1 = readValidSavedMask_(kGhostRandomColorsMaskP1Key);
        std::string p2 = readValidSavedMask_(kGhostRandomColorsMaskP2Key);
        const std::string fallback = kDefaultRandomColorMask;

        // if old version with only one palette, duplicate to both players
        if (p1.empty()) p1 = !legacy.empty() ? legacy : (!p2.empty() ? p2 : fallback);
        if (p2.empty()) p2 = !legacy.empty() ? legacy : (!p1.empty() ? p1 : fallback);

        maskFromString_(m_allowedP1, p1);
        maskFromString_(m_allowedP2, p2);
        saveMasks_();
    }

    void saveMasks_() {
        const std::string p1 = maskToString_(m_allowedP1);
        const std::string p2 = maskToString_(m_allowedP2);
        std::string legacy(kRandomColorSlots, '0');

        for (int slot = 0; slot < kRandomColorSlots; ++slot) {
            legacy[slot] = (m_allowedP1.test((size_t)slot) || m_allowedP2.test((size_t)slot)) ? '1' : '0';
        }

        auto* mod = Mod::get();
        mod->setSavedValue(kGhostRandomColorsMaskP1Key, p1);
        mod->setSavedValue(kGhostRandomColorsMaskP2Key, p2);
        mod->setSavedValue(kGhostRandomColorsMaskKey, legacy);
    }

    void refreshAllX_() {
        for (int slot = 0; slot < kRandomColorSlots; ++slot) updateOneX_(slot);
    }

    void updateOneX_(int slot) {
        if (slot < 0 || slot >= kRandomColorSlots) return;
        if (!m_xMark[slot]) return;
        m_xMark[slot]->setVisible(!activeMask_().test((size_t)slot));
    }

    void buildColorButtons_() {
        if (!m_uiRoot) return;

        auto* buttonsNode = m_uiRoot->getChildByIDRecursive("ColorButtonsNode"_spr);
        if (!buttonsNode) return;

        int slot = 0;
        slot = fillRow_(buttonsNode, "colorButtonsRow1"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow2"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow3"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow4"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow5"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow6"_spr, slot, kRowX16, false);
        slot = fillRow_(buttonsNode, "colorButtonsRow7"_spr, slot, kRowX4,  false);
        slot = fillRow_(buttonsNode, "colorButtonsRow8"_spr, slot, kRowX8,  false);

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
        x->setVisible(!activeMask_().test((size_t)slot));

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

    static constexpr const char* kPresetFormatMarker = "APXCOLORSTWO";
    static constexpr const char* kPresetP1Prefix = "PLAYERONE:";
    static constexpr const char* kPresetP2Prefix = "PLAYERTWO:";

    static std::string encodeMaskLetters_(std::bitset<kRandomColorSlots> const& mask) {
        std::string out(kRandomColorSlots, 'A');
        for (int i = 0; i < kRandomColorSlots; ++i) {
            out[i] = mask.test((size_t)i) ? 'B' : 'A';
        }
        return out;
    }

    static bool decodeMaskLetters_(std::string const& encoded, std::bitset<kRandomColorSlots>& out) {
        if ((int)encoded.size() != kRandomColorSlots) return false;
        out.reset();
        for (int i = 0; i < kRandomColorSlots; ++i) {
            const char ch = encoded[i];
            if (ch == 'B' || ch == 'b') out.set((size_t)i);
            else if (ch != 'A' && ch != 'a') return false;
        }
        return true;
    }

    std::string buildPresetData_() const {
        std::bitset<kRandomColorSlots> legacy = m_allowedP1 | m_allowedP2;
        return maskToString_(legacy) + "\n" +
            kPresetFormatMarker + "\n" +
            kPresetP1Prefix + encodeMaskLetters_(m_allowedP1) + "\n" +
            kPresetP2Prefix + encodeMaskLetters_(m_allowedP2) + "\n";
    }

    static void normalizeLegacyMaskString_(std::string& s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char ch) {
            return ch != '0' && ch != '1';
        }), s.end());
    }

    static bool readPrefixedLine_(
        std::string const& data,
        const char* prefix,
        std::string& out
    ) {
        const auto pos = data.find(prefix);
        if (pos == std::string::npos) return false;

        const auto begin = pos + std::char_traits<char>::length(prefix);

        const auto end = data.find_first_of("\r\n", begin);

        out = data.substr(
            begin,
            end == std::string::npos
                ? std::string::npos
                : end - begin
        );

        return true;
    }

    enum class PresetLoadKind {
        Invalid,
        LegacyConverted,
        Current
    };

    PresetLoadKind applyPresetData_(std::string const& data) {
        std::bitset<kRandomColorSlots> p1;
        std::bitset<kRandomColorSlots> p2;

        if (data.find(kPresetFormatMarker) != std::string::npos) {
            std::string p1Letters;
            std::string p2Letters;
            if (!readPrefixedLine_(data, kPresetP1Prefix, p1Letters) ||
                !readPrefixedLine_(data, kPresetP2Prefix, p2Letters) ||
                !decodeMaskLetters_(p1Letters, p1) ||
                !decodeMaskLetters_(p2Letters, p2)) {
                log::warn("[ColorPreset] malformed P1/P2 preset extension");
                return PresetLoadKind::Invalid;
            }
        } else {
            std::string legacy = data;
            normalizeLegacyMaskString_(legacy);
            if (!isValidMaskString_(legacy)) {
                log::warn("[ColorPreset] invalid legacy mask length: got {}, expected {}", legacy.size(), kRandomColorSlots);
                return PresetLoadKind::Invalid;
            }
            maskFromString_(p1, legacy);
            p2 = p1;
        }
 
        m_allowedP1 = p1;
        m_allowedP2 = p2;
        refreshAllX_();
        saveMasks_();
 
        Ghosts::I().m_randomColorMaskLoaded = false;
        Ghosts::I().updateAllColors();

        return data.find(kPresetFormatMarker) != std::string::npos
            ? PresetLoadKind::Current
            : PresetLoadKind::LegacyConverted;
    }

    void onSaveColorPreset_(cocos2d::CCObject*) {
        saveMasks_();

        const std::string data = buildPresetData_();
        const auto dir = presetsDir_();
        const auto suggested = dir / kDefaultPresetName;

        auto opt = presetPickOptions_(suggested, true);

        async::spawn(
            geode::utils::file::pick(
                geode::utils::file::PickMode::SaveFile,
                opt
            ),
            [data](
                geode::Result<std::optional<std::filesystem::path>> result
            ) {
                if (result.isErr()) {
                    result.inspectErr([](std::string const& error) {
                        log::info(
                            "[ColorPreset] Save cancelled / failed: {}",
                            error
                        );
                    });
                    return;
                }

                std::optional<std::filesystem::path> selectedPath;

                result.inspect(
                    [&](std::optional<std::filesystem::path> const& value) {
                        selectedPath = value;
                    }
                );

                if (!selectedPath || selectedPath->empty()) {
                    return;
                }

                std::filesystem::path path = *selectedPath;

                if (!path.has_extension()) {
                    path.replace_extension(kPresetExt);
                }

                std::error_code ec;
                const auto parent = path.parent_path();

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
                    wr.inspectErr([&](std::string const& error) {
                        log::error(
                            "[ColorPreset] Failed to write {}: {}",
                            geode::utils::string::pathToString(path),
                            error
                        );
                    });
                    return;
                }
            }
        );
    }

    void onLoadColorPreset_(cocos2d::CCObject*) {
        const auto dir = presetsDir_();
        auto opt = presetPickOptions_(dir, false);

        auto* self = this;
        self->retain();

        async::spawn(
            geode::utils::file::pick(
                geode::utils::file::PickMode::OpenFile,
                opt
            ),
            [self](
                geode::Result<std::optional<std::filesystem::path>> result
            ) {
                if (result.isErr()) {
                    result.inspectErr([](std::string const& error) {
                        log::info(
                            "[ColorPreset] Load cancelled / failed: {}",
                            error
                        );
                    });

                    self->release();
                    return;
                }

                std::optional<std::filesystem::path> selectedPath;

                result.inspect(
                    [&](std::optional<std::filesystem::path> const& value) {
                        selectedPath = value;
                    }
                );

                if (!selectedPath || selectedPath->empty()) {
                    self->release();
                    return;
                }

                auto rd =
                    geode::utils::file::readString(*selectedPath);

                if (rd.isErr()) {
                    rd.inspectErr([&](std::string const& error) {
                        log::error(
                            "[ColorPreset] Failed to read {}: {}",
                            geode::utils::string::pathToString(
                                *selectedPath
                            ),
                            error
                        );
                    });

                    self->release();
                    return;
                }

                std::string data;

                rd.inspect([&](std::string const& value) {
                    data = value;
                });

                const auto kind =
                    self->applyPresetData_(data);

                if (kind == PresetLoadKind::LegacyConverted) {
                    auto wr =
                        geode::utils::file::writeStringSafe(
                            *selectedPath,
                            self->buildPresetData_()
                        );

                    if (wr.isErr()) {
                        wr.inspectErr(
                            [](std::string const& error) {
                                log::warn(
                                    "[ColorPreset] Loaded old preset "
                                    "but failed to rewrite it as "
                                    "P1/P2 format: {}",
                                    error
                                );
                            }
                        );
                    }
                    else {
                        log::info(
                            "[ColorPreset] Converted old preset "
                            "to P1/P2 format: {}",
                            geode::utils::string::pathToString(
                                *selectedPath
                            )
                        );
                    }
                }

                self->release();
            }
        );
    }
};
