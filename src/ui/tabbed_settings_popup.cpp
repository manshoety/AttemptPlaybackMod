#include "tabbed_settings_popup.hpp"
#include "animated_switch.hpp"
#include "ghost_preview_stuff.hpp"
#include "inset_world_view.hpp"
#include "ghost_settings_popup.hpp"
#include "color_selector_popup.hpp"
#include "playback_mod_menu_popup.hpp"
#include "../core/random_color_ids.hpp"
#include "../core/ghost_manager.hpp"
#include "../utils/ui_utils.hpp"

#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cerrno>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace {
    constexpr float kSettingsW = 600.f;
    constexpr float kSettingsH = 340.f;
    constexpr float kSettingsViewW = 556.f;
    constexpr float kSettingsViewH = 264.f;
    constexpr float kRowLeft = -235.f;
    constexpr float kInputRight = 175.f;

    void fitSettings(CCNode* node) {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        const float s = std::min({1.f, (win.width - 18.f) / kSettingsW, (win.height - 14.f) / kSettingsH});
        node->setScale(std::max(0.1f, s));
    }

    CCLabelBMFont* label(CCNode* parent, const char* text, float x, float y, float scale = .40f, bool left = true) {
        auto* l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(scale);
        l->setAnchorPoint(left ? CCPoint{0.f, .5f} : CCPoint{.5f, .5f});
        l->setPosition({x, y});
        parent->addChild(l, 2);
        return l;
    }

    CCLabelBMFont* valueLabel(CCNode* parent, const char* text, float x, float y) {
        auto* l = CCLabelBMFont::create(text, "goldFont.fnt");
        l->setScale(.33f);
        l->setAnchorPoint({0.f, .5f});
        l->setPosition({x, y});
        parent->addChild(l, 2);
        return l;
    }

    void panel(CCNode* parent, float x, float y, float width, float height) {
        auto* p = CCScale9Sprite::create("square02b_001.png");
        if (!p) return;
        p->setContentSize({width, height});
        p->setColor({60, 32, 22});
        p->setOpacity(223);
        p->setPosition({x, y});
        parent->addChild(p, 0);
    }

    void rowBand(CCNode* parent, float x, float y, float width, float = 29.f) {
        auto* bg = CCScale9Sprite::create("square02b_001.png");
        if (!bg) return;
        bg->setContentSize({width, 55.f});
        bg->setScaleY(0.5f);
        bg->setColor({106, 59, 36});
        bg->setOpacity(116);
        bg->setPosition({x, y});
        parent->addChild(bg, 1);
    }

    CCMenuItemSpriteExtra* button(CCObject* owner, SEL_MenuHandler callback, const char* text,
            float x, float y, float width, float height, const char* texture = "GJ_button_02.png") {
        auto* bg = CCScale9Sprite::create(texture);
        bg->setContentSize({width, height});
        auto* l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(std::min(.43f, (width - 12.f) / std::max(1.f, l->getContentSize().width)));
        l->setPosition({width / 2.f, height / 2.f});
        bg->addChild(l);
        auto* b = CCMenuItemSpriteExtra::create(bg, owner, callback);
        b->setPosition({x, y});
        b->setSizeMult(1.05f);
        return b;
    }

    void tabs(CCMenu* menu, CCObject* owner, SEL_MenuHandler callback, int selected, const char* const* names, int count) {
        const float w = 550.f / count;
        for (int i = 0; i < count; ++i) {
            auto* b = button(owner, callback, names[i], -275.f + w * (i + .5f), 113.f, w - .5f, 29.f,
                i == selected ? "GJ_button_01.png" : "GJ_button_04.png");
            b->setTag(i);
            if (i != selected) {
                if (auto* surface = typeinfo_cast<CCScale9Sprite*>(b->getNormalImage())) {
                    surface->setColor({163, 117, 86});
                }
            }
            menu->addChild(b);
        }
    }

    void toggle(CCNode* page, CCMenu* menu, CCObject* owner, SEL_MenuHandler callback,
        const char* text, float x, float y, bool current, int tag, float width = 230.f) {
        const bool narrow = x > 0.f;
        rowBand(page, narrow ? 177.f : 0.f, y, narrow ? 188.f : 532.f);
        label(page, text, x, y, .34f);
        auto* b = attemptplayback::ui::AnimatedSwitch::create(owner, callback);
        b->setTag(tag);
        b->setPosition({x + width, y});
        b->toggle(current);
        menu->addChild(b);
    }

    std::string colorMask(int player) {
        auto* mod = Mod::get();
        const auto* key = player == 0 ? kGhostRandomColorsMaskP1Key : kGhostRandomColorsMaskP2Key;
        std::string s = mod->getSavedValue<std::string>(key);
        if (s.size() != kRandomColorIDs.size()) s = kDefaultRandomColorMask;
        return s;
    }

    void saveMask(int player, const std::string& mask) {
        Mod::get()->setSavedValue(player == 0 ? kGhostRandomColorsMaskP1Key : kGhostRandomColorsMaskP2Key, mask);
        std::string a = player == 0 ? mask : colorMask(0);
        std::string b = player == 1 ? mask : colorMask(1);
        std::string legacy(a.size(), '0');
        for (size_t i = 0; i < a.size(); ++i) legacy[i] = (a[i] == '1' || b[i] == '1') ? '1' : '0';
        Mod::get()->setSavedValue(kGhostRandomColorsMaskKey, legacy);
        Ghosts::I().m_randomColorMaskLoaded = false;
        Ghosts::I().updateAllColors();
    }

    void showNative(geode::Popup* popup) {
        if (!popup) return;
        popup->m_noElasticity = true;
        popup->show();
    }
}

TabbedGhostSettingsPopup* TabbedGhostSettingsPopup::create() {
    auto* p = new TabbedGhostSettingsPopup();
    if (p && p->init(kSettingsW, kSettingsH)) {
        p->autorelease();
        return p;
    }
    CC_SAFE_DELETE(p);
    return nullptr;
}

