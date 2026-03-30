// playback_mod_menu_popup.cpp
#include "playback_mod_menu_popup.hpp"

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/Log.hpp>

#include <Geode/ui/Layout.hpp>
#include <Geode/ui/General.hpp>

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/general.hpp>

#include <Geode/ui/BasedButtonSprite.hpp>
#include <UIBuilder.hpp>
#include "ghost_distance_popup.hpp"
#include "color_selector_popup.hpp"
#include "../core/types.hpp"
#include "../utils/ui_utils.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <cctype>
#include <iomanip>
#include <type_traits>
#include <utility>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;

namespace replay_ui_detail {
    inline bool getFallbackFlag() {
        return Mod::get()->getSavedValue<bool>("apx-replay-ui-active");
    }

    inline void setFallbackFlag(bool v) {
        Mod::get()->setSavedValue("apx-replay-ui-active", v);
    }

    inline bool isReplayingRuntime() {
        auto& G = Ghosts::I();
        return G.isReplaying();
    }

    inline void stopReplayRuntime() {
        auto& G = Ghosts::I();
        G.stopReplay();
        G.restartLevel();
        G.setRecordingEnabled(true);
        Mod::get()->setSavedValue("recording-runtime", true);
        G.setActiveGhostsInvisible();
    }
}

namespace {
    constexpr float kPopupW = 600.f;
    constexpr float kPopupH = 340.f;
    constexpr float kPreloadPopupW = 300.f;
    constexpr float kPreloadPopupH = 200.f;
    constexpr float kPlaybackSettingsW = 300.f;
    constexpr float kPlaybackSettingsH = 200.f;

    constexpr float kAttemptMbCost = 0.45f;
}

template <class T>static void showPopupNoElasticity_(T* popup) {
    if (!popup) return;
    popup->m_noElasticity = true;
    popup->show();
}

