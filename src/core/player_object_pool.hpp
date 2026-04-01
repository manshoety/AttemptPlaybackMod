// player_object_pool.hpp
#pragma once

#include <algorithm>
#include <deque>
#include <limits>
#include <unordered_map>
#include <vector>
#include <cstdint>

#include <Geode/binding/PlayerObject.hpp>

#include "../core/ghost_pool.hpp"

struct PlayerObjectPool {
    struct Handle {
        int index = -1;
        PlayerObject* po = nullptr;
        explicit operator bool() const { return index >= 0 && po != nullptr; }
    };

private:
    struct Slot {
        PlayerObject* po = nullptr;
        int owner = kNoOwner;
        int ghostSerial = 0;
        bool inUse = false;
        uint32_t attemptIdx = UINT32_MAX;
        bool isP2 = false;
    };

    static constexpr int kNoOwner = std::numeric_limits<int>::min();

    ProfiledGhostPool* m_ghostPool = nullptr;

    int m_max = 0;
    int m_serialBase = 0x60000000;

    std::vector<Slot> m_slots;
    std::deque<int> m_free;

    std::unordered_map<int, int> m_ownerToIndex;

    int serialForIndex_(int idx) const {
        return m_serialBase + idx;
    }

    struct ActiveAttemptUse {
        uint32_t attemptIdx = UINT32_MAX;
        bool hasP1 = false;
        bool hasP2 = false;
    };

    std::vector<ActiveAttemptUse> m_activeAttempts;
    std::unordered_map<uint32_t, int> m_attemptToActiveIndex;

    void markAttemptUse_(uint32_t attemptIdx, bool isP2) {
        if (attemptIdx == UINT32_MAX) return;

        auto it = m_attemptToActiveIndex.find(attemptIdx);
        if (it == m_attemptToActiveIndex.end()) {
            int pos = static_cast<int>(m_activeAttempts.size());
            ActiveAttemptUse e;
            e.attemptIdx = attemptIdx;
            e.hasP1 = !isP2;
            e.hasP2 = isP2;
            m_activeAttempts.push_back(e);
            m_attemptToActiveIndex[attemptIdx] = pos;
            return;
        }

        auto& e = m_activeAttempts[static_cast<size_t>(it->second)];
        if (isP2) e.hasP2 = true;
        else e.hasP1 = true;
    }

    void unmarkAttemptUse_(uint32_t attemptIdx, bool isP2) {
        auto it = m_attemptToActiveIndex.find(attemptIdx);
        if (it == m_attemptToActiveIndex.end()) return;

        int pos = it->second;
        auto& e = m_activeAttempts[static_cast<size_t>(pos)];

        if (isP2) e.hasP2 = false;
        else e.hasP1 = false;

        if (e.hasP1 || e.hasP2) return;

        int last = static_cast<int>(m_activeAttempts.size()) - 1;
        if (pos != last) {
            m_activeAttempts[static_cast<size_t>(pos)] = m_activeAttempts[static_cast<size_t>(last)];
            m_attemptToActiveIndex[m_activeAttempts[static_cast<size_t>(pos)].attemptIdx] = pos;
        }

        m_activeAttempts.pop_back();
        m_attemptToActiveIndex.erase(it);
    }

public:
    PlayerObjectPool() = default;

    void init(ProfiledGhostPool* ghostPool, int maxPlayerObjects = 2000, int serialBase = 0x60000000) {
        m_ghostPool = ghostPool;
        m_max = std::max(0, maxPlayerObjects);
        m_serialBase = serialBase;

        m_slots.clear();
        m_slots.resize((size_t)m_max);

        m_free.clear();
        m_free.resize(0);
        for (int i = 0; i < m_max; ++i) {
            m_slots[i] = Slot{};
            m_slots[i].ghostSerial = serialForIndex_(i);
            m_free.push_back(i);
        }

        m_ownerToIndex.clear();
        m_activeAttempts.clear();
        m_attemptToActiveIndex.clear();
    }

    bool isInitialized() const { return m_max > 0; }
    int capacity() const { return m_max; }

    int inUseCount() const {
        return m_max - (int)m_free.size();
    }

