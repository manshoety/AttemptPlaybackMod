#include "ghost_manager.hpp"
#include <chrono>

void Ghosts::armFreshRecordingAfterDeleteRestart_() {
        m_forceFreshRecordingAfterDelete = true;

        recording = true;
        recordingBlocked = false;
        block_attempt_push_on_recording_start = false;
        m_justDied = false;

        m_current = Attempt{};
    }

bool Ghosts::deleteAttemptsBySerials(std::vector<int> const& serials) {
        std::unordered_set<int> deleteSet;
        deleteSet.reserve(serials.size());

        for (int serial : serials) {
            if (serial > 0) deleteSet.insert(serial);
        }

        return deleteAttemptsBySerialSet_(deleteSet);
    }

bool Ghosts::deleteAttemptsByEndPercent(float minPercent, float maxPercent) {
        if (minPercent > maxPercent) std::swap(minPercent, maxPercent);

        minPercent = std::clamp(minPercent, 0.f, 100.f);
        maxPercent = std::clamp(maxPercent, 0.f, 100.f);

        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return false;
        }

        std::unordered_set<int> deleteSet;

        for (auto const& entry : m_attemptCatalog) {
            if (entry.practiceAttempt) continue;

            if (entry.endPercent >= minPercent && entry.endPercent <= maxPercent) {
                deleteSet.insert(entry.serial);
            }
        }

        return deleteAttemptsBySerialSet_(deleteSet);
    }

bool Ghosts::deleteCurrentLevelSaveFile() {
        if (!hasCurrentPersistenceTarget_()) {
            log::warn(
                "[APX delete-all] no current persistence target: levelID={}, customSaveId='{}'",
                m_levelIDOnAttach,
                m_customSaveId
            );
            return false;
        }

        const auto path = fileForLevel_(m_levelIDOnAttach);

        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            log::warn(
                "[APX delete-all] exists failed for {}: {}",
                geode::utils::string::pathToString(path),
                ec.message()
            );
            return false;
        }

        if (exists) {
            ec.clear();
            std::filesystem::remove(path, ec);
            if (ec) {
                log::warn(
                    "[APX delete-all] remove failed for {}: {}",
                    geode::utils::string::pathToString(path),
                    ec.message()
                );
                return false;
            }
        }

        stopReplay();
        stopPlayback();

        botActive = false;
        playback = false;
        m_playbackArmed = false;
        m_pendingRearm = false;

        clearHeldInputs_();
        applyBotSafety_(false);
        setdisablePlayerMove(false);
        tagCompositeOwnersHidden_(false);
        forceHidePlayLayerGhostTextLabelOnly();

        clearAllGhostNodes();

        attempts.clear();
        m_current = Attempt{};

        invalidateAttemptPointerCaches_();
        invalidatePrimedPlayerObjectRefs();

        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerSerial = -1;
        m_replayOwnerIndex = -1;
        m_activeOwnerSerial = -1;
        m_practiceCompositeOwnerSerial = -1;

        m_loadedAttemptKeys.clear();
        m_loadedLevelID = 0;
        clearAttemptCatalog_();

        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;

        m_replayIdx1 = 0;
        m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        m_grid.clear();
        m_gridThreshold.clear();
        m_spans.clear();
        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
        m_serialCacheDirty = true;

        invalidateAttemptCounts();

        m_checkpointMgr = CheckpointManager();
        m_needsMigrationRewrite = false;
        m_loadedFileWasLegacyAttempts = false;

        m_justDied = false;
        m_blockAttemptCount = false;
        block_attempt_push_on_recording_start = false;
        recordingBlocked = false;
        recording = true;

        armFreshRecordingAfterDeleteRestart_();

        restartLevel();

        return true;
    }

std::filesystem::path Ghosts::getAttemptExportsDir() const {
        auto dir = Mod::get()->getSaveDir() / "exports";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            log::warn(
                "[APX export] create_directories failed for {}: {}",
                geode::utils::string::pathToString(dir),
                ec.message()
            );
        }
        return dir;
    }

bool Ghosts::exportCurrentLevelAttempts(
        std::filesystem::path const& destination,
        AttemptFileExportMode mode,
        std::string* outError) {
        auto fail = [&](std::string const& error) {
            if (outError) *outError = error;
            log::warn("[APX export] {}", error);
            return false;
        };

        if (outError) outError->clear();
        if (destination.empty()) return fail("No export destination was selected.");
        if (!hasCurrentPersistenceTarget_()) return fail("There is no active level save to export.");

        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return fail("The current APX save could not be read.");
        }

        const auto source = fileForLevel_(m_levelIDOnAttach);
        std::error_code ec;
        if (!std::filesystem::exists(source, ec) || ec) {
            return fail("No APX save file exists for this level yet.");
        }

        if (mode == AttemptFileExportMode::FullFile) {
            ec.clear();
            const auto parent = destination.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, ec);
                if (ec) return fail("Could not create the export folder: " + ec.message());
            }

            ec.clear();
            bool sameFile = false;
            const auto sourceAbs = std::filesystem::absolute(source, ec);
            if (!ec) {
                ec.clear();
                const auto destAbs = std::filesystem::absolute(destination, ec);
                if (!ec) sameFile = sourceAbs.lexically_normal() == destAbs.lexically_normal();
            }

            if (sameFile) return true;

            ec.clear();
            std::filesystem::copy_file(
                source,
                destination,
                std::filesystem::copy_options::overwrite_existing,
                ec
            );
            if (ec) return fail("Failed to copy the APX file: " + ec.message());
            return true;
        }

        if (mode == AttemptFileExportMode::BestNormalAttempt) {
            const APXAttemptDiskInfo* best = nullptr;

            auto better = [](APXAttemptDiskInfo const& a, APXAttemptDiskInfo const& b) {
                if (a.completed != b.completed) return a.completed;
                if (a.endPercent != b.endPercent) return a.endPercent > b.endPercent;
                if (a.endX != b.endX) return a.endX > b.endX;
                return a.serial > b.serial;
            };

            for (auto const& entry : m_attemptCatalog) {
                if (entry.practiceAttempt) continue;
                if (entry.p1Count == 0 && entry.p2Count == 0) continue;
                if (!best || better(entry, *best)) best = &entry;
            }

            if (!best) return fail("There are no saved normal-mode attempts to export.");

            Attempt attempt{};
            bool usedLegacy = false;
            if (!loadAPXAttemptByCatalogEntry(source, *best, attempt, &usedLegacy)) {
                return fail("The best normal-mode attempt could not be loaded from the APX file.");
            }

            attempt.persistedOnDisk = true;
            attempt.recordedThisSession = false;

            PracticePath emptyPath{};
            std::vector<Attempt> exportedAttempts;
            exportedAttempts.push_back(std::move(attempt));
            if (!saveAPXFileCurrent(destination, exportedAttempts, emptyPath)) {
                return fail("Failed to write the best normal-mode attempt export.");
            }
            return true;
        }

        const PracticeSession* bestSession = nullptr;
        auto sessionReplayEnd = [](PracticeSession const& session) {
            double end = 0.0;
            for (auto const& segment : session.segments) {
                end = std::max(end, segment.absEnd());
            }
            return end;
        };
        auto betterSession = [&](PracticeSession const& a, PracticeSession const& b) {
            if (a.completed != b.completed) return a.completed;
            if (a.endX != b.endX) return a.endX > b.endX;
            const double aEnd = sessionReplayEnd(a);
            const double bEnd = sessionReplayEnd(b);
            if (aEnd != bEnd) return aEnd > bEnd;
            return a.sessionId > b.sessionId;
        };

        for (auto const& session : m_attemptCatalogPracticePath.sessions) {
            if (session.segments.empty()) continue;
            if (!bestSession || betterSession(session, *bestSession)) bestSession = &session;
        }

        if (!bestSession) return fail("There is no saved practice replay path to export.");

        std::vector<int> pathSerials;
        std::unordered_set<int> seenPathSerials;
        pathSerials.reserve(bestSession->segments.size());

        for (auto const& segment : bestSession->segments) {
            if (segment.ownerSerial <= 0) {
                return fail("The best practice path contains an invalid attempt reference.");
            }
            if (seenPathSerials.insert(segment.ownerSerial).second) {
                pathSerials.push_back(segment.ownerSerial);
            }
        }

        if (pathSerials.empty()) return fail("The best practice path does not contain any replay attempts.");

        std::vector<Attempt> pathAttempts;
        pathAttempts.reserve(pathSerials.size());

        for (int serial : pathSerials) {
            const APXAttemptDiskInfo* entry = findCatalogEntryBySerialAndPractice_(serial, true);
            if (!entry) {
                return fail(fmt::format("Practice path attempt serial {} is missing from the APX file.", serial));
            }

            Attempt attempt{};
            bool usedLegacy = false;
            if (!loadAPXAttemptByCatalogEntry(source, *entry, attempt, &usedLegacy)) {
                return fail(fmt::format("Practice path attempt serial {} could not be loaded.", serial));
            }

            attempt.practiceAttempt = true;
            attempt.persistedOnDisk = true;
            attempt.recordedThisSession = false;
            pathAttempts.push_back(std::move(attempt));
        }

        PracticeSession exportedSession = *bestSession;
        exportedSession.allAttemptSerials = pathSerials;
        exportedSession.frozen = false;
        exportedSession.updateSpan();

        PracticePath exportedPath{};
        exportedPath.sessions.push_back(std::move(exportedSession));
        exportedPath.activeSessionId = bestSession->sessionId;
        exportedPath.selectedSessionId = bestSession->sessionId;
        exportedPath.frozen = false;

        if (!saveAPXFileCurrent(destination, pathAttempts, exportedPath)) {
            return fail("Failed to write the best practice replay-path export.");
        }
        return true;
    }

