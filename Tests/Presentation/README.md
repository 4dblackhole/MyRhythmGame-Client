# Lane key beam regression test

Build the Client solution first. From an x64 Visual Studio developer command
prompt in the repository root, compile the presentation test against the same
configuration's Client object and engine library (set `FMOD_ROOT` to the local SDK):

```bat
cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /IClient /IFingerDrum.Rhythm /IDependencies\MRG-Engine\Engine\SDK Tests\Presentation\LaneKeyBeamTests.cpp build\obj\MRG.Client\x64\Debug\LaneKeyBeam.obj build\obj\MRG.Client\x64\Debug\Pch.obj bin\x64\Debug\FingerDrum.Rhythm.lib Dependencies\MRG-Engine\bin\x64\Debug\MRG.Core.lib /Fobuild\obj\LaneKeyBeamTests.obj /Febin\x64\Debug\LaneKeyBeamTests.exe /link /OPT:NOICF /OPT:NOREF /LIBPATH:"%FMOD_ROOT%\api\core\lib\x64" user32.lib gdi32.lib ole32.lib
bin\x64\Debug\LaneKeyBeamTests.exe
```

For Release, replace `Debug` with `Release`, `/MDd /D_DEBUG` with `/MD /DNDEBUG`.
Include the Client `Pch.obj`: the production key-beam object uses its PCH.
Release may additionally pass `/LTCG` to the linker to avoid its automatic restart.
The test verifies free input, judgement colors/interpolation, input rejection,
roll ticks, missed-note rollover, alpha at 0/100/200ms, retrigger, sprite reuse,
inherited Lane rotation/scale, resize clipping, and reset. It does not initialize
an audio device or renderer; normal gameplay smoke covers resource loading.

## Editor audio view regression

After building both configurations, run this from PowerShell in an x64 Visual Studio
developer environment. Set `FMOD_ROOT` to the installed SDK and change `$configuration`
to `Release` with `$runtime = @('/MD', '/DNDEBUG')` for the other build.

```powershell
$configuration = 'Debug'
$runtime = @('/MDd', '/D_DEBUG')
[xml]$clientProject = Get-Content Client/MRG.Client.vcxproj
$objects = @($clientProject.Project.ItemGroup.ClCompile | Where-Object { $_.Include -match '^(EditorScene\\|Texts\\EditorScene\\)' } | ForEach-Object { "build/obj/MRG.Client/x64/$configuration/$([IO.Path]::GetFileNameWithoutExtension($_.Include)).obj" })
$testObjectDirectory = "build/obj/EditorPresentationTests/$configuration"
New-Item -ItemType Directory -Path $testObjectDirectory -Force | Out-Null
$objects += @('Pch', 'TextCatalog', 'SkinSetSelection', 'SongPreviewController') | ForEach-Object { "build/obj/MRG.Client/x64/$configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Editor', 'Assets') | ForEach-Object { "bin/x64/$configuration/FingerDrum.$_.lib" }
& cl /nologo /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IFingerDrum.Rhythm /IFingerDrum.Chart /IFingerDrum.Modes /IFingerDrum.Editor /IFingerDrum.Assets/Public /IDependencies/MRG-Engine/Engine/SDK Tests/Presentation/EditorAudioViewTests.cpp Tests/Presentation/EditorModeTests.cpp @objects @libraries "Dependencies/MRG-Engine/bin/x64/$configuration/MRG.Core.lib" "/Fo$testObjectDirectory/" "/Febin/x64/$configuration/EditorAudioViewTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/FingerDrum.Assets/x64/$configuration/FingerDrum.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view test build failed' }
& "./bin/x64/$configuration/EditorAudioViewTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Editor audio view tests failed' }
```

This checks the production view's draw packets for real song/hit spectra, negative
note/music time ranges, slider input actions, tab visibility, localization, resize,
and Canvas cleanup. `EditorModeTests.cpp` also checks Taiko tool placement, snapping,
deletion/cancellation and expected cues, then injects a test-only alternative mode
and document adapter to verify different tools/layout/coordinates/metadata/sound
targets, Save dispatch and marker revision caching through the common editor.
It also covers realtime lane boundaries, spaced/case-insensitive Buzz options,
Purple and timed/explicit sound agreement with play, invalid seek preservation,
effect/timing commands, failed edit rollback and drawing failure recovery.
The object list comes from the Client project so removed/stale objects are not linked.
Immutable rectangle batches are expanded only in the recording renderer for visual assertions.
It also checks actual asynchronous MP3 opening/playback with the FMOD NoSound output,
preview failure retry suppression/selection recovery/owned voice cleanup and
detaching a cancelled analysis job. Drawing assertions do not initialize a GPU;
this does not replace visual inspection, listening or mouse testing in the app.

## All-notes actual gameplay replay

After a full solution build, run from the repository root in an x64 Visual Studio
developer PowerShell with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Release
```

This standalone test loads the deployed AngelDream `All Notes Verification` YMP
and its actual MP3. It runs the engine for the entire chart using a QPC rhythm
timer, DSP-scheduled music, the production GameplayPresenter and audio router,
D3D12 rendering and a real audio backend (no NoSound fallback). Normal notes
receive perfect scripted inputs; dense Don/Kat Buzz heads receive +50ms GOOD
inputs. All 19 logical notes, exact-once ordered ticks and final accuracy must
match the logic replay. It also verifies deferred note-node creation, removing expired
subtrees and recreating them after reset at the first note time. Skin aliases share samples as in production. This test
has no game autoplay route and does not change options or user songs/skins.

The window is hidden and input timestamps are supplied directly to PlaySession.
This verifies real rendering/audio execution, not pixel appearance, hearing,
physical keyboard Raw Input or mouse interaction. The test requires Songs beside
its executable; it fails when the all-notes chart or music is absent.
