# Copies the panorama localization token files into the directory the {localization} named path
# resolves to.
#
# CLocalization::BLoadLocalizationFile( "<prefix>" ) composes
#     "<{localization}>/<prefix>_<language>.txt"
# and {localization} comes out of the mod's panorama/panorama.cfg.  For this content that file is
# Valve's own and points at <mod>/panorama/localization, while the token files ship in
# <mod>/resource - so without this copy every "#token" in the CS:GO layouts logs
#     **** Unable to localize '#...' on panel '...'
#
# A prefix that walks out of {localization} does NOT work: CLocalization::ConstructLocalizationFilePath()
# runs the composed path through V_FixupPathName(), which collapses the ".." segments instead of
# resolving them (so "../../resource/csgo" ends up as "resource/csgo" relative to nothing useful).
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/dev/stage_panorama_localization.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/dev/stage_panorama_localization.ps1 -Only csgo_schinese.txt
param(
    [string]$Mod  = 'D:\cstrike\cstrike',
    [string]$Only = ''
)

$srcDir = Join-Path $Mod 'resource'
$dstDir = Join-Path $Mod 'panorama\localization'

if ( -not (Test-Path $srcDir) ) { Write-Output ('MISSING ' + $srcDir); exit 1 }
if ( -not (Test-Path $dstDir) ) {
    New-Item -ItemType Directory -Path $dstDir | Out-Null
    Write-Output ('created ' + $dstDir)
}

$filters = @('csgo_*.txt', 'cstrike_*.txt')
if ( $Only ) { $filters = @($Only) }

$copied = 0
foreach ( $f in $filters ) {
    foreach ( $file in (Get-ChildItem $srcDir -Filter $f -File -ErrorAction SilentlyContinue) ) {
        $dst = Join-Path $dstDir $file.Name
        if ( (Test-Path $dst) -and ((Get-Item $dst).Length -eq $file.Length) ) { continue }
        Copy-Item $file.FullName $dst -Force
        $copied++
    }
}

$count = @(Get-ChildItem $dstDir -Filter '*.txt' -File).Count
Write-Output ('{0} file(s) copied, {1} token file(s) now in {2}' -f $copied, $count, $dstDir)
Get-ChildItem $dstDir -Filter '*_english.txt' -File |
    ForEach-Object { '  ' + $_.Name + '  ' + [int]($_.Length / 1024) + ' KB' }