Ghosts::AttemptFileImportPreview Ghosts::inspectAttemptImportFile(std::filesystem::path const& source) {
        AttemptFileImportPreview preview{};
        if (source.empty()) {
            preview.error = "No import file was selected.";
            return preview;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(source, ec);
        if (ec || !exists) {
            preview.error = ec ? "Could not access the selected file: " + ec.message() : "The selected file does not exist.";
            return preview;
        }

        APXCatalogScanResult incoming{};
        if (!scanAPXFileCatalog(source, incoming)) {
            preview.error = "The selected file is not a valid supported APX save.";
            return preview;
        }

        preview.incomingAttempts = incoming.attempts.size();
        preview.incomingPracticeSessions = incoming.practicePath.sessions.size();
        preview.incomingIsLegacy = incoming.loadedLegacy;

        for (auto const& entry : incoming.attempts) {
            if (entry.practiceAttempt) preview.incomingContainsPractice = true;
            else preview.incomingContainsNormal = true;
        }
        if (!incoming.practicePath.sessions.empty()) preview.incomingContainsPractice = true;

        if (!hasCurrentPersistenceTarget_()) {
            preview.error = "There is no active level save to import into.";
            return preview;
        }

        refreshAttemptCatalogForCurrentLevel_(true, true);
        preview.currentAttempts = m_attemptCatalog.size();
        preview.currentPracticeSessions = m_attemptCatalogPracticePath.sessions.size();

        std::unordered_set<int> currentSerials;
        currentSerials.reserve(m_attemptCatalog.size());
        for (auto const& entry : m_attemptCatalog) currentSerials.insert(entry.serial);

        for (auto const& entry : incoming.attempts) {
            if (currentSerials.count(entry.serial) != 0) ++preview.attemptSerialConflicts;
        }

        std::unordered_set<int> currentSessionIds;
        currentSessionIds.reserve(m_attemptCatalogPracticePath.sessions.size());
        for (auto const& session : m_attemptCatalogPracticePath.sessions) {
            currentSessionIds.insert(session.sessionId);
        }
        for (auto const& session : incoming.practicePath.sessions) {
            if (currentSessionIds.count(session.sessionId) != 0) ++preview.practiceSessionIdConflicts;
        }

        preview.valid = true;
        return preview;
    }

bool Ghosts::importAttemptFile(
        std::filesystem::path const& source,
        AttemptFileImportMode mode,
        std::string* outError) {
        auto fail = [&](std::string const& error) {
            if (outError) *outError = error;
            log::warn("[APX import] {}", error);
            return false;
        };

        if (outError) outError->clear();
        if (source.empty()) return fail("No import file was selected.");
        if (!hasCurrentPersistenceTarget_()) return fail("There is no active level save to import into.");
        if (m_isSaving) return fail("The APX save is currently busy. Try again after the current save finishes.");

        std::error_code ec;
        if (!std::filesystem::exists(source, ec) || ec) {
            return fail(ec ? "Could not access the selected file: " + ec.message() : "The selected file does not exist.");
        }


        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return fail("The current APX save could not be prepared for import.");
        }

        std::vector<Attempt> incomingAttempts;
        PracticePath incomingPath;

        if (!loadAPXFileWithMigration(source, incomingAttempts, incomingPath, false)) {
            return fail("The selected APX file could not be loaded.");
        }

        std::unordered_set<int> incomingSerials;
        incomingSerials.reserve(incomingAttempts.size());
        for (auto const& attempt : incomingAttempts) {
            if (attempt.serial <= 0 || !incomingSerials.insert(attempt.serial).second) {
                return fail("The selected APX file contains invalid or duplicate attempt serial IDs.");
            }
        }

        std::unordered_set<int> incomingSessionIds;
        incomingSessionIds.reserve(incomingPath.sessions.size());
        for (auto const& session : incomingPath.sessions) {
            if (session.sessionId <= 0 || !incomingSessionIds.insert(session.sessionId).second) {
                return fail("The selected APX file contains invalid or duplicate practice-session IDs.");
            }
        }

        auto validatePracticeRefs = [&](PracticePath const& path, std::unordered_set<int> const& serials) {
            for (auto const& session : path.sessions) {
                for (auto const& segment : session.segments) {
                    if (segment.ownerSerial <= 0 || serials.count(segment.ownerSerial) == 0) return false;
                }
            }
            return true;
        };

        if (!validatePracticeRefs(incomingPath, incomingSerials)) {
            return fail("The selected APX practice path references attempts that are missing from the file.");
        }

        std::vector<Attempt> finalAttempts;
        PracticePath finalPath;

        if (mode == AttemptFileImportMode::ReplaceCurrent) {
            finalAttempts = std::move(incomingAttempts);
            finalPath = std::move(incomingPath);
        }
        else {
            const auto currentPath = fileForLevel_(m_levelIDOnAttach);

            ec.clear();
            const auto sourceAbs = std::filesystem::absolute(source, ec);
            bool sameFile = false;
            if (!ec) {
                ec.clear();
                const auto currentAbs = std::filesystem::absolute(currentPath, ec);
                if (!ec) sameFile = sourceAbs.lexically_normal() == currentAbs.lexically_normal();
            }
            if (sameFile) return fail("You cannot merge the current save file into itself.");

            if (!loadAPXFileWithMigration(currentPath, finalAttempts, finalPath)) {
                return fail("The current APX save could not be loaded for merging.");
            }

            int maxSerial = 0;
            for (auto const& attempt : finalAttempts) maxSerial = std::max(maxSerial, attempt.serial);

            std::unordered_map<int, int> serialMap;
            serialMap.reserve(incomingAttempts.size());
            for (auto& attempt : incomingAttempts) {
                const int oldSerial = attempt.serial;
                const int newSerial = ++maxSerial;
                serialMap.emplace(oldSerial, newSerial);
                attempt.serial = newSerial;
                attempt.persistedOnDisk = true;
                attempt.recordedThisSession = false;
                finalAttempts.push_back(std::move(attempt));
            }

            const bool currentHadSessions = !finalPath.sessions.empty();
            int maxSessionId = 0;
            for (auto const& session : finalPath.sessions) maxSessionId = std::max(maxSessionId, session.sessionId);

            const int oldIncomingActive = incomingPath.activeSessionId;
            const int oldIncomingSelected = incomingPath.selectedSessionId;
            std::unordered_map<int, int> sessionMap;
            sessionMap.reserve(incomingPath.sessions.size());

            for (auto& session : incomingPath.sessions) {
                const int oldSessionId = session.sessionId;
                const int newSessionId = ++maxSessionId;
                sessionMap.emplace(oldSessionId, newSessionId);
                session.sessionId = newSessionId;

                std::vector<int> mappedAttemptSerials;
                mappedAttemptSerials.reserve(session.allAttemptSerials.size());
                std::unordered_set<int> seenMapped;
                for (int oldSerial : session.allAttemptSerials) {
                    auto it = serialMap.find(oldSerial);
                    if (it == serialMap.end()) continue;
                    if (seenMapped.insert(it->second).second) mappedAttemptSerials.push_back(it->second);
                }
                session.allAttemptSerials = std::move(mappedAttemptSerials);

                for (auto& segment : session.segments) {
                    auto it = serialMap.find(segment.ownerSerial);
                    if (it == serialMap.end()) {
                        return fail("An imported practice path reference could not be remapped during merge.");
                    }
                    segment.ownerSerial = it->second;
                }

                session.updateSpan();
                finalPath.sessions.push_back(std::move(session));
            }

            if (!currentHadSessions) {
                if (auto it = sessionMap.find(oldIncomingActive); it != sessionMap.end()) {
                    finalPath.activeSessionId = it->second;
                }
                if (auto it = sessionMap.find(oldIncomingSelected); it != sessionMap.end()) {
                    finalPath.selectedSessionId = it->second;
                }
                finalPath.frozen = false;
            }
        }

        for (auto& attempt : finalAttempts) {
            attempt.persistedOnDisk = true;
            attempt.recordedThisSession = false;
        }

        auto normalizePathIds = [](PracticePath& path) {
            auto hasSession = [&](int id) {
                if (id <= 0) return false;
                return std::any_of(
                    path.sessions.begin(),
                    path.sessions.end(),
                    [&](PracticeSession const& session) { return session.sessionId == id; }
                );
            };

            if (path.sessions.empty()) {
                path.activeSessionId = 0;
                path.selectedSessionId = 0;
                path.frozen = false;
                return;
            }

            for (auto& session : path.sessions) session.updateSpan();
            if (!hasSession(path.activeSessionId)) path.activeSessionId = path.sessions.front().sessionId;
            if (!hasSession(path.selectedSessionId)) path.selectedSessionId = path.activeSessionId;
        };
        normalizePathIds(finalPath);

        const auto destination = fileForLevel_(m_levelIDOnAttach);
        if (!saveAPXFileCurrent(destination, finalAttempts, finalPath)) {
            return fail("Failed to write the imported APX data into the current level save.");
        }

        clearRuntimeReplayStateAfterAttemptDeletion_();
        clearAllGhostNodes();
        attempts.clear();
        m_current = Attempt{};
        m_loadedAttemptKeys.clear();
        m_loadedLevelID = 0;
        m_lastWinSerial = -1;
        m_nextAttemptSerial = 1;

        invalidateAttemptPointerCaches_();
        invalidatePrimedPlayerObjectRefs();
        m_preloadOrder.clear();
        m_initialAttemptsToSet.clear();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeIndices.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;
        m_spans.clear();
        m_grid.clear();
        m_gridThreshold.clear();
        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;
        m_serialCacheDirty = true;
        invalidateAttemptCounts();

        m_checkpointMgr.restorePath(finalPath);
        m_needsMigrationRewrite = false;
        m_loadedFileWasLegacyAttempts = false;
        clearAttemptCatalog_();
        if (!scanAttemptCatalogForLevel_(m_levelIDOnAttach)) {
            return fail("The imported file was written, but its new catalog could not be rebuilt.");
        }

        recording = true;
        recordingBlocked = false;
        m_justDied = false;
        m_blockAttemptCount = false;
        block_attempt_push_on_recording_start = false;
        armFreshRecordingAfterDeleteRestart_();
        restartLevel();
        return true;
    }

