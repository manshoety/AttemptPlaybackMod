#include "ghost_manager.hpp"

void Ghosts::replaceAllGhostTextToken_(std::string& value, std::string const& token, std::string const& replacement) {
        size_t pos = 0;
        while ((pos = value.find(token, pos)) != std::string::npos) {
            value.replace(pos, token.size(), replacement);
            pos += replacement.size();
        }
    }

std::string Ghosts::formatGhostPercentage_(size_t count, size_t total, int precision) {
        precision = std::clamp(precision, 0, 6);
        if (total == 0) return precision == 0 ? "0" : fmt::format("{:.{}f}", 0.0, precision);

        count = std::min(count, total);

        long long scale = 1;
        for (int i = 0; i < precision; ++i) scale *= 10;

        const double percent = 100.0 * static_cast<double>(count) / static_cast<double>(total);
        long long units = std::llround(percent * static_cast<double>(scale));
        const long long fullUnits = 100LL * scale;

        if (count < total && units >= fullUnits) units = fullUnits - 1;
        if (count == total) units = fullUnits;

        return fmt::format("{:.{}f}", static_cast<double>(units) / static_cast<double>(scale), precision);
    }

void Ghosts::replaceGhostPercentageTokens_(std::string& value, std::string const& name, size_t count, size_t total) {
        replaceAllGhostTextToken_(
            value,
            "{" + name + "}",
            formatGhostPercentage_(count, total, 0)
        );

        const std::string prefix = "{" + name + ":";
        size_t pos = 0;
        while ((pos = value.find(prefix, pos)) != std::string::npos) {
            const size_t end = value.find('}', pos + prefix.size());
            if (end == std::string::npos) break;

            const std::string spec = value.substr(pos + prefix.size(), end - (pos + prefix.size()));
            bool valid = !spec.empty();
            int precision = 0;
            for (char c : spec) {
                if (c < '0' || c > '9') {
                    valid = false;
                    break;
                }
                precision = std::min(6, precision * 10 + (c - '0'));
            }

            if (!valid) {
                pos = end + 1;
                continue;
            }

            const std::string replacement = formatGhostPercentage_(count, total, precision);
            value.replace(pos, end - pos + 1, replacement);
            pos += replacement.size();
        }
    }

std::string Ghosts::formatGhostTextForCounts_(
        std::string out,
        size_t alive,
        size_t dead,
        size_t total) {
        replaceAllGhostTextToken_(out, "{alive}", std::to_string(alive));
        replaceAllGhostTextToken_(out, "{Alive}", std::to_string(alive));
        replaceAllGhostTextToken_(out, "{dead}", std::to_string(dead));
        replaceAllGhostTextToken_(out, "{Dead}", std::to_string(dead));
        replaceAllGhostTextToken_(out, "{total}", std::to_string(total));
        replaceAllGhostTextToken_(out, "{Total}", std::to_string(total));
        replaceGhostPercentageTokens_(out, "percent_alive", alive, total);
        replaceGhostPercentageTokens_(out, "percent_dead", dead, total);
        return out;
    }

std::string Ghosts::formatCustomGhostText_() const {
        return formatGhostTextForCounts_(
            m_ghostTextCustomFormat.empty()
                ? std::string("{alive}/{total} attempts remain")
                : m_ghostTextCustomFormat,
            m_numAliveGhosts,
            m_numDeadGhosts,
            m_numTotalGhosts
        );
    }

void Ghosts::rebuildGhostTextString_() {
        switch (m_ghostTextMode) {
            case GhostTextPreset::DeadAttempts:
                m_playLayerGhostText = fmt::format("Dead Attempts: {}", m_numDeadGhosts);
                break;
            case GhostTextPreset::AliveAttempts:
                m_playLayerGhostText = fmt::format("Alive Attempts: {}", m_numAliveGhosts);
                break;
            case GhostTextPreset::Custom:
                m_playLayerGhostText = formatCustomGhostText_();
                break;
        }
        m_lastAppliedPlayLayerGhostText.clear();
    }

void Ghosts::syncGhostUiSettingsFromSaved_() {
        auto* mod = Mod::get();
        if (!mod) return;

        m_showPlayLayerGhostText = mod->hasSavedValue("ghost-text-enabled")
            ? mod->getSavedValue<bool>("ghost-text-enabled") : false;

        int mode = mod->hasSavedValue("ghost-text-mode")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-text-mode")) : 0;
        mode = std::clamp(mode, 0, 2);
        m_ghostTextMode = static_cast<GhostTextPreset>(mode);

        m_ghostTextCustomFormat = mod->hasSavedValue("ghost-text-custom")
            ? mod->getSavedValue<std::string>("ghost-text-custom")
            : std::string("{alive}/{total} attempts remain");
        if (m_ghostTextCustomFormat.empty()) m_ghostTextCustomFormat = "{alive}/{total} attempts remain";

        const int x = mod->hasSavedValue("ghost-text-x")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-text-x")) : 20;
        const int y = mod->hasSavedValue("ghost-text-y")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-text-y")) : 280;
        m_playLayerGhostTextPos.x = static_cast<float>(x);
        m_playLayerGhostTextPos.y = static_cast<float>(y);

        const int scalePercent = mod->hasSavedValue("ghost-text-scale-percent")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-text-scale-percent")) : 45;
        m_playLayerGhostTextScale = std::clamp(scalePercent / 100.f, 0.15f, 1.25f);

        const int textOpacity = mod->hasSavedValue("ghost-text-opacity")
            ? static_cast<int>(mod->getSavedValue<int64_t>("ghost-text-opacity")) : 180;
        m_playLayerGhostTextOpacity = static_cast<uint8_t>(std::clamp(textOpacity, 0, 255));

        m_ghostTextFlashOnDeath = mod->hasSavedValue("ghost-text-flash-on-death")
            ? mod->getSavedValue<bool>("ghost-text-flash-on-death") : true;

        m_showDeathMarkers = mod->hasSavedValue("death-markers-enabled")
            ? mod->getSavedValue<bool>("death-markers-enabled") : false;
        const int markerSize = mod->hasSavedValue("death-marker-size")
            ? static_cast<int>(mod->getSavedValue<int64_t>("death-marker-size")) : 25;
        m_deathMarkerSize = static_cast<float>(std::clamp(markerSize, 4, 40));
        if (mod->hasSavedValue("death-marker-thickness-tenths")) {
            const int thicknessTenths = static_cast<int>(
                mod->getSavedValue<int64_t>("death-marker-thickness-tenths")
            );
            m_deathMarkerThickness = std::clamp(thicknessTenths / 10.f, 1.f, 8.f);
        } else {
            m_deathMarkerThickness = 5.f;
        }

        const int markerOpacity = mod->hasSavedValue("death-marker-opacity")
            ? static_cast<int>(mod->getSavedValue<int64_t>("death-marker-opacity")) : 255;
        m_deathMarkerOpacity = static_cast<uint8_t>(std::clamp(markerOpacity, 0, 255));

        rebuildGhostTextString_();
    }

