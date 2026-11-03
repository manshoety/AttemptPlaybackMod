#pragma once

#include "../core/ghost_manager.hpp"
#include "../core/random_color_ids.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <string>

namespace attemptplayback::ui {
    struct GhostPreviewAppearanceState {
        int playerFrame = -1;
        int playerColor = -1;
        int playerColor2 = -1;
        bool randomIcons = false;
        bool colorsInitialized = false;
        int lastOpacity = -1;
        std::string colorMode;
        std::string maskP1;
        std::string maskP2;
        cocos2d::ccColor3B leftPrimary = {255, 255, 255};
        cocos2d::ccColor3B leftSecondary = {255, 255, 255};
        cocos2d::ccColor3B rightPrimary = {255, 255, 255};
        cocos2d::ccColor3B rightSecondary = {255, 255, 255};
    };

    inline void updateGhostPreviewAppearance(
        SimplePlayer* middle, SimplePlayer* left, SimplePlayer* right,
        GhostPreviewAppearanceState& state, int opacity
    ) {
        auto* gm = GameManager::sharedState();
        if (!gm || !middle) return;

        auto& ghosts = Ghosts::I();
        const int playerFrame = std::max(1, gm->getPlayerFrame());
        const bool randomIcons = ghosts.isRandomIconsEnabled();
        const bool iconChanged = state.playerFrame != playerFrame || state.randomIcons != randomIcons;
        if (iconChanged) {
            middle->updatePlayerFrame(playerFrame, IconType::Cube);
            const int leftFrame = randomIcons ? 1 + randomIconFrame(IconType::Cube, 0xA571u) : playerFrame;
            int rightFrame = randomIcons ? 1 + randomIconFrame(IconType::Cube, 0xC19Du) : playerFrame;
            if (randomIcons && rightFrame == leftFrame) rightFrame = rightFrame % 485 + 1;
            if (left) left->updatePlayerFrame(leftFrame, IconType::Cube);
            if (right) right->updatePlayerFrame(rightFrame, IconType::Cube);
            state.playerFrame = playerFrame;
            state.randomIcons = randomIcons;
        }

        const int playerColor = gm->getPlayerColor();
        const int playerColor2 = gm->getPlayerColor2();
        const auto playerPrimary = gm->colorForIdx(playerColor);
        const auto playerSecondary = gm->colorForIdx(playerColor2);
        auto* mod = geode::Mod::get();
        const auto mode = mod->getSavedValue<std::string>("ghost-colors");
        const auto p1Mask = mod->getSavedValue<std::string>(kGhostRandomColorsMaskP1Key);
        const auto p2Mask = mod->getSavedValue<std::string>(kGhostRandomColorsMaskP2Key);
        const bool colorChanged = mode != state.colorMode || p1Mask != state.maskP1 || p2Mask != state.maskP2
            || state.playerColor != playerColor || state.playerColor2 != playerColor2 || !state.colorsInitialized;
        if (colorChanged) {
            state.leftPrimary = playerPrimary;
            state.leftSecondary = playerSecondary;
            state.rightPrimary = playerPrimary;
            state.rightSecondary = playerSecondary;
            if (mode == "White" || mode == "white") {
                state.leftPrimary = state.leftSecondary = {255, 255, 255};
                state.rightPrimary = state.rightSecondary = {255, 255, 255};
            }
            else if (mode == "Black" || mode == "black") {
                state.leftPrimary = state.leftSecondary = {35, 35, 35};
                state.rightPrimary = state.rightSecondary = {35, 35, 35};
            }
            else if (mode == "Random" || mode == "random") {
                state.leftPrimary = gm->colorForIdx(ghosts.pickRandomAllowedGhostColorIdx(0xA571u, true));
                state.leftSecondary = gm->colorForIdx(ghosts.pickRandomAllowedGhostColorIdx(0xA573u, true));
                state.rightPrimary = gm->colorForIdx(ghosts.pickRandomAllowedGhostColorIdx(0xC19Du, false));
                state.rightSecondary = gm->colorForIdx(ghosts.pickRandomAllowedGhostColorIdx(0xC19Fu, false));
            }
            state.colorsInitialized = true;
            state.playerColor = playerColor;
            state.playerColor2 = playerColor2;
            state.colorMode = mode;
            state.maskP1 = p1Mask;
            state.maskP2 = p2Mask;
        }

        if (colorChanged || iconChanged) {
            middle->setColor(playerPrimary);
            middle->setSecondColor(playerSecondary);
            middle->setOpacity(255);
            if (left) {
                left->setColor(state.leftPrimary);
                left->setSecondColor(state.leftSecondary);
            }
            if (right) {
                right->setColor(state.rightPrimary);
                right->setSecondColor(state.rightSecondary);
            }
        }
        const int clampedOpacity = std::clamp(opacity, 0, 255);
        if (clampedOpacity != state.lastOpacity || iconChanged) {
            const auto alpha = static_cast<GLubyte>(clampedOpacity);
            if (left) left->setOpacity(alpha);
            if (right) right->setOpacity(alpha);
            state.lastOpacity = clampedOpacity;
        }
    }
}