std::filesystem::path Ghosts::attemptsDir_() const {
        if (m_attemptsDir.empty()) {
            auto base = Mod::get()->getSaveDir();
            m_attemptsDir = base / "attempts";

            std::error_code ec;
            std::filesystem::create_directories(m_attemptsDir, ec);
            if (ec) {
                log::warn(
                    "[SAVE FILE] create_directories failed: {} ({})",
                    geode::utils::string::pathToString(m_attemptsDir),
                    ec.message()
                );
            }
        }
        return m_attemptsDir;
    }

std::filesystem::path Ghosts::fileForLevel_(int levelID) const {
        auto dir = attemptsDir_();

        if (!m_customSaveId.empty()) {
            return dir / (m_customSaveId + "_attempts.apx");
        }

        return dir / (std::to_string(levelID) + "_attempts.apx");
    }

void Ghosts::ensureHeader_(std::filesystem::path const& path) {
        std::error_code ec;

        ec.clear();
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            log::warn(
                "[SAVE FILE] exists failed for {}: {}",
                geode::utils::string::pathToString(path),
                ec.message()
            );
            return;
        }

        uintmax_t fsz = 0;
        if (exists) {
            ec.clear();
            fsz = std::filesystem::file_size(path, ec);
            if (ec) {
                log::warn(
                    "[SAVE FILE] file_size failed for {}: {}",
                    geode::utils::string::pathToString(path),
                    ec.message()
                );
                return;
            }
        }

        if (exists && fsz >= sizeof(APXHeader)) {
            std::ifstream checkFile(path, std::ios::binary);
            if (checkFile) {
                APXHeader existing{};
                checkFile.read(reinterpret_cast<char*>(&existing), sizeof(existing));
                if (checkFile && std::string(existing.magic, 4) == "APX2") {
                    return;
                }
            }
        }

        std::ofstream hout(path, std::ios::binary | std::ios::trunc);
        if (!hout) {
            log::warn(
                "[SAVE FILE] failed to open header file for write: {}",
                geode::utils::string::pathToString(path)
            );
            return;
        }

        APXHeader h{};
        std::memcpy(h.magic, "APX2", 4);
        h.version = kAPXVersion;
        hout.write(reinterpret_cast<const char*>(&h), sizeof(h));
        hout.flush();
    }

