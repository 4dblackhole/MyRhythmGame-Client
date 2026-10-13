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

## Music selection viewport and catalog worker

Run `./Tests/Presentation/RunMusicSelectViewTests.ps1 -Configuration Debug`
and the same command with Release from the developer PowerShell.
The test links production view/state objects, checks that 5,000 in-memory songs
create only viewport cards across focus/search/resize, and polls real catalog
loads/cache reuse/error results. It writes no song or option files.
It also checks marquee Unicode preservation, shared repeated text payloads,
pixel displacement and stopping after the label becomes wide enough.

## Gameplay judgement guide and overlays

After a solution build, run in an x64 Visual Studio developer PowerShell with
`FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Debug
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Release
```

The test links the production Presenter/JudgementView/key-beam objects and reads
real fallback PNG metadata. It checks independent band colors, JudgeLevel-based
lengths, fixed base windows during syncopation, +/-10ms margins, early/late/zero
and clamped markers, all seven overlays, MISS without invented input timing,
POOR only on red-beam rejection, out-of-range/release/unjudged-tick suppression,
200ms fade/retrigger/reset, inherited circle centre, resize, node/image reuse,
and accepted-hit priority over a passive MISS.
It checks coincident note/measure centres under Whole and Separate at several times and viewport sizes,
and all four count-only roll heads for suppressed timing feedback, white beams and OK/MISS completion.
Both DengDeng starting colors cover repeated-action rejection with a stable count/next action,
no POOR/timing marker, white beams, Reset and successful OK completion.
It also verifies displaying an automation diagnostic in the shared error HUD and hiding it when cleared.
The same executable tests the production gameplay progress view: last long-tail
duration, pre/post-roll clamping, fill UV and marker position, current/total time,
the shared RhythmTimer's pause/resume, Reset, four viewport sizes, compiled
BPM/delay timing, empty/zero/negative duration and constant node/image reuse.
The all-notes actual D3D12/FMOD replay additionally checks that progress advances
during real playback and reaches 100% by the final note/tail.
The judgement/progress geometry executable uses MSVC `/analyze`; it starts no GPU/audio device.
Automated geometry checks do not replace actual screen/input/listening inspection.

## Allocation failure boundary regression

After a solution build, run in an x64 Visual Studio developer PowerShell:

```powershell
./Tests/Presentation/RunFailureBoundaryTests.ps1 -Configuration Debug
./Tests/Presentation/RunFailureBoundaryTests.ps1 -Configuration Release
```

The standalone test replaces C++ allocation only inside its own single-threaded
process. It checks that the numeric parser returns false when its temporary
string cannot be allocated and that asset initialization returns false with
empty error text when storing its error also fails. It deliberately omits the
RCDATA pack, so initialization fails before installing or reading the user's cache.
The test source is compiled with MSVC `/analyze`; this is not a game startup route.

## Editor audio view regression

After building both configurations, run in an x64 Visual Studio developer PowerShell.
The script obtains the editor object list from the Client project and compiles the four
test translation units with MSVC Code Analysis. Set `FMOD_ROOT` for a custom SDK install.

```powershell
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Debug
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Release
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
Custom Separate curves also cover visible slow-note drawing/deletion and grid snapping after removing the slow note.
`EditorMenuTests.cpp` checks the Save-only Canvas menu, Korean/English and dirty
labels, pointer mapping at four viewport sizes, one-shot mouse/keyboard Save,
Escape/outside/right-click dismissal, input consumption and observer cleanup.
The composed view checks menu/footer coexistence and menu resizing. These are
synthetic menu intents and draw geometry, not physical Raw Input or manual UI tests.
The object list comes from the Client project so removed/stale objects are not linked.
Immutable rectangle batches are expanded only in the recording renderer for visual assertions.
It also checks actual asynchronous MP3 opening/playback with the FMOD NoSound output,
accurate music duration/near-end seeking, editor DSP transport, pause/resume/negative pre-roll,
dense cue deadlines, alias/voice reuse, expired-cue skipping and cleanup through EditorPlaybackTests.
EditorModeTests checks time zoom at five scales, stable lane/head dimensions and zoomed snapping,
unchanged chart timing/revision and consecutive Z/X, last-tail/music-end destinations,
missing/stale duration rejection, retry and manual-seek/direction reset behavior.
preview failure retry suppression/selection recovery/owned voice cleanup and
detaching a cancelled analysis job. Drawing assertions do not initialize a GPU;
this does not replace visual inspection, listening or mouse testing in the app.

## Options panel and Option.ini regression

After building both configurations, run in an x64 Visual Studio developer PowerShell
with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Debug
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Release
```

