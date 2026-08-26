#include "ghost_settings_popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/cocos/draw_nodes/CCDrawNode.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>
#include <Geode/loader/Mod.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Layout.hpp>
#include <UIBuilder.hpp>

#include "color_selector_popup.hpp"
#include "ghost_distance_popup.hpp"
#include "../utils/ui_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace {
    constexpr float kGhostSettingsW = 430.f;
    constexpr float kGhostSettingsH = 250.f;
    constexpr float kGhostTextW = 440.f;
    constexpr float kGhostTextH = 300.f;
    constexpr float kGhostTextPosW = 300.f;
    constexpr float kGhostTextPosH = 185.f;
    constexpr float kDeathMarkerW = 360.f;
    constexpr float kDeathMarkerH = 230.f;
    constexpr float kButtonMult = 1.1f;

    constexpr float kUiTextureScale = 2.f;

    void setScale9VisualSize_(
        CCScale9Sprite* sprite,
        CCSize const& visualSize
    ) {
        if (!sprite) return;

        sprite->setContentSize({
            visualSize.width * kUiTextureScale,
            visualSize.height * kUiTextureScale
        });
        sprite->setScale(1.f / kUiTextureScale);
    }

    template <class T>
    T* findNodeOfTypeRecursive_(CCNode* root) {
        if (!root) return nullptr;

        if (auto* typed = typeinfo_cast<T*>(root)) {
            return typed;
        }

        for (auto* child : root->getChildrenExt()) {
            if (auto* found = findNodeOfTypeRecursive_<T>(child)) {
                return found;
            }
        }

        return nullptr;
    }

    CCMenuItemToggler* makeToggle_(CCObject* target, SEL_MenuHandler callback) {
        auto* onSprite = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto* offSprite = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

        auto* toggle = CCMenuItemToggler::create(
            offSprite,
            onSprite,
            target,
            callback
        );

        if (toggle) {
            toggle->setSizeMult(kButtonMult);
        }

        return toggle;
    }

    template <class PopupT>
    void showNoElasticity_(PopupT* popup) {
        if (!popup) return;

        popup->m_noElasticity = true;
        popup->show();
    }

    

    std::string previewFormat_(std::string formatText) {
        size_t total = Ghosts::I().getGhostTextTotalCount();
        size_t alive = Ghosts::I().getGhostTextAliveCount();
        size_t dead = Ghosts::I().getGhostTextDeadCount();

        // settings preview example numbers 
        if (total == 0) {
            total = 250;
            alive = 127;
            dead = 123;
        }

        return Ghosts::I().formatGhostTextForCounts_(
            formatText,
            alive,
            dead,
            total
        );
    }

    void addDarkRow_(CCNode* root, float y, float width = 390.f) {
        auto* background = CCScale9Sprite::create("square02b_001.png");
        if (!background) return;

        setScale9VisualSize_(background, { width, 42.f });
        background->setColor({ 60, 25, 15 });
        background->setOpacity(85);
        background->setPosition({ 0.f, y });
        root->addChild(background, -1);
    }

    void drawDeathMarkerShape_(
        CCDrawNode* node,
        CCPoint const& center,
        float size,
        float thickness,
        float opacity = 1.f
    ) {
        if (!node) return;

        const float half = std::max(2.f, size * 0.5f);
        const float radius = std::max(0.25f, thickness * 0.5f);

        const float alpha = std::clamp(opacity, 0.f, 1.f);
        if (alpha <= 0.f) return;

        const ccColor4F color = ccc4f(
            alpha,
            0.08f * alpha,
            0.08f * alpha,
            alpha
        );

        node->drawSegment(
            { center.x - half, center.y - half },
            { center.x + half, center.y + half },
            radius,
            color
        );

        node->drawSegment(
            { center.x - half, center.y + half },
            { center.x + half, center.y - half },
            radius,
            color
        );
    }
}

// Ghost settings hub

GhostSettingsPopup* GhostSettingsPopup::create() {
    auto* popup = new GhostSettingsPopup();

    if (popup && popup->init(kGhostSettingsW, kGhostSettingsH)) {
        popup->autorelease();
        return popup;
    }

    CC_SAFE_DELETE(popup);
    return nullptr;
}

