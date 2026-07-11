param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$ShaderDir = Join-Path $RootDir "shaders"
$OutDir = Join-Path $RootDir "build\shaders\$Config"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$dxc = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\dxc.exe" |
    Sort-Object { [version]($_.Directory.Parent.Name) } -Descending |
    Select-Object -First 1

if (-not $dxc) {
    throw "DXC not found. Install the Windows SDK with HLSL tools."
}

Write-Host "Using DXC: $($dxc.FullName)"
Write-Host "Output: $OutDir"

$shaders = @(
    @{ Name = "BasicVS"; Type = "vs_6_0" },
    @{ Name = "BasicPS"; Type = "ps_6_0" }
)

foreach ($shader in $shaders) {
    $inputPath = Join-Path $ShaderDir "$($shader.Name).hlsl"
    $outputPath = Join-Path $OutDir "$($shader.Name).cso"

    & $dxc.FullName `
        -T $shader.Type `
        -E main `
        -Fo $outputPath `
        $inputPath

    if ($LASTEXITCODE -ne 0) {
        throw "DXC failed for $($shader.Name)"
    }

    Write-Host "Compiled $($shader.Name) -> $outputPath"
}