void Ghosts::setGhostTextCustomFormat(std::string text) {
        if (text.empty()) text = "{alive}/{total} attempts remain";
        m_ghostTextCustomFormat = std::move(text);
        Mod::get()->setSavedValue("ghost-text-custom", m_ghostTextCustomFormat);
        rebuildGhostTextString_();
        updatePlayLayerGhostTextLabel_();
     }

void Ghosts::updatePlayLayerGhostTextLabel_() {
        auto* label = m_playLayerGhostTextLabel.data();
        if (!label) return;

        const bool replayVisible = botActive || playback || showWhilePlaying;
        const bool show = isModEnabled() && (
            m_ghostTextPositionPreview ||
            (m_showPlayLayerGhostText && replayVisible)
        );

        if (show != m_lastAppliedPlayLayerGhostTextVisible) {
            label->setVisible(show);
            m_lastAppliedPlayLayerGhostTextVisible = show;
        }

        if (!show) return;

        if (m_playLayerGhostText != m_lastAppliedPlayLayerGhostText) {
            label->setString(m_playLayerGhostText.c_str());
            m_lastAppliedPlayLayerGhostText = m_playLayerGhostText;
        }

        label->setPosition(m_playLayerGhostTextPos);
        label->setScale(m_playLayerGhostTextScale);
        label->setOpacity(m_playLayerGhostTextOpacity);
    }

void Ghosts::forceHidePlayLayerGhostTextLabelOnly() {
        if (m_playLayerGhostTextLabel) {
            m_playLayerGhostTextLabel->setVisible(false);
        }

        m_lastAppliedPlayLayerGhostTextVisible = false;
    }

void Ghosts::ensureDeathMarkerLayer_() {
        if (m_deathMarkerDrawNode || !m_ghostRoot) return;
        auto* worldParent = m_ghostRoot->getParent();
        if (!worldParent) return;
        m_deathMarkerDrawNode = cocos2d::CCDrawNode::create();
        if (!m_deathMarkerDrawNode) return;
        worldParent->addChild(m_deathMarkerDrawNode, 9999);
    }

void Ghosts::drawDeathMarkerAt_(cocos2d::CCPoint const& p) {
        if (!m_deathMarkerDrawNode || !m_showDeathMarkers) return;

        const float half = std::max(2.f, m_deathMarkerSize * 0.5f);
        const float radius = std::max(
            0.25f,
            m_deathMarkerThickness * 0.5f
        );

        const float alpha =
            static_cast<float>(m_deathMarkerOpacity) / 255.f;
        if (alpha <= 0.f) return;

        const auto color = cocos2d::ccc4f(
            alpha,
            0.08f * alpha,
            0.08f * alpha,
            alpha
        );

        m_deathMarkerDrawNode->drawSegment(
            { p.x - half, p.y - half },
            { p.x + half, p.y + half },
            radius,
            color
        );

        m_deathMarkerDrawNode->drawSegment(
            { p.x - half, p.y + half },
            { p.x + half, p.y - half },
            radius,
            color
        );
    }

bool Ghosts::hasDistinctP2DeathMarker_(DeathMarkerPositions const& positions) {
        if (!positions.hasP2) return false;

        const float dx = positions.p2.x - positions.p1.x;
        const float dy = positions.p2.y - positions.p1.y;
        return dx * dx + dy * dy > 0.01f;
    }

void Ghosts::redrawDeathMarkers_() {
        if (!m_deathMarkerDrawNode) return;
        m_deathMarkerDrawNode->clear();
        if (!m_showDeathMarkers) return;
        for (auto const& [serial, positions] : m_deathMarkerPositions) {
            (void)serial;
            drawDeathMarkersForAttempt_(positions);
        }
    }

void Ghosts::recordDeathMarker_(Attempt const& a) {
        if (a.serial <= 0 || a.completed || a.p1.empty()) return;
        if (m_deathMarkerPositions.find(a.serial) != m_deathMarkerPositions.end()) return;
        DeathMarkerPositions positions;
        positions.p1.x = a.p1.back().x;
        positions.p1.y = a.p1.back().y;

        if (a.hadDual && !a.p2.empty()) {
            constexpr double kDualDeathTimeTolerance = 0.1;
            const double p1EndTime = static_cast<double>(a.p1.back().t);
            const double p2EndTime = static_cast<double>(a.p2.back().t);

            if (std::abs(p1EndTime - p2EndTime) <= kDualDeathTimeTolerance) {
                positions.p2.x = a.p2.back().x;
                positions.p2.y = a.p2.back().y;
                positions.hasP2 = true;
            }
        }

        auto const [it, inserted] = m_deathMarkerPositions.emplace(a.serial, positions);
        if (inserted) drawDeathMarkersForAttempt_(it->second);
    }

void Ghosts::setDeathMarkerThickness(float thickness) {
        m_deathMarkerThickness = std::clamp(thickness, 1.f, 8.f);
        Mod::get()->setSavedValue(
            "death-marker-thickness-tenths",
            static_cast<int64_t>(std::lround(m_deathMarkerThickness * 10.f))
        );
        redrawDeathMarkers_();
    }

bool Ghosts::countGhostTextDeathOnceBySerial_(int serial) {
        if (serial <= 0) return false;

        if (!m_ghostTextCountedDeadSerials.insert(serial).second) return false;

        updateGhostTextOnDeath(false);
        return true;
    }

