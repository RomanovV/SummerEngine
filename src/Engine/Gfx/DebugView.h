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

// Overview tiles around a 5x5 grid. Center is Lit (drawn separately). Tiles pack the first row,
// then the left/right of each middle row (col 0 then col max), then the last row — no gaps
// between views on the first or last row.
inline constexpr uint32_t kGBufferOverviewGrid = 5;
inline constexpr uint32_t kGBufferOverviewTileCount = 7;

inline constexpr DebugView kGBufferOverviewViews[kGBufferOverviewTileCount] = {
    DebugView::GBufferAlbedo,
    DebugView::GBufferNormal,
    DebugView::GBufferOcclusion,
    DebugView::GBufferMetallic,
    DebugView::GBufferRoughness,
    DebugView::GBufferEmissive,
    DebugView::GBufferDepth,
};

struct DebugViewTile {
    DebugView view = DebugView::GBufferAlbedo;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

inline void FillGBufferOverviewTiles(uint32_t width, uint32_t height, DebugViewTile out[kGBufferOverviewTileCount]) {
    const uint32_t grid = kGBufferOverviewGrid;
    const uint32_t count = kGBufferOverviewTileCount;
    const float cellW = static_cast<float>(width) / static_cast<float>(grid);
    const float cellH = static_cast<float>(height) / static_cast<float>(grid);

    uint32_t placed = 0;
    auto place = [&](uint32_t col, uint32_t row) {
        out[placed].view = kGBufferOverviewViews[placed];
        out[placed].x = static_cast<float>(col) * cellW;
        out[placed].y = static_cast<float>(row) * cellH;
        out[placed].width = cellW;
        out[placed].height = cellH;
        ++placed;
    };

    // First row, packed left to right.
    for (uint32_t col = 0; col < grid && placed < count; ++col) {
        place(col, 0);
    }
    if (placed >= count || grid < 2) {
        return;
    }

    const uint32_t lastRow = grid - 1;
    const uint32_t leftover = count - placed;
    // Keep the last row packed: extras beyond one full last row walk the middle sides first.
    const uint32_t lastRowTiles = leftover > grid ? grid : leftover;
    const uint32_t sideTiles = leftover > grid ? leftover - grid : 0;

    // Middle rows: (row, 0) then (row, max), i.e. 1,0 -> 1,max, until the last row.
    uint32_t sidesPlaced = 0;
    for (uint32_t row = 1; row < lastRow && sidesPlaced < sideTiles && placed < count; ++row) {
        place(0, row);
        ++sidesPlaced;
        if (sidesPlaced < sideTiles && placed < count) {
            place(grid - 1, row);
            ++sidesPlaced;
        }
    }

    for (uint32_t col = 0; col < lastRowTiles && placed < count; ++col) {
        place(col, lastRow);
    }
}

} // namespace summer::gfx
