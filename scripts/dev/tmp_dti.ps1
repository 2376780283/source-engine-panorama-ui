Write-Output '== CSGO DECLARE_TIER2_INTERFACE definition =='
Get-ChildItem 'D:\CSGO2019\public' -Recurse -File -Filter *.h -ErrorAction SilentlyContinue |
    Select-String -Pattern '#define\s+DECLARE_TIER2_INTERFACE' -ErrorAction SilentlyContinue |
    Select-Object -First 5 | ForEach-Object { $_.Path.Substring('D:\CSGO2019\'.Length) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
Write-Output '== search broader (tier2/tier2.h?) =='
$t = 'D:\CSGO2019\public\tier2\tier2.h'
if (Test-Path $t) { Select-String -Path $t -Pattern 'DECLARE_TIER2_INTERFACE|TIER2_INTERFACE' | Select-Object -First 10 | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() } }
Write-Output '== SE tier2/tier2.h interface macros present? =='
$s = 'd:\source-engine\public\tier2\tier2.h'
if (Test-Path $s) { Select-String -Path $s -Pattern 'INTERFACE|extern ' | Select-Object -First 10 | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() } } else { 'no SE tier2/tier2.h' }