PlaybackModMenu* PlaybackModMenu::create() {
    auto ret = new PlaybackModMenu();
    if (ret && ret->init(kPopupW, kPopupH)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}


bool PlaybackModMenu::init(float width, float height) {
    if (!Popup::init(width, height))
        return false;
    this->setID("playbackModMenu-popup"_spr);

    setChildrenInvisible(m_mainLayer);

    setTitle("");

    // Murder the default close button
    if (CCMenuItemSpriteExtra* close = findDefaultCloseButton(m_mainLayer)) {
        close->stopAllActions();
        close->setVisible(false);
        close->setEnabled(false);
    }

    buildTemplateUI_();

    refreshToggles_();
    refreshColorsLabel_();

    int v = 255;
    auto* mod = Mod::get();
    if (mod->hasSavedValue("ghost-opacity"))
        v = (int)mod->getSavedValue<int64_t>("ghost-opacity");
    v = std::clamp(v, 0, 255);

    if (m_opacitySlider) {
        m_opacitySlider->setValue(v / 255.f);
    }
    if (m_opacityLabel) {
        m_opacityLabel->setString(("Ghost Opacity (" + std::to_string((int)(v/2.55)) + ")").c_str());
    }
    Ghosts::I().setOpacityVar((GLubyte)v);

    cacheReplayButtons_();
    m_lastReplayingState = queryIsReplaying_();
    refreshReplayButtons_();

    this->scheduleUpdate();

    return true;
}

void PlaybackModMenu::syncUIFromRuntime() {
    refreshToggles_();
    refreshColorsLabel_();

    int v = 255;
    auto* mod = Mod::get();
    if (mod->hasSavedValue("ghost-opacity"))
        v = (int)mod->getSavedValue<int64_t>("ghost-opacity");

    v = std::clamp(v, 0, 255);

    if (m_opacitySlider) m_opacitySlider->setValue(v / 255.f);
    if (m_opacityLabel)  m_opacityLabel->setString(("Ghost Opacity (" + std::to_string((int)(v/2.55)) + ")").c_str());

    cacheReplayButtons_();
    refreshReplayButtons_();
}

CCMenuItemToggler* mkToggler(CCObject* target, SEL_MenuHandler sel) {
    auto on  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
    auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    auto toggler = CCMenuItemToggler::create(off, on, target, sel);
    toggler->setSizeMult(kPopupButtonSizeMult);
    return toggler;
}

void PlaybackModMenu::buildTemplateUI_() {
    m_mainLayer->setLayout(AnchorLayout::create());

    CCSprite* gradient = createFullscreenGradient_();

    Build<CCMenu>::create()
        .contentSize(0.f, 0.f)
        .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
        .children(
            Build<CCSprite>(gradient)
                .color(100, 86, 255)
                .anchorPoint(0.5f, 0.5f)
                .zOrder(-1)
                .scaleX(38.f).scaleY(2.f),

            Build<CCLabelBMFont>::create("Attempt Playback Mod", "bigFont.fnt")
                .pos(0.f, 140.f)
                .anchorPoint(0.5f, 0.5f),

            Build<CCMenu>::create()
                .id("playbackButtonsRow"_spr)
                .pos(0.f, 94.f)
                .contentSize(485.f, 40.f)
                .layout(Build<AxisLayout>::create()
                    .axis(Axis::Row)
                    .gap(54.f)
                    .autoScale(false)
                    .autoGrow(0.f))
                .children(
                    createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Replay Best",
                    /*callback*/menu_selector(PlaybackModMenu::onReplayBest),
                    /*node_id*/"replay-best-button"_spr,
                    /*pos*/{0.f, 0.f},
                    /*width*/190.f,
                    /*height*/40.f,
                    /*spriteScale*/1.f,
                    /*labelScale*/0.8f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ),

                    createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Replay Practice",
                    /*callback*/menu_selector(PlaybackModMenu::onReplayPractice),
                    /*node_id*/"replay-practice-button"_spr,
                    /*pos*/{0.f, 0.f},
                    /*width*/241.f,
                    /*height*/40.f,
                    /*spriteScale*/1.f,
                    /*labelScale*/0.8f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ))
                .updateLayout()
                .ignoreAnchorPointForPos(false)
                .anchorPoint(0.5f, 0.5f),
            
            createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Stop Replay",
                    /*callback*/menu_selector(PlaybackModMenu::onStopReplay),
                    /*node_id*/"stop-replay-button"_spr,
                    /*pos*/{0.f, 94.f},
                    /*width*/485.f,
                    /*height*/40.f,
                    /*spriteScale*/1.f,
                    /*labelScale*/0.8f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ).visible(false),

            Build<CCLabelBMFont>::create("Ghost Opacity (100)", "bigFont.fnt")
                        .scale(0.5f)
                        .pos(125.f, 60.f)
                        .store(m_opacityLabel),

            Build<Slider>::create(this, menu_selector(PlaybackModMenu::onSliderInt), 0.9f)
                        .id("ghost-opacity"_spr)
                        .scale(0.78f)
                        .pos(125.f, 40.f)
                        .anchorPoint(0.f, 0.f)
                        .store(m_opacitySlider),

            Build<CCMenu>::create()
                .id("togglesColumn"_spr)
                .pos(-240.f, 63.f)
                .contentSize(35.f, 74.f)
                .layout(Build<AxisLayout>::create()
                    .axis(Axis::Column)
                    .reverse(true)
                    .autoScale(false)
                    .autoGrow(0.f))
                .children(
                    Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackModMenu::onToggleRandomIcons)))
                        .id("RandomIconsToggle"_spr)
                        .store(m_tgRandomIcons)
                        .scale(1.f)
                        .children(
                            Build<CCLabelBMFont>::create("Random Icons", "bigFont.fnt")
                                .pos(38.f, 5.f)
                                .anchorPoint(0.f, 0.f)
                                .scale(0.725f)
                        ),
                    Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackModMenu::onToggleGhostsExplodeSFX)))
                        .id("GhostsExplodeSFXToggle"_spr)
                        .store(m_tgGhostsExplodeSFX)
                        .scale(1.f)
                        .children(
                            Build<CCLabelBMFont>::create("Ghost Death Sound", "bigFont.fnt")
                                .pos(38.f, 5.f)
                                .anchorPoint(0.f, 0.f)
                                .scale(0.725f)
                        ),
                    Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackModMenu::onToggleGhostsExplode)))
                        .id("GhostsExplodeToggle"_spr)
                        .store(m_tgGhostsExplode)
                        .scale(1.f)
                        .children(
                            Build<CCLabelBMFont>::create("Ghosts Explode", "bigFont.fnt")
                                .pos(38.f, 5.f)
                                .anchorPoint(0.f, 0.f)
                                .scale(0.725f)
                        ),
                    Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackModMenu::onToggleBlockRecording)))
                        .id("BlockRecordingToggle"_spr)
                        .store(m_tgBlockRecording)
                        .scale(1.f)
                        .children(
                            Build<CCLabelBMFont>::create("Disable Recording", "bigFont.fnt")
                                .pos(38.f, 5.f)
                                .anchorPoint(0.f, 0.f)
                                .scale(0.725f)
                        )
                    
                    )
                .updateLayout()
                .ignoreAnchorPointForPos(false)
                .anchorPoint(0.f, 1.f),

                

            Build<CCLabelBMFont>::create("Mod by manshoety", "bigFont.fnt")
                .pos(-275.f, -150.f)
                .anchorPoint(0.f, 0.f)
                .scale(0.4f),

            Build<CCLabelBMFont>::create("Beta 1.4.22", "bigFont.fnt")
                .pos(195.f, 136.f)
                .anchorPoint(0.f, 0.5f)
                .scale(0.475f),

            Build<EditorButtonSprite>::create(
                        Build<CCSprite>::createSpriteName("geode.loader/settings.png"),
                        EditorBaseColor::Gray,
                        EditorBaseSize::Normal
                    )
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onOpenPlaybackSettings))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .pos(-260.f, -118.f)
                        .scale(0.8f)/*not sure how to fix base scale being wrong when unpressed*/
                        .scaleMult(kPopupButtonSizeMult)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("PlaybackSettingsButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Playback\nSettings", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                        ),

            Build<CCMenu>::create()
                .id("MoreOptionsColumn"_spr)
                .pos(168.f, -149.f)
                .contentSize(234.5f, 146.9187f)
                .scale(0.8f)
                .layout(Build<AxisLayout>::create()
                    .axis(Axis::Column)
                    .reverse(true)
                    .autoScale(false)
                    .autoGrow(0.f))
                .children(
                    Build<CCLabelBMFont>::create("More Options", "bigFont.fnt"),

                    Build<EditorButtonSprite>::create(
                        Build<CCSprite>::createSpriteName("geode.loader/update.png"),
                        EditorBaseColor::LightBlue,
                        EditorBaseSize::Normal
                    )
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onCycleGhostColors))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .scaleMult(kPopupButtonSizeMult)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("ColorModeButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Color Mode: Player", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                                .store(m_colorModeLabel)
                        ),
                    Build<EditorButtonSprite>::create(
                        Build<CCSprite>::createSpriteName("GJ_colorBtn_001.png"),
                        EditorBaseColor::LightBlue,
                        EditorBaseSize::Normal
                    )
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onOpenColorSelector))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .scaleMult(kPopupButtonSizeMult)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("AllowedRandomColorsButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Color Selector", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                        ),
                    Build<EditorButtonSprite>::create(
                        Build<CCSprite>::createSpriteName("geode.loader/settings.png"),
                        EditorBaseColor::LightBlue,
                        EditorBaseSize::Normal
                    )
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onOpenGhostDistance))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .scaleMult(kPopupButtonSizeMult)
                        .pos(-100.f, 0)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("GhostDistanceButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Ghost Distance", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                        ),

                    
                    Build<CCSprite>::createSpriteName("geode.loader/tag-paid.png")
                        .scale(0.775f)
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onFreeRobux))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .scaleMult(kPopupButtonSizeMult)
                        .pos(-100.f, 0)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("FreeRobuxButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Free Robux", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                        ),
                        
                    Build<CircleButtonSprite>::create(Build<CCSprite>::createSpriteName("edit_delBtn_001.png"), CircleBaseColor::Gray, CircleBaseSize::Small)
                        .scale(0.775f)
                        .intoMenuItem(this, menu_selector(PlaybackModMenu::onDeleteSaveFile))
                        .ignoreAnchorPointForPos(false)
                        .anchorPoint(0.5f, 0.5f)
                        .scaleMult(kPopupButtonSizeMult)
                        .pos(-100.f, 0)
                        .layoutOpts(Build<AxisLayoutOptions>::create()
                            .crossAlign(AxisAlignment::Start)
                        )
                        .id("DeleteSaveFileButton"_spr)
                        .children(
                            Build<CCLabelBMFont>::create("Delete Attempts", "bigFont.fnt")
                                .alignment(kCCTextAlignmentCenter)
                                .pos(35.f, 17.f)
                                .anchorPoint(0.f, 0.5f)
                                .scale(0.625f)
                        ))
                .updateLayout()
                .ignoreAnchorPointForPos(false)
                .anchorPoint(0.5f, 0.f),

            Build(CircleButtonSprite::createWithSpriteFrameName(
                "geode.loader/close.png",
                0.85f,
                CircleBaseColor::Green
            ))
                .intoMenuItem(this, menu_selector(PlaybackModMenu::onExitButton))
                .ignoreAnchorPointForPos(false)
                .anchorPoint(0.f, 1.f)
                .scaleMult(0.8)
                .id("ExitButton"_spr)
                .pos(-283.f, 158.f)
                .scale(0.75f)
        )
        .updateLayout()
        .ignoreAnchorPointForPos(false)
        .anchorPoint(0.5f, 0.5f)
        .parentAtPos(m_mainLayer, Anchor::Center)
        .collect();

        scaleUIForThatOneTabletUser(kPopupW, kPopupH);

        cacheReplayButtons_();
        refreshReplayButtons_();
}

