$cs = 'D:\CSGO2019'
$se = 'd:\source-engine'
$log = 'd:\source-engine\docs\panorama_phase2_l1_copied.txt'
$done = New-Object System.Collections.Generic.List[string]

function Copy-Tree([string]$src, [string]$dst) {
    if (-not (Test-Path $dst)) {
        Copy-Item -Path $src -Destination $dst -Recurse
        $done.Add("TREE $src  ->  $dst  (" + @(Get-ChildItem $dst -Recurse -File -EA SilentlyContinue).Count + " files)")
    } else { $done.Add("SKIP(exists) $dst") }
}
function Copy-File([string]$src, [string]$dst) {
    if (Test-Path $src) {
        $dd = Split-Path $dst -Parent
        if (-not (Test-Path $dd)) { New-Item -ItemType Directory -Path $dd -Force | Out-Null }
        Copy-Item $src $dst -Force
        $done.Add("FILE $src  ->  $dst")
    } else { $done.Add("MISSING-SRC $src") }
}

# L1: interface trees from CSGO public
Copy-Tree (Join-Path $cs 'public\rendersystem')   (Join-Path $se 'public\rendersystem')
Copy-Tree (Join-Path $cs 'public\resourcesystem') (Join-Path $se 'public\resourcesystem')
Copy-Tree (Join-Path $cs 'public\resourcefile')   (Join-Path $se 'public\resourcefile')

# L1: standalone headers
Copy-File (Join-Path $cs 'public\iimemanager.h')          (Join-Path $se 'public\iimemanager.h')
Copy-File (Join-Path $cs 'common\enumutils_panorama.h')   (Join-Path $se 'common\enumutils_panorama.h')
Copy-File (Join-Path $cs 'public\gcsdk\enumutils.h')      (Join-Path $se 'public\gcsdk\enumutils.h')
Copy-File (Join-Path $cs 'public\gcsdk\enumutils.h')      (Join-Path $se 'common\enumutils.h')   # relocate: plain 'enumutils.h' include resolves via common/ dir

# v8 mirror into thirdparty submodule working tree (needed by SOURCE2 hardcoded path)
$v8dir = Join-Path $se 'thirdparty\v8'
if (-not (Test-Path (Join-Path $v8dir 'include\v8.h'))) {
    New-Item -ItemType Directory -Path $v8dir -Force | Out-Null
    Copy-Item -Path (Join-Path $se 'external\v8\include') -Destination (Join-Path $v8dir 'include') -Recurse -Force
    $done.Add("V8 mirror external/v8/include -> thirdparty/v8/include")
}

$done | Set-Content -Path $log -Encoding UTF8
$done | ForEach-Object { Write-Output $_ }
