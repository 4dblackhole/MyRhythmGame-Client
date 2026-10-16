param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
# Every literal UI key must have an English default in the authoritative INI.
$defaults = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$section = ''
foreach ($line in Get-Content Client/Texts/Languages/en.ini -Encoding utf8) {
    if ($line -match '^\[([^\]]+)\]$') { $section = $Matches[1] }
    elseif ($line -match '^([A-Za-z0-9_.-]+)=') { [void]$defaults.Add("$section.$($Matches[1])") }
}
foreach ($folder in 'App','EditorScene','GameScene','Presentation','Texts') {
    foreach ($source in Get-ChildItem "Client/$folder" -Recurse -File | Where-Object Extension -In '.cpp','.h') {
        foreach ($key in [regex]::Matches([IO.File]::ReadAllText($source.FullName), '(?:\.|->)(?:Get|Format)\("([^"]+)"')) {
            if (-not $defaults.Contains($key.Groups[1].Value)) { throw "Missing English UI key: $($key.Groups[1].Value) in $($source.FullName)" }
        }
    }
}
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
$units = @('Pch','TextCatalog','LanguageFile','LogoTexts','OptionTexts','EditorTexts','TaikoEditorTexts',
    'MusicSelectTexts','GameplayTexts','PauseTexts','ResultTexts')
$objects = $units | ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
$directory = "build/obj/LanguageFileTests/$Configuration"
New-Item -ItemType Directory -Force $directory | Out-Null
& cl /nologo /W4 /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IDependencies/MRG-Engine/Engine/SDK `
    Tests/Presentation/LanguageFileTests.cpp @objects "bin/x64/$Configuration/FingerDrum.Assets.lib" `
    "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" "/Fo$directory/" "/Febin/x64/$Configuration/LanguageFileTests.exe" `
    /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'Language-file test build failed.' }
& "bin/x64/$Configuration/LanguageFileTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Language-file test failed.' }
