// random_color_ids.hpp
#pragma once
#include <array>

// saved mask key (one char per SLOT in kRandomColorIDs: '1' enabled, '0' disabled)
static constexpr const char* kGhostRandomColorsMaskKey = "ghost-random-colors-mask";

// Why did robtop use this order????
static constexpr std::array<int, 107> kRandomColorIDs = {
    51, 19, 48, 9, 62, 63, 10, 29, 70, 42, 11, 27, 72, 73, 0, 1,
    37, 53, 54, 55, 26, 59, 60, 61, 71, 14, 31, 45, 105, 28, 32, 20,
    25, 56, 57, 58, 30, 64, 65, 66, 46, 67, 68, 69, 2, 38, 79, 80,
    74, 75, 44, 3, 83, 16, 4, 5, 52, 41, 6, 35, 98, 8, 36, 103,
    40, 76, 77, 78, 22, 39, 84, 50, 47, 23, 92, 93, 7, 13, 24, 104,
    33, 21, 81, 82, 34, 85, 86, 87, 49, 95, 96, 97, 43, 99, 100, 101,
    106,88, 89, 90,
    12, 91, 17, 102, 18, 94, 15
};

static inline constexpr char kDefaultRandomColorMask[] =
    "11001100100000000000000010000000000000000000000001001000110010001000000010000000000000000000000010001000000";

static constexpr int kRandomColorSlots = (int)kRandomColorIDs.size();
