param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot with the installed SDK path.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'GameplayPresenter', 'GameplayLayout', 'GameplayNoteVisuals',
    'GameplayFeedbackVisuals', 'GameplayKeys', 'LaneKeyBeam', 'GameplayTexts',
    'TextCatalog', 'SkinSetSelection', 'GameplayAudioRouter')
$objects = $units | ForEach-Object { "build/obj/MRG.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Assets') | ForEach-Object { "bin/x64/$Configuration/FingerDrum.$_.lib" }
$objectDirectory = "build/obj/AllNotesGameplayTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Rhythm /IFingerDrum.Chart /IFingerDrum.Modes /IFingerDrum.Editor /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/AllNotesGameplayTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$Configuration/MRG.Core.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/AllNotesGameplayTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$Configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'All-notes gameplay test build failed.' }
& "./bin/x64/$Configuration/AllNotesGameplayTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'All-notes gameplay test failed.' }
