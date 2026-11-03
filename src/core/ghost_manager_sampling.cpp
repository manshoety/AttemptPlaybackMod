#include "ghost_manager.hpp"

void Ghosts::buildAccelTime(const std::vector<Frame>& v,
                           FrameAccelTime& a,
                           uint32_t binWQ,
                           bool buildTimes) {
        a.reset();
        const size_t n = v.size();
        if (n == 0) return;

        const uint32_t minTQ = v.front().t.q;
        const uint32_t maxTQ = v.back().t.q;

        if (maxTQ < minTQ) {
            log::warn("[buildAccelTime] Invalid time data: minTQ={}, maxTQ={}", minTQ, maxTQ);
            return;
        }
        if (maxTQ == minTQ) {
            return;
        }

        a.baseTQ = minTQ;
        a.binWQ = std::max<uint32_t>(1u, binWQ);

        const uint64_t spanQ = static_cast<uint64_t>(maxTQ) - static_cast<uint64_t>(minTQ);
        int bins = static_cast<int>((spanQ + a.binWQ - 1) / a.binWQ) + 1;

        if (bins <= 0 || bins > 10000000) {
            log::warn("[buildAccelTime] Invalid bin count: {}, minTQ={}, maxTQ={}, binWQ={}",
                    bins, minTQ, maxTQ, a.binWQ);
            a.reset();
            return;
        }

        if (bins > 100000) {
            a.binWQ = std::max<uint32_t>(1u, static_cast<uint32_t>((spanQ + 99998ull) / 99999ull));
            bins = 100000;
        }

        a.bins = bins;
        a.range.resize(static_cast<size_t>(bins));
        a.idx.resize(static_cast<size_t>(bins));

        if (buildTimes) {
            a.timesQ.resize(n);
            for (size_t i = 0; i < n; ++i) a.timesQ[i] = v[i].t.q;
        } else {
            a.timesQ.clear();
        }

        const uint32_t* tarr = buildTimes ? a.timesQ.data() : nullptr;
        auto getTQ = [&](size_t i) -> uint32_t {
            return tarr ? tarr[i] : v[i].t.q;
        };

        size_t iHi = 0;
        uint32_t binEndQ = minTQ + a.binWQ;

        for (int b = 0; b < bins; ++b) {
            const size_t iLo = iHi;

            while (iHi + 1 < n && getTQ(iHi + 1) < binEndQ) ++iHi;

            a.range[static_cast<size_t>(b)] = FrameAccelTime::BinRange{
                static_cast<uint32_t>(iLo),
                static_cast<uint32_t>(iHi)
            };
            a.idx[static_cast<size_t>(b)] = static_cast<uint32_t>(iLo);

            if (UINT32_MAX - binEndQ < a.binWQ) {
                binEndQ = UINT32_MAX;
            } else {
                binEndQ += a.binWQ;
            }
        }
    }

size_t Ghosts::idxForTime(const FrameAccelTime& acc, const std::vector<Frame>& v, double t) {
        if (v.empty()) return 0;

        const uint32_t wantTQ = runtimeQuantTimeQ_(t);
        size_t idx = 0;

        if (acc.valid()) {
            idx = acc.idx[static_cast<size_t>(acc.binOfQ(wantTQ))];
            if (idx >= v.size()) idx = v.size() - 1;

            while (idx + 1 < v.size() && v[idx + 1].t.q <= wantTQ) ++idx;
            while (idx > 0 && v[idx].t.q > wantTQ) --idx;
        } else {
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                const size_t mid = (lo + hi) >> 1;
                if (v[mid].t.q < wantTQ) lo = mid + 1;
                else hi = mid;
            }
            idx = lo ? lo - 1 : 0;
        }

        return idx;
    }

size_t Ghosts::ptrToIndex_(const Attempt* p) const {
        if (!p) return SIZE_MAX;
        if (p == &m_current) return SIZE_MAX;
        for (size_t j = 0; j < attempts.size(); ++j) {
            if (&attempts[j] == p) return j;
        }
        return SIZE_MAX;
    }

