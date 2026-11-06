#include "ghost_manager.hpp"
#include <chrono>
#include <fstream>
#include "preload_sort_keys.hpp"

void Ghosts::buildInitialAttemptsFromPreloadOrder(
        const std::vector<uint32_t>& preloadOrder,
        std::vector<uint32_t>& outInitial) {
        std::vector<uint32_t> sorted = preloadOrder;

        if (m_replayKind == ReplayKind::PracticeComposite) {
            auto getEndT = [&](uint32_t serial) -> double {
                const Attempt* a = findLoadedAttemptBySerialAndPractice_(static_cast<int>(serial), true);
                if (a && !a->p1.empty()) {
                    return static_cast<double>(a->p1.back().t);
                }

                const APXAttemptDiskInfo* entry = findCatalogEntryBySerialAndPractice_(static_cast<int>(serial), true);
                if (entry) {
                    return entry->endTime;
                }

                return 0.0;
            };

            auto keys = attemptplayback::makePreloadSortKeys(
                sorted, [&](uint32_t serial) {
                    return attemptplayback::PreloadSortKey{serial, 0.f, getEndT(serial)};
                 });
            std::sort(keys.begin(), keys.end(), [](auto const& lhs, auto const& rhs) {
                if (lhs.endT != rhs.endT) return lhs.endT < rhs.endT;
                return lhs.serial < rhs.serial;
            });
            for (size_t i = 0; i < keys.size(); ++i) sorted[i] = keys[i].serial;
        }

        const size_t initialCount = std::min(
            static_cast<size_t>(m_playerObjectPool.capacity()),
            sorted.size()
        );

        outInitial.assign(sorted.begin(), sorted.begin() + initialCount);
    }

void Ghosts::buildPreloadOrderForCount_(int targetCount, std::vector<uint32_t>& outOrder) const {
        outOrder.clear();

        if (targetCount <= 0) return;

        const auto wantPractice = practiceFilterEvenWhenBotNotOn();
        const int totalMatching = wantPractice ? m_cachedPracticeAttempts : m_cachedNormalAttempts;
        const int takeCount = std::min(targetCount, totalMatching);

        if (takeCount <= 0) return;

        std::vector<uint32_t> candidateSerials = m_preloadableSerials;
        if (candidateSerials.empty()) return;

        outOrder.reserve(std::min(takeCount, static_cast<int>(candidateSerials.size())));

        PreloadSortMode sortMode = m_preloadSortMode;
        if (m_replayKind == ReplayKind::PracticeComposite) {
            sortMode = PreloadSortMode::Spread;
        }

        auto keys = sortMode == PreloadSortMode::Recent
            ? std::vector<attemptplayback::PreloadSortKey>{}
            : attemptplayback::makePreloadSortKeys(candidateSerials, [&](uint32_t serial) {
                attemptplayback::PreloadSortKey key{};
                key.serial = serial;
                getPreloadSortInfoForSerial_(static_cast<int>(serial), key.endX, key.endT);
                return key;
            });

        switch (sortMode) {
            case PreloadSortMode::Best: {
                std::sort(keys.begin(), keys.end(), [](auto const& lhs, auto const& rhs) {
                    if (lhs.endX != rhs.endX) return lhs.endX > rhs.endX;
                    if (lhs.endT != rhs.endT) return lhs.endT > rhs.endT;
                    return lhs.serial > rhs.serial;
                });
                for (size_t i = 0; i < keys.size(); ++i) candidateSerials[i] = keys[i].serial;

                outOrder.insert(
                    outOrder.end(),
                    candidateSerials.begin(),
                    candidateSerials.begin() + std::min(takeCount, static_cast<int>(candidateSerials.size()))
                );
                break;
            }

            case PreloadSortMode::Recent: {
                std::sort(candidateSerials.begin(), candidateSerials.end(),
                    [&](int lhs, int rhs) {
                        return lhs > rhs;
                    });

                outOrder.insert(
                    outOrder.end(),
                    candidateSerials.begin(),
                    candidateSerials.begin() + std::min(takeCount, static_cast<int>(candidateSerials.size()))
                );
                break;
            }

            case PreloadSortMode::Spread:
            default: {
                // Sort candidates by endT first
                std::sort(keys.begin(), keys.end(), [](auto const& lhs, auto const& rhs) {
                    if (lhs.endT != rhs.endT) return lhs.endT < rhs.endT;
                    return lhs.serial < rhs.serial;
                });
                for (size_t i = 0; i < keys.size(); ++i) candidateSerials[i] = keys[i].serial;
                
                // Quantile buckets by sorted rank, bucket sizes balanced even if endT is heavily skewed
                const int desiredBuckets = std::max(
                    4,
                    std::min(
                        64,
                        std::max(
                            takeCount / 2,
                            static_cast<int>(std::sqrt(static_cast<float>(candidateSerials.size())))
                        )
                    )
                );

                const int numBuckets = std::max(
                    1,
                    std::min(desiredBuckets, static_cast<int>(candidateSerials.size()))
                );

                std::vector<std::vector<int>> buckets(numBuckets);

                for (size_t i = 0; i < candidateSerials.size(); ++i) {
                    const int bucket = std::min(
                        numBuckets - 1,
                        static_cast<int>((i * static_cast<size_t>(numBuckets)) / candidateSerials.size())
                    );
                    buckets[bucket].push_back(candidateSerials[i]);
                }

                std::mt19937 rng(static_cast<uint32_t>(m_levelIDOnAttach) ^ 0x9E3779B9u);
                // Shuffle within each endT bucket

                for (auto& bucket : buckets) {
                    std::shuffle(bucket.begin(), bucket.end(), rng);
                }

                // Build a randomized round-robin bucket visitation order
                std::vector<int> bucketVisitOrder(numBuckets);
                std::iota(bucketVisitOrder.begin(), bucketVisitOrder.end(), 0);
                std::shuffle(bucketVisitOrder.begin(), bucketVisitOrder.end(), rng);

                std::vector<size_t> bucketCursors(numBuckets, 0);

                while (static_cast<int>(outOrder.size()) < takeCount) {
                    bool anyRemaining = false;

                    for (int b : bucketVisitOrder) {
                        if (bucketCursors[b] < buckets[b].size()) {
                            outOrder.push_back(static_cast<uint32_t>(buckets[b][bucketCursors[b]++]));
                            anyRemaining = true;

                            if (static_cast<int>(outOrder.size()) >= takeCount) {
                                break;
                            }
                        }
                    }

                    if (!anyRemaining) break;

                    // Rotate the visit order
                    if (bucketVisitOrder.size() > 1) {
                        std::rotate(
                            bucketVisitOrder.begin(),
                            bucketVisitOrder.begin() + 1,
                            bucketVisitOrder.end()
                        );
                    }
                }

                break;
            }
        }
    }