void Ghosts::migrateLegacyCustomLevelFiles_(
        int levelID,
        std::string const& legacyNameHash,
        std::string const& canonicalSaveId) {
        if (canonicalSaveId.empty()) return;

        const auto dir = attemptsDir_();
        const auto canonicalPath = dir / (canonicalSaveId + "_attempts.apx");

        std::vector<std::filesystem::path> legacyPaths;
        auto addLegacy = [&](std::filesystem::path const& p) {
            if (p == canonicalPath) return;
            std::error_code ec;
            if (!std::filesystem::exists(p, ec) || ec) return;
            if (std::find(legacyPaths.begin(), legacyPaths.end(), p) == legacyPaths.end()) {
                legacyPaths.push_back(p);
            }
        };

        if (!legacyNameHash.empty()) {
            addLegacy(dir / (legacyNameHash + "_attempts.apx"));
        }

        if (levelID > 0) {
            addLegacy(dir / (std::to_string(levelID) + "_attempts.apx"));
        }

        if (legacyPaths.empty()) return;

        std::error_code ec;
        const bool canonicalExists = std::filesystem::exists(canonicalPath, ec) && !ec;


        if (!canonicalExists && legacyPaths.size() == 1) {
            ec.clear();
            std::filesystem::rename(legacyPaths.front(), canonicalPath, ec);
            if (!ec) {
                log::info(
                    "[APX custom-id] migrated {} -> {}",
                    geode::utils::string::pathToString(legacyPaths.front()),
                    geode::utils::string::pathToString(canonicalPath)
                );
                return;
            }

            log::warn(
                "[APX custom-id] rename migration failed ({}); falling back to merge",
                ec.message()
            );
        }

        std::vector<Attempt> mergedAttempts;
        PracticePath mergedPath;

        if (canonicalExists) {
            if (!loadAPXFileWithMigration(canonicalPath, mergedAttempts, mergedPath)) {
                log::warn(
                    "[APX custom-id] refusing migration because canonical file could not be read: {}",
                    geode::utils::string::pathToString(canonicalPath)
                );
                return;
            }
        }

        int maxSerial = 0;
        for (auto const& a : mergedAttempts) maxSerial = std::max(maxSerial, a.serial);

        int maxSessionId = 0;
        for (auto const& s : mergedPath.sessions) maxSessionId = std::max(maxSessionId, s.sessionId);

        std::vector<std::filesystem::path> mergedLegacyPaths;

        for (auto const& legacyPath : legacyPaths) {
            std::vector<Attempt> incomingAttempts;
            PracticePath incomingPath;

            if (!loadAPXFileWithMigration(legacyPath, incomingAttempts, incomingPath)) {
                log::warn(
                    "[APX custom-id] could not read legacy file; leaving it untouched: {}",
                    geode::utils::string::pathToString(legacyPath)
                );
                continue;
            }

            mergedLegacyPaths.push_back(legacyPath);

            std::unordered_map<int, int> serialMap;
            serialMap.reserve(incomingAttempts.size());

            for (auto& a : incomingAttempts) {
                const int oldSerial = a.serial;
                const int newSerial = ++maxSerial;
                serialMap[oldSerial] = newSerial;
                a.serial = newSerial;
                a.persistedOnDisk = true;
                a.recordedThisSession = false;
                mergedAttempts.push_back(std::move(a));
            }

            const int oldActiveSession = incomingPath.activeSessionId;
            const int oldSelectedSession = incomingPath.selectedSessionId;
            std::unordered_map<int, int> sessionMap;
            sessionMap.reserve(incomingPath.sessions.size());

            for (auto& session : incomingPath.sessions) {
                const int oldSessionId = session.sessionId;
                const int newSessionId = ++maxSessionId;
                sessionMap[oldSessionId] = newSessionId;
                session.sessionId = newSessionId;

                for (int& serial : session.allAttemptSerials) {
                    if (auto it = serialMap.find(serial); it != serialMap.end()) {
                        serial = it->second;
                    }
                }
                for (auto& seg : session.segments) {
                    if (auto it = serialMap.find(seg.ownerSerial); it != serialMap.end()) {
                        seg.ownerSerial = it->second;
                    }
                }

                session.updateSpan();
                mergedPath.sessions.push_back(std::move(session));
            }

            if (mergedPath.activeSessionId == 0) {
                if (auto it = sessionMap.find(oldActiveSession); it != sessionMap.end()) {
                    mergedPath.activeSessionId = it->second;
                }
            }
            if (mergedPath.selectedSessionId == 0) {
                if (auto it = sessionMap.find(oldSelectedSession); it != sessionMap.end()) {
                    mergedPath.selectedSessionId = it->second;
                }
            }
        }

        if (!canonicalExists && mergedLegacyPaths.empty()) {
            return;
        }

        if (!saveAPXFileCurrent(canonicalPath, mergedAttempts, mergedPath)) {
            log::warn(
                "[APX custom-id] merged migration failed; legacy files were not removed: {}",
                geode::utils::string::pathToString(canonicalPath)
            );
            return;
        }

        for (auto const& legacyPath : mergedLegacyPaths) {
            ec.clear();
            std::filesystem::remove(legacyPath, ec);
            if (ec) {
                log::warn(
                    "[APX custom-id] migrated data but could not remove old file {}: {}",
                    geode::utils::string::pathToString(legacyPath),
                    ec.message()
                );
            }
        }
    }

void Ghosts::refreshLevelName(int levelID) {
        m_customSaveId.clear();
        if (!m_pl || !m_pl->m_level) return;

        auto* level = m_pl->m_level;
        const std::string lvlName = level->m_levelName;
        const std::string legacyNameHash = lvlName.empty() ? std::string{} : customLevelSaveId(lvlName);

        // Editor Level ID API goop
        if (level->m_levelType == GJLevelType::Editor) {
            const int editorLevelId = EditorIDs::getID(level);
            if (editorLevelId > 0) {
                m_customSaveId = "editor-" + std::to_string(editorLevelId);
                migrateLegacyCustomLevelFiles_(levelID, legacyNameHash, m_customSaveId);
                return;
            }
        }

        // Compatibility with the old way I did it
        if (levelID < 120 && !legacyNameHash.empty()) {
            m_customSaveId = legacyNameHash;
         }
    }

bool Ghosts::rewriteWholeFileToCurrent_(std::filesystem::path const& path) {
        const size_t validAttempts = countAttemptsWithData_(attempts);

        if (validAttempts == 0) {
            log::error(
                "[APX save] ABORTING migration rewrite for {}: 0 valid attempts in memory. "
                "Refusing to overwrite existing file.",
                geode::utils::string::pathToString(path)
            );
            return false;
        }
        
        auto tmp = path;
        tmp += ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                log::warn(
                    "[APX save] failed to open temp migration file: {}",
                    geode::utils::string::pathToString(tmp)
                );
                return false;
            }

            APXHeader h{};
            std::memcpy(h.magic, "APX2", 4);
            h.version = kAPXVersion;
            out.write(reinterpret_cast<const char*>(&h), sizeof(h));
            if (!out.good()) return false;

            size_t writtenAttempts = 0;
            for (auto const& a : attempts) {
                if (!attemptHasData_(a)) continue;
                if (!writeAPXAttemptCompact(out, a)) return false;
                ++writtenAttempts;
            }

            if (writtenAttempts == 0) {
                log::error(
                    "[APX save] ABORTING migration rewrite for {}: wrote 0 attempts unexpectedly.",
                    geode::utils::string::pathToString(path)
                );
                return false;
            }

            if (!writeAPXPracticePath(out, m_checkpointMgr.getPath())) {
                return false;
            }

            out.flush();
            if (!out.good()) return false;
        }

        if (!replaceAPXFileSafely_(tmp, path)) {
            log::warn(
                "[APX save] migration replacement failed {} -> {}",
                geode::utils::string::pathToString(tmp),
                geode::utils::string::pathToString(path)
            );
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }

        for (auto& a : attempts) {
            if (attemptHasData_(a)) a.persistedOnDisk = true;
        }

        m_needsMigrationRewrite = false;
        m_loadedFileWasLegacyAttempts = false;
        return true;
    }

