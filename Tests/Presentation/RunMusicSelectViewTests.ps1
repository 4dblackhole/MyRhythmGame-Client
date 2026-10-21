param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'MusicSelectView', 'SongSelectLayout', 'SongSelectPanels', 'SongSelectCards', 'SongSelectInput', 'SongSelectCreateMenu', 'EditorOsuImport', 'OsuImportValidation',
    'MusicSelectTexts', 'MarqueeTextComponent', 'OptionsPanel', 'OptionsDisplay', 'OptionsKeys', 'OptionsOffset', 'OptionsSongs', 'OptionSettings', 'OptionTexts', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Modes', 'Chart', 'Rhythm', 'ClientLogic', 'Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testObjects = "build/obj/MusicSelectViewTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Modes /IYOUMU.Chart /IYOUMU.Rhythm /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/MusicSelectViewTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/MusicSelectViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib comdlg32.lib
if ($LASTEXITCODE -ne 0) { throw 'MusicSelect view test compilation failed.' }
& "./bin/x64/$Configuration/MusicSelectViewTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'MusicSelect view tests failed.' }