bool TabbedGhostSettingsPopup::init(float w, float h) {
    if (!Popup::init(w, h)) return false;
    setID("tabbed-ghost-settings-popup"_spr);
    setTitle("Ghost Appearance");
    auto* tabBar = CCScale9Sprite::create("square02b_001.png");
    if (tabBar) {
        tabBar->setContentSize({554.f, 34.f});
        tabBar->setPosition({w * .5f, h * .5f + 113.f});
        tabBar->setColor({80, 39, 19});
        m_mainLayer->addChild(tabBar, 9);
    }
    m_tabs = CCMenu::create();
    m_tabs->setPosition({w * .5f, h * .5f});
    m_mainLayer->addChild(m_tabs, 10);
    m_scroll = attemptplayback::ui::AttemptScrollView::create({kSettingsViewW, kSettingsViewH});
    if (!m_scroll) return false;
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->setPosition({w * .5f - kSettingsViewW * .5f, h * .5f - 166.f});
    if (auto* dispatcher = CCDirector::sharedDirector()->getTouchDispatcher())
        m_scroll->setTouchPriority(dispatcher->getTargetPrio());
    m_mainLayer->addChild(m_scroll, 3);
    buildTabs();
    buildPage();
    fitSettings(m_mainLayer);
    scheduleUpdate();
    return true;
}

void TabbedGhostSettingsPopup::buildTabs() {
    m_tabs->removeAllChildrenWithCleanup(true);
    const char* names[] = {"Appearance", "Colors", "Ghost Text", "Death Markers", "Effects"};
    tabs(m_tabs, this, menu_selector(TabbedGhostSettingsPopup::onTab), m_tab, names, 5);
}

void TabbedGhostSettingsPopup::onTab(CCObject* sender) {
    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node || node->getTag() == m_tab) return;
    commitText();
    m_tab = node->getTag();
    m_tabs->setEnabled(false);
    scheduleOnce(schedule_selector(TabbedGhostSettingsPopup::applyQueuedTab), 0.f);
}

void TabbedGhostSettingsPopup::applyQueuedTab(float) {
    buildTabs();
    buildPage();
    m_tabs->setEnabled(true);
}

void TabbedGhostSettingsPopup::applyQueuedPage(float) {
    buildPage();
}

