// hook_player_object.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include "../core/ghost_manager.hpp"
#include "../core/globals.hpp"
#include <Geode/loader/Log.hpp>

using namespace geode::prelude;

class $modify(MyPlayerObject, PlayerObject) {
    //bool init(int player, int ship, GJBaseGameLayer* gameLayer, CCLayer* layer, bool playLayer) {
    //    if (!PlayerObject::init(player, ship, gameLayer, layer, playLayer)) return false;
        // log::info("PlayerObject::init");
    //    return true;
    //}
    bool pushButton(PlayerButton btn) {
        if (auto* pl = typeinfo_cast<PlayLayer*>(this->m_gameLayer)) {
            if (this == pl->m_player1) {
                // log::info("button: {}, holding: true", int(btn));
                if (btn==PlayerButton::Jump) Ghosts::I().setP1Hold(true);
                else if (btn==PlayerButton::Left) Ghosts::I().setP1LHold(true);
                else if (btn==PlayerButton::Right) Ghosts::I().setP1RHold(true);
                // log::info("[PlayerObject] pushButton({})", int(btn));
            }
            else if (this == pl->m_player2) {
                if (btn==PlayerButton::Jump) Ghosts::I().setP2Hold(true);
                else if (btn==PlayerButton::Left) Ghosts::I().setP2LHold(true);
                else if (btn==PlayerButton::Right) Ghosts::I().setP2RHold(true);
            }
        }
        return PlayerObject::pushButton(btn);
    }
    bool releaseButton(PlayerButton btn) {
        if (auto* pl = typeinfo_cast<PlayLayer*>(this->m_gameLayer)) {
            if (this == pl->m_player1) {
                // log::info("button: {}, holding: false", int(btn));
                if (btn==PlayerButton::Jump) Ghosts::I().setP1Hold(false);
                else if (btn==PlayerButton::Left) Ghosts::I().setP1LHold(false);
                else if (btn==PlayerButton::Right) Ghosts::I().setP1RHold(false);
                // log::info("[PlayerObject] releaseButton({})", int(btn));
            }
            else if (this == pl->m_player2) {
                
                if (btn==PlayerButton::Jump) Ghosts::I().setP2Hold(false);
                else if (btn==PlayerButton::Left) Ghosts::I().setP2LHold(false);
                else if (btn==PlayerButton::Right) Ghosts::I().setP2RHold(false);
            }
        }
        return PlayerObject::releaseButton(btn);
    } 
    // Update order
    // SET previous position
    // Player object update
    // SET MY position
    // SET current position
    // Player object update done
    // SET current position (If I manually override this, I can avoid wacky GD placement issues, and have follow player objects work)
    // And I need to set that to what GD wants it to be, not what I want it to be
    // Still have no clue why GD sometimes sets this value to actual garbage when I mess with the real player position (super wrong y pos like 405)
    void setPosition(CCPoint const& position) {
        auto& ghosts = Ghosts::I();

        auto* pl = typeinfo_cast<PlayLayer*>(this->m_gameLayer);
        const bool isP1 = pl && this == pl->m_player1;
        const bool isP2 = pl && this == pl->m_player2;

        if (ghosts.isModEnabled() && ghosts.isBotActive()) {
            if (ghosts.isResetting()) {
                PlayerObject::setPosition(position);
                return;
            }

            if (ghosts.isdisablePlayerMove()) {
                if (isP1) {
                    PlayerObject::setPosition(ghosts.forceSetPosP1);
                    return;
                }
                if (isP2) {
                    PlayerObject::setPosition(ghosts.forceSetPosP2);
                    return;
                }
            } else {
                if (isP1) ghosts.forceSetPosP1 = position;
                else if (isP2) ghosts.forceSetPosP2 = position;
            }
        }

        PlayerObject::setPosition(position);
    }
    void update(float p0) { //  is p0 the frac of real GJ update?
        if (Ghosts::I().isModEnabled()) {
            if (auto* pl = typeinfo_cast<PlayLayer*>(this->m_gameLayer)) {
                if (pl) {
                    // log::info("p1: {}, p2: {}", this == pl->m_player1, this == pl->m_player2);
                    if (this == pl->m_player1) {
                        //log::info("update");
                        Ghosts::I().setdisablePlayerMove(false);
                        
                        Ghosts::I().updateClickState(/*isPlayer1*/true);
                        Ghosts::I().applySegmentBasedReplay_(/*isPlayer1*/true);
                        
                        Ghosts::I().beginPostUpdateTick_();

                        Ghosts::I().preUpdate();
                        Ghosts::I().recordUpdate(/*isPlayer1*/true);

                        Ghosts::I().endPostUpdateTick_();

                        PlayerObject::update(p0);
                        //log::info("update done");

                        Ghosts::I().setdisablePlayerMove(true);

                        return;
                    }
                    else if (this == pl->m_player2) {
                        Ghosts::I().setdisablePlayerMove(false);
                        Ghosts::I().updateClickState(/*isPlayer1*/false);
                        Ghosts::I().applySegmentBasedReplay_(/*isPlayer1*/false);
                        
                        Ghosts::I().beginPostUpdateTick_();

                        Ghosts::I().preUpdateP2();
                        Ghosts::I().recordUpdate(/*isPlayer1*/false);
                        
                        Ghosts::I().endPostUpdateTick_();

                        PlayerObject::update(p0);
                        Ghosts::I().setdisablePlayerMove(true);

                        return;
                    }
                    //log::info("[PlayerObject] update {}", p0);
                }
            }
        }

        PlayerObject::update(p0);
    }

    void releaseAllButtons() {
        //log::info("[PlayerObject] releaseAllButtons()");
        Ghosts::I().setP1Hold(false);
        Ghosts::I().setP1LHold(false);
        Ghosts::I().setP1RHold(false);
        Ghosts::I().setP2Hold(false);
        Ghosts::I().setP2LHold(false);
        Ghosts::I().setP2RHold(false);

        PlayerObject::releaseAllButtons();
    }

    void flipGravity(bool p0, bool p1) {
        // log::info("[PlayerObject] flipGravity({}, {})", p0, p1);
        PlayerObject::flipGravity(p0, p1);
    }

    void toggleFlyMode(bool p0, bool p1) {
        //log::info("[PlayerObject] toggleFlyMode({}, {})", p0, p1);
        PlayerObject::toggleFlyMode(p0, p1);
    }

    void toggleDartMode(bool p0, bool p1) {
        //log::info("[PlayerObject] toggleDartMode({}, {})", p0, p1);
        PlayerObject::toggleDartMode(p0, p1);
    }

    void setColor(ccColor3B const& color) {
        //log::info("[PlayerObject] color set, was me: {}", Ghosts::I().testerBool);
        PlayerObject::setColor(color);
    }
};
