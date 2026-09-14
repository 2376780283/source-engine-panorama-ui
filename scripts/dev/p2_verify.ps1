$log = 'd:\source-engine\build\_p2cfg.txt'
if (Test-Path $log) { Write-Output ("p2cfg size: " + (Get-Item $log).Length); Get-Content $log -Tail 4 } else { Write-Output 'p2cfg MISSING' }
$cache = 'd:\source-engine\build\c4che\_cache.py'
$hit = Select-String -Path $cache -Pattern 'panorama' -SimpleMatch -ErrorAction SilentlyContinue
Write-Output ('panorama in config: ' + $(if($hit){'YES'}else{'no'}))
Get-Content 'd:\source-engine\build\configuration.py' -ErrorAction SilentlyContinue | Select-String -Pattern "'GAMES'" | ForEach-Object { ($_.Line -split 'GAMES')[1].Substring(0,20) }
