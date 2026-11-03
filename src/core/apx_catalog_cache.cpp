#include "apx_catalog_cache.hpp"
#include <array>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <system_error>

namespace {
    constexpr uint32_t kCacheVersion = 1;
    constexpr uint32_t kMaxPayload = 128u * 1024u * 1024u;
    constexpr size_t kSampleBytes = 4096;
    constexpr uint64_t kHashOffset = 14695981039346656037ull;
    constexpr uint64_t kHashPrime = 1099511628211ull;

    #pragma pack(push, 1)
    struct CacheHeader {
        char magic[4];
        uint32_t version;
        uint64_t fileSize;
        int64_t writeTime;
        uint64_t sampledHash;
        uint64_t payloadHash;
        uint32_t payloadSize;
    };
    #pragma pack(pop)

    using FileIdentity = APXCatalogFileIdentity;

    uint64_t hashBytes_(uint64_t hash, const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            hash = (hash ^ data[i]) * kHashPrime;
        }
        return hash;
    }

    uint64_t payloadHash_(std::vector<uint8_t> const& payload) {
        return hashBytes_(kHashOffset, payload.data(), payload.size());
    }

    bool getFileIdentity_(std::filesystem::path const& path, FileIdentity& out) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec) return false;

        ec.clear();
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size < 8) return false;

        ec.clear();
        const auto writeTime = std::filesystem::last_write_time(path, ec);
        if (ec) return false;

        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        const uint64_t offsets[] = {
            0,
            static_cast<uint64_t>(size / 2),
            static_cast<uint64_t>(size > kSampleBytes ? size - kSampleBytes : 0)
        };
        std::array<uint8_t, kSampleBytes> sample{};
        uint64_t hash = kHashOffset;
        for (uint64_t offset : offsets) {
            in.clear();
            in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            if (!in) return false;
            const auto count = static_cast<std::streamsize>(std::min<uint64_t>(kSampleBytes, size - offset));
            in.read(reinterpret_cast<char*>(sample.data()), count);
            if (!in) return false;
            hash = hashBytes_(hash, sample.data(), static_cast<size_t>(count));
        }

        out.size = static_cast<uint64_t>(size);
        out.writeTime = static_cast<int64_t>(writeTime.time_since_epoch().count());
        out.sampledHash = hash;
        return true;
    }

    std::filesystem::path cachePath_(std::filesystem::path const& apxPath) {
        auto path = apxPath;
        path += ".idx";
        return path;
    }
}

bool getAPXCatalogFileIdentity(
    std::filesystem::path const& apxPath,
    APXCatalogFileIdentity& out
) {
    return getFileIdentity_(apxPath, out);
}

bool loadAPXCatalogCache(std::filesystem::path const& apxPath, std::vector<uint8_t>& payload) {
    payload.clear();

    FileIdentity identity{};
    if (!getFileIdentity_(apxPath, identity)) return false;

    auto path = cachePath_(apxPath);
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;

    const auto fileSize = in.tellg();
    if (fileSize < static_cast<std::streamoff>(sizeof(CacheHeader)) ||
        fileSize > static_cast<std::streamoff>(sizeof(CacheHeader) + kMaxPayload)) return false;

    in.seekg(0);
    CacheHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in || std::memcmp(hdr.magic, "APCI", 4) != 0 || hdr.version != kCacheVersion ||
        hdr.fileSize != identity.size || hdr.writeTime != identity.writeTime ||
        hdr.sampledHash != identity.sampledHash || hdr.payloadSize > kMaxPayload ||
        fileSize != static_cast<std::streamoff>(sizeof(hdr) + hdr.payloadSize)) return false;

    std::vector<uint8_t> readPayload(hdr.payloadSize);
    if (hdr.payloadSize) {
        in.read(reinterpret_cast<char*>(readPayload.data()), hdr.payloadSize);
        if (!in) return false;
    }
    if (payloadHash_(readPayload) != hdr.payloadHash) return false;

    FileIdentity after{};
    if (!getFileIdentity_(apxPath, after) || after.size != identity.size ||
        after.writeTime != identity.writeTime || after.sampledHash != identity.sampledHash) return false;

    payload = std::move(readPayload);
    return true;
}

bool saveAPXCatalogCache(
    std::filesystem::path const& apxPath,
    std::vector<uint8_t> const& payload,
    APXCatalogFileIdentity const& expectedIdentity
) {
    if (payload.size() > kMaxPayload) return false;

    FileIdentity identity{};
    if (!getFileIdentity_(apxPath, identity) || !(identity == expectedIdentity)) return false;

    CacheHeader hdr{};
    std::memcpy(hdr.magic, "APCI", 4);
    hdr.version = kCacheVersion;
    hdr.fileSize = identity.size;
    hdr.writeTime = identity.writeTime;
    hdr.sampledHash = identity.sampledHash;
    hdr.payloadHash = payloadHash_(payload);
    hdr.payloadSize = static_cast<uint32_t>(payload.size());

    auto path = cachePath_(apxPath);
    auto tmp = path;
    tmp += ".tmp";
    std::error_code tempEc;
    const auto tempStatus = std::filesystem::symlink_status(tmp, tempEc);
    if (tempStatus.type() == std::filesystem::file_type::symlink ||
        (tempEc && tempEc != std::errc::no_such_file_or_directory)) return false;
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<char const*>(&hdr), sizeof(hdr));
        if (!payload.empty()) out.write(reinterpret_cast<char const*>(payload.data()), payload.size());
        out.flush();
        if (!out.good()) return false;
    }

    FileIdentity after{};
    if (!getFileIdentity_(apxPath, after) || !(after == expectedIdentity)) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        // rebuildable index
        ec.clear();
        std::filesystem::remove(path, ec);
        if (ec) return false;
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
    }
    return !ec;
}
