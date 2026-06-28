// attempt_manager_popup.cpp
#include "attempt_manager_popup.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/General.hpp>
#include <Geode/ui/Layout.hpp>

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

#include <UIBuilder.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>
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
using namespace alpha::prelude;

namespace {
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
        panel->setContentSize({w, h});
        panel->setPosition({x, y});
        panel->setColor({0, 0, 0});
        panel->setOpacity(opacity);
        parent->addChild(panel);
        return panel;
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
        bg->setContentSize({ w, h });
        bg->setAnchorPoint({ 0.5f, 0.5f });

        auto* label = CCLabelBMFont::create(text.empty() ? " " : text.c_str(), "bigFont.fnt");
        label->setAnchorPoint({ 0.5f, 0.5f });
        label->setPosition({ w * 0.5f, h * 0.52f });
        label->setScale(0.40f);
        bg->addChild(label);

        auto* item = CCMenuItemSpriteExtra::create(bg, target, selector);
        item->setTag(tab);
        item->setID(id);
        item->setPosition({ x, y });
        item->setSizeMult(kPopupButtonSizeMult);
        return item;
    }

    CCMenuItemToggler* makeCheckbox_(CCObject* target, SEL_MenuHandler selector,
        int serial, float x, float y, bool selected, char const* id) {
        auto* toggler = CCMenuItemToggler::createWithStandardSprites(target, selector, 0.45f);
        toggler->setPosition({ x, y });
        toggler->setTag(serial);
        toggler->setID(id);

        // Keep the visual checkbox size, but shrink interactive area so clicks do not hit other stuff
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
}

AttemptManagerPopup* AttemptManagerPopup::create() {
    auto ret = new AttemptManagerPopup();
    if (ret && ret->init(kPopupW, kPopupH)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool AttemptManagerPopup::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    this->setID("attempt-manager-popup"_spr);
    setTitle("");

    // Murder the default close button
    if (CCMenuItemSpriteExtra* close = findDefaultCloseButton(m_mainLayer)) {
        close->stopAllActions();
        close->setVisible(false);
        close->setEnabled(false);
    }

    m_mainLayer->setLayout(AnchorLayout::create());

    this->setTouchEnabled(false);
    m_initialBuildQueued = true;

    loadAttemptsFromRuntime_();

    this->scheduleOnce(
        schedule_selector(AttemptManagerPopup::doInitialBuild_),
        0.f
    );

    return true;
}

void AttemptManagerPopup::doInitialBuild_(float) {
    m_initialBuildQueued = false;

    rebuild_();

    m_enableTouchesQueued = true;
    this->scheduleOnce(
        schedule_selector(AttemptManagerPopup::enablePopupTouches_),
        0.f
    );
}

void AttemptManagerPopup::enablePopupTouches_(float) {
    m_enableTouchesQueued = false;

    if (m_confirmOpen || m_destructiveActionBusy) return;

    this->setTouchEnabled(true);
}

void AttemptManagerPopup::cancelTopControlsRefresh_() {
    if (!m_topControlsRefreshQueued) return;

    this->unschedule(schedule_selector(AttemptManagerPopup::doQueuedTopControlsRefresh_));
    m_topControlsRefreshQueued = false;
}

void AttemptManagerPopup::clearManageLayer_(CCNode* layer) {
    if (!layer) return;

    disableManageLayerTouches_(layer);
    layer->setVisible(false);
    layer->removeAllChildrenWithCleanup(true);
}

void AttemptManagerPopup::disableManageLayerTouches_(CCNode* layer) {
    if (!layer) return;

    if (m_rowsScrollLayer && m_rowsScrollLayer->getParent()) {
        m_rowsScrollLayer->setVerticalScroll(false);
        m_rowsScrollLayer->setVerticalScrollWheel(false);
        m_rowsScrollLayer->setDraggingEnabled(false);
        m_rowsScrollLayer->blockTouchBehind(false);
        m_rowsScrollLayer->allowEmptyClickThrough(true);
    }

    if (m_rowsMenu) m_rowsMenu->setEnabled(false);
    if (m_actionMenu) m_actionMenu->setEnabled(false);
    if (m_pageMenu) m_pageMenu->setEnabled(false);
    if (m_sortMenu) m_sortMenu->setEnabled(false);
}

void AttemptManagerPopup::onClose(CCObject* sender) {
    if (m_confirmOpen || m_destructiveActionBusy || m_uiTransitioning || m_rebuildQueued) {
        return;
    }

    cancelTopControlsRefresh_();

    if (m_initialBuildQueued) {
        this->unschedule(schedule_selector(AttemptManagerPopup::doInitialBuild_));
        m_initialBuildQueued = false;
    }
    if (m_enableTouchesQueued) {
        this->unschedule(schedule_selector(AttemptManagerPopup::enablePopupTouches_));
        m_enableTouchesQueued = false;
    }

    this->unschedule(schedule_selector(AttemptManagerPopup::doQueuedRebuildBody_));
    m_rebuildQueued = false;
    m_uiTransitioning = false;

    this->unschedule(schedule_selector(AttemptManagerPopup::doQueuedRefreshManagePage_));
    m_manageRefreshQueued = false;
    m_manageRefreshReload = false;

    if (m_rowsScrollLayer && m_rowsScrollLayer->getParent()) {
        m_rowsScrollLayer->setVerticalScroll(false);
        m_rowsScrollLayer->setVerticalScrollWheel(false);
        m_rowsScrollLayer->setDraggingEnabled(false);
        m_rowsScrollLayer->blockTouchBehind(false);
        m_rowsScrollLayer->allowEmptyClickThrough(true);
    }

    m_rowBgBySerial.clear();

    Popup::onClose(sender);
}

void AttemptManagerPopup::loadAttemptsFromRuntime_() {
    m_normalAttempts.clear();
    m_practiceAttempts.clear();
    m_visibleAttempts.clear();
    m_startPosGroups.clear();
    m_practiceSessions.clear();
    m_totalCatalogCount = 0;
    m_hiddenPracticeCount = 0;

    auto& G = Ghosts::I();
    if (!G.hasModAttachedToLevel()) {
        log::warn("[AttemptManager] Cannot load attempts: mod is not attached to a level");
        return;
    }

    G.refreshAttemptCatalogForCurrentLevel(true);
    const auto& catalog = G.getAttemptCatalog();
    m_totalCatalogCount = static_cast<int>(catalog.size());

    for (auto const& a : catalog) {
        if (a.practiceAttempt) {
            ++m_hiddenPracticeCount;
            m_practiceAttempts.push_back(a);
            continue;
        }
        m_normalAttempts.push_back(a);
    }

    buildStartPosGroups_();
    buildPracticeSessions_();
    applyCurrentFilter_();

    for (auto it = m_selectedSerials.begin(); it != m_selectedSerials.end();) {
        const int serial = *it;
        if (!findAnyAttempt_(serial)) it = m_selectedSerials.erase(it);
        else ++it;
    }

    applySort_();

    if (!findAttempt_(m_selectedSerial)) {
        m_selectedSerial = m_selectedSerials.empty() ? -1 : *m_selectedSerials.begin();
    }

    clampPage_();
}

void AttemptManagerPopup::rebuild_() {
    if (!m_mainLayer) return;

    cancelTopControlsRefresh_();

    if (m_manageNormalLayer) disableManageLayerTouches_(m_manageNormalLayer);
    if (m_managePracticeLayer) disableManageLayerTouches_(m_managePracticeLayer);

    if (m_root && m_root->getParent()) {
        m_root->removeAllChildrenWithCleanup(true);
    }
    else {
        m_root = CCNode::create();
        m_root->setID("attempt-manager-root"_spr);
        m_mainLayer->addChild(m_root, 100);
    }

    m_bodyLayer = nullptr;
    m_manageNormalLayer = nullptr;
    m_managePracticeLayer = nullptr;
    m_exportLayer = nullptr;
    m_importLayer = nullptr;

    m_tabMenu = nullptr;
    m_tabNormalBtn = nullptr;
    m_tabPracticeBtn = nullptr;
    m_tabExportBtn = nullptr;
    m_tabImportBtn = nullptr;

    resetPageWidgets_();

    m_root->setContentSize({0.f, 0.f});
    m_root->setAnchorPoint({0.5f, 0.5f});
    m_root->setPosition(m_mainLayer->getContentSize() * 0.5f);

    auto* gradient = createFullscreenGradient_();
    gradient->setColor({100, 86, 255});
    gradient->setAnchorPoint({0.5f, 0.5f});
    gradient->setScaleX(48.f);
    gradient->setScaleY(2.55f);
    gradient->setPosition({0.f, 0.f});
    gradient->setZOrder(-2);
    m_root->addChild(gradient);

    makeLabel_("Attempt Manager", 0.f, 162.f, 0.76f, m_root, {255, 220, 80});

    m_tabMenu = CCMenu::create();
    m_tabMenu->setPosition({0.f, 0.f});
    m_root->addChild(m_tabMenu, 80);

    m_tabNormalBtn = makeTabButton_(this, menu_selector(AttemptManagerPopup::onTabButton), "Normal", static_cast<int>(Tab::Manage), -225.f, 122.f, 132.f, 30.f, "tab-manage"_spr);
    m_tabPracticeBtn = makeTabButton_(this, menu_selector(AttemptManagerPopup::onTabButton), "Practice", static_cast<int>(Tab::Practice), -75.f, 122.f, 132.f, 30.f, "tab-practice"_spr);
    m_tabExportBtn = makeTabButton_(this, menu_selector(AttemptManagerPopup::onTabButton), "Export", static_cast<int>(Tab::Export), 75.f, 122.f, 132.f, 30.f, "tab-export"_spr);
    m_tabImportBtn = makeTabButton_(this, menu_selector(AttemptManagerPopup::onTabButton), "Import", static_cast<int>(Tab::Import), 225.f, 122.f, 132.f, 30.f, "tab-import"_spr);

    m_tabMenu->addChild(m_tabNormalBtn);
    m_tabMenu->addChild(m_tabPracticeBtn);
    m_tabMenu->addChild(m_tabExportBtn);
    m_tabMenu->addChild(m_tabImportBtn);

    refreshTabButtons_();

    m_bodyLayer = CCNode::create();
    m_bodyLayer->setPosition({0.f, 0.f});
    m_root->addChild(m_bodyLayer, 1);

    rebuildBody_();

    auto* closeMenu = CCMenu::create();
    closeMenu->setPosition({0.f, 0.f});
    m_root->addChild(closeMenu, 50);

    auto* closeSpr = CircleButtonSprite::createWithSpriteFrameName(
        "geode.loader/close.png",
        0.85f,
        CircleBaseColor::Green
    );

    auto* closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr,
        this,
        menu_selector(AttemptManagerPopup::onClose)
    );

    closeBtn->ignoreAnchorPointForPosition(false);
    closeBtn->setAnchorPoint({0.f, 1.f});
    closeBtn->setPosition({-328, 182});
    closeBtn->setScale(0.8f);
    closeBtn->setSizeMult(0.8f);
    closeBtn->setID("attempt-manager-close"_spr);
    closeMenu->addChild(closeBtn);

    scaleUIForThatOneTabletUser(kPopupW, kPopupH);
    normalizePopupMenuTouchPriorities(m_mainLayer, -504);
}

