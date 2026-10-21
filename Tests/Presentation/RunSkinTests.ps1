param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $repoRoot ('build/skin-tests/' + [guid]::NewGuid().ToString('N') + '/deployment/game')
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent', 'LogoView', 'LogoLayout', 'LogoInput', 'LogoTexts',
    'MusicSelectView', 'SongSelectLayout', 'SongSelectPanels', 'SongSelectCards', 'SongSelectInput', 'SongSelectCreateMenu', 'EditorOsuImport', 'OsuImportValidation', 'MusicSelectTexts',
    'MarqueeTextComponent', 'OptionsPanel', 'OptionsDisplay', 'OptionsKeys', 'OptionsOffset', 'OptionsSongs', 'OptionSettings',
    'OptionTexts', 'TextCatalog', 'LanguageFile')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Assets', 'Modes', 'Chart', 'Rhythm', 'ClientLogic') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testExe = Join-Path $testRoot 'SkinTests.exe'
Push-Location $repoRoot
try {
    & cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Assets/Public `
        /IYOUMU.Modes /IYOUMU.Chart /IYOUMU.Rhythm /IDependencies/PhantomCore/Engine/SDK `
        Tests/Presentation/SkinTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
        "/Fo$testRoot/SkinTests.obj" "/Fe$testExe" /link /OPT:NOICF /OPT:NOREF `
        "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" `
        user32.lib gdi32.lib ole32.lib comdlg32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Skin test compilation failed.' }
    $fmod = if ($Configuration -eq 'Debug') { 'fmodL.dll' } else { 'fmod.dll' }
    Copy-Item -LiteralPath "bin/x64/$Configuration/$fmod" -Destination $testRoot
    Push-Location $testRoot
    try {
        & $testExe $repoRoot
        if ($LASTEXITCODE -ne 0) { throw 'Skin tests failed.' }
    } finally { Pop-Location }
} finally { Pop-Location }
