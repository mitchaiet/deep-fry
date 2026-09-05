// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#include "JpegCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

namespace
{
using Tile = std::array<float, deepfry::tileSize>;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0;

void require(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

double rmsError(const Tile& a, const Tile& b)
{
    double squared = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
        squared += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(squared / static_cast<double>(a.size()));
}

void requireValid(const Tile& output, const deepfry::TileFrame& frame)
{
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        require(std::isfinite(output[i]) && output[i] >= -1.0f && output[i] <= 1.0f,
                "decoded audio is finite and bounded");
        require(std::isfinite(frame.before[i]) && frame.before[i] >= 1.0f && frame.before[i] <= 255.0f,
                "source image luminance is finite and bounded");
        require(std::isfinite(frame.after[i]) && frame.after[i] >= 1.0f && frame.after[i] <= 255.0f,
                "decoded image luminance is finite and bounded");
        require(std::abs(frame.after[i] - (128.0f + 127.0f * output[i])) < 0.0001f,
                "visualized pixels represent actual audio output");
    }
    require(frame.retained >= 0.0f && frame.retained <= 1.0f, "retention is a valid fraction");
}

void silenceAndQuantization()
{
    deepfry::JpegCodec codec;
    Tile input {}, output {};
    deepfry::TileFrame frame;
    for (float quality : { 1.0f, 35.0f, 100.0f })
        for (float fry : { 0.0f, 25.0f, 100.0f })
            for (int bits = 2; bits <= 8; ++bits)
            {
                codec.process(input, output, { quality, fry, bits }, &frame);
                for (std::size_t i = 0; i < input.size(); ++i)
                {
                    require(output[i] == 0.0f, "silence stays exactly silent at every setting");
                    require(frame.before[i] == 128.0f && frame.after[i] == 128.0f,
                            "silence is an exactly neutral grey image");
                }
                require(frame.retained == 0.0f, "silent tile has no retained coefficients");
            }

    // Analytic JPEG reference, independent of the implementation's transform:
    // a constant image at signed pixel32 has only DC=8*32=256. At quality10,
    // Annex K DC quantizer16 scales to80, so DC rounds to240 -> pixel30.
    input.fill(32.0f / 127.0f);
    codec.process(input, output, { 10.0f, 0.0f, 8 }, &frame);
    for (float sample : output)
        require(std::abs(sample - 30.0f / 127.0f) < 0.00001f, "JPEG DC quantization matches analytic reference");
    require(frame.retained == 1.0f / 64.0f, "constant image retains exactly one coefficient");

    // Quality1 clamps baseline quantizer to255: DC256 ->255 -> pixel32.
    codec.process(input, output, { 1.0f, 0.0f, 8 });
    for (float sample : output)
        require(std::abs(sample - 32.0f / 127.0f) < 0.00001f, "baseline JPEG quantizer is capped at255");

    for (int bits = 2; bits <= 8; ++bits)
    {
        codec.process(input, output, { 100.0f, 0.0f, bits });
        const float levels = static_cast<float>((1 << (bits - 1)) - 1);
        for (float sample : output)
            require(std::abs(sample * levels - std::round(sample * levels)) < 0.00001f,
                    "posterized output lies on the expected symmetric pixel grid");
    }
}

void qualityAndReconstruction()
{
    deepfry::JpegCodec codec;
    Tile input {}, pristine {}, destroyed {};
    deepfry::TileFrame highFrame, lowFrame;
    std::mt19937 random(0x4a504547);
    std::uniform_real_distribution<float> amplitude(-0.9f, 0.9f);
    double meanError = 0.0;
    for (int trial = 0; trial < 250; ++trial)
    {
        for (float& sample : input)
            sample = amplitude(random);
        codec.process(input, pristine, { 100.0f, 0.0f, 8 }, &highFrame);
        const double error = rmsError(input, pristine);
        require(error < 0.008, "quality100 reconstructs broadband tiles within pixel quantization error");
        for (std::size_t i = 0; i < input.size(); ++i)
            require(std::abs(pristine[i] - input[i]) < 0.020f, "quality100 maximum reconstruction error stays small");
        meanError += error;
    }

    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(0.42 * std::sin(2.0 * pi * 5.25 * i / input.size())
                                     + 0.18 * std::sin(2.0 * pi * 17.0 * i / input.size()));
    codec.process(input, pristine, { 100.0f, 0.0f, 8 }, &highFrame);
    codec.process(input, destroyed, { 4.0f, 0.0f, 8 }, &lowFrame);
    const double highError = rmsError(input, pristine);
    const double lowError = rmsError(input, destroyed);
    require(lowError > 0.04 && lowError > highError * 8.0,
            "low JPEG quality materially changes a compound audio tone");
    require(lowFrame.retained < highFrame.retained, "low JPEG quality removes image-frequency coefficients");

    codec.process(input, destroyed, { 100.0f, 100.0f, 8 }, &lowFrame);
    require(rmsError(pristine, destroyed) > 0.15, "Fry image operations materially affect the sound");

    std::cout << "Quality100 broadband RMS error: " << meanError / 250.0
              << "; compound tone RMS: clean=" << highError << ", low quality=" << lowError << '\n';
}

void transformOrientationReference()
{
    // An image containing only horizontal frequency1: DCT coefficient(0,1)=128.
    // Pixel rounding perturbs it slightly, but quality50 must quantize with the
    // horizontal table entry11, not vertical12. The direct mathematical expected
    // image is inverse DCT coefficient132. This detects accidental table transpose.
    deepfry::JpegCodec codec;
    Tile input {}, output {};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            input[y * 8 + x] = static_cast<float>(128.0 * std::sqrt(1.0 / 8.0) * 0.5
                * std::cos(pi * (2 * x + 1) / 16.0) / 127.0);
    codec.process(input, output, { 50.0f, 0.0f, 8 });
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
        {
            const float expected = static_cast<float>(std::round(132.0 * std::sqrt(1.0 / 8.0) * 0.5
                * std::cos(pi * (2 * x + 1) / 16.0)) / 127.0);
            require(std::abs(output[y * 8 + x] - expected) < 0.00001f,
                    "horizontal image frequency uses the correct Annex K quantizer");
        }
}

void impulsesIndependenceAndAliasing()
{
    deepfry::JpegCodec sharedCodec, otherCodec;
    Tile left {}, right {}, leftOutput {}, rightOutput {}, reference {};
    deepfry::TileFrame frame;
    for (int position = 0; position < deepfry::tileSize; ++position)
    {
        left.fill(0.0f);
        left[position] = 1.0f;
        sharedCodec.process(left, leftOutput, { 7.0f, 100.0f, 3 }, &frame);
        requireValid(leftOutput, frame);
        sharedCodec.process(right, rightOutput, { 7.0f, 100.0f, 3 }, &frame);
        for (float sample : rightOutput)
            require(sample == 0.0f, "one channel's impulse does not leak into another channel or tile");
    }
    for (std::size_t i = 0; i < right.size(); ++i)
        right[i] = static_cast<float>(0.6 * std::sin(i * 0.31));
    const deepfry::CodecSettings settings { 37.0f, 41.0f, 6 };
    sharedCodec.process(right, rightOutput, settings);
    otherCodec.process(right, reference, settings);
    require(rightOutput == reference, "codec instances and sequential channels produce identical results");
    sharedCodec.process(right, right, settings);
    require(right == reference, "in-place processing is supported");
}

void adversarialInputsAndSettings()
{
    deepfry::JpegCodec codec;
    Tile input {}, output {}, repeat {};
    deepfry::TileFrame frame;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 8> corrupt { nan, infinity, -infinity, 1.0e30f, -1.0e30f, 1.0f, -1.0f, 0.0f };
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = corrupt[i % corrupt.size()];
    for (float quality : { nan, infinity, -infinity, -1.0e30f, 1.0e30f, 0.0f, 0.5f })
        for (float fry : { nan, infinity, -infinity, -1.0e30f, 1.0e30f })
            for (int bits : { std::numeric_limits<int>::min(), -1, 0, 2, 8, std::numeric_limits<int>::max() })
            {
                const deepfry::CodecSettings settings { quality, fry, bits };
                codec.process(input, output, settings, &frame);
                requireValid(output, frame);
                codec.process(input, repeat, settings);
                require(output == repeat, "invalid settings are sanitized deterministically");
            }

    // The codec should preserve sign symmetry rather than introducing a
    // systematic grey-level offset from the unsigned JPEG representation.
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(0.7 * std::sin(i * 0.721));
    codec.process(input, output, { 29.0f, 65.0f, 5 });
    for (float& value : input)
        value = -value;
    codec.process(input, repeat, { 29.0f, 65.0f, 5 });
    for (std::size_t i = 0; i < output.size(); ++i)
        require(output[i] == -repeat[i], "signed pixel mapping and image effects preserve odd symmetry");
}
}

int main()
{
    silenceAndQuantization();
    qualityAndReconstruction();
    transformOrientationReference();
    impulsesIndependenceAndAliasing();
    adversarialInputsAndSettings();
    std::cout << "All " << checks << " codec checks passed.\n";
}