void AttemptManagerPopup::scaleUIForThatOneTabletUser(float designWidth, float designHeight) {
    if (!m_mainLayer) return;

    const auto size = fitPopupToWindow_(designWidth, designHeight);
    const float scale = computeFitScale_(size.width, size.height, designWidth, designHeight);

    m_mainLayer->setScale(scale);
}

void AttemptManagerPopup::rebuildBody_() {
    if (!m_bodyLayer) return;

    cancelTopControlsRefresh_();

    // Ensure tab root layers exist
    if (!m_manageNormalLayer) {
        m_manageNormalLayer = CCNode::create();
        m_bodyLayer->addChild(m_manageNormalLayer);
    }

    if (!m_managePracticeLayer) {
        m_managePracticeLayer = CCNode::create();
        m_bodyLayer->addChild(m_managePracticeLayer);
    }

    if (!m_exportLayer) {
        m_exportLayer = CCNode::create();
        m_bodyLayer->addChild(m_exportLayer);
        rebuildBlankTab_(m_exportLayer, "Export Attempts");
    }

    if (!m_importLayer) {
        m_importLayer = CCNode::create();
        m_bodyLayer->addChild(m_importLayer);
        rebuildBlankTab_(m_importLayer, "Import Attempts");
    }

    const bool showNormal   = m_tab == Tab::Manage;
    const bool showPractice = m_tab == Tab::Practice;
    const bool showExport   = m_tab == Tab::Export;
    const bool showImport   = m_tab == Tab::Import;

    // Hide inactive roots
    if (m_manageNormalLayer) {
        if (!showNormal) {
            disableManageLayerTouches_(m_manageNormalLayer);
        }
        m_manageNormalLayer->setVisible(showNormal);
    }

    if (m_managePracticeLayer) {
        if (!showPractice) {
            disableManageLayerTouches_(m_managePracticeLayer);
        }
        m_managePracticeLayer->setVisible(showPractice);
    }

    if (m_exportLayer) {
        m_exportLayer->setVisible(showExport);
    }

    if (m_importLayer) {
        m_importLayer->setVisible(showImport);
    }

    // Rebuild active manage page
    if (showNormal || showPractice) {
        rebuildManagePage_();
    }

    refreshTabButtons_();
}

void AttemptManagerPopup::rebuildBlankTab_(CCNode* layer, char const* title) {
    if (!layer) return;

    layer->removeAllChildrenWithCleanup(true);
    makePanel_(0.f, -16.f, 625.f, 238.f, layer, 76);
    makeLabel_(title, 0.f, 48.f, 0.68f, layer, {255, 220, 80});
    makeLabel_("I will add this in a later update.", 0.f, 8.f, 0.40f, layer, {190, 200, 255});
    makeLabel_("If you have any ideas for stuff to add, please let me know in the discord (link on mod page).", 0.f, -20.f, 0.32f, layer, {160, 170, 215});
}

void AttemptManagerPopup::resetPageWidgets_() {
    // I love eating nullptrs
    m_rowsScrollLayer = nullptr;
    m_rowsScrollBar = nullptr;
    m_rowsLayer = nullptr;
    m_rowsMenu = nullptr;
    m_detailsLayer = nullptr;
    m_actionMenu = nullptr;
    m_pageMenu = nullptr;
    m_sortMenu = nullptr;
    m_totalLabel = nullptr;
    m_pageLabel = nullptr;
    m_selectedLabel = nullptr;
    m_emptyRowsLabel = nullptr;
    m_emptyPracticeRowsLabel = nullptr;
    m_filterLabel = nullptr;
    m_filterButtonLabel = nullptr;
    m_prevPageBtn = nullptr;
    m_prevPageEndBtn = nullptr;
    m_nextPageBtn = nullptr;
    m_nextPageEndBtn = nullptr;
    m_deleteSelectedBtn = nullptr;
    m_selectRunBtn = nullptr;
    m_backToListBtn = nullptr;
    m_deselectBtn = nullptr;
    m_refreshBtn = nullptr;
    m_infoBtn = nullptr;
    m_percentFromInput = nullptr;
    m_percentToInput = nullptr;
    m_sortRecentBtn = nullptr;
    m_sortBestBtn = nullptr;
    m_sortRecentLabel = nullptr;
    m_sortBestLabel = nullptr;
    m_rowSlots.clear();
    m_listSlots.clear();
    m_rowBgBySerial.clear();
}

void AttemptManagerPopup::rebuildManagePage_() {
    CCNode* layer = m_tab == Tab::Practice ? m_managePracticeLayer : m_manageNormalLayer;
    if (!layer) return;

    disableManageLayerTouches_(layer);
    layer->removeAllChildrenWithCleanup(true);
    resetPageWidgets_();
    layer->setPosition({-25.f, 0.f});

    makePanel_(kLeftPanelX, kPanelY, kLeftPanelW, kLeftPanelH, layer, 86);
    makePanel_(kRightPanelX, kRightPanelY, kRightPanelW, kRightPanelH, layer, 86);

    makeLabel_(m_tab == Tab::Practice ? "Practice:" : "Normal:", -280.f, 90.f, 0.27f, layer, {190, 200, 255}, {0.f, 0.5f});

    m_sortMenu = CCMenu::create();
    m_sortMenu->setPosition({0.f, 0.f});
    layer->addChild(m_sortMenu, 30);

    m_sortRecentBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onSortRecent),
        sortRecentText_(),
        "GJ_button_02.png",
        -200.f,
        90.f,
        64.f,
        20.f,
        "sort-recent"_spr,
        0.9f,
        &m_sortRecentLabel
    );

    m_sortBestBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onSortBest),
        sortBestText_(),
        "GJ_button_02.png",
        -140.f,
        90.f,
        56.f,
        20.f,
        "sort-best"_spr,
        0.9f,
        &m_sortBestLabel
    );

    m_deselectBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onDeselectSelected),
        "Deselect",
        "GJ_button_04.png",
        kDeselectBtnX,
        kTopControlsY,
        72.f,
        20.f,
        "deselect-selected"_spr,
        0.9f
    );

    m_refreshBtn = makeSpriteButton_(
        this,
        menu_selector(AttemptManagerPopup::onRefresh),
        "GJ_updateBtn_001.png",
        kRefreshBtnX,
        kTopControlsY,
        "refresh-attempts"_spr,
        0.4f,
        0.30f
    );

    m_infoBtn = makeInfoButton_(
        this,
        m_tab == Tab::Practice
            ? menu_selector(AttemptManagerPopup::onPracticeInfo)
            : menu_selector(AttemptManagerPopup::onNormalInfo),
        0,
        kInfoBtnX,
        kTopControlsY,
        "page-info"_spr
    );

    m_sortMenu->addChild(m_sortRecentBtn);
    m_sortMenu->addChild(m_sortBestBtn);
    m_sortMenu->addChild(m_deselectBtn);
    m_sortMenu->addChild(m_refreshBtn);
    m_sortMenu->addChild(m_infoBtn);

    refreshSortButtons_();

    m_actionMenu = CCMenu::create();
    m_actionMenu->setPosition({0.f, 0.f});
    layer->addChild(m_actionMenu, 40);

    m_totalLabel = makeLabel_("", kRightPanelX, 84.f, 0.30f, layer, {190, 200, 255}, {0.5f, 0.5f});

    int y = 62.f;

    if (isNormalRunsView_()) {
        makeLabel_("Run", -261.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Attempts", -121.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Best", -17.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Del", 110.f, y, 0.24f, layer, {210, 215, 255}, {0.5f, 0.5f});
    }
    else if (isPracticeSessionsView_()) {
        makeLabel_("Session", -261.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Attempts", -121.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Best", -17.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Del", 110.f, y, 0.24f, layer, {210, 215, 255}, {0.5f, 0.5f});
    }
    else {
        if (isNormalAttemptsView_() || isPracticeAttemptsView_()) {
            m_backToListBtn = makeSpriteButton_(
                this,
                menu_selector(AttemptManagerPopup::onBackToList),
                "backArrowPlain_01_001.png",
                -270.f,
                y,
                "back-to-list"_spr,
                0.60f,
                0.42f
            );

            m_sortMenu->addChild(m_backToListBtn);
        }

        makeLabel_("Start %", -246.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("End %", -155.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Duration", -55.f, y, 0.23f, layer, {210, 215, 255}, {0.f, 0.5f});
        makeLabel_("Del", 110.f, y, 0.24f, layer, {210, 215, 255}, {0.5f, 0.5f});
    }

    setupRowsScroll_(layer);

    m_detailsLayer = CCNode::create();
    m_detailsLayer->setPosition({0.f, 0.f});
    layer->addChild(m_detailsLayer);

    m_pageMenu = CCMenu::create();
    m_pageMenu->setPosition({0.f, 0.f});
    layer->addChild(m_pageMenu);

    m_prevPageBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onPrevPage),
        "Prev",
        "GJ_button_01.png",
        -260.f,
        -151.f,
        58.f,
        21.f,
        "page-prev"_spr
    );

    m_prevPageEndBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onPrevPage),
        "Prev",
        "GJ_button_04.png",
        -260.f,
        -151.f,
        58.f,
        21.f,
        "page-prev-end"_spr
    );

    m_nextPageBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onNextPage),
        "Next",
        "GJ_button_01.png",
        -195.f,
        -151.f,
        58.f,
        21.f,
        "page-next"_spr
    );

    m_nextPageEndBtn = makeSmallTextButton_(
        this,
        menu_selector(AttemptManagerPopup::onNextPage),
        "Next",
        "GJ_button_04.png",
        -195.f,
        -151.f,
        58.f,
        21.f,
        "page-next-end"_spr
    );

    m_pageMenu->addChild(m_prevPageBtn);
    m_pageMenu->addChild(m_prevPageEndBtn);
    m_pageMenu->addChild(m_nextPageBtn);
    m_pageMenu->addChild(m_nextPageEndBtn);

    m_pageLabel = makeLabel_("Page 1 / 1", -280.f, -127.f, 0.27f, layer, {190, 200, 255}, {0.f, 0.5f});
    m_selectedLabel = makeLabel_("0 Selected", -34.f, -151.f, 0.29f, layer, {210, 215, 255}, {1.f, 0.5f});

    m_deleteSelectedBtn = makeSmallTextButton_(this, menu_selector(AttemptManagerPopup::onDeleteSelected), "Delete Selected", "GJ_button_05.png", 90.f, -151.f, 104.f, 22.f, "delete-selected"_spr);
    m_pageMenu->addChild(m_deleteSelectedBtn);

    rebuildRows_();
    rebuildDetails_();
    refreshTopLabels_();
    refreshPagingButtons_();
    refreshTopControls_();
}

