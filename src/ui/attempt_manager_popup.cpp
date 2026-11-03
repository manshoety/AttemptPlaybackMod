#include "attempt_manager_popup_helpers.hpp"

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

    loadAttemptsFromRuntime_();
    rebuild_();

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
        m_rowsScrollLayer->setScrollEnabled(false);
    }

    if (m_rowsMenu) m_rowsMenu->setEnabled(false);
    if (m_actionMenu) m_actionMenu->setEnabled(false);
    if (m_pageMenu) m_pageMenu->setEnabled(false);
    if (m_sortMenu) m_sortMenu->setEnabled(false);
}

void AttemptManagerPopup::onClose(CCObject* sender) {
    if (m_rowsScrollLayer && m_rowsScrollLayer->getParent()) {
        m_rowsScrollLayer->setScrollEnabled(false);
    }
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
        m_rowsScrollLayer->setScrollEnabled(false);
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
        rebuildExportPage_(m_exportLayer);
    }

    if (!m_importLayer) {
        m_importLayer = CCNode::create();
        m_bodyLayer->addChild(m_importLayer);
        rebuildImportPage_(m_importLayer);
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

cocos2d::CCLayer* CreateAttemptManagerPopup() {
    auto* p = AttemptManagerPopup::create();
    return p;
}
