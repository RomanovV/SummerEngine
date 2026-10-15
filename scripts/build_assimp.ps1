# One-time (or after submodule update) Assimp build for SummerEngine.
# Assimp's official build system is CMake; we link the resulting static lib from Premake.

$ErrorActionPreference = "Stop"

$AssimpRoot = Join-Path $PSScriptRoot "..\vendor\assimp"
$BuildDir   = Join-Path $AssimpRoot "build"

Write-Host "Configuring Assimp..."
cmake -S $AssimpRoot -B $BuildDir -G "Visual Studio 18 2026" -A x64 `
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF `
    -DASSIMP_BUILD_TESTS=OFF `
    -DASSIMP_BUILD_SAMPLES=OFF `
    -DASSIMP_INSTALL=OFF `
    -DBUILD_SHARED_LIBS=OFF `
    -DASSIMP_WARNINGS_AS_ERRORS=OFF

Write-Host "Building Assimp (Debug)..."
cmake --build $BuildDir --config Debug --target assimp --parallel 8

Write-Host "Building Assimp (Release)..."
cmake --build $BuildDir --config Release --target assimp --parallel 8

Write-Host "Done. Libraries are in vendor/assimp/build/lib/"