void AttemptManagerPopup::setupRowsScroll_(CCNode* layer) {
    m_rowsScrollLayer = AdvancedScrollLayer::create({ kRowsViewW, kRowsViewH });
    m_rowsScrollLayer->setPosition({ kRowsViewX, kRowsViewY });
    m_rowsScrollLayer->setID("attempt-manager-rows-scroll"_spr);

    m_rowsScrollLayer->setVerticalScroll(true);
    m_rowsScrollLayer->setHorizontalScroll(false);
    m_rowsScrollLayer->setVerticalScrollWheel(true);
    m_rowsScrollLayer->setHorizontalScrollWheel(false);
    m_rowsScrollLayer->setVerticalScrollForHorizontal(true);

    m_rowsScrollLayer->setDraggingEnabled(true);
    m_rowsScrollLayer->blockTouchBehind(false);
    m_rowsScrollLayer->allowEmptyClickThrough(true);

    m_rowsScrollLayer->setScrollDelta(kRowH);
    m_rowsScrollLayer->setOvershoot(16.f);
    m_rowsScrollLayer->setFriction(0.82f);
    m_rowsScrollLayer->setMinVelocity(0.05f);

    layer->addChild(m_rowsScrollLayer, 5);

    m_rowsLayer = CCNode::create();
    m_rowsLayer->setAnchorPoint({0.f, 0.f});
    m_rowsLayer->setPosition({0.f, 0.f});
    m_rowsLayer->setContentSize({kRowsViewW, kRowsViewH});
    m_rowsLayer->setID("attempt-manager-rows-layer"_spr);
    m_rowsScrollLayer->getContentLayer()->addChild(m_rowsLayer, 5);

    m_rowsMenu = CCMenu::create();
    m_rowsMenu->setAnchorPoint({0.f, 0.f});
    m_rowsMenu->setPosition({0.f, 0.f});
    m_rowsMenu->setContentSize({kRowsViewW, kRowsViewH});
    m_rowsMenu->setID("attempt-manager-rows-menu"_spr);
    m_rowsScrollLayer->getContentLayer()->addChild(m_rowsMenu, 10);

    buildRowSlots_();
    buildListSlots_();

    m_emptyRowsLabel = makeLabel_("No attempts found for this level.", kRowsViewW * 0.5f, kRowsViewH * 0.5f + 18.f, 0.36f, m_rowsLayer, {210, 215, 255});
    m_emptyRowsLabel->setVisible(false);

    m_emptyPracticeRowsLabel = makeLabel_("Practice attempts are managed in the Practice tab.", kRowsViewW * 0.5f, kRowsViewH * 0.5f - 8.f, 0.30f, m_rowsLayer, {160, 170, 215});
    m_emptyPracticeRowsLabel->setVisible(false);

    setRowsScrollEnabled_(false);
}

void AttemptManagerPopup::ensureRowsScrollBar_(bool enabled) {
    if (!m_rowsScrollLayer) return;

    if (!m_rowsScrollBar) {
        m_rowsScrollBar = AdvancedScrollBar::create(m_rowsScrollLayer, ScrollOrientation::VERTICAL);
        m_rowsScrollBar->setPosition({ kRowsViewX + kRowsViewW * 0.5f + 8.f, kRowsViewY });
        m_rowsScrollBar->setContentSize({ 12.f, kRowsViewH });
        m_rowsScrollBar->setID("attempt-manager-rows-scrollbar"_spr);
        m_rowsScrollBar->lockToScrollLayer(true);

        auto* parent = m_tab == Tab::Practice ? m_managePracticeLayer : m_manageNormalLayer;
        if (parent) parent->addChild(m_rowsScrollBar, 6);
    }

    m_rowsScrollBar->setVisible(enabled);
}

void AttemptManagerPopup::setRowsScrollEnabled_(bool enabled) {
    if (!m_rowsScrollLayer) return;

    m_rowsScrollLayer->setVerticalScroll(enabled);
    m_rowsScrollLayer->setVerticalScrollWheel(enabled);
    m_rowsScrollLayer->setDraggingEnabled(enabled);

    m_rowsScrollLayer->blockTouchBehind(enabled);
    m_rowsScrollLayer->allowEmptyClickThrough(true);

    if (!enabled) {
        m_rowsScrollLayer->setScrollY(0.f, false);
    }

    ensureRowsScrollBar_(enabled);
}

void AttemptManagerPopup::scrollRowsPageToTop_() {
    if (!m_rowsScrollLayer) return;
    m_rowsScrollLayer->setScrollY(0.f, false);
}

void AttemptManagerPopup::setRowSlotVisible_(RowSlot& slot, bool visible) {
    if (slot.bg) slot.bg->setVisible(visible);
    if (slot.startLabel) slot.startLabel->setVisible(visible);
    if (slot.endLabel) slot.endLabel->setVisible(visible);
    if (slot.durationLabel) slot.durationLabel->setVisible(visible);

    if (slot.checkbox) {
        if (visible) {
            slot.checkbox->setVisible(true);
            slot.checkbox->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.checkbox);
        }
    }

    if (slot.trash) {
        if (visible) {
            slot.trash->setVisible(true);
            slot.trash->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.trash);
        }
    }

    if (slot.info) {
        if (visible) {
            slot.info->setVisible(true);
            slot.info->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.info);
        }
    }
}

void AttemptManagerPopup::buildRowSlots_() {
    if (!m_rowsLayer || !m_rowsMenu) return;
    if (!m_rowSlots.empty()) return;

    m_rowSlots.reserve(kRowsPerPage);

    for (int row = 0; row < kRowsPerPage; ++row) {
        RowSlot slot{};

        slot.bg = CCScale9Sprite::create("square02b_001.png");
        slot.bg->setContentSize({ kRowsViewW - 8.f, kRowBgH });
        slot.bg->setScaleY(kRowBgScaleY);
        slot.bg->setColor({0, 0, 0});
        slot.bg->setOpacity(64);
        m_rowsLayer->addChild(slot.bg);

        slot.checkbox = makeCheckbox_(this, menu_selector(AttemptManagerPopup::onToggleAttemptSelected), -1, kRowsCheckboxX, 0.f, false, "select-attempt-checkbox"_spr);
        slot.checkbox->setScale(0.82f);
        slot.checkbox->setSizeMult(0.28f);
        m_rowsMenu->addChild(slot.checkbox);

        slot.startLabel = makeLabel_("", kRowsStartPercentX, 0.f, kRowsLabelScale, m_rowsLayer, {235, 238, 255}, {0.f, 0.5f});
        slot.endLabel = makeLabel_("", kRowsEndPercentX, 0.f, kRowsLabelScale, m_rowsLayer, {235, 238, 255}, {0.f, 0.5f});
        slot.durationLabel = makeLabel_("", kRowsDurationX, 0.f, kRowsLabelScale, m_rowsLayer, {235, 238, 255}, {0.f, 0.5f});

        slot.trash = makeTrashButton_(this, menu_selector(AttemptManagerPopup::onDeleteAttempt), -1, kRowsDeleteX, 0.f, "delete-attempt"_spr);
        slot.trash->setScale(0.78f);
        slot.trash->setSizeMult(0.35f);
        m_rowsMenu->addChild(slot.trash);

        slot.info = makeInfoButton_(this, menu_selector(AttemptManagerPopup::onReplayPathInfo), -1, kRowsInfoX, 0.f, "replay-path-info"_spr);
        m_rowsMenu->addChild(slot.info);

        setRowSlotVisible_(slot, false);
        m_rowSlots.push_back(slot);
    }
}

static void safeSetString_(CCLabelBMFont* label, std::string const& text) {
    if (!label) return;

    const std::string safeText = text.empty() ? " " : text;
    const char* current = label->getString();

    if (current && safeText == current) return;

    label->setString(safeText.c_str());
}

void AttemptManagerPopup::updateAttemptRowSlot_(RowSlot& slot, APXAttemptDiskInfo const& a, int row, float contentH) {
    const float y = contentH - kRowsContentPadTop - kRowH * 0.5f - static_cast<float>(row) * kRowH;
    const bool selected = isSelected_(m_selectedSerials, a.serial);

    slot.bg->setPosition({ kRowsViewW * 0.5f, y });
    slot.bg->setColor(selected ? ccColor3B{5, 5, 5} : ccColor3B{0, 0, 0});
    slot.bg->setOpacity(selected ? 126 : 64);

    showMenuItem_(slot.checkbox, kRowsCheckboxX, y, a.serial);
    slot.checkbox->toggle(selected);
    showMenuItem_(slot.trash, kRowsDeleteX, y, a.serial);

    slot.startLabel->setPosition({ kRowsStartPercentX, y });
    safeSetString_(slot.startLabel, formatPercent_(a.startPercent));

    slot.endLabel->setPosition({ kRowsEndPercentX, y });
    safeSetString_(slot.endLabel, formatPercent_(a.endPercent));

    slot.durationLabel->setPosition({ kRowsDurationX, y });
    safeSetString_(slot.durationLabel, formatTime_(a.endTime - a.baseTimeOffset));

    setRowSlotVisible_(slot, true);

    slot.info->setVisible(false);
    slot.info->setEnabled(false);
    slot.info->setTag(-1);
}

void AttemptManagerPopup::updateBlockedPracticeRowSlot_(RowSlot& slot, PracticeSessionGroup const& session, int row, float contentH) {
    const float y = contentH - kRowsContentPadTop - kRowH * 0.5f - static_cast<float>(row) * kRowH;

    slot.bg->setPosition({ kRowsViewW * 0.5f, y });
    slot.bg->setColor({120, 20, 20});
    slot.bg->setOpacity(122);

    parkMenuItem_(slot.checkbox);
    parkMenuItem_(slot.trash);

    slot.startLabel->setVisible(true);
    slot.startLabel->setPosition({ kRowsStartPercentX, y });
    safeSetString_(slot.startLabel, "Replay Path");
    slot.startLabel->setColor({255, 210, 210});

    slot.endLabel->setVisible(true);
    slot.endLabel->setPosition({ kRowsEndPercentX + 8.f, y });
    std::ostringstream ss;
    ss << session.replayPathAttempts << " locked";
    safeSetString_(slot.endLabel, ss.str().c_str());
    slot.endLabel->setColor({255, 210, 210});

    slot.durationLabel->setVisible(true);
    slot.durationLabel->setPosition({ kRowsDurationX - 10.f, y });
    safeSetString_(slot.durationLabel, "full session only");
    slot.durationLabel->setColor({255, 185, 185});

    showMenuItem_(slot.info, kRowsDeleteX, y, session.sessionId);

    slot.bg->setVisible(true);
}

void AttemptManagerPopup::setListSlotVisible_(ListSlot& slot, bool visible) {
    if (slot.bg) slot.bg->setVisible(visible);
    if (slot.nameLabel) slot.nameLabel->setVisible(visible);
    if (slot.countLabel) slot.countLabel->setVisible(visible);
    if (slot.bestLabel) slot.bestLabel->setVisible(visible);

    if (slot.button) {
        if (visible) {
            slot.button->setVisible(true);
            slot.button->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.button);
        }
    }

    if (slot.trash) {
        if (visible) {
            slot.trash->setVisible(true);
            slot.trash->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.trash);
        }
    }

    if (slot.info) {
        if (visible) {
            slot.info->setVisible(true);
            slot.info->setEnabled(true);
        }
        else {
            parkMenuItem_(slot.info);
        }
    }
}

