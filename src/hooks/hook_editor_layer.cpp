// hook_editor_layer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        auto& G = Ghosts::I();

        if (G.hasModAttachedToLevel()) {
            G.clearPlayLayerGhostTextLabel();
            G.saveNewAttemptsForCurrentLevel();
            G.onQuit();
        }
        if (!LevelEditorLayer::init(level, noUI)) return false;
        
        
        return true;
    }
};
