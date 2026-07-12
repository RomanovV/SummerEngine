#include "Assets/Image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_ONLY_HDR
#define STBI_ONLY_PSD
#include <stb_image.h>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace summer::assets {

bool Image::LoadFromFile(const std::string& path, Image& outImage, bool flipY) {
    outImage = {};

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channelsInFile, STBI_rgb_alpha);
    if (!data) {
        spdlog::error("Failed to load image '{}': {}", path, stbi_failure_reason());
        return false;
    }

    outImage.width = static_cast<uint32_t>(width);
    outImage.height = static_cast<uint32_t>(height);
    outImage.pixels.assign(data, data + static_cast<size_t>(width) * height * kChannels);
    stbi_image_free(data);

    spdlog::info("Loaded image '{}' ({}x{}, {} src channels)", path, width, height, channelsInFile);
    return true;
}

Image Image::MakeCheckerboard(uint32_t size, uint32_t checks, uint32_t colorA, uint32_t colorB) {
    Image image;
    image.width = size;
    image.height = size;
    image.pixels.resize(static_cast<size_t>(size) * size * kChannels);

    const uint32_t cellSize = std::max<uint32_t>(1, size / std::max<uint32_t>(1, checks));

    auto writePixel = [](uint8_t* dst, uint32_t rgba) {
        // colorX is packed 0xAABBGGRR to match RGBA byte order in memory.
        dst[0] = static_cast<uint8_t>(rgba & 0xFF);
        dst[1] = static_cast<uint8_t>((rgba >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((rgba >> 16) & 0xFF);
        dst[3] = static_cast<uint8_t>((rgba >> 24) & 0xFF);
    };

    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool checker = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            uint8_t* dst = &image.pixels[(static_cast<size_t>(y) * size + x) * kChannels];
            writePixel(dst, checker ? colorA : colorB);
        }
    }

    return image;
}

Image Image::MakeSolid(uint32_t rgba, uint32_t size) {
    Image image;
    image.width = std::max<uint32_t>(1, size);
    image.height = image.width;
    image.pixels.resize(static_cast<size_t>(image.width) * image.height * kChannels);

    for (size_t i = 0; i < image.pixels.size(); i += kChannels) {
        image.pixels[i + 0] = static_cast<uint8_t>(rgba & 0xFF);
        image.pixels[i + 1] = static_cast<uint8_t>((rgba >> 8) & 0xFF);
        image.pixels[i + 2] = static_cast<uint8_t>((rgba >> 16) & 0xFF);
        image.pixels[i + 3] = static_cast<uint8_t>((rgba >> 24) & 0xFF);
    }

    return image;
}

std::vector<Image> Image::GenerateMipChain(const Image& base) {
    std::vector<Image> mips;
    if (!base.IsValid()) {
        return mips;
    }

    mips.push_back(base);

    while (mips.back().width > 1 || mips.back().height > 1) {
        const Image& src = mips.back();
        Image dst;
        dst.width = std::max<uint32_t>(1, src.width / 2);
        dst.height = std::max<uint32_t>(1, src.height / 2);
        dst.pixels.resize(static_cast<size_t>(dst.width) * dst.height * kChannels);

        for (uint32_t y = 0; y < dst.height; ++y) {
            for (uint32_t x = 0; x < dst.width; ++x) {
                // Average up to a 2x2 block from the source (clamped at edges for odd sizes).
                const uint32_t sx0 = std::min(x * 2, src.width - 1);
                const uint32_t sy0 = std::min(y * 2, src.height - 1);
                const uint32_t sx1 = std::min(sx0 + 1, src.width - 1);
                const uint32_t sy1 = std::min(sy0 + 1, src.height - 1);

                const uint8_t* p00 = &src.pixels[(static_cast<size_t>(sy0) * src.width + sx0) * kChannels];
                const uint8_t* p10 = &src.pixels[(static_cast<size_t>(sy0) * src.width + sx1) * kChannels];
                const uint8_t* p01 = &src.pixels[(static_cast<size_t>(sy1) * src.width + sx0) * kChannels];
                const uint8_t* p11 = &src.pixels[(static_cast<size_t>(sy1) * src.width + sx1) * kChannels];

                uint8_t* out = &dst.pixels[(static_cast<size_t>(y) * dst.width + x) * kChannels];
                for (uint32_t c = 0; c < kChannels; ++c) {
                    const uint32_t sum = static_cast<uint32_t>(p00[c]) + p10[c] + p01[c] + p11[c];
                    out[c] = static_cast<uint8_t>(sum / 4);
                }
            }
        }

        mips.push_back(std::move(dst));
    }

    return mips;
}

} // namespace summer::assets
