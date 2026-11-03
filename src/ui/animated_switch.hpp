#pragma once

#include <Geode/Geode.hpp>

namespace attemptplayback::ui {

class AnimatedSwitch final : public CCMenuItemToggler {
public:
    static AnimatedSwitch* create(cocos2d::CCObject* target, cocos2d::SEL_MenuHandler callback);

    static void setDefaultDuration(float seconds);
    void setDuration(float seconds);

    void activate() override;

private:
    cocos2d::CCNode* m_offFace = nullptr;
    cocos2d::CCNode* m_onFace = nullptr;
    float m_duration = 0.5f;

    bool initSwitch(cocos2d::CCObject* target, cocos2d::SEL_MenuHandler callback);
    static cocos2d::CCNode* createFace(bool on);
};

}
