// ghost_pool.hpp
#pragma once

#include <Geode/Geode.hpp>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <UIBuilder.hpp>

#include <string>
#include <sstream>
#include <iomanip>

#if defined(_WIN32)
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
#elif defined(__linux__) || defined(__ANDROID__)
    #include <unistd.h>
    #include <fstream>
#endif

using namespace geode::prelude;

class GhostContainer : public cocos2d::CCNode {
public:
    static GhostContainer* create() {
        auto* ret = new GhostContainer();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

struct PreloadStats {
    double acquireMs = 0;
    double setFrameMs = 0;
    double colorsMs = 0;
    double opacityMs = 0;
    double iconsMs = 0;
    double otherMs = 0;
    int count = 0;
    
    void log() {
        if (count == 0) return;
        //log::info("[PreloadStats] {} preloads, total time breakdown:", count);
        //log::info("  acquire:   {:.1f}ms ({:.3f}ms avg)", acquireMs, acquireMs/count);
        //log::info("  setFrame:  {:.1f}ms ({:.3f}ms avg)", setFrameMs, setFrameMs/count);
        //log::info("  colors:    {:.1f}ms ({:.3f}ms avg)", colorsMs, colorsMs/count);
        //log::info("  opacity:   {:.1f}ms ({:.3f}ms avg)", opacityMs, opacityMs/count);
        //log::info("  icons:     {:.1f}ms ({:.3f}ms avg)", iconsMs, iconsMs/count);
        //log::info("  other:     {:.1f}ms ({:.3f}ms avg)", otherMs, otherMs/count);
        //double total = acquireMs + setFrameMs + colorsMs + opacityMs + iconsMs + otherMs;
        //log::info("  TOTAL:     {:.1f}ms ({:.3f}ms avg per preload)", total, total/count);
    }
    
    void reset() { *this = PreloadStats{}; }
};

inline PreloadStats g_preloadStats;

inline double getTimeMs() {
    static auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
}


class ProfiledGhostPool {
public:
    struct PoolSlot {
        geode::Ref<PlayerObject> ghost = nullptr;
        bool inUse = false;
        int boundSerial = -1;
    };

    uint32_t m_createdGhosts = 0;
    
    struct Stats {
        int totalAcquires = 0;
        double totalAcquireMs = 0;
        double createOnlyMs = 0;
        double addChildMs = 0;
        double trailMs = 0;
        double setupMs = 0;
        double bookkeepingMs = 0;
        
        void log(const char* ctx) {
            if (totalAcquires == 0) return;
            //log::info("[GhostPool:{}] {} acquires", ctx, totalAcquires);
            //log::info("  total:       {:.1f}ms ({:.3f}ms avg)", totalAcquireMs, totalAcquireMstotalAcquires);
            //log::info("  create:      {:.1f}ms ({:.3f}ms avg)", createOnlyMs, createOnlyMs/totalAcquires);
            //log::info("  addChild:    {:.1f}ms ({:.3f}ms avg)", addChildMs, addChildMs/totalAcquires);
            //log::info("  trail:       {:.1f}ms ({:.3f}ms avg)", trailMs, trailMs/totalAcquires);
            //log::info("  setup:       {:.1f}ms ({:.3f}ms avg)", setupMs, setupMs/totalAcquires);
            //log::info("  bookkeeping: {:.1f}ms ({:.3f}ms avg)", bookkeepingMs, bookkeepingMs/totalAcquires);
        }
        void reset() { *this = Stats{}; }
    };

private:
    std::vector<PoolSlot> m_pool;
    std::unordered_map<int, size_t> m_serialToSlot;
    std::deque<size_t> m_freeSlots;
    
    geode::Ref<GhostContainer> m_container = nullptr;
    geode::Ref<GhostContainer> m_trailContainer = nullptr;
    PlayLayer* m_pl = nullptr;
    
    bool m_initialized = false;
    bool m_cleaned = false;
    size_t m_activeCount = 0;
    Stats m_stats;
    
public:
    void initialize(PlayLayer* pl, CCNode* parentForContainer, size_t expectedCapacity = 2000) {
        if (m_initialized) {
            //log::warn("[GhostPool] Already initialized, cleaning up first...");
            cleanup();
        }
        
        m_pl = pl;
        
        m_container = GhostContainer::create();
        m_container->setPosition({0, 0});
        m_container->ignoreAnchorPointForPosition(true);
        
        m_trailContainer = GhostContainer::create();
        m_trailContainer->setPosition({0, 0});
        m_trailContainer->ignoreAnchorPointForPosition(true);
        
        if (parentForContainer) {
            parentForContainer->addChild(m_container, 5);
            parentForContainer->addChild(m_trailContainer, -1);
        }
        
        m_pool.reserve(expectedCapacity);
        m_serialToSlot.reserve(expectedCapacity);
        
        m_stats.reset();
        m_initialized = true;
        m_cleaned = false;
        
        //log::info("[GhostPool] Initialized with capacity {}", expectedCapacity);
    }
    
    PlayerObject* acquire(int serial) {
        double t0 = getTimeMs();
        m_stats.totalAcquires++;
        
        // Check if already bound
        auto it = m_serialToSlot.find(serial);
        if (it != m_serialToSlot.end() && m_pool[it->second].inUse) {
            m_stats.totalAcquireMs += getTimeMs() - t0;
            return m_pool[it->second].ghost.data();
        }
        
        // Check free list
        if (!m_freeSlots.empty()) {
            size_t idx = m_freeSlots.front();
            m_freeSlots.pop_front();
            
            double tBook = getTimeMs();
            m_pool[idx].inUse = true;
            m_pool[idx].boundSerial = serial;
            m_serialToSlot[serial] = idx;
            ++m_activeCount;
            m_stats.bookkeepingMs += getTimeMs() - tBook;
            
            m_stats.totalAcquireMs += getTimeMs() - t0;
            return m_pool[idx].ghost;
        }

        PlayerObject* ghost = createNewGhost();
        if (!ghost) {
            m_stats.totalAcquireMs += getTimeMs() - t0;
            return nullptr;
        }
        
        double tBook = getTimeMs();
        size_t idx = m_pool.size();
        PoolSlot slot;
        slot.ghost = ghost;
        slot.inUse = true;
        slot.boundSerial = serial;
        m_pool.push_back(slot);
        m_serialToSlot[serial] = idx;
        ++m_activeCount;
        m_stats.bookkeepingMs += getTimeMs() - tBook;
        
        m_stats.totalAcquireMs += getTimeMs() - t0;
        
        return ghost;
    }
    
private:

    static void safeRemoveNode(CCNode* n, bool cleanup = true) {
        if (!n) return;
        n->stopAllActions();
        n->unscheduleAllSelectors();
        n->unscheduleUpdate();

        if (n->getParent()) {
            n->removeFromParentAndCleanup(cleanup);
        }
    }

    static void makeNodePassive(cocos2d::CCNode* n) {
        if (!n) return;
        n->stopAllActions();
        n->unscheduleAllSelectors();
        n->unscheduleUpdate();
    }

    static cocos2d::CCLayer* findBestLayer(PlayLayer* pl, CCNode* parentFallback) {
        if (auto* layer = typeinfo_cast<cocos2d::CCLayer*>(parentFallback)) return layer;
        return pl;
    }

    static void moveTrailToNode(PlayerObject* po, cocos2d::CCNode* target, int z = -1) {
        if (!po || !po->m_waveTrail || !target) return;

        if (po->m_waveTrail->getParent() != target) {
            po->m_waveTrail->retain();
            po->m_waveTrail->removeFromParentAndCleanup(false);
            target->addChild(po->m_waveTrail, z);
            po->m_waveTrail->release();
        }
    }

    static cocos2d::CCNode* getWaveTrailParent(PlayLayer* pl) {
        if (!pl) return nullptr;

        // Copy whatever GD uses
        if (pl->m_player1 && pl->m_player1->m_waveTrail) {
            if (auto* p = pl->m_player1->m_waveTrail->getParent())
                return p;
        }

        // Fallbacks that arent batch nodes
        if (pl->m_objectLayer) return pl->m_objectLayer;
        return pl;
    }

    static cocos2d::CCNode* avoidBatchNodes(cocos2d::CCNode* n) {
        while (n && typeinfo_cast<cocos2d::CCSpriteBatchNode*>(n)) {
            n = n->getParent(); // climb out of the batch node
        }
        return n;
    }

    static void moveTrailToNodeSafe(PlayerObject* po, cocos2d::CCNode* target, int z = 0) {
        if (!po || !po->m_waveTrail || !target) return;

        target = avoidBatchNodes(target);
        if (!target) return;

        auto* trail = po->m_waveTrail;

        if (trail->getParent() != target) {
            trail->retain();
            trail->removeFromParentAndCleanup(false);
            target->addChild(trail, z);
            trail->release();
        }

        // HardStreak is a draw node: keep it untransformed so points are in parent space
        trail->setPosition({0.f, 0.f});
        trail->setAnchorPoint({0.f, 0.f});
        trail->setScale(1.f);
        trail->setRotation(0.f);
    }

    PlayerObject* createNewGhost() {
        if (!m_pl || !m_container) return nullptr;
        g_disableUpdate = true;

        auto* gm = GameManager::sharedState();
        int playerId = gm->getPlayerFrame();
        int shipId = gm->getPlayerShip();

        
        PlayerObject* po = Build<PlayerObject>::create(playerId, shipId, m_pl, m_pl->m_objectLayer, false).parent(m_container).collect();
        
        if (!po) {
            g_disableUpdate = false;
            return nullptr;
        }

        po->disablePlayerControls();
        po->m_playEffects = true;


        po->setVisible(false);
        po->setPosition({-10000, -10000});
        po->setZOrder(58);

        if (po->m_waveTrail) {
            po->m_waveTrail->reset();
            po->m_waveTrail->setVisible(false);
            auto* parent = getWaveTrailParent(m_pl);
            moveTrailToNodeSafe(po, parent, 57);

        }

        g_disableUpdate = false;

        return po;
    }
    
public:
    void release(int serial) {
        auto it = m_serialToSlot.find(serial);
        if (it == m_serialToSlot.end()) return;

        size_t idx = it->second;
        if (idx >= m_pool.size()) return;

        auto& slot = m_pool[idx];

        if (slot.ghost) {
            slot.ghost->setVisible(false);
            slot.ghost->setPosition({-10000, -10000});
            if (slot.ghost->m_waveTrail) {
                slot.ghost->m_waveTrail->reset();
                slot.ghost->m_waveTrail->setVisible(false);
            }
        }

        slot.inUse = false;
        slot.boundSerial = -1;
        m_freeSlots.push_back(idx);
        m_serialToSlot.erase(it);
        --m_activeCount;
    }
    
    void releaseAll() {
        if (m_cleaned) return;
        
        for (auto& slot : m_pool) {
            if (auto* g = slot.ghost.data()) {
                g->stopAllActions();
                g->unscheduleAllSelectors();
                g->unscheduleUpdate();
                g->setVisible(false);
                g->setPosition({-10000, -10000});

                if (g->m_waveTrail) {
                    g->m_waveTrail->reset();
                    g->m_waveTrail->setVisible(false);
                }
            }
            
            slot.inUse = false;
            slot.boundSerial = -1;
        }

        m_serialToSlot.clear();
        m_freeSlots.clear();
        
        for (size_t i = 0; i < m_pool.size(); ++i) {
            m_freeSlots.push_back(i);
        }
        
        m_activeCount = 0;
    }

    void cleanup() {
        if (!m_initialized || m_cleaned) return;
        m_cleaned = true;

        if (m_container) {
            m_container->stopAllActions();
            m_container->unscheduleAllSelectors();
            m_container->removeFromParentAndCleanup(true);
        }

        if (m_trailContainer) {
            m_trailContainer->stopAllActions();
            m_trailContainer->unscheduleAllSelectors();
            m_trailContainer->removeFromParentAndCleanup(true);
        }

        for (auto& slot : m_pool) {
            if (auto* g = slot.ghost.data()) {
                g->stopAllActions();
                g->unscheduleAllSelectors();
                g->unscheduleUpdate();
            }
            slot.ghost = nullptr;
            slot.inUse = false;
            slot.boundSerial = -1;
        }

        m_pool.clear();
        m_serialToSlot.clear();
        m_freeSlots.clear();
        m_activeCount = 0;

        m_container = nullptr;
        m_trailContainer = nullptr;
        m_pl = nullptr;

        m_initialized = false;
    }
    
    PlayerObject* get(int serial) {
        auto it = m_serialToSlot.find(serial);
        if (it != m_serialToSlot.end() && m_pool[it->second].inUse) {
            return m_pool[it->second].ghost.data();
        }
        return nullptr;
    }

    void logStats(const char* ctx = "Stats") { m_stats.log(ctx); }
    void resetStats() { m_stats.reset(); }
    
    size_t activeCount() const { return m_activeCount; }
    size_t poolSize() const { return m_pool.size(); }
    bool isInitialized() const { return m_initialized; }
};