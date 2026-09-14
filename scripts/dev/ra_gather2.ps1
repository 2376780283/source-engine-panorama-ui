$cs = 'D:\CSGO2019'
function In-Tree([string]$dir, [string]$pat) {
    if (Test-Path $dir) {
        Get-ChildItem $dir -Recurse -File -Include *.h,*.cpp -EA SilentlyContinue |
            Select-String -Pattern $pat -EA SilentlyContinue |
            Select-Object -First 6 | ForEach-Object { $_.Path.Substring($cs.Length+1) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
    }
}
Write-Output '===== CRefCount in CSGO public/tier1 ====='
In-Tree (Join-Path $cs 'public\tier1') 'CRefCount'
Write-Output '===== CRefCount in CSGO public (top few) ====='
In-Tree (Join-Path $cs 'public') '^\s*(class|struct)\s+CRefCount\b'
Write-Output '===== currencyamount.h ====='
Get-ChildItem $cs -Recurse -Filter 'currencyamount.h' -EA SilentlyContinue | Select-Object -First 4 | ForEach-Object { $_.FullName }
Write-Output '===== ToQAngle in CSGO mathlib ====='
In-Tree (Join-Path $cs 'public\mathlib') 'ToQAngle'
Write-Output '===== GetIdentityMatrix in CSGO mathlib ====='
In-Tree (Join-Path $cs 'public\mathlib') 'GetIdentityMatrix'
Write-Output '===== memstack.h WillAllocSucceed CSGO ====='
$ms = Join-Path $cs 'public\tier1\memstack.h'
if (Test-Path $ms) { Select-String -Path $ms -Pattern 'WillAllocSucceed' | Select-Object -First 6 | ForEach-Object { $_.LineNumber.ToString()+': '+$_.Line.Trim() } } else { Write-Output 'no CSGO public/tier1/memstack.h' }
Write-Output '===== SE memstack.h WillAllocSucceed ====='
$se = 'd:\source-engine\public\tier1\memstack.h'
if (Test-Path $se) { $h=Select-String -Path $se -Pattern 'WillAllocSucceed'; if($h){$h|Select-Object -First 6|ForEach-Object{$_.LineNumber.ToString()+': '+$_.Line.Trim()}}else{'SE: none'} } else {'SE: no file'}
