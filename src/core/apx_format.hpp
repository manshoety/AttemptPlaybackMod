#pragma once
#include <cstdint>

static constexpr uint32_t kChunk_ATTZ = 0x5A545441;          // 'ATTZ' legacy attempt chunk
static constexpr uint32_t kChunk_ATT3 = 0x33545441;          // 'ATT3' compact attempt chunk
static constexpr uint32_t kChunk_CheckpointZ2 = 0x325A5043;  // legacy checkpoint chain chunk
static constexpr uint32_t kChunk_SEGZ = 0x5A474553;          // legacy
static constexpr uint32_t kChunk_PATH = 0x48544150;          // 'PATH'

static constexpr uint16_t kAPXCompactAttemptVersion_V1 = 1;  // old ATT3 with startCheckpointId
static constexpr uint16_t kAPXCompactAttemptVersion_V2 = 2;  // new ATT3 without it
static constexpr uint16_t kAPXCompactAttemptVersion = kAPXCompactAttemptVersion_V2;

static constexpr uint32_t kAPXPathVersion_V4 = 4;  // old PATH with checkpoints/activeChain
static constexpr uint32_t kAPXPathVersion_V5 = 5;  // new PATH without checkpoints

// V8 removes old wave slide and replaces it with the wave point bool thingy (wavePointThisFrame)
static constexpr uint32_t kAPXVersion = 8;
static constexpr uint32_t kAPXVersion_WavePointThisFrame = 8;