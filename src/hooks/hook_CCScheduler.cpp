// hook_CCScheduler.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/cocos/CCScheduler.h>
#include <Geode/modify/CCScheduler.hpp>
#include "../core/globals.hpp"
#include <Geode/loader/Log.hpp>

using namespace geode::prelude;

// dankmeme01 is goated for this (allows WAY faster player oject creation)
class $modify(CCScheduler) {
    void scheduleUpdateForTarget(CCObject* pTarget, int nPriority, bool bPaused) {
        if (g_disableUpdate && typeinfo_cast<CCParticleSystem*>(pTarget))
            return;

        CCScheduler::scheduleUpdateForTarget(pTarget, nPriority, bPaused);
    }
};