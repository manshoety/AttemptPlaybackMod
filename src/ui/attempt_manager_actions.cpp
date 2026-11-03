#include "attempt_manager_popup_helpers.hpp"

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

    if (Ghosts::I().isPracticeSessionBeingRecorded(sessionId)) {
        FLAlertLayer::create(
            "Practice Run Is Recording",
            "You cannot delete the practice session that is currently being recorded. Finish that run or exit practice mode first.",
            "OK"
        )->show();
        return;
    }

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
