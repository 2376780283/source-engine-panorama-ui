param( [string]$Path, [int]$From, [int]$To )
# Print a line range with tabs/spaces made visible (tabs -> <TAB>, spaces -> .)

$raw = [System.IO.File]::ReadAllText( $Path )
$lines = $raw -split "`r`n|`n"
for ( $i = $From - 1; $i -lt [Math]::Min( $To, $lines.Count ); $i++ ) {
	$vis = $lines[ $i ].Replace( "`t", '<TAB>' ).Replace( ' ', '.' )
	Write-Output ( ( $i + 1 ).ToString().PadLeft( 5 ) + ': ' + $vis )
}