void AttemptManagerPopup::buildListSlots_() {
    if (!m_rowsLayer || !m_rowsMenu) return;
    if (!m_listSlots.empty()) return;

    m_listSlots.reserve(kRowsPerPage);

    for (int row = 0; row < kRowsPerPage; ++row) {
        ListSlot slot{};

        slot.bg = CCScale9Sprite::create("square02b_001.png");
        slot.bg->setContentSize({ kRowsViewW - 8.f, kRowBgH });
        slot.bg->setScaleY(kRowBgScaleY);
        slot.bg->setColor({0, 0, 0});
        slot.bg->setOpacity(64);
        m_rowsLayer->addChild(slot.bg);

        slot.nameLabel = makeLabel_("", 22.f, 0.f, 0.23f, m_rowsLayer, {235, 238, 255}, {0.f, 0.5f});
        slot.countLabel = makeLabel_("", 188.f, 0.f, 0.22f, m_rowsLayer, {210, 215, 255}, {0.f, 0.5f});
        slot.bestLabel = makeLabel_("", 300.f, 0.f, 0.22f, m_rowsLayer, {210, 215, 255}, {0.f, 0.5f});

        slot.button = makeHitButton_(
            this,
            menu_selector(AttemptManagerPopup::onNormalRunSelected),
            -1,
            155.f,
            0.f,
            kListRowHitW,
            17.f,
            "list-row"_spr
        );
        m_rowsMenu->addChild(slot.button, 1);

        slot.trash = makeTrashButton_(this, menu_selector(AttemptManagerPopup::onDeleteListItem), -1, kRowsDeleteX, 0.f, "delete-list-item"_spr);
        slot.trash->setScale(0.78f);
        slot.trash->setSizeMult(0.35f);
        m_rowsMenu->addChild(slot.trash, 20);

        slot.info = makeInfoButton_(this, menu_selector(AttemptManagerPopup::onReplayPathInfo), -1, kRowsInfoX, 0.f, "list-info"_spr);
        m_rowsMenu->addChild(slot.info, 20);

        setListSlotVisible_(slot, false);
        m_listSlots.push_back(slot);
    }
}

void AttemptManagerPopup::updateNormalRunSlot_(ListSlot& slot, int itemIndex, int row, float contentH) {
    const float y = contentH - kRowsContentPadTop - kRowH * 0.5f - static_cast<float>(row) * kRowH;

    const bool isAll = itemIndex == 0;
    const StartPosGroup* group = nullptr;
    if (!isAll) {
        const int groupIndex = itemIndex - 1;
        if (groupIndex >= 0 && groupIndex < static_cast<int>(m_startPosGroups.size())) {
            group = &m_startPosGroups[groupIndex];
        }
    }

    slot.bg->setPosition({ kRowsViewW * 0.5f, y });
    slot.bg->setColor({0, 0, 0});
    slot.bg->setOpacity(64);

    slot.nameLabel->setPosition({ 22.f, y });
    slot.countLabel->setPosition({ 160.f, y });
    slot.bestLabel->setPosition({ 255.f, y });
    showMenuItem_(
        slot.button,
        kListRowHitX,
        y,
        isAll ? -1 : (group ? group->id : -2)
    );

    if (isAll) {
        safeSetString_(slot.nameLabel, "All Attempts   (click to open)");
        std::ostringstream count;
        count << m_normalAttempts.size() << " attempts";
        safeSetString_(slot.countLabel, count.str().c_str());

        float best = 0.f;
        for (auto const& a : m_normalAttempts) best = std::max(best, a.endPercent);
        safeSetString_(slot.bestLabel, ("Best " + formatPercent_(best)).c_str());
    }
    else if (group) {
        safeSetString_(slot.nameLabel, normalRunText_(*group).c_str());
        safeSetString_(slot.countLabel, normalRunCountText_(*group).c_str());
        safeSetString_(slot.bestLabel, normalRunBestText_(*group).c_str());
    }

    setListSlotVisible_(slot, true);

    slot.info->setVisible(false);
    slot.info->setEnabled(false);

    if (isAll || !group) {
        parkMenuItem_(slot.trash);
    }
    else {
        showMenuItem_(slot.trash, kRowsDeleteX, y, group->id);
    }
}

void AttemptManagerPopup::updatePracticeSessionSlot_(ListSlot& slot, int itemIndex, int row, float contentH) {
    const float y = contentH - kRowsContentPadTop - kRowH * 0.5f - static_cast<float>(row) * kRowH;
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_practiceSessions.size())) return;

    const auto& session = m_practiceSessions[itemIndex];

    slot.bg->setPosition({ kRowsViewW * 0.5f, y });
    slot.bg->setColor({0, 0, 0});
    slot.bg->setOpacity(64);

    slot.nameLabel->setPosition({ 22.f, y });
    safeSetString_(slot.nameLabel, practiceSessionText_(session).c_str());

    slot.countLabel->setPosition({ 160.f, y });
    safeSetString_(slot.countLabel, practiceSessionCountText_(session).c_str());

    slot.bestLabel->setPosition({ 255.f, y });
    safeSetString_(slot.bestLabel, practiceSessionBestText_(session).c_str());

    showMenuItem_(slot.button, kListRowHitX, y, session.sessionId);

    setListSlotVisible_(slot, true);

    showMenuItem_(slot.trash, kRowsDeleteX, y, session.sessionId);

    parkMenuItem_(slot.info);
}

void AttemptManagerPopup::rebuildRows_() {
    if (!m_rowsScrollLayer || !m_rowsLayer || !m_rowsMenu) return;

    buildRowSlots_();
    buildListSlots_();
    m_rowBgBySerial.clear();

    if (m_emptyRowsLabel) m_emptyRowsLabel->setVisible(false);
    if (m_emptyPracticeRowsLabel) m_emptyPracticeRowsLabel->setVisible(false);

    for (auto& slot : m_rowSlots) {
        setRowSlotVisible_(slot, false);
        if (slot.checkbox) slot.checkbox->setTag(-1);
        if (slot.trash) slot.trash->setTag(-1);
        if (slot.info) slot.info->setTag(-1);
        if (slot.startLabel) slot.startLabel->setColor({235, 238, 255});
        if (slot.endLabel) slot.endLabel->setColor({235, 238, 255});
        if (slot.durationLabel) slot.durationLabel->setColor({235, 238, 255});
    }

    for (auto& slot : m_listSlots) {
        setListSlotVisible_(slot, false);
        if (slot.button) slot.button->setTag(-2);
        if (slot.trash) slot.trash->setTag(-1);
        if (slot.info) slot.info->setTag(-1);
    }

    const int totalRows = activeRowCount_();

    if (totalRows <= 0) {
        m_rowsScrollLayer->setInnerContentSize({ kRowsViewW, kRowsViewH });
        m_rowsLayer->setContentSize({ kRowsViewW, kRowsViewH });
        m_rowsMenu->setContentSize({ kRowsViewW, kRowsViewH });
        setRowsScrollEnabled_(false);

        if (m_emptyRowsLabel) {
            m_emptyRowsLabel->setVisible(true);
            safeSetString_(m_emptyRowsLabel, m_tab == Tab::Practice ? "No practice sessions found for this level." : "No normal-mode runs found for this level.");
            m_emptyRowsLabel->setPosition({ kRowsViewW * 0.5f, kRowsViewH * 0.5f + 18.f });
        }

        if (m_emptyPracticeRowsLabel) {
            m_emptyPracticeRowsLabel->setVisible(m_tab == Tab::Manage && m_hiddenPracticeCount > 0);
            m_emptyPracticeRowsLabel->setPosition({ kRowsViewW * 0.5f, kRowsViewH * 0.5f - 8.f });
        }

        scrollRowsPageToTop_();
        return;
    }

    clampPage_();

    const int start = pageStart_();
    const int end = pageEnd_();
    const int rowsOnThisPage = std::max(0, end - start);

    const float rawContentH = kRowsContentPadTop + kRowsContentPadBottom + static_cast<float>(rowsOnThisPage) * kRowH;
    const bool canScroll = rawContentH > kRowsViewH + 0.5f;
    const float contentH = std::max(kRowsViewH, rawContentH);

    m_rowsScrollLayer->setInnerContentSize({ kRowsViewW, contentH });
    m_rowsLayer->setContentSize({ kRowsViewW, contentH });
    m_rowsMenu->setContentSize({ kRowsViewW, contentH });
    setRowsScrollEnabled_(canScroll);

    if (isNormalRunsView_()) {
        for (int i = start; i < end; ++i) {
            const int row = i - start;
            if (row < 0 || row >= static_cast<int>(m_listSlots.size())) break;
            updateNormalRunSlot_(m_listSlots[row], i, row, contentH);
        }
    }
    else if (isPracticeSessionsView_()) {
        for (int i = start; i < end; ++i) {
            const int row = i - start;
            if (row < 0 || row >= static_cast<int>(m_listSlots.size())) break;
            updatePracticeSessionSlot_(m_listSlots[row], i, row, contentH);
        }
    }
    else if (isPracticeAttemptsView_()) {
        const auto* session = findPracticeSession_(m_selectedPracticeSessionId);
        const int blockedRow = session ? static_cast<int>(m_visibleAttempts.size()) : -1;

        for (int i = start; i < end; ++i) {
            const int row = i - start;
            if (row < 0 || row >= static_cast<int>(m_rowSlots.size())) break;

            if (i < static_cast<int>(m_visibleAttempts.size())) {
                auto const& a = m_visibleAttempts[i];
                updateAttemptRowSlot_(m_rowSlots[row], a, row, contentH);
                m_rowBgBySerial[a.serial] = m_rowSlots[row].bg;
            }
            else if (session && i == blockedRow && session->replayPathAttempts > 0) {
                updateBlockedPracticeRowSlot_(m_rowSlots[row], *session, row, contentH);
            }
        }
    }
    else {
        for (int i = start; i < end; ++i) {
            const int row = i - start;
            if (row < 0 || row >= static_cast<int>(m_rowSlots.size())) break;

            auto const& a = m_visibleAttempts[i];
            updateAttemptRowSlot_(m_rowSlots[row], a, row, contentH);
            m_rowBgBySerial[a.serial] = m_rowSlots[row].bg;
        }
    }

    scrollRowsPageToTop_();
}

