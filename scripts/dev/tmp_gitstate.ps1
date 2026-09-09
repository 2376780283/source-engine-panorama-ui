$lines = & 'D:\Git\cmd\git.exe' -C 'd:\source-engine' status --porcelain
$m = @($lines | Where-Object { $_ -match '^ M |^M ' }).Count
$unt = @($lines | Where-Object { $_ -match '^\?\?' }).Count
$add = @($lines | Where-Object { $_ -match '^A ' }).Count
Write-Output ("total changed lines: " + $lines.Count)
Write-Output ("modified(M): " + $m + "   added-staged(A): " + $add + "   untracked(??): " + $unt)
Write-Output '--- modified files (tracked, top) ---'
$lines | Where-Object { $_ -match '^ M |^M ' } | ForEach-Object { $_ } | Select-Object -First 40
Write-Output '--- untracked dirs sample ---'
$lines | Where-Object { $_ -match '^\?\?' } | ForEach-Object { ($_ -replace '^\?\? ','') } | Select-Object -First 25
