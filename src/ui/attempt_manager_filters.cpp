#include "attempt_manager_popup_helpers.hpp"

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
