#include "ghost_manager.hpp"

void Ghosts::endPlayerDestroyedCheck(bool playerDied) {
        if (shouldIgnoreNoclipDetection_()) {
            isNoclipDetected = false;
            setFalseIfPlayerWasDestroyedCheck = false;
            return;
        }

         if (!isNoclipDetected) {
             if (setFalseIfPlayerWasDestroyedCheck) isNoclipDetected = !playerDied;
         }
    }

float Ghosts::getAccuratePercentForX(float x, float fallbackPercent) const {
        const float fallback = std::clamp(fallbackPercent, 0.f, 100.f);
        if (!m_pl || m_pl->m_isPlatformer || !std::isfinite(x)) return fallback;

        const float endX = m_pl->m_endXPosition;
        if (!std::isfinite(endX) || endX <= 0.f) return fallback;

        return std::clamp((x / endX) * 100.f, 0.f, 100.f);
    }

bool Ghosts::removeReverseOrDuplicateFrames(std::vector<Frame>& frames, double t) {
        // Reverse frames
        while (!frames.empty() && t < frames.back().t) {
            frames.pop_back();
        }

        // Duplicate frame
        if (!frames.empty() && t == frames.back().t) return false;

        return true;
    }

void Ghosts::insertWaveStateIntoLastFrame(bool isPlayer1) {
        if (isPlayer1) {
            if (m_current.p1.empty()) {
                p1WavePointThisFrame = false;
                return;
            }

            m_current.p1.back().wavePointThisFrame = p1WavePointThisFrame;
            p1WavePointThisFrame = false;
            return;
        }

        if (m_current.p2.empty()) {
            p2WavePointThisFrame = false;
            return;
        }

        m_current.p2.back().wavePointThisFrame = p2WavePointThisFrame;
        p2WavePointThisFrame = false;
    }

void Ghosts::recordCurrentFrame_(bool isPlayer1) {
        if (m_justDied) return;
        
        const double absoluteTime = m_pl->m_attemptTime + m_current.baseTimeOffset;
        // log::info("p1Hold: {}", p1Hold);
        
        if (m_pl->m_player1 && isPlayer1) {
            if (!removeReverseOrDuplicateFrames(m_current.p1, absoluteTime)) return;

            auto* p1 = m_pl->m_player1;
            Frame f;
            f.t = absoluteTime;
            cocos2d::CCPoint pos = p1->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p1->getRotation();
            f.vehicleSize = p1->m_vehicleSize;
            f.waveSize = p1->m_waveTrail ? p1->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p1->m_isUpsideDown;
            f.isDashing = p1->m_isDashing;
            f.isVisible = p1->isVisible();
            //f.wavePointThisFrame = p1WavePointThisFrame;
            //log::info("wavePointThisFrame: {}", p1WavePointThisFrame);
            /*
           log::info(
                "[record P1] serial={} idx={} t={} y={} exp={}",
                m_current.serial,
                m_current.p1.size(),
                absoluteTime,
                static_cast<float>(f.y),
                static_cast<int>(f.wavePointThisFrame)
            );*/

            // Moved to runtime to reduce recording delay
            //if (!m_current.p1.empty()) {
            //    const float prevY = static_cast<float>(m_current.p1.back().y);
            //    f.stateDartSlide = std::fabs(static_cast<float>(f.y) - prevY) <= kYEqualEps;
            //} else {
            //    f.stateDartSlide = false;
            //}

            f.mode = currentMode(p1, m_pl->m_isPlatformer);
            f.hold = p1Hold;
            f.holdL = p1LHold;
            f.holdR = p1RHold;

            m_current.p1.push_back(f);
            m_lastRecordedX = static_cast<float>(f.x);
            const float currentPercent = m_pl->getCurrentPercent();
            if (currentPercent > m_current.endPercent) m_current.endPercent = currentPercent;
            // p1WavePointThisFrame = false;
        }
        
        if (m_pl->m_player2 && m_prevHadP2 && !isPlayer1) {
            if (!removeReverseOrDuplicateFrames(m_current.p2, absoluteTime)) return;
            m_current.hadDual = true;
            auto* p2 = m_pl->m_player2;
            Frame f;
            f.t = absoluteTime;
            cocos2d::CCPoint pos = p2->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p2->getRotation();
            f.vehicleSize = p2->m_vehicleSize;
            f.waveSize = p2->m_waveTrail ? p2->m_waveTrail->m_waveSize : 1.0f;
            f.upsideDown = p2->m_isUpsideDown;
            f.isDashing = p2->m_isDashing;
            f.isVisible = p2->isVisible();
            //f.wavePointThisFrame = p2WavePointThisFrame;

            // Moved to runtime to reduce recording delay
            //if (!m_current.p2.empty()) {
            //    const float prevY = static_cast<float>(m_current.p2.back().y);
            //    f.stateDartSlide = std::fabs(static_cast<float>(f.y) - prevY) <= kYEqualEps;
            //} else {
            //    f.stateDartSlide = false;
            //}

            f.mode = currentMode(p2, m_pl->m_isPlatformer);
            f.hold = p2Hold;
            f.holdL = p2LHold;
            f.holdR = p2RHold;
            m_current.p2.push_back(f);
            // p2WavePointThisFrame = false;
        }
    }

