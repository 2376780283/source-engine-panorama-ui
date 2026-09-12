# Sets up / refreshes the runtime layout of the panorama test mod under build/out and launches it.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev\run-panorama-test.ps1 [-NoRun] [-Seconds 25]
#
# What it does:
#   1. copies the mod sources from mods/panorama_test into build/out/panorama_test (gameinfo.txt,
#      panorama/layout/*.xml - they are plain runtime data, nothing compiles them)
#   2. copies the freshly built game DLLs from build/out/cstrike/bin into the mod's own bin/ (Source
#      Engine loads client.dll / server.dll from the mod directory, and waf.bat install only refreshes
#      build/out/cstrike/bin)
#   3. copies icudtl.dat next to hl2_launcher.exe (the panorama UI runs v8, which looks for it there)
#   4. launches build/out/hl2_launcher.exe -game panorama_test, waits, kills it and copies engine.log
#      to build/_run_engine.log so the last run can always be inspected
param(
    [switch]$NoRun,
    [int]$Seconds = 25
)

$ErrorActionPreference = 'Stop'
$repo = 'd:\source-engine'
$out = Join-Path $repo 'build\out'
$mod = Join-Path $out 'panorama_test'

if (-not (Test-Path (Join-Path $out 'hl2_launcher.exe'))) {
    throw "build\out\hl2_launcher.exe not found - run 'waf.bat install' first."
}

Write-Output '=== 1) mod sources -> build/out/panorama_test ==='
$srcMod = Join-Path $repo 'mods\panorama_test'
if (Test-Path $srcMod) {
    Get-ChildItem $srcMod -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($srcMod.Length + 1)
        $dst = Join-Path $mod $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
        Copy-Item $_.FullName $dst -Force
        Write-Output ('  ' + $rel)
    }
}
else {
    Write-Output '  (mods\panorama_test missing - keeping what is already in build/out)'
}

Write-Output '=== 2) game DLLs -> mod bin/ ==='
$gameBin = Join-Path $out 'cstrike\bin'
if (Test-Path $gameBin) {
    New-Item -ItemType Directory -Force -Path (Join-Path $mod 'bin') | Out-Null
    foreach ($dll in 'client.dll', 'server.dll') {
        $s = Join-Path $gameBin $dll
        if (Test-Path $s) {
            Copy-Item $s (Join-Path $mod ('bin\' + $dll)) -Force
            Write-Output ('  ' + $dll + '  ' + (Get-Item $s).LastWriteTime)
        }
    }
}
else {
    Write-Output '  (build\out\cstrike\bin missing - run waf.bat install)'
}

Write-Output '=== 3) icudtl.dat ==='
$icuSrc = Join-Path $repo 'external\v8\icu\icudtl.dat'
foreach ($d in $out, (Join-Path $out 'bin')) {
    if (Test-Path $icuSrc) {
        Copy-Item $icuSrc (Join-Path $d 'icudtl.dat') -Force
        Write-Output ('  -> ' + $d)
    }
}

if ($NoRun) { Write-Output '=== -NoRun: not launching ==='; return }

Write-Output '=== 4) launching panorama_test ==='
Remove-Item (Join-Path $out 'engine.log') -ErrorAction SilentlyContinue
$args = @('-game', 'panorama_test', '-console', '-windowed', '-novid', '-nosound', '-insecure')
$p = Start-Process -FilePath (Join-Path $out 'hl2_launcher.exe') -WorkingDirectory $out -ArgumentList $args -PassThru
Write-Output ('  pid ' + $p.Id + ', waiting ' + $Seconds + 's')
Start-Sleep -Seconds $Seconds
Get-Process -Name 'hl2_launcher' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$log = Join-Path $out 'engine.log'
if (Test-Path $log) {
    Copy-Item $log (Join-Path $repo 'build\_run_engine.log') -Force
    Write-Output ('=== engine.log (' + (Get-Item $log).Length + ' bytes) copied to build\_run_engine.log ===')
    Get-Content $log -Encoding Default | Select-Object -Last 40 | ForEach-Object { Write-Output $_ }
}
else {
    Write-Output '=== engine.log was not produced ==='
}
