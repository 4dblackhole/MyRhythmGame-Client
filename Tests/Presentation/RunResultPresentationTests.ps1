param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot with the installed SDK path.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'TaikoResultOptions', 'ResultScene', 'ResultViewFactory', 'TaikoResultView', 'TaikoResultGraphs', 'ResultTexts',
    'TaikoResultPanels', 'TaikoResultGraphView',
    'RhythmTestScene', 'GameplaySessionController', 'GameplaySessionFactory', 'GameplayInput', 'GameplayAudio',
    'GameplayState', 'GameplayPause', 'GameplayPauseMenu', 'GameplayPauseLayout', 'PauseTexts',
    'GameplayPresenter', 'GameplayLayout', 'GameplayNoteVisuals', 'GameplayFeedbackVisuals', 'GameplayKeys',
    'GameplayJudgementView', 'GameplayProgressView', 'LaneKeyBeam', 'GameplayTexts', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent', 'GameplayAudioRouter')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'ClientLogic', 'Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$objectDirectory = "build/obj/ResultPresentationTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/ResultPresentationTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/ResultPresentationTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Result presentation test build failed.' }
& "./bin/x64/$Configuration/ResultPresentationTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Result presentation test failed.' }
