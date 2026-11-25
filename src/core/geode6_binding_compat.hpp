#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CheckpointObject.hpp>

// Some stuff doesn't exist on IOS and macOS now for Geode V6 (evil)
namespace attemptplayback::bindingcompat {

inline void resetWaveTrail(HardStreak* trail) {
    if (!trail) return;
#if defined(GEODE_IS_IOS)
    if (trail->m_pointArray) trail->m_pointArray->removeAllObjects();
    trail->clear();
#else
    trail->reset();
#endif
}

inline CheckpointObject* lastCheckpoint(PlayLayer* layer) {
    if (!layer) return nullptr;
#if defined(GEODE_IS_IOS)
    auto* checkpoints = layer->m_checkpointArray;
    return checkpoints && checkpoints->count() != 0
        ? static_cast<CheckpointObject*>(checkpoints->lastObject()) : nullptr;
#else
    return layer->getLastCheckpoint();
#endif
}

inline void setToggleSizeMult(CCMenuItemToggler* toggle, float mult) {
    if (!toggle) return;
#if defined(GEODE_IS_IOS)
    if (toggle->m_offButton) toggle->m_offButton->setSizeMult(mult);
    if (toggle->m_onButton) toggle->m_onButton->setSizeMult(mult);
#else
    toggle->setSizeMult(mult);
#endif
}

}
