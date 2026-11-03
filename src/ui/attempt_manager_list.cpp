#include "attempt_manager_popup_helpers.hpp"

void AttemptManagerPopup::resetPageWidgets_() {
    // I love eating nullptrs
    m_rowsScrollLayer = nullptr;
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
    m_rowsScrollLayer = attemptplayback::ui::AttemptScrollView::create({ kRowsViewW, kRowsViewH });
    if (!m_rowsScrollLayer) return;

    m_rowsScrollLayer->setPosition({ kRowsViewX, kRowsViewY });
    m_rowsScrollLayer->setID("attempt-manager-rows-scroll"_spr);

    m_rowsScrollLayer->setTouchPriority(currentPopupTouchPriority_());

    layer->addChild(m_rowsScrollLayer, 5);

    auto* contentLayer = m_rowsScrollLayer->getContentLayer();
    if (!contentLayer) return;

    m_rowsLayer = CCNode::create();
    m_rowsLayer->setAnchorPoint({0.f, 0.f});
    m_rowsLayer->setPosition({0.f, 0.f});
    m_rowsLayer->setContentSize({kRowsViewW, kRowsViewH});
    m_rowsLayer->setID("attempt-manager-rows-layer"_spr);
    contentLayer->addChild(m_rowsLayer, 5);;

    m_rowsMenu = CCMenu::create();
    m_rowsMenu->setAnchorPoint({0.f, 0.f});
    m_rowsMenu->setPosition({0.f, 0.f});
    m_rowsMenu->setContentSize({kRowsViewW, kRowsViewH});
    m_rowsMenu->setID("attempt-manager-rows-menu"_spr);
    contentLayer->addChild(m_rowsMenu, 10);

    buildRowSlots_();
    buildListSlots_();

    m_emptyRowsLabel = makeLabel_("No attempts found for this level.", kRowsViewW * 0.5f, kRowsViewH * 0.5f + 18.f, 0.36f, m_rowsLayer, {210, 215, 255});
    m_emptyRowsLabel->setVisible(false);

    m_emptyPracticeRowsLabel = makeLabel_("Practice attempts are managed in the Practice tab.", kRowsViewW * 0.5f, kRowsViewH * 0.5f - 8.f, 0.30f, m_rowsLayer, {160, 170, 215});
    m_emptyPracticeRowsLabel->setVisible(false);

    setRowsScrollEnabled_(false);
}

void AttemptManagerPopup::setRowsScrollEnabled_(bool enabled) {
    if (m_rowsScrollLayer) m_rowsScrollLayer->setScrollEnabled(enabled);
}

void AttemptManagerPopup::scrollRowsPageToTop_() {
    if (m_rowsScrollLayer) m_rowsScrollLayer->scrollToTop();
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
    if (Ghosts::I().isPracticeSessionBeingRecorded(session.sessionId)) {
        parkMenuItem_(slot.trash);
    }
    else {
        showMenuItem_(slot.trash, kRowsDeleteX, y, session.sessionId);
    }

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
            if (Ghosts::I().isPracticeSessionBeingRecorded(m_selectedPracticeSessionId)) {
                deleteSessionBtn->setEnabled(false);
                deleteSessionBtn->setOpacity(90);
            }
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
    if (m_sortMenu) m_sortMenu->setEnabled(true);
    if (m_pageMenu) m_pageMenu->setEnabled(true);
    if (m_rowsMenu) m_rowsMenu->setEnabled(true);
    if (m_actionMenu) m_actionMenu->setEnabled(true);

    refreshTabButtons_();
    refreshPagingButtons_();
    refreshTopControls_();
}