bool GhostSettingsPopup::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    setID("ghost-settings-popup"_spr);
    setTitle("Ghost Settings");

    auto* root = CCNode::create();
    root->setPosition(m_mainLayer->getContentSize() * 0.5f);
    m_mainLayer->addChild(root);

    for (float y : { 58.f, 14.f, -30.f, -74.f }) {
        addDarkRow_(root, y);
    }

    auto* menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    root->addChild(menu);

    auto addLabel = [&](const char* text, float y) {
        auto* label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ -150.f, y });
        label->setScale(0.52f);
        root->addChild(label);
    };

    addLabel("Color Selector", 58.f);
    addLabel("Ghost Distance", 14.f);
    addLabel("Ghost Text", -30.f);
    addLabel("Death Markers", -74.f);

    auto addActionButton = [&] (
        const char* text,
        SEL_MenuHandler callback,
        float y,
        const char* id
    ) {
        auto* node = createTextButton_(
            this,
            "GJ_button_02.png",
            text,
            callback,
            id,
            { 142.f, y },
            125.f,
            34.f,
            0.62f,
            0.38f,
            1.05f
        ).collect();

        menu->addChild(node);
    };

    addActionButton(
        "Open",
        menu_selector(GhostSettingsPopup::onOpenColorSelector),
        58.f,
        "ghost-settings-color"_spr
    );

    addActionButton(
        "Edit",
        menu_selector(GhostSettingsPopup::onOpenGhostDistance),
        14.f,
        "ghost-settings-distance"_spr
    );

    addActionButton(
        "Configure",
        menu_selector(GhostSettingsPopup::onConfigureGhostText),
        -30.f,
        "ghost-settings-text"_spr
    );

    addActionButton(
        "Configure",
        menu_selector(GhostSettingsPopup::onConfigureDeathMarkers),
        -74.f,
        "ghost-settings-markers"_spr
    );

    m_distanceValue = CCLabelBMFont::create("45 px", "goldFont.fnt");
    m_distanceValue->setAnchorPoint({ 1.f, 0.5f });
    m_distanceValue->setPosition({ 72.f, 14.f });
    m_distanceValue->setScale(0.5f);
    root->addChild(m_distanceValue);

    m_ghostTextToggle = makeToggle_(
        this,
        menu_selector(GhostSettingsPopup::onToggleGhostText)
    );
    m_ghostTextToggle->setPosition({ 70.f, -30.f });
    m_ghostTextToggle->setScale(0.72f);
    menu->addChild(m_ghostTextToggle);

    m_deathMarkersToggle = makeToggle_(
        this,
        menu_selector(GhostSettingsPopup::onToggleDeathMarkers)
    );
    m_deathMarkersToggle->setPosition({ 70.f, -74.f });
    m_deathMarkersToggle->setScale(0.72f);
    menu->addChild(m_deathMarkersToggle);

    refresh_();
    scheduleUpdate();
    return true;
}

void GhostSettingsPopup::update(float) {
    refresh_();
}

void GhostSettingsPopup::refresh_() {
    auto& ghosts = Ghosts::I();

    if (
        m_ghostTextToggle &&
        m_ghostTextToggle->isToggled() != ghosts.isPlayLayerGhostTextEnabled()
    ) {
        m_ghostTextToggle->toggle(ghosts.isPlayLayerGhostTextEnabled());
    }

    if (
        m_deathMarkersToggle &&
        m_deathMarkersToggle->isToggled() != ghosts.areDeathMarkersEnabled()
    ) {
        m_deathMarkersToggle->toggle(ghosts.areDeathMarkersEnabled());
    }

    if (m_distanceValue) {
        m_distanceValue->setString(
            fmt::format("{} px", ghosts.getGhostDistance()).c_str()
        );
    }
}

void GhostSettingsPopup::onOpenColorSelector(CCObject*) {
    showNoElasticity_(ColorSelectorPopup::create());
}

void GhostSettingsPopup::onOpenGhostDistance(CCObject*) {
    showNoElasticity_(GhostDistancePopup::create());
}

void GhostSettingsPopup::onConfigureGhostText(CCObject*) {
    showNoElasticity_(GhostTextPopup::create());
}

void GhostSettingsPopup::onConfigureDeathMarkers(CCObject*) {
    showNoElasticity_(DeathMarkerSettingsPopup::create());
}

void GhostSettingsPopup::onToggleGhostText(CCObject*) {
    auto& ghosts = Ghosts::I();
    ghosts.setPlayLayerGhostTextEnabled(
        !ghosts.isPlayLayerGhostTextEnabled()
    );
}

void GhostSettingsPopup::onToggleDeathMarkers(CCObject*) {
    auto& ghosts = Ghosts::I();
    ghosts.setDeathMarkersEnabled(!ghosts.areDeathMarkersEnabled());
}

// Ghost text popup

GhostTextPopup* GhostTextPopup::create() {
    auto* popup = new GhostTextPopup();

    if (popup && popup->init(kGhostTextW, kGhostTextH)) {
        popup->autorelease();
        return popup;
    }

    CC_SAFE_DELETE(popup);
    return nullptr;
}