void PlaybackModMenu::scaleUIForThatOneTabletUser(float designWidth, float designHeight) {
    if (!m_mainLayer) return;

    const auto size = fitPopupToWindow_(designWidth, designHeight);
    const float scale = computeFitScale_(size.width, size.height, designWidth, designHeight);

    m_mainLayer->setScale(scale);
}

void PlaybackModMenu::onReplayBest(CCObject*) {
    replay_ui_detail::setFallbackFlag(true);

    if (auto* p = PreloadAttemptsPopup::create(ReplayKind::BestSingle)) {
        p->m_noElasticity = true;
        p->show();
    } else {
        replay_ui_detail::setFallbackFlag(false);
        this->syncUIFromRuntime();
    }
}

void PlaybackModMenu::onReplayPractice(CCObject*) {
    replay_ui_detail::setFallbackFlag(true);

    if (auto* p = PreloadAttemptsPopup::create(ReplayKind::PracticeComposite)) {
        p->m_noElasticity = true;
        p->show();
    } else {
        replay_ui_detail::setFallbackFlag(false);
        this->syncUIFromRuntime();
    }
}

void PlaybackModMenu::cacheReplayButtons_() {
    if (!m_mainLayer) return;
    m_playbackButtonsRow = typeinfo_cast<CCMenu*>(m_mainLayer->getChildByIDRecursive("playbackButtonsRow"_spr));
    m_stopReplayBtn = typeinfo_cast<CCMenuItem*>(m_mainLayer->getChildByIDRecursive("stop-replay-button"_spr));
}

bool PlaybackModMenu::queryIsReplaying_() const {
    return replay_ui_detail::isReplayingRuntime();
}

void PlaybackModMenu::refreshReplayButtons_() {
    cacheReplayButtons_();

    bool replaying = queryIsReplaying_();

    if (m_suppressReplayPollFrames > 0) {
        replaying = false;
    }

    if (!replaying && Mod::get()->getSavedValue<bool>("apx-replay-ui-active")) {
        replay_ui_detail::setFallbackFlag(false);
    }

    if (m_playbackButtonsRow) {
        m_playbackButtonsRow->setVisible(!replaying);

        for (CCNode* node : m_playbackButtonsRow->getChildrenExt()) {
            if (auto* mi = typeinfo_cast<CCMenuItem*>(node)) {
                mi->setEnabled(!replaying);
            }
        }
    }

    if (m_stopReplayBtn) {
        m_stopReplayBtn->setVisible(replaying);
        m_stopReplayBtn->setEnabled(replaying);
    }

    m_lastReplayingState = replaying;
}

void PlaybackModMenu::onStopReplay(CCObject*) {
    replay_ui_detail::stopReplayRuntime();
    replay_ui_detail::setFallbackFlag(false);

    m_suppressReplayPollFrames = 12;
    m_lastReplayingState = false;

    refreshReplayButtons_();

    refreshToggles_();
    refreshColorsLabel_();
}

void PlaybackModMenu::update(float dt) {
    if (m_suppressReplayPollFrames > 0) {
        --m_suppressReplayPollFrames;
        if (m_suppressReplayPollFrames == 0) {
            refreshReplayButtons_();
        }
        return;
    }

    const bool now = queryIsReplaying_();
    if (now != m_lastReplayingState) {
        refreshReplayButtons_();
    }
}

void PlaybackModMenu::onDeleteSaveFile(CCObject*) {
    auto& G = Ghosts::I();

    if (!G.hasModAttachedToLevel()) {
        FLAlertLayer::create(
            "Error",
            gd::string("Not currently in a level"),
            "OK"
        )->show();
        return;
    }

    std::string filename = G.getCurrentLevelSaveFileName();
    if (filename.empty()) {
        FLAlertLayer::create(
            "Error",
            gd::string("No save file found for this level"),
            "OK"
        )->show();
        return;
    }

    std::ostringstream ss;
    ss
        << "Are you sure you want to delete all saved attempts for this level?\n\n"
        << "<cy>File: " << filename << "</c>\n\n"
        << "This action cannot be undone!";

    gd::string msg(ss.str().c_str());

    createQuickPopup(
        "Confirm Delete",
        msg,
        "Cancel", "Delete",
        [this](auto, bool btn2) {
            if (!btn2) return;

            auto& G = Ghosts::I();
            G.restartLevel();
            bool ok = G.deleteCurrentLevelSaveFile();

            if (ok) {
                FLAlertLayer::create(
                    "Success",
                    gd::string("All saved attempts for this level have been deleted.\n\n"
                            "Recording has been restarted."),
                    "OK"
                )->show();

                if (auto* scene = CCDirector::sharedDirector()->getRunningScene()) {
                    if (auto* menu = typeinfo_cast<PlaybackModMenu*>(
                            scene->getChildByIDRecursive("playbackModMenu-popup"_spr))) {
                        menu->syncUIFromRuntime();
                    }
                }
            } else {
                FLAlertLayer::create(
                    "Error",
                    gd::string("Failed to delete the save file.\n"
                            "Check the logs for more information."),
                    "OK"
                )->show();
            }
        }
    );
}

void PlaybackModMenu::onToggleRecording(CCObject*) {
    auto& G = Ghosts::I();
    bool want = !G.isRecording();
    G.setRecordingEnabled(want);
    Mod::get()->setSavedValue("recording-runtime", want);
}

void PlaybackModMenu::onToggleInterpolation(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isInterpolationEnabled();
    G.setInterpolationEnabled(on);
    Mod::get()->setSavedValue("interp-enabled", on);
    Mod::get()->setSettingValue("interp-enabled", on);
}

void PlaybackModMenu::onToggleRandomIcons(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isRandomIconsEnabled();
    Mod::get()->setSavedValue("randomIcons-enabled", on);
    Mod::get()->setSettingValue("randomIcons-enabled", on);
    G.setRandomIconsEnabled(on);
}

void PlaybackModMenu::onToggleBlockRecording(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isRecordingBlocked();
    Mod::get()->setSavedValue("recording-blocked", on);
    Mod::get()->setSettingValue("recording-blocked", on);
    G.setRecordingBlocked(on);
}

void PlaybackModMenu::onToggleGhostsExplodeSFX(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isGhostsExplodeSFXEnabled();
    G.setGhostsExplodeSFXEnabled(on);
    Mod::get()->setSavedValue("ghosts-explode-sfx", on);
    Mod::get()->setSettingValue("ghosts-explode-sfx", on);
}