int Ghosts::saveNewAttemptsForLevel_(int levelID) {
        if (!hasPersistenceTargetForLevel_(levelID)) {
            log::warn(
                "[APX save] no persistence target: levelID={}, customSaveId='{}'",
                levelID,
                m_customSaveId
            );
            return 0;
        }
        if (m_isSaving) return 0;
        m_isSaving = true;

        auto path = fileForLevel_(levelID);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            log::warn(
                "[APX save] create_directories failed: {} ({})",
                geode::utils::string::pathToString(path.parent_path()),
                ec.message()
            );
            m_isSaving = false;
            return 0;
        }

        const size_t validAttempts = countAttemptsWithData_(attempts);

        int unsavedValidAttempts = 0;
        for (auto const& a : attempts) {
            if (!attemptHasData_(a)) continue;
            if (!a.recordedThisSession) continue;
            if (a.persistedOnDisk) continue;
            ++unsavedValidAttempts;
        }

        const bool canWritePracticePathOnly =
            !m_needsMigrationRewrite &&
            m_checkpointMgr.isDirty() &&
            m_attemptCatalogScanned && !m_attemptCatalog.empty();

        if (validAttempts == 0 && !canWritePracticePathOnly) {
            m_isSaving = false;
            return 0;
        }

        if (!m_needsMigrationRewrite && unsavedValidAttempts == 0 && !m_checkpointMgr.isDirty()) {
            m_isSaving = false;
            return 0;
        }


        int written = 0;
        bool rewroteWholeFile = false;
        bool wrotePracticePath = false;

        if (m_needsMigrationRewrite) {
            if (!rewriteWholeFileToCurrent_(path)) {
                m_isSaving = false;
                return 0;
            }
            rewroteWholeFile = true;
        } else {
            ensureHeader_(path);
        }

        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) {
            m_isSaving = false;
            return 0;
        }

        for (auto& a : attempts) {
            if (!attemptHasData_(a)) continue;
            if (!a.recordedThisSession) continue;
            if (a.persistedOnDisk) continue;

            if (!writeAPXAttemptCompact(out, a)) {
                m_isSaving = false;
                return written;
            }
            a.persistedOnDisk = true;
            ++written;
        }

        if (m_checkpointMgr.isDirty()) {
            if (writeAPXPracticePath(out, m_checkpointMgr.getPath())) {
                m_checkpointMgr.clearDirty();
                wrotePracticePath = true;
            } else {
                log::warn("[APX save] PATH write failed (stream bad)");
            }
        }

        out.flush();

        if (written > 0 || rewroteWholeFile || wrotePracticePath) {
            clearAttemptCatalog_();
        }

        m_isSaving = false;
        return written;
    }