bool GhostTextPopup::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    setID("ghost-text-popup"_spr);
    setTitle("Ghost Text");

    auto& ghosts = Ghosts::I();

    auto* root = CCNode::create();
    root->setPosition(m_mainLayer->getContentSize() * 0.5f);
    m_mainLayer->addChild(root);

    m_rootMenu = CCMenu::create();
    m_rootMenu->setPosition({ 0.f, 0.f });
    root->addChild(m_rootMenu);

    auto* showLabel = CCLabelBMFont::create("Enable Ghost Text", "bigFont.fnt");
    showLabel->setAnchorPoint({ 0.f, 0.5f });
    showLabel->setPosition({ -160.f, 98.f });
    showLabel->setScale(0.48f);
    root->addChild(showLabel);

    m_showToggle = makeToggle_(
        this,
        menu_selector(GhostTextPopup::onToggleShow)
    );
    m_showToggle->setPosition({ -175.f, 98.f });
    m_showToggle->setScale(0.72f);
    m_rootMenu->addChild(m_showToggle);

    auto* modeLabel = CCLabelBMFont::create("Mode", "bigFont.fnt");
    modeLabel->setAnchorPoint({ 0.f, 0.5f });
    modeLabel->setPosition({ -185.f, 64.f });
    modeLabel->setScale(0.43f);
    root->addChild(modeLabel);

    struct ModeSpec {
        const char* text;
        SEL_MenuHandler callback;
        const char* baseId;
        const char* selectedId;
        float x;
    };

    const ModeSpec modes[] = {
        {
            "Alive",
            menu_selector(GhostTextPopup::onModeAlive),
            "ghost-text-alive-base"_spr,
            "ghost-text-alive-selected"_spr,
            -70.f
        },
        {
            "Dead",
            menu_selector(GhostTextPopup::onModeDead),
            "ghost-text-dead-base"_spr,
            "ghost-text-dead-selected"_spr,
            30.f
        },
        {
            "Custom",
            menu_selector(GhostTextPopup::onModeCustom),
            "ghost-text-custom-base"_spr,
            "ghost-text-custom-selected"_spr,
            130.f
        },
    };

    for (auto const& spec : modes) {
        auto* baseButton = createTextButton_(
            this,
            "GJ_button_04.png",
            spec.text,
            spec.callback,
            spec.baseId,
            { spec.x, 64.f },
            93.f,
            30.f,
            0.62f,
            0.34f,
            1.05f
        ).collect();

        auto* selectedButton = createTextButton_(
            this,
            "GJ_button_01.png",
            spec.text,
            spec.callback,
            spec.selectedId,
            { spec.x, 64.f },
            93.f,
            30.f,
            0.62f,
            0.34f,
            1.05f
        ).collect();

        m_rootMenu->addChild(baseButton);
        m_rootMenu->addChild(selectedButton);
    }

    m_customInput = TextInput::create(
        300.f,
        "{alive}/{total} attempts remain",
        "bigFont.fnt"
    );
    m_customInput->setPosition({ 15.f, -92.f });
    m_customInput->setMaxCharCount(120);
    m_customInput->setFilter(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
        "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
    );
    m_customInput->setString(ghosts.getGhostTextCustomFormat().c_str(), false);
    m_customInput->setCallback([this](std::string const& value) {
        Ghosts::I().setGhostTextCustomFormat(value);
        refreshPreview_();
    });
    root->addChild(m_customInput);

    auto* variablesButton = createTextButton_(
        this,
        "GJ_button_02.png",
        "Variables",
        menu_selector(GhostTextPopup::onVariables),
        "ghost-text-variables"_spr,
        { -105.f, -125 },
        100.f,
        29.f,
        0.65f,
        0.33f,
        1.05f
    ).collect();
    m_rootMenu->addChild(variablesButton);

    auto* resetButton = createTextButton_(
        this,
        "GJ_button_02.png",
        "Reset",
        menu_selector(GhostTextPopup::onResetText),
        "ghost-text-reset"_spr,
        { 138.f, -125 },
        80.f,
        29.f,
        0.65f,
        0.33f,
        1.05f
    ).collect();
    m_rootMenu->addChild(resetButton);

    auto* sizeText = CCLabelBMFont::create("Text Size", "bigFont.fnt");
    sizeText->setAnchorPoint({ 0.f, 0.5f });
    sizeText->setPosition({ -185.f, -18.f + 56.f});
    sizeText->setScale(0.4f);
    root->addChild(sizeText);

    m_sizeSlider = Slider::create(
        this,
        menu_selector(GhostTextPopup::onSizeSlider),
        0.72f
    );
    m_sizeSlider->setPosition({ -136.f, -64.f + 56.f});
    m_sizeSlider->setScale(0.72f);
    m_rootMenu->addChild(m_sizeSlider);

    m_sizeLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_sizeLabel->setPosition({ 18.f, -18.f + 56.f});
    m_sizeLabel->setScale(0.35f);
    root->addChild(m_sizeLabel);

    auto* opacityText = CCLabelBMFont::create("Opacity", "bigFont.fnt");
    opacityText->setAnchorPoint({ 0.f, 0.5f });
    opacityText->setPosition({ -185.f, -48.f + 56.f});
    opacityText->setScale(0.4f);
    root->addChild(opacityText);

    m_opacitySlider = Slider::create(
        this,
        menu_selector(GhostTextPopup::onOpacitySlider),
        0.72f
    );
    m_opacitySlider->setPosition({ -136.f, -94.f + 56.f});
    m_opacitySlider->setScale(0.72f);
    m_rootMenu->addChild(m_opacitySlider);

    m_opacityLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_opacityLabel->setPosition({ 18.f, -48.f + 56.f});
    m_opacityLabel->setScale(0.35f);
    root->addChild(m_opacityLabel);

    m_flashToggle = makeToggle_(
        this,
        menu_selector(GhostTextPopup::onToggleFlash)
    );
    m_flashToggle->setPosition({ -175.f, -70.f + 56.f});
    m_flashToggle->setScale(0.68f);
    m_rootMenu->addChild(m_flashToggle);

    auto* flashText = CCLabelBMFont::create("Flash on Death", "bigFont.fnt");
    flashText->setAnchorPoint({ 0.f, 0.5f });
    flashText->setPosition({ -145.f, -70.f + 56.f});
    flashText->setScale(0.4f);
    root->addChild(flashText);

    auto* positionText = CCLabelBMFont::create("Position", "bigFont.fnt");
    positionText->setAnchorPoint({ 0.f, 0.5f });
    positionText->setPosition({ 25.f, -70.f + 56.f});
    positionText->setScale(0.4f);
    root->addChild(positionText);

    auto* positionButton = createTextButton_(
        this,
        "GJ_button_02.png",
        "Configure",
        menu_selector(GhostTextPopup::onConfigurePosition),
        "ghost-text-position"_spr,
        { 138.f, -70.f + 56.f},
        100.f,
        30.f,
        0.62f,
        0.32f,
        1.05f
    ).collect();
    m_rootMenu->addChild(positionButton);

    auto* previewCard = CCScale9Sprite::create("square02b_001.png");
    setScale9VisualSize_(previewCard, { 390.f, 42.f });
    previewCard->setColor({ 45, 20, 10 });
    previewCard->setOpacity(90);
    previewCard->setPosition({ 0.f, -50.f });
    root->addChild(previewCard, -1);

    m_previewLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_previewLabel->setPosition({ 0.f, -50.f });
    m_previewLabel->setScale(0.8f);
    root->addChild(m_previewLabel);

    m_showToggle->toggle(ghosts.isPlayLayerGhostTextEnabled());
    m_flashToggle->toggle(ghosts.isGhostTextFlashOnDeath());

    const float sizeSliderValue = std::clamp(
        (ghosts.getPlayLayerGhostTextScale() - 0.15f) / (1.25f - 0.15f),
        0.f,
        1.f
    );

    m_sizeSlider->setValue(sizeSliderValue);
    m_opacitySlider->setValue(ghosts.getPlayLayerGhostTextOpacity() / 255.f);

    refreshModeButtons_();
    refreshLabels_();
    refreshPreview_();
    scheduleUpdate();
    return true;
}