void PlaybackModMenu::onToggleGhostsExplode(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isGhostsExplodeEnabled();

    // Test if the player has death effects disabled (doesn't currently work so always show popup)
    if (on) {
        FLAlertLayer::create(
                    "Warning",
                    gd::string("Please enable player death effects in your mod menu!\n"
                            "This feature doesn't work otherwise.\n"
                        "Also, you may lag with this option enabled."),
                    "OK"
                )->show();
        // PlayLayer* m_pl = G.getPlayLayer();
        // log::info("Enable death effect");
        /*
        if (m_pl) {
            PlayerObject* po = Build<PlayerObject>::create(0, 0, m_pl, m_pl->m_objectLayer, false);
            log::info("po opacity: {}, isdead: {}, ishidden: {}", po->getOpacity(), po->m_isDead, po->m_isHidden);
            po->setOpacity(255);
            log::info("po opacity: {}, isdead: {}, ishidden: {}", po->getOpacity(), po->m_isDead, po->m_isHidden);
            po->playDeathEffect();
            // po->playerDestroyed(false);
            log::info("po opacity: {}, isdead: {}, ishidden: {}", po->getOpacity(), po->m_isDead, po->m_isHidden);
            if (po->getOpacity() == 255) {
                FLAlertLayer::create(
                    "Warning",
                    gd::string("Please enable player death effects in your mod menu!\n"
                            "This feature doesn't work otherwise."),
                    "OK"
                )->show();
            }
            po = nullptr;
        }
            */
    }

    G.setGhostsExplodeEnabled(on);
    Mod::get()->setSavedValue("ghosts-explode", on);
    Mod::get()->setSettingValue("ghosts-explode", on);
}

void PlaybackModMenu::onToggleReplayPreventCompletion(CCObject*) {
    auto& G = Ghosts::I();
    bool on = !G.isReplayPreventCompletionEnabled();
    G.setReplayPreventCompletionEnabled(on);
    Mod::get()->setSavedValue("ReplayPreventCompletion-enabled", on);
    Mod::get()->setSettingValue("ReplayPreventCompletion-enabled", on);
}

void PlaybackModMenu::onCycleGhostColors(CCObject*) {
    static std::vector<std::string> modes{ "PlayerColors", "Random", "White", "Black" };
    auto cur = Mod::get()->getSavedValue<std::string>("ghost-colors");
    auto it = std::find(modes.begin(), modes.end(), cur);
    size_t i = (it == modes.end()) ? 0 : (size_t(it - modes.begin()) + 1) % modes.size();
    auto next = modes[i];
    Mod::get()->setSavedValue("ghost-colors", next);

    ColorMode cm = ColorMode::PlayerColors;
    if (next == "Random") cm = ColorMode::Random;
    else if (next == "White") cm = ColorMode::White;
    else if (next == "Black") cm = ColorMode::Black;

    Ghosts::I().setColorMode(cm);
    refreshColorsLabel_();
}

void PlaybackModMenu::onOpenColorSelector(CCObject*) {
    if (auto* p = ColorSelectorPopup::create()) {
        p->m_noElasticity = true;
        p->show();
    }
}

void PlaybackModMenu::onOpenGhostDistance(CCObject*) {
    if (auto* p = GhostDistancePopup::create()) {
        p->m_noElasticity = true;
        p->show();
    }
}

void PlaybackModMenu::onOpenPlaybackSettings(CCObject*) {
    if (auto* p = PlaybackSettingsPopup::create()) {
        p->m_noElasticity = true;
        p->show();
    }
}

void PlaybackModMenu::onFreeRobux(CCObject*) {
    // This might turn into a fun settings menu for goofy stuff
    FLAlertLayer::create("Attention", "Free Robux will be added in a later update", "I WANT THEM NOW")->show();
}

void PlaybackModMenu::onExitButton(CCObject*) {
    Ghosts::I().updateOpacityAll();
    this->onClose(nullptr);
}

void PlaybackModMenu::onSliderInt(CCObject*) {
    if (!m_opacitySlider) return;

    float t = std::clamp(m_opacitySlider->getValue(), 0.f, 1.f);

    int v = (int)std::lround(t * 255.f);
    v = std::clamp(v, 0, 255);

    Mod::get()->setSavedValue("ghost-opacity", v);
    Ghosts::I().setOpacityVar((GLubyte)v);

    if (m_opacityLabel) {
        m_opacityLabel->setString(("Ghost Opacity (" + std::to_string((int)(v/2.55)) + ")").c_str());
    }
}

std::pair<std::string, float> PlaybackModMenu::sliderKeyAndValue_(CCObject* obj) {
    auto* n = typeinfo_cast<cocos2d::CCNode*>(obj);
    while (n && !typeinfo_cast<Slider*>(n)) n = n->getParent();
    if (!n) return {"", 0.f};

    auto* s = static_cast<Slider*>(n);
    const float t = std::clamp(s->getThumb()->getValue(), 0.f, 1.f);
    return { s->getID(), t };
}

void PlaybackModMenu::refreshToggles_() {
    auto& G = Ghosts::I();
    if (m_tgRecording) m_tgRecording->toggle(G.isRecording());
    if (m_tgInterp) m_tgInterp->toggle(G.isInterpolationEnabled());
    if (m_tgRandomIcons) m_tgRandomIcons->toggle(G.isRandomIconsEnabled());
    if (m_tgGhostsExplode) m_tgGhostsExplode->toggle(G.isGhostsExplodeEnabled());
    if (m_tgGhostsExplodeSFX) m_tgGhostsExplodeSFX->toggle(G.isGhostsExplodeSFXEnabled());
    if (m_tgBlockRecording) m_tgBlockRecording->toggle(G.isRecordingBlocked());
    if (m_tgReplayPreventCompletion) m_tgReplayPreventCompletion->toggle(G.isReplayPreventCompletionEnabled());
}

void PlaybackModMenu::refreshColorsLabel_() {
    auto s = Mod::get()->getSavedValue<std::string>("ghost-colors");
    std::string nice = "Player";
    if (s == "random") nice = "Random";
    else if (s == "Random") nice = "Random";
    else if (s == "White") nice = "White";
    else if (s == "Black") nice = "Black";

    if (m_colorModeLabel) {
        m_colorModeLabel->setString(("Color Mode: " + nice).c_str());
    }
}

