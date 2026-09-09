Write-Output '== CSGO: where g_pAsyncFileSystem declared/defined =='
Get-ChildItem 'D:\CSGO2019\public' -Recurse -File -Include *.h,*.cpp -ErrorAction SilentlyContinue |
    Select-String -Pattern 'g_pAsyncFileSystem' -ErrorAction SilentlyContinue |
    Select-Object -First 12 | ForEach-Object { $_.Path.Substring('D:\CSGO2019\'.Length) + ':' + $_.LineNumber + ': ' + $_.Line.Trim() }
Write-Output '== CSGO public/tier1/utlstringtoken.h exists? =='
Write-Output (Test-Path 'D:\CSGO2019\public\tier1\utlstringtoken.h')
Write-Output '== head of CSGO filesystem/iasyncfilesystem.h (1..30) =='
Get-Content 'D:\CSGO2019\public\filesystem\iasyncfilesystem.h' -TotalCount 30 -ErrorAction SilentlyContinue