void GhostTextPopup::update(float) {
    auto& ghosts = Ghosts::I();

    if (m_showToggle &&
        m_showToggle->isToggled() != ghosts.isPlayLayerGhostTextEnabled()
    ) {
        m_showToggle->toggle(ghosts.isPlayLayerGhostTextEnabled());
    }

    if (m_flashToggle &&
        m_flashToggle->isToggled() != ghosts.isGhostTextFlashOnDeath()
    ) {
        m_flashToggle->toggle(ghosts.isGhostTextFlashOnDeath());
    }
    refreshPreview_();
}

void GhostTextPopup::setModeVariantVisible_(const char* id, bool visible) {
    if (!m_rootMenu) return;

    auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(
        m_rootMenu->getChildByIDRecursive(id)
    );

    if (!item) return;

    item->setVisible(visible);
    item->setEnabled(visible);
}

void GhostTextPopup::refreshModeButtons_() {
    const auto mode = Ghosts::I().getGhostTextMode();
    const bool aliveSelected = mode == GhostTextPreset::AliveAttempts;
    const bool deadSelected = mode == GhostTextPreset::DeadAttempts;
    const bool customSelected = mode == GhostTextPreset::Custom;

    setModeVariantVisible_("ghost-text-alive-base"_spr, !aliveSelected);
    setModeVariantVisible_("ghost-text-alive-selected"_spr, aliveSelected);
    setModeVariantVisible_("ghost-text-dead-base"_spr, !deadSelected);
    setModeVariantVisible_("ghost-text-dead-selected"_spr, deadSelected);
    setModeVariantVisible_("ghost-text-custom-base"_spr, !customSelected);
    setModeVariantVisible_("ghost-text-custom-selected"_spr, customSelected);

    if (m_customInput) {
        m_customInput->setVisible(customSelected);
        m_customInput->setEnabled(customSelected);
    }

    // Variables and Reset only apply to the custom format, so keep the whole
    // custom-format strip out of the way for the Alive / Dead presets.
    const auto setCustomButtonVisible = [this, customSelected](const char* id) {
        if (auto* item = typeinfo_cast<CCMenuItem*>(
            m_rootMenu->getChildByIDRecursive(id)
        )) {
            item->setVisible(customSelected);
            item->setEnabled(customSelected);
        }
    };

    setCustomButtonVisible("ghost-text-variables"_spr);
    setCustomButtonVisible("ghost-text-reset"_spr);
}

void GhostTextPopup::refreshLabels_() {
    auto& ghosts = Ghosts::I();

    if (m_sizeLabel) {
        const int percent = static_cast<int>(std::lround(
            ghosts.getPlayLayerGhostTextScale() * 100.f
        ));
        m_sizeLabel->setString(fmt::format("{}%", percent).c_str());
    }

    if (m_opacityLabel) {
        const int percent = static_cast<int>(std::lround(
            ghosts.getPlayLayerGhostTextOpacity() * 100.f / 255.f
        ));
        m_opacityLabel->setString(fmt::format("{}%", percent).c_str());
    }
}

