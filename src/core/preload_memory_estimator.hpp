#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

#include "types.hpp"
#include "apx_io.hpp"

namespace preload_memory {

struct PreloadMemoryEstimate {
    size_t attemptBytes = 0;
    size_t playerObjectBytes = 0;

    size_t totalBytes() const {
        return attemptBytes + playerObjectBytes;
    }

    double attemptMiB() const {
        return static_cast<double>(attemptBytes) / (1024.0 * 1024.0);
    }

    double playerObjectMiB() const {
        return static_cast<double>(playerObjectBytes) / (1024.0 * 1024.0);
    }

    double totalMiB() const {
        return static_cast<double>(totalBytes()) / (1024.0 * 1024.0);
    }
};

inline size_t loadedAttemptBytes(const Attempt& a) {
    size_t bytes = sizeof(Attempt);

    bytes += a.p1.capacity() * sizeof(Frame);
    bytes += a.p2.capacity() * sizeof(Frame);

    bytes += a.acc1.idx.capacity() * sizeof(uint32_t);
    bytes += a.acc2.idx.capacity() * sizeof(uint32_t);

    bytes += a.acc1Time.range.capacity() * sizeof(FrameAccelTime::BinRange);
    bytes += a.acc1Time.idx.capacity() * sizeof(uint32_t);
    bytes += a.acc1Time.timesQ.capacity() * sizeof(uint32_t);

    bytes += a.acc2Time.range.capacity() * sizeof(FrameAccelTime::BinRange);
    bytes += a.acc2Time.idx.capacity() * sizeof(uint32_t);
    bytes += a.acc2Time.timesQ.capacity() * sizeof(uint32_t);

    return bytes;
}

inline size_t estimatedAccelTimeBytes(
    uint32_t frameCount,
    double durationSec,
    uint32_t binWQ = 600u
) {
    if (frameCount == 0 || durationSec <= 0.0) {
        return 0;
    }

    uint64_t spanQ = static_cast<uint64_t>(std::llround(durationSec * kAPXTimeScaleRuntime_));
    if (spanQ == 0) {
        return 0;
    }

    uint64_t bins = ((spanQ + binWQ - 1) / binWQ) + 1;
    if (bins > 100000ull) {
        bins = 100000ull;
    }

    size_t bytes = 0;
    bytes += static_cast<size_t>(bins) * sizeof(FrameAccelTime::BinRange);
    bytes += static_cast<size_t>(bins) * sizeof(uint32_t);
    bytes += static_cast<size_t>(frameCount) * sizeof(uint32_t);

    return bytes;
}

inline size_t estimatedAttemptBytesFromCatalog(const APXAttemptDiskInfo& e) {
    size_t bytes = sizeof(Attempt);

    bytes += static_cast<size_t>(e.p1Count) * sizeof(Frame);
    bytes += static_cast<size_t>(e.p2Count) * sizeof(Frame);

    bytes += estimatedAccelTimeBytes(e.p1Count, e.endTime);
    bytes += estimatedAccelTimeBytes(e.p2Count, e.endTime);

    return bytes;
}

inline int estimatedPlayerObjectCountForAttempt(bool hadDual) {
    return hadDual ? 2 : 1;
}

inline PreloadMemoryEstimate estimateForSerialOrder(
    const std::vector<uint32_t>& serialOrder,
    int attemptCount,
    int realPlayerObjectAttempts,
    size_t bytesPerPlayerObject,
    const std::vector<Attempt>& loadedAttempts,
    const std::unordered_map<int, size_t>& loadedBySerial,
    const std::vector<APXAttemptDiskInfo>& catalog,
    const std::unordered_map<int, size_t>& catalogBySerial
) {
    PreloadMemoryEstimate out{};

    if (attemptCount <= 0 || serialOrder.empty()) {
        return out;
    }

    std::unordered_set<int> seen;
    seen.reserve(static_cast<size_t>(attemptCount) * 2);

    int countedAttempts = 0;

    for (uint32_t serialU : serialOrder) {
        if (countedAttempts >= attemptCount) break;

        const int serial = static_cast<int>(serialU);
        if (!seen.insert(serial).second) continue;

        bool hadDual = false;
        bool found = false;

        auto itLoaded = loadedBySerial.find(serial);
        if (itLoaded != loadedBySerial.end() && itLoaded->second < loadedAttempts.size()) {
            const Attempt& a = loadedAttempts[itLoaded->second];
            out.attemptBytes += loadedAttemptBytes(a);
            hadDual = a.hadDual;
            found = true;
        } else {
            auto itCatalog = catalogBySerial.find(serial);
            if (itCatalog != catalogBySerial.end() && itCatalog->second < catalog.size()) {
                const APXAttemptDiskInfo& e = catalog[itCatalog->second];
                out.attemptBytes += estimatedAttemptBytesFromCatalog(e);
                hadDual = e.hadDual;
                found = true;
            }
        }

        if (!found) {
            continue;
        }

        if (countedAttempts < realPlayerObjectAttempts) {
            out.playerObjectBytes +=
                static_cast<size_t>(estimatedPlayerObjectCountForAttempt(hadDual)) *
                bytesPerPlayerObject;
        }

        ++countedAttempts;
    }

    return out;
}

}