int Ghosts::loadFromDisk_(int levelID, LoadFilter const& flt) {
        const auto loadStart = std::chrono::steady_clock::now();
        if (m_loadedLevelID == levelID && (m_attemptCatalogScanned || !attempts.empty())) {
            scanAttemptCatalogForLevel_(levelID);
            return 0;
        }

        if (m_loadedLevelID != levelID) {
            m_checkpointMgr = CheckpointManager();
            m_needsMigrationRewrite = false;
            m_loadedFileWasLegacyAttempts = false;
            clearAttemptCatalog_();
        }

        const auto catalogStart = std::chrono::steady_clock::now();
        const bool catalogReady = scanAttemptCatalogForLevel_(levelID);
        const auto catalogMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - catalogStart).count();

        auto path = fileForLevel_(levelID);
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            log::warn("[APX load] !in {}", geode::utils::string::pathToString(path));
            return 0;
        }

        APXHeader h{};
        in.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!in || std::memcmp(h.magic, "APX2", 4) != 0) {
            log::warn("[APX load] bad header for {}", geode::utils::string::pathToString(path));
            return 0;
        }

        if (catalogReady && m_attemptCatalogCurrentFormat && h.version == kAPXVersion &&
            !flt.practice.has_value() && !flt.minPct.has_value() &&
            !flt.maxPct.has_value() && flt.maxNonPractice == 0) {
            m_checkpointMgr.restorePath(m_attemptCatalogPracticePath);
            m_loadedLevelID = levelID;

            const auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - loadStart).count();
            log::info(
                "[APX load] Catalog-only {} attempts from {} (version {}) in {} ms (catalog={} ms, resident={})",
                m_attemptCatalog.size(),
                geode::utils::string::pathToString(path),
                h.version,
                loadMs,
                catalogMs,
                attempts.size()
            );
            log::info("[APX load] migration=no");
            return 0;
        }

        const bool headerIsLegacy = (h.version < kAPXVersion);
        const bool oldWavePointBitMeaning = h.version < kAPXVersion_WavePointThisFrame;

        const bool forceFullMigrationLoad = headerIsLegacy || oldWavePointBitMeaning;

        if (forceFullMigrationLoad) {
            m_needsMigrationRewrite = true;
        }

        size_t loaded = 0;
        size_t nonPracticeLoaded = 0;
        uint32_t maxSerialSeen = 0;

        while (in) {
            uint32_t type = 0, sz = 0;
            in.read(reinterpret_cast<char*>(&type), sizeof(type));
            in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
            if (!in) break;

            std::streampos chunkStart = in.tellg();

            if (type == kChunk_ATT3) {
                Attempt a{};
                if (!readAPXAttemptCompact(in, sz, a)) {
                    log::warn(
                        "[APX load] skipping malformed ATT3 chunk size={} after {} successfully loaded attempts",
                        sz,
                        loaded
                    );
                    continue;
                }

                if (oldWavePointBitMeaning) {
                    clearAPXWavePointThisFrameBits(a);
                }

                const bool isPractice = a.practiceAttempt;
                const uint64_t attemptKey = attemptIdentityKey_(a.serial, isPractice);
                if (m_loadedAttemptKeys.count(attemptKey)) {
                    continue;
                }

                maxSerialSeen = std::max(maxSerialSeen, static_cast<uint32_t>(a.serial));

                bool loadThis = true;

                if (!(forceFullMigrationLoad)) {
                    if (flt.practice.has_value() && flt.practice.value() != isPractice) loadThis = false;
                    if (flt.minPct.has_value() && a.startPercent < flt.minPct.value()) loadThis = false;
                    if (flt.maxPct.has_value() && a.startPercent > flt.maxPct.value()) loadThis = false;
                    if (loadThis && !isPractice && flt.maxNonPractice > 0
                        && nonPracticeLoaded >= flt.maxNonPractice) loadThis = false;
                }

                if (!loadThis) {
                    continue;
                }

                m_loadedAttemptKeys.insert(attemptKey);
                pushAttempt(std::move(a), false);

                ++loaded;
                if (!isPractice) ++nonPracticeLoaded;
            }
            else if (type == kChunk_ATTZ) {
                m_loadedFileWasLegacyAttempts = true;
                m_needsMigrationRewrite = true;

                std::streampos attzChunkStart = in.tellg();

                APXMeta m{};
                in.read(reinterpret_cast<char*>(&m), sizeof(m));
                if (!in) break;

                const bool isPractice = (m.flags & 1u) != 0;
                const uint64_t attemptKey = attemptIdentityKey_(static_cast<int>(m.serial), isPractice);
                if (m_loadedAttemptKeys.count(attemptKey)) {
                    auto now = in.tellg();
                    auto readSoFar = (std::streamoff)(now - attzChunkStart);
                    auto remain = (std::streamoff)sz - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                    continue;
                }

                maxSerialSeen = std::max(maxSerialSeen, m.serial);

                bool loadThis = true;

                if (!forceFullMigrationLoad) {
                    if (flt.practice.has_value() && flt.practice.value() != isPractice) loadThis = false;
                    if (flt.minPct.has_value() && m.startPercent < flt.minPct.value()) loadThis = false;
                    if (flt.maxPct.has_value() && m.startPercent > flt.maxPct.value()) loadThis = false;
                    if (loadThis && !isPractice && flt.maxNonPractice > 0
                        && nonPracticeLoaded >= flt.maxNonPractice) loadThis = false;
                }

                auto skipToEndOfChunk = [&]() {
                    auto now = in.tellg();
                    auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                    auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                };

                auto readFrames = [&](std::vector<Frame>& dst, uint32_t n) {
                    dst.resize(n);
                    for (uint32_t i = 0; i < n; ++i) {
                        APXFrame s{};
                        in.read(reinterpret_cast<char*>(&s), sizeof(s));

                        Frame f{};
                        f.x = s.x;
                        f.y = s.y;
                        f.rot = s.rot;
                        f.vehicleSize = s.vehicleSize;
                        f.waveSize = s.waveSize;
                        f.mode = static_cast<IconType>(s.mode);
                        f.t = s.t;
                        frameFromFlags_(s, f);

                        dst[i] = f;
                    }
                };

                if (!loadThis) {
                    skipToEndOfChunk();
                    continue;
                }

                Attempt a{};
                a.serial = static_cast<int>(m.serial);
                a.startPercent = m.startPercent;
                a.endPercent = m.endPercent;
                a.practiceAttempt = isPractice;
                a.completed = (m.flags & (1u << 1)) != 0;
                a.hadDual = (m.flags & (1u << 2)) != 0;
                a.startX = m.startX;
                a.startY = m.startY;
                a.baseTimeOffset = m.baseTimeOffset;
                a.seed = static_cast<uintptr_t>(m.seed);

                readFrames(a.p1, m.p1Count);
                readFrames(a.p2, m.p2Count);

                if (oldWavePointBitMeaning) {
                    clearAPXWavePointThisFrameBits(a);
                }

                if (!a.p1.empty()) {
                    a.startX = static_cast<float>(a.p1.front().x);
                    a.startY = static_cast<float>(a.p1.front().y);
                    a.endX = static_cast<float>(a.p1.back().x);
                } else if (!a.p2.empty()) {
                    a.startX = static_cast<float>(a.p2.front().x);
                    a.startY = static_cast<float>(a.p2.front().y);
                    a.endX = static_cast<float>(a.p2.back().x);
                }

                a.persistedOnDisk = true;
                a.recordedThisSession = false;

                m_loadedAttemptKeys.insert(attemptKey);
                pushAttempt(std::move(a), false);

                ++loaded;
                if (!isPractice) ++nonPracticeLoaded;

                auto now = in.tellg();
                auto readSoFar = static_cast<std::streamoff>(now - attzChunkStart);
                auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                if (remain > 0) in.seekg(remain, std::ios::cur);
            }
            else if (type == kChunk_PATH) {
                PracticePath loadedPath;

                if (readAPXPracticePath(in, sz, loadedPath)) {
                    m_checkpointMgr.restorePath(loadedPath);
                } else {
                    auto now = in.tellg();
                    auto readSoFar = static_cast<std::streamoff>(now - chunkStart);
                    auto remain = static_cast<std::streamoff>(sz) - readSoFar;
                    if (remain > 0) in.seekg(remain, std::ios::cur);
                }
            }
            else if (type == kChunk_CheckpointZ2 || type == kChunk_SEGZ) {
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            } else {
                in.seekg(static_cast<std::streamoff>(sz), std::ios::cur);
            }
        }

        m_loadedLevelID = levelID;

        if (m_nextAttemptSerial <= static_cast<int>(maxSerialSeen)) {
            m_nextAttemptSerial = static_cast<int>(maxSerialSeen) + 1;
        }

        const auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - loadStart).count();
        log::info(
            "[APX load] Loaded {} attempts from {} (version {}) in {} ms",
            loaded,
            geode::utils::string::pathToString(path),
            h.version,
            loadMs
        );

        log::info(
            "[APX load] migration={}",
            m_needsMigrationRewrite ? "yes" : "no"
        );

        in.close();

        if (m_needsMigrationRewrite) {
            if (oldWavePointBitMeaning) {
                log::info(
                    "[APX migration] clearing old wavePointThisFrame/sliding bit and rewriting {} to APX v{}",
                    geode::utils::string::pathToString(path),
                    kAPXVersion
                );
            } else {
                log::info(
                    "[APX migration] rewriting {} to APX v{}",
                    geode::utils::string::pathToString(path),
                    kAPXVersion
                );
            }

            if (!rewriteWholeFileToCurrent_(path)) {
                log::warn(
                    "[APX migration] failed to rewrite migration file: {}",
                    geode::utils::string::pathToString(path)
                );
            }
        }

        return static_cast<int>(loaded);
    }

int Ghosts::saveNewAttemptsForCurrentLevel() {
        if (!hasCurrentPersistenceTarget_()) {
            log::warn(
                "[APX save] no current persistence target: levelID={}, customSaveId='{}'",
                m_levelIDOnAttach,
                m_customSaveId
            );
            return 0;
        }

        return saveNewAttemptsForLevel_(m_levelIDOnAttach);
    }

bool Ghosts::refreshAttemptCatalogForCurrentLevel_(bool forceRescan, bool pushCurrentAttempt) {
        if (!hasCurrentPersistenceTarget_()) {
            clearAttemptCatalog_();
            return false;
        }

        // write already-pushed recordedThisSession attempts so the catalog can see them from disk
        flushPendingSaves_();

        if (forceRescan) {
            clearAttemptCatalog_();
        }

        return scanAttemptCatalogForLevel_(m_levelIDOnAttach);
    }

void Ghosts::prunePracticePathSerials_(
        PracticePath& path,
        std::unordered_set<int> const& deleteSet) {
        if (deleteSet.empty()) return;

        for (auto& session : path.sessions) {
            session.allAttemptSerials.erase(
                std::remove_if(
                    session.allAttemptSerials.begin(),
                    session.allAttemptSerials.end(),
                    [&](int serial) {
                        return deleteSet.count(serial) != 0;
                    }
                ),
                session.allAttemptSerials.end()
            );

            session.segments.erase(
                std::remove_if(
                    session.segments.begin(),
                    session.segments.end(),
                    [&](PracticeSegment const& seg) {
                        return deleteSet.count(seg.ownerSerial) != 0;
                    }
                ),
                session.segments.end()
            );

            session.updateSpan();
        }

        path.sessions.erase(
            std::remove_if(
                path.sessions.begin(),
                path.sessions.end(),
                [](PracticeSession const& session) {
                    return session.allAttemptSerials.empty() && session.segments.empty();
                }
            ),
            path.sessions.end()
        );

        auto hasSession = [&](int id) {
            if (id <= 0) return false;
            return std::any_of(
                path.sessions.begin(),
                path.sessions.end(),
                [&](PracticeSession const& session) {
                    return session.sessionId == id;
                }
            );
        };

        if (path.sessions.empty()) {
            path.activeSessionId = 0;
            path.selectedSessionId = 0;
            path.frozen = false;
            return;
        }

        if (!hasSession(path.activeSessionId)) {
            path.activeSessionId = path.sessions.front().sessionId;
        }

        if (!hasSession(path.selectedSessionId)) {
            path.selectedSessionId = path.activeSessionId;
        }
    }

