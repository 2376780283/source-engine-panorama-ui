$cs = 'D:\CSGO2019'
$se = 'd:\source-engine'
$items = @(
  'public\resourcesystem\iresourcesystem.h',
  'public\rendersystem\irenderdevice.h',
  'public\rendersystem\irendercontext.h',
  'public\materialsystem2\imaterialsystem2.h',
  'public\assetsystem\iassetsystem.h',
  'public\imemanager\iimemanager.h',
  'common\enumutils.h',
  'common\enumutils_panorama.h',
  'public\resourcefile\*.h',
  'public\resourcesystem',
  'public\rendersystem',
  'public\materialsystem2',
  'public\assetsystem',
  'imemanager'
)
foreach ($it in $items) {
  $c = Join-Path $cs $it
  $s = Join-Path $se $it
  Write-Output ("== " + $it)
  Write-Output ("   CSGO: " + (Test-Path $c))
  if (Test-Path $c) {
    $fi = Get-Item $c -ErrorAction SilentlyContinue
    if (-not $fi.PSIsContainer) { Write-Output ("        file size: " + $fi.Length) }
    else { Write-Output ("        dir files: " + @(Get-ChildItem $c -Recurse -File -EA SilentlyContinue).Count) }
  }
  Write-Output ("   SE  : " + (Test-Path $s))
}
# where does rendersystem/irenderdevice.h live in the copied s1wrapper?
Write-Output "== s1wrapper rendersystem (copied) =="
Get-ChildItem 'd:\source-engine\panorama_s1wrapper\rendersystem' -File -EA SilentlyContinue | Select-Object -ExpandProperty Name
# where is iimemanager.h in CSGO2019?
Write-Output "== find iimemanager.h in CSGO =="
Get-ChildItem $cs -Recurse -Filter iimemanager.h -EA SilentlyContinue | Select-Object -First 5 -ExpandProperty FullName
