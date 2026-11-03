#include "attempt_manager_popup_helpers.hpp"

void AttemptManagerPopup::rebuildExportPage_(CCNode* layer) {
    if (!layer) return;

    layer->removeAllChildrenWithCleanup(true);
    makePanel_(0.f, -16.f, 625.f, 238.f, layer, 76);
    makeLabel_("Export Attempts", 0.f, 78.f, 0.62f, layer, {255, 220, 80});

    auto* menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    layer->addChild(menu, 20);

    CCLabelBMFont* fullLabel = nullptr;
    auto* full = makeSmallTextButton_(
        this, menu_selector(AttemptManagerPopup::onExportFullFile),
        "Export Full File", "GJ_button_01.png",
        -155.f, 18.f, 220.f, 34.f, "export-full-file"_spr, 1.f, &fullLabel);
    
    CCLabelBMFont* normalLabel = nullptr;
    auto* normal = makeSmallTextButton_(
        this, menu_selector(AttemptManagerPopup::onExportBestNormal),
        "Export Best Normal", "GJ_button_01.png",
        -155.f, -35.f, 220.f, 34.f, "export-best-normal"_spr, 1.f, &normalLabel);
    
    CCLabelBMFont* practiceLabel = nullptr;
    auto* practice = makeSmallTextButton_(
        this, menu_selector(AttemptManagerPopup::onExportBestPractice),
        "Export Best Practice", "GJ_button_01.png",
        -155.f, -88.f, 220.f, 34.f, "export-best-practice"_spr, 1.f, &practiceLabel);

    if (fullLabel) fullLabel->setScale(fullLabel->getScale() * 2.f);
    if (normalLabel) normalLabel->setScale(normalLabel->getScale() * 2.f);
    if (practiceLabel) practiceLabel->setScale(practiceLabel->getScale() * 2.f);

    menu->addChild(full);
    menu->addChild(normal);
    menu->addChild(practice);

    makeLabel_("Everything in this level's save file.", -18.f, 18.f, 0.28f, layer, {215, 220, 255}, {0.f, 0.5f});
    makeLabel_("Only the single best normal-mode attempt.", -18.f, -35.f, 0.28f, layer, {215, 220, 255}, {0.f, 0.5f});
    makeLabel_("Only the best practice replay path and the attempts it uses.", -18.f, -88.f, 0.25f, layer, {215, 220, 255}, {0.f, 0.5f});
}

void AttemptManagerPopup::rebuildImportPage_(CCNode* layer) {
    if (!layer) return;

    layer->removeAllChildrenWithCleanup(true);
    makePanel_(0.f, -16.f, 625.f, 238.f, layer, 76);
    makeLabel_("Import Attempts", 0.f, 72.f, 0.62f, layer, {255, 220, 80});

    auto* menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    layer->addChild(menu, 20);

    CCLabelBMFont* replaceLabel = nullptr;
    auto* replace = makeSmallTextButton_(
        this, menu_selector(AttemptManagerPopup::onImportReplace),
        "Replace Current File", "GJ_button_06.png",
        -150.f, 5.f, 230.f, 36.f, "import-replace"_spr, 1.f, &replaceLabel);
    
    CCLabelBMFont* mergeLabel = nullptr;
    auto* merge = makeSmallTextButton_(
        this, menu_selector(AttemptManagerPopup::onImportMerge),
        "Merge With Current Save", "GJ_button_01.png",
        -150.f, -65.f, 230.f, 36.f, "import-merge"_spr, 1.f, &mergeLabel);

    if (replaceLabel) replaceLabel->setScale(replaceLabel->getScale() * 1.5f);
    if (mergeLabel) mergeLabel->setScale(mergeLabel->getScale() * 1.5f);

    menu->addChild(replace);
    menu->addChild(merge);

    makeLabel_("Overwrites this level's current attempts save.", -8.f, 10.f, 0.27f, layer, {255, 205, 205}, {0.f, 0.5f});
    makeLabel_("Backup before doing this if you care about the current save.", -8.f, -8.f, 0.23f, layer, {220, 185, 185}, {0.f, 0.5f});

    makeLabel_("Keeps current data and adds the imported data.", -8.f, -60.f, 0.27f, layer, {215, 220, 255}, {0.f, 0.5f});
    makeLabel_("Conflicting IDs are remapped so neither save overwrites the other.", -8.f, -78.f, 0.23f, layer, {185, 195, 235}, {0.f, 0.5f});
}

