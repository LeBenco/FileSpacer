$payload=[Environment]::GetCommandLineArgs()|Select-Object -Skip 6
$json=ConvertTo-Json -InputObject @($payload) -Compress
[Console]::Out.WriteLine([Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($json)))
[Console]::Error.WriteLine('helper stderr')
exit 17
