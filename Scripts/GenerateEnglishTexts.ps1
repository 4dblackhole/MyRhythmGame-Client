param([Parameter(Mandatory=$true)][string]$Source, [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
# Embed the authoritative English INI verbatim. Runtime parsing is shared with
# external files, and no generated copy is written unless its content changed.
$encoding = [Text.UTF8Encoding]::new($false, $true)
$text = [IO.File]::ReadAllText([IO.Path]::GetFullPath($Source), $encoding)
if ($text.Contains(')MRG_ENGLISH"')) { throw 'English INI contains the reserved C++ raw-string delimiter.' }
$header = "#pragma once`n#include <string_view>`nnamespace finger_drum::texts {`ninline constexpr std::string_view EnglishDefaultIni = R`"MRG_ENGLISH($text)MRG_ENGLISH`";`n}`n"
$destination = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
if (-not [IO.File]::Exists($destination) -or [IO.File]::ReadAllText($destination) -ne $header) {
    [IO.File]::WriteAllText($destination, $header, $encoding)
}