void GhostTextPopup::refreshPreview_() {
    if (!m_previewLabel) return;

    auto& ghosts = Ghosts::I();

    const size_t previewAlive = ghosts.getGhostTextTotalCount() > 0
        ? ghosts.getGhostTextAliveCount()
        : 127;

    const size_t previewDead = ghosts.getGhostTextTotalCount() > 0
        ? ghosts.getGhostTextDeadCount()
        : 123;

    std::string text;

    switch (ghosts.getGhostTextMode()) {
        case GhostTextPreset::AliveAttempts:
            text = fmt::format("Alive Attempts: {}", previewAlive);
            break;

        case GhostTextPreset::DeadAttempts:
            text = fmt::format("Dead Attempts: {}", previewDead);
            break;

        case GhostTextPreset::Custom:
            text = previewFormat_(ghosts.getGhostTextCustomFormat());
            break;
    }

    m_previewLabel->setString(text.c_str());
    m_previewLabel->setOpacity(
        static_cast<GLubyte>(ghosts.getPlayLayerGhostTextOpacity())
    );
    m_previewLabel->setScale(
        std::clamp(ghosts.getPlayLayerGhostTextScale() * 0.9f, 0.22f, 0.8f)
    );
}

void GhostTextPopup::onToggleShow(CCObject*) {
    auto& ghosts = Ghosts::I();
    ghosts.setPlayLayerGhostTextEnabled(
        !ghosts.isPlayLayerGhostTextEnabled()
    );
}

void GhostTextPopup::onToggleFlash(CCObject*) {
    auto& ghosts = Ghosts::I();
    ghosts.setGhostTextFlashOnDeath(
        !ghosts.isGhostTextFlashOnDeath()
    );
}

void GhostTextPopup::onModeAlive(CCObject*) {
    Ghosts::I().setGhostTextMode(GhostTextPreset::AliveAttempts);
    refreshModeButtons_();
    refreshPreview_();
}

void GhostTextPopup::onModeDead(CCObject*) {
    Ghosts::I().setGhostTextMode(GhostTextPreset::DeadAttempts);
    refreshModeButtons_();
    refreshPreview_();
}

void GhostTextPopup::onModeCustom(CCObject*) {
    Ghosts::I().setGhostTextMode(GhostTextPreset::Custom);
    refreshModeButtons_();
    refreshPreview_();
}

void GhostTextPopup::onVariables(CCObject*) {
    FLAlertLayer::create(
        "Ghost Text Variables",
        "<cy>{alive}</c>  attempts still alive\n"
        "<cy>{dead}</c>  attempts that died\n"
        "<cy>{total}</c>  total loaded attempts\n"
        "<cy>{percent_alive}</c>  alive percent\n"
        "<cy>{percent_dead}</c>  dead percent\n"
        "add decimal places with <cy>:#</c>\n"
        "ex: 2 decimal places: <cy>{percent_alive:2}</c>\n",
        "OK"
    )->show();
}

void GhostTextPopup::onResetText(CCObject*) {
    const std::string defaultText = "{alive}/{total} attempts remain";

    Ghosts::I().setGhostTextCustomFormat(defaultText);

    if (m_customInput) {
        m_customInput->setString(defaultText.c_str(), false);
    }

    refreshPreview_();
}

void GhostTextPopup::onConfigurePosition(CCObject*) {
    showNoElasticity_(GhostTextPositionPopup::create());
}

void GhostTextPopup::onSizeSlider(CCObject*) {
    if (!m_sizeSlider) return;

    const float scale = 0.15f +
        std::clamp(m_sizeSlider->getValue(), 0.f, 1.f) * (1.25f - 0.15f);

    Ghosts::I().setPlayLayerGhostTextScale(scale);
    refreshLabels_();
    refreshPreview_();
}

void GhostTextPopup::onOpacitySlider(CCObject*) {
    if (!m_opacitySlider) return;

    const int opacity = static_cast<int>(std::lround(
        std::clamp(m_opacitySlider->getValue(), 0.f, 1.f) * 255.f
    ));

    Ghosts::I().setPlayLayerGhostTextOpacity(opacity);
    refreshLabels_();
    refreshPreview_();
}

// Ghost text position popup

GhostTextPositionPopup* GhostTextPositionPopup::create() {
    auto* popup = new GhostTextPositionPopup();

    if (popup && popup->init(kGhostTextPosW, kGhostTextPosH)) {
        popup->autorelease();
        return popup;
    }

    CC_SAFE_DELETE(popup);
    return nullptr;
}

