// sfx_handling.hpp
#pragma once

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <chrono>
#include <Geode/Geode.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>


static std::filesystem::path explodeSfxDir_() {
    auto dir = Mod::get()->getSaveDir() / "ghost-explode-sfx";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        log::warn("[GhostSFX] create_directories failed: {} ({})",
            utils::string::pathToString(dir), ec.message());
    }
    return dir;
}

static bool isAudioFile_(std::filesystem::path const& p) {
    if (!p.has_extension()) return false;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return ext == ".ogg" || ext == ".mp3" || ext == ".wav";
}

static void refreshCustomExplodeSfx_(
    std::vector<std::string>& out,
    std::filesystem::file_time_type& lastDirWrite
) {
    auto dir = explodeSfxDir_();

    std::error_code ec;
    auto t = std::filesystem::last_write_time(dir, ec);
    if (!ec && !out.empty() && t == lastDirWrite) {
        return; // no changes since last scan
    }
    if (!ec) lastDirWrite = t;

    out.clear();

    for (auto const& ent : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!ent.is_regular_file()) continue;

        auto const& p = ent.path();
        if (!isAudioFile_(p)) continue;

        out.push_back(utils::string::pathToString(p)); // absolute path string
    }

    // Stable ordering (nice for debugging)
    std::sort(out.begin(), out.end());
}

static std::string lowerASCII_(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return s;
}

