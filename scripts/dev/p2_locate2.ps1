$cs = 'D:\CSGO2019'
foreach ($name in @('imaterialsystem2.h','iassetsystem.h','enumutils.h')) {
    Write-Output ("== " + $name)
    Get-ChildItem $cs -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 6 | ForEach-Object { Write-Output ("   " + $_.FullName) }
}
# materialsystem2 references: which CSGO file includes materialsystem2/imaterialsystem2.h ?
Write-Output '== files including materialsystem2/imaterialsystem2.h in CSGO panorama=='
Get-ChildItem (Join-Path $cs 'panorama') -Recurse -File -Include *.cpp,*.h -EA SilentlyContinue |
    Select-String -List -Pattern 'materialsystem2/imaterialsystem2.h' -EA SilentlyContinue |
    ForEach-Object { $_.Path } | Select-Object -First 10
Write-Output '== s1wrapper include of materialsystem2/imaterialsystem2.h? =='
Get-ChildItem (Join-Path $cs 'panorama_s1wrapper') -Recurse -File -Include *.cpp,*.h -EA SilentlyContinue |
    Select-String -List -Pattern 'materialsystem2/imaterialsystem2.h' -EA SilentlyContinue |
    ForEach-Object { $_.Path } | Select-Object -First 10
