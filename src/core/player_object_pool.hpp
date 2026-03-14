// player_object_pool.hpp
#pragma once

#include <algorithm>
#include <deque>
#include <limits>
#include <unordered_map>
#include <vector>

#include <Geode/binding/PlayerObject.hpp>

#include "../core/ghost_pool.hpp"

struct PlayerObjectPool {
    struct Handle {
        int index = -1; // slot index in this pool
        PlayerObject* po = nullptr; // non-owning pointer from ghost pool
        explicit operator bool() const { return index >= 0 && po != nullptr; }
    };

private:
    struct Slot {
        PlayerObject* po = nullptr; // non-owning
        int owner = kNoOwner; // attempt serial or other owner key
        int ghostSerial = 0; // serial used to acquire/reacquire from ghost pool
        bool inUse = false;
    };

    static constexpr int kNoOwner = std::numeric_limits<int>::min();

    ProfiledGhostPool* m_ghostPool = nullptr;

    // Fixed size
    int m_max = 0;
    int m_serialBase = 0x60000000; // default; change if you want

    std::vector<Slot> m_slots;
    std::deque<int> m_free; // indices available for reuse

    // owner -> slot index (so same attempt can reacquire its slot)
    std::unordered_map<int, int> m_ownerToIndex;

    // Helper: stable per-slot serial used with the GhostPool
    int serialForIndex_(int idx) const {
        // Keep it simple and stable:
        // - idx in [0, m_max)
        // - serialBase chosen to avoid colliding with "real" attempt serials
        return m_serialBase + idx;
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
    }

    bool isInitialized() const { return m_max > 0; }
    int capacity() const { return m_max; }

    int inUseCount() const {
        return m_max - (int)m_free.size();
    }

    // Returns existing handle if owner already has one, otherwise allocates a new slot
    Handle acquireForOwner(int ownerKey) {
        if (!m_ghostPool || m_max <= 0) return {};

        // Already assigned?
        auto it = m_ownerToIndex.find(ownerKey);
        if (it != m_ownerToIndex.end()) {
            const int idx = it->second;
            if (idx >= 0 && idx < m_max) {
                auto& s = m_slots[(size_t)idx];
                if (s.inUse) {
                    // If pointer is null (e.g., after ghostpool cleared), reacquire
                    if (!s.po) {
                        s.po = m_ghostPool->acquire(s.ghostSerial);
                    }
                    return { idx, s.po };
                }
            }
            // stale mapping, fall through to allocate fresh
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
        // Acquire from ghost pool using stable per-slot serial
        s.po = m_ghostPool->acquire(s.ghostSerial);

        if (!s.po) {
            // Failed: return slot to free list
            s.inUse = false;
            s.owner = kNoOwner;
            m_free.push_back(idx);
            return {};
        }

        m_ownerToIndex[ownerKey] = idx;
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

        // Return to ghostpool's free list (does NOT destroy the object)
        if (m_ghostPool && s.po) {
            m_ghostPool->release(s.ghostSerial);
        }

        // Mark slot free
        s.po = nullptr; // avoid stale pointer usage
        s.owner = kNoOwner;
        s.inUse = false;
        m_free.push_back(idx);
    }

    // release by owner (not currently using)
    void releaseOwner(int ownerKey) {
        auto it = m_ownerToIndex.find(ownerKey);
        if (it == m_ownerToIndex.end()) return;
        const int idx = it->second;
        // releaseIndex will erase mapping
        releaseIndex(idx);
    }

    // Clear all owners and make all slots free again
    // If releaseToGhostPool=true, call ghostPool->release(serial) for each in-use slot
    // (good so trails/visibility reset)
    // This does not destroy PlayerObjects. It just makes them unassigned/available
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

        for (int i = 0; i < m_max; ++i) {
            auto& s = m_slots[(size_t)i];
            s.po = nullptr;
            s.owner = kNoOwner;
            s.inUse = false;
            // keep ghostSerial stable
            if (s.ghostSerial == 0) s.ghostSerial = serialForIndex_(i);
            m_free.push_back(i);
        }
    }

    // SAFE INTERNAL CLEAR.
    // Called after ProfiledGhostPool has been cleared/cleanup externally
    // This drops the pointers/mappings so we don't keep stale refs and we can reassign later
    void clearAfterGhostPoolCleared() {
        m_ownerToIndex.clear();
        m_free.clear();

        for (int i = 0; i < m_max; ++i) {
            auto& s = m_slots[(size_t)i];
            s.po = nullptr;
            s.owner = kNoOwner;
            s.inUse = false;
            if (s.ghostSerial == 0) s.ghostSerial = serialForIndex_(i);
            m_free.push_back(i);
        }
    }

    // If ghost pool pointer changes (reinit), call this function (not currently used)
    void setGhostPool(ProfiledGhostPool* gp) { m_ghostPool = gp; }
};