void Ghosts::beginPreloadAttempts(int targetCount) {
        if (isPureRecordingMode_()) {
            m_preloadActive = false;
            m_initialAttemptsToSet.clear();
            return;
        }

        if (targetCount <= 0) {
            cancelPreloadAttempts(true);
            return;
        }

        m_1PreloadedSoDontShowGhosts = (targetCount == 1);
        m_attemptCountsDirty = true;
        rebuildAttemptCountsIfNeeded();
        if (botActive && practiceRouteOwnerHidingEnabled_()) {
            rebuildRouteOwnerCache_();
        }

        const bool wantPractice = practiceFilterEvenWhenBotNotOn();
        const int totalMatching = wantPractice ? m_cachedPracticeAttempts : m_cachedNormalAttempts;
        const int requestedCount = std::min(targetCount, totalMatching);

        // Selection must be computed for the FULL target, not just its delta:
        // Best/Recent/Spread can choose different serials as the target grows.
        buildPreloadOrderForCount_(requestedCount, m_preloadOrder);

        std::unordered_set<uint32_t> requestedSerials(m_preloadOrder.begin(), m_preloadOrder.end());
        bool canReuse = true;
        for (size_t idx : m_preloadedIndices) {
            if (idx >= attempts.size() || !attempts[idx].preloaded ||
                attempts[idx].practiceAttempt != wantPractice ||
                requestedSerials.count(static_cast<uint32_t>(attempts[idx].serial)) == 0) {
                canReuse = false;
                break;
            }
        }

        if (!canReuse) {
            cancelPreloadAttempts(true);
            m_attemptCountsDirty = true;
            rebuildAttemptCountsIfNeeded();
            if (botActive && practiceRouteOwnerHidingEnabled_()) rebuildRouteOwnerCache_();
            buildPreloadOrderForCount_(requestedCount, m_preloadOrder);
        }

        m_preloadActive = false;
        m_preloadCursor = 0;
        m_preloadTarget = static_cast<int>(m_preloadOrder.size());

        buildInitialAttemptsFromPreloadOrder(m_preloadOrder, m_initialAttemptsToSet);
        prioritizeInitialAttemptsInPreloadOrder_(targetCount);

        int readyNow = 0;
        for (uint32_t serial : m_preloadOrder) {
            const size_t idx = findLoadedAttemptIndexBySerialFiltered_(
                static_cast<int>(serial), wantPractice);
            if (idx != SIZE_MAX && idx < attempts.size() && attempts[idx].preloaded &&
                m_preloadedSet.count(idx) != 0) {
                ++readyNow;
            }
        }
        m_preloadLoaded = readyNow;

        m_preloadActive = m_preloadLoaded < m_preloadTarget && !m_preloadOrder.empty();

        log::info(
            "[Preload] target={} reused={} missing={} initialPO={} active={}",
            m_preloadTarget, readyNow, m_preloadOrder.size() - readyNow,
            m_initialAttemptsToSet.size(), m_preloadActive
        );
        if (!m_preloadActive) {
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
            updateGhostTextOnDeath(true);
        }
    }