void AttemptManagerPopup::rebuildDetails_() {
    if (!m_detailsLayer || !m_actionMenu) return;

    std::string fromValue = m_percentFromInput ? m_percentFromInput->getString() : "";
    std::string toValue = m_percentToInput ? m_percentToInput->getString() : "";

    m_detailsLayer->removeAllChildrenWithCleanup(true);
    m_actionMenu->removeAllChildrenWithCleanup(true);
    m_percentFromInput = nullptr;
    m_percentToInput = nullptr;

    if (m_tab == Tab::Practice) {
        makePanel_(kRightPanelX, 20.f, kRightPanelW - 20.f, 100.f, m_detailsLayer, 62);
        makeLabel_(isPracticeSessionsView_() ? "Practice Sessions" : "Session Attempts", kRightPanelX, 58.f, 0.32f, m_detailsLayer, {255, 220, 80});

        if (isPracticeSessionsView_()) {
            makeLabel_("Click a session to edit it.", kRightPanelX, 25.f, 0.25f, m_detailsLayer, {190, 200, 255});
            makeLabel_("Trash deletes the full session.", kRightPanelX, 4.f, 0.23f, m_detailsLayer, {255, 185, 185});
        }
        else {
            m_actionMenu->addChild(makeSmallTextButton_(this, menu_selector(AttemptManagerPopup::onSelectAllInRun), "Select Deletable", "GJ_button_01.png", kRightPanelX, 33.f, 116.f, 24.f, "select-deletable"_spr));
            auto* deleteSessionBtn = makeSmallTextButton_(this, menu_selector(AttemptManagerPopup::onDeletePracticeSession), "Delete Session", "GJ_button_05.png", kRightPanelX, 0.f, 112.f, 23.f, "delete-session"_spr);
            deleteSessionBtn->setTag(m_selectedPracticeSessionId);
            m_actionMenu->addChild(deleteSessionBtn);
        }
        return;
    }

    makePanel_(kRightPanelX, 20.f, kRightPanelW - 20.f, 100.f, m_detailsLayer, 62);

    makeLabel_("Delete By Percent", kRightPanelX, 58.f, 0.32f, m_detailsLayer, {255, 220, 80});

    m_percentFromInput = TextInput::create(90.f, "start%", "bigFont.fnt");
    m_percentFromInput->setFilter("0123456789.");
    m_percentFromInput->setMaxCharCount(6);
    m_percentFromInput->setPosition({204.f, 22.f});
    m_percentFromInput->setScale(0.66f);
    if (!fromValue.empty() && fromValue != "0" && fromValue != "100") {
        m_percentFromInput->setString(fromValue);
    }
    m_detailsLayer->addChild(m_percentFromInput);

    makeLabel_("-", kRightPanelX, 22.f, 0.40f, m_detailsLayer, {235, 238, 255});

    m_percentToInput = TextInput::create(90.f, "end%", "bigFont.fnt");
    m_percentToInput->setFilter("0123456789.");
    m_percentToInput->setMaxCharCount(6);
    m_percentToInput->setPosition({284.f, 22.f});
    m_percentToInput->setScale(0.66f);
    if (!toValue.empty() && toValue != "0" && toValue != "100") {
        m_percentToInput->setString(toValue);
    }
    m_detailsLayer->addChild(m_percentToInput);

    m_actionMenu->addChild(makeSmallTextButton_(this, menu_selector(AttemptManagerPopup::onDeletePercentRange), "Delete Range", "GJ_button_05.png", kRightPanelX, -10.f, 116.f, 25.f, "delete-percent-range"_spr));

    m_actionMenu->addChild(makeSmallTextButton_(this, menu_selector(AttemptManagerPopup::onDeleteAllAttempts), "Delete All Data", "GJ_button_05.png", kRightPanelX, -56.f, 108.f, 23.f, "delete-all-attempts"_spr));
}

void AttemptManagerPopup::refreshManagePage_(bool reloadFromRuntime) {
    if (reloadFromRuntime) {
        loadAttemptsFromRuntime_();
    } else {
        applyCurrentFilter_();
        applySort_();
        clampPage_();
    }

    rebuildRows_();
    rebuildDetails_();
    refreshTopLabels_();
    m_destructiveActionBusy = false;
    refreshPagingButtons_();
}

void AttemptManagerPopup::buildStartPosGroups_() {
    m_startPosGroups.clear();

    std::vector<APXAttemptDiskInfo> sorted = m_normalAttempts;
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
        if (a.startX != b.startX) return a.startX < b.startX;
        return a.serial < b.serial;
    });

    const float tolerance = Ghosts::I().getStartPosTolerance();
    int nextId = 1;

    for (auto const& a : sorted) {
        StartPosGroup* target = nullptr;

        for (auto& group : m_startPosGroups) {
            if (std::fabs(a.startX - group.representativeStartX) <= tolerance) {
                target = &group;
                break;
            }
        }

        if (!target) {
            StartPosGroup group{};
            group.id = nextId++;
            group.representativeStartX = a.startX;
            group.representativeStartPercent = a.startPercent;
            m_startPosGroups.push_back(group);
            target = &m_startPosGroups.back();
        }

        target->serials.push_back(a.serial);
        target->count = static_cast<int>(target->serials.size());
        target->bestEndPercent = std::max(target->bestEndPercent, a.endPercent);
        target->latestSerial = std::max(target->latestSerial, a.serial);
        if (a.completed) ++target->completedCount;
    }

    std::sort(m_startPosGroups.begin(), m_startPosGroups.end(), [](auto const& a, auto const& b) {
        if (a.representativeStartPercent != b.representativeStartPercent) {
            return a.representativeStartPercent < b.representativeStartPercent;
        }
        return a.representativeStartX < b.representativeStartX;
    });

    if (m_runFilter == RunFilter::Group && !findRunGroup_(m_selectedRunGroupId)) {
        m_runFilter = RunFilter::All;
        m_selectedRunGroupId = -1;
    }
}

void AttemptManagerPopup::buildPracticeSessions_() {
    m_practiceSessions.clear();

    auto& G = Ghosts::I();
    const PracticePath& path = G.getPracticePathCatalog();

    std::unordered_map<int, APXAttemptDiskInfo const*> bySerial;
    bySerial.reserve(m_practiceAttempts.size());
    for (auto const& a : m_practiceAttempts) {
        bySerial[a.serial] = &a;
    }

    for (auto const& s : path.sessions) {
        PracticeSessionGroup group{};
        group.sessionId = s.sessionId;
        group.startX = s.startX;
        group.completed = s.completed;

        bool haveStartPercent = false;

        std::unordered_set<int> all;
        for (int serial : s.allAttemptSerials) {
            if (serial > 0) all.insert(serial);
        }
        for (auto const& seg : s.segments) {
            if (seg.ownerSerial > 0) all.insert(seg.ownerSerial);
        }

        std::unordered_set<int> replay;
        for (auto const& seg : s.segments) {
            if (seg.ownerSerial > 0) replay.insert(seg.ownerSerial);
        }

        group.allSerials.assign(all.begin(), all.end());
        group.replayPathSerials.assign(replay.begin(), replay.end());
        std::sort(group.allSerials.begin(), group.allSerials.end());
        std::sort(group.replayPathSerials.begin(), group.replayPathSerials.end());

        group.totalAttempts = static_cast<int>(group.allSerials.size());
        group.replayPathAttempts = static_cast<int>(group.replayPathSerials.size());

        for (int serial : group.allSerials) {
            auto it = bySerial.find(serial);
            if (it != bySerial.end()) {
                auto const& a = *it->second;

                group.bestEndPercent = std::max(group.bestEndPercent, a.endPercent);
                group.endPercent = std::max(group.endPercent, a.endPercent);

                if (!haveStartPercent || a.startPercent < group.startPercent) {
                    group.startPercent = a.startPercent;
                    haveStartPercent = true;
                }
            }

            if (replay.count(serial) == 0) {
                group.deletableSerials.push_back(serial);
            }
        }

        if (!haveStartPercent) {
            float bestDist = std::numeric_limits<float>::max();

            for (auto const& a : m_practiceAttempts) {
                const float d = std::fabs(a.startX - s.startX);
                if (d < bestDist) {
                    bestDist = d;
                    group.startPercent = a.startPercent;
                    haveStartPercent = true;
                }
            }
        }

        std::sort(group.deletableSerials.begin(), group.deletableSerials.end());
        group.deletableAttempts = static_cast<int>(group.deletableSerials.size());

        if (group.totalAttempts > 0 || !s.segments.empty()) {
            m_practiceSessions.push_back(std::move(group));
        }
    }

    std::sort(m_practiceSessions.begin(), m_practiceSessions.end(), [](auto const& a, auto const& b) {
        if (a.startPercent != b.startPercent) return a.startPercent < b.startPercent;
        return a.sessionId < b.sessionId;
    });

    if (m_selectedPracticeSessionId != 0 && !findPracticeSession_(m_selectedPracticeSessionId)) {
        m_selectedPracticeSessionId = 0;
        m_practiceView = PracticeView::Sessions;
    }
}

void AttemptManagerPopup::applyCurrentFilter_() {
    if (m_tab == Tab::Practice) applyPracticeSessionFilter_();
    else applyNormalRunFilter_();
}

void AttemptManagerPopup::applyNormalRunFilter_() {
    m_visibleAttempts.clear();

    if (m_runFilter == RunFilter::All) {
        m_visibleAttempts = m_normalAttempts;
        return;
    }

    auto const* group = findRunGroup_(m_selectedRunGroupId);
    if (!group) {
        m_runFilter = RunFilter::All;
        m_selectedRunGroupId = -1;
        m_visibleAttempts = m_normalAttempts;
        return;
    }

    for (auto const& a : m_normalAttempts) {
        if (std::find(group->serials.begin(), group->serials.end(), a.serial) != group->serials.end()) {
            m_visibleAttempts.push_back(a);
        }
    }
}

void AttemptManagerPopup::applyPracticeSessionFilter_() {
    m_visibleAttempts.clear();

    auto const* session = findPracticeSession_(m_selectedPracticeSessionId);
    if (!session) return;

    for (int serial : session->deletableSerials) {
        if (auto const* a = findAnyAttempt_(serial)) {
            m_visibleAttempts.push_back(*a);
        }
    }
}

int AttemptManagerPopup::activeRowCount_() const {
    if (isNormalRunsView_()) {
        if (m_normalAttempts.empty() && m_startPosGroups.empty()) return 0;
        return static_cast<int>(m_startPosGroups.size()) + 1;
    }

    if (isPracticeSessionsView_()) {
        return static_cast<int>(m_practiceSessions.size());
    }

    if (isPracticeAttemptsView_()) {
        int count = static_cast<int>(m_visibleAttempts.size());
        if (auto const* session = findPracticeSession_(m_selectedPracticeSessionId)) {
            if (session->replayPathAttempts > 0) ++count;
        }
        return count;
    }

    return static_cast<int>(m_visibleAttempts.size());
}

bool AttemptManagerPopup::isNormalRunsView_() const {
    return m_tab == Tab::Manage && m_normalView == NormalView::Runs;
}

bool AttemptManagerPopup::isNormalAttemptsView_() const {
    return m_tab == Tab::Manage && m_normalView == NormalView::Attempts;
}

bool AttemptManagerPopup::isPracticeSessionsView_() const {
    return m_tab == Tab::Practice && m_practiceView == PracticeView::Sessions;
}

bool AttemptManagerPopup::isPracticeAttemptsView_() const {
    return m_tab == Tab::Practice && m_practiceView == PracticeView::Attempts;
}

bool AttemptManagerPopup::isSpecificRunFilter_() const {
    return m_runFilter == RunFilter::Group && findRunGroup_(m_selectedRunGroupId) != nullptr;
}

AttemptManagerPopup::StartPosGroup const* AttemptManagerPopup::findRunGroup_(int id) const {
    if (id <= 0) return nullptr;

    for (auto const& group : m_startPosGroups) {
        if (group.id == id) return &group;
    }

    return nullptr;
}

AttemptManagerPopup::PracticeSessionGroup const* AttemptManagerPopup::findPracticeSession_(int sessionId) const {
    if (sessionId <= 0) return nullptr;

    for (auto const& session : m_practiceSessions) {
        if (session.sessionId == sessionId) return &session;
    }

    return nullptr;
}

bool AttemptManagerPopup::serialInSelectedPracticeReplayPath_(int serial) const {
    auto const* session = findPracticeSession_(m_selectedPracticeSessionId);
    if (!session) return false;
    return std::find(session->replayPathSerials.begin(), session->replayPathSerials.end(), serial) != session->replayPathSerials.end();
}

