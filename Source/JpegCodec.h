#pragma once

#include <array>

namespace deepfry
{
constexpr int tileSize = 64;

struct CodecSettings
{
    float quality = 35.0f; // JPEG quality: 1 (destroyed) .. 100 (minimum loss).
    float fry = 25.0f;    // Image contrast and four-neighbour sharpening: 0 .. 100.
    int pixelBits = 8;    // Zero-centred posterization: 2 .. 8 bits.
};

struct TileFrame
{
    // Actual luminance pixels: 128 is silence, 1/255 are negative/positive full scale.
    std::array<float, tileSize> before {}, after {};
    // Fraction of the 64 quantized DCT coefficients that remain nonzero.
    // This is a visual detail metric, not a compressed-file-size estimate.
    float retained = 1.0f;
};

/** The lossy image stages of grayscale baseline JPEG, applied to audio tiles.

    Consecutive samples become an 8 x 8 image in row-major order. The codec applies
    an orthonormal 2D DCT, the Annex K luminance quantization table with IJG quality
    scaling, and inverse DCT. Entropy coding and a JPEG file container are omitted:
    they are lossless stages and do not change the resulting audio.

    Unlike ordinary unsigned image posterization, our audio mapping uses signed
    pixel values -127 .. 127 around neutral 128. Symmetric quantizers reserve an
    exact zero at every bit depth, preventing a silent input from creating DC.
    There are 2^bits - 1 symmetric output levels (255 at eight bits).

    Construction precomputes the cosine basis. Processing has no allocations,
    locks, I/O, or trigonometry. The caller owns buffering and latency. Channels
    may share an instance sequentially: the transform has no signal history.
*/
class JpegCodec
{
public:
    JpegCodec() noexcept;

    void process(const std::array<float, tileSize>& input,
                 std::array<float, tileSize>& output,
                 const CodecSettings& settings,
                 TileFrame* visualization = nullptr) noexcept;

private:
    std::array<float, tileSize> basis {};
};
}