PreloadAttemptsPopup* PreloadAttemptsPopup::create(ReplayKind kind) {
    auto ret = new PreloadAttemptsPopup();
    ret->m_kind = kind;

    if (ret && ret->init(kPreloadPopupW, kPreloadPopupH)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool PreloadAttemptsPopup::init(float width, float height) {
    if (!Popup::init(width, height))
        return false;

    this->setID("preload-attempts-popup"_spr);
    setTitle("");

    auto& G = Ghosts::I();

    if (m_kind == ReplayKind::BestSingle) {
        G.setUseCheckpointsRoute(false);
        G.startReplayBest();
    }
    else {
        G.setUseCheckpointsRoute(true);
        G.startReplayPractice();
    }

    m_replayOn = true;

    m_mainLayer->setLayout(AnchorLayout::create());

    m_totalAttempts = Ghosts::I().getTotalAttemptsCount();

    if (m_totalAttempts <= 0) {
        Ghosts::I().cancelPreloadAttempts(false);
        Ghosts::I().stopReplay();
        Ghosts::I().setActiveGhostsInvisible();

        replay_ui_detail::setFallbackFlag(false);

        if (m_kind == ReplayKind::PracticeComposite) {
            FLAlertLayer::create(
                "No Attempts",
                gd::string("There are no recorded practice attempts for this level or for this start position"),
                "OK"
            )->show();
        } else {
            FLAlertLayer::create(
                "No Attempts",
                gd::string("There are no recorded normal mode attempts either for this level or for this start position"),
                "OK"
            )->show();
        }

        return false;
    }

    int64_t saved = Mod::get()->getSavedValue<int64_t>("preload-attempts");
    int startVal = (saved > 0) ? (int)saved : 500;
    if (m_totalAttempts > 0) startVal = std::clamp(startVal, 0, m_totalAttempts);
    else startVal = std::max(startVal, 0);

    Build<CCNode>::create()
        .contentSize(0.f, 0.f)
        .anchorPoint(0.5f, 0.5f)
        .layoutOpts(Build<AnchorLayoutOptions>::create()
            .anchor(Anchor::Center))
        .children(
            // BG
            Build<CCScale9Sprite>::create("GJ_square01.png")
                .contentSize(300.f, 200.f)
                .anchorPoint(0.5f, 0.5f)
                .zOrder(-1),

            Build<CCLabelBMFont>::create("Preload Attempts", "bigFont.fnt")
                .id("TitleLabel"_spr)
                .pos(0.f, 83.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.825f),

            Build<CCLabelBMFont>::create("Load 0 of 0", "bigFont.fnt")
                .id("NumAttemptsLoadingLabel"_spr)
                .pos(0.f, -27.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.525f)
                .store(m_numAttemptsLoadingLabel),

            
            Build<CCLabelBMFont>::create("(Estimated RAM: 0MB)", "bigFont.fnt")
                .id("EstimatedRAMLabel"_spr)
                .pos(0.f, -43.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.425f)
                .store(m_estimatedRamLabel),

            Build<TextInput>::create(200.f, "Type number", "bigFont.fnt")
                .$build_wrap(setString(std::to_string(startVal)))
                .id("numberTextInput"_spr)
                .store(m_numAttemptsToLoad)
                .contentSize(200.f, 30.f)
                .anchorPoint(0.5f, 0.5f)
        )
        .parentAtPos(m_mainLayer, Anchor::Center)
        .collect();

    Build<CCMenu>::create()
        .contentSize(0.f, 0.f)
        .anchorPoint(0.5f, 0.5f)
        .ignoreAnchorPointForPos(false)
        .layoutOpts(Build<AnchorLayoutOptions>::create()
            .anchor(Anchor::Center))
        .children(
            createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Load",
                    /*callback*/menu_selector(PreloadAttemptsPopup::onPressLoad),
                    /*node_id*/"load-button"_spr,
                    /*pos*/{0.f, -73.f},
                    /*width*/150.f,
                    /*height*/40.f,
                    /*spriteScale*/0.925f,
                    /*labelScale*/0.8f,
                    /*sizeMult*/kPopupBigButtonSizeMult
                    ).store(m_loadBtn),

            Build<CCSprite>::createSpriteName("GJ_plus2Btn_001.png")
                .anchorPoint(0.5f, 0.5f)
                .scale(1.325f)
                .intoMenuItem(this, menu_selector(PreloadAttemptsPopup::onPressMax))
                .scaleMult(kPopupButtonSizeMult)
                .store(m_maxBtn)
                .pos(124.f, 0.f),

            Build<CCLabelBMFont>::create("Max", "bigFont.fnt")
                .id("maxLabel"_spr)
                .pos(124.f, 24.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.425f)
        )
        .parentAtPos(m_mainLayer, Anchor::Center)
        .collect();

    setInputValue_(startVal);
    refreshInfoLabels_(startVal);
    // setPreloadValue_(); I don't use this now and reset preload on each click of the load buttons

    this->scheduleUpdate();
    return true;
}

void PreloadAttemptsPopup::onClose(CCObject* sender) {
    if (Ghosts::I().isPreloadingAttempts() || (!m_startedLoading && m_replayOn)) { 
        Ghosts::I().cancelPreloadAttempts(false);
        m_replayOn = false;
    }

    if (auto* scene = CCDirector::sharedDirector()->getRunningScene()) {
        if (auto* xs = typeinfo_cast<PlaybackModMenu*>(scene->getChildByIDRecursive("playbackModMenu-popup"_spr))) {
            xs->syncUIFromRuntime();
        }
    }

    Popup::onClose(sender);
}

void PreloadAttemptsPopup::update(float) {
    if (Ghosts::I().isPreloadingAttempts()) {
        tickLoading_();
        return;
    }

    int n = sanitizeParseClamp_();
    if (n != m_lastClampedValue) {
        m_lastClampedValue = n;
        refreshInfoLabels_(n);
    }
}

int PreloadAttemptsPopup::sanitizeParseClamp_() {
    if (!m_numAttemptsToLoad) return 0;

    std::string raw = m_numAttemptsToLoad->getString();

    std::string digits;
    digits.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isdigit(c)) digits.push_back(static_cast<char>(c));
    }

    if (digits.empty()) digits = "0";

    long long v = 0;

    auto parsed = geode::utils::numFromString<long long>(digits);
    if (parsed.isOk()) {
        v = parsed.unwrap();
    }
    else {
        v = 0;
    }

    if (v < 0) v = 0;
    if (m_totalAttempts > 0) v = std::min<long long>(v, m_totalAttempts);

    int clamped = static_cast<int>(v);

    if (digits != raw || raw != std::to_string(clamped)) {
        setInputValue_(clamped);
    }

    return clamped;
}

void PreloadAttemptsPopup::setInputValue_(int v) {
    if (!m_numAttemptsToLoad) return;
    m_numAttemptsToLoad->setString(std::to_string(std::max(v, 0)));
}

void PreloadAttemptsPopup::setPreloadValue_() {
    if (!m_alreadyPreloadedLabel) return;
    int preloaded = Ghosts::I().getNumAttemptsPreloadedTotal();
    if (preloaded == 0) {
        m_alreadyPreloadedLabel->setVisible(false);
        return;
    }
    std::ostringstream ss;
    ss << preloaded << " attempts are already preloaded\n(exit the level to unload them)";
    m_alreadyPreloadedLabel->setString(ss.str().c_str());
    m_alreadyPreloadedLabel->setVisible(true);
}

