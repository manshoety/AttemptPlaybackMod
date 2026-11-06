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