void TabbedGhostSettingsPopup::buildPage() {
    m_opacity = nullptr;
    m_distance = nullptr;
    m_distanceLabel = nullptr;
    m_opacityLabel = nullptr;
    m_textSize = nullptr;
    m_textOpacity = nullptr;
    m_markerSize = nullptr;
    m_markerThickness = nullptr;
    m_markerOpacity = nullptr;
    m_textInput = nullptr;
    m_markerDraw = nullptr;
    m_textSizeLabel = nullptr;
    m_textOpacityLabel = nullptr;
    m_markerSizeLabel = nullptr;
    m_markerThicknessLabel = nullptr;
    m_markerOpacityLabel = nullptr;
    if (m_page) m_page->removeFromParentAndCleanup(true);
    const float contentH = m_tab == 2 ? 390.f : kSettingsViewH;
    m_scroll->setScrollEnabled(false);
    m_scroll->setInnerContentSize({kSettingsViewW, contentH});
    m_page = CCNode::create();
    m_page->setAnchorPoint({0.f, 0.f});
    m_page->setContentSize({kSettingsViewW, contentH});
    m_page->setPosition({0.f, 0.f});
    m_scroll->getContentLayer()->addChild(m_page, 3);
    m_scroll->setScrollEnabled(contentH > kSettingsViewH);
    m_previewWorld = nullptr;
    m_player = nullptr;
    m_behind = nullptr;
    m_ahead = nullptr;
    m_previewAppearance = {};
    m_colorModeLabel = nullptr;
    m_textPreviewLabel = nullptr;
    m_colorMarks.clear();
    m_switches.clear();
    auto* menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_page->addChild(menu, 8);
    auto* mod = Mod::get();
    auto& G = Ghosts::I();
    if (m_tab == 0) {
        panel(m_page, -100.f, 6.f, 351.f, 197.f);
        label(m_page, "Ghost Preview", -258.f, 83.f, .40f);
        auto* scene = CCScale9Sprite::create("square02b_001.png");
        if (scene) {
            scene->setContentSize({326.f, 66.f});
            scene->setPosition({-100.f, 39.f});
            scene->setColor({24, 69, 180});
            scene->setOpacity(255);
            m_page->addChild(scene, 1);
            auto inset = attemptplayback::ui::createInsetWorldView({326.f, 66.f});
            scene->addChild(inset.root, 1);
            m_previewWorld = inset.world;
            m_previewWorld->setContentSize({900.f, 66.f});
            attemptplayback::ui::addGhostScene(m_previewWorld, 900.f, 66.f, false);
            auto* gm = GameManager::sharedState();
            const int frame = gm ? gm->getPlayerFrame() : 1;
            m_player = SimplePlayer::create(frame);
            m_behind = SimplePlayer::create(frame);
            m_ahead = SimplePlayer::create(frame);
            for (auto* cube :{m_behind, m_ahead, m_player}) {
                if (!cube) continue;
                cube->setScale(.94f);
                m_previewWorld->addChild(cube, 4);
            }
        }
        rowBand(m_page, -100.f, -43.f, 326.f, 37.f);
        label(m_page, "Ghost Opacity", -258.f, -43.f, .33f);
        m_opacity = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onOpacity), .86f);
        attemptplayback::ui::placeSlider(m_opacity, m_page, -55.f, -43.f, .62f);
        int alpha = mod->hasSavedValue("ghost-opacity")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-opacity"))
            : 255;
        m_opacity->setValue(std::clamp(alpha / 255.f, 0.f, 1.f));
        m_opacityLabel = valueLabel(m_page, "100%", 27.f, -43.f);
        rowBand(m_page, -100.f, -82.f, 326.f, 37.f);
        label(m_page, "Ghost Distance", -258.f, -82.f, .33f);
        m_distance = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onDistance), .86f);
        attemptplayback::ui::placeSlider(m_distance, m_page, -55.f, -82.f, .62f);
        m_distance->setValue(std::clamp(G.getGhostDistance() / 174.f, 0.f, 1.f));
        m_distanceLabel = valueLabel(m_page, "45 px", 27.f, -82.f);
        panel(m_page, 177.f, 6.f, 196.f, 197.f);
        label(m_page, "Color Mode", 94.f, 83.f, .34f);
        rowBand(m_page, 177.f, 54.f, 188.f, 38.f);
        m_colorModeLabel = label(m_page, "Player", 96.f, 54.f, .36f);
        menu->addChild(button(this, menu_selector(TabbedGhostSettingsPopup::onCycleColor),
            "Next", 233.f, 54.f, 60.f, 24.f));
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Random Icons", 91.f, 9.f, G.isRandomIconsEnabled(), 1, 143.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Wave Ghost Trail", 91.f, -35.f, mod->getSettingValue<bool>("wave-trail-ghost"), 2, 143.f);
        refreshPreview();
    }
    if (m_tab == 1) {
        auto* p1 = button(this, menu_selector(TabbedGhostSettingsPopup::onColorPlayer),
            "Player 1", -85.f, 83.f, 123.f, 27.f, m_colorPlayer == 0 ? "GJ_button_01.png" : "GJ_button_04.png");
        p1->setTag(0);
        menu->addChild(p1);
        auto* p2 = button(this, menu_selector(TabbedGhostSettingsPopup::onColorPlayer),
            "Player 2", 85.f, 83.f, 123.f, 27.f, m_colorPlayer == 1 ? "GJ_button_01.png" : "GJ_button_04.png");
        p2->setTag(1);
        menu->addChild(p2);
        auto* gm = GameManager::sharedState();
        for (int slot = 0; slot<kRandomColorSlots; ++slot) {
            int r = slot / 16, col = slot % 16;
            float x = -254.f;
            float y = 57.f;
            if (slot < 96) {
                x += static_cast<float>(col / 4) * 120.f + static_cast<float>(col % 4) * 21.f;
                y -= static_cast<float>(r) * 23.f +(r >= 3 ? 9.f : 0.f);
            }
            else if (slot < 100) {
                x += 120.f + static_cast<float>(slot - 96) * 21.f;
                y = -105.f;
            }
            else {
                x += 330.f + static_cast<float>(slot - 100) * 21.f;
                y = -125.f;
            }
            auto* swatch = CCSprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
            if (!swatch) continue;
            if (gm) swatch->setColor(gm->colorForIdx(kRandomColorIDs[slot]));
            swatch->setScale(.61f);
            auto* item = CCMenuItemSpriteExtra::create(swatch, this, menu_selector(TabbedGhostSettingsPopup::onColorSlot));
            item->setPosition({x, y});
            item->setTag(slot);
            menu->addChild(item);
            auto* mark = CCLabelBMFont::create("X", "bigFont.fnt");
            mark->setColor({255, 40, 30});
            mark->setScale(.45f);
            mark->setPosition({x, y});
            m_page->addChild(mark, 9);
            m_colorMarks.push_back(mark);
        }
        auto* all = button(this, menu_selector(TabbedGhostSettingsPopup::onAllColors),
            "Enable All", -213.f, -147.f, 102.f, 24.f, "GJ_button_01.png");
        all->setTag(1);
        menu->addChild(all);
        auto* none = button(this, menu_selector(TabbedGhostSettingsPopup::onAllColors),
            "Disable All", -99.f, -147.f, 102.f, 24.f, "GJ_button_06.png");
        none->setTag(0);
        menu->addChild(none);
        menu->addChild(button(this, menu_selector(TabbedGhostSettingsPopup::onOpenPresets),
            "Save / Load Preset", 153.f, -147.f, 179.f, 24.f));
        refreshColorMarks();
    }
    if (m_tab == 2) {
        panel(m_page, 0.f, -73.f, 550.f, 344.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Enable Ghost Text", -239.f, 77.f, G.isPlayLayerGhostTextEnabled(), 3, 425.f);
        rowBand(m_page, 0.f, 40.f, 528.f, 31.f);
        label(m_page, "Mode", -238.f, 40.f, .37f);
        const char* modes[] = {"Alive", "Dead", "Custom"};
        const GhostTextPreset modeValues[] = {
            GhostTextPreset::AliveAttempts,
            GhostTextPreset::DeadAttempts,
            GhostTextPreset::Custom
        };
        for (int i = 0; i < 3; ++i) {
            auto* b = button(this, menu_selector(TabbedGhostSettingsPopup::onTextMode),
                modes[i], -75.f + i * 90.f, 40.f, 83.f, 24.f,
                    G.getGhostTextMode() == modeValues[i] ? "GJ_button_01.png" : "GJ_button_04.png");
            b->setTag(i);
            menu->addChild(b);
        }
        rowBand(m_page, 0.f, -1.f, 528.f, 32.f);
        label(m_page, "Text Size", kRowLeft, -1.f, .34f);
        m_textSize = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onTextSize), .75f);
        attemptplayback::ui::placeSlider(m_textSize, m_page, 45.f, -1.f, .75f);
        m_textSize->setValue(std::clamp((G.getPlayLayerGhostTextScale() - .15f) / 1.10f, 0.f, 1.f));
        m_textSizeLabel = valueLabel(m_page, "50%", 148.f, -1.f);
        rowBand(m_page, 0.f, -40.f, 528.f, 32.f);
        label(m_page, "Opacity", kRowLeft, -40.f, .34f);
        m_textOpacity = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onTextOpacity), .75f);
        attemptplayback::ui::placeSlider(m_textOpacity, m_page, 45.f, -40.f, .75f);
        m_textOpacity->setValue(G.getPlayLayerGhostTextOpacity() / 255.f);
        m_textOpacityLabel = valueLabel(m_page, "70%", 148.f, -40.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Flash On Death", -239.f, -81.f, G.isGhostTextFlashOnDeath(), 8, 425.f);
        rowBand(m_page, 0.f, -124.f, 528.f, 34.f);
        label(m_page, "Custom Text", kRowLeft, -124.f, .33f);
        m_textInput = TextInput::create(360.f, "Custom Text", "bigFont.fnt");
        m_textOriginal = G.getGhostTextCustomFormat();
        m_textInput->setString(m_textOriginal);
        m_textInput->setScale(0.8f);
        m_textInput->setPosition({-55.f, -157.f});
        m_textInput->setScale(.76f);
        m_page->addChild(m_textInput, 7);
        label(m_page, "Preview", kRowLeft, -190.f, .33f);
        panel(m_page, -55.f, -218.f, 390.f, 30.f);
        m_textPreviewLabel = label(m_page, "Alive Attempts: 127", -55.f, -218.f, .35f, false);
        menu->addChild(button(this, menu_selector(TabbedGhostSettingsPopup::onOpenText),
            "Variables / Position", 183.f, -245.f, 166.f, 25.f));
        refreshTextPreview();
    }
    if (m_tab == 3) {
        panel(m_page, 0.f, 0.f, 550.f, 220.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Show Death Markers", -239.f, 75.f, G.areDeathMarkersEnabled(), 4, 425.f);
        rowBand(m_page, -99.f, 29.f, 326.f, 32.f);
        label(m_page, "Size", kRowLeft, 29.f, .36f);
        m_markerSize = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onMarkerSize), .7f);
        attemptplayback::ui::placeSlider(m_markerSize, m_page, -63.f, 29.f, .73f);
        m_markerSize->setValue(std::clamp((G.getDeathMarkerSize() - 4.f) / 36.f, 0.f, 1.f));
        m_markerSizeLabel = valueLabel(m_page, "25px", 42.f, 29.f);
        rowBand(m_page, -99.f, -9.f, 326.f, 32.f);
        label(m_page, "Thickness", kRowLeft, -9.f, .36f);
        m_markerThickness = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onMarkerThickness), .7f);
        attemptplayback::ui::placeSlider(m_markerThickness, m_page, -63.f, -9.f, .73f);
        m_markerThickness->setValue(std::clamp((G.getDeathMarkerThickness() - 1.f) / 7.f, 0.f, 1.f));
        m_markerThicknessLabel = valueLabel(m_page, "5.0px", 42.f, -9.f);
        rowBand(m_page, -99.f, -47.f, 326.f, 32.f);
        label(m_page, "Opacity", kRowLeft, -47.f, .36f);
        m_markerOpacity = Slider::create(this, menu_selector(TabbedGhostSettingsPopup::onMarkerOpacity), .7f);
        attemptplayback::ui::placeSlider(m_markerOpacity, m_page, -63.f, -47.f, .73f);
        m_markerOpacity->setValue(G.getDeathMarkerOpacity() / 255.f);
        m_markerOpacityLabel = valueLabel(m_page, "100%", 42.f, -47.f);
        panel(m_page, 161.f, -22.f, 192.f, 108.f);
        auto* markerScene = CCNode::create();
        markerScene->setPosition({81.f, -75.f});
        m_page->addChild(markerScene, 3);
        for (int i = 0; i < 6; ++i) {
            attemptplayback::ui::addRealBlock(markerScene, 14.f + i * 27.f, 13.5f, 27.f);
        }
        attemptplayback::ui::addRealSpike(markerScene, 82.f, 39.f, 28.f);
        m_markerDraw = CCDrawNode::create();
        markerScene->addChild(m_markerDraw, 4);
        refreshMarkerPreview();
        menu->addChild(button(this, menu_selector(TabbedGhostSettingsPopup::onOpenMarkers),
            "Detailed Marker Preview", 0.f, -132.f, 250.f, 27.f));
    }
    if (m_tab == 4) {
        panel(m_page, 0.f, 0.f, 550.f, 220.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Ghost Death Sound", -235.f, 63.f, G.isGhostsExplodeSFXEnabled(), 5, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Ghosts Explode", -235.f, 20.f, G.isGhostsExplodeEnabled(), 6, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedGhostSettingsPopup::onToggle),
            "Custom Death Sound", -235.f, -23.f, G.getPlaybackCustomDeathSoundEnabled(), 7, 418.f);
        menu->addChild(button(this, menu_selector(TabbedGhostSettingsPopup::onOpenEffects),
            "Custom Sounds / Open Folder", 0.f, -119.f, 250.f, 30.f));
    }
    for (auto* child : m_page->getChildrenExt()) {
        child->setPosition(child->getPosition() + CCPoint{kSettingsViewW * .5f, contentH - 98.f});
    }
    m_scroll->scrollToTop();
    refreshSliderLabels();
}