static int getSettingIntOrDefault_(geode::Mod* mod, const char* key, int def) {
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", def);
            return def;
        }
        return static_cast<int>(mod->getSettingValue<int64_t>(key));
    }


void PreloadAttemptsPopup::refreshInfoLabels_(int clampedN) {
    if (m_numAttemptsLoadingLabel) {
        std::ostringstream ss;
        ss << "Load " << clampedN << " of " << m_totalAttempts;
        m_numAttemptsLoadingLabel->setString(ss.str().c_str());
    }

    if (m_estimatedRamLabel) { // Robert Tobert player objects are not RAM efficient
        //float mb = clampedN * kAttemptMbCost;
        float mb = std::min(getSettingIntOrDefault_(Mod::get(), "real-player-objects", 1000), clampedN) * kAttemptMbCost;
        std::ostringstream ss;
        ss << "(Estimated RAM: " << std::fixed << std::setprecision(1) << mb << "MB)";
        m_estimatedRamLabel->setString(ss.str().c_str());
    }
}

void PreloadAttemptsPopup::onPressMax(CCObject*) {
    int v = std::max(m_totalAttempts, 0);
    setInputValue_(v);
    m_lastClampedValue = v;
    refreshInfoLabels_(v);
}

void PreloadAttemptsPopup::onPressLoad(CCObject*) {
    int n = sanitizeParseClamp_();
    Mod::get()->setSavedValue("preload-attempts", (int64_t)n);

    if (n <= 0) {
        m_startedLoading = false;
        this->onClose(nullptr);
        return;
    }

    beginLoading_(n);
}

void PreloadAttemptsPopup::beginLoading_(int clampedN) {
    m_startedLoading = true;

    if (m_loadBtn) m_loadBtn->setEnabled(false);
    if (m_maxBtn)  m_maxBtn->setEnabled(false);

    Ghosts::I().beginPreloadAttempts(clampedN);

    const int loaded = Ghosts::I().getPreloadLoadedCount();
    const int target = Ghosts::I().getPreloadTargetCount();

    if (m_numAttemptsLoadingLabel) {
        std::ostringstream ss;
        ss << "Loaded: " << loaded << " / " << target;
        m_numAttemptsLoadingLabel->setString(ss.str().c_str());
    }

    if (target <= 0 || loaded >= target || !Ghosts::I().isPreloadingAttempts()) {
        this->onClose(nullptr);
        return;
    }
}

void PreloadAttemptsPopup::tickLoading_() {
    int loaded = 0;
    int realPO = getSettingIntOrDefault_(Mod::get(), "real-player-objects", 1000);
    if (Ghosts::I().isPreloadingAttempts()) {
        // Load 64 per frame for the first ones with real player objects (slow), then speed up a ton for the rest since they're fast
        if (loaded < realPO) Ghosts::I().preloadStep(64);
        else Ghosts::I().preloadStep(1000);
        loaded = Ghosts::I().getPreloadLoadedCount();
    }

    loaded = Ghosts::I().getPreloadLoadedCount();
    const int target = Ghosts::I().getPreloadTargetCount();

    if (m_numAttemptsLoadingLabel) {
        std::ostringstream ss;
        ss << "Loaded: " << loaded << " / " << target;
        m_numAttemptsLoadingLabel->setString(ss.str().c_str());
    }

    if (target <= 0 || loaded >= target || !Ghosts::I().isPreloadingAttempts()) {
        this->onClose(nullptr);
    }
}

