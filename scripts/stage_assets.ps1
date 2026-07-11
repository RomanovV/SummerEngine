param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

# Stages runtime resources (assets + compiled shaders) next to the built executable so the
# app can be launched standalone (e.g. by RenderDoc or PIX) regardless of working directory.

$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExeDir = Join-Path $RootDir "build\bin\$Config"
$ShaderSrc = Join-Path $RootDir "build\shaders\$Config"
$AssetSrc = Join-Path $RootDir "assets"

$ShaderDst = Join-Path $ExeDir "shaders"
$AssetDst = Join-Path $ExeDir "assets"

New-Item -ItemType Directory -Force -Path $ExeDir | Out-Null

if (Test-Path $ShaderSrc) {
    New-Item -ItemType Directory -Force -Path $ShaderDst | Out-Null
    Copy-Item -Path (Join-Path $ShaderSrc "*.cso") -Destination $ShaderDst -Force -ErrorAction SilentlyContinue
    # Stage shader PDBs too so PIX/RenderDoc can resolve shader debug symbols standalone.
    Copy-Item -Path (Join-Path $ShaderSrc "*.pdb") -Destination $ShaderDst -Force -ErrorAction SilentlyContinue
    Write-Host "Staged shaders (+pdb) -> $ShaderDst"
} else {
    Write-Warning "Shader output '$ShaderSrc' not found; run compile_shaders.ps1 first."
}

if (Test-Path $AssetSrc) {
    New-Item -ItemType Directory -Force -Path $AssetDst | Out-Null
    Copy-Item -Path (Join-Path $AssetSrc "*") -Destination $AssetDst -Recurse -Force
    Write-Host "Staged assets -> $AssetDst"
} else {
    Write-Warning "Asset source '$AssetSrc' not found."
}
