#include <Geode/DefaultInclude.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/binding/EndLevelLayer.hpp>
#include <Geode/modify/IDManager.hpp>

#include <cmath>

#include "../core/ghost_manager.hpp"

using namespace geode::prelude;

class $modify(AttemptPlaybackEndLevelLayer, EndLevelLayer) {
    struct Fields {
        cocos2d::CCNode* m_hideButton = nullptr;
        bool m_offsetApplied = false;
        bool m_haveLastAppliedX = false;
        float m_lastAppliedX = 0.f;
    };

    static constexpr float kBotHideButtonOffsetX = 2.f;

    void syncHideButtonOffset_() {
        NodeIDs::provideFor(this);

        auto* hideMenu = this->getChildByID("hide-layer-menu");
        if (!hideMenu) {
            hideMenu = this->getChildByIDRecursive("hide-layer-menu");
        }
        auto* hideButton = hideMenu ? hideMenu->getChildByID("hide-button") : nullptr;
        if (!hideButton) {
            m_fields->m_hideButton = nullptr;
            m_fields->m_offsetApplied = false;
            m_fields->m_haveLastAppliedX = false;
            m_fields->m_lastAppliedX = 0.f;
            return;
        }

        if (m_fields->m_hideButton != hideButton) {
            m_fields->m_hideButton = hideButton;
            m_fields->m_offsetApplied = false;
            m_fields->m_haveLastAppliedX = false;
            m_fields->m_lastAppliedX = hideButton->getPositionX();
        }

        float baseX = hideButton->getPositionX();


        if (m_fields->m_haveLastAppliedX && std::abs(baseX - m_fields->m_lastAppliedX) < 0.01f) {
            baseX -= m_fields->m_offsetApplied ? kBotHideButtonOffsetX : 0.f;
        }

        const bool shouldOffset = Ghosts::I().didUseBotThisAttempt();
        const float targetX = baseX + (shouldOffset ? kBotHideButtonOffsetX : 0.f);

        hideButton->setPositionX(targetX);

        m_fields->m_offsetApplied = shouldOffset;
        m_fields->m_haveLastAppliedX = true;
        m_fields->m_lastAppliedX = targetX;
    }

    static void onModify(auto& self) {
        (void)self.setHookPriorityPost(
            "EndLevelLayer::customSetup",
            Priority::Last
        );
        (void)self.setHookPriorityPost(
            "EndLevelLayer::showLayer",
            Priority::Last
        );
    }

    $override void customSetup() {
        EndLevelLayer::customSetup();
        syncHideButtonOffset_();
    }
 
    $override void showLayer(bool instant) {
        EndLevelLayer::showLayer(instant);
        syncHideButtonOffset_();
     }
};