void Ghosts::seedInitialAttemptFrame_() {
        if (!m_pl || m_is_quitting) return;
        if (!recording || recordingBlocked) return;
        if (m_justDied) return;

        const bool isPractice = m_pl->m_isPracticeMode;
        if (isPractice && !recordInPractice) return;

        const double t = m_baseTime;

        if (m_pl->m_player1 && m_current.p1.empty()) {
            auto* p1 = m_pl->m_player1;

            Frame f;
            f.t = t;

            cocos2d::CCPoint pos = p1->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p1->getRotation();

            f.vehicleSize = p1->m_vehicleSize;
            f.waveSize = p1->m_waveTrail ? p1->m_waveTrail->m_waveSize : 1.0f;

            f.upsideDown = p1->m_isUpsideDown;
            f.isDashing = p1->m_isDashing;
            f.isVisible = p1->isVisible();

            f.wavePointThisFrame = p1WavePointThisFrame;
            f.mode = currentMode(p1, m_pl->m_isPlatformer);

            f.hold = p1Hold;
            f.holdL = p1LHold;
            f.holdR = p1RHold;

            m_current.p1.push_back(f);

            /*
            log::info(
                "[seed P1] serial={} idx=0 t={} y={} exp={}",
                m_current.serial,
                static_cast<double>(f.t),
                static_cast<float>(f.y),
                static_cast<int>(f.wavePointThisFrame)
            );*/

            p1WavePointThisFrame = false;
        }

        if (m_pl->m_player2 && m_prevHadP2 && m_current.p2.empty()) {
            auto* p2 = m_pl->m_player2;

            Frame f;
            f.t = t;

            cocos2d::CCPoint pos = p2->getPosition();
            f.x = pos.x;
            f.y = pos.y;
            f.rot = p2->getRotation();

            f.vehicleSize = p2->m_vehicleSize;
            f.waveSize = p2->m_waveTrail ? p2->m_waveTrail->m_waveSize : 1.0f;

            f.upsideDown = p2->m_isUpsideDown;
            f.isDashing = p2->m_isDashing;
            f.isVisible = p2->isVisible();

            f.wavePointThisFrame = p2WavePointThisFrame;
            f.mode = currentMode(p2, m_pl->m_isPlatformer);

            f.hold = p2Hold;
            f.holdL = p2LHold;
            f.holdR = p2RHold;

            m_current.p2.push_back(f);
            m_current.hadDual = true;

            /*
            log::info(
                "[seed P2] serial={} idx=0 t={} y={} exp={}",
                m_current.serial,
                static_cast<double>(f.t),
                static_cast<float>(f.y),
                static_cast<int>(f.wavePointThisFrame)
            );*/

            p2WavePointThisFrame = false;
        }
    }

void Ghosts::startNewAttempt() {
        m_current = Attempt{};
        m_currentDidNoclip = false;
        m_current.serial = m_nextAttemptSerial++;
        m_currentAttemptStart = m_frameCounter;
        
        float startX = 0.f;
        float startY = 0.f;
        m_current.endPercent = 0.f;
        
        if (m_pl && m_pl->m_player1) {
            startX = m_pl->m_player1->getPositionX();
            startY = m_pl->m_player1->getPositionY();
        }
        
        m_current.startX = startX;
        m_current.startY = startY;

        m_baseTime = getStartTime();
        m_current.baseTimeOffset = m_baseTime;

        // log::info("[startNewAttempt] baseTime: {}", m_baseTime);

        //if (recording) {
            //m_current.seed = seedutils::getCurrentSeed();
            //if (m_current.seed == 0) {
            //    m_current.seed = seedutils::generateFreshSeed();
            //    seedutils::setRawSeed(m_current.seed);
            //}
            //log::info("[Seed] Captured seed {} for attempt {}", m_current.seed, m_current.serial);
        //}

        //log::info("[startNewAttempt] Attempt created with time offset {}", m_current.baseTimeOffset);
        
        m_current.practiceAttempt = (m_pl && m_pl->m_isPracticeMode);
        m_current.ignorePlayback = false;
        m_lastRecordedX = 0.f;
        
        if (m_pl) {
            m_current.startPercent = m_current.practiceAttempt
                ? getAccuratePercentForX(startX, m_pl->getCurrentPercent())
                : m_pl->getCurrentPercent();
        } else {
            m_current.startPercent = 0;
        }
        m_current.recordedThisSession = true;

        seedInitialAttemptFrame_();
    }

void Ghosts::sortBestToFront() {
        float best=-1.f;
        size_t bestIdx=0;
        for (size_t i=0; i<attempts.size(); ++i) {
            float lastX = !attempts[i].p1.empty() ? static_cast<float>(attempts[i].p1.back().x) : 0.f;
            if (lastX > best) {
                best=lastX;
                bestIdx=i;
            }
        }
        if (bestIdx!=0) {
            std::swap(attempts[0], attempts[bestIdx]);
        }
    }