bool GhostTextPositionPopup::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    setID("ghost-text-position-popup"_spr);
    setTitle("");

    setOpacity(0);

    setChildrenInvisible(m_mainLayer);

    auto* root = CCNode::create();
    root->setPosition(m_mainLayer->getContentSize() * 0.5f);
    m_mainLayer->addChild(root, 100);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    root->addChild(menu);

    auto* xLabel = CCLabelBMFont::create("X", "bigFont.fnt");
    xLabel->setPosition({ -125.f + 30.f, 32.f });
    xLabel->setScale(0.48f);
    root->addChild(xLabel);

    m_xSlider = Slider::create(
        this,
        menu_selector(GhostTextPositionPopup::onXSlider),
        0.9f
    );
    m_xSlider->setPosition({ -80.f + 30.f, 2.f });
    m_xSlider->setScale(0.82f);
    menu->addChild(m_xSlider);

    m_xValueLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_xValueLabel->setAnchorPoint({ 0.f, 0.5f });
    m_xValueLabel->setPosition({ 62.f + 30.f, 32.f });
    m_xValueLabel->setScale(0.42f);
    root->addChild(m_xValueLabel);

    auto* yLabel = CCLabelBMFont::create("Y", "bigFont.fnt");
    yLabel->setPosition({ -125.f + 30.f, -12.f });
    yLabel->setScale(0.48f);
    root->addChild(yLabel);

    m_ySlider = Slider::create(
        this,
        menu_selector(GhostTextPositionPopup::onYSlider),
        0.9f
    );
    m_ySlider->setPosition({ -80.f + 30.f, -42.f });
    m_ySlider->setScale(0.82f);
    menu->addChild(m_ySlider);

    m_yValueLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_yValueLabel->setAnchorPoint({ 0.f, 0.5f });
    m_yValueLabel->setPosition({ 62.f + 30.f, -12.f });
    m_yValueLabel->setScale(0.42f);
    root->addChild(m_yValueLabel);

    auto* resetButton = createTextButton_(
        this,
        "GJ_button_02.png",
        "Reset",
        menu_selector(GhostTextPositionPopup::onReset),
        "ghost-text-position-reset"_spr,
        { -48.f, -58.f },
        82.f,
        30.f,
        0.68f,
        0.34f,
        1.10f
    ).collect();
    menu->addChild(resetButton);

    auto* doneButton = createTextButton_(
        this,
        "GJ_button_01.png",
        "Done",
        menu_selector(GhostTextPositionPopup::onDone),
        "ghost-text-position-done"_spr,
        { 48.f, -58.f },
        82.f,
        30.f,
        0.68f,
        0.34f,
        1.10f
    ).collect();
    menu->addChild(doneButton);

    refreshPositionControls_();
    return true;
}
void GhostTextPositionPopup::onEnter() {
    Popup::onEnter();
    setOpacity(0);
    beginPreviewMode_();
}

void GhostTextPositionPopup::onExit() {
    endPreviewMode_();
    Popup::onExit();
}

void GhostTextPositionPopup::onClose(CCObject* sender) {
    endPreviewMode_();
    Popup::onClose(sender);
}

void GhostTextPositionPopup::hideNodeForPreview_(CCNode* node) {
    if (!node || node == this) return;

    CCNode* branchToEditor = this;
    while (branchToEditor && branchToEditor->getParent() != node) {
        branchToEditor = branchToEditor->getParent();
    }

    if (branchToEditor && branchToEditor->getParent() == node) {
        for (auto* child : node->getChildrenExt()) {
            if (child != branchToEditor) {
                hideNodeForPreview_(child);
            }
        }
        return;
    }

    for (auto const& state : m_hiddenNodes) {
        if (state.node == node) return;
    }

    node->retain();
    m_hiddenNodes.push_back({ node, node->isVisible() });
    node->setVisible(false);
}

void GhostTextPositionPopup::beginPreviewMode_() {
    if (m_previewModeActive) return;
    m_previewModeActive = true;

    auto* director = CCDirector::sharedDirector();
    auto* scene = director ? director->getRunningScene() : nullptr;

    if (scene) {
        // Hide Attempt Playback popup
        hideNodeForPreview_(
            scene->getChildByIDRecursive("playbackModMenu-popup"_spr)
        );
        hideNodeForPreview_(
            scene->getChildByIDRecursive("ghost-settings-popup"_spr)
        );
        hideNodeForPreview_(
            scene->getChildByIDRecursive("ghost-text-popup"_spr)
        );

        // Hide pasuse screen
        if (auto* pauseLayer = findNodeOfTypeRecursive_<PauseLayer>(scene)) {
            hideNodeForPreview_(pauseLayer);
        }
    }

    Ghosts::I().setGhostTextPositionPreview(true);
    refreshPositionControls_();
}

void GhostTextPositionPopup::endPreviewMode_() {
    if (!m_previewModeActive) return;
    m_previewModeActive = false;

    Ghosts::I().setGhostTextPositionPreview(false);

    for (auto it = m_hiddenNodes.rbegin(); it != m_hiddenNodes.rend(); ++it) {
        if (!it->node) continue;

        if (it->node->getParent()) {
            it->node->setVisible(it->wasVisible);
        }

        it->node->release();
    }

    m_hiddenNodes.clear();
}

