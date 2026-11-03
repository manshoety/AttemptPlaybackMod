#include "attempt_manager_popup_helpers.hpp"

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
        m_rowsScrollLayer->setScrollEnabled(false);
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
