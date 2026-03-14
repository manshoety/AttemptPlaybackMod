// hook_editor_layer.cpp
#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;
        if (Ghosts::I().isModEnabled()) {
            Ghosts::I().saveNewAttemptsForCurrentLevel();
            Ghosts::I().onQuit();
        }
        return true;
    }
};