void Ghosts::updateGhostTextOnDeath(bool reset) {
        if (reset) {
            m_ghostTextCountedDeadSerials.clear();
            m_numDeadGhosts = 0;
            m_numTotalGhosts = m_preloadLoaded > 0
                ? m_preloadLoaded
                : m_preloadedIndices.size();
            m_numAliveGhosts = m_numTotalGhosts;
            clearDeathMarkers_();
        }
        else {
            ++m_numDeadGhosts;
            if (m_numAliveGhosts > 0) --m_numAliveGhosts;
            if (m_ghostTextFlashOnDeath) flashPlayLayerGhostTextRed(0.2);
        }

        rebuildGhostTextString_();
        updatePlayLayerGhostTextLabel_();
    }

void Ghosts::updateDeadAttemptsText(bool reset) {
        if (reset) {
            m_numDeadGhosts = 0;
            m_numAliveGhosts = m_numTotalGhosts;
        }
        rebuildGhostTextString_();
    }

void Ghosts::flashPlayLayerGhostTextRed(double fadeSeconds) {
        if (!m_ghostTextFlashOnDeath) return;
        auto* label = m_playLayerGhostTextLabel.data();
        if (!label) return;

        label->stopActionByTag(m_ghostTextFlashActionTag);

        label->setColor({ 255, 0, 0 });

        auto* fadeToWhite = cocos2d::CCTintTo::create(
            static_cast<float>(std::max(0.0, fadeSeconds)), 255, 255, 255);

        fadeToWhite->setTag(m_ghostTextFlashActionTag);
        label->runAction(fadeToWhite);
    }

void Ghosts::refreshCustomDeathSfxNow(bool resetGhostObjects) {
        customLastWrite = {};
        refreshCustomExplodeSfx_(customSfx, customLastWrite);

        if (resetGhostObjects) {
            invalidatePrimedPlayerObjectRefs();
            InitialGamemodeSet();
        }
    }

void Ghosts::randomColorArrayFromString_(
        std::array<uint8_t, kRandomColorCount>& out,
        std::string const& s) {
        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            out[slot] = s[slot] == '1' ? 1 : 0;
        }
    }

std::string Ghosts::randomColorArrayToString_(
        std::array<uint8_t, kRandomColorCount> const& mask) {
        std::string s(kRandomColorCount, '0');
        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            s[slot] = mask[slot] ? '1' : '0';
        }
        return s;
    }

void Ghosts::ensureRandomColorMaskLoaded_() {
        if (m_randomColorMaskLoaded) return;
        m_randomColorMaskLoaded = true;

        auto* mod = Mod::get();

        const std::string fallback = kDefaultRandomColorMask;

        auto readValid = [mod](const char* key) -> std::string {
            if (!mod->hasSavedValue(key)) return {};
            auto value = mod->getSavedValue<std::string>(key);
            return validRandomColorMaskString_(value) ? value : std::string{};
        };
 
        const std::string legacy = readValid(kGhostRandomColorsMaskKey);
        std::string p1 = readValid(kGhostRandomColorsMaskP1Key);
        std::string p2 = readValid(kGhostRandomColorsMaskP2Key);

        // Tired of the old single color? Now introducing TWO COLORS!
        // With this revolutionaly new change you can have TWO COLORS at the SAME TIME!
        if (p1.empty()) p1 = !legacy.empty() ? legacy : (!p2.empty() ? p2 : fallback);
        if (p2.empty()) p2 = !legacy.empty() ? legacy : (!p1.empty() ? p1 : fallback);

        randomColorArrayFromString_(m_randomColorAllowedP1, p1);
        randomColorArrayFromString_(m_randomColorAllowedP2, p2);
        rebuildRandomColorAllowedLists_();

        saveRandomColorMasks_();
    }

void Ghosts::rebuildRandomColorAllowedLists_() {
        m_randomColorAllowedListP1.clear();
        m_randomColorAllowedListP2.clear();
        m_randomColorAllowedListP1.reserve(kRandomColorCount);
        m_randomColorAllowedListP2.reserve(kRandomColorCount);

        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            if (m_randomColorAllowedP1[slot]) m_randomColorAllowedListP1.push_back(kRandomColorIDs[slot]);
            if (m_randomColorAllowedP2[slot]) m_randomColorAllowedListP2.push_back(kRandomColorIDs[slot]);
        }
    }

void Ghosts::saveRandomColorMasks_() {
        const std::string p1 = randomColorArrayToString_(m_randomColorAllowedP1);
        const std::string p2 = randomColorArrayToString_(m_randomColorAllowedP2);
 
        std::string legacy(kRandomColorCount, '0');

        for (int slot = 0; slot < kRandomColorCount; ++slot) {
            legacy[slot] = (m_randomColorAllowedP1[slot] || m_randomColorAllowedP2[slot]) ? '1' : '0';
        }

        auto* mod = Mod::get();
        mod->setSavedValue(kGhostRandomColorsMaskP1Key, p1);
        mod->setSavedValue(kGhostRandomColorsMaskP2Key, p2);
        mod->setSavedValue(kGhostRandomColorsMaskKey, legacy);
    }

void Ghosts::setRandomGhostColorAllowed(int slot, bool allowed, bool player1) {
        ensureRandomColorMaskLoaded_();

        if (slot < 0 || slot >= kRandomColorCount) return;
        auto& mask = player1 ? m_randomColorAllowedP1 : m_randomColorAllowedP2;
        mask[slot] = allowed ? 1 : 0;
        rebuildRandomColorAllowedLists_();
        saveRandomColorMasks_();
    }

void Ghosts::setAllRandomGhostColorsAllowed(bool allowed, bool player1) {
        ensureRandomColorMaskLoaded_();

        auto& mask = player1 ? m_randomColorAllowedP1 : m_randomColorAllowedP2;
        mask.fill(allowed ? 1 : 0);
        rebuildRandomColorAllowedLists_();
        saveRandomColorMasks_();
    }

