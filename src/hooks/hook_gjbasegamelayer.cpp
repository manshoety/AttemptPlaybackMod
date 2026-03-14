// hook_gjbasegamelayer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include "../core/ghost_manager.hpp"
#include "../core/globals.hpp"

using namespace geode::prelude;

class $modify(GJBaseGameLayerHook, GJBaseGameLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("GJBaseGameLayer::update", Priority::First)) {
            log::warn("Failed to set hook priority for GJBaseGameLayer::update");
        }
    }
    void update(float p0) {
        // log::info("[GJBaseGameLayer] update {}", p0);
        GJBaseGameLayer::update(p0);
        // log::info("[postUpdate]");
        if (Ghosts::I().isModEnabled()) Ghosts::I().postUpdate();
        // Try here? Idk
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        //log::info("[GJBaseGameLayer] handleButton: down {}, button {}", down, button);
        //if (button==1) {
        //    if (isPlayer1) Ghosts::I().setP1Hold(down);
        //    else Ghosts::I().setP2Hold(down);
        //}
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};
