#include "JpegCodec.h"

#include <algorithm>
#include <cmath>

namespace deepfry
{
namespace
{
constexpr int side = 8;
constexpr float pixelScale = 127.0f;
constexpr float neutralPixel = 128.0f;

// ITU-T T.81, Annex K, Table K.1. IJG quality scaling is documented in:
// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/src/jcparam.c
constexpr std::array<int, tileSize> luminanceQuantization {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

float finiteClamped(float value, float minimum, float maximum, float fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}
}

JpegCodec::JpegCodec() noexcept
{
    constexpr double pi = 3.1415926535897932384626433832795;
    for (int frequency = 0; frequency < side; ++frequency)
    {
        const double normalization = frequency == 0 ? std::sqrt(1.0 / side) : 0.5;
        for (int position = 0; position < side; ++position)
            basis[frequency * side + position] = static_cast<float>(normalization
                * std::cos(pi * (2 * position + 1) * frequency / (2 * side)));
    }
}

void JpegCodec::process(const std::array<float, tileSize>& input,
                       std::array<float, tileSize>& output,
                       const CodecSettings& settings,
                       TileFrame* visualization) noexcept
{
    const int quality = static_cast<int>(std::round(finiteClamped(settings.quality, 1.0f, 100.0f, 35.0f)));
    const int qualityScale = quality < 50 ? 5000 / quality : 200 - quality * 2;
    const float fry = finiteClamped(settings.fry, 0.0f, 100.0f, 25.0f) * 0.01f;
    const int bits = std::clamp(settings.pixelBits, 2, 8);
    const float signedLevels = static_cast<float>((1 << (bits - 1)) - 1);

    std::array<float, tileSize> pixels {}, seasoned {}, temporary {}, coefficients {};

    // Make a real integer-luminance image. Sanitization also contains broken
    // upstream samples so NaN/Inf cannot reach the host or visualization.
    for (int i = 0; i < tileSize; ++i)
    {
        pixels[i] = std::round(finiteClamped(input[i], -1.0f, 1.0f, 0.0f) * pixelScale);
        if (visualization != nullptr)
            visualization->before[i] = neutralPixel + pixels[i];
    }

    // Meme-style contrast and an image-space four-neighbour unsharp mask.
    // Replicated edge pixels avoid inventing an artificial black image border.
    // Both operations are odd-symmetric: black/white excursions balance around
    // neutral grey, and zero pixels remain exactly zero at any Fry setting.
    for (int y = 0; y < side; ++y)
    {
        for (int x = 0; x < side; ++x)
        {
            const int i = y * side + x;
            const float neighbours = 0.25f * (
                pixels[y * side + std::max(0, x - 1)]
                + pixels[y * side + std::min(side - 1, x + 1)]
                + pixels[std::max(0, y - 1) * side + x]
                + pixels[std::min(side - 1, y + 1) * side + x]);
            const float contrast = pixels[i] * (1.0f + 3.0f * fry);
            const float sharpen = (pixels[i] - neighbours) * (2.0f * fry);
            seasoned[i] = std::round(std::clamp(contrast + sharpen, -pixelScale, pixelScale));
        }
    }

    // Separable forward 2D DCT: horizontal frequency, then vertical frequency.
    for (int y = 0; y < side; ++y)
        for (int u = 0; u < side; ++u)
        {
            float sum = 0.0f;
            for (int x = 0; x < side; ++x)
                sum += seasoned[y * side + x] * basis[u * side + x];
            temporary[y * side + u] = sum;
        }

    int retained = 0;
    for (int v = 0; v < side; ++v)
        for (int u = 0; u < side; ++u)
        {
            float sum = 0.0f;
            for (int y = 0; y < side; ++y)
                sum += temporary[y * side + u] * basis[v * side + y];
            const int i = v * side + u;
            const int quantizer = std::clamp((luminanceQuantization[i] * qualityScale + 50) / 100, 1, 255);
            const float quantized = std::round(sum / static_cast<float>(quantizer));
            retained += quantized != 0.0f ? 1 : 0;
            coefficients[i] = quantized * static_cast<float>(quantizer);
        }

    // Inverse transform uses the transpose of the orthonormal basis.
    for (int y = 0; y < side; ++y)
        for (int u = 0; u < side; ++u)
        {
            float sum = 0.0f;
            for (int v = 0; v < side; ++v)
                sum += coefficients[v * side + u] * basis[v * side + y];
            temporary[y * side + u] = sum;
        }

    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
        {
            float sum = 0.0f;
            for (int u = 0; u < side; ++u)
                sum += temporary[y * side + u] * basis[u * side + x];
            const int i = y * side + x;
            const float decodedPixel = std::round(std::clamp(sum, -pixelScale, pixelScale));
            const float sample = std::round(decodedPixel * signedLevels / pixelScale) / signedLevels;
            output[i] = sample;
            if (visualization != nullptr)
                visualization->after[i] = neutralPixel + sample * pixelScale;
        }

    if (visualization != nullptr)
        visualization->retained = static_cast<float>(retained) / static_cast<float>(tileSize);
}
}