int Ghosts::pickRandomAllowedGhostColorIdx(uint32_t seed, bool player1) {
        ensureRandomColorMaskLoaded_();

        std::mt19937 rng(seed);
        auto const& allowed = player1 ? m_randomColorAllowedListP1 : m_randomColorAllowedListP2;

        if (allowed.empty()) {
            std::uniform_int_distribution<size_t> pick(0, kRandomColorCount - 1);
            return kRandomColorIDs[pick(rng)];
        }

        std::uniform_int_distribution<size_t> pick(0, allowed.size() - 1);
        return allowed[pick(rng)];
    }

void Ghosts::updateOpacityForAttempt(Attempt& a, GLubyte o, bool force) {
        m_skipGhostWork = (o == 0);
        if (!force && a.opacity == o) return;
        auto applyAll = [&](PlayerObject* po){
                if (!po) return;
                po->setOpacity(o);
                applyOpacityTree(po, o);
                if (po->m_waveTrail) {
                    po->m_waveTrail->setOpacity(
                        (GLubyte)std::clamp<int>(int(o * (m_waveTrailOpacityPct / 100.f)), 0, 255));
                    po->m_waveTrail->setVisible(o > 0);
                }
            };
        applyAll(a.g1);
        applyAll(a.g2);

        if (m_skipGhostWork) {
            if (a.g1) { a.setP1Visible(false); }
            if (a.g2) { a.setP2Visible(false); }
            a.primedP1 = a.primedP2 = false;
        }
        a.opacity = o;
    }

void Ghosts::setOpacityAll(GLubyte o) {
        m_skipGhostWork = (o == 0);

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                updateOpacityForAttempt(attempts[idx], o);
            }
        }

        m_zeroOpacityLatched = m_skipGhostWork;
    }

void Ghosts::updateOpacityAll() {
        m_skipGhostWork = (opacity == 0);

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                updateOpacityForAttempt(attempts[idx], opacity);
            }
        }

        m_zeroOpacityLatched = m_skipGhostWork;
    }

void Ghosts::setColorMode(ColorMode m) {
        colors = m;
        
        m_current.colorsAssigned = false;
        refreshAttemptColors_(m_current);
        
        for (auto& a : attempts) {
            a.colorsAssigned = false;
            if (a.g1 || a.g2) { 
                refreshAttemptColors_(a);
            }
        }
    }

void Ghosts::updateAllColors() {
        m_current.colorsAssigned = false;
        refreshAttemptColors_(m_current);
        for (auto& a : attempts) {
            a.colorsAssigned = false;
            if (a.g1 || a.g2) refreshAttemptColors_(a);
        }
    }

PlayLayer* Ghosts::getPlayLayer() const {
        auto* active = GJBaseGameLayer::get();
        if (!active || active != static_cast<GJBaseGameLayer*>(m_pl)) {
            return nullptr;
        }

        return typeinfo_cast<PlayLayer*>(active);
    }

void Ghosts::setRandomIconsEnabled(bool on) {
        if (m_randomIcons == on) return;
        m_randomIcons = on;

        for (size_t idx : m_preloadedIndices) {
            if (idx < attempts.size()) {
                auto& a = attempts[idx];
                if (a.g1) setPOFrameForIcon(a.g1, a.g1CurMode, a, m_randomIcons);
                if (a.g2) setPOFrameForIcon(a.g2, a.g2CurMode, a, m_randomIcons);
            }
        }
    }

bool Ghosts::practiceFilterEvenWhenBotNotOn() const {
        switch (m_replayKind) {
            case ReplayKind::PracticeComposite: return true;
            case ReplayKind::BestSingle: return false;
            default: return false;
        }
    }

std::optional<bool> Ghosts::practiceFilterForGhosts_() const {
        if (botActive) {
            switch (m_replayKind) {
                case ReplayKind::PracticeComposite: return std::optional<bool>(true);
                case ReplayKind::BestSingle: return std::optional<bool>(false);
                default: return std::nullopt;
            }
        }

        if (playback) return std::nullopt;
        
        const bool inPractice = (m_pl && m_pl->m_isPracticeMode);
        return std::optional<bool>(inPractice);
    }

void Ghosts::updateGhostVisibility() {
        if (opacity == 0) {
            for (size_t idx : m_primedIndices) {
                if (idx < attempts.size()) {
                    auto& a = attempts[idx];
                    if (a.g1) a.setP1Visible(false);
                    if (a.g2) a.setP2Visible(false);
                    a.primedP1 = a.primedP2 = false;
                }
            }
            return;
        }
    }

bool Ghosts::mirrorEndpointSide_(float t, int& sideOut) const {
        if (t <= kMirrorEndpointEps_) {
            sideOut = 0;
            return true;
        }
        if (t >= 1.f - kMirrorEndpointEps_) {
            sideOut = 1;
            return true;
        }
        return false;
    }

void Ghosts::syncGhostMirrorEndpointState_(int side) {
        m_mirrorAnchorSide = side;
        m_haveMirrorAnchorSide = true;
        m_lastMirrorFlipT = side ? 1.f : 0.f;
        m_haveMirrorFlipSample = true;

        m_mirrorEndpointHoldFrames = 0;
        m_pendingMirrorAnchorSide = -1;
        m_lastMirrorVisualT = 0.f;
    }

float Ghosts::mirrorFlipT_() const {
        if (!m_gl) return 0.f;

        float t = static_cast<float>(m_gl->m_gameState.m_levelFlipping);
        if (!std::isfinite(t)) return 0.f;

        return std::clamp(t, 0.f, 1.f);
    }

void Ghosts::updatePlayerMirrorAnchorSide_(float t) {

        if (t == 0.f) {
            m_playerMirrorAnchorSide = 0;
            m_havePlayerMirrorAnchorSide = true;
        }
        else if (t == 1.f) {
            m_playerMirrorAnchorSide = 1;
            m_havePlayerMirrorAnchorSide = true;
        }
        else if (!m_havePlayerMirrorAnchorSide) {
            if (m_havePlayerMirrorFlipSample) {
                if (t > m_lastPlayerMirrorFlipT) {
                    m_playerMirrorAnchorSide = 0;
                    m_havePlayerMirrorAnchorSide = true;
                }
                else if (t < m_lastPlayerMirrorFlipT) {
                    m_playerMirrorAnchorSide = 1;
                    m_havePlayerMirrorAnchorSide = true;
                }
            }

            if (!m_havePlayerMirrorAnchorSide) {
                m_playerMirrorAnchorSide = t < 0.5f ? 0 : 1;
                m_havePlayerMirrorAnchorSide = true;
            }
        }

        m_lastPlayerMirrorFlipT = t;
        m_havePlayerMirrorFlipSample = true;
    }