void AttemptManagerPopup::onExportFullFile(CCObject*) {
    beginExport_(Ghosts::AttemptFileExportMode::FullFile);
}

void AttemptManagerPopup::onExportBestNormal(CCObject*) {
    beginExport_(Ghosts::AttemptFileExportMode::BestNormalAttempt);
}

void AttemptManagerPopup::onExportBestPractice(CCObject*) {
    beginExport_(Ghosts::AttemptFileExportMode::BestPracticePath);
}

void AttemptManagerPopup::onImportReplace(CCObject*) {
    beginImport_(Ghosts::AttemptFileImportMode::ReplaceCurrent);
}

void AttemptManagerPopup::onImportMerge(CCObject*) {
    beginImport_(Ghosts::AttemptFileImportMode::MergeWithCurrent);
}

void AttemptManagerPopup::beginExport_(Ghosts::AttemptFileExportMode mode) {
    if (m_confirmOpen || m_destructiveActionBusy || m_uiTransitioning || m_rebuildQueued) return;

    auto& G = Ghosts::I();
    if (!G.hasModAttachedToLevel()) {
        FLAlertLayer::create("Export Failed", "You must be inside a level to export its attempts.", "OK")->show();
        return;
    }

    std::string base = geode::utils::string::pathToString(G.getCurrentLevelSavePath().stem());
    if (base.empty()) base = "attempts";

    std::string suffix;
    switch (mode) {
        case Ghosts::AttemptFileExportMode::FullFile:
            suffix = "_full.apx";
            break;
        case Ghosts::AttemptFileExportMode::BestNormalAttempt:
            suffix = "_best-normal.apx";
            break;
        case Ghosts::AttemptFileExportMode::BestPracticePath:
            suffix = "_best-practice.apx";
            break;
    }

    const auto suggested = G.getAttemptExportsDir() / (base + suffix);
    auto opt = attemptFilePickOptions_(suggested);

    auto* self = this;
    self->retain();

    async::spawn(
        geode::utils::file::pick(geode::utils::file::PickMode::SaveFile, opt),
        [self, mode](geode::Result<std::optional<std::filesystem::path>> result) {
            if (result.isErr()) {
                result.inspectErr([](std::string const& error) {
                    log::info("[APX export] picker cancelled / failed: {}", error);
                });
                self->release();
                return;
            }

            std::optional<std::filesystem::path> selectedPath;
            result.inspect([&](std::optional<std::filesystem::path> const& value) {
                selectedPath = value;
            });

            if (!selectedPath || selectedPath->empty()) {
                self->release();
                return;
            }

            const auto destination = ensureApxExtension_(*selectedPath);
            std::string error;
            if (!Ghosts::I().exportCurrentLevelAttempts(destination, mode, &error)) {
                const std::string message = error.empty()
                    ? "The APX export failed. Check the logs for details."
                    : error;
                FLAlertLayer::create("Export Failed", gd::string(message.c_str()), "OK")->show();
                self->release();
                return;
            }

            const std::string name = importFileName_(destination);
            const std::string message = "Exported successfully as:\n<cy>" + name + "</c>";
            FLAlertLayer::create("Export Complete", gd::string(message.c_str()), "OK")->show();
            self->release();
        }
    );
}

void AttemptManagerPopup::beginImport_(Ghosts::AttemptFileImportMode mode) {
    if (m_confirmOpen || m_destructiveActionBusy || m_uiTransitioning || m_rebuildQueued) return;

    auto& G = Ghosts::I();
    if (!G.hasModAttachedToLevel()) {
        FLAlertLayer::create("Import Failed", "You must be inside a level to import attempts into it.", "OK")->show();
        return;
    }

    auto opt = attemptFilePickOptions_(G.getAttemptExportsDir());
    auto* self = this;
    self->retain();

    async::spawn(
        geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, opt),
        [self, mode](geode::Result<std::optional<std::filesystem::path>> result) {
            if (result.isErr()) {
                result.inspectErr([](std::string const& error) {
                    log::info("[APX import] picker cancelled / failed: {}", error);
                });
                self->release();
                return;
            }

            std::optional<std::filesystem::path> selectedPath;
            result.inspect([&](std::optional<std::filesystem::path> const& value) {
                selectedPath = value;
            });

            if (!selectedPath || selectedPath->empty()) {
                self->release();
                return;
            }

            auto preview = Ghosts::I().inspectAttemptImportFile(*selectedPath);
            if (!preview.valid) {
                const std::string message = preview.error.empty()
                    ? "The selected file is not a valid APX save."
                    : preview.error;
                FLAlertLayer::create("Import Failed", gd::string(message.c_str()), "OK")->show();
                self->release();
                return;
            }

            self->showImportConfirm_(*selectedPath, mode, preview);
            self->release();
        }
    );
}