void Ghosts::prioritizeInitialAttemptsInPreloadOrder_(int targetCount) {
        if (m_preloadOrder.empty()) return;

        std::unordered_set<uint32_t> validCandidates(
            m_preloadOrder.begin(),
            m_preloadOrder.end()
        );

        std::unordered_set<uint32_t> initialSet(
            m_initialAttemptsToSet.begin(),
            m_initialAttemptsToSet.end()
        );

        std::unordered_set<uint32_t> added;
        std::vector<uint32_t> reordered;
        reordered.reserve(m_preloadOrder.size() + 1);

        auto pushIfValid = [&](uint32_t serial) {
            if (validCandidates.find(serial) == validCandidates.end()) return;
            if (added.insert(serial).second) {
                reordered.push_back(serial);
            }
        };

        // Force route owner if part of this preload candidate set
        if (targetCount > 1) {
            if (m_activeOwnerSerial > 0) {
                pushIfValid(static_cast<uint32_t>(m_activeOwnerSerial));
            } else if (m_replayOwnerSerial > 0) {
                pushIfValid(static_cast<uint32_t>(m_replayOwnerSerial));
            }
        }

        // Force initially assigned PlayerObject attempts
        for (uint32_t serial : m_initialAttemptsToSet) {
            pushIfValid(serial);
        }

        // Append the rest
        for (uint32_t serial : m_preloadOrder) {
            pushIfValid(serial);
        }

        if (!reordered.empty()) {
            m_preloadOrder = std::move(reordered);
        }
    }

