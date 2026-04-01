// apx_io.hpp
#pragma once
#include <vector>
#include <iosfwd>
#include "types.hpp"
#include "apx_format.hpp"
#include <Geode/binding/PlayerObject.hpp>

bool writeAPXPracticePath(std::ostream& os, const PracticePath& path);
bool readAPXPracticePath(std::istream& in, uint32_t chunkSize, PracticePath& path, bool* outUsedLegacy = nullptr);
bool writeAPXAttemptCompact(std::ostream& out, Attempt const& attempt);
bool readAPXAttemptCompact(std::istream& in, uint32_t chunkSize, Attempt& attempt, bool* outUsedLegacy = nullptr);

bool loadAPXFileWithMigration(std::filesystem::path const& path, std::vector<Attempt>& attemptsOut, PracticePath& practicePathOut);
bool saveAPXFileCurrent(std::filesystem::path const& path, std::vector<Attempt> const& attempts, PracticePath const& practicePath);

// Core flag encoding - works with any uint8_t flags field
static inline uint8_t flagsFromFrame_(Frame const& f) {
    uint8_t b = 0;
    b |= (f.upsideDown     ? 1<<0 : 0);
    b |= (f.hold           ? 1<<1 : 0);
    b |= (f.holdL          ? 1<<2 : 0);
    b |= (f.holdR          ? 1<<3 : 0);
    b |= (f.isDashing      ? 1<<4 : 0);
    b |= (f.stateDartSlide ? 1<<5 : 0);
    b |= (f.isVisible      ? 1<<6 : 0);
    return b;
}

inline void flagsFromFrame_(Frame const& f, APXFrame& s) {
    s.flags = flagsFromFrame_(f);
}

inline void frameFromFlags_(uint8_t flags, Frame& d) {
    d.upsideDown     = (flags & (1 << 0)) != 0;
    d.hold           = (flags & (1 << 1)) != 0;
    d.holdL          = (flags & (1 << 2)) != 0;
    d.holdR          = (flags & (1 << 3)) != 0;
    d.isDashing      = (flags & (1 << 4)) != 0;
    d.stateDartSlide = (flags & (1 << 5)) != 0;
    d.isVisible      = (flags & (1 << 6)) != 0;
}

inline void frameFromFlags_(APXFrame const& s, Frame& d) {
    frameFromFlags_(s.flags, d);
}

