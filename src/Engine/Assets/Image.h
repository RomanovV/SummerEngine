#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace summer::assets {

// CPU-side image, always stored as tightly-packed 8-bit RGBA (4 channels).
// This is the hand-off point between file/procedural sources and GPU texture upload.
struct Image {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;

    static constexpr uint32_t kChannels = 4;

    bool IsValid() const { return !pixels.empty() && width > 0 && height > 0; }
    uint32_t RowPitch() const { return width * kChannels; }

    // Loads any stb-supported format (PNG/JPG/TGA/BMP/...) and expands to RGBA8.
    // flipY is useful when a source uses bottom-up UVs.
    static bool LoadFromFile(const std::string& path, Image& outImage, bool flipY = false);

    // Procedural RGBA checkerboard, handy as a default/placeholder base color texture.
    static Image MakeCheckerboard(
        uint32_t size = 256,
        uint32_t checks = 8,
        uint32_t colorA = 0xFFFFFFFFu,
        uint32_t colorB = 0xFF303030u);

    // Builds a full mip chain (including the base level at index 0) via box filtering,
    // down to 1x1. Returns an empty vector if the base image is invalid.
    static std::vector<Image> GenerateMipChain(const Image& base);
};

} // namespace summer::assets
