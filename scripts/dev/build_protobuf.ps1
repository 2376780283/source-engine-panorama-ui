# build_protobuf.ps1
#
# Builds libprotobuf.lib + protoc.exe (protobuf 2.6.1, Windows x86, /MT) straight out of
# thirdparty/protobuf-2.6.1, without touching that submodule.
#
#   * protoc.exe        -> generate panorama/*.pb.{h,cc} (the in-repo gcsdk/bin/protoc.exe is
#                          2.3.0 and its output is rejected by the 2.6.1 headers)
#   * libprotobuf.lib   -> runtime needed when the panorama libs finally link
#
# Both land in build/protobuf/ (git-ignored build tree); re-run any time.
param(
    [string]$Root = 'd:\source-engine',
    [string]$VsVcVars = 'E:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
)

$ErrorActionPreference = 'Stop'
$o = New-Object System.Collections.Generic.List[string]

$src = Join-Path $Root 'thirdparty\protobuf-2.6.1\src'
$out = Join-Path $Root 'build\protobuf'
$objDir = Join-Path $out 'obj'
$compObjDir = Join-Path $out 'objc'
$libOut = Join-Path $out 'libprotobuf.lib'
New-Item -ItemType Directory -Force -Path $out | Out-Null

# ---- source lists (note: -Include needs -Recurse, use -Filter for flat dirs) ---------
$libFiles = Get-ChildItem (Join-Path $src 'google\protobuf') -Recurse -File -Filter '*.cc' |
    Where-Object {
        $_.FullName -notmatch '\\compiler\\' -and
        $_.FullName -notmatch '\\testdata\\' -and
        $_.FullName -notmatch '\\testing\\' -and
        $_.Name -notmatch 'test'
    } | Sort-Object FullName

$compFiles = Get-ChildItem (Join-Path $src 'google\protobuf\compiler') -Recurse -File -Filter '*.cc' |
    Where-Object { $_.Name -ne 'main.cc' -and $_.Name -notmatch 'unittest' -and $_.Name -notmatch 'mock' -and $_.Name -notmatch 'test_plugin' } | Sort-Object FullName

$mainFile = Join-Path $src 'google\protobuf\compiler\main.cc'
$o.Add('lib sources      : ' + $libFiles.Count)
$o.Add('compiler sources : ' + $compFiles.Count)
$o.Add('main.cc          : ' + (Test-Path $mainFile))
if ($libFiles.Count -eq 0 -or $compFiles.Count -eq 0 -or -not (Test-Path $mainFile)) { throw 'source discovery failed' }

$libRsp = Join-Path $out 'lib.rsp'
$compRsp = Join-Path $out 'comp.rsp'
($libFiles | ForEach-Object { '"' + $_.FullName + '"' }) | Set-Content $libRsp -Encoding ASCII
($compFiles | ForEach-Object { '"' + $_.FullName + '"' }) | Set-Content $compRsp -Encoding ASCII

$defs = '/nologo /c /MP /O2 /MT /EHsc /W0 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /DWIN32 /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS /I"' + $src + '" /I"' + (Join-Path $src '..\vsprojects') + '"'

function Run-In([string]$workDir, [string]$clArgs) {
    $prev = Get-Location
    Set-Location $workDir
    try {
        $r = & cmd /c ('call "' + $VsVcVars + '" >nul 2>&1 && ' + $clArgs + ' 2>&1')
        return @{ code = $LASTEXITCODE; out = $r }
    } finally {
        Set-Location $prev
    }
}

# ---- 1. libprotobuf objects + lib ---------------------------------------------------
if (Test-Path $libOut) {
    $o.Add('libprotobuf.lib already present - skipping library stage')
} else {
    if (Test-Path $objDir) { Remove-Item -Recurse -Force $objDir }
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    $o.Add('--- compiling libprotobuf (' + $libFiles.Count + ' files) ---')
    $r = Run-In $objDir ('cl ' + $defs + ' @' + $libRsp)
    $o.Add('compile exit=' + $r.code)
    $r.out | Where-Object { $_ -match 'error' } | Select-Object -First 20 | ForEach-Object { $o.Add([string]$_) }
    if ($r.code -ne 0) {
        $o | Set-Content (Join-Path $Root 'build\_protobuf_build.txt') -Encoding UTF8
        Write-Output 'FAILED (see build\_protobuf_build.txt)'
        exit 1
    }

    $objs = Get-ChildItem $objDir -Filter '*.obj' | ForEach-Object { '"' + $_.FullName + '"' }
    $o.Add('objects: ' + $objs.Count)
    $r = Run-In $out ('lib.exe /nologo /OUT:"' + $libOut + '" ' + ($objs -join ' '))
    $o.Add('lib exit=' + $r.code)
    $r.out | Where-Object { $_ -match 'error' } | Select-Object -First 10 | ForEach-Object { $o.Add([string]$_) }
}

# ---- 2. protoc ----------------------------------------------------------------------
if (Test-Path $compObjDir) { Remove-Item -Recurse -Force $compObjDir }
New-Item -ItemType Directory -Force -Path $compObjDir | Out-Null
$o.Add('--- compiling protoc (' + ($compFiles.Count + 1) + ' files) ---')
$r = Run-In $compObjDir ('cl ' + $defs + ' @' + $compRsp + ' "' + $mainFile + '"')
$o.Add('compile exit=' + $r.code)
$r.out | Where-Object { $_ -match 'error' } | Select-Object -First 20 | ForEach-Object { $o.Add([string]$_) }
if ($r.code -eq 0) {
    $compObjs = Get-ChildItem $compObjDir -Filter '*.obj' | ForEach-Object { '"' + $_.FullName + '"' }
    $protocOut = Join-Path $out 'protoc.exe'
    $r2 = Run-In $out ('cl /nologo /MT /O2 /EHsc /Fe:"' + $protocOut + '" ' + ($compObjs -join ' ') + ' /link /LIBPATH:"' + $out + '" libprotobuf.lib')
    $o.Add('protoc link exit=' + $r2.code)
    $r2.out | Select-Object -First 15 | ForEach-Object { $o.Add([string]$_) }
}

foreach ($f in @($libOut, (Join-Path $out 'protoc.exe'))) {
    if (Test-Path $f) { $o.Add('OK  ' + $f + '  ' + (Get-Item $f).Length) } else { $o.Add('MISSING ' + $f) }
}
$o | Set-Content (Join-Path $Root 'build\_protobuf_build.txt') -Encoding UTF8
Write-Output 'done'