void TabbedGhostSettingsPopup::update(float) {
    if (m_tab == 0) refreshPreview();
    if (m_tab == 2) refreshTextPreview();
}

void TabbedGhostSettingsPopup::refreshTextPreview() {
    if (!m_textPreviewLabel) return;
    auto& G = Ghosts::I();
    std::string text;
    switch (G.getGhostTextMode()) {
        case GhostTextPreset::AliveAttempts:
            text = "Alive Attempts: 727";
            break;
        case GhostTextPreset::DeadAttempts:
            text = "Dead Attempts: 67";
            break;
        case GhostTextPreset::Custom:
            text = m_textInput ? m_textInput->getString() : G.getGhostTextCustomFormat();
            break;
    }
    m_textPreviewLabel->setString(text.empty() ? " " : text.c_str());
    m_textPreviewLabel->setOpacity(static_cast<GLubyte>(G.getPlayLayerGhostTextOpacity()));
}

void TabbedGhostSettingsPopup::refreshPreview() {
    auto& G = Ghosts::I();
    const int distance = G.getGhostDistance();
    if (m_distanceLabel) m_distanceLabel->setString(fmt::format("{} px", distance).c_str());
    const int alpha = Mod::get()->hasSavedValue("ghost-opacity")
        ? static_cast<int>(Mod::get()->getSavedValue<int64_t>("ghost-opacity"))
        : 255;
    if (m_opacityLabel)
        m_opacityLabel->setString(fmt::format("{}%", static_cast<int>(std::lround(alpha * 100.f / 255.f))).c_str());
    if (m_colorModeLabel) {
        auto mode = Mod::get()->getSavedValue<std::string>("ghost-colors");
        m_colorModeLabel->setString(mode == "PlayerColors" ? "Player" : mode.empty() ? "Player" : mode.c_str());
    }

    auto* gm = GameManager::sharedState();
    if (!gm || !m_player) return;
    attemptplayback::ui::updateGhostPreviewAppearance(
        m_player, m_behind, m_ahead, m_previewAppearance, alpha
    );
    attemptplayback::ui::positionGhostsInInset(
        m_previewWorld, m_player, m_behind, m_ahead, {326.f, 66.f}, 122.f,
        static_cast<float>(distance));
}

