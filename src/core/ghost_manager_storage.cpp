#include "ghost_manager.hpp"
#include <chrono>
#include <climits>

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
                attemptplayback::pathcompat::toUtf8(path),
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
                    attemptplayback::pathcompat::toUtf8(path),
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
        m_persistenceReadFailed = false; // Explicit delete all succeeded
        m_storageErrorShown = false;

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
        const auto saveDir = attemptplayback::pathcompat::toStdOrLog(
            Mod::get()->getSaveDir(), "[APX export]", "save"
        );
        if (saveDir.empty()) return {};
        auto dir = saveDir / "exports";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            log::warn(
                "[APX export] create_directories failed for {}: {}",
                attemptplayback::pathcompat::toUtf8(dir),
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

        const bool sameFile = std::filesystem::equivalent(source, destination, ec);
        if (sameFile && !ec) return fail("Choose an export file other than the current recording.");
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
                if (maxSerial >= INT_MAX - 1) return fail("No attempt IDs available for import.");
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
                if (maxSessionId >= INT_MAX - 1) return fail("No practice session IDs available for import.");
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
            auto base = attemptplayback::pathcompat::toStdOrLog(
                Mod::get()->getSaveDir(), "[SAVE FILE]", "save"
            );
            if (base.empty()) return {};
            m_attemptsDir = base / "attempts";

            std::error_code ec;
            std::filesystem::create_directories(m_attemptsDir, ec);
            if (ec) {
                log::warn(
                    "[SAVE FILE] create_directories failed: {} ({})",
                    attemptplayback::pathcompat::toUtf8(m_attemptsDir),
                    ec.message()
                );
            }
        }
        return m_attemptsDir;
    }

std::filesystem::path Ghosts::fileForLevel_(int levelID) const {
        auto dir = attemptsDir_();
        if (dir.empty()) return {};

        if (!m_customSaveId.empty()) {
            return dir / (m_customSaveId + "_attempts.apx");
        }

        return dir / (std::to_string(levelID) + "_attempts.apx");
    }

bool Ghosts::ensureHeader_(std::filesystem::path const& path) {
        if (path.empty()) return false;

        std::error_code ec;

        const auto status = std::filesystem::symlink_status(path, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) return false;
        if (status.type() != std::filesystem::file_type::not_found) {
            if (ec || !std::filesystem::is_regular_file(status)) return false;
            apxio::Reader in(path);
            APXHeader h{};
            in.read(reinterpret_cast<char*>(&h), sizeof(h));
            return in && std::memcmp(h.magic, "APX2", 4) == 0 && h.version == kAPXVersion;
        }
        
        apxio::Writer out(path, apxio::Mode::CreateNew);
        if (!out) return false;

        APXHeader h{};
        h.version = kAPXVersion;
        out.write(reinterpret_cast<const char*>(&h), sizeof(h));
        const bool ok = out.good();
        out.close();
        if (!ok) {
            std::filesystem::remove(path, ec);
            if (ec) log::warn("[APX save] failed removing incomplete new header: {}", ec.message());
        }
        return ok;
    }

