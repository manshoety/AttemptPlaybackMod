// attempt_manager_popup.hpp
#pragma once

#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/cocos/menu_nodes/CCMenu.h>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

#include "../core/ghost_manager.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace alpha::ui {
    class AdvancedScrollLayer;
    class AdvancedScrollBar;
}

class AttemptManagerPopup : public geode::Popup {
public:
    static AttemptManagerPopup* create();
    bool init(float width, float height);

protected:
    void onClose(cocos2d::CCObject* sender) override;

private:
    enum class Tab {
        Manage = 0,
        Practice = 1,
        Export = 2,
        Import = 3
    };

    enum class SortMode : int {
        Recent = 0,
        Best = 1,
    };

    enum class NormalView : int {
        Runs = 0,
        Attempts = 1,
    };

    enum class PracticeView : int {
        Sessions = 0,
        Attempts = 1,
    };

    enum class RunFilter : int {
        All = 0,
        Group = 1,
    };

    struct RowSlot {
        cocos2d::extension::CCScale9Sprite* bg = nullptr;
        cocos2d::CCLabelBMFont* startLabel = nullptr;
        cocos2d::CCLabelBMFont* endLabel = nullptr;
        cocos2d::CCLabelBMFont* durationLabel = nullptr;
        CCMenuItemToggler* checkbox = nullptr;
        CCMenuItemSpriteExtra* trash = nullptr;
        CCMenuItemSpriteExtra* info = nullptr;
    };

    struct ListSlot {
        cocos2d::extension::CCScale9Sprite* bg = nullptr;
        cocos2d::CCLabelBMFont* nameLabel = nullptr;
        cocos2d::CCLabelBMFont* countLabel = nullptr;
        cocos2d::CCLabelBMFont* bestLabel = nullptr;
        CCMenuItemSpriteExtra* button = nullptr;
        CCMenuItemSpriteExtra* trash = nullptr;
        CCMenuItemSpriteExtra* info = nullptr;
    };

    struct StartPosGroup {
        int id = -1;
        float representativeStartX = 0.f;
        float representativeStartPercent = 0.f;
        int count = 0;
        float bestEndPercent = 0.f;
        int completedCount = 0;
        int latestSerial = -1;
        std::vector<int> serials;
    };

    struct PracticeSessionGroup {
        int sessionId = 0;
        float startX = 0.f;
        float startPercent = 0.f;
        float endPercent = 0.f;
        float bestEndPercent = 0.f;
        int totalAttempts = 0;
        int replayPathAttempts = 0;
        int deletableAttempts = 0;
        bool completed = false;
        std::vector<int> allSerials;
        std::vector<int> replayPathSerials;
        std::vector<int> deletableSerials;
    };

private:
    std::vector<APXAttemptDiskInfo> m_normalAttempts;
    std::vector<APXAttemptDiskInfo> m_practiceAttempts;
    std::vector<APXAttemptDiskInfo> m_visibleAttempts;
    std::vector<StartPosGroup> m_startPosGroups;
    std::vector<PracticeSessionGroup> m_practiceSessions;
    std::unordered_set<int> m_selectedSerials;
    std::unordered_map<int, cocos2d::extension::CCScale9Sprite*> m_rowBgBySerial;

    Tab m_tab = Tab::Manage;
    SortMode m_sortMode = SortMode::Recent;
    NormalView m_normalView = NormalView::Runs;
    PracticeView m_practiceView = PracticeView::Sessions;
    RunFilter m_runFilter = RunFilter::All;
    int m_page = 0;
    int m_selectedSerial = -1;
    int m_selectedRunGroupId = -1;
    int m_selectedPracticeSessionId = 0;

    bool m_recentSortDescending = true; // true = newest first
    bool m_bestSortDescending = true;   // true = highest/best first

    int m_totalCatalogCount = 0;
    int m_hiddenPracticeCount = 0;
    bool m_destructiveActionBusy = false;
    bool m_confirmOpen = false;
    bool m_topControlsRefreshQueued = false;
    bool m_rebuildQueued = false;
    bool m_uiTransitioning = false;
    bool m_manageRefreshQueued = false;
    bool m_manageRefreshReload = false;

    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCNode* m_bodyLayer = nullptr;
    cocos2d::CCNode* m_manageNormalLayer = nullptr;
    cocos2d::CCNode* m_managePracticeLayer = nullptr;
    cocos2d::CCNode* m_exportLayer = nullptr;
    cocos2d::CCNode* m_importLayer = nullptr;
    cocos2d::CCNode* m_rowsLayer = nullptr;
    cocos2d::CCNode* m_detailsLayer = nullptr;