void GhostTextPositionPopup::refreshPositionControls_() {
    const auto position = Ghosts::I().getPlayLayerGhostTextPos();
    const auto win = CCDirector::sharedDirector()->getWinSize();
    const float maxX = std::max(1.f, win.width);
    const float maxY = std::max(1.f, win.height);

    if (m_xSlider) {
        m_xSlider->setValue(std::clamp(position.x / maxX, 0.f, 1.f));
    }

    if (m_ySlider) {
        m_ySlider->setValue(std::clamp(position.y / maxY, 0.f, 1.f));
    }

    if (m_xValueLabel) {
        m_xValueLabel->setString(fmt::format("{:.0f}", position.x).c_str());
    }

    if (m_yValueLabel) {
        m_yValueLabel->setString(fmt::format("{:.0f}", position.y).c_str());
    }
}

void GhostTextPositionPopup::onXSlider(CCObject*) {
    if (!m_xSlider) return;

    const auto win = CCDirector::sharedDirector()->getWinSize();
    auto position = Ghosts::I().getPlayLayerGhostTextPos();
    position.x = std::clamp(m_xSlider->getValue(), 0.f, 1.f) * win.width;

    Ghosts::I().setPlayLayerGhostTextPos(position);
    refreshPositionControls_();
}

void GhostTextPositionPopup::onYSlider(CCObject*) {
    if (!m_ySlider) return;

    const auto win = CCDirector::sharedDirector()->getWinSize();
    auto position = Ghosts::I().getPlayLayerGhostTextPos();
    position.y = std::clamp(m_ySlider->getValue(), 0.f, 1.f) * win.height;

    Ghosts::I().setPlayLayerGhostTextPos(position);
    refreshPositionControls_();
}

void GhostTextPositionPopup::onDone(CCObject*) {
    onClose(nullptr);
}

void GhostTextPositionPopup::onReset(CCObject*) {
    const auto win = CCDirector::sharedDirector()->getWinSize();

    Ghosts::I().setPlayLayerGhostTextPos(CCPoint(
        std::clamp(2.f, 0.f, win.width),
        std::clamp(310.f, 0.f, win.height)
    ));

    refreshPositionControls_();
}

// Death-marker settings popup

DeathMarkerSettingsPopup* DeathMarkerSettingsPopup::create() {
    auto* popup = new DeathMarkerSettingsPopup();

    if (popup && popup->init(kDeathMarkerW, kDeathMarkerH)) {
        popup->autorelease();
        return popup;
    }

    CC_SAFE_DELETE(popup);
    return nullptr;
}

bool DeathMarkerSettingsPopup::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    setID("death-marker-settings-popup"_spr);
    setTitle("Death Markers");

    auto& ghosts = Ghosts::I();

    auto* root = CCNode::create();
    root->setPosition(m_mainLayer->getContentSize() * 0.5f);
    m_mainLayer->addChild(root);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    root->addChild(menu);

    m_showToggle = makeToggle_(
        this,
        menu_selector(DeathMarkerSettingsPopup::onToggleShow)
    );
    m_showToggle->setPosition({ -140.f, 70.f });
    m_showToggle->setScale(0.70f);
    menu->addChild(m_showToggle);

    auto* showLabel = CCLabelBMFont::create(
        "Show Death Markers",
        "bigFont.fnt"
    );
    showLabel->setAnchorPoint({ 0.f, 0.5f });
    showLabel->setPosition({ -125.f, 70.f });
    showLabel->setScale(0.43f);
    root->addChild(showLabel);

    constexpr float rowX = -145.f;
    constexpr float valueX = 116.f;
    constexpr float sliderX = -36.f;
    constexpr float sliderYOffset = -41.f;

    constexpr float sizeY = 37.f;
    constexpr float thicknessY = 3.f;
    constexpr float opacityY = -31.f;

    constexpr float textScale = 0.43f;
    constexpr float valueScale = 0.35f;
    constexpr float sliderScale = 0.75f;

    // Size
    auto* sizeText = CCLabelBMFont::create("Size", "bigFont.fnt");
    sizeText->setAnchorPoint({ 0.f, 0.5f });
    sizeText->setPosition({ rowX, sizeY });
    sizeText->setScale(textScale);
    root->addChild(sizeText);

    m_sizeSlider = Slider::create(
        this,
        menu_selector(DeathMarkerSettingsPopup::onSizeSlider),
        0.75f
    );
    m_sizeSlider->setPosition({
        sliderX,
        sizeY + sliderYOffset
    });
    m_sizeSlider->setScale(sliderScale);
    menu->addChild(m_sizeSlider);

    m_sizeLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_sizeLabel->setPosition({ valueX, sizeY });
    m_sizeLabel->setScale(valueScale);
    root->addChild(m_sizeLabel);

    // Thickness
    auto* thicknessText = CCLabelBMFont::create("Thickness", "bigFont.fnt");
    thicknessText->setAnchorPoint({ 0.f, 0.5f });
    thicknessText->setPosition({ rowX, thicknessY });
    thicknessText->setScale(textScale);
    root->addChild(thicknessText);

    m_thicknessSlider = Slider::create(
        this,
        menu_selector(DeathMarkerSettingsPopup::onThicknessSlider),
        0.75f
    );
    m_thicknessSlider->setPosition({
        sliderX,
        thicknessY + sliderYOffset
    });
    m_thicknessSlider->setScale(sliderScale);
    menu->addChild(m_thicknessSlider);

    m_thicknessLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_thicknessLabel->setPosition({ valueX, thicknessY });
    m_thicknessLabel->setScale(valueScale);
    root->addChild(m_thicknessLabel);

    // Opacity
    auto* opacityText = CCLabelBMFont::create("Opacity", "bigFont.fnt");
    opacityText->setAnchorPoint({ 0.f, 0.5f });
    opacityText->setPosition({ rowX, opacityY });
    opacityText->setScale(textScale);
    root->addChild(opacityText);

    m_opacitySlider = Slider::create(
        this,
        menu_selector(DeathMarkerSettingsPopup::onOpacitySlider),
        0.75f
    );
    m_opacitySlider->setPosition({
        sliderX,
        opacityY + sliderYOffset
    });
    m_opacitySlider->setScale(sliderScale);
    menu->addChild(m_opacitySlider);

    m_opacityLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_opacityLabel->setPosition({ valueX, opacityY });
    m_opacityLabel->setScale(valueScale);
    root->addChild(m_opacityLabel);

    auto* previewCard = CCScale9Sprite::create("square02b_001.png");
    setScale9VisualSize_(previewCard, { 300.f, 42.f });
    previewCard->setColor({ 45, 20, 10 });
    previewCard->setOpacity(80);
    previewCard->setPosition({ 0.f, -68.f });
    root->addChild(previewCard, -1);

    m_preview = CCDrawNode::create();
    m_preview->setPosition({ 0.f, -68.f });
    root->addChild(m_preview);

    m_showToggle->toggle(ghosts.areDeathMarkersEnabled());

    m_sizeSlider->setValue(
        std::clamp(
            (ghosts.getDeathMarkerSize() - 4.f) / (40.f - 4.f),
            0.f,
            1.f
        )
    );

    m_thicknessSlider->setValue(
        std::clamp(
            (ghosts.getDeathMarkerThickness() - 1.f) / (8.f - 1.f),
            0.f,
            1.f
        )
    );

    m_opacitySlider->setValue(ghosts.getDeathMarkerOpacity() / 255.f);

    refreshLabels_();
    refreshPreview_();
    scheduleUpdate();
    return true;
}

