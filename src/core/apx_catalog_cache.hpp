#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

struct APXCatalogFileIdentity {
    uint64_t size = 0;
    int64_t writeTime = 0;
    uint64_t sampledHash = 0;

    bool operator==(APXCatalogFileIdentity const&) const = default;
};

bool getAPXCatalogFileIdentity(
    std::filesystem::path const& apxPath,
    APXCatalogFileIdentity& out
);

bool loadAPXCatalogCache(
    std::filesystem::path const& apxPath,
    std::vector<uint8_t>& payload
);

bool saveAPXCatalogCache(
    std::filesystem::path const& apxPath,
    std::vector<uint8_t> const& payload,
    APXCatalogFileIdentity const& expectedIdentity
);