void AttemptManagerPopup::showImportConfirm_(
    std::filesystem::path const& source,
    Ghosts::AttemptFileImportMode mode,
    Ghosts::AttemptFileImportPreview const& preview
) {
    if (m_confirmOpen || m_destructiveActionBusy) return;

    m_confirmOpen = true;
    refreshPagingButtons_();
    refreshTabButtons_();

    std::ostringstream ss;
    ss << "File: <cy>" << importFileName_(source) << "</c>\n";
    ss << "Current: <cy>" << preview.currentAttempts << " attempts / "
       << preview.currentPracticeSessions << " practice runs</c>\n";
    ss << "Import: <cy>" << preview.incomingAttempts << " attempts / "
       << preview.incomingPracticeSessions << " practice runs</c>\n\n";

    std::string title;
    std::string confirmText;

    if (mode == Ghosts::AttemptFileImportMode::ReplaceCurrent) {
        title = "Replace Current Save?";
        confirmText = "Replace";
        ss << "<cr>WARNING: This overwrites the current APX save.</c>\n";
        ss << "Data missing from the import will be deleted.\n";
        ss << "If you care about this save, please make a backup first.";
    }
    else {
        title = "Merge APX Saves?";
        confirmText = "Merge";
        ss << "Current data stays; imported data is added.\n";
        ss << "ID conflicts: <cy>" << preview.attemptSerialConflicts << " attempts / "
           << preview.practiceSessionIdConflicts << " practice runs</c>\n";
        ss << "Imported IDs are remapped. Duplicate runs are kept.";
    }

    this->retain();
    createQuickPopup(
        title.c_str(),
        gd::string(ss.str().c_str()),
        "Cancel",
        confirmText.c_str(),
        [this, source, mode](FLAlertLayer*, bool btn2) {
            m_confirmOpen = false;

            if (!btn2 || m_destructiveActionBusy) {
                refreshPagingButtons_();
                refreshTabButtons_();
                this->release();
                return;
            }

            m_destructiveActionBusy = true;
            refreshPagingButtons_();
            refreshTabButtons_();

            std::string error;
            const bool ok = Ghosts::I().importAttemptFile(source, mode, &error);

            m_destructiveActionBusy = false;
            refreshPagingButtons_();
            refreshTabButtons_();

            if (!ok) {
                const std::string message = error.empty()
                    ? "The APX import failed. Check the logs for details."
                    : error;
                FLAlertLayer::create("Import Failed", gd::string(message.c_str()), "OK")->show();
                this->release();
                return;
            }

            m_selectedSerials.clear();
            m_selectedSerial = -1;
            m_selectedRunGroupId = -1;
            m_selectedPracticeSessionId = 0;
            m_normalView = NormalView::Runs;
            m_practiceView = PracticeView::Sessions;
            m_runFilter = RunFilter::All;
            m_page = 0;
            loadAttemptsFromRuntime_();

            const char* message = mode == Ghosts::AttemptFileImportMode::ReplaceCurrent
                ? "The current APX save was replaced successfully.\n\nRecording has been restarted."
                : "The APX saves were merged successfully.\n\nImported IDs were remapped and recording has been restarted.";
            FLAlertLayer::create("Import Complete", gd::string(message), "OK")->show();
            this->release();
        }
    );
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
    float v = fallback;
    if (auto value = parsed.ok()) v = *value;
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

    if (Ghosts::I().isPracticeSessionBeingRecorded(sessionId)) {
        FLAlertLayer::create(
            "Practice Run Is Recording",
            "You cannot delete the practice session that is currently being recorded. Finish that run or exit practice mode first.",
            "OK"
        )->show();
        return;
    }

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
            "Practice attempts are grouped by practice session.\n\n"
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
