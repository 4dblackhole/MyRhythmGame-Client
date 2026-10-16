param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'GameplaySessionController', 'GameplaySessionFactory', 'GameplayInput', 'GameplayAudio',
    'GameplayState', 'GameplayPause', 'GameplayPauseMenu', 'PauseTexts', 'GameplayAudioRouter', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent')
$objects = $units | ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Assets') | ForEach-Object { "bin/x64/$Configuration/FingerDrum.$_.lib" }
$objectDirectory = "build/obj/GameplayPauseTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IFingerDrum.Rhythm /IFingerDrum.Chart /IFingerDrum.Modes /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/GameplayPauseTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/GameplayPauseTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$Configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Pause test build failed.' }
& "./bin/x64/$Configuration/GameplayPauseTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Pause test failed.' }