void Ghosts::updateMirrorAnchorSide_(float t) {
        if (!m_haveMirrorAnchorSide) {
            if (t == 0.f) {
                m_mirrorAnchorSide = 0;
                m_haveMirrorAnchorSide = true;
            }
            else if (t == 1.f) {
                m_mirrorAnchorSide = 1;
                m_haveMirrorAnchorSide = true;
            }
            else if (m_haveMirrorFlipSample) {
                if (t > m_lastMirrorFlipT) {
                    m_mirrorAnchorSide = 0;
                    m_haveMirrorAnchorSide = true;
                }
                else if (t < m_lastMirrorFlipT) {
                    m_mirrorAnchorSide = 1;
                    m_haveMirrorAnchorSide = true;
                }
            }

            if (!m_haveMirrorAnchorSide) {
                m_mirrorAnchorSide = t < 0.5f ? 0 : 1;
                m_haveMirrorAnchorSide = true;
            }
        }

        m_lastMirrorFlipT = t;
        m_haveMirrorFlipSample = true;
    }

bool Ghosts::mirrorEndpointVisualHold_(float rawT, float& visualTOut) {
        const bool atNormalEnd = rawT == 0.f;
        const bool atMirrorEnd = rawT == 1.f;

        if (!atNormalEnd && !atMirrorEnd) {
            m_mirrorEndpointHoldFrames = 0;
            m_pendingMirrorAnchorSide = -1;
            return false;
        }

        const int endpointSide = atMirrorEnd ? 1 : 0;

        if (m_pendingMirrorAnchorSide != endpointSide) {
            m_pendingMirrorAnchorSide = endpointSide;
            m_mirrorEndpointHoldFrames = 1;

            visualTOut = m_mirrorAnchorSide == 0
                ? static_cast<float>(endpointSide)
                : 1.f - static_cast<float>(endpointSide);

            m_lastMirrorVisualT = visualTOut;
            return true;
        }

        if (m_mirrorEndpointHoldFrames > 0) {
            --m_mirrorEndpointHoldFrames;

            visualTOut = m_lastMirrorVisualT;
            return true;
        }

        m_mirrorAnchorSide = endpointSide;
        m_haveMirrorAnchorSide = true;
        m_pendingMirrorAnchorSide = -1;

        visualTOut = 0.f;
        m_lastMirrorVisualT = 0.f;
        return true;
    }

float Ghosts::screenCenterXInNode_(cocos2d::CCNode* node) const {
        if (!node) return 0.f;

        auto* director = cocos2d::CCDirector::sharedDirector();
        if (!director) return 0.f;

        const float screenCX = (director->m_fScreenLeft + director->m_fScreenRight) * 0.5f;
        const float screenCY = (director->m_fScreenTop + director->m_fScreenBottom) * 0.5f;

        return node->convertToNodeSpace({screenCX, screenCY}).x;
    }

cocos2d::CCPoint Ghosts::mirrorDisplayPointInParentAtT_(
        cocos2d::CCPoint p,
        cocos2d::CCNode* parent,
        float t) const {
        if (!parent) return p;

        t = std::clamp(t, 0.f, 1.f);

        const float pivotX = screenCenterXInNode_(parent);

        p.x = p.x * (1.f - 2.f * t) + 2.f * pivotX * t;
        return p;
    }

cocos2d::CCPoint Ghosts::parentVectorToPlayerLocal_(
        PlayerObject* player,
        cocos2d::CCPoint deltaParent) const {
        if (!player) return {0.f, 0.f};

        auto inv = cocos2d::CCAffineTransformInvert(player->nodeToParentTransform());

        auto zero = cocos2d::CCPointApplyAffineTransform({0.f, 0.f}, inv);
        auto moved = cocos2d::CCPointApplyAffineTransform(deltaParent, inv);

        return {
            moved.x - zero.x,
            moved.y - zero.y
        };
    }

void Ghosts::applyMirrorVisualTransformToNode_(cocos2d::CCNode* node) {
        if (!node) return;

        auto* parent = node->getParent();
        if (!parent) return;

        const float rawT = mirrorFlipT_();
        int endpointSide = 0;
        if (mirrorEndpointSide_(rawT, endpointSide)) {
            syncGhostMirrorEndpointState_(endpointSide);
            node->setScaleX(1.f);
            node->setPositionX(0.f);
            return;
        }

        updateMirrorAnchorSide_(rawT);

        const float t = mirrorVisualT_(rawT);
        m_lastMirrorVisualT = t;

        const float pivotX = screenCenterXInNode_(parent);
        const float sx = 1.f - 2.f * t;

        node->setScaleX(sx);
        node->setPositionX(2.f * pivotX * t);
    }

void Ghosts::resetMirrorVisualState_() {
        m_lastMirrorFlipT = mirrorFlipT_();
        m_mirrorAnchorSide = m_lastMirrorFlipT >= 0.5f ? 1 : 0;

        int endpointSide = 0;
        if (mirrorEndpointSide_(m_lastMirrorFlipT, endpointSide)) {
            m_mirrorAnchorSide = endpointSide;
            m_playerMirrorAnchorSide = endpointSide;
        }
        else {
            m_mirrorAnchorSide = m_lastMirrorFlipT >= 0.5f ? 1 : 0;
            m_playerMirrorAnchorSide = m_mirrorAnchorSide;
        }

        m_haveMirrorFlipSample = true;
        m_haveMirrorAnchorSide = true;

        m_mirrorEndpointHoldFrames = 0;
        m_pendingMirrorAnchorSide = -1;
        m_lastMirrorVisualT = 0.f;

        m_lastPlayerMirrorFlipT = m_lastMirrorFlipT;
        m_havePlayerMirrorFlipSample = true;
        m_havePlayerMirrorAnchorSide = true;

        if (m_ghostRoot) {
            m_ghostRoot->setScaleX(1.f);
            m_ghostRoot->setPositionX(0.f);
        }
        if (m_deathMarkerDrawNode) {
            m_deathMarkerDrawNode->setScaleX(1.f);
            m_deathMarkerDrawNode->setPositionX(0.f);
        }

        resetPlayerMirrorVisualOffsets_();
    }

