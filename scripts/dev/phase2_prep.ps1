$src = 'D:\CSGO2019'
$dst = 'd:\source-engine'

# ---- 1) copy the three source trees into the repo (untracked for now) ----
$trees = @('panorama','panoramauiclient','panorama_s1wrapper')
foreach ($t in $trees) {
    $s = Join-Path $src $t
    $d = Join-Path $dst $t
    if (Test-Path $s) {
        if (Test-Path $d) { Write-Output "skip(exists): $t" }
        else {
            Copy-Item -Path $s -Destination $d -Recurse
            $n = @(Get-ChildItem -Path $d -Recurse -File -ErrorAction SilentlyContinue).Count
            Write-Output "copied: $t ($n files)"
        }
    } else { Write-Output "MISSING source: $t" }
}

# ---- 2) scan includes of the copied trees and find ones SE can't resolve ----
$root = $dst
$scanDirs = $trees | ForEach-Object { Join-Path $root $_ }
$files = $scanDirs | ForEach-Object { Get-ChildItem -Path $_ -Recurse -File -Include *.cpp,*.h,*.hpp -ErrorAction SilentlyContinue }
$incRe = '#include\s*[<"]([^">]+)[" >]'
$missing = @{}
$resolved = 0
foreach ($f in $files) {
    $dir = Split-Path $f.FullName -Parent
    $m = [regex]::Matches((Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue), $incRe)
    foreach ($x in $m) {
        $inc = $x.Groups[1].Value.Trim()
        if ($inc -match '^(\.\.|\.)/' -or $inc -match '\.\./') {
            # relative path -> resolve against including file's folder
            $cand = [System.IO.Path]::GetFullPath((Join-Path $dir $inc))
            if (Test-Path $cand) { $resolved++ }
            else { $missing[$inc] = ($missing[$inc] + 1) }
        } else {
            # framework-style absolute-ish include: check SE include roots
            $found = $false
            foreach ($rootDir in @((Join-Path $root 'public'), (Join-Path $root 'common'), $root, (Join-Path $root 'panorama'))) {
                $cand = Join-Path $rootDir $inc
                if (Test-Path $cand) { $found = $true; break }
            }
            if ($found) { $resolved++ }
            else { $missing[$inc] = ($missing[$inc] + 1) }
        }
    }
}
$report = 'd:\source-engine\docs\panorama_phase2_missing_includes.txt'
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("Panorama source include dependency scan (Phase 2 prep)")
$lines.Add("scanned files: $($files.Count)   resolved includes: $resolved   distinct unresolved: $($missing.Count)")
$lines.Add('')
$lines.Add('=== unresolved includes (count, path) ===')
$missing.GetEnumerator() | Sort-Object { $_.Value } -Descending | ForEach-Object { $lines.Add(("{0,5}  {1}" -f $_.Value, $_.Key)) }
$lines | Set-Content -Path $report -Encoding UTF8
Write-Output ("scanned files: " + $files.Count + "  resolved: " + $resolved + "  unresolved distinct: " + $missing.Count)
Write-Output ("report: " + $report)