std::string AttemptManagerPopup::filterText_() const {
    if (isNormalRunsView_()) return "Choose Run";
    if (isPracticeSessionsView_()) return "Choose Session";

    if (m_tab == Tab::Practice) {
        if (auto const* session = findPracticeSession_(m_selectedPracticeSessionId)) {
            return "Session: " + practiceSessionText_(*session);
        }
        return "Session: None";
    }

    if (auto const* group = findRunGroup_(m_selectedRunGroupId)) {
        return "Run: " + normalRunText_(*group);
    }

    return "Run: All Attempts";
}

std::string AttemptManagerPopup::normalRunText_(StartPosGroup const& group) const {
    return "Start " + formatPercent_(group.representativeStartPercent) + "   (click to open)";
    // return "Start " + formatPercent_(group.representativeStartPercent);
}

std::string AttemptManagerPopup::normalRunCountText_(StartPosGroup const& group) const {
    std::ostringstream ss;
    ss << group.count << " attempts";
    return ss.str();
}

std::string AttemptManagerPopup::normalRunBestText_(StartPosGroup const& group) const {
    return "Best " + formatPercent_(group.bestEndPercent);
}

std::string AttemptManagerPopup::practiceSessionText_(PracticeSessionGroup const& session) const {
    std::ostringstream ss;
    ss << "Session " << session.sessionId << " / start " << formatPercent_(session.startPercent);
    return ss.str();
}

std::string AttemptManagerPopup::practiceSessionCountText_(PracticeSessionGroup const& session) const {
    std::ostringstream ss;
    ss << session.totalAttempts << " attempts";
    return ss.str();
}

std::string AttemptManagerPopup::practiceSessionBestText_(PracticeSessionGroup const& session) const {
    return "Best " + formatPercent_(session.bestEndPercent);
}

void AttemptManagerPopup::setNormalRunsView_() {
    m_normalView = NormalView::Runs;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;
    rebuildManagePageForViewChange_();
}

void AttemptManagerPopup::setNormalRunAll_() {
    m_normalView = NormalView::Attempts;
    m_runFilter = RunFilter::All;
    m_selectedRunGroupId = -1;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;
    rebuildManagePageForViewChange_();
}

void AttemptManagerPopup::setNormalRunGroup_(int groupId) {
    if (!findRunGroup_(groupId)) return;

    m_normalView = NormalView::Attempts;
    m_runFilter = RunFilter::Group;
    m_selectedRunGroupId = groupId;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;
    rebuildManagePageForViewChange_();
}

void AttemptManagerPopup::setPracticeSessionsView_() {
    m_practiceView = PracticeView::Sessions;
    m_selectedPracticeSessionId = 0;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;
    rebuildManagePageForViewChange_();
}

void AttemptManagerPopup::setPracticeSession_(int sessionId) {
    if (!findPracticeSession_(sessionId)) return;

    m_practiceView = PracticeView::Attempts;
    m_selectedPracticeSessionId = sessionId;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;
    rebuildManagePageForViewChange_();
}

void AttemptManagerPopup::applySort_() {
    if (m_sortMode == SortMode::Recent) {
        const bool desc = m_recentSortDescending;

        std::sort(m_visibleAttempts.begin(), m_visibleAttempts.end(), [desc](auto const& a, auto const& b) {
            return desc ? a.serial > b.serial : a.serial < b.serial;
        });
        return;
    }

    const bool desc = m_bestSortDescending;

    std::sort(m_visibleAttempts.begin(), m_visibleAttempts.end(), [desc](auto const& a, auto const& b) {
        const int ac = a.completed ? 1 : 0;
        const int bc = b.completed ? 1 : 0;

        if (ac != bc) return desc ? ac > bc : ac < bc;
        if (a.endPercent != b.endPercent) return desc ? a.endPercent > b.endPercent : a.endPercent < b.endPercent;
        if (a.endTime != b.endTime) return desc ? a.endTime > b.endTime : a.endTime < b.endTime;
        return desc ? a.serial > b.serial : a.serial < b.serial;
    });
}

int AttemptManagerPopup::pageCount_() const {
    const int total = activeRowCount_();
    if (total <= 0) return 1;
    return (total + kRowsPerPage - 1) / kRowsPerPage;
}

int AttemptManagerPopup::maxPageIndex_() const {
    return std::max(0, pageCount_() - 1);
}

int AttemptManagerPopup::pageStart_() const {
    const int page = std::clamp(m_page, 0, maxPageIndex_());
    return page * kRowsPerPage;
}

int AttemptManagerPopup::pageEnd_() const {
    return std::min(pageStart_() + kRowsPerPage, activeRowCount_());
}

const APXAttemptDiskInfo* AttemptManagerPopup::findAttempt_(int serial) const {
    if (serial <= 0) return nullptr;
    for (auto const& a : m_visibleAttempts) {
        if (a.serial == serial) return &a;
    }
    return nullptr;
}

const APXAttemptDiskInfo* AttemptManagerPopup::findAnyAttempt_(int serial) const {
    if (serial <= 0) return nullptr;
    for (auto const& a : m_normalAttempts) {
        if (a.serial == serial) return &a;
    }
    for (auto const& a : m_practiceAttempts) {
        if (a.serial == serial) return &a;
    }
    return nullptr;
}

void AttemptManagerPopup::setTab_(Tab tab) {
    if (m_tab == tab) return;
    if (m_uiTransitioning || m_rebuildQueued || m_confirmOpen || m_destructiveActionBusy) return;

    cancelTopControlsRefresh_();

    m_tab = tab;
    m_page = 0;
    m_selectedSerials.clear();
    m_selectedSerial = -1;

    applyCurrentFilter_();
    applySort_();
    clampPage_();

    queueRebuildBody_();
}

void AttemptManagerPopup::setSortMode_(SortMode sortMode) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    if (isNormalRunsView_() || isPracticeSessionsView_()) return;

    if (m_sortMode == sortMode) {
        if (sortMode == SortMode::Recent) m_recentSortDescending = !m_recentSortDescending;
        else m_bestSortDescending = !m_bestSortDescending;
    }
    else {
        m_sortMode = sortMode;
        if (sortMode == SortMode::Recent) m_recentSortDescending = true;
        else m_bestSortDescending = true;
    }

    m_page = 0;
    applySort_();

    if (!findAttempt_(m_selectedSerial)) {
        m_selectedSerial = -1;
    }

    if (!m_selectedSerials.empty()) {
        for (auto const& a : m_visibleAttempts) {
            if (m_selectedSerials.count(a.serial)) {
                m_selectedSerial = a.serial;
                break;
            }
        }
    }

    if (m_selectedSerial <= 0 && !m_visibleAttempts.empty()) {
        m_selectedSerial = m_visibleAttempts.front().serial;
    }

    rebuildRows_();
    refreshSortButtons_();
    refreshTopLabels_();
    refreshPagingButtons_();
}

void AttemptManagerPopup::clampPage_() {
    m_page = std::clamp(m_page, 0, maxPageIndex_());
}

