#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(HardStreakHook, HardStreak) {
    void addPoint(cocos2d::CCPoint point) {
        auto& G = Ghosts::I();

        if (G.isModEnabled()) {
            if (auto* base = GJBaseGameLayer::get()) {
                if (auto* pl = typeinfo_cast<PlayLayer*>(base)) {
                    if (pl->m_player1 && this == pl->m_player1->m_waveTrail) {
                        if (G.shouldDisableHardStreakAddPoint()) return;
                        G.markWavePointThisFrameP1();
                    } else if (pl->m_player2 && this == pl->m_player2->m_waveTrail) {
                        if (G.shouldDisableHardStreakAddPoint()) return;
                        G.markWavePointThisFrameP2();
                    }
                }
            }
        }

        HardStreak::addPoint(point);
    }
};