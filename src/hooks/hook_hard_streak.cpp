#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(HardStreakHook, HardStreak) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPost("HardStreak::addPoint", Priority::First)) {
            log::warn("Failed to set first addPoint priority");
        }
    }

    void addPoint(cocos2d::CCPoint point) {
        auto& G = Ghosts::I();

        if (G.allowWaveHook()) {
            bool isP1Trail = false;
            bool isP2Trail = false;

            if (!LevelEditorLayer::get() && G.hasModAttachedToLevel()) {
                auto* pl = G.getPlayLayer();

                if (pl && G.shouldHandlePlayLayer(pl)) {
                    isP1Trail = pl->m_player1 && this == pl->m_player1->m_waveTrail;
                    isP2Trail = pl->m_player2 && this == pl->m_player2->m_waveTrail;
                }
            }
            
            // Need to let Wave Trail Drag Fix mod place points
            if ((isP1Trail || isP2Trail) && !G.skipHardStreakCheck()) {
                //log::info("point");
                if (isP1Trail) G.markWavePointThisFrameP1();
                else G.markWavePointThisFrameP2();
            }
            //else {
            //    log::info("No point");
            //}
        }
        HardStreak::addPoint(point);
    }
};