void DeathMarkerSettingsPopup::update(float) {
    refreshLabels_();
    refreshPreview_();
}

void DeathMarkerSettingsPopup::refreshLabels_() {
    auto& ghosts = Ghosts::I();

    if (m_thicknessLabel) {
        m_thicknessLabel->setString(
            fmt::format("{:.1f}px", ghosts.getDeathMarkerThickness()).c_str()
        );
    }

    if (m_sizeLabel) {
        m_sizeLabel->setString(
            fmt::format("{:.0f}px", ghosts.getDeathMarkerSize()).c_str()
        );
    }

    if (m_opacityLabel) {
        const int percent = static_cast<int>(std::lround(
            ghosts.getDeathMarkerOpacity() * 100.f / 255.f
        ));

        m_opacityLabel->setString(fmt::format("{}%", percent).c_str());
    }
}

void DeathMarkerSettingsPopup::refreshPreview_() {
    if (!m_preview) return;

    auto& ghosts = Ghosts::I();

    m_preview->clear();

    drawDeathMarkerShape_(
        m_preview,
        { 0.f, 0.f },
        ghosts.getDeathMarkerSize() * 1.15f,
        ghosts.getDeathMarkerThickness(),
        ghosts.getDeathMarkerOpacity() / 255.f
    );
}

void DeathMarkerSettingsPopup::onToggleShow(CCObject*) {
    auto& ghosts = Ghosts::I();
    ghosts.setDeathMarkersEnabled(!ghosts.areDeathMarkersEnabled());
}

void DeathMarkerSettingsPopup::onSizeSlider(CCObject*) {
    if (!m_sizeSlider) return;

    const float size = 4.f +
        std::clamp(m_sizeSlider->getValue(), 0.f, 1.f) * 36.f;

    Ghosts::I().setDeathMarkerSize(size);
    refreshLabels_();
    refreshPreview_();
}

void DeathMarkerSettingsPopup::onThicknessSlider(CCObject*) {
    if (!m_thicknessSlider) return;

    const float thickness = 1.f +
        std::clamp(m_thicknessSlider->getValue(), 0.f, 1.f) * 7.f;

    Ghosts::I().setDeathMarkerThickness(thickness);
    refreshLabels_();
    refreshPreview_();
}

void DeathMarkerSettingsPopup::onOpacitySlider(CCObject*) {
    if (!m_opacitySlider) return;

    const int opacity = static_cast<int>(std::lround(
        std::clamp(m_opacitySlider->getValue(), 0.f, 1.f) * 255.f
    ));

    Ghosts::I().setDeathMarkerOpacity(opacity);
    refreshLabels_();
    refreshPreview_();
}