void Ghosts::updateMirrorVisuals_() {
        if (!botActive) return;

        if (m_mirrorVisualWarmupFrames > 0) {
            --m_mirrorVisualWarmupFrames;

            if (m_ghostRoot) {
                m_ghostRoot->setScaleX(1.f);
                m_ghostRoot->setPositionX(0.f);
            }
            if (m_deathMarkerDrawNode) {
                m_deathMarkerDrawNode->setScaleX(1.f);
                m_deathMarkerDrawNode->setPositionX(0.f);
            }

            resetPlayerMirrorVisualOffsets_();

            int endpointSide = 0;
            if (mirrorEndpointSide_(mirrorFlipT_(), endpointSide)) {
                syncGhostMirrorEndpointState_(endpointSide);
                syncPlayerMirrorEndpointState_(endpointSide);
            }

            return;
        }

        applyMirrorVisualTransformToNode_(m_ghostRoot);
        if (m_ghostRoot && m_deathMarkerDrawNode) {
            m_deathMarkerDrawNode->setScaleX(m_ghostRoot->getScaleX());
            m_deathMarkerDrawNode->setPositionX(m_ghostRoot->getPositionX());
        }
        updatePlayerMirrorVisuals_();
    }

void Ghosts::applyPlayerVisualChildOffset_(
        PlayerObject* player,
        cocos2d::CCPoint wantedOffset) {
        if (!player) return;

        cocos2d::CCPoint* prevOffset = nullptr;

        if (m_pl && player == m_pl->m_player1) {
            prevOffset = &m_playerMirrorVisualOffsetP1;
        }
        else if (m_pl && player == m_pl->m_player2) {
            prevOffset = &m_playerMirrorVisualOffsetP2;
        }
        else {
            return;
        }

        const cocos2d::CCPoint delta = {
            wantedOffset.x - prevOffset->x,
            wantedOffset.y - prevOffset->y
        };

        if (std::abs(delta.x) <= 0.0001f && std::abs(delta.y) <= 0.0001f) {
            return;
        }

        for (auto* child : player->getChildrenExt()) {
            if (!child) continue;

            if (player->m_waveTrail && child == player->m_waveTrail) {
                continue;
            }

            const auto pos = child->getPosition();
            child->setPosition({
                pos.x + delta.x,
                pos.y + delta.y
            });
        }

        *prevOffset = wantedOffset;
    }

cocos2d::CCPoint Ghosts::playerMirrorVisualOffsetLocal_(PlayerObject* player) {
        if (!player) return {0.f, 0.f};

        auto* parent = player->getParent();
        if (!parent) return {0.f, 0.f};

        const float rawT = mirrorFlipT_();
        int endpointSide = 0;
        if (mirrorEndpointSide_(rawT, endpointSide)) {
            syncPlayerMirrorEndpointState_(endpointSide);
            return {0.f, 0.f};
        }

        updatePlayerMirrorAnchorSide_(rawT);

        float p = playerMirrorProgress_(rawT);
        p = std::clamp(p, 0.f, 1.f);

        if (p == 0.f) {
            return {0.f, 0.f};
        }

        const auto rawParentPos = player->getPosition();

        const auto targetParentPos = mirrorDisplayPointInParentAtT_(
            rawParentPos,
            parent,
            p
        );

        cocos2d::CCPoint deltaParent = {
            targetParentPos.x - rawParentPos.x,
            0.f
        };

        if (p > 0.5f) {
            deltaParent.x = -deltaParent.x;
        }

        return parentVectorToPlayerLocal_(
            player,
            deltaParent
        );
    }

void Ghosts::updatePlayerMirrorVisuals_() {
        if (!m_pl) return;

        applyPlayerVisualChildOffset_(
            m_pl->m_player1,
            playerMirrorVisualOffsetLocal_(m_pl->m_player1)
        );

        if (m_pl->m_player2) {
            applyPlayerVisualChildOffset_(
                m_pl->m_player2,
                playerMirrorVisualOffsetLocal_(m_pl->m_player2)
            );
        }
    }

void Ghosts::resetPlayerMirrorVisualOffsets_() {
        if (m_pl) {
            applyPlayerVisualChildOffset_(m_pl->m_player1, {0.f, 0.f});

            if (m_pl->m_player2) {
                applyPlayerVisualChildOffset_(m_pl->m_player2, {0.f, 0.f});
            }
        }

        m_playerMirrorVisualOffsetP1.x = 0.f;
        m_playerMirrorVisualOffsetP1.y = 0.f;
        m_playerMirrorVisualOffsetP2.x = 0.f;
        m_playerMirrorVisualOffsetP2.y = 0.f;

        m_lastPlayerMirrorFlipT = mirrorFlipT_();
        m_playerMirrorAnchorSide = m_lastPlayerMirrorFlipT >= 0.5f ? 1 : 0;
        m_havePlayerMirrorFlipSample = true;
        m_havePlayerMirrorAnchorSide = true;
    }

bool Ghosts::needToFreePlayerObjectsForWaitingQueue(size_t numCurrentlyDead) {
        if (!m_allowClearOldPlayerObjectsWhenMoreNeeded) return false;
        if (m_wantToPrimeIndices.empty()) return false;
        if (numCurrentlyDead < minKeepDead) return false;
        int free = m_playerObjectPool.capacity() - 2 - m_playerObjectPool.inUseCount();
        if (m_wantToPrimeIndices.size() > free) return true;
        return false;
    }

void Ghosts::refreshAndPreloadCustomExplodeSfx_(bool force) {
        if (!m_useCustomExplodeSounds)
            return;

        if (!force) {
            const uint64_t elapsed = m_tickId - m_lastCustomSfxRefreshTick;
            if (elapsed < kCustomSfxRefreshIntervalTicks)
                return;
        }

        m_lastCustomSfxRefreshTick = m_tickId;

        if (force) {
            customLastWrite = {};
        }

        refreshCustomExplodeSfx_(customSfx, customLastWrite);

        if (!customSfx.empty()) {
            g_explodeLimiter.preloadAll(customSfx);
        }
    }