void Ghosts::clearRuntimeReplayStateAfterAttemptDeletion_() {
        stopReplay();
        stopPlayback();

        playback = false;
        m_playbackArmed = false;
        m_pendingRearm = false;
        botActive = false;

        clearHeldInputs_();
        applyBotSafety_(false);
        setdisablePlayerMove(false);

        tagCompositeOwnersHidden_(false);
        forceHidePlayLayerGhostTextLabelOnly();

        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerSerial = -1;
        m_replayOwnerIndex = -1;
        m_activeOwnerSerial = -1;
        m_practiceCompositeOwnerSerial = -1;

        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;

        m_replayIdx1 = 0;
        m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        m_didInitialWarp = false;
        m_freezePlayerXAtEnd = false;
        m_freezePlayerX = 0.f;
        m_freezePlayerY = 0.f;
        m_playerPrevTeleportedP1 = false;
        m_playerPrevTeleportedP2 = false;

        m_gridThreshold.clear();
        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;

        invalidateAttemptPointerCaches_();
        invalidatePrimedPlayerObjectRefs();
    }

void Ghosts::removeDeletedAttemptsFromRuntime_(
    std::unordered_set<int> const& deleteSet) {
        if (deleteSet.empty()) return;

        clearAllGhostNodes();

        attempts.clear();
        invalidateAttemptPointerCaches_();

        m_loadedAttemptKeys.clear();

        for (int serial : deleteSet) {
            m_ghostTextCountedDeadSerials.erase(serial);
        }

        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerSerial = -1;
        m_replayOwnerIndex = -1;
        m_activeOwnerSerial = -1;
        m_practiceCompositeOwnerSerial = -1;

        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        m_primedIndices.clear();
        m_wantToPrimeIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeSet.clear();
        m_attemptsPreloadedTotal = 0;

        m_replayIdx1 = 0;
        m_replayIdx2 = 0;
        m_lastEmitIdx1 = m_lastEmitIdx2 = kNoEmitIdx;
        m_lastPoseIdx1 = m_lastPoseIdx2 = kNoEmitIdx;

        m_serialCacheDirty = true;
        m_spans.clear();
        m_grid.clear();
        m_gridThreshold.clear();
        m_cachedCandidates.clear();
        m_cachedBinL = INT_MAX;
        m_cachedBinR = INT_MIN;

        invalidateAttemptCounts();
        invalidatePrimedPlayerObjectRefs();
        updateGhostVisibility();
    }

bool Ghosts::deleteAttemptsBySerialSet_(
        std::unordered_set<int> const& deleteSet) {
        if (deleteSet.empty()) return false;
        if (!hasCurrentPersistenceTarget_()) return false;

        if (!refreshAttemptCatalogForCurrentLevel_(true, true)) {
            return false;
        }

        bool foundAny = false;

        for (auto const& entry : m_attemptCatalog) {
            if (deleteSet.count(entry.serial) == 0) continue;

            // Normal Attempt Manager can't delete practice attempts (that's different part)
            if (entry.practiceAttempt) {
                log::warn(
                    "[APX delete] refusing normal-attempt delete because serial={} is practice",
                    entry.serial
                );
                return false;
            }

            foundAny = true;
        }

        if (!foundAny) return false;
        if (m_isSaving) return false;

        m_isSaving = true;
        struct SaveGuard {
            bool& flag;
            ~SaveGuard() { flag = false; }
        } guard{ m_isSaving };

        clearRuntimeReplayStateAfterAttemptDeletion_();

        const auto path = fileForLevel_(m_levelIDOnAttach);

        std::vector<Attempt> keptAttempts;
        keptAttempts.reserve(m_attemptCatalog.size());

        for (auto const& entry : m_attemptCatalog) {
            if (deleteSet.count(entry.serial) != 0) {
                continue;
            }

            Attempt loaded{};
            bool usedLegacy = false;

            if (!loadAPXAttemptByCatalogEntry(path, entry, loaded, &usedLegacy)) {
                log::warn(
                    "[APX delete] failed to lazy-load kept attempt serial={} from {}",
                    entry.serial,
                    geode::utils::string::pathToString(path)
                );
                return false;
            }

            loaded.persistedOnDisk = true;
            loaded.recordedThisSession = false;
            keptAttempts.push_back(std::move(loaded));
        }

        // Practice session deletion handled by Practice tab
        PracticePath keptPath = m_checkpointMgr.getPath();

        if (!saveAPXFileCurrent(path, keptAttempts, keptPath)) {
            log::warn(
                "[APX delete] failed to rewrite APX file after normal-attempt deletion: {}",
                geode::utils::string::pathToString(path)
            );
            return false;
        }

        removeDeletedAttemptsFromRuntime_(deleteSet);

        clearRuntimeReplayStateAfterAttemptDeletion_();

        clearAttemptCatalog_();
        scanAttemptCatalogForLevel_(m_levelIDOnAttach);

        armFreshRecordingAfterDeleteRestart_();
        restartLevel();

        return true;
    }

void Ghosts::clearAttemptCatalog_() {
        m_attemptCatalog.clear();
        m_attemptCatalogPracticePath.clear();
        m_attemptCatalogBySerial.clear();
        m_attemptCatalogByIdentity.clear();
        m_attemptCatalogLevelID = 0;
        m_attemptCatalogCustomSaveId.clear();
        m_attemptCatalogScanned = false;
        m_attemptCatalogCurrentFormat = false;
    }

bool Ghosts::scanAttemptCatalogForLevel_(int levelID) {
        if (
            m_attemptCatalogScanned &&
            m_attemptCatalogLevelID == levelID &&
            m_attemptCatalogCustomSaveId == m_customSaveId
        ) {
            return true;
        }

        clearAttemptCatalog_();

        APXCatalogScanResult scan{};
        if (!scanAPXFileCatalog(fileForLevel_(levelID), scan)) {
            return false;
        }

        m_attemptCatalogCurrentFormat = !scan.loadedLegacy && !scan.sawLegacyAttemptChunks;
        m_attemptCatalog = std::move(scan.attempts);
        m_attemptCatalogPracticePath = std::move(scan.practicePath);
        m_attemptCatalogBySerial.reserve(m_attemptCatalog.size());
        m_attemptCatalogByIdentity.reserve(m_attemptCatalog.size());

        for (size_t i = 0; i < m_attemptCatalog.size(); ++i) {
            const APXAttemptDiskInfo& entry = m_attemptCatalog[i];
            m_attemptCatalogBySerial[entry.serial] = i;
            const uint64_t entryKey = attemptIdentityKey_(entry.serial, entry.practiceAttempt);
            if (m_attemptCatalogByIdentity.contains(entryKey)) m_attemptCatalogCurrentFormat = false;
            m_attemptCatalogByIdentity[entryKey] = i;
        }

        if (m_nextAttemptSerial <= static_cast<int>(scan.maxSerialSeen)) {
            m_nextAttemptSerial = static_cast<int>(scan.maxSerialSeen) + 1;
        }

        m_attemptCatalogLevelID = levelID;
        m_attemptCatalogCustomSaveId = m_customSaveId;
        m_attemptCatalogScanned = true;
        return true;
    }

const APXAttemptDiskInfo* Ghosts::findCatalogEntryBySerialAndPractice_(
        int serial,
        bool practiceAttempt) const {
        const auto it = m_attemptCatalogByIdentity.find(attemptIdentityKey_(serial, practiceAttempt));
        if (it == m_attemptCatalogByIdentity.end()) return nullptr;
        if (it->second >= m_attemptCatalog.size()) return nullptr;

        const APXAttemptDiskInfo& entry = m_attemptCatalog[it->second];
        if (entry.serial != serial || entry.practiceAttempt != practiceAttempt) return nullptr;
        return &entry;
    }

