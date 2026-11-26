#pragma once

#include <algorithm>
#include <cmath>

namespace attemptplayback::ui::popup_layout {

struct Size {
    float width;
    float height;
};

inline constexpr Size Compact{300.f, 200.f};
inline constexpr Size ReplayDialog{360.f, 270.f};
inline constexpr Size Medium{440.f, 300.f};
inline constexpr Size Large{600.f, 340.f};
inline constexpr Size Wide{700.f, 380.f};
inline constexpr Size PositionPreview{300.f, 185.f};

inline constexpr float ScreenMarginX = 12.f;
inline constexpr float ScreenMarginY = 10.f;
inline constexpr float ContentPadding = 16.f;
inline constexpr float ControlGap = 8.f;
inline constexpr float CloseCenterInset = 10.f;
inline constexpr float CloseDiameter = 42.f;
inline constexpr float CloseOverhang = CloseDiameter * .5f - CloseCenterInset;
inline constexpr float PreferredScale = 1.f;

inline bool validSize(Size size) {
    return std::isfinite(size.width) && std::isfinite(size.height)
        && size.width > 0.f && size.height > 0.f;
}

inline float fitScale(Size viewport, Size design, float preferred = PreferredScale) {
    if (!validSize(viewport) || !validSize(design)
        || !std::isfinite(preferred) || preferred <= 0.f) return 0.f;

    const float marginX = std::min(ScreenMarginX, viewport.width * .25f);
    const float marginY = std::min(ScreenMarginY, viewport.height * .25f);
    return std::min({preferred, (viewport.width - 2.f * marginX) / design.width,
        (viewport.height - 2.f * marginY) / design.height,
        viewport.width / (design.width + 2.f * CloseOverhang),
        viewport.height / (design.height + 2.f * CloseOverhang)});
}

}
