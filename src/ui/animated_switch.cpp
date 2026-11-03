#include "animated_switch.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <filesystem>

using namespace cocos2d;

namespace attemptplayback::ui {
namespace {
    constexpr float kWidth = 36.f;
    constexpr float kHeight = 18.f;
    constexpr float kLeft = 9.f;
    constexpr float kRight = 27.f;
    constexpr int kThumbTag = 6212;
    float s_defaultDuration = 0.15f;

    // Background yoink correct color
    CCSprite* resourceSprite(const char* file) {
        auto dir = geode::Mod::get()->getResourcesDir();
        for (auto const& relative : {
            std::filesystem::path("resources") / "ui" / file,
            std::filesystem::path("ui") / file,
            std::filesystem::path(file)
        }) {
            const auto path = dir / relative;
            std::error_code ec;
            if (std::filesystem::is_regular_file(path, ec)) {
                if (auto* sprite = CCSprite::create(path.string().c_str())) return sprite;
            }
        }
        return nullptr;
    }
}

void AnimatedSwitch::setDefaultDuration(float seconds) {
    s_defaultDuration = std::clamp(seconds, 0.f, 3.f);
}

void AnimatedSwitch::setDuration(float seconds) {
    m_duration = std::clamp(seconds, 0.f, 3.f);
}

CCNode* AnimatedSwitch::createFace(bool on) {
    auto* root = CCNode::create();
    root->setContentSize({kWidth, kHeight});

    if (auto* track = resourceSprite(on ? "switch-on.png" : "switch-off.png")) {
        const auto size = track->getContentSize();
        track->setScale(std::min(kWidth / std::max(1.f, size.width),
                                 kHeight / std::max(1.f, size.height)));
        track->setPosition({kWidth * .5f, kHeight * .5f});
        if (!on) {
            // doo doo brown
            track->setColor({124, 78, 54});
            track->setOpacity(205);
        } else {
            // nuclear radioactive green
            track->setOpacity(235);
            track->setColor({58, 219, 0});
        }
        root->addChild(track, 0);
    } else if (auto* backup = CCSprite::createWithSpriteFrameName(
                   on ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png")) {
        backup->setScale(0.5f);
        backup->setPosition({kWidth * .5f, kHeight * .5f});
        root->addChild(backup);
    }

    if (auto* thumb = resourceSprite("switch-thumb.png")) {
        const auto size = thumb->getContentSize();
        thumb->setScale(14.f / std::max(1.f, size.width));
        thumb->setPosition({on ? kRight : kLeft, kHeight * .5f});
        thumb->setTag(kThumbTag);
        root->addChild(thumb, 2);
    }
    return root;
}

bool AnimatedSwitch::initSwitch(CCObject* target, SEL_MenuHandler callback) {
    m_duration = s_defaultDuration;
    m_offFace = createFace(false);
    m_onFace = createFace(true);
    if (!m_offFace || !m_onFace || !CCMenuItemToggler::init(m_offFace, m_onFace, target, callback)) {
        m_offFace = m_onFace = nullptr;
        return false;
    }
    setSizeMult(1.15f);
    return true;
}

AnimatedSwitch* AnimatedSwitch::create(CCObject* target, SEL_MenuHandler callback) {
    auto* ret = new AnimatedSwitch();
    if (ret && ret->initSwitch(target, callback)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void AnimatedSwitch::activate() {
    if (!isEnabled() || m_notClickable) return;

    const bool wasOn = isToggled();
    auto* targetFace = wasOn ? m_offFace : m_onFace;
    if (targetFace) {
        if (auto* thumb = targetFace->getChildByTag(kThumbTag)) {
            thumb->stopAllActions();
            thumb->setPosition({wasOn ? kRight : kLeft, kHeight * .5f});
            const CCPoint destination{wasOn ? kLeft : kRight, kHeight * .5f};
            if (m_duration <= 0.f) {
                thumb->setPosition(destination);
            } else {
                thumb->runAction(CCEaseSineInOut::create(CCMoveTo::create(m_duration, destination)));
            }
        }
    }
    CCMenuItemToggler::activate();
}

}
