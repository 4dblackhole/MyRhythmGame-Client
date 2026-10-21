param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT,
    [string]$SongFilter = '',
    [switch]$Effects
)
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot with the installed SDK path.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
if ($Effects) { $runtime += '/DTEST_YME_EFFECTS' }
$units = @('Pch', 'GameplayPresenter', 'GameplayLayout', 'GameplayNoteVisuals',
    'GameplayFeedbackVisuals', 'GameplayKeys', 'GameplayJudgementView', 'GameplayProgressView', 'LaneKeyBeam', 'GameplayTexts',
    'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent', 'GameplayAudioRouter')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$objectDirectory = "build/obj/AllNotesGameplayTests/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Editor /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/AllNotesGameplayTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/AllNotesGameplayTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'All-notes gameplay test build failed.' }
if ($SongFilter) { & "./bin/x64/$Configuration/AllNotesGameplayTests.exe" $SongFilter }
else { & "./bin/x64/$Configuration/AllNotesGameplayTests.exe" }
if ($LASTEXITCODE -ne 0) { throw 'All-notes gameplay test failed.' }
