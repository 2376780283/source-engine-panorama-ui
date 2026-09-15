param( [string]$LogPath )
# Extract the unique list of unresolved externals from a MSVC link log (LNK2001/LNK2019),
# together with how many object files referenced each one.

$lines = Get-Content $LogPath -Encoding Default
$syms = @{}
foreach ( $line in $lines ) {
	if ( $line -match 'LNK200[19]' ) {
		$start = $line.IndexOf( '"' )
		if ( $start -ge 0 ) {
			$end = $line.IndexOf( '"', $start + 1 )
			if ( $end -gt $start ) {
				$sym = $line.Substring( $start + 1, $end - $start - 1 )
				if ( $syms.ContainsKey( $sym ) ) { $syms[$sym]++ } else { $syms[$sym] = 1 }
			}
		}
	}
}

Write-Output ( 'unique unresolved symbols = ' + $syms.Count )
$syms.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object {
	Write-Output ( $_.Value.ToString().PadLeft( 4 ) + '  ' + $_.Key )
}
