#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include "popup_layout_metrics.hpp"

namespace attemptplayback::ui {

class LayoutPopup : public geode::Popup {
protected:
    bool init(float width, float height);
    void onEnter() override;
    void onExit() override;

    void useFullscreenGradient(cocos2d::ccColor3B color);

    void useTransparentPreview();

private:
    void refreshLayout(float dt);
    popup_layout::Size m_design{0.f, 0.f};
    popup_layout::Size m_lastViewport{0.f, 0.f};
    cocos2d::CCSprite* m_fullscreenGradient = nullptr;
};

}
