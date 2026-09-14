$cs = 'D:\CSGO2019'
Write-Output '===== CRefCount class def anywhere in CSGO ====='
Get-ChildItem $cs -Recurse -File -Include *.h -EA SilentlyContinue |
    Select-String -Pattern '^\s*(class|struct)\s+CRefCount\b' -EA SilentlyContinue |
    Select-Object -First 8 | ForEach-Object { $_.Path.Substring($cs.Length+1) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
Write-Output '===== currencyamount.h location ====='
Get-ChildItem $cs -Recurse -Filter 'currencyamount.h' -EA SilentlyContinue | Select-Object -First 4 | ForEach-Object { $_.FullName }
Write-Output '===== CSGO memstack WillAllocSucceed ====='
$ms = Get-ChildItem $cs -Recurse -Filter 'memstack.h' -EA SilentlyContinue | Select-Object -First 1
if ($ms) { Select-String -Path $ms.FullName -Pattern 'WillAllocSucceed|class CMemoryStack|MemAlloc|size_t' | Select-Object -First 12 | ForEach-Object { $_.LineNumber.ToString()+': '+$_.Line.Trim() } }
Write-Output '===== CSGO Quaternion::ToQAngle ====='
Get-ChildItem $cs -Recurse -File -Include *.h -EA SilentlyContinue |
    Select-String -Pattern 'ToQAngle' -EA SilentlyContinue |
    Select-Object -First 6 | ForEach-Object { $_.Path.Substring($cs.Length+1) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
Write-Output '===== CSGO VMatrix::GetIdentityMatrix ====='
Get-ChildItem $cs -Recurse -File -Include *.h -EA SilentlyContinue |
    Select-String -Pattern 'GetIdentityMatrix' -EA SilentlyContinue |
    Select-Object -First 6 | ForEach-Object { $_.Path.Substring($cs.Length+1) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