void Ghosts::preloadStep(int maxPerTick) {
        if (isPureRecordingMode_()) {
            m_preloadActive = false;
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
            return;
        }

        if (!m_preloadActive || maxPerTick <= 0) return;

        const std::unordered_set<uint32_t> pooledSerials(
            m_initialAttemptsToSet.begin(), m_initialAttemptsToSet.end());
        const std::optional<bool> wantPractice = practiceFilterForGhosts_();
        std::ifstream batchAPX;

        int work = 0;
        while (work < maxPerTick &&
            m_preloadLoaded < m_preloadTarget &&
            m_preloadCursor < m_preloadOrder.size())
        {
            const int serial = static_cast<int>(m_preloadOrder[m_preloadCursor]);
            // never re-decode/re-prepare an attempt already present in the selected set
            const size_t cachedIdx = findLoadedAttemptIndexBySerialFiltered_(serial, wantPractice);
            if (cachedIdx != SIZE_MAX && cachedIdx < attempts.size() &&
                attempts[cachedIdx].preloaded && m_preloadedSet.count(cachedIdx) != 0) {
                ++m_preloadCursor;
                continue;
            }
            if (cachedIdx == SIZE_MAX && !batchAPX.is_open() && hasCurrentPersistenceTarget_()) {
                batchAPX.open(fileForLevel_(m_levelIDOnAttach), std::ios::binary);
            }
            Attempt* loaded = ensureAttemptLoadedBySerialFiltered_(
                serial, wantPractice, false, batchAPX.is_open() ? &batchAPX : nullptr);
            if (!loaded) {
                ++m_preloadCursor;
                continue;
            }

            const size_t idx = findLoadedAttemptIndexBySerialFiltered_(serial, wantPractice);
            if (idx == SIZE_MAX || idx >= attempts.size()) {
                ++m_preloadCursor;
                continue;
            }

            Attempt& a = attempts[idx];
            const auto prepareStart = std::chrono::steady_clock::now();
            const bool priorityVisual = pooledSerials.count(static_cast<uint32_t>(serial)) != 0 ||
                (m_preloadTarget > 1 && (serial == m_activeOwnerSerial || serial == m_replayOwnerSerial));
            
            // Build the accel stuff for all for now but might change this later idk (want to avoid playback lag)
            preloadAttempt_(a, idx, true, priorityVisual);
            m_preloadPrepareTimeUs += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - prepareStart).count());
            // log::info("m_preloadCursor: {}, idx: {}, p1: {}", m_preloadCursor, serial, a.p1.size());
            if (priorityVisual) InitialGamemodeSetProcessAttemptIdx(idx);

            if (a.preloaded) {
                ++m_preloadLoaded;
                ++work;
            }

            ++m_preloadCursor;
        }

        if (m_preloadLoaded >= m_preloadTarget || m_preloadCursor >= m_preloadOrder.size()) {

            if (m_preloadLoaded < m_preloadTarget && m_preloadCursor >= m_preloadOrder.size()) {
                log::warn(
                    "[Preload] order ran out early: loaded={} target={} cursor={} order={} candidates={}",
                    m_preloadLoaded,
                    m_preloadTarget,
                    m_preloadCursor,
                    m_preloadOrder.size(),
                    m_preloadableSerials.size()
                );
            }

            m_preloadActive = false;
            if (m_replayKind == ReplayKind::BestSingle) replayBestSetOwner();
            rebuildGridPreloadedSet_();
            rebuildGridThreasholdSet_();
            updateGhostTextOnDeath(true);
        }
    }

void Ghosts::InitialGamemodeSetProcessAttemptIdx(size_t attemptIdx) {
        if (attemptIdx >= attempts.size()) return;
        if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) return;

        Attempt& a = attempts[attemptIdx];
        prepareAttemptVisualData_(a);

        auto h = m_playerObjectPool.acquireForOwner(a.serial, static_cast<uint32_t>(attemptIdx), false);
        if (h) { a.g1Idx = h.index; a.g1 = h.po; }

        if (a.hadDual) {
            auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000, static_cast<uint32_t>(attemptIdx), true);
            if (h2) { a.g2Idx = h2.index; a.g2 = h2.po; }
        }

        if (a.g1 && !a.p1.empty()) {
            a.g1CurMode = a.p1.front().mode;
            setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
            a.setP1Visible(false, true);
        }
        if (a.g2 && !a.p2.empty()) {
            a.g2CurMode = a.p2.front().mode;
            setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
            a.setP2Visible(false, true);
        }
    }

void Ghosts::InitialGamemodeSetProcessBatch(
        const std::vector<uint32_t>* orderList,
        int batchSize,
        int startIndex) {
        if (!orderList || orderList->empty()) return;

        const auto& order = *orderList;
        const int end = std::min(startIndex + batchSize, static_cast<int>(order.size()));

        for (int i = startIndex; i < end; ++i) {
            const int serial = static_cast<int>(order[i]);
            const std::optional<bool> wantPractice = practiceFilterForGhosts_();

            Attempt* loaded = ensureAttemptLoadedBySerialFiltered_(serial, wantPractice, false);
            if (!loaded) {
                continue;
            }

            const size_t attemptIdx = findLoadedAttemptIndexBySerialFiltered_(serial, wantPractice);
            if (attemptIdx == SIZE_MAX || attemptIdx >= attempts.size()) {
                continue;
            }

            if (m_playerObjectPool.inUseCount() >= m_playerObjectPool.capacity() - 2) {
                return;
            }

            Attempt& a = attempts[attemptIdx];
            prepareAttemptVisualData_(a);

            auto h = m_playerObjectPool.acquireForOwner(a.serial, static_cast<uint32_t>(attemptIdx), false);
            if (h) {
                a.g1Idx = h.index;
                a.g1 = h.po;
            }

            if (a.hadDual) {
                auto h2 = m_playerObjectPool.acquireForOwner(a.serial + 0x100000, static_cast<uint32_t>(attemptIdx), true);
                if (h2) {
                    a.g2Idx = h2.index;
                    a.g2 = h2.po;
                }
            }

            if (a.g1 && !a.p1.empty()) {
                a.g1CurMode = a.p1.front().mode;
                setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
                a.setP1Visible(false, true);
            }

            if (a.g2 && !a.p2.empty()) {
                a.g2CurMode = a.p2.front().mode;
                setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
                a.setP2Visible(false, true);
            }
        }
    }

