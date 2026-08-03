// globals.cpp
#pragma once
#include <cstddef>   // size_t
#include <cstdint>   // uintptr_t

extern bool g_isNextFrame;
extern bool g_isCanUpdatePlayerAgain;
extern std::size_t activeOwnerIdx;
extern bool g_disableUpdate;
// extern std::uintptr_t kSeedOffset;
static constexpr char const* kDeathTrackerModID = "elohmrow.death_tracker";
static constexpr char const* kMoreThan100ModID = "caebae22.more-than-100";