void Ghosts::invalidateAttemptPointerCaches_() {
        m_currentOwner = nullptr;
        m_replayAttempt = nullptr;
        m_replayOwnerIndex = SIZE_MAX;
        m_compOwnerIdx = SIZE_MAX;
        m_serialCacheDirty = true;
    }

bool Ghosts::getSettingBoolOrDefault_(geode::Mod* mod, const char* key, bool def) {
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", def);
            return def;
        }
        return mod->getSettingValue<bool>(key);
    }

int Ghosts::getSettingIntOrDefault_(geode::Mod* mod, const char* key, int def) {
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", def);
            return def;
        }
        return static_cast<int>(mod->getSettingValue<int64_t>(key));
    }

std::string Ghosts::getSettingStrOrDefault_(geode::Mod* mod, const char* key, const char* def) {
        const char* fallback = def ? def : "";
        if (!mod || !key || !mod->hasSetting(key)) {
            geode::log::warn("[Ghosts] setting '{}' missing; using {}", key ? key : "<null>", fallback);
            return std::string(fallback);
        }
        return mod->getSettingValue<std::string>(key);
    }

float Ghosts::attemptSpanX_(Attempt const& a) const
    {
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();

        if (!a.p1.empty()) {
            minX = std::min<float>(minX, static_cast<float>(a.p1.front().x));
            maxX = std::max<float>(maxX, static_cast<float>(a.p1.back().x));
        }
        if (a.hadDual && !a.p2.empty()) {
            minX = std::min<float>(minX, static_cast<float>(a.p2.front().x));
            maxX = std::max<float>(maxX, static_cast<float>(a.p2.back().x));
        }

        if (maxX <= minX)
            return 0.f;

        return maxX - minX;
    }

void Ghosts::applyRandomOffsetSingle(Attempt& a, bool forceNegative) {
        if (m_randomDistPx == 0) {
            a.ghostOffsetPx = 0.f;
            a.ghostOffsetTime = 0.0;
            return;
        }

        auto lastT = [](const std::vector<Frame>& v) -> double {
            return v.empty() ? 0.0 : static_cast<double>(v.back().t);
        };
        const double endT = std::max(lastT(a.p1), lastT(a.p2));

        // Deterministic signed random in [-1, 1]
        const float u = signedUnit_(
            static_cast<uint32_t>(a.serial) ^
            static_cast<uint32_t>(m_levelIDOnAttach) ^
            0x9E3779B9u
        );

        float offsetPx = u * static_cast<float>(m_randomDistPx);

        constexpr float kMinOffsetPx = -100.0f;
        constexpr float kMaxOffsetPx = 174.0f;
        constexpr float kExactOverlapEpsilonPx = 1.0f;
        constexpr double kEndGapSec = 0.5;

        const double ups = std::max(1e-6, m_offsetUnitsPerSecond);

        // Positive limit:
        const double maxForwardTime = std::max(0.0, endT - kEndGapSec);
        const double maxForwardPxD  = maxForwardTime * ups;
        const float usableMaxPx = std::min(
            kMaxOffsetPx,
            static_cast<float>(std::max(0.0, maxForwardPxD))
        );

        // Negative limit with wrapping:
        auto wrapIntoWindow = [](float x, float lo, float hi) -> float {
            if (hi <= lo) return lo;
            if (x >= lo && x <= hi) return x;

            const double span = static_cast<double>(hi) - static_cast<double>(lo);
            double y = static_cast<double>(x) - static_cast<double>(lo);
            y = std::fmod(y, span);
            if (y < 0.0) y += span;
            return static_cast<float>(static_cast<double>(lo) + y);
        };

        if (forceNegative) {
            offsetPx = -std::fabs(offsetPx);
            offsetPx = wrapIntoWindow(offsetPx, kMinOffsetPx, 0.0f);
        } else {
            offsetPx = wrapIntoWindow(offsetPx, kMinOffsetPx, usableMaxPx);
        }

        // Don't have 0 offset to avoid weird overlap
        if (std::fabs(offsetPx) < kExactOverlapEpsilonPx) {
            const bool canNudgeForward = usableMaxPx >= kExactOverlapEpsilonPx;
            const float sign = signedUnit_(
                static_cast<uint32_t>(a.serial) ^
                static_cast<uint32_t>(m_levelIDOnAttach) ^
                0x7F4A7C15u
            ) >= 0.f ? 1.f : -1.f;

            offsetPx = (sign > 0.f && canNudgeForward)
                ? kExactOverlapEpsilonPx
                : -kExactOverlapEpsilonPx;
        }

        a.ghostOffsetPx   = offsetPx;
        a.ghostOffsetTime = static_cast<double>(offsetPx) / ups;
    }

void Ghosts::applyRandomOffsetsAll_() {
        for (auto& a : attempts) {
            if (isRouteOwnerForOffset_(a)) {
                clearGhostOffset_(a);
                continue;
            }

            applyRandomOffsetSingle(a, false);
            a.offsetApplied = true;
        }
    }