void TabbedGhostSettingsPopup::onCycleColor(CCObject*) {
    const std::string names[] = {"PlayerColors", "Random", "White", "Black"};
    auto current = Mod::get()->getSavedValue<std::string>("ghost-colors");
    int index = 0;
    for (int i = 0; i < 4; ++i) {
        if (names[i] == current) index = i;
    }
    auto next = names[(index + 1) % 4];
    Mod::get()->setSavedValue("ghost-colors", next);
    Ghosts::I().setColorMode(
        next == "Random" ? ColorMode::Random :
        next == "White" ? ColorMode::White :
        next == "Black" ? ColorMode::Black : ColorMode::PlayerColors);
    refreshPreview();
}

void TabbedGhostSettingsPopup::onOpacity(CCObject*) {
    if (!m_opacity) return;
    int a = static_cast<int>(std::lround(m_opacity->getValue() * 255.f));
    Mod::get()->setSavedValue("ghost-opacity", static_cast<int64_t>(a));
    Ghosts::I().setOpacityVar(static_cast<GLubyte>(a));
    Ghosts::I().updateOpacityAll();
    refreshPreview();
}

void TabbedGhostSettingsPopup::onDistance(CCObject*) {
    if (!m_distance) return;
    int px = static_cast<int>(std::lround(m_distance->getValue() * 174.f));
    Ghosts::I().setGhostDistance(px);
    Mod::get()->setSavedValue("ghost-distance", static_cast<int64_t>(px));
    refreshPreview();
}

void TabbedGhostSettingsPopup::onToggle(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n) return;
    auto& G = Ghosts::I();
    auto* mod = Mod::get();
    switch (n->getTag()) {
        case 1: {
            bool v = !G.isRandomIconsEnabled();
            G.setRandomIconsEnabled(v);
            mod->setSavedValue("randomIcons-enabled", v);
            mod->setSettingValue("randomIcons-enabled", v);
            break;
        }
        case 2: {
            bool v = !mod->getSettingValue<bool>("wave-trail-ghost");
            mod->setSettingValue("wave-trail-ghost", v);
            break;
        }
        case 3:
            G.setPlayLayerGhostTextEnabled(!G.isPlayLayerGhostTextEnabled());
            break;
        case 4:
            G.setDeathMarkersEnabled(!G.areDeathMarkersEnabled());
            break;
        case 5: {
            bool v = !G.isGhostsExplodeSFXEnabled();
            G.setGhostsExplodeSFXEnabled(v);
            mod->setSavedValue("ghosts-explode-sfx", v);
            mod->setSettingValue("ghosts-explode-sfx", v);
            break;
        }
        case 6: {
            bool v = !G.isGhostsExplodeEnabled();
            G.setGhostsExplodeEnabled(v);
            mod->setSavedValue("ghosts-explode", v);
            mod->setSettingValue("ghosts-explode", v);
            break;
        }
        case 7:
            G.setPlaybackCustomDeathSoundEnabled(!G.getPlaybackCustomDeathSoundEnabled());
            G.refreshCustomDeathSfxNow(G.isReplaying());
            break;
        case 8:
            G.setGhostTextFlashOnDeath(!G.isGhostTextFlashOnDeath());
            break;
    }
}

void TabbedGhostSettingsPopup::commitText() {
    if (m_textInput && m_textInput->getString() != m_textOriginal) {
        m_textOriginal = m_textInput->getString();
        Ghosts::I().setGhostTextCustomFormat(m_textOriginal);
    }
}

void TabbedGhostSettingsPopup::onClose(CCObject* sender) {
    unschedule(schedule_selector(TabbedGhostSettingsPopup::applyQueuedTab));
    unschedule(schedule_selector(TabbedGhostSettingsPopup::applyQueuedPage));
    commitText();
    Ghosts::I().updateOpacityAll();
    if (auto* scene = CCDirector::sharedDirector()->getRunningScene()) {
        if (auto* parent = typeinfo_cast<PlaybackModMenu*>(scene->getChildByIDRecursive("playbackModMenu-popup"_spr))) {
            parent->syncUIFromRuntime();
        }
    }
    Popup::onClose(sender);
}

void TabbedGhostSettingsPopup::onTextMode(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n) return;
    commitText();
    Ghosts::I().setGhostTextMode(n->getTag() == 0 ? GhostTextPreset::AliveAttempts :
    n->getTag() == 1 ? GhostTextPreset::DeadAttempts : GhostTextPreset::Custom);
    refreshTextPreview();
    scheduleOnce(schedule_selector(TabbedGhostSettingsPopup::applyQueuedPage), 0.f);
}

