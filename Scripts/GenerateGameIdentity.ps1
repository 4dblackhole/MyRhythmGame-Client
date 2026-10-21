param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
[xml]$identity = Get-Content -LiteralPath $Source -Raw -Encoding UTF8
$name = [string]$identity.Project.PropertyGroup.GameDisplayName
if ([string]::IsNullOrWhiteSpace($name) -or $name.Contains("`r") -or $name.Contains("`n")) {
    throw 'GameDisplayName must be one nonempty line.'
}
$literal = $name.Replace('\', '\\').Replace('"', '\"')
$header = "#pragma once`n`n// Generated from GameIdentity.props by GenerateGameIdentity.ps1.`ninline constexpr wchar_t GameName[] = L`"$literal`";`n"
$destination = [IO.Path]::GetFullPath($Output)
if (-not [IO.File]::Exists($destination) -or [IO.File]::ReadAllText($destination) -cne $header) {
    [IO.File]::WriteAllText($destination, $header, [Text.UTF8Encoding]::new($false))
}