This links the production panel/settings objects. It checks UTF-8 settings round trips,
invalid values, all three audio ComboBoxes, sparse driver indices, output-scoped lists,
failed switches and rollback after a read-only INI blocks saving. Test files are isolated
in a unique temporary directory; the user's Option.ini and skins are not changed.
The ASIO transition/failure cases use a test backend. A separate actual FMOD check
switches Automatic to WASAPI and selects the current driver while retaining a loaded
sample. It does not play sound or replace manual mouse/listening/real ASIO-device checks.

## All-notes actual gameplay replay

After a full solution build, run from the repository root in an x64 Visual Studio
developer PowerShell with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Release
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug -Effects
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Release -Effects
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
physical keyboard Raw Input or mouse interaction. The test uses the production
asset-root resolver (Client/Assets during development, Asset beside deployed copies);
it fails when the all-notes chart or music is absent.

`-Effects` compiles this test with `TEST_YME_EFFECTS` and loads AngelDream
`YME Effects Verification` instead. It is a test-runner option, not a game CLI route.
The explicit YMP reference resolves its paired YME in the same difficulty directory, including its relative
sound table. The same real engine/presenter/router run plays all 15 logical notes
with Whole/Separate curves, hit-sound changes/volume and syncopation ranges.
The dense Buzz head is deliberately late: 2 of 37 body ticks must fail and the
remaining 35 succeed once. Final scripted accuracy is 97.8897%.
YmeEffectsTests separately checks every expanded judgement boundary, exclusions,
all interpolation types, position continuity, sound precedence and Kiai ON/OFF state.
This remains hidden-window automation; it does not verify audible volume differences
or the visual appearance of motion.

## Development and deployment asset paths

After a solution build, run from an x64 Visual Studio developer PowerShell:

```powershell
./Tests/Presentation/RunAssetPathTests.ps1 -Configuration Debug
./Tests/Presentation/RunAssetPathTests.ps1 -Configuration Release
```

This links the actual asset library and OptionSettings object. The same binary is
copied to isolated development/deployment directory trees and launched with CWD
pointing to the opposite tree. It checks external Songs/default-skin paths and
that Option.ini stays beside the EXE. Fixtures stay under ignored build output;
no user settings, songs, skins or embedded cache are changed. The test source
uses MSVC Code Analysis. Catalog tests separately check the AngelDream YMP/YME pairs.

## Result graphs and actual Scene transition

After a solution build, run in an x64 Visual Studio developer PowerShell with `FMOD_ROOT`:

```powershell
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Debug
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Release
```

This links the production Result Scene/view/factory/graphs and gameplay objects.
It checks six-decimal accuracy text, Korean/English strings, signed scatter axes,
histogram counts, empty samples, narrow/wide layout, pointer-router return actions,
and one immutable graph batch for 10,003 samples. It then runs D3D12/FMOD with a
hidden window and a one-note regression session: the production gameplay Scene
waits three seconds after automatic judgement, moves its result to the lazy Result
factory, and destroys its own Canvas. Leaving Result destroys its Canvas too.
The test source uses MSVC Code Analysis and does not write user options/songs/skins.
This is automation, not a manual pixel/physical keyboard/mouse/listening check.
ESC routing is connected in production; its interrupted scoring and tied-us input
order are covered by domain tests, without claiming a physical ESC test.
