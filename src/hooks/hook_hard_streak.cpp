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

        if (G.isBotActive()) {
            auto* arr = this->m_pointArray;
            
            // Already has that wave point
            if (arr->count() > 0) {
                auto* lastNode = static_cast<PointNode*>(
                    arr->lastObject()
                );

                if (lastNode) {
                    const auto previousPoint = lastNode->m_point;

                    if (previousPoint.x == point.x && previousPoint.y == point.y) {
                        return;
                    }
                }
            }

            this->m_currentPoint = point;
        }

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