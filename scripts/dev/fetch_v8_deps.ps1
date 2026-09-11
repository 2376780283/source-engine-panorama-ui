# fetch_v8_deps.ps1
#
# Fetches the prebuilt V8 artifacts required to link the Panorama JS host.
#
#   * headers + static libs : NuGet package v8_monolithic.windows-latest.<arch>.release
#   * icudtl.dat            : extracted from an Electron release zip of the same
#                             Chromium milestone (V8 7.3.492 == Chromium 73 == Electron 5)
#
# Nothing is committed to git (v8_monolith.lib is ~424 MB); this script recreates
# external/v8/{include,lib,icu}.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev\fetch_v8_deps.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev\fetch_v8_deps.ps1 -Arch x64
#
param(
    [ValidateSet('x86', 'x64')]
    [string]$Arch = 'x86',
    [string]$V8Version = '7.3.492',
    [string]$ElectronVersion = '5.0.13',
    [string]$ElectronZipArch = 'ia32',
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.IO.Compression.FileSystem

$cache = Join-Path $Root 'build\_v8deps'
New-Item -ItemType Directory -Force -Path $cache | Out-Null

function Get-File($url, $dest) {
    if (Test-Path $dest) { Write-Host ("cached  " + $dest); return }
    Write-Host ("fetch   " + $url)
    $tmp = "$dest.part"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $tmp -TimeoutSec 900
    Move-Item -Force $tmp $dest
}

# ---------- 1. v8 headers + static libs (NuGet) ----------
$pkgId = "v8_monolithic.windows-latest.$Arch.release"
$nupkg = Join-Path $cache "$pkgId.$V8Version.nupkg"
Get-File "https://api.nuget.org/v3-flatcontainer/$pkgId/$V8Version/$pkgId.$V8Version.nupkg" $nupkg

$extract = Join-Path $cache "extract-$Arch"
if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
Write-Host "expand  $nupkg"
[System.IO.Compression.ZipFile]::ExtractToDirectory($nupkg, $extract)

$inc = Join-Path $Root 'external\v8\include'
if (Test-Path $inc) { Remove-Item -Recurse -Force $inc }
New-Item -ItemType Directory -Force -Path $inc | Out-Null
Copy-Item -Recurse -Force (Join-Path $extract 'build\build\include\*') $inc
Write-Host ("headers -> " + $inc)

$libSrc = Join-Path $extract "build\build\native\lib\win\$Arch\win-$Arch-release"
$libDst = Join-Path $Root "external\v8\lib\win\$Arch"
New-Item -ItemType Directory -Force -Path $libDst | Out-Null
foreach ($l in @('v8_monolith.lib', 'v8_libbase.lib', 'v8_libplatform.lib')) {
    Copy-Item -Force (Join-Path $libSrc $l) (Join-Path $libDst $l)
    Write-Host ("lib     -> " + (Join-Path $libDst $l))
}

# ---------- 2. icudtl.dat (Electron of the matching Chromium milestone) ----------
$ezip = Join-Path $cache "electron-v$ElectronVersion-win32-$ElectronZipArch.zip"
Get-File "https://github.com/electron/electron/releases/download/v$ElectronVersion/electron-v$ElectronVersion-win32-$ElectronZipArch.zip" $ezip

$zip = [System.IO.Compression.ZipFile]::OpenRead($ezip)
try {
    $hit = $zip.Entries | Where-Object { $_.Name -eq 'icudtl.dat' } | Select-Object -First 1
    if (-not $hit) { throw "icudtl.dat not found inside $ezip" }
    $icuDst = Join-Path $Root 'external\v8\icu\icudtl.dat'
    New-Item -ItemType Directory -Force -Path (Split-Path $icuDst) | Out-Null
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($hit, $icuDst, $true)
    Write-Host ("icu     -> " + $icuDst + "  (" + [math]::Round((Get-Item $icuDst).Length / 1MB, 1) + " MB)")
} finally {
    $zip.Dispose()
}

Write-Host ''
Write-Host 'V8 dependencies installed. Remember: icudtl.dat must be deployed next to the'
Write-Host 'executable (the engine expects it at bin\icudtl.dat).'
