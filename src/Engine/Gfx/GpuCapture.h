#pragma once

namespace summer::gfx {

// Best-effort integration with GPU capture tools (PIX and RenderDoc).
//
// PIX: loading WinPixGpuCapturer.dll before the D3D12 device is created is what lets PIX
//      attach to / launch a standalone build and take GPU captures.
// RenderDoc: when the app is launched through the RenderDoc UI, renderdoc.dll is injected;
//      we grab its in-application API so captures can be triggered programmatically.
//
// All functions are safe no-ops when the corresponding tool is not present.
class GpuCapture {
public:
    // Call as EARLY as possible in startup, BEFORE creating the D3D12 device.
    static void LoadCaptureTools();

    static bool IsPixLoaded();
    static bool IsRenderDocLoaded();

    // Associates the RenderDoc overlay/capture with a specific device + window.
    // Call once after the device and window exist. Parameters are ID3D12Device* and HWND.
    static void SetActiveWindow(void* d3d12Device, void* windowHandle);

    // Triggers a RenderDoc capture of the next presented frame (no-op without RenderDoc).
    static void TriggerRenderDocCapture();
};

} // namespace summer::gfx
