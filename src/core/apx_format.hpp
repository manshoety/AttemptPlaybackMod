// apx_format.hpp
#pragma once
#include <cstdint>

static constexpr uint32_t kChunk_ATTZ = 0x5A545441;         // 'ATTZ'
static constexpr uint32_t kChunk_CheckpointZ2 = 0x325A5043; // Legacy
static constexpr uint32_t kChunk_SEGZ = 0x5A474553;         // Legacy
static constexpr uint32_t kChunk_PATH = 0x48544150;         // 'PATH'

static constexpr uint32_t kAPXVersion = 3;
