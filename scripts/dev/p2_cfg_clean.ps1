Set-Location 'd:\source-engine'
& 'd:\source-engine\waf.bat' configure -T release --32bits --build-games=cstrike --prefix=build/out/ --disable-warns --enable-opus 2>&1 | Out-File -FilePath 'd:\source-engine\build\_p2cfg.txt' -Encoding utf8
Get-Content 'd:\source-engine\build\_p2cfg.txt' -Tail 3