void Ghosts::refreshAttemptColors_(Attempt& a) {
        if (!a.colorsAssigned) {
            if (colors == ColorMode::Random) {
                uint32_t seed = static_cast<uint32_t>(a.serial) ^ 0xA5339E4Du;
                a.c1p1 = randomGameColor(seed, true);
                a.c2p1 = randomGameColor(seed ^ 0x0BADA551u, true);
                a.c1p2 = randomGameColor(seed ^ 0x9E3779B9u, false);
                a.c2p2 = randomGameColor(seed ^ 0x85EBCA6Bu, false);
                hasGlowActive(true, a.hasGlowP1);
                hasGlowActive(false, a.hasGlowP2);
                if (a.hasGlowP1) a.cgp1 = randomGameColor(seed ^ 0x6C8E9CF5u, true);
                if (a.hasGlowP2) a.cgp2 = randomGameColor(seed ^ 0xD1B54A35u, false);
            } else {
                a.c1p1 = chooseColor1(true);
                a.c2p1 = chooseColor2(true);
                a.c1p2 = chooseColor1(false);
                a.c2p2 = chooseColor2(false);
                a.cgp1 = chooseGlowColor(true, a.hasGlowP1);
                a.cgp2 = chooseGlowColor(false, a.hasGlowP2);
            }
            a.colorsAssigned = true;
        }

        if (a.g1) {
            a.g1->setColor(a.c1p1);
            a.g1->setSecondColor(a.c2p1);
            if (a.g1->m_waveTrail) {
                if (m_pl->m_player1->m_switchWaveTrailColor) a.g1->m_waveTrail->setColor(a.c2p1);
                else a.g1->m_waveTrail->setColor(a.c1p1);
            }
            if (a.hasGlowP1) {
                a.g1->m_hasGlow = true;
                a.g1->updatePlayerGlow();
                a.g1->enableCustomGlowColor(a.cgp1);
                a.g1->updateGlowColor();
            }
        }
        if (a.g2) {
            a.g2->setColor(a.c1p2);
            a.g2->setSecondColor(a.c2p2);
            if (a.g2->m_waveTrail) {
                if (m_pl->m_player2->m_switchWaveTrailColor) a.g2->m_waveTrail->setColor(a.c1p2);
                else a.g2->m_waveTrail->setColor(a.c2p2);
            }
            if (a.hasGlowP2) {
                a.g2->m_hasGlow = true;
                a.g2->updatePlayerGlow();
                a.g2->enableCustomGlowColor(a.cgp2);
                a.g2->updateGlowColor();
            }
        }
    }

void Ghosts::prepareAttemptVisualData_(Attempt& a) {
        if (!a.acc1Time.valid() && !a.p1.empty()) {
            buildAccelTime(a.p1, a.acc1Time);
        }
        if (a.hadDual && !a.acc2Time.valid() && !a.p2.empty()) {
            buildAccelTime(a.p2, a.acc2Time);
        }
        if (!a.iconsAssigned) {
            const uint32_t base = static_cast<uint32_t>(a.serial) ^ 0xDEADBEEFu;
            for (int i = 0; i < 9; ++i) {
                a.randomFrame[i] = randomIconFrame(static_cast<IconType>(i), base + i * 97);
            }
            a.iconsAssigned = true;
        }
    }

void Ghosts::preloadAttempt_(Attempt& a, size_t attemptIdx, bool force, bool prepareVisuals) {
        if (!force && a.preloaded) return;
        if (!m_pl || !m_pl->m_player1) return;

        a.m_isPlatformer = m_pl->m_isPlatformer;

        // Old start pos attempts will have timestep 0.f but new ones have the correct timestep, so offset them all (oopsies this might be slow)
        if (m_baseTime != 0.f && a.baseTimeOffset == 0.f) {
            a.baseTimeOffset = m_baseTime;
            const uint32_t offsetQ = runtimeQuantTimeQ_(m_baseTime);
            offsetAttemptTimesFast(a, offsetQ);
        }

        prepareAttemptVisualData_(a);

        a.preloaded = true;
        ++m_attemptsPreloadedTotal;
        
        if (attemptIdx != SIZE_MAX && m_preloadedSet.find(attemptIdx) == m_preloadedSet.end()) {
            //log::info("push into preload set");
            m_preloadedIndices.push_back(attemptIdx);
            m_preloadedSet.insert(attemptIdx);
        }
        else {
            log::warn("Didn't push into preload set: attemptIdx: {}, m_preloadedSet.size(): {}", attemptIdx, m_preloadedSet.size());
        }
    }

void Ghosts::tagCompositeOwnersHidden_(bool hide) {
        for (auto& a : attempts) a.ignorePlayback = false;
        if (!hide) return;
        if (!routeIsComposite_()) {
            if (m_compOwnerIdx != SIZE_MAX && m_compOwnerIdx < attempts.size())
                attempts[m_compOwnerIdx].ignorePlayback = true;
        }
    }

std::optional<cocos2d::CCPoint> Ghosts::snapWavePointSimple_(
        cocos2d::CCPoint raw,
        bool isMini,
        bool isP1) {
        if (!m_CBFwaveFixEnabled) return raw;
        auto& have = isP1 ? m_haveLastSnappedWavePointP1 : m_haveLastSnappedWavePointP2;
        auto& last = isP1 ? m_lastSnappedWavePointP1 : m_lastSnappedWavePointP2;
        auto& lastMini = isP1 ? m_lastSnappedWaveMiniP1 : m_lastSnappedWaveMiniP2;
        auto& lastDir = isP1 ? m_lastSnappedWaveDirP1 : m_lastSnappedWaveDirP2;

        if (!std::isfinite(raw.x) || !std::isfinite(raw.y))
            return std::nullopt;

        const float slope = isMini ? 2.0f : 1.0f;

        if (!have || lastMini != isMini) {
            have = true;
            last = raw;
            lastMini = isMini;
            lastDir = 0;
            return raw;
        }

        const float dx = raw.x - last.x;

        if (dx < 0.25f)
            return std::nullopt;

        if (std::fabs(raw.y - last.y) > kWaveTeleportedTolerance) {
            last = raw;
            lastMini = isMini;
            lastDir = 0;
            return raw;
        }

        const float upY   = last.y + slope * dx;
        const float flatY = last.y;
        const float downY = last.y - slope * dx;

        float bestY = flatY;
        int bestDir = 0;
        float bestErr = std::fabs(raw.y - flatY);

        const float upErr = std::fabs(raw.y - upY);
        if (upErr < bestErr) {
            bestErr = upErr;
            bestY = upY;
            bestDir = 1;
        }

        const float downErr = std::fabs(raw.y - downY);
        if (downErr < bestErr) {
            bestErr = downErr;
            bestY = downY;
            bestDir = -1;
        }

        // If it is close to flat, force flat
        constexpr float kFlatSnapTolerancePx = 2.0f;
        if (std::fabs(raw.y - flatY) <= kFlatSnapTolerancePx) {
            bestY = flatY;
            bestDir = 0;
        }

        cocos2d::CCPoint snapped = cocos2d::CCPoint(raw.x, bestY);

        if (!std::isfinite(snapped.x) || !std::isfinite(snapped.y))
            return std::nullopt;

        last = snapped;
        lastMini = isMini;
        lastDir = bestDir;

        return snapped;
    }

