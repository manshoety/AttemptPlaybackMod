// dashing_args.hpp
#pragma once
#include <variant>
#include <Geode/DefaultInclude.hpp>
#include <Geode/cocos/cocoa/CCGeometry.h>

class DashRingObject;

namespace attemptplayback {
    using Argument = std::variant<unsigned char, DashRingObject*, cocos2d::CCPoint>;

    extern Argument g_dashArg0;
    extern Argument g_dashArg1;

    inline DashRingObject* p0d() {
        if (auto p = std::get_if<DashRingObject*>(&g_dashArg0)) return *p;
        return nullptr;
    }
    inline DashRingObject* p1d() {
        if (auto p = std::get_if<DashRingObject*>(&g_dashArg1)) return *p;
        return nullptr;
    }
}
