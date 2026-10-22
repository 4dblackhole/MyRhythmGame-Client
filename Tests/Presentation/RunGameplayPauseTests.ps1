param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'GameplaySessionController', 'GameplaySessionFactory', 'GameplayInput', 'GameplayAudio',
    'GameplayState', 'GameplayPause', 'GameplayPauseMenu', 'GameplayPauseLayout', 'PauseTexts', 'GameplayAudioRouter', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent',
    'RhythmTestScene', 'GameplayPresenter', 'GameplayLayout', 'GameplayNoteVisuals', 'GameplayFeedbackVisuals',
    'GameplayKeys', 'GameplayJudgementView', 'GameplayProgressView', 'LaneKeyBeam', 'GameplayTexts')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$objectDirectory = "build/obj/GameplayPauseTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/GameplayPauseTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/GameplayPauseTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Pause test build failed.' }
& "./bin/x64/$Configuration/GameplayPauseTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Pause test failed.' }
& "./bin/x64/$Configuration/GameplayPauseTests.exe" --cold-start
if ($LASTEXITCODE -ne 0) { throw 'Cold-start input test failed.' }
