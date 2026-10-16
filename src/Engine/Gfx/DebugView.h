#pragma once

#include <cstdint>

namespace summer::gfx {

// Backbuffer visualization. Lit is the default deferred+tonemap path. G-buffer modes either
// replace the whole frame or (Overview) keep Lit in the center and tile the buffers around it.
enum class DebugView : uint32_t {
    Lit = 0,
    GBufferOverview,
    GBufferAlbedo,
    GBufferNormal,
    GBufferOcclusion,
    GBufferMetallic,
    GBufferRoughness,
    GBufferEmissive,
    GBufferDepth,
};

inline const char* DebugViewLabel(DebugView view) {
    switch (view) {
        case DebugView::Lit:
            return "Lit";
        case DebugView::GBufferOverview:
            return "Overview";
        case DebugView::GBufferAlbedo:
            return "Albedo";
        case DebugView::GBufferNormal:
            return "Normal";
        case DebugView::GBufferOcclusion:
            return "Occlusion";
        case DebugView::GBufferMetallic:
            return "Metallic";
        case DebugView::GBufferRoughness:
            return "Roughness";
        case DebugView::GBufferEmissive:
            return "Emissive";
        case DebugView::GBufferDepth:
            return "Depth";
        default:
            return "Unknown";
    }
}

// Overview tiles around a 5x5 grid. Center cell is Lit (drawn separately); tiles sit on the frame.
inline constexpr uint32_t kGBufferOverviewGrid = 5;
inline constexpr uint32_t kGBufferOverviewTileCount = 7;

struct DebugViewTile {
    DebugView view = DebugView::GBufferAlbedo;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

inline void FillGBufferOverviewTiles(uint32_t width, uint32_t height, DebugViewTile out[kGBufferOverviewTileCount]) {
    const float cellW = static_cast<float>(width) / static_cast<float>(kGBufferOverviewGrid);
    const float cellH = static_cast<float>(height) / static_cast<float>(kGBufferOverviewGrid);
    const struct {
        uint32_t col;
        uint32_t row;
        DebugView view;
    } cells[kGBufferOverviewTileCount] = {
        {0, 0, DebugView::GBufferAlbedo},
        {2, 0, DebugView::GBufferNormal},
        {4, 0, DebugView::GBufferOcclusion},
        {0, 2, DebugView::GBufferMetallic},
        {4, 2, DebugView::GBufferRoughness},
        {0, 4, DebugView::GBufferEmissive},
        {2, 4, DebugView::GBufferDepth},
    };
    for (uint32_t i = 0; i < kGBufferOverviewTileCount; ++i) {
        out[i].view = cells[i].view;
        out[i].x = static_cast<float>(cells[i].col) * cellW;
        out[i].y = static_cast<float>(cells[i].row) * cellH;
        out[i].width = cellW;
        out[i].height = cellH;
    }
}

} // namespace summer::gfx
