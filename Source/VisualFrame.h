// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#pragma once

#include "JpegCodec.h"
#include <array>
#include <cstdint>

namespace deepfry
{
struct VisualChannelFrame
{
    // The input and decoded wet tile use the codec's luminance mapping.
    TileFrame image;
    // These are the actual final audio samples, including smoothed Mix, Output,
    // and Bypass. They are not clamped to the visualizer's display range.
    std::array<float, tileSize> output {};
};

struct VisualFrame
{
    std::array<VisualChannelFrame, 2> channels {};
    int channelCount = 0;
    double sampleRate = 0.0;
    // Distinguishes prepares even if a restarted sample counter has already
    // passed the held history while the editor was frozen or not polling.
    std::uint64_t streamGeneration = 0;
    // First source input sample since prepareToPlay. The captured output for
    // this tile occurs 64 samples later, matching the processor's latency.
    std::uint64_t samplePosition = 0;
};
}