void AttemptManagerPopup::refreshTopLabels_() {
    if (m_totalLabel) {
        std::ostringstream ss;
        if (isNormalRunsView_()) ss << m_startPosGroups.size() << " Runs";
        else if (isPracticeSessionsView_()) ss << m_practiceSessions.size() << " Sessions";
        else ss << m_visibleAttempts.size() << " Attempts";
        safeSetString_(m_totalLabel, ss.str().c_str());
    }
    
    if (m_filterLabel) safeSetString_(m_filterLabel, filterText_().c_str());

    if (m_pageLabel) {
        std::ostringstream ss;
        const int total = activeRowCount_();
        const int start = total > 0 ? pageStart_() + 1 : 0;
        const int end = total > 0 ? pageEnd_() : 0;

        ss << "Rows " << start << "-" << end
           << " / " << total
           << " | Page " << (m_page + 1)
           << " / " << pageCount_();

        safeSetString_(m_pageLabel, ss.str().c_str());
    }

    if (m_selectedLabel) {
        std::ostringstream ss;
        ss << m_selectedSerials.size() << " Selected";
        safeSetString_(m_selectedLabel, ss.str().c_str());
    }
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

void AttemptManagerPopup::refreshPagingButtons_() {
    clampPage_();

    const bool canUse = !m_destructiveActionBusy && !m_confirmOpen;
    const bool attemptsView = isNormalAttemptsView_() || isPracticeAttemptsView_();

    const bool atStart = m_page <= 0;
    const bool atEnd = m_page >= maxPageIndex_();

    setPageVariantVisible_(
        m_prevPageBtn,
        m_prevPageEndBtn,
        atStart,
        canUse
    );

    setPageVariantVisible_(
        m_nextPageBtn,
        m_nextPageEndBtn,
        atEnd,
        canUse
    );
    if (m_deleteSelectedBtn) m_deleteSelectedBtn->setEnabled(canUse && attemptsView && !m_selectedSerials.empty());
    if (m_selectRunBtn) m_selectRunBtn->setEnabled(canUse && isPracticeAttemptsView_() && !m_visibleAttempts.empty());
    if (m_refreshBtn) {
        m_refreshBtn->setEnabled(canUse);
    }

    refreshTopLabels_();
    refreshSortButtons_();
}

void AttemptManagerPopup::refreshTabButtons_() {
    const bool canUse =
        !m_uiTransitioning &&
        !m_rebuildQueued &&
        !m_destructiveActionBusy &&
        !m_confirmOpen;

    if (m_tabMenu) {
        m_tabMenu->setEnabled(canUse);
    }

    auto setTabState = [this, canUse](CCMenuItemSpriteExtra* btn, Tab tab) {
        if (!btn) return;

        const bool active = (m_tab == tab);
        btn->setEnabled(canUse);
        btn->setOpacity(active ? 255 : 185);
        btn->setScale(active ? 1.02f : 0.96f);
    };

    setTabState(m_tabNormalBtn, Tab::Manage);
    setTabState(m_tabPracticeBtn, Tab::Practice);
    setTabState(m_tabExportBtn, Tab::Export);
    setTabState(m_tabImportBtn, Tab::Import);
}

void AttemptManagerPopup::rebuildManagePageForViewChange_() {
    if (m_uiTransitioning || m_rebuildQueued) return;

    applyCurrentFilter_();
    applySort_();
    clampPage_();

    queueRebuildBody_();
}

void AttemptManagerPopup::refreshTopControls_() {
    const bool canUse = !m_destructiveActionBusy && !m_confirmOpen;
    const bool attemptsView = isNormalAttemptsView_() || isPracticeAttemptsView_();

    if (m_deselectBtn) {
        const bool showDeselect =
            canUse &&
            attemptsView &&
            !m_selectedSerials.empty();

        m_deselectBtn->setVisible(showDeselect);
        m_deselectBtn->setEnabled(showDeselect);
        m_deselectBtn->setPosition(showDeselect ? ccp(kDeselectBtnX, kTopControlsY) : ccp(10000.f, 10000.f));
    }

    if (m_backToListBtn) {
        const bool showBack = canUse && attemptsView;

        m_backToListBtn->setVisible(showBack);
        m_backToListBtn->setEnabled(showBack);
    }

    if (m_refreshBtn) {
        m_refreshBtn->setEnabled(canUse);
    }
}

void AttemptManagerPopup::queueTopControlsRefresh_() {
    if (m_topControlsRefreshQueued) return;

    m_topControlsRefreshQueued = true;
    this->scheduleOnce(
        schedule_selector(AttemptManagerPopup::doQueuedTopControlsRefresh_),
        0.f
    );
}

void AttemptManagerPopup::doQueuedTopControlsRefresh_(float) {
    m_topControlsRefreshQueued = false;
    refreshTopControls_();
}

void AttemptManagerPopup::queueRefreshManagePage_(bool reloadFromRuntime) {
    m_manageRefreshReload = m_manageRefreshReload || reloadFromRuntime;

    if (m_manageRefreshQueued) return;
    m_manageRefreshQueued = true;

    if (m_sortMenu) m_sortMenu->setEnabled(false);
    if (m_pageMenu) m_pageMenu->setEnabled(false);
    if (m_rowsMenu) m_rowsMenu->setEnabled(false);
    if (m_actionMenu) m_actionMenu->setEnabled(false);

    this->scheduleOnce(
        schedule_selector(AttemptManagerPopup::doQueuedRefreshManagePage_),
        0.f
    );
}

void AttemptManagerPopup::doQueuedRefreshManagePage_(float) {
    const bool reload = m_manageRefreshReload;

    m_manageRefreshQueued = false;
    m_manageRefreshReload = false;

    refreshManagePage_(reload);
}

std::string AttemptManagerPopup::sortRecentText_() const {
    return std::string("Recent ") + (m_recentSortDescending ? "v" : "^");
}

std::string AttemptManagerPopup::sortBestText_() const {
    return std::string("Best ") + (m_bestSortDescending ? "v" : "^");
}

void AttemptManagerPopup::refreshSortButtons_() {
    if (m_sortRecentLabel) safeSetString_(m_sortRecentLabel, sortRecentText_().c_str());
    if (m_sortBestLabel) safeSetString_(m_sortBestLabel, sortBestText_().c_str());
    const bool canUse = !m_destructiveActionBusy && !m_confirmOpen && !isNormalRunsView_() && !isPracticeSessionsView_();

    auto setSortState = [this, canUse](CCMenuItemSpriteExtra* btn, SortMode mode) {
        if (!btn) return;

        const bool active = (m_sortMode == mode);
        btn->setEnabled(canUse);
        btn->setScale(active ? 1.02f : 0.96f);
    };

    setSortState(m_sortRecentBtn, SortMode::Recent);
    setSortState(m_sortBestBtn, SortMode::Best);
}

void AttemptManagerPopup::applyRowSelectionVisual_(int serial, CCMenuItemToggler* skipCheckbox) {
    const bool selected = isSelected_(m_selectedSerials, serial);

    auto it = m_rowBgBySerial.find(serial);
    if (it != m_rowBgBySerial.end() && it->second) {
        it->second->setColor(selected ? ccColor3B{5, 5, 5} : ccColor3B{0, 0, 0});
        it->second->setOpacity(selected ? 126 : 64);
    }

    for (auto& slot : m_rowSlots) {
        if (!slot.checkbox) continue;
        if (slot.checkbox == skipCheckbox) continue;
        if (slot.checkbox->getTag() != serial) continue;

        slot.checkbox->toggle(selected);
        break;
    }
}

void AttemptManagerPopup::refreshSelectedLabelOnly_() {
    if (!m_selectedLabel) return;

    std::ostringstream ss;
    ss << m_selectedSerials.size() << " Selected";
    safeSetString_(m_selectedLabel, ss.str().c_str());
}

void AttemptManagerPopup::queueRebuildBody_() {
    if (m_rebuildQueued) return;

    m_rebuildQueued = true;
    m_uiTransitioning = true;

    cancelTopControlsRefresh_();

    if (m_tabMenu) m_tabMenu->setEnabled(false);
    if (m_sortMenu) m_sortMenu->setEnabled(false);
    if (m_pageMenu) m_pageMenu->setEnabled(false);
    if (m_rowsMenu) m_rowsMenu->setEnabled(false);
    if (m_actionMenu) m_actionMenu->setEnabled(false);

    if (m_rowsScrollLayer && m_rowsScrollLayer->getParent()) {
        m_rowsScrollLayer->setVerticalScroll(false);
        m_rowsScrollLayer->setVerticalScrollWheel(false);
        m_rowsScrollLayer->setDraggingEnabled(false);
        m_rowsScrollLayer->blockTouchBehind(false);
        m_rowsScrollLayer->allowEmptyClickThrough(true);
    }

    this->scheduleOnce(
        schedule_selector(AttemptManagerPopup::doQueuedRebuildBody_),
        0.f
    );
}

void AttemptManagerPopup::doQueuedRebuildBody_(float) {
    m_rebuildQueued = false;

    rebuildBody_();

    m_uiTransitioning = false;

    refreshTabButtons_();
    refreshPagingButtons_();
    refreshTopControls_();
}

void AttemptManagerPopup::onTabButton(CCObject* sender) {
    if (m_uiTransitioning || m_rebuildQueued || m_confirmOpen || m_destructiveActionBusy) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    auto tab = static_cast<Tab>(node->getTag());
    setTab_(tab);
}

void AttemptManagerPopup::onSortRecent(CCObject*) {
    setSortMode_(SortMode::Recent);
}

void AttemptManagerPopup::onSortBest(CCObject*) {
    setSortMode_(SortMode::Best);
}

void AttemptManagerPopup::onPrevPage(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    const int oldPage = m_page;
    m_page = std::max(0, m_page - 1);
    if (m_page == oldPage) return;

    rebuildRows_();
    refreshTopLabels_();
    refreshPagingButtons_();
}

void AttemptManagerPopup::onNextPage(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    const int oldPage = m_page;
    m_page = std::min(maxPageIndex_(), m_page + 1);
    if (m_page == oldPage) return;

    rebuildRows_();
    refreshTopLabels_();
    refreshPagingButtons_();
}

void AttemptManagerPopup::onToggleAttemptSelected(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    if (!isNormalAttemptsView_() && !isPracticeAttemptsView_()) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    auto* toggler = typeinfo_cast<CCMenuItemToggler*>(sender);
    if (!node) return;

    const int serial = node->getTag();
    if (serial <= 0) return;
    if (isPracticeAttemptsView_() && serialInSelectedPracticeReplayPath_(serial)) return;

    if (m_selectedSerials.count(serial)) {
        m_selectedSerials.erase(serial);

        if (m_selectedSerial == serial) {
            m_selectedSerial = m_selectedSerials.empty()
                ? -1
                : *m_selectedSerials.begin();
        }
    }
    else {
        m_selectedSerials.insert(serial);
        m_selectedSerial = serial;
    }

    applyRowSelectionVisual_(serial, toggler);
    refreshSelectedLabelOnly_();
    refreshPagingButtons_();
    queueTopControlsRefresh_();
}

void AttemptManagerPopup::onDeleteAttempt(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    const int serial = node->getTag();
    if (serial <= 0) return;
    if (isPracticeAttemptsView_() && serialInSelectedPracticeReplayPath_(serial)) return;

    showDeleteAttemptConfirm_(serial);
}

void AttemptManagerPopup::onDeleteSelected(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    if (m_selectedSerials.empty()) return;
    if (!isNormalAttemptsView_() && !isPracticeAttemptsView_()) return;

    showDeleteSelectedConfirm_();
}

void AttemptManagerPopup::onDeletePercentRange(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    if (m_tab != Tab::Manage || !isNormalAttemptsView_()) return;

    float from = sanitizePercentInput_(m_percentFromInput, 0.f);
    float to = sanitizePercentInput_(m_percentToInput, 100.f);

    if (from > to) std::swap(from, to);

    showDeletePercentConfirm_(from, to);
}

void AttemptManagerPopup::onDeleteAllAttempts(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    showDeleteAllConfirm_();
}

void AttemptManagerPopup::onSelectAllInRun(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;
    if (!isNormalAttemptsView_() && !isPracticeAttemptsView_()) return;

    m_selectedSerials.clear();
    for (auto const& a : m_visibleAttempts) {
        if (isPracticeAttemptsView_() && serialInSelectedPracticeReplayPath_(a.serial)) continue;
        m_selectedSerials.insert(a.serial);
    }

    m_selectedSerial = m_selectedSerials.empty() ? -1 : *m_selectedSerials.begin();
    rebuildRows_();
    refreshSelectedLabelOnly_();
    refreshPagingButtons_();
    queueTopControlsRefresh_();
}

void AttemptManagerPopup::onDeselectSelected(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    if (m_selectedSerials.empty()) {
        queueTopControlsRefresh_();
        return;
    }

    std::vector<int> oldSelected;
    oldSelected.reserve(m_selectedSerials.size());

    for (int serial : m_selectedSerials) {
        oldSelected.push_back(serial);
    }

    m_selectedSerials.clear();
    m_selectedSerial = -1;

    for (int serial : oldSelected) {
        applyRowSelectionVisual_(serial);
    }

    refreshSelectedLabelOnly_();
    refreshPagingButtons_();
    queueTopControlsRefresh_();
}

void AttemptManagerPopup::onBackToList(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    if (m_tab == Tab::Practice) {
        if (isPracticeAttemptsView_()) {
            setPracticeSessionsView_();
        }
        return;
    }

    if (isNormalAttemptsView_()) {
        setNormalRunsView_();
    }
}

void AttemptManagerPopup::onNormalRunSelected(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    const int tag = node->getTag();

    if (m_tab == Tab::Practice) {
        if (tag > 0) setPracticeSession_(tag);
        return;
    }

    if (tag == -1) setNormalRunAll_();
    else if (tag > 0) setNormalRunGroup_(tag);
}

void AttemptManagerPopup::onPracticeSessionSelected(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    const int sessionId = node->getTag();
    if (sessionId > 0) setPracticeSession_(sessionId);
}

void AttemptManagerPopup::onDeleteListItem(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    const int tag = node->getTag();
    if (tag <= 0) return;

    if (isNormalRunsView_()) {
        showDeleteNormalRunConfirm_(tag);
        return;
    }

    if (isPracticeSessionsView_()) {
        showDeletePracticeSessionConfirm_(tag);
    }
}

void AttemptManagerPopup::onDeletePracticeSession(CCObject* sender) {
    if (m_destructiveActionBusy || m_confirmOpen) return;

    auto* node = typeinfo_cast<CCNode*>(sender);
    const int sessionId = node ? node->getTag() : m_selectedPracticeSessionId;
    if (sessionId <= 0) return;

    showDeletePracticeSessionConfirm_(sessionId);
}

void AttemptManagerPopup::onReplayPathInfo(CCObject*) {
    showReplayPathInfo_();
}

void AttemptManagerPopup::onNormalInfo(CCObject*) {
    showNormalInfo_();
}

void AttemptManagerPopup::onPracticeInfo(CCObject*) {
    showPracticeInfo_();
}

void AttemptManagerPopup::onRefresh(CCObject*) {
    if (m_destructiveActionBusy || m_confirmOpen || m_uiTransitioning || m_rebuildQueued) return;
    queueRefreshManagePage_(true);
}

bool AttemptManagerPopup::deleteAttemptBySerial_(int serial) {
    if (m_tab == Tab::Practice) {
        return Ghosts::I().deletePracticeAttemptsBySerials({ serial });
    }
    return Ghosts::I().deleteAttemptBySerial(serial);
}

bool AttemptManagerPopup::deleteSelectedAttempts_() {
    if (m_selectedSerials.empty()) return false;

    std::vector<int> serials;
    serials.reserve(m_selectedSerials.size());

    for (int serial : m_selectedSerials) {
        serials.push_back(serial);
    }

    if (m_tab == Tab::Practice) {
        return Ghosts::I().deletePracticeAttemptsBySerials(serials);
    }

    return Ghosts::I().deleteAttemptsBySerials(serials);
}

bool AttemptManagerPopup::deleteAttemptsByPercentRange_(float minPercent, float maxPercent) {
    return Ghosts::I().deleteAttemptsByEndPercent(minPercent, maxPercent);
}

bool AttemptManagerPopup::deleteAllAttempts_() {
    return Ghosts::I().deleteCurrentLevelSaveFile();
}

bool AttemptManagerPopup::deleteNormalRun_(int groupId) {
    auto const* group = findRunGroup_(groupId);
    if (!group || group->serials.empty()) return false;

    return Ghosts::I().deleteAttemptsBySerials(group->serials);
}

bool AttemptManagerPopup::deletePracticeSession_(int sessionId) {
    return Ghosts::I().deletePracticeSessionById(sessionId);
}

float AttemptManagerPopup::sanitizePercentInput_(TextInput* input, float fallback) {
    if (!input) return fallback;

    std::string raw = input->getString();
    std::string clean;
    clean.reserve(raw.size());
    bool dot = false;

    for (unsigned char c : raw) {
        if (std::isdigit(c)) clean.push_back(static_cast<char>(c));
        else if (c == '.' && !dot) {
            clean.push_back('.');
            dot = true;
        }
    }

    if (clean.empty() || clean == ".") {
        input->setString("");
        return fallback;
    }

    auto parsed = geode::utils::numFromString<float>(clean);
    float v = parsed.isOk() ? parsed.unwrap() : fallback;
    v = std::clamp(v, 0.f, 100.f);

    std::string formatted = formatPercent_(v);
    if (!formatted.empty() && formatted.back() == '%') formatted.pop_back();

    input->setString(formatted);
    return v;
}

void AttemptManagerPopup::showDeleteAttemptConfirm_(int serial) {
    if (m_confirmOpen) return;
    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    const APXAttemptDiskInfo* a = findAttempt_(serial);
    if (!a) a = findAnyAttempt_(serial);

    std::ostringstream ss;
    ss << "Delete this recorded attempt?\n\n";
    ss << "<cy>Serial: " << serial << "</c>";

    if (a) {
        ss << "\n<cy>Start: " << formatPercent_(a->startPercent) << "</c>";
        ss << "\n<cy>End: " << formatPercent_(a->endPercent) << "</c>";
        ss << "\n<cy>Kind: " << attemptKind_(*a) << "</c>";
    }

    ss << "\n\nThis action cannot be undone.";

    createQuickPopup(
        "Confirm Delete",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this, serial](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deleteAttemptBySerial_(serial)) {
                m_selectedSerials.erase(serial);
                if (m_selectedSerial == serial) m_selectedSerial = -1;
                queueRefreshManagePage_(true);
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    "Could not delete that attempt. Check the logs for the APX rewrite error.",
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showDeleteSelectedConfirm_() {
    if (m_confirmOpen) return;
    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    std::ostringstream ss;
    ss << "Delete <cy>" << m_selectedSerials.size() << "</c> selected attempt(s)?\n\n";
    ss << "This action cannot be undone.";

    createQuickPopup(
        "Confirm Delete",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deleteSelectedAttempts_()) {
                m_selectedSerials.clear();
                m_selectedSerial = -1;
                queueRefreshManagePage_(true);
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    "Could not delete the selected attempts. Check the logs for the APX rewrite error.",
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showDeletePercentConfirm_(float minPercent, float maxPercent) {
    if (m_confirmOpen) return;
    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    int matching = 0;
    for (auto const& a : m_visibleAttempts) {
        if (a.endPercent >= minPercent && a.endPercent <= maxPercent) ++matching;
    }

    if (matching <= 0) {
        std::ostringstream ss;
        ss << "No attempts meet the specified range.\n\n";
        ss << "<cy>Range: " << formatPercent_(minPercent) << " - " << formatPercent_(maxPercent) << "</c>";

        m_confirmOpen = false;
        refreshPagingButtons_();
        refreshTabButtons_();

        FLAlertLayer::create("No Attempts Found", gd::string(ss.str().c_str()), "OK")->show();
        return;
    }

    std::ostringstream ss;
    ss << "Delete attempts ending from <cy>" << formatPercent_(minPercent)
       << "</c> to <cy>" << formatPercent_(maxPercent) << "</c>?\n\n";
    ss << "Matching attempts: <cy>" << matching << "</c>\n\n";
    ss << "This action cannot be undone.";

    createQuickPopup(
        "Confirm Range Delete",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this, minPercent, maxPercent](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deleteAttemptsByPercentRange_(minPercent, maxPercent)) {
                m_selectedSerials.clear();
                m_selectedSerial = -1;
                queueRefreshManagePage_(true);
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    "Could not delete attempts in that percent range. Check the logs for the APX rewrite error.",
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showDeleteAllConfirm_() {
    if (m_confirmOpen) return;

    auto& G = Ghosts::I();

    if (!G.hasModAttachedToLevel()) {
        FLAlertLayer::create("Error", gd::string("Not currently in a level"), "OK")->show();
        return;
    }

    std::string filename = G.getCurrentLevelSaveFileName();
    if (filename.empty()) {
        FLAlertLayer::create("Error", gd::string("No save file found for this level"), "OK")->show();
        return;
    }

    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    std::ostringstream ss;
    ss << "Are you sure you want to delete ALL saved attempts for this level?\nThis inclused all practice mode attempts.\n\n";
    ss << "<cy>File: " << filename << "</c>\n\n";
    ss << "This action cannot be undone!";

    createQuickPopup(
        "Confirm Delete",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deleteAllAttempts_()) {
                m_selectedSerials.clear();
                m_selectedSerial = -1;
                m_savedRowsScrollY = 0.f;
                m_normalView = NormalView::Runs;
                m_practiceView = PracticeView::Sessions;
                queueRefreshManagePage_(true);

                FLAlertLayer::create(
                    "Success",
                    gd::string("All saved attempts for this level have been deleted.\n\nRecording has been restarted."),
                    "OK"
                )->show();
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    gd::string("Failed to delete the save file.\nCheck the logs for more information."),
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showDeleteNormalRunConfirm_(int groupId) {
    if (m_confirmOpen) return;

    auto const* group = findRunGroup_(groupId);
    if (!group) return;

    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    std::ostringstream ss;
    ss << "Delete all attempts in this run?\n\n";
    ss << "<cy>" << normalRunText_(*group) << "</c>\n";
    ss << "Attempts in run: <cy>" << group->count << "</c>\n";
    ss << "Best: <cy>" << formatPercent_(group->bestEndPercent) << "</c>\n\n";
    ss << "This action cannot be undone.";

    createQuickPopup(
        "Confirm Run Delete",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this, groupId](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deleteNormalRun_(groupId)) {
                m_normalView = NormalView::Runs;
                m_runFilter = RunFilter::All;
                m_selectedRunGroupId = -1;
                m_page = 0;
                m_selectedSerials.clear();
                m_selectedSerial = -1;
                queueRefreshManagePage_(true);
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    "Could not delete that run. Check the logs for the APX rewrite error.",
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showDeletePracticeSessionConfirm_(int sessionId) {
    if (m_confirmOpen) return;

    auto const* session = findPracticeSession_(sessionId);
    if (!session) return;

    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    std::ostringstream ss;
    ss << "Delete this full practice session?\n\n";
    ss << "<cy>" << practiceSessionText_(*session) << "</c>\n";
    ss << "Attempts in session: <cy>" << session->totalAttempts << "</c>\n";
    ss << "Replay path attempts: <cr>" << session->replayPathAttempts << "</c>\n\n";
    ss << "This deletes the full session and cannot be undone.";

    createQuickPopup(
        "Delete Practice Session",
        gd::string(ss.str().c_str()),
        "Cancel", "Delete",
        [this, sessionId](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (deletePracticeSession_(sessionId)) {
                m_selectedSerials.clear();
                m_selectedSerial = -1;
                m_practiceView = PracticeView::Sessions;
                m_selectedPracticeSessionId = 0;
                queueRefreshManagePage_(true);
            }
            else {
                m_destructiveActionBusy = false;
                refreshPagingButtons_();
                refreshTabButtons_();

                FLAlertLayer::create(
                    "Delete Failed",
                    "Could not delete that practice session. Check the logs for the APX rewrite error.",
                    "OK"
                )->show();
            }
        }
    );
}

void AttemptManagerPopup::showNormalInfo_() {
    FLAlertLayer::create(
        "Normal Runs",
        gd::string(
            "Normal attempts are grouped by start position.\n\n"
            "Click a run to edit only attempts from that start position.\n"
            "When editing a run, the arrow next to <cy>Start %</c> takes you back to the run list."
        ),
        "OK"
    )->show();
}

void AttemptManagerPopup::showPracticeInfo_() {
    FLAlertLayer::create(
        "Practice Sessions",
        gd::string(
            "Practice attempts are grouped by full practice session.\n\n"
            "Deleting a session removes every attempt and the replay path.\n"
            "Inside a session, only attempts that are not part of the replay path can be deleted."
        ),
        "OK"
    )->show();
}

void AttemptManagerPopup::showReplayPathInfo_() {
    FLAlertLayer::create(
        "Replay Path Attempts",
        gd::string(
            "These attempts are part of the practice replay path.\n\n"
            "They cannot be deleted individually because they are used to build the full practice replay route.\n"
            "To remove them, delete the full practice session."
        ),
        "OK"
    )->show();
}

std::string AttemptManagerPopup::formatTime_(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;

    const int minutes = static_cast<int>(seconds / 60.0);
    const double rem = seconds - static_cast<double>(minutes) * 60.0;

    std::ostringstream ss;
    ss << minutes << ":" << std::setw(6) << std::setfill('0') << std::fixed << std::setprecision(3) << rem;
    return ss.str();
}

std::string AttemptManagerPopup::formatPercent_(float percent) {
    if (!std::isfinite(percent)) percent = 0.f;
    percent = std::clamp(percent, 0.f, 100.f);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << percent;
    std::string s = ss.str();

    if (auto dot = s.find('.'); dot != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }

    return s + "%";
}

std::string AttemptManagerPopup::formatFloat_(float value, int precision) {
    if (!std::isfinite(value)) value = 0.f;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(std::max(0, precision)) << value;
    return ss.str();
}

std::string AttemptManagerPopup::attemptKind_(APXAttemptDiskInfo const& a) {
    if (a.practiceAttempt) return "Practice";
    if (a.completed) return "Complete";
    return "Normal";
}

bool AttemptManagerPopup::isSelected_(std::unordered_set<int> const& set, int serial) {
    return set.find(serial) != set.end();
}

cocos2d::CCLayer* CreateAttemptManagerPopup() {
    auto* p = AttemptManagerPopup::create();
    return p;
}
