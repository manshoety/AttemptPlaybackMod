#pragma once

#include <Geode/DefaultInclude.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>

namespace attemptplayback::ui {

class AttemptScrollView final : public cocos2d::CCNode {
public:
    static AttemptScrollView* create(cocos2d::CCSize const& size) {
        auto* ret = new AttemptScrollView();
        if (ret && ret->init(size)) {
            ret->autorelease();
            return ret;
        }

        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    cocos2d::CCNode* getContentLayer() const {
        return m_scrollLayer ? m_scrollLayer->m_contentLayer : nullptr;
    }

    void setInnerContentSize(cocos2d::CCSize const& size) {
        if (!m_scrollLayer) return;
        m_scrollLayer->setContentLayerSize(size);
    }

    void setScrollEnabled(bool enabled) {
        if (!m_scrollLayer) return;

        m_scrollLayer->m_disableHorizontal = true;
        m_scrollLayer->m_disableVertical = !enabled;
        m_scrollLayer->m_disableMovement = !enabled;
        m_scrollLayer->enableScrollWheel(enabled);
        m_scrollLayer->setTouchEnabled(enabled);

        if (m_scrollbar) {
            m_scrollbar->setVisible(enabled);
            m_scrollbar->setTouchEnabled(enabled);
        }

        if (!enabled) scrollToTop();
    }

    void scrollToTop() {
        if (m_scrollLayer) m_scrollLayer->scrollToTop();
    }

    void setTouchPriority(int priority) {
        if (m_scrollLayer) m_scrollLayer->setTouchPriority(priority);
        if (m_scrollbar) m_scrollbar->setTouchPriority(priority);
    }

private:
    geode::ScrollLayer* m_scrollLayer = nullptr;
    geode::Scrollbar* m_scrollbar = nullptr;

    bool init(cocos2d::CCSize const& size) {
        if (!CCNode::init()) return false;

        setAnchorPoint({0.5f, 0.5f});
        setContentSize(size);

        m_scrollLayer = geode::ScrollLayer::create(size, true, true);
        if (!m_scrollLayer) return false;

        m_scrollLayer->setAnchorPoint({0.f, 0.f});
        m_scrollLayer->setPosition({0.f, 0.f});
        m_scrollLayer->setStealingTouches(true);
        m_scrollLayer->setCancelTouchLimit(7.f);
        m_scrollLayer->m_disableHorizontal = true;
        addChild(m_scrollLayer, 0);

        m_scrollbar = geode::Scrollbar::create(m_scrollLayer);
        if (m_scrollbar) {
            m_scrollbar->setAnchorPoint({0.5f, 0.5f});
            m_scrollbar->setPosition({size.width + 8.f, size.height * 0.5f});
            addChild(m_scrollbar, 1);
        }

        setScrollEnabled(false);
        return true;
    }
};

} // namespace attemptplayback::ui
