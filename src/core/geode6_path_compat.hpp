#pragma once

// Geode V6 doo doo stuff
#include <asp/fs/std.hpp>
#include <Geode/Geode.hpp>
#include <filesystem>
#include <string>
#include <string_view>

namespace attemptplayback::pathcompat {
inline std::filesystem::path toStd(asp::fs::Path const& path) {
    // v6 toStdPath returns Result<path>
    // Encoding failure must never lead to writing a relative file: callers check for empty paths
    return path.toStdPath().unwrapOrDefault();
}

inline std::filesystem::path toStdOrLog(
    asp::fs::Path const& path,
    std::string_view context,
    std::string_view directoryKind
) {
    auto native = toStd(path);
    if (native.empty()) {
        geode::log::error(
            "{} Geode {} directory could not be converted to a native path",
            context, directoryKind
        );
    }
    return native;
}

inline asp::fs::Path toGeode(std::filesystem::path const& path) {
    return asp::fs::Path::fromStdPath(path);
}

inline std::string toUtf8(std::filesystem::path const& path) {
    return toGeode(path).string();
}
} 