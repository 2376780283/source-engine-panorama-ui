param(
    [Parameter(Mandatory = $true)][string]$Pattern,
    [string[]]$Roots = @('public', 'tier0', 'tier1', 'tier2', 'common', 'panorama', 'vstdlib'),
    [string]$Base = 'D:\CSGO2019',
    [int]$Max = 40,
    [string]$Out = 'd:\source-engine\build\_findsym.txt'
)
$o = New-Object System.Collections.Generic.List[string]
foreach ($r in $Roots) {
    $dir = Join-Path $Base $r
    if (-not (Test-Path $dir)) { continue }
    $files = Get-ChildItem -Path $dir -Recurse -Include '*.h', '*.cpp', '*.inl' -File -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $m = Select-String -Path $f.FullName -Pattern $Pattern -ErrorAction SilentlyContinue
        foreach ($x in $m) {
            $o.Add($x.Path.Replace($Base + '\', '') + ':' + $x.LineNumber + ': ' + $x.Line.Trim())
            if ($o.Count -ge $Max) { break }
        }
        if ($o.Count -ge $Max) { break }
    }
    if ($o.Count -ge $Max) { break }
}
$o | Set-Content $Out -Encoding UTF8
Write-Output ('hits=' + $o.Count)
