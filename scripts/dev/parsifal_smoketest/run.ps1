# run.ps1 - build and run the libparsifal (SE port) smoke test.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev\parsifal_smoketest\run.ps1
#
# Compiles ptest.cpp together with the parser implementation (both are
# dependency-free) into build\_parsifaltest\ptest.exe and runs it.
# Exit code 0 == every check passed.
param(
    [string]$Root = 'd:\source-engine',
    [string]$VsVcVars = 'E:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
)

$ErrorActionPreference = 'Stop'

$src = Join-Path $Root 'scripts\dev\parsifal_smoketest'
$out = Join-Path $Root 'build\_parsifaltest'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$inc1 = Join-Path $Root 'panorama\thirdparty\libparsifal-0.8.3\include'
$inc2 = Join-Path $inc1 'libparsifal'
$impl = Join-Path $Root 'panorama\thirdparty\libparsifal-0.8.3\src\parsifal.cpp'

$cmd = 'call "' + $VsVcVars + '" >nul 2>&1 && cl /nologo /W3 /MT /O2 /EHsc /TP /D_CRT_SECURE_NO_WARNINGS /I"' + $inc1 + '" /I"' + $inc2 + '" "' + (Join-Path $src 'ptest.cpp') + '" "' + $impl + '" /Fo:"' + $out + '\\" /Fe:"' + $out + '\ptest.exe"'

$output = & cmd /c $cmd 2>&1
$output | ForEach-Object { Write-Host $_ }
if ( $LASTEXITCODE -ne 0 ) {
    Write-Error "compile failed ($LASTEXITCODE)"
    exit 1
}

Push-Location $out
try {
    & .\ptest.exe
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}

exit $code