void TabbedGhostSettingsPopup::onTextSize(CCObject*) {
    if (m_textSize) Ghosts::I().setPlayLayerGhostTextScale(.15f + 1.10f * m_textSize->getValue());
    refreshSliderLabels();
}

void TabbedGhostSettingsPopup::onTextOpacity(CCObject*) {
    if (m_textOpacity)
        Ghosts::I().setPlayLayerGhostTextOpacity(static_cast<int>(std::lround(255.f * m_textOpacity->getValue())));
    refreshSliderLabels();
}

void TabbedGhostSettingsPopup::refreshSliderLabels() {
    auto& G = Ghosts::I();
    if (m_textSizeLabel)
        m_textSizeLabel->setString(
            fmt::format("{}%", static_cast<int>(std::lround(G.getPlayLayerGhostTextScale() * 100.f))).c_str());
    if (m_textOpacityLabel)
        m_textOpacityLabel->setString(
            fmt::format("{}%", static_cast<int>(std::lround(G.getPlayLayerGhostTextOpacity() * 100.f / 255.f))).c_str());
    if (m_markerSizeLabel)
        m_markerSizeLabel->setString(fmt::format("{:.0f}px", G.getDeathMarkerSize()).c_str());
    if (m_markerThicknessLabel)
        m_markerThicknessLabel->setString(fmt::format("{:.1f}px", G.getDeathMarkerThickness()).c_str());
    if (m_markerOpacityLabel)
        m_markerOpacityLabel->setString(
            fmt::format("{}%", static_cast<int>(std::lround(G.getDeathMarkerOpacity() * 100.f / 255.f))).c_str());
}

void TabbedGhostSettingsPopup::refreshMarkerPreview() {
    if (!m_markerDraw) return;
    m_markerDraw->clear();
    auto& G = Ghosts::I();
    const float half = std::max(2.f, G.getDeathMarkerSize() * .5f);
    const float thickness = std::max(.5f, G.getDeathMarkerThickness() * .5f);
    const auto color = ccc4f(1.f, .08f, .08f, G.getDeathMarkerOpacity() / 255.f);
    m_markerDraw->drawSegment({82.f - half, 61.f - half}, {82.f + half, 61.f + half}, thickness, color);
    m_markerDraw->drawSegment({82.f - half, 61.f + half}, {82.f + half, 61.f - half}, thickness, color);
}

void TabbedGhostSettingsPopup::onMarkerSize(CCObject*) {
    if (m_markerSize) Ghosts::I().setDeathMarkerSize(4.f + 36.f * m_markerSize->getValue());
    refreshSliderLabels();
    refreshMarkerPreview();
}

void TabbedGhostSettingsPopup::onMarkerThickness(CCObject*) {
    if (m_markerThickness) Ghosts::I().setDeathMarkerThickness(1.f + 7.f * m_markerThickness->getValue());
    refreshSliderLabels();
    refreshMarkerPreview();
}

void TabbedGhostSettingsPopup::onMarkerOpacity(CCObject*) {
    if (m_markerOpacity)
        Ghosts::I().setDeathMarkerOpacity(static_cast<int>(std::lround(255.f * m_markerOpacity->getValue())));
    refreshSliderLabels();
    refreshMarkerPreview();
}

void TabbedGhostSettingsPopup::onColorPlayer(CCObject* sender) {
    if (auto* n = typeinfo_cast<CCNode*>(sender)) {
        m_colorPlayer = n->getTag();
        scheduleOnce(schedule_selector(TabbedGhostSettingsPopup::applyQueuedPage), 0.f);
    }
}

void TabbedGhostSettingsPopup::refreshColorMarks() {
    const auto mask = colorMask(m_colorPlayer);
    for (size_t i = 0; i < m_colorMarks.size() && i < mask.size(); ++i) m_colorMarks[i]->setVisible(mask[i] != '1');
}

void TabbedGhostSettingsPopup::onColorSlot(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n) return;
    auto s = colorMask(m_colorPlayer);
    int i = n->getTag();
    if (i < 0 || i >= static_cast<int>(s.size())) return;
    s[i] = s[i] == '1' ? '0' : '1';
    saveMask(m_colorPlayer, s);
    refreshColorMarks();
}

void TabbedGhostSettingsPopup::onAllColors(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n) return;
    std::string s(kRandomColorSlots, n->getTag() == 1 ? '1' : '0');
    saveMask(m_colorPlayer, s);
    refreshColorMarks();
}

void TabbedGhostSettingsPopup::onOpenPresets(CCObject*) {
    showNative(ColorSelectorPopup::create());
}

void TabbedGhostSettingsPopup::onOpenText(CCObject*) {
    showNative(GhostTextPopup::create());
}

void TabbedGhostSettingsPopup::onOpenMarkers(CCObject*) {
    showNative(DeathMarkerSettingsPopup::create());
}

void TabbedGhostSettingsPopup::onOpenEffects(CCObject*) {
    showNative(PlaybackSettingsPopup::create());
}

TabbedPlaybackSettingsPopup* TabbedPlaybackSettingsPopup::create() {
    auto* p = new TabbedPlaybackSettingsPopup();
    if (p && p->init(kSettingsW, kSettingsH)) {
        p->autorelease();
        return p;
    }
    CC_SAFE_DELETE(p);
    return nullptr;
}

bool TabbedPlaybackSettingsPopup::init(float w, float h) {
    if (!Popup::init(w, h)) return false;
    setID("tabbed-playback-settings-popup"_spr);
    setTitle("Playback Settings");
    auto* tabBar = CCScale9Sprite::create("square02b_001.png");
    if (tabBar) {
        tabBar->setContentSize({554.f, 34.f});
        tabBar->setPosition({w * .5f, h * .5f + 113.f});
        tabBar->setColor({80, 39, 19});
        m_mainLayer->addChild(tabBar, 9);
    }
    m_tabs = CCMenu::create();
    m_tabs->setPosition({w * .5f, h * .5f});
    m_mainLayer->addChild(m_tabs, 10);
    m_scroll = attemptplayback::ui::AttemptScrollView::create({kSettingsViewW, kSettingsViewH});
    if (!m_scroll) return false;
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->setPosition({w * .5f - kSettingsViewW * .5f, h * .5f - 166.f});
    if (auto* dispatcher = CCDirector::sharedDirector()->getTouchDispatcher())
        m_scroll->setTouchPriority(dispatcher->getTargetPrio());
    m_mainLayer->addChild(m_scroll, 3);
    buildTabs();
    buildPage();
    fitSettings(m_mainLayer);
    return true;
}

