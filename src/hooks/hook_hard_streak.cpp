#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(HardStreakHook, HardStreak) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPost("HardStreak::addPoint", Priority::First)) {
            geode::log::warn("Failed to set hook priority for HardStreak::addPoint");
        }
    }

    void addPoint(cocos2d::CCPoint point) {
        auto& G = Ghosts::I();

        if (
            G.allowWaveHook() &&
            !G.skipHardStreakCheck() &&
            !LevelEditorLayer::get() &&
            G.hasModAttachedToLevel()
        ) {
            if (auto* pl = G.getPlayLayer(); pl && G.shouldHandlePlayLayer(pl)) {
                if (pl->m_player1 && this == pl->m_player1->m_waveTrail) {
                    G.markWavePointThisFrameP1();
                }
                else if (pl->m_player2 && this == pl->m_player2->m_waveTrail) {
                    G.markWavePointThisFrameP2();
                }
            }
        }

        HardStreak::addPoint(point);
    }
};