void Ghosts::resetAttemptPreloadState_(Attempt& a, int idx) {
        (void)idx;

        if (a.g1) a.setP1Visible(false, true);
        if (a.g2) a.setP2Visible(false, true);

        a.preloaded = false;

        a.resetPlayback();

        a.p1Visible = false;
        a.p2Visible = false;
        a.last1 = PoseCache{};
        a.last2 = PoseCache{};

        a.g1CurMode = IconType::Cube;
        a.g2CurMode = IconType::Cube;

        a.g1 = nullptr;
        a.g2 = nullptr;
        a.g1Idx = -1;
        a.g2Idx = -1;
    }

void Ghosts::cancelPreloadAttempts(bool ignoreStopPlayback) {
        //m_ghostPool.logStats("CancelPreload");
        m_preloadActive = false;
        m_preloadTarget = 0;
        m_preloadLoaded = 0;
        m_preloadCursor = 0;
        m_preloadOrder.clear();
        m_initialAttemptsToSet.clear();
        m_preloadedIndices.clear();
        m_preloadedSet.clear();
        practiceAttemptReplayIndex.clear();
        m_attemptsPreloadedTotal = 0;

        m_playerObjectPool.clearOwners(true);
        clearAllAttemptPreloadState_();

        m_primedIndices.clear();
        m_primedSet.clear();
        m_wantToPrimeIndices.clear();
        m_wantToPrimeSet.clear();
        m_practiceSerialSet.clear();

        const bool keepRouteOwnerCache =
            ignoreStopPlayback &&
            botActive &&
            practiceRouteOwnerHidingEnabled_();

        if (!keepRouteOwnerCache) {
            m_routeOwnerSerials.clear();
            m_currentlyHiddenSerials.clear();
        }

        if (!ignoreStopPlayback) {
            stopReplay();
            playback = false;
            recording = true;

            forcePlayersVisible_();
            clearHeldInputs_();
        }
    }

bool Ghosts::hasAnyReplayDataForCurrentLevel(ReplayKind kind) {
        const bool wantPractice = (kind == ReplayKind::PracticeComposite);

        if (hasCurrentPersistenceTarget_()) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }

        for (auto const& a : attempts) {
            if (a.practiceAttempt != wantPractice) continue;

            const bool hasData = !a.p1.empty() || (a.hadDual && !a.p2.empty());
            if (hasData) return true;
        }

        for (auto const& entry : m_attemptCatalog) {
            if (entry.practiceAttempt != wantPractice) continue;

            if (entry.p1Count > 0 || entry.p2Count > 0) {
                return true;
            }
        }

        return false;
    }

int Ghosts::getTotalAttemptsCount() {
        // No valid session for start pos for practice mode
        if (m_replayKind == ReplayKind::PracticeComposite && m_checkpointMgr.noValidSessionForStartX()) return 0;

        m_attemptCountsDirty = true; // Fix later, but for now always rebuild
        // Actually I'll probably leave it dirty since I don't want to retain anything as that overcomplicates it for barely any benifit
        rebuildAttemptCountsIfNeeded();

        if (practiceFilterEvenWhenBotNotOn()) return m_cachedPracticeAttempts;
        else return m_cachedNormalAttempts;
    }

preload_memory::PreloadMemoryEstimate Ghosts::estimatePreloadMemoryUsage(
        int targetCount,
        int realPlayerObjectAttempts,
        size_t bytesPerPlayerObject) {
        if (targetCount <= 0) {
            return {};
        }

        m_attemptCountsDirty = true;
        rebuildAttemptCountsIfNeeded();
        rebuildSerialCache_();

        if (hasCurrentPersistenceTarget_()) {
            scanAttemptCatalogForLevel_(m_levelIDOnAttach);
        }

        std::vector<uint32_t> order;
        buildPreloadOrderForCount_(targetCount, order);

        return preload_memory::estimateForSerialOrder(
            order,
            targetCount,
            std::max(0, realPlayerObjectAttempts),
            bytesPerPlayerObject,
            attempts,
            m_serialToIdx,
            m_attemptCatalog,
            m_attemptCatalogBySerial
        );
    }