void TabbedPlaybackSettingsPopup::buildTabs() {
    m_tabs->removeAllChildrenWithCleanup(true);
    const char* names[] = {"Replay & Filtering", "Recording & Saving", "Performance & Advanced"};
    tabs(m_tabs, this, menu_selector(TabbedPlaybackSettingsPopup::onTab), m_tab, names, 3);
}

void TabbedPlaybackSettingsPopup::onTab(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n || n->getTag() == m_tab) return;
    commitInputs();
    m_tab = n->getTag();
    m_tabs->setEnabled(false);
    scheduleOnce(schedule_selector(TabbedPlaybackSettingsPopup::applyQueuedTab), 0.f);
}

void TabbedPlaybackSettingsPopup::applyQueuedTab(float) {
    buildTabs();
    buildPage();
    m_tabs->setEnabled(true);
}

void TabbedPlaybackSettingsPopup::buildPage() {
    if (m_page) m_page->removeFromParentAndCleanup(true);
    m_percent = nullptr;
    m_autosaveMinutes = nullptr;
    m_maxPlayerObjects = nullptr;
    m_capacityStatus = nullptr;
    const float contentH = kSettingsViewH;
    m_scroll->setScrollEnabled(false);
    m_scroll->setInnerContentSize({kSettingsViewW, contentH});
    m_page = CCNode::create();
    m_page->setAnchorPoint({0.f, 0.f});
    m_page->setContentSize({kSettingsViewW, contentH});
    m_page->setPosition({0.f, 0.f});
    m_scroll->getContentLayer()->addChild(m_page, 3);
    auto* menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_page->addChild(menu, 5);
    auto* mod = Mod::get();
    auto& G = Ghosts::I();
    panel(m_page, 0.f, -24.f, 548.f, 264.f);
    if (m_tab == 0) {
        label(m_page, "Replay Accuracy", -251.f, 85.f, .45f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Accurate High TPS Replay (can be laggy)", -235.f, 49.f,
            mod->getSettingValue<bool>("accurate-high-tps-replay"), 1, 418.f);
        label(m_page, "Process every recorded frame crossed during replay.", -235.f, 23.f, .27f);
        label(m_page, "Ghost Filtering", -251.f, -8.f, .45f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Only show attempts past a percentage", -235.f, -42.f,
            G.getPlaybackOnlyPastPercentEnabled(), 2, 418.f);
        rowBand(m_page, 0.f, -87.f, 532.f);
        label(m_page, "Minimum completion (%)", kRowLeft, -87.f, .34f);
        m_percent = TextInput::create(80.f, "Percent", "bigFont.fnt");
        m_percent->setFilter("0123456789.");
        m_percent->setMaxCharCount(6);
        m_percent->setString(fmt::format("{:.2f}", G.getPlaybackOnlyPastPercentThreshold()), false);
        m_percent->setScale(0.8f);
        m_percent->setPosition({kInputRight, -87.f});
        m_page->addChild(m_percent, 5);
        m_percent->setCallback([this](std::string const& value) {applyPercentInput_(value);});
    }
    if (m_tab == 1) {
        label(m_page, "Recording & Saving", -251.f, 85.f, .45f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Record Attempts", -235.f, 48.f, !G.isRecordingBlocked(), 3, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Record Noclip Attempts", -235.f, 14.f, !G.isRecordingBlockedOnNoclip(), 4, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Record in Practice", -235.f, -20.f, mod->getSettingValue<bool>("record-in-practice"), 5, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Allow Platformer Recording", -235.f, -54.f, mod->getSettingValue<bool>("allow-platformer"), 6, 418.f);
        toggle(m_page, menu, this, menu_selector(TabbedPlaybackSettingsPopup::onToggle),
            "Autosave Attempts", -235.f, -88.f, mod->getSettingValue<bool>("autosave-enabled"), 7, 418.f);
        rowBand(m_page, 0.f, -126.f, 532.f);
        label(m_page, "Autosave interval (minutes)", kRowLeft, -126.f, .34f);
        m_autosaveMinutes = TextInput::create(80.f, "Minutes", "bigFont.fnt");
        m_autosaveMinutes->setFilter("0123456789");
        m_autosaveMinutes->setMaxCharCount(3);
        m_autosaveMinutes->setString(fmt::format("{}", mod->getSettingValue<int64_t>("autosave-minutes")), false);
        m_autosaveMinutes->setScale(0.8f);
        m_autosaveMinutes->setPosition({kInputRight, -126.f});
        m_page->addChild(m_autosaveMinutes, 5);
        m_autosaveMinutes->setCallback([this](std::string const& value) {applyAutosaveInput_(value);});
    }
    if (m_tab == 2) {
        label(m_page, "Performance", -251.f, 85.f, .45f);
        rowBand(m_page, 0.f, 47.f, 532.f);
        label(m_page, "Maximum Real PlayerObjects", kRowLeft, 47.f, .34f);
        m_maxPlayerObjects = TextInput::create(100.f, "Objects", "bigFont.fnt");
        m_maxPlayerObjects->setFilter("0123456789");
        m_maxPlayerObjects->setMaxCharCount(5);
        m_maxPlayerObjects->setString(fmt::format("{}", mod->getSettingValue<int64_t>("real-player-objects")), false);
        m_maxPlayerObjects->setScale(0.8f);
        m_maxPlayerObjects->setPosition({kInputRight, 47.f});
        m_page->addChild(m_maxPlayerObjects, 5);
        m_maxPlayerObjects->setCallback([this](std::string const& value) {applyPlayerObjectInput_(value);});
        m_capacityStatus = label(m_page, "Every 1k uses 500 MB of RAM and going higher can dramatically increase lag.", -235.f, 11.f, .28f);
    }
    // Keep page origin and scrolling consistent
    for (auto* child : m_page->getChildrenExt()) {
        child->setPosition(child->getPosition() + CCPoint{kSettingsViewW * .5f, contentH - 98.f});
    }
    m_scroll->scrollToTop();
}