PlaybackSettingsPopup* PlaybackSettingsPopup::create() {
    auto ret = new PlaybackSettingsPopup();
    if (ret && ret->init(kPlaybackSettingsW, kPlaybackSettingsH)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool PlaybackSettingsPopup::init(float width, float height) {
    if (!Popup::init(width, height))
        return false;

    this->setID("playback-settings-popup"_spr);
    setTitle("");

    // Pull initial state (defaults should be OFF if Ghosts has no saved state)
    pullFromRuntime_();

    m_mainLayer->setLayout(AnchorLayout::create());

    Build<CCNode>::create()
        .contentSize(0.f, 0.f)
        .anchorPoint(0.5f, 0.5f)
        .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
        .children(

            Build<CCLabelBMFont>::create("Playback Settings", "bigFont.fnt")
                .id("TitleLabel"_spr)
                .pos(0.f, 83.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.825f),

            Build<CCMenu>::create()
                .id("CustomDeathSoundMenu"_spr)
                .store(m_customDeathSoundMenu)
                .pos(57.f, 18.f)
                .contentSize(100.f, 30.f)
                .anchorPoint(0.5f, 0.5f)
                .ignoreAnchorPointForPos(false)
                .children(
                    createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Open Folder",
                    /*callback*/menu_selector(PlaybackSettingsPopup::onOpenCustomDeathSoundFolder),
                    /*node_id*/"open-custom-death-sound-folder-button"_spr,
                    /*pos*/{-70.f, 15.f},
                    /*width*/100.f,
                    /*height*/30.f,
                    /*spriteScale*/1.f,
                    /*labelScale*/0.35f,
                    /*sizeMult*/kPopupButtonSizeMult
                    ).store(m_openCustomDeathSoundFolderBtn),

                    createTextButton_(
                    this,
                    /*spriteFrame*/"GJ_button_01.png",
                    /*text*/"Install Meme SFX",
                    /*callback*/menu_selector(PlaybackSettingsPopup::onDownloadMemeSounds),
                    /*node_id*/"install-meme-sfx-button"_spr,
                    /*pos*/{70.f, 15.f},
                    /*width*/120.f,
                    /*height*/30.f,
                    /*spriteScale*/1.f,
                    /*labelScale*/0.35f,
                    /*sizeMult*/kPopupButtonSizeMult
                    ).store(m_downloadMemeSoundsBtn)
                ),

            Build<CCLabelBMFont>::create("Only showing ghosts of attempts\nthat passed 50%", "bigFont.fnt")
                .alignment(kCCTextAlignmentCenter)
                .id("OnlyPastPercentDynamicLabel"_spr)
                .pos(0.f, -80.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.325f)
                .store(m_percentDynamicLabel)
        )
        .parentAtPos(m_mainLayer, Anchor::Center)
        .collect();

    Build<CCMenu>::create()
        .contentSize(0.f, 0.f)
        .anchorPoint(0.5f, 0.5f)
        .ignoreAnchorPointForPos(false)
        .layoutOpts(Build<AnchorLayoutOptions>::create().anchor(Anchor::Center))
        .children(

            // Custom Death Sound toggle
            Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackSettingsPopup::onToggleCustomDeathSound)))
                .id("CustomDeathSoundToggle"_spr)
                .pos(-110.f, 50.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.8f)
                .store(m_tgCustomDeathSound)
                .children(
                    Build<CCLabelBMFont>::create("Custom Death Sound", "bigFont.fnt")
                        .id("CustomDeathSoundToggleLabel"_spr)
                        .pos(142.f, 18.f)
                        .anchorPoint(0.5f, 0.5f)
                        .scale(0.575f)
                ),

            // Only attempts past N% toggle
            Build<CCMenuItemToggler>(mkToggler(this, menu_selector(PlaybackSettingsPopup::onToggleOnlyPast)))
                .id("OnlyPastPercentageGhostsToggle"_spr)
                .pos(-110.f, -24.f)
                .anchorPoint(0.5f, 0.5f)
                .scale(0.8f)
                .store(m_tgOnlyPastPercent)
                .children(
                    Build<CCLabelBMFont>::create("Only Attempts that\npassed n%", "bigFont.fnt")
                        .alignment(kCCTextAlignmentCenter)
                        .id("OnlyPastPercentageGhostsLabel"_spr)
                        .pos(142.f, 18.f)
                        .anchorPoint(0.5f, 0.5f)
                        .scale(0.575f)
                ),

            Build<TextInput>::create(100.f, "50", "bigFont.fnt")
                .$build_wrap(setString("50"))
                .$build_wrap(setWidth(100.f))
                .$build_wrap(setFilter("0123456789."))
                .$build_wrap(setMaxCharCount(6))
                .id("OnlyPastPercentInput"_spr)
                .store(m_percentInput)
                .pos(57.f, -55.f)
                .contentSize(100.f, 30.f)
                .anchorPoint(0.5f, 0.5f)
                .children(
                    Build<CCLabelBMFont>::create("Passed\nPercent", "bigFont.fnt")
                        .alignment(kCCTextAlignmentCenter)
                        .id("OnlyPastPercentInputLabel"_spr)
                        .pos(-59.f, 15.f)
                        .anchorPoint(0.5f, 0.5f)
                        .scale(0.475f)
                )
        )
        .parentAtPos(m_mainLayer, Anchor::Center)
        .collect();

    // Apply initial values to widgets
    //if (m_tgLimitVisible) m_tgLimitVisible->toggle(m_limitVisibleOn);
    if (m_tgCustomDeathSound) m_tgCustomDeathSound->toggle(m_customDeathSoundOn);
    if (m_tgOnlyPastPercent) m_tgOnlyPastPercent->toggle(m_onlyPastOn);

    //if (m_maxVisibleInput) {
    //    const int v = std::max(0, m_lastMaxVisible >= 0 ? m_lastMaxVisible : 100);
    //    setMaxVisibleInput_(v);
    //}

    if (m_percentInput) {
        float p = (m_lastPercent >= 0.f ? m_lastPercent : 50.f);
        p = std::clamp(p, 0.f, 100.f);
        setPercentInput_(p);
        refreshPercentLabel_(p);
    } else {
        float p = (m_lastPercent >= 0.f ? m_lastPercent : 50.f);
        p = std::clamp(p, 0.f, 100.f);
        refreshPercentLabel_(p);
    }

    // Enforce dependent visibility/enable state
    refreshDependentVisibility_();

    this->scheduleUpdate();
    return true;
}

void PlaybackSettingsPopup::onClose(CCObject* sender) {
    // const int maxVisible = sanitizeParseClampMaxVisible_();
    const float percent = sanitizeParseClampPercent_();

    // pushLimitVisibleToRuntime_();
    pushCustomDeathSoundToRuntime_();
    pushOnlyPastToRuntime_();

    //if (m_limitVisibleOn) {
    //    pushMaxVisibleToRuntime_(maxVisible);
    //}

    if (m_onlyPastOn) {
        pushPercentToRuntime_(percent);
    }

    if (auto* scene = CCDirector::sharedDirector()->getRunningScene()) {
        if (auto* xs = typeinfo_cast<PlaybackModMenu*>(scene->getChildByIDRecursive("playbackModMenu-popup"_spr))) {
            xs->syncUIFromRuntime();
        }
    }

    Popup::onClose(sender);
}

void PlaybackSettingsPopup::update(float) {
    //if (m_maxVisibleInput) {
    //    (void)sanitizeParseClampMaxVisible_();
    //}

    if (m_percentInput) {
        const float p = sanitizeParseClampPercent_();
        // keep the label in sync (especially while typing)
        if (m_onlyPastOn) {
            refreshPercentLabel_(p);
        }
        m_lastPercent = p;
    }
}

void PlaybackSettingsPopup::onToggleLimitVisible(CCObject*) {
    m_limitVisibleOn = !m_limitVisibleOn;
    refreshDependentVisibility_();
}

void PlaybackSettingsPopup::onToggleCustomDeathSound(CCObject*) {
    m_customDeathSoundOn = !m_customDeathSoundOn;
    refreshDependentVisibility_();
}

void PlaybackSettingsPopup::onOpenCustomDeathSoundFolder(CCObject*) {
    // Ensure folder exists
    auto dir = explodeSfxDir_();

    // Count files so the popup is more helpful
    std::vector<std::string> files;
    std::filesystem::file_time_type dummy{};
    refreshCustomExplodeSfx_(files, dummy);

    std::ostringstream ss;
    ss << "Put <cy>.ogg</c>, <cy>.mp3</c>, or <cy>.wav</c> files in this folder:\n\n"
       //<< "<cy>" << utils::string::pathToString(dir) << "</c>\n\n"
       << "One will randomly be chosen each time a ghost dies.\n\n"
       << "Currently found: <cy>" << files.size() << "</c> file(s).";

    FLAlertLayer::create("Custom Death Sounds", ss.str().c_str(), "OK")->show();

    if (!geode::utils::file::openFolder(dir)) {
        log::warn("[GhostSFX] openFolder failed: {}", utils::string::pathToString(dir));
    }
}

void PlaybackSettingsPopup::onDownloadMemeSounds(CCObject*) {
    syncBundledExplodeSfxIntoCustomDir_();
}

void PlaybackSettingsPopup::onToggleOnlyPast(CCObject*) {
    m_onlyPastOn = !m_onlyPastOn;
    refreshDependentVisibility_();

    if (m_onlyPastOn && m_percentInput) {
        const float p = sanitizeParseClampPercent_();
        m_lastPercent = p;
        refreshPercentLabel_(p);
    }
}

static std::string formatPercentValue_(double v) {
    v = std::clamp(v, 0.0, 100.0);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    std::string s = ss.str();

    // trim trailing zeros + optional dot
    if (auto dot = s.find('.'); dot != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s.empty()) s = "0";
    return s;
}

