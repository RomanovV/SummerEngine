# SummerEngine Architecture

> Sandbox rendering engine for learning and experimenting with D3D12, shaders, and rendering techniques.
> This document reflects the codebase as of **Step 5** (clear color + ImGui).

---

## High-Level Overview

SummerEngine is split into two build targets:

| Target | Type | Role |
|---|---|---|
| **Engine** | Static library | All reusable boilerplate (platform, D3D12, ImGui) |
| **Sandbox** | Console executable | Thin entry point; hosts experiment UI and the main loop |

Third-party code lives in `vendor/` as git submodules. Build is driven by **Premake5** generating a VS 2022 solution.

```
SummerEngine/
├── premake5.lua          # Build configuration
├── docs/                   # Architecture and design notes
├── src/
│   ├── Engine/             # Static lib (summer:: / summer::gfx::)
│   └── Sandbox/            # main.cpp
├── shaders/                # HLSL (empty until mesh rendering step)
├── assets/models/          # Meshes via Assimp (future step)
└── vendor/                 # GLFW, ImGui, spdlog, stb (+ premake binary)
```

---

## Module Diagram

```mermaid
graph TB
    subgraph Sandbox["Sandbox (executable)"]
        Main["main.cpp<br/>main loop + ImGui UI"]
    end

    subgraph Engine["Engine (static lib)"]
        subgraph Core["Core"]
            Log["Log"]
        end

        subgraph Platform["Platform"]
            Window["Window<br/>(GLFW wrapper)"]
        end

        subgraph Gfx["Gfx"]
            Renderer["Renderer<br/>(orchestrator)"]
            Device["Device<br/>(D3D12 core)"]
            SwapChain["SwapChain<br/>(DXGI presentation)"]
            ImGuiLayer["ImGuiLayer<br/>(UI backends)"]
            Helpers["Helpers<br/>(HRESULT checks)"]
        end
    end

    subgraph Vendor["Vendor"]
        GLFW["GLFW"]
        ImGui["Dear ImGui"]
        SPDLOG["spdlog"]
    end

    Main --> Renderer
    Main --> Window
    Main --> Log

    Renderer --> Device
    Renderer --> SwapChain
    Renderer --> ImGuiLayer
    Renderer --> Window

    SwapChain --> Device
    ImGuiLayer --> Device
    ImGuiLayer --> SwapChain
    ImGuiLayer --> Window

    Window --> GLFW
    ImGuiLayer --> ImGui
    Log --> SPDLOG
    Device --> D3D12["D3D12 / DXGI"]
    SwapChain --> D3D12
```

---

## Namespace Layout

| Namespace | Modules | Purpose |
|---|---|---|
| `summer::` | `Log`, `Window` | Platform-agnostic engine utilities |
| `summer::gfx::` | `Device`, `SwapChain`, `Renderer`, `ImGuiLayer` | All D3D12 rendering code |

Keeping rendering under `gfx::` leaves room for future subsystems (`assets::`, `scene::`, etc.) without polluting the root namespace.

---

## Initialization Order

Startup sequence in `Sandbox/main.cpp`:

```
1. Log::Init()
2. Window creation          → GLFW window + HWND
3. Renderer::Initialize()
   ├── Device::Initialize()     → DXGI factory, D3D12 device, queue, fence
   ├── SwapChain::Initialize()  → DXGI swap chain bound to HWND, RTV heap
   └── ImGuiLayer::Initialize() → ImGui context, GLFW+DX12 backends, SRV heap
```

Shutdown is the reverse, with `Device::WaitForGpu()` first to drain in-flight frames.

---

## Per-Frame Flow

This is the most important loop to understand when adding rendering experiments.

```mermaid
sequenceDiagram
    participant Main as Sandbox/main.cpp
    participant Win as Window
    participant Rnd as Renderer
    participant UI as ImGuiLayer
    participant Dev as Device
    participant SC as SwapChain

    loop Every frame
        Main->>Win: PollEvents()
        alt Window resized
            Main->>Rnd: OnResize(w, h)
            Rnd->>Dev: WaitForGpu()
            Rnd->>SC: Resize()
        end

        Main->>Rnd: BeginImGuiFrame()
        Rnd->>UI: BeginFrame() (DX12 + GLFW NewFrame)
        Main->>Main: Build ImGui widgets

        Main->>Rnd: RenderFrame()
        Rnd->>SC: GetCurrentBackBufferIndex()
        Rnd->>Dev: WaitForBackBuffer(index)
        Rnd->>Dev: Reset allocator + command list
        Rnd->>Rnd: Transition PRESENT → RENDER_TARGET
        Rnd->>Rnd: ClearRenderTargetView
        Rnd->>UI: RenderDrawData (ImGui on top)
        Rnd->>Rnd: Transition RENDER_TARGET → PRESENT
        Rnd->>Dev: ExecuteCommandLists + Present + SignalBackBuffer
    end
```

### Where to hook your experiments

| What you want to do | Where to put it |
|---|---|
| UI toggles, debug values | `Sandbox/main.cpp` (between `BeginImGuiFrame` and `RenderFrame`) |
| New render passes / techniques | `Renderer::RenderFrame()` or new methods called from there |
| Shader / PSO experiments | New files under `Gfx/` + `shaders/` (future step) |
| Asset loading | Future `Assets/` module (Assimp, Step 6) |

---

## D3D12 Object Ownership

```mermaid
graph LR
    Device["Device"]
    Device --> Factory["IDXGIFactory4"]
    Device --> D3D["ID3D12Device"]
    Device --> Queue["ID3D12CommandQueue"]
    Device --> CmdList["ID3D12GraphicsCommandList"]
    Device --> Fence["ID3D12Fence"]
    Device --> Allocators["CommandAllocators[2]"]

    SwapChain["SwapChain"]
    SwapChain --> DXGI["IDXGISwapChain3"]
    SwapChain --> RTVHeap["RTV DescriptorHeap"]
    SwapChain --> BackBuffers["BackBuffer Resources[2]"]

    ImGuiLayer["ImGuiLayer"]
    ImGuiLayer --> SRVHeap["SRV DescriptorHeap (64 slots)"]
    ImGuiLayer --> ImGuiCtx["ImGui Context"]

    Renderer["Renderer"] --> Device
    Renderer --> SwapChain
    Renderer --> ImGuiLayer
```

### Key constants

- **`FrameCount = 2`** — double-buffered swap chain. Defined in `Device.h`, used by `SwapChain` and `ImGuiLayer`.
- **`SrvHeapCapacity = 64`** — shader-visible descriptor heap shared by ImGui and (eventually) engine textures.

---

## GPU Synchronization Model

D3D12 has no implicit sync. SummerEngine uses a **fence per back buffer**:

1. Before recording into back buffer `N`, check if the GPU finished the previous frame that used `N` (`WaitForBackBuffer`).
2. Record commands, execute, present.
3. Signal the fence with a monotonically increasing value (`SignalBackBuffer`).

Full GPU drain (`WaitForGpu`) is used only at shutdown and resize — expensive, but required before destroying or resizing GPU resources.

```
Back buffer 0: fence value 4  ← GPU still working
Back buffer 1: fence value 3  ← GPU done, safe to record
```

---

## Descriptor Heaps

D3D12 uses descriptor heaps instead of bind slots. SummerEngine currently has two:

| Heap | Type | Owner | Shader-visible? | Purpose |
|---|---|---|---|---|
| RTV heap | `D3D12_DESCRIPTOR_HEAP_TYPE_RTV` | `SwapChain` | No | Back buffer render target views |
| SRV heap | `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` | `ImGuiLayer` | **Yes** | ImGui font atlas + future engine textures |

> **Important:** Only one shader-visible heap can be bound per command list (`SetDescriptorHeaps`). When you add engine textures, allocate from the same SRV heap (or redesign heap management).

The SRV heap uses a simple free-list allocator (`ImGuiLayer::DescriptorAllocator`) because ImGui 1.92+ requires the application to provide alloc/free callbacks.

---

## Build System

```
premake5.lua
├── GLFW       (static lib, C)
├── ImGui      (static lib, C++ — core + glfw/dx12 backends)
├── Engine     (static lib, C++ — links GLFW + ImGui)
└── Sandbox    (console app, links Engine)
```

Generate and build:

```powershell
.\vendor\premake\premake5.exe vs2022
msbuild build\SummerEngine.sln /p:Configuration=Debug /p:Platform=x64
.\build\bin\Debug\Sandbox.exe
```

Debug builds define `SUMMER_DEBUG`, which enables the D3D12 debug layer.

---

## Vendor Dependencies

| Library | Submodule path | Used for |
|---|---|---|
| GLFW 3.4 | `vendor/glfw` | Window, input, Win32 HWND |
| Dear ImGui | `vendor/imgui` | Debug UI, experiment controls |
| spdlog | `vendor/spdlog` | Logging (header-only) |
| stb | `vendor/stb` | Image loading (future step) |

Clone with submodules:

```powershell
git submodule update --init --recursive
```

---

## Planned Next Steps

| Step | What | New modules / files |
|---|---|---|
| 6 | Assimp mesh loading | `Assets/ModelLoader` |
| 7 | GPU mesh + draw call | `Gfx/Mesh`, `Gfx/Shader`, `shaders/BasicVS.hlsl` |
| — | Depth buffer | Extend `SwapChain` + `Renderer` |
| — | DXC shader build step | `premake5.lua` prebuild command |

---

## File Quick Reference

| File | Read this when… |
|---|---|
| `Sandbox/main.cpp` | You want to add UI or change the main loop |
| `Gfx/Renderer.cpp` | You want to add render passes or change frame flow |
| `Gfx/Device.cpp` | You need to understand GPU sync or add D3D12 objects |
| `Gfx/SwapChain.cpp` | You need to change presentation or add depth/stencil |
| `Gfx/ImGuiLayer.cpp` | You need to debug UI rendering or add texture SRVs |
| `Platform/Window.cpp` | You need input handling or window behavior changes |
| `premake5.lua` | You add source files, libs, or shader build steps |
