$log = 'd:\source-engine\build\_panorama.log'
if (-not (Test-Path $log)) { Write-Output 'no log'; exit }
Write-Output ("log lines: " + @(Get-Content $log).Count)
Get-Content $log | Where-Object { $_ -match 'error C|fatal error|Build failed|Build completed|Linking|Waf: Leaving' } | Select-Object -Last 40 | ForEach-Object { $_ }
