// hook_play_layer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include "../core/ghost_manager.hpp"
#include "../core/globals.hpp"

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
    $override bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        Ghosts::I().updateModEnabled();
        if (Ghosts::I().isModEnabled()) {
            // log::info("Attaching to level");
            Ghosts::I().prepareLevelPersistence(level->m_levelID, this);
            Ghosts::I().attach(this);

            int lvlId = level ? level->m_levelID : 0;
            Ghosts::I().m_levelIDOnAttach = lvlId;
            if (Ghosts::I().isRecording()) {
                Ghosts::I().renumberCurrentAttemptIfFresh();
            }

            Ghosts::I().stopReplay();
        }
        
        return true;
    }
    $override void resetLevel() {
        //log::info("[PlayLayer] resetLevel");
        if (Ghosts::I().isModEnabled()) Ghosts::I().setResetting(true);
        
        PlayLayer::resetLevel();
        if (Ghosts::I().isModEnabled()) {
            Ghosts::I().onReset();
            Ghosts::I().setResetting(false);
        }
    }
    // Someone crashed here? Who knows why.
    //$override void updateTimeLabel(int seconds, int centiseconds, bool decimals) {
    //    log::info("[PlayLayer] updateTimeLabel seconds: {}, centiseconds: {}, decimals: {}", seconds, centiseconds, decimals);
    //    PlayLayer::updateTimeLabel(seconds, centiseconds, decimals);
    //}
    //$override void onExit() { Pausing?
    //    //log::info("[PlayLayer] onExit");
    //    PlayLayer::onExit();
    //}
    $override void onQuit() {
        //log::info("[PlayLayer] onQuit");
        if (Ghosts::I().isModEnabled()) {
            Ghosts::I().saveNewAttemptsForCurrentLevel();
            Ghosts::I().onQuit();
            Ghosts::I().stopReplay();
        } else {
            Ghosts::I().onQuit();
            Ghosts::I().stopReplay();
        }
        PlayLayer::onQuit();
    }
    static void onModify(auto& self) {
        auto _ = self.setHookPriority("PlayLayer::levelComplete", -1000000);
    }
    $override void levelComplete() {
        //log::info("[PlayLayer] levelComplete");
        if (Ghosts::I().isModEnabled()) {
            if (Ghosts::I().safeMode_enabled) {
                onQuit();
                return;
            }
            Ghosts::I().onComplete();
        }
        PlayLayer::levelComplete();
    } 
    // Idk how to check consistantly if a mod is using noclip
    // I want to add a toggle for "don't record noclip runs"
    $override void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        auto& G = Ghosts::I();
        if (!G.isModEnabled()) {
            PlayLayer::destroyPlayer(p0, p1);
            return;
        }

        if (p0 && p0 == this->m_player1 || p0 == this->m_player2) {
            if (G.noclip_enabled) return;
            PlayLayer::destroyPlayer(p0, p1);
            if (this->m_playerDied) {
                //log::info("[PlayLayer] destroyPlayer");
                G.onDeath();
            }
        }
        else PlayLayer::destroyPlayer(p0, p1);
    }
    $override void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (!Ghosts::I().isModEnabled() || !Ghosts::I().safeMode_enabled) {
            PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
        }
    }
    $override void updateAttempts() {
        if (Ghosts::I().isModEnabled() && Ghosts::I().isUpdateAttemptCountBlocked()) return;
        PlayLayer::updateAttempts();
    }
    //$override void activateEndTrigger(int targetID, bool reverse, bool lockPlayerY) { // can't hook
    //    log::info("[PlayLayer] activateEndTrigger targetID {}, reverse {}, lockPlayerY {}", targetID, reverse, lockPlayerY);
    //    PlayLayer::activateEndTrigger(targetID, reverse, lockPlayerY);
    //}
    //$override void pauseGame(bool p0) {
    //    //log::info("[PlayLayer] pauseGame {}", p0);
    //    PlayLayer::pauseGame(p0);
    //}
    //$override void storeCheckpoint(CheckpointObject* p0) {
    //    //log::info("[PlayLayer] storeCheckpoint()");
    //    PlayLayer::storeCheckpoint(p0);
    //}
    //$override void createCheckpoint() {
    //    //log::info("[PlayLayer] createCheckpoint()");
    //    PlayLayer::createCheckpoint();
    //}
    //$override void removeCheckpoint(bool p0) {
        //log::info("[PlayLayer] removeCheckpoint({})", p0);
    //    PlayLayer::removeCheckpoint(p0);
    //    if (Ghosts::I().isModEnabled()) {
    //        Ghosts::I().onPracticeUndoCheckpoint();
    //    }
    //}
    $override void togglePracticeMode(bool on) {
        // log::info("[PlayLayer] togglePracticeMode({})", on);
        if (Ghosts::I().isModEnabled()) Ghosts::I().onPracticeToggle(on);
        PlayLayer::togglePracticeMode(on);
    }
    //$override void commitJumps() {
    //    PlayLayer::commitJumps();
    //}
    //$override void update(float dt) {
    //    PlayLayer::update(dt);
    //}
    
    //$override void postUpdate(float dt) {
    //    PlayLayer::postUpdate(dt);
        
    //}
    //$override float getCurrentPercent() {
    //    return PlayLayer::getCurrentPercent();
    //}
};


class $modify(DestroyPlayerEarliest, PlayLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First)) {
            log::warn("Failed to set earliest destroyPlayer priority");
        }
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        if (Ghosts::I().isModEnabled()) {
            if (p0 && p1 && p0 == this->m_player1 || p0 == this->m_player2) {
                if (!(p1->m_positionX == 0 && p1->m_positionY == 105)) Ghosts::I().beginPlayerDestroyedCheck();
            }
        }
        
        PlayLayer::destroyPlayer(p0, p1);
    }
};

class $modify(DestroyPlayerLatest, PlayLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPost("PlayLayer::destroyPlayer", Priority::Last)) {
            log::warn("Failed to set latest destroyPlayer priority");
        }
        if (!self.setHookPriorityPost("PlayLayer::resetLevel", Priority::Last)) {
            log::warn("Failed to set latest resetLevel priority");
        }
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);

        if (Ghosts::I().isModEnabled()) {
            if (p0 && p0 == this->m_player1 || p0 == this->m_player2) {
                Ghosts::I().endPlayerDestroyedCheck(this->m_playerDied);
                // log::info("End player destroyed");
                // log::info("Object: {} {} {} {} {} {}", p1->m_isDisabled, p1->m_isNoTouch,  p1->m_isStartPos,  p1->m_isPassable,  p1->m_positionX,  p1->m_positionY);
            }
        }
    }
    void resetLevel() {
        PlayLayer::resetLevel();
        if (Ghosts::I().isModEnabled()) Ghosts::I().resetNoclipDetectedFlag();
        // log::info("Reset level");
    }
};


