param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'MusicSelectView', 'SongSelectLayout', 'SongSelectPanels', 'SongSelectCards', 'SongSelectInput',
    'MusicSelectTexts', 'MarqueeTextComponent', 'OptionsPanel', 'OptionsDisplay', 'OptionsKeys', 'OptionsOffset', 'OptionSettings', 'OptionTexts', 'TextCatalog', 'LanguageFile', 'SkinSetSelection')
$objects = $units | ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
$libraries = @('Modes', 'Chart', 'Rhythm', 'ClientLogic', 'Assets') | ForEach-Object { "bin/x64/$Configuration/FingerDrum.$_.lib" }
$testObjects = "build/obj/MusicSelectViewTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Modes /IFingerDrum.Chart /IFingerDrum.Rhythm /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/MusicSelectViewTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/MusicSelectViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$Configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'MusicSelect view test compilation failed.' }
& "./bin/x64/$Configuration/MusicSelectViewTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'MusicSelect view tests failed.' }