// Find a resource file anywhere under the mod's runtime resources directory by *filename*.
static std::optional<std::filesystem::path> findModResourceByFilename_(
    std::filesystem::path const& resDir,
    std::string const& filename
) {
    std::error_code ec;
    const std::string want = lowerASCII_(filename);

    for (auto it = std::filesystem::recursive_directory_iterator(resDir, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         ++it)
    {
        const auto& ent = *it;
        if (!ent.is_regular_file(ec)) continue;

        auto p = ent.path();
        if (lowerASCII_(p.filename().string()) == want) {
            return p;
        }
    }
    return std::nullopt;
}

struct ExplodeSfxSyncResult {
    int copied = 0;
    int skipped = 0;
    int overwritten = 0;
    int missing = 0;
};

static ExplodeSfxSyncResult syncBundledExplodeSfxIntoCustomDir_(
    bool overwriteExisting = false,
    bool overwriteIfZeroBytes = true
) {
    ExplodeSfxSyncResult r{};

    auto* mod = Mod::get();
    if (!mod) return r;

    const auto dstDir = explodeSfxDir_();
    const auto resDir = mod->getResourcesDir();

    static constexpr const char* kSeedFiles[] = {
        "explode_vine.ogg",
        "explode_vineShort.ogg",
        "explode_11.ogg",
        "explode_baba.ogg",
        "explode_bruh.ogg",
        "explode_ahh.ogg",
        "explode_sans.ogg",
        "explode_taco.ogg",
        "explode_what.ogg",
    };

    for (auto* name : kSeedFiles) {
        std::filesystem::path dst = dstDir / name;

        // Decide what copy mode to use
        std::filesystem::copy_options mode = std::filesystem::copy_options::skip_existing;

        std::error_code ec;

        const bool exists = std::filesystem::exists(dst, ec) && !ec;
        bool shouldOverwrite = overwriteExisting;

        if (!shouldOverwrite && exists && overwriteIfZeroBytes) {
            ec.clear();
            auto sz = std::filesystem::file_size(dst, ec);
            if (!ec && sz == 0) {
                shouldOverwrite = true;
            }
        }

        if (exists && !shouldOverwrite) {
            r.skipped++;
            continue;
        }

        if (shouldOverwrite) {
            mode = std::filesystem::copy_options::overwrite_existing;
        }

        // Find source (direct or recursive)
        std::filesystem::path src = resDir / name;
        ec.clear();
        if (!(std::filesystem::exists(src, ec) && !ec)) {
            auto found = findModResourceByFilename_(resDir, name);
            if (!found) {
                r.missing++;
                log::warn("[GhostSFX] Bundled seed file missing in resources: {}", name);
                continue;
            }
            src = *found;
        }

        ec.clear();
        std::filesystem::copy_file(src, dst, mode, ec);
        if (ec) {
            log::warn("[GhostSFX] copy_file failed '{}' -> '{}': {}",
                utils::string::pathToString(src),
                utils::string::pathToString(dst),
                ec.message()
            );
            // treat as "missing"/failed copy, but keep counters stable
            continue;
        }

        if (shouldOverwrite && exists) r.overwritten++;
        else r.copied++;
    }

    return r;
}

// Don't use currently but this places custom sounds once on install
static void seedDefaultCustomExplodeSfxOnce_() {
    auto* mod = Mod::get();
    if (!mod) return;

    static constexpr const char* kSeedKey = "seeded-custom-explode-sfx-v1";

    // Already seeded?
    if (mod->hasSavedValue(kSeedKey) && mod->getSavedValue<bool>(kSeedKey)) {
        return;
    }

    // If user already has any custom audio files, don't seed (avoid adding stuff on updates)
    std::vector<std::string> existing;
    std::filesystem::file_time_type dummy{};
    refreshCustomExplodeSfx_(existing, dummy);

    if (!existing.empty()) {
        mod->setSavedValue(kSeedKey, true);
        return;
    }

    // Seed (idempotent copy-missing behavior)
    auto res = syncBundledExplodeSfxIntoCustomDir_(
        /*overwriteExisting=*/false,
        /*overwriteIfZeroBytes=*/true
    );

    mod->setSavedValue(kSeedKey, true);
}


struct ActiveSfx {
    int channelID = 0;
};

class SfxLimiter {
public:
    explicit SfxLimiter(int maxVoices = 20)
        : m_maxVoices(std::max(1, maxVoices)) {}

    void setMaxVoices(int n) { m_maxVoices = std::max(1, n); }

    void play(const std::filesystem::path& path, float volume = 1.f, float pitch = 1.f) {
        auto* eng = FMODAudioEngine::sharedEngine();
        if (!eng) return;

        pruneDead_(eng);

        const auto s = utils::string::pathToString(path);

        // No cache: always preload right before play
        eng->preloadEffect(s);

        while ((int)m_live.size() >= m_maxVoices) {
            eng->stopChannel(m_live.front().channelID);
            m_live.pop_front();
        }

        const int channelID = eng->getNextChannelID();
        const int effectID  = nextEffectID_();

        eng->playEffectAdvanced(
            s,
            /*speed*/ 1.f,
            /*unknown*/ 0.f,
            /*volume*/ volume,
            /*pitch*/ pitch,
            /*fft*/ false,
            /*reverb*/ false,
            /*startMillis*/ 0,
            /*endMillis*/ 0,
            /*fadeIn*/ 0,
            /*fadeOut*/ 0,
            /*loopEnabled*/ false,
            /*effectID*/ effectID,
            /*override*/ false,
            /*noPreload*/ false,
            /*channelID*/ channelID,
            /*uniqueID*/ 0,
            /*minInterval*/ 0.f,
            /*sfxGroup*/ 0
        );

        m_live.push_back({ channelID });
    }

private:
    void pruneDead_(FMODAudioEngine* eng) {
        for (auto it = m_live.begin(); it != m_live.end(); ) {
            FMOD::Channel* ch = eng->channelForChannelID(it->channelID);
            bool playing = false;

            if (ch) {
                if (ch->isPlaying(&playing) == FMOD_OK && playing) {
                    ++it;
                    continue;
                }
            }
            it = m_live.erase(it);
        }
    }

    int nextEffectID_() {
        return ++m_effectIDCounter;
    }

private:
    std::deque<ActiveSfx> m_live;
    int m_maxVoices = 20;
    int m_effectIDCounter = 600000;
};
