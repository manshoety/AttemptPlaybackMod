// hook_player_checkpoint.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/PlayerCheckpoint.hpp>
#include <Geode/binding/PlayerCheckpoint.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(PCHook, PlayerCheckpoint) {
    $override bool init() {
        // log::info("[PlayerCheckpoint] init()");
        if (Ghosts::I().isModEnabled()) {
            Ghosts::I().onPracticeCheckpoint();
        }
        if (!PlayerCheckpoint::init()) return false;
        return true;
    }
};