size_t Ghosts::findLoadedAttemptIndexBySerialAndPractice_(
        int serial,
        bool practiceAttempt) const {
        rebuildSerialCache_();
        const auto it = m_attemptToIdxByIdentity.find(attemptIdentityKey_(serial, practiceAttempt));
        if (it == m_attemptToIdxByIdentity.end()) return SIZE_MAX;
        if (it->second >= attempts.size()) return SIZE_MAX;

        const Attempt& attempt = attempts[it->second];
        if (attempt.serial != serial || attempt.practiceAttempt != practiceAttempt) return SIZE_MAX;
        return it->second;
    }

size_t Ghosts::findLoadedAttemptIndexBySerial_(int serial) const {
        rebuildSerialCache_();
        auto it = m_serialToIdx.find(serial);
        if (it == m_serialToIdx.end()) return SIZE_MAX;
        if (it->second >= attempts.size()) return SIZE_MAX;
        if (attempts[it->second].serial != serial) return SIZE_MAX;
        return it->second;
    }

bool Ghosts::getPreloadSortInfoForSerial_(int serial, float& endX, double& endT) const {
        const bool wantPractice = (m_replayKind == ReplayKind::PracticeComposite);

        if (const Attempt* a = findLoadedAttemptBySerialAndPractice_(serial, wantPractice)) {
            if (!a->p1.empty()) {
                endX = static_cast<float>(a->p1.back().x);
                endT = static_cast<double>(a->p1.back().t);
                return true;
            }
            if (a->hadDual && !a->p2.empty()) {
                endX = static_cast<float>(a->p2.back().x);
                endT = static_cast<double>(a->p2.back().t);
                return true;
            }
        }

        if (const APXAttemptDiskInfo* entry = findCatalogEntryBySerialAndPractice_(serial, wantPractice)) {
            if (entry->p1Count <= 0 && (!entry->hadDual || entry->p2Count <= 0)) return false;
            endX = entry->endX;
            endT = entry->endTime;
            return true;
        }

        endX = 0.0f;
        endT = 0.0;
        return false;
    }

void Ghosts::buildInitialAttemptsFromPreloadSerialOrder_(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial) {
        std::vector<uint32_t> sorted = preloadOrder;

        if (m_replayKind == ReplayKind::PracticeComposite) {
            auto getEndT = [&](uint32_t serial) -> double {
                float endX = 0.0f;
                double endT = 0.0;
                getPreloadSortInfoForSerial_(static_cast<int>(serial), endX, endT);
                return endT;
            };

            std::sort(sorted.begin(), sorted.end(),
                [&](uint32_t lhs, uint32_t rhs) {
                    const double tL = getEndT(lhs);
                    const double tR = getEndT(rhs);
                    if (tL != tR) return tL < tR;
                    return lhs < rhs;
                });
        }

        const size_t initialCount = std::min(
            static_cast<size_t>(m_playerObjectPool.capacity()),
            sorted.size()
        );

        outInitial.assign(sorted.begin(), sorted.begin() + initialCount);
    }

Attempt* Ghosts::ensureAttemptLoadedBySerialFiltered_(int serial, std::optional<bool> wantPractice, bool spawnNow, std::istream* batchStream) {
        if (serial <= 0) return nullptr;

        Attempt* already = nullptr;
        if (wantPractice.has_value()) {
            already = findLoadedAttemptBySerialAndPractice_(serial, wantPractice.value());
        } else {
            already = findLoadedAttemptBySerialOnly_(serial);
        }
        if (already) return already;

        if (!m_pl || !hasCurrentPersistenceTarget_()) return nullptr;

        const auto path = fileForLevel_(m_levelIDOnAttach);

        if (!scanAttemptCatalogForLevel_(m_levelIDOnAttach)) return nullptr;

        const APXAttemptDiskInfo* entry = wantPractice.has_value()
            ? findCatalogEntryBySerialAndPractice_(serial, wantPractice.value())
            : findCatalogEntryBySerial_(serial);
        if (!entry) return nullptr;

        Attempt loaded{};
        bool usedLegacy = false;

        const auto decodeStart = std::chrono::steady_clock::now();
        const bool decoded = batchStream
            ? loadAPXAttemptByCatalogEntry(*batchStream, *entry, loaded, &usedLegacy)
            : loadAPXAttemptByCatalogEntry(path, *entry, loaded, &usedLegacy);
        m_lazyDecodeTimeUs += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - decodeStart).count());
        if (!decoded) {
            ++m_lazyLoadFailures;
            return nullptr;
        }
        if (loaded.serial != entry->serial || loaded.practiceAttempt != entry->practiceAttempt) {
            ++m_lazyLoadFailures;
            log::warn("[APX lazy] catalog identity mismatch for serial={}", serial);
            return nullptr;
        }
        ++m_lazyLoadedAttempts;

        if (wantPractice.has_value() && loaded.practiceAttempt != wantPractice.value()) {
            log::warn("[APX] serial={} loaded with practice={} but practice={} was requested", serial, loaded.practiceAttempt, wantPractice.value());
            return nullptr;
        }

        const uint64_t loadedKey = attemptIdentityKey_(loaded.serial,loaded.practiceAttempt);
        if (m_loadedAttemptKeys.count(loadedKey) != 0) {
            Attempt* existing = findLoadedAttemptBySerialAndPractice_(loaded.serial,loaded.practiceAttempt);
            if (existing) return existing;

            m_loadedAttemptKeys.erase(loadedKey);
        }

        const int loadedSerial = loaded.serial;
        const bool loadedPractice = loaded.practiceAttempt;
        m_loadedAttemptKeys.insert(loadedKey);
        pushAttempt(std::move(loaded), spawnNow);

        Attempt* result = findLoadedAttemptBySerialAndPractice_(loadedSerial,loadedPractice);

        if (!result) {
            m_loadedAttemptKeys.erase(loadedKey);
        }

        return result;
    }

Attempt* Ghosts::ensurePracticeReplayOwnerLoaded_(
        int serial,
        char const* reason) {
        if (serial <= 0) return nullptr;

        Attempt* owner = ensureAttemptLoadedBySerialAndPractice_(serial, true, false);

        if (!owner) {
            log::warn(
                "[Bot] practice replay missing owner serial={} reason={}",
                serial,
                reason ? reason : "unknown"
            );
            return nullptr;
        }

        if (owner->p1.empty()) {
            log::warn(
                "[Bot] practice replay owner serial={} has no P1 frames reason={}",
                serial,
                reason ? reason : "unknown"
            );
            return nullptr;
        }

        // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
        if (m_baseTime != 0.f && owner->baseTimeOffset == 0.f) {
            owner->baseTimeOffset = m_baseTime;
            const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
            offsetAttemptTimesFast(*owner, offsetQ);
        }

        return owner;
    }

void Ghosts::rebuildSerialCache_() const {
        if (!m_serialCacheDirty) return;
        m_serialToIdx.clear();
        m_attemptToIdxByIdentity.clear();
        m_serialToIdx.reserve(attempts.size());
        m_attemptToIdxByIdentity.reserve(attempts.size());
        for (size_t i = 0; i < attempts.size(); ++i) {
            const Attempt& attempt = attempts[i];
            m_serialToIdx[attempt.serial] = i;
            m_attemptToIdxByIdentity[attemptIdentityKey_(attempt.serial, attempt.practiceAttempt)] = i;
        }
        m_serialCacheDirty = false;
    }