void PlaybackSettingsPopup::setPercentInput_(float v) {
    if (!m_percentInput) return;
    m_percentInput->setString(formatPercentValue_((double)v));
}

float PlaybackSettingsPopup::sanitizeParseClampPercent_() {
    if (!m_percentInput) return 0.f;

    const std::string raw = m_percentInput->getString();

    // keep digits and a single '.'
    std::string s;
    s.reserve(raw.size());
    bool dotUsed = false;
    for (unsigned char c : raw) {
        if (std::isdigit(c)) {
            s.push_back(static_cast<char>(c));
        }
        else if (c == '.' && !dotUsed) {
            s.push_back('.');
            dotUsed = true;
        }
    }

    if (s.empty()) s = "0";

    // leading '.' -> "0."
    if (!s.empty() && s.front() == '.') {
        s.insert(s.begin(), '0');
    }

    // truncate to 2 decimals (but allow trailing '.' while typing)
    bool trailingDot = (!s.empty() && s.back() == '.');
    auto dotPos = s.find('.');
    if (dotPos != std::string::npos) {
        const size_t after = s.size() - (dotPos + 1);
        if (after > 2) {
            s.erase(dotPos + 1 + 2);
            trailingDot = false;
        }
        trailingDot = (dotPos == s.size() - 1);
    }

    double v = 0.0;
    auto parsed = geode::utils::numFromString<double>(s);
    if (parsed.isOk()) {
        v = parsed.unwrap();
    }
    else {
        v = 0.0;
    }

    const double clamped = std::clamp(v, 0.0, 100.0);

    // Keep "5." while typing if it’s in-range and parse didn’t clamp
    std::string normalized;
    if (trailingDot && std::fabs(clamped - v) < 1e-9) {
        normalized = s;
    } 
    else {
        normalized = formatPercentValue_(clamped);
    }

    if (normalized != raw) {
        m_percentInput->setString(normalized);
    }

    return static_cast<float>(clamped);
}

void PlaybackSettingsPopup::refreshPercentLabel_(float percent) {
    if (!m_percentDynamicLabel) return;

    percent = std::clamp(percent, 0.f, 100.f);
    const std::string pstr = formatPercentValue_((double)percent);

    std::ostringstream ss;
    ss << "Only showing ghosts of attempts\nthat passed " << pstr << "%";
    m_percentDynamicLabel->setString(ss.str().c_str());
}

int PlaybackSettingsPopup::sanitizeParseClampMaxVisible_() {
    if (!m_maxVisibleInput) return 0;

    std::string raw = m_maxVisibleInput->getString();

    std::string digits;
    digits.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isdigit(c)) {
            digits.push_back(static_cast<char>(c));
        }
    }

    if (digits.empty()) digits = "0";

    long long v = 0;
    auto parsed = geode::utils::numFromString<long long>(digits);
    if (parsed.isOk()) {
        v = parsed.unwrap();
    }
    else {
        v = 0;
    }

    if (v < 0) v = 0;
    if (v > 9999) v = 9999;

    const int clamped = static_cast<int>(v);

    if (digits != raw || raw != std::to_string(clamped)) {
        setMaxVisibleInput_(clamped);
    }

    return clamped;
}

void PlaybackSettingsPopup::setMaxVisibleInput_(int v) {
    if (!m_maxVisibleInput) return;
    m_maxVisibleInput->setString(std::to_string(std::max(v, 0)));
}

void PlaybackSettingsPopup::refreshDependentVisibility_() {
    //if (m_maxVisibleInput) {
    //    m_maxVisibleInput->setVisible(m_limitVisibleOn);
    //    m_maxVisibleInput->setEnabled(m_limitVisibleOn);
    //}
    //if (m_maxVisibleLabel) {
    //    m_maxVisibleLabel->setVisible(m_limitVisibleOn);
    //}

    // Custom Death Sound -> show folder button + label
    if (m_customDeathSoundMenu) {
        m_customDeathSoundMenu->setVisible(m_customDeathSoundOn);
    }
    if (m_openCustomDeathSoundFolderBtn) {
        m_openCustomDeathSoundFolderBtn->setVisible(m_customDeathSoundOn);
        m_openCustomDeathSoundFolderBtn->setEnabled(m_customDeathSoundOn);
    }
    if (m_downloadMemeSoundsBtn) {
        m_downloadMemeSoundsBtn->setVisible(m_customDeathSoundOn);
        m_downloadMemeSoundsBtn->setEnabled(m_customDeathSoundOn);
    }
    // Only past percent -> controls percent input + dynamic label
    if (m_percentInput) {
        m_percentInput->setVisible(m_onlyPastOn);
        m_percentInput->setEnabled(m_onlyPastOn);
    }
    if (m_percentDynamicLabel) {
        m_percentDynamicLabel->setVisible(m_onlyPastOn);
    }
}

void PlaybackSettingsPopup::pullFromRuntime_() {
    auto& G = Ghosts::I();

    // m_limitVisibleOn = G.getPlaybackLimitVisibleGhostsEnabled();
    m_customDeathSoundOn = G.getPlaybackCustomDeathSoundEnabled();
    m_onlyPastOn = G.getPlaybackOnlyPastPercentEnabled();

    //m_lastMaxVisible = G.getPlaybackMaxVisibleGhosts();
    //if (m_lastMaxVisible <= 0) m_lastMaxVisible = 100;

    m_lastPercent = G.getPlaybackOnlyPastPercentThreshold(); // now float
    m_lastPercent = std::clamp(m_lastPercent, 0.f, 100.f);
}

void PlaybackSettingsPopup::pushLimitVisibleToRuntime_() {
    auto& G = Ghosts::I();
    G.setPlaybackLimitVisibleGhostsEnabled(m_limitVisibleOn);
}

void PlaybackSettingsPopup::pushCustomDeathSoundToRuntime_() {
    auto& G = Ghosts::I();
    G.setPlaybackCustomDeathSoundEnabled(m_customDeathSoundOn);
}

void PlaybackSettingsPopup::pushOnlyPastToRuntime_() {
    auto& G = Ghosts::I();
    G.setPlaybackOnlyPastPercentEnabled(m_onlyPastOn);
}

void PlaybackSettingsPopup::pushMaxVisibleToRuntime_(int v) {
    auto& G = Ghosts::I();
    v = std::max(0, v);
    G.setPlaybackMaxVisibleGhosts(v);
}

void PlaybackSettingsPopup::pushPercentToRuntime_(float percent) {
    auto& G = Ghosts::I();
    percent = std::clamp(percent, 0.f, 100.f);
    G.setPlaybackOnlyPastPercentThreshold(percent);
}

static size_t countCustomExplodeSfxFiles_() {
    std::vector<std::string> files;
    std::filesystem::file_time_type dummy{};
    refreshCustomExplodeSfx_(files, dummy);
    return files.size();
}
