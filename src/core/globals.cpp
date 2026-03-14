// globals.cpp
#include "globals.hpp"
#include <limits>

bool g_isNextFrame = true;
bool g_isCanUpdatePlayerAgain = true;
std::size_t activeOwnerIdx = std::numeric_limits<std::size_t>::max();
bool g_disableUpdate = false;
// std::uintptr_t kSeedOffset = 0x6a4e20; // In the future I'll try to get seeds working