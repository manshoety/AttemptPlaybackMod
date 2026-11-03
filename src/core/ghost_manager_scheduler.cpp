#include "ghost_manager.hpp"

float Ghosts::getPlaybackOnlyPastPercentThreshold() {
        auto* mod = Mod::get();
        if (!mod->hasSavedValue("ghosts-passed-percent-threshold")) setPlaybackOnlyPastPercentThreshold(20);
        else {
            m_GhostsPassedPercentThreshold = mod->getSavedValue<float>("ghosts-passed-percent-threshold");
        }
        return m_GhostsPassedPercentThreshold;
    }

void Ghosts::setPlaybackOnlyPastPercentEnabled(bool on) {
        m_onlyShowGhostsThatPassedPercent = on;
        Mod::get()->setSavedValue("only-show-ghosts-that-passed-percent", on);

        m_attemptCountsDirty = true;
        buildGhostThatPassedPercentSerialList();
        rebuildGridThreasholdSet_();
    }

void Ghosts::setPlaybackOnlyPastPercentThreshold(float percent) {
        m_GhostsPassedPercentThreshold = percent;
        Mod::get()->setSavedValue("ghosts-passed-percent-threshold", percent);

        m_attemptCountsDirty = true;
        buildGhostThatPassedPercentSerialList();
        rebuildGridThreasholdSet_();
    }

void Ghosts::buildGhostThatPassedPercentSerialList() {
        m_ghostsThatPassThePercentThreshold.clear();
        m_playerObjectPool.clearOwners(true);

        for (size_t i = 0; i < attempts.size(); ++i) {
            Attempt& a = attempts[i];

            if (!a.practiceAttempt && a.endPercent >= m_GhostsPassedPercentThreshold) {
                m_ghostsThatPassThePercentThreshold.push_back(static_cast<uint32_t>(i)); // loaded attempt indices
            }
            else {
                a.setP1Visible(false, true);
                a.setP2Visible(false, true);
            }
        }

        m_initialAttemptsToSet.clear();
        m_attemptCountsDirty = true;
    }

bool Ghosts::isOwnerGhost_(size_t ai, Attempt const& a) const {
        if (!botActive) return false;
        
        if (a.serial == m_activeOwnerSerial)
            return true;
        
        if (!m_useCheckpointsRoute && ai == m_compOwnerIdx)
            return true;
        
        return false;
    }

void Ghosts::rebuildAttemptCountsIfNeeded() {
        if (!m_attemptCountsDirty) return;

        int normal = 0;
        int practice = 0;
        m_cachedNormalAttempts = 0;
        m_cachedPracticeAttempts = 0;

        m_preloadableIndexesInAttemptsList.clear();
        m_preloadableSerials.clear();

        std::unordered_set<uint64_t> loadedSeen;
        loadedSeen.reserve(attempts.size());
        for (const auto& a : attempts) {
            loadedSeen.insert(attemptIdentityKey_(a.serial, a.practiceAttempt));
        }

        if (hasCurrentPersistenceTarget_()) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }

        if (m_replayKind == ReplayKind::PracticeComposite) {
            if (!rebuildPracticePreloadSerialSetForCurrentStart_()) {
                m_cachedPracticeAttempts = 0;
                m_cachedNormalAttempts = 0;
                m_attemptCountsDirty = false;
                return;
            }

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];

                if (!a.practiceAttempt) continue;
                if (m_practiceSerialSet.find(a.serial) == m_practiceSerialSet.end()) continue;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) continue;

                m_preloadableIndexesInAttemptsList.push_back(static_cast<int>(i));
                m_preloadableSerials.push_back(static_cast<uint32_t>(a.serial));
                practice++;
            }

            for (const auto& entry : m_attemptCatalog) {
                if (loadedSeen.find(attemptIdentityKey_(entry.serial, entry.practiceAttempt)) != loadedSeen.end()) continue;

                if (!entry.practiceAttempt) continue;
                if (m_practiceSerialSet.find(entry.serial) == m_practiceSerialSet.end()) continue;
                if (entry.p1Count <= 0) continue;

                m_preloadableSerials.push_back(static_cast<uint32_t>(entry.serial));
                practice++;
            }

            m_cachedPracticeAttempts = practice;
            m_cachedNormalAttempts = 0;
        }
        else {
            auto loadedMatchesNormalReplayFilter = [&](const Attempt& a) -> bool {
                if (a.practiceAttempt) return false;
                if (!matchesStartPositionFilter_(a)) return false;

                const bool hasSpawnableData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
                if (!hasSpawnableData) return false;

                if (m_onlyShowGhostsThatPassedPercent &&
                    a.endPercent < m_GhostsPassedPercentThreshold) {
                    return false;
                }

                return true;
            };

            auto catalogMatchesNormalReplayFilter = [&](const APXAttemptDiskInfo& entry) -> bool {
                if (entry.practiceAttempt) return false;
                if (!matchesStartPositionFilterCatalogEntry_(entry)) return false;
                if (entry.p1Count <= 0) return false;

                if (m_onlyShowGhostsThatPassedPercent &&
                    entry.endPercent < m_GhostsPassedPercentThreshold) {
                    return false;
                }

                return true;
            };

            for (size_t i = 0; i < attempts.size(); ++i) {
                Attempt& a = attempts[i];
                if (!loadedMatchesNormalReplayFilter(a)) continue;

                m_preloadableIndexesInAttemptsList.push_back(static_cast<int>(i));
                m_preloadableSerials.push_back(static_cast<uint32_t>(a.serial));
                normal++;
            }

            for (const auto& entry : m_attemptCatalog) {
                if (loadedSeen.find(attemptIdentityKey_(entry.serial, entry.practiceAttempt)) != loadedSeen.end()) continue;
                if (!catalogMatchesNormalReplayFilter(entry)) continue;

                m_preloadableSerials.push_back(static_cast<uint32_t>(entry.serial));
                normal++;
            }

            m_cachedNormalAttempts = normal;
        }

        m_attemptCountsDirty = false;
    }