void Ghosts::migrateLegacyCustomLevelFiles_(
        int levelID,
        std::string const& legacyNameHash,
        std::string const& canonicalSaveId) {
        if (canonicalSaveId.empty()) return;
        const auto dir = attemptsDir_();
        if (dir.empty()) return;
        const auto canonicalPath = dir / (canonicalSaveId + "_attempts.apx");
        std::error_code ec;
        const bool canonicalExists = std::filesystem::exists(canonicalPath, ec);
        if (ec) {
            reportStorageError_("APX-MIGRATE", "Could not check editor recording: " + ec.message());
            return;
        }

        // Move the old save to the editor ID if there is exactly one matching file
        std::vector<std::filesystem::path> legacyPaths;
        auto addLegacy = [&](std::filesystem::path const& path) {
            if (path != canonicalPath && std::filesystem::exists(path, ec) && !ec &&
                std::find(legacyPaths.begin(), legacyPaths.end(), path) == legacyPaths.end()) {
                legacyPaths.push_back(path);
            }
        };
        if (!legacyNameHash.empty()) addLegacy(dir / (legacyNameHash + "_attempts.apx"));
        if (levelID > 0) addLegacy(dir / (std::to_string(levelID) + "_attempts.apx"));
        if (legacyPaths.empty()) return;
        if (canonicalExists || legacyPaths.size() != 1) {
            reportStorageError_("APX-MIGRATE", "Multiple old editor recordings found. Import them manually; originals were preserved.");
            return;
        }
        std::filesystem::rename(legacyPaths.front(), canonicalPath, ec);
        if (ec) reportStorageError_("APX-MIGRATE", "Could not move old editor recording: " + ec.message());
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

void Ghosts::reportStorageError_(std::string const& code, std::string const& detail) {
        const auto path = fileForLevel_(m_levelIDOnAttach);
        log::error("[{}] level={} file={}: {}", code, m_levelIDOnAttach,
            attemptplayback::pathcompat::toUtf8(path), detail);
        if (m_storageErrorShown) return;
        m_storageErrorShown = true;
        const auto message = fmt::format("{}: {}\nLevel: {}\nPlease send the Geode log and APX file to me (manshoety) using the discord linked on the mod page. Unsaved attempts may be lost if you leave the level.",
            code, detail, m_levelIDOnAttach);
        Loader::get()->queueInMainThread([message] {
            FLAlertLayer::create("Attempt Playback Storage Error", message, "OK")->show();
        });
    }

int Ghosts::saveNewAttemptsForLevel_(int levelID) {
        if (m_isSaving || !hasPendingSaves_()) return 0;
        if (!hasPersistenceTargetForLevel_(levelID)) {
            reportStorageError_("APX-SAVE", "No save location for this level.");
            return 0;
        }
        if (m_persistenceReadFailed) {
            reportStorageError_("APX-SAVE", "Existing recording could not be read; saving is disabled to protect it.");
            return 0;
        }
        m_isSaving = true;
        struct SaveGuard {
            bool& flag;
            ~SaveGuard() { flag = false; }
        } guard{ m_isSaving };

        const auto path = fileForLevel_(levelID);
        if (path.empty()) {
            reportStorageError_("APX-OPEN", "Save directory unavailable.");
            return 0;
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec || !ensureHeader_(path)) {
            reportStorageError_("APX-OPEN", ec ? ec.message() : "Could not open or create the recording header.");
            return 0;
        }

        apxio::Writer out(path, apxio::Mode::Append);
        const auto start = out.position();
        if (!out || start == UINT64_MAX) {
            reportStorageError_("APX-OPEN", "Could not open recording for append.");
            return 0;
        }

        auto fail = [&](std::string const& detail) {
            if (!out.rollback(start)) {
                m_persistenceReadFailed = true;
                reportStorageError_("APX-ROLLBACK", detail + " Rollback failed; further writes disabled.");
            } else {
                reportStorageError_("APX-WRITE", detail);
            }
            clearAttemptCatalog_();
            return 0;
        };

        int written = 0;
        for (auto& a : attempts) {
            if (!attemptHasData_(a) || !a.recordedThisSession || a.persistedOnDisk) continue;
            if (a.serial <= 0 || !writeAPXAttemptCompact(out, a)) {
                return fail(fmt::format("Could not write attempt {}.", a.serial));
            }
            ++written;
        }
        if (m_checkpointMgr.isDirty() && !writeAPXPracticePath(out, m_checkpointMgr.getPath())) {
            return fail("Could not write practice session data.");
        }

        out.close();
        for (auto& a : attempts) {
            if (a.recordedThisSession && attemptHasData_(a)) a.persistedOnDisk = true;
        }
        m_checkpointMgr.clearDirty();
        clearAttemptCatalog_();
        m_storageErrorShown = false;
        return written;
    }

int Ghosts::loadFromDisk_(int levelID, LoadFilter const& flt) {
        if (!hasPersistenceTargetForLevel_(levelID) || !scanAttemptCatalogForLevel_(levelID)) return 0;
        m_checkpointMgr.restorePath(m_attemptCatalogPracticePath);

        // Normal level entry only loads the catalog
        // Filtered loads decode requested attempts
        if (!flt.practice.has_value() && !flt.minPct.has_value() &&
            !flt.maxPct.has_value() && flt.maxNonPractice == 0) return 0;

        int loaded = 0;
        size_t nonPracticeLoaded = 0;
        apxio::Reader in(fileForLevel_(levelID));
        for (auto const& entry : m_attemptCatalog) {
            if (flt.practice.has_value() && flt.practice.value() != entry.practiceAttempt) continue;
            if (flt.minPct.has_value() && entry.startPercent < flt.minPct.value()) continue;
            if (flt.maxPct.has_value() && entry.startPercent > flt.maxPct.value()) continue;
            if (!entry.practiceAttempt && flt.maxNonPractice > 0 && nonPracticeLoaded >= flt.maxNonPractice) continue;
            const auto key = attemptIdentityKey_(entry.serial, entry.practiceAttempt);
            if (m_loadedAttemptKeys.count(key)) continue;
            Attempt a;
            if (!loadAPXAttemptByCatalogEntry(in, entry, a)) {
                m_persistenceReadFailed = true;
                reportStorageError_("APX-LOAD", fmt::format("Could not read attempt {}.", entry.serial));
                break;
            }
            m_loadedAttemptKeys.insert(key);
            pushAttempt(std::move(a), false);
            ++loaded;
            if (!entry.practiceAttempt) ++nonPracticeLoaded;
        }
        return loaded;
    }

int Ghosts::saveNewAttemptsForCurrentLevel() {
        return saveNewAttemptsForLevel_(m_levelIDOnAttach);
    }

bool Ghosts::refreshAttemptCatalogForCurrentLevel_(bool forceRescan, bool pushCurrentAttempt) {
    if (!hasCurrentPersistenceTarget_()) {
        clearAttemptCatalog_();
        return false;
    }

    if (!flushPendingSaves_()) return false;

    if (forceRescan) clearAttemptCatalog_();

    return scanAttemptCatalogForLevel_(m_levelIDOnAttach);
}

bool Ghosts::refreshPracticeRuns() {
        // Selector can open before the first death
        // Register recorded prefix and continue same session if user cancels
        if (m_pl && m_pl->m_isPracticeMode && !m_justDied && finalizeCurrentAttempt_(false)) {
            startNewAttempt(true);
        }
        return refreshAttemptCatalogForCurrentLevel_(true, false);
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
        PracticePath keptPath = m_checkpointMgr.getPath();
        return rewriteAttemptsAndPracticePathAfterDeletion_(deleteSet, keptPath, "normal-attempt");
    }

void Ghosts::clearAttemptCatalog_() {
        m_attemptCatalog.clear();
        m_attemptCatalogPracticePath.clear();
        m_attemptCatalogBySerial.clear();
        m_attemptCatalogByIdentity.clear();
        m_attemptCatalogLevelID = 0;
        m_attemptCatalogCustomSaveId.clear();
        m_attemptCatalogScanned = false;
    }

bool Ghosts::scanAttemptCatalogForLevel_(int levelID) {
    if (m_attemptCatalogScanned &&
        m_attemptCatalogLevelID == levelID &&
        m_attemptCatalogCustomSaveId == m_customSaveId) {
        return true;
    }

   clearAttemptCatalog_();

    const auto path = fileForLevel_(levelID);
    APXCatalogScanResult scan{};
    if (!scanAPXFileCatalog(path, scan)) {
        m_persistenceReadFailed = true;
        reportStorageError_("APX-LOAD", "Recording catalog could not be read. Original file left unchanged.");
        return false;
    }

    if (scan.loadedLegacy || scan.sawLegacyAttemptChunks) {
        std::vector<Attempt> legacyAttempts;
        PracticePath legacyPath;
        if (!loadAPXFileWithMigration(path, legacyAttempts, legacyPath) ||
            !scanAPXFileCatalog(path, scan) || scan.loadedLegacy || scan.sawLegacyAttemptChunks) {
            m_persistenceReadFailed = true;
            reportStorageError_("APX-MIGRATE", "Could not convert the old recording. Please send the original file.");
            return false;
        }
    }

    m_attemptCatalog = std::move(scan.attempts);
    m_attemptCatalogPracticePath = std::move(scan.practicePath);
    m_attemptCatalogBySerial.reserve(m_attemptCatalog.size());
    m_attemptCatalogByIdentity.reserve(m_attemptCatalog.size());

    for (size_t i = 0; i < m_attemptCatalog.size(); ++i) {
        const APXAttemptDiskInfo& entry = m_attemptCatalog[i];
        m_attemptCatalogBySerial[entry.serial] = i;
        const uint64_t entryKey = attemptIdentityKey_(entry.serial, entry.practiceAttempt);
        m_attemptCatalogByIdentity[entryKey] = i;
    }

    uint32_t maxReservedSerial = scan.maxSerialSeen;
    for (auto const& session : m_attemptCatalogPracticePath.sessions) {
        for (int serial : session.allAttemptSerials) {
            if (serial > 0) maxReservedSerial = std::max(maxReservedSerial, static_cast<uint32_t>(serial));
        }

        for (auto const& seg : session.segments) {
            if (seg.ownerSerial > 0) maxReservedSerial = std::max(maxReservedSerial, static_cast<uint32_t>(seg.ownerSerial));
        }
    }

    if (maxReservedSerial >= static_cast<uint32_t>(INT_MAX)) m_nextAttemptSerial = INT_MAX;
    else m_nextAttemptSerial = std::max(m_nextAttemptSerial, static_cast<int>(maxReservedSerial) + 1);

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

Attempt* Ghosts::ensureAttemptLoadedBySerialFiltered_(int serial, std::optional<bool> wantPractice, bool spawnNow, apxio::Reader* batchStream) {
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

