Write-Output '== CSGO generichash.h MurmurHash2LowerCase =='
Get-Content 'D:\CSGO2019\public\tier1\generichash.h' -ErrorAction SilentlyContinue |
    Select-String -Pattern 'MurmurHash2LowerCase' |
    Select-Object -First 8 | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
Write-Output '== utlstringtoken.h line 91 context =='
Get-Content 'd:\source-engine\public\tier1\utlstringtoken.h' -ErrorAction SilentlyContinue |
    Select-Object -Skip 85 -First 12 | ForEach-Object { $_ }