    cocos2d::CCMenu* m_tabMenu = nullptr;
    cocos2d::CCMenu* m_sortMenu = nullptr;
    cocos2d::CCMenu* m_pageMenu = nullptr;
    cocos2d::CCMenu* m_rowsMenu = nullptr;
    cocos2d::CCMenu* m_actionMenu = nullptr;

    alpha::ui::AdvancedScrollLayer* m_rowsScrollLayer = nullptr;
    alpha::ui::AdvancedScrollBar* m_rowsScrollBar = nullptr;

    float m_savedRowsScrollY = 0.f;

    cocos2d::CCLabelBMFont* m_totalLabel = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
    cocos2d::CCLabelBMFont* m_selectedLabel = nullptr;
    cocos2d::CCLabelBMFont* m_emptyRowsLabel = nullptr;
    cocos2d::CCLabelBMFont* m_emptyPracticeRowsLabel = nullptr;
    cocos2d::CCLabelBMFont* m_filterLabel = nullptr;
    cocos2d::CCLabelBMFont* m_filterButtonLabel = nullptr;
    cocos2d::CCLabelBMFont* m_sortRecentLabel = nullptr;
    cocos2d::CCLabelBMFont* m_sortBestLabel = nullptr;

    geode::TextInput* m_percentFromInput = nullptr;
    geode::TextInput* m_percentToInput = nullptr;

    CCMenuItemSpriteExtra* m_tabNormalBtn = nullptr;
    CCMenuItemSpriteExtra* m_tabPracticeBtn = nullptr;
    CCMenuItemSpriteExtra* m_tabExportBtn = nullptr;
    CCMenuItemSpriteExtra* m_tabImportBtn = nullptr;

    CCMenuItemSpriteExtra* m_prevPageBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevPageEndBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextPageBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextPageEndBtn = nullptr;
    CCMenuItemSpriteExtra* m_deleteSelectedBtn = nullptr;
    CCMenuItemSpriteExtra* m_selectRunBtn = nullptr;
    CCMenuItemSpriteExtra* m_backToListBtn = nullptr;
    CCMenuItemSpriteExtra* m_deselectBtn = nullptr;
    CCMenuItemSpriteExtra* m_refreshBtn = nullptr;
    CCMenuItemSpriteExtra* m_infoBtn = nullptr;
    CCMenuItemSpriteExtra* m_sortRecentBtn = nullptr;
    CCMenuItemSpriteExtra* m_sortBestBtn = nullptr;

    std::vector<RowSlot> m_rowSlots;
    std::vector<ListSlot> m_listSlots;

private:
    void loadAttemptsFromRuntime_();
    void rebuild_();
    void scaleUIForThatOneTabletUser(float designWidth, float designHeight);
    void rebuildBody_();
    void rebuildManagePage_();
    void rebuildDetails_();
    void refreshManagePage_(bool reloadFromRuntime);
    void rebuildBlankTab_(cocos2d::CCNode* layer, char const* title);

    void resetPageWidgets_();
    void setupRowsScroll_(cocos2d::CCNode* layer);
    void rebuildRows_();
    void scrollRowsPageToTop_();
    void setRowsScrollEnabled_(bool enabled);
    void ensureRowsScrollBar_(bool enabled);
    void buildRowSlots_();
    void setRowSlotVisible_(RowSlot& slot, bool visible);
    void updateAttemptRowSlot_(RowSlot& slot, APXAttemptDiskInfo const& a, int row, float contentH);
    void updateBlockedPracticeRowSlot_(RowSlot& slot, PracticeSessionGroup const& session, int row, float contentH);
    void buildListSlots_();
    void setListSlotVisible_(ListSlot& slot, bool visible);
    void updateNormalRunSlot_(ListSlot& slot, int itemIndex, int row, float contentH);
    void updatePracticeSessionSlot_(ListSlot& slot, int itemIndex, int row, float contentH);