void TabbedPlaybackSettingsPopup::stopReplayForFilterChange_() {
    auto& G = Ghosts::I();
    if (!G.isReplaying()) return;
    G.stopReplay();
    G.restartLevel();
    G.setRecordingEnabled(true);
    Mod::get()->setSavedValue("recording-runtime", true);
    Mod::get()->setSavedValue("apx-replay-ui-active", false);
}

void TabbedPlaybackSettingsPopup::onToggle(CCObject* sender) {
    auto* n = typeinfo_cast<CCNode*>(sender);
    if (!n) return;
    auto* mod = Mod::get();
    auto& G = Ghosts::I();
    switch (n->getTag()) {
        case 1: {
            const bool enabled = !mod->getSettingValue<bool>("accurate-high-tps-replay");
            mod->setSettingValue("accurate-high-tps-replay", enabled);
            G.setAccurateHighTPSReplay(enabled);
            break;
        }
        case 2: {
            // Stop first: changing the filter rebuilds PlayerObject owners
            stopReplayForFilterChange_();
            G.setPlaybackOnlyPastPercentEnabled(!G.getPlaybackOnlyPastPercentEnabled());
            break;
        }
        case 3: {
            const bool blocked = !G.isRecordingBlocked();
            G.setRecordingBlocked(blocked);
            mod->setSavedValue("recording-blocked", blocked);
            mod->setSettingValue("recording-blocked", blocked);
            break;
        }
        case 4: {
            const bool blocked = !G.isRecordingBlockedOnNoclip();
            G.setRecordingBlockedOnNoclip(blocked);
            mod->setSavedValue("recording-blocked-on-noclip", blocked);
            mod->setSettingValue("recording-blocked-on-noclip", blocked);
            break;
        }
        case 5: {
            const bool enabled = !mod->getSettingValue<bool>("record-in-practice");
            mod->setSettingValue("record-in-practice", enabled);
            G.setRecordInPractice(enabled);
            break;
        }
        case 6: {
            const bool enabled = !mod->getSettingValue<bool>("allow-platformer");
            mod->setSettingValue("allow-platformer", enabled);
            G.updateModEnabled();
            // also refreshes runtime platformer flag
            if (!G.isModEnabled()) G.setActiveGhostsInvisible();
            break;
        }
        case 7: {
            const bool enabled = !mod->getSettingValue<bool>("autosave-enabled");
            mod->setSettingValue("autosave-enabled", enabled);
            G.setAutosaveEnabled(enabled);
            break;
        }
    }
}

void TabbedPlaybackSettingsPopup::applyPercentInput_(std::string const& raw) {
    char* end = nullptr;
    const float value = std::strtof(raw.c_str(), & end);
    if (end == raw.c_str() || * end != '\0' || !std::isfinite(value)) return;
    const float clamped = std::clamp(value, 0.f, 100.f);
    auto& G = Ghosts::I();
    if (std::fabs(clamped - G.getPlaybackOnlyPastPercentThreshold()) < .0001f) return;
    if (G.getPlaybackOnlyPastPercentEnabled()) stopReplayForFilterChange_();
    G.setPlaybackOnlyPastPercentThreshold(clamped);
}

void TabbedPlaybackSettingsPopup::applyAutosaveInput_(std::string const& raw) {
    char* end = nullptr;
    const long value = std::strtol(raw.c_str(), & end, 10);
    if (end == raw.c_str() || * end != '\0' || value < 1 || value> 120) return;
    auto* mod = Mod::get();
    if (mod->getSettingValue<int64_t>("autosave-minutes") == value) return;
    mod->setSettingValue("autosave-minutes", static_cast<int64_t>(value));
    Ghosts::I().setAutosaveMinutes(static_cast<int>(value));
}

void TabbedPlaybackSettingsPopup::applyPlayerObjectInput_(std::string const& raw) {
    char* end = nullptr;
    const long value = std::strtol(raw.c_str(), & end, 10);
    if (end == raw.c_str() || * end != '\0' || value < 250 || value> 5000) return;
    auto* mod = Mod::get();
    if (mod->getSettingValue<int64_t>("real-player-objects") == value) return;
    mod->setSettingValue("real-player-objects", static_cast<int64_t>(value));
    const bool resized = Ghosts::I().resizeRealPlayerObjects(static_cast<int>(value));
    if (m_capacityStatus) {
        m_capacityStatus->setString(resized
        ? "Capacity updated for the current level."
        : "In-use slots prevent shrinking; applies next level.");
    }
}

void TabbedPlaybackSettingsPopup::commitInputs() {
    if (m_percent) applyPercentInput_(m_percent->getString());
    if (m_autosaveMinutes) applyAutosaveInput_(m_autosaveMinutes->getString());
    if (m_maxPlayerObjects) applyPlayerObjectInput_(m_maxPlayerObjects->getString());
}

void TabbedPlaybackSettingsPopup::onClose(CCObject* sender) {
    unschedule(schedule_selector(TabbedPlaybackSettingsPopup::applyQueuedTab));
    commitInputs();
    if (auto* scene = CCDirector::sharedDirector()->getRunningScene()) {
        if (auto* parent = typeinfo_cast<PlaybackModMenu*>(scene->getChildByIDRecursive("playbackModMenu-popup"_spr))) {
            parent->syncUIFromRuntime();
        }
    }
    Popup::onClose(sender);
}
