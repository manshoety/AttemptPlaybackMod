#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace attemptplayback {
    struct PreloadSortKey {
        uint32_t serial = 0;
        float endX = 0.f;
        double endT = 0.0;
    };

    template <class Serials, class Fetch>
    std::vector<PreloadSortKey> makePreloadSortKeys(
        Serials const& serials, Fetch&& fetch
    ) {
        std::vector<PreloadSortKey> keys;
        keys.reserve(serials.size());
        for (auto serial : serials) {
            keys.push_back(fetch(static_cast<uint32_t>(serial)));
        }
        return keys;
    }
}

namespace attemptplayback {
    struct DeathFirstKey {
        uint32_t serial = 0;
        double endT = 0.0;
        bool completed = false;
        bool hasEndT = false;
    };

    inline bool deathFirstLess(DeathFirstKey const& lhs, DeathFirstKey const& rhs) {
        // Completed attempts never died, allocate their slots last
        if (lhs.completed != rhs.completed) return !lhs.completed;
        const bool lhsValid = lhs.hasEndT && std::isfinite(lhs.endT) && lhs.endT >= 0.0;
        const bool rhsValid = rhs.hasEndT && std::isfinite(rhs.endT) && rhs.endT >= 0.0;
        if (lhsValid != rhsValid) return lhsValid;
        if (lhsValid && lhs.endT != rhs.endT) return lhs.endT < rhs.endT;
        return lhs.serial < rhs.serial;
    }

    template <class Fetch>
    std::vector<uint32_t> deathFirstSerialOrder(
        std::vector<uint32_t> const& selected, Fetch&& fetch) {
        std::vector<DeathFirstKey> keys;
        keys.reserve(selected.size());
        for (auto serial : selected) keys.push_back(fetch(serial));
        std::sort(keys.begin(), keys.end(), deathFirstLess);
        std::vector<uint32_t> result;
        result.reserve(keys.size());
        for (auto const& key : keys) result.push_back(key.serial);
        return result;
    }
}