    void buildStartPosGroups_();
    void buildPracticeSessions_();
    void applyCurrentFilter_();
    void applyNormalRunFilter_();
    void applyPracticeSessionFilter_();
    int activeRowCount_() const;
    bool isNormalRunsView_() const;
    bool isNormalAttemptsView_() const;
    bool isPracticeSessionsView_() const;
    bool isPracticeAttemptsView_() const;
    bool isSpecificRunFilter_() const;
    StartPosGroup const* findRunGroup_(int id) const;
    PracticeSessionGroup const* findPracticeSession_(int sessionId) const;
    bool serialInSelectedPracticeReplayPath_(int serial) const;
    std::string filterText_() const;
    std::string normalRunText_(StartPosGroup const& group) const;
    std::string normalRunCountText_(StartPosGroup const& group) const;
    std::string normalRunBestText_(StartPosGroup const& group) const;
    std::string practiceSessionText_(PracticeSessionGroup const& session) const;
    std::string practiceSessionCountText_(PracticeSessionGroup const& session) const;
    std::string practiceSessionBestText_(PracticeSessionGroup const& session) const;
    void setNormalRunsView_();
    void setNormalRunAll_();
    void setNormalRunGroup_(int groupId);
    void setPracticeSessionsView_();
    void setPracticeSession_(int sessionId);

    void applySort_();
    int maxPageIndex_() const;
    int pageCount_() const;
    int pageStart_() const;
    int pageEnd_() const;
    const APXAttemptDiskInfo* findAttempt_(int serial) const;
    const APXAttemptDiskInfo* findAnyAttempt_(int serial) const;

    void setTab_(Tab tab);
    void setSortMode_(SortMode sortMode);
    void clampPage_();
    void refreshTopLabels_();
    void refreshPagingButtons_();
    void refreshTabButtons_();
    void refreshSortButtons_();
    void rebuildManagePageForViewChange_();
    void applyRowSelectionVisual_(int serial, CCMenuItemToggler* skipCheckbox = nullptr);
    void refreshSelectedLabelOnly_();
    void refreshTopControls_();
    void queueTopControlsRefresh_();
    void doQueuedTopControlsRefresh_(float);

    void onTabButton(cocos2d::CCObject* sender);
    void onSortRecent(cocos2d::CCObject* sender);
    void onSortBest(cocos2d::CCObject* sender);
    void onPrevPage(cocos2d::CCObject* sender);
    void onNextPage(cocos2d::CCObject* sender);
    void onToggleAttemptSelected(cocos2d::CCObject* sender);
    void onDeleteAttempt(cocos2d::CCObject* sender);
    void onDeleteSelected(cocos2d::CCObject* sender);
    void onDeletePercentRange(cocos2d::CCObject* sender);
    void onDeleteAllAttempts(cocos2d::CCObject* sender);
    void onSelectAllInRun(cocos2d::CCObject* sender);
    void onBackToList(cocos2d::CCObject* sender);
    void onDeselectSelected(cocos2d::CCObject* sender);
    void onNormalRunSelected(cocos2d::CCObject* sender);
    void onPracticeSessionSelected(cocos2d::CCObject* sender);
    void onDeletePracticeSession(cocos2d::CCObject* sender);
    void onDeleteListItem(cocos2d::CCObject* sender);
    void onReplayPathInfo(cocos2d::CCObject* sender);
    void onNormalInfo(cocos2d::CCObject* sender);
    void onPracticeInfo(cocos2d::CCObject* sender);
    void onRefresh(cocos2d::CCObject* sender);

    bool deleteAttemptBySerial_(int serial);
    bool deleteSelectedAttempts_();
    bool deleteAttemptsByPercentRange_(float minPercent, float maxPercent);
    bool deleteAllAttempts_();
    bool deleteNormalRun_(int groupId);
    bool deletePracticeSession_(int sessionId);

    float sanitizePercentInput_(geode::TextInput* input, float fallback);
    void showDeleteAttemptConfirm_(int serial);
    void showDeleteSelectedConfirm_();
    void showDeletePercentConfirm_(float minPercent, float maxPercent);
    void showDeleteAllConfirm_();
    void showDeleteNormalRunConfirm_(int groupId);
    void showDeletePracticeSessionConfirm_(int sessionId);
    void showNormalInfo_();
    void showPracticeInfo_();
    void showReplayPathInfo_();

    static std::string formatTime_(double seconds);
    static std::string formatPercent_(float percent);
    static std::string formatFloat_(float value, int precision = 1);
    static std::string attemptKind_(APXAttemptDiskInfo const& a);
    static bool isSelected_(std::unordered_set<int> const& set, int serial);

    std::string sortRecentText_() const;
    std::string sortBestText_() const;

    void cancelTopControlsRefresh_();
    void clearManageLayer_(cocos2d::CCNode* layer);

    void queueRebuildBody_();
    void doQueuedRebuildBody_(float);
    void disableManageLayerTouches_(cocos2d::CCNode* layer);
    void queueRefreshManagePage_(bool reloadFromRuntime);
    void doQueuedRefreshManagePage_(float);
};

cocos2d::CCLayer* CreateAttemptManagerPopup();