    // Returns existing handle if owner already has one, otherwise allocates a new slot
    Handle acquireForOwner(int ownerKey, uint32_t attemptIdx, bool isP2 = false) {
        if (!m_ghostPool || m_max <= 0) return {};

        // Already assigned?
        auto it = m_ownerToIndex.find(ownerKey);
        if (it != m_ownerToIndex.end()) {
            const int idx = it->second;
            if (idx >= 0 && idx < m_max) {
                auto& s = m_slots[(size_t)idx];

                // Only trust the mapping if the slot is still live and still belongs to this owner
                if (s.inUse && s.owner == ownerKey) {
                    s.attemptIdx = attemptIdx;
                    s.isP2 = isP2;
                    markAttemptUse_(attemptIdx, isP2);

                    // If pointer is null (for example after ghost pool was cleared), reacquire it
                    if (!s.po) {
                        s.po = m_ghostPool->acquire(s.ghostSerial);
                    }

                    return { idx, s.po };
                }
            }

            // Stale mapping, fall through to allocate fresh
            m_ownerToIndex.erase(it);
        }

        // No free slots
        if (m_free.empty()) {
            return {};
        }

        const int idx = m_free.front();
        m_free.pop_front();

        auto& s = m_slots[(size_t)idx];
        s.inUse = true;
        s.owner = ownerKey;
        s.attemptIdx = attemptIdx;
        s.isP2 = isP2;

        // Acquire from ghost pool using stable per-slot serial
        s.po = m_ghostPool->acquire(s.ghostSerial);

        if (!s.po) {
            // Failed: return slot to free list and fully reset slot metadata
            s.inUse = false;
            s.owner = kNoOwner;
            s.attemptIdx = UINT32_MAX;
            s.isP2 = false;
            m_free.push_back(idx);
            return {};
        }

        m_ownerToIndex[ownerKey] = idx;
        markAttemptUse_(attemptIdx, isP2);
        return { idx, s.po };
    }

    // Non-owning access (returns nullptr if invalid)
    PlayerObject* getByIndex(int idx) const {
        if (idx < 0 || idx >= m_max) return nullptr;
        const auto& s = m_slots[(size_t)idx];
        return s.inUse ? s.po : nullptr;
    }

    int ownerOf(int idx) const {
        if (idx < 0 || idx >= m_max) return kNoOwner;
        return m_slots[(size_t)idx].owner;
    }

    bool isInUse(int idx) const {
        if (idx < 0 || idx >= m_max) return false;
        return m_slots[(size_t)idx].inUse;
    }

    // Release by index when an attempt is done
    // This also releases the underlying ghost serial back to the GhostPool (hides, resets trail, etc.)
    void releaseIndex(int idx) {
        if (idx < 0 || idx >= m_max) return;

        auto& s = m_slots[(size_t)idx];
        if (!s.inUse) return;

        // Remove owner mapping
        if (s.owner != kNoOwner) {
            auto it = m_ownerToIndex.find(s.owner);
            if (it != m_ownerToIndex.end() && it->second == idx) {
                m_ownerToIndex.erase(it);
            }
        }

        unmarkAttemptUse_(s.attemptIdx, s.isP2);

        // Return to ghostpool's free list
        if (m_ghostPool && s.po) {
            m_ghostPool->release(s.ghostSerial);
        }

        // Mark slot free
        s.po = nullptr;
        s.owner = kNoOwner;
        s.inUse = false;
        s.attemptIdx = UINT32_MAX;
        s.isP2 = false;
        m_free.push_back(idx);
    }

    void releaseOwner(int ownerKey) {
        auto it = m_ownerToIndex.find(ownerKey);
        if (it == m_ownerToIndex.end()) return;
        const int idx = it->second;
        // releaseIndex will erase mapping
        releaseIndex(idx);
    }

    void clearOwners(bool releaseToGhostPool = true) {
        if (m_max <= 0) return;

        if (releaseToGhostPool && m_ghostPool) {
            for (int i = 0; i < m_max; ++i) {
                auto& s = m_slots[(size_t)i];
                if (s.inUse && s.po) {
                    m_ghostPool->release(s.ghostSerial);
                }
            }
        }

        m_ownerToIndex.clear();
        m_free.clear();
        m_activeAttempts.clear();
        m_attemptToActiveIndex.clear();

        for (int i = 0; i < m_max; ++i) {
            auto& s = m_slots[(size_t)i];
            s.po = nullptr;
            s.owner = kNoOwner;
            s.inUse = false;
            s.attemptIdx = UINT32_MAX;
            s.isP2 = false;
            if (s.ghostSerial == 0) s.ghostSerial = serialForIndex_(i);
            m_free.push_back(i);
        }
    }

    void clearAfterGhostPoolCleared() {
        m_ownerToIndex.clear();
        m_free.clear();
        m_activeAttempts.clear();
        m_attemptToActiveIndex.clear();

        for (int i = 0; i < m_max; ++i) {
            auto& s = m_slots[(size_t)i];
            s.po = nullptr;
            s.owner = kNoOwner;
            s.inUse = false;
            s.attemptIdx = UINT32_MAX;
            s.isP2 = false;
            if (s.ghostSerial == 0) s.ghostSerial = serialForIndex_(i);
            m_free.push_back(i);
        }
    }

    void setGhostPool(ProfiledGhostPool* gp) { m_ghostPool = gp; }

    const std::vector<ActiveAttemptUse>& getActiveAttempts() const {
        return m_activeAttempts;
    }
};