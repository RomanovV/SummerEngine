#include "Gfx/GpuCapture.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <renderdoc_app.h>
#include <spdlog/spdlog.h>

#include <string>

namespace summer::gfx {

namespace {

HMODULE g_pixModule = nullptr;
RENDERDOC_API_1_6_0* g_renderDocApi = nullptr;

// Finds the newest installed WinPixGpuCapturer.dll under "%ProgramFiles%\Microsoft PIX\<version>\".
// This mirrors what PIXLoadLatestWinPixGpuCapturerLibrary() does, without needing the PIX headers.
std::wstring FindLatestWinPixGpuCapturer() {
    wchar_t programFiles[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH) == 0) {
        return {};
    }

    const std::wstring pixRoot = std::wstring(programFiles) + L"\\Microsoft PIX\\";
    const std::wstring searchPattern = pixRoot + L"*";

    WIN32_FIND_DATAW findData{};
    HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::wstring newest;
    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        const std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        const std::wstring candidate = pixRoot + name + L"\\WinPixGpuCapturer.dll";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
            // Version folders sort lexicographically well enough for picking the newest.
            if (candidate > newest) {
                newest = candidate;
            }
        }
    } while (FindNextFileW(findHandle, &findData));

    FindClose(findHandle);
    return newest;
}

void LoadPix() {
    // If PIX launched us (or already attached), the capturer is already in the process.
    if (GetModuleHandleW(L"WinPixGpuCapturer.dll") != nullptr) {
        g_pixModule = GetModuleHandleW(L"WinPixGpuCapturer.dll");
        spdlog::info("PIX GPU capturer already present in process");
        return;
    }

    const std::wstring capturerPath = FindLatestWinPixGpuCapturer();
    if (capturerPath.empty()) {
        spdlog::info("PIX not found (WinPixGpuCapturer.dll); PIX GPU captures unavailable");
        return;
    }

    g_pixModule = LoadLibraryW(capturerPath.c_str());
    if (g_pixModule) {
        spdlog::info("Loaded PIX GPU capturer; PIX can now attach for GPU captures");
    } else {
        spdlog::warn("Found PIX capturer but failed to load it (error {})", GetLastError());
    }
}

void LoadRenderDoc() {
    // RenderDoc only injects renderdoc.dll when the app is launched/attached through its UI.
    HMODULE renderDocModule = GetModuleHandleW(L"renderdoc.dll");
    if (!renderDocModule) {
        return;
    }

    auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(
        GetProcAddress(renderDocModule, "RENDERDOC_GetAPI"));
    if (!getApi) {
        spdlog::warn("renderdoc.dll present but RENDERDOC_GetAPI not found");
        return;
    }

    if (getApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&g_renderDocApi)) == 1 &&
        g_renderDocApi != nullptr) {
        int major = 0;
        int minor = 0;
        int patch = 0;
        g_renderDocApi->GetAPIVersion(&major, &minor, &patch);
        spdlog::info("RenderDoc API {}.{}.{} connected", major, minor, patch);
    } else {
        g_renderDocApi = nullptr;
        spdlog::warn("Failed to obtain RenderDoc API");
    }
}

} // namespace

void GpuCapture::LoadCaptureTools() {
    LoadPix();
    LoadRenderDoc();
}

bool GpuCapture::IsPixLoaded() {
    return g_pixModule != nullptr;
}

bool GpuCapture::IsRenderDocLoaded() {
    return g_renderDocApi != nullptr;
}

void GpuCapture::SetActiveWindow(void* d3d12Device, void* windowHandle) {
    if (g_renderDocApi && d3d12Device && windowHandle) {
        g_renderDocApi->SetActiveWindow(d3d12Device, windowHandle);
    }
}

void GpuCapture::TriggerRenderDocCapture() {
    if (g_renderDocApi) {
        g_renderDocApi->TriggerCapture();
        spdlog::info("RenderDoc: queued capture of next frame");
    }
}

} // namespace summer::gfx
