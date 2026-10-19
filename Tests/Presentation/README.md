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
Both gameplay and editor selection check one-shot launch from a difficulty click,
including a different difficulty and a second click on the selected difficulty.
It also checks marquee Unicode preservation, shared repeated text payloads,
pixel displacement and stopping after the label becomes wide enough.
Options-button input closes an already open panel. Catalog refreshes queue only
the latest Songs root, discard obsolete results and handle switching back mid-load.

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
Timing markers retain the latest 32 hits in a fixed ring; up to eight evicted
markers fade for 200ms. The test checks overflow, lifetime, resizing and node reuse.
Key-overlay flashes share the Don/Kat red/blue palette. Background tests use real
PNG metadata to verify aspect-preserving cover and 2D offsets at four viewports.
It moves the shared ScrollGear parent and checks that every descendant moves together while
the screen HUD stays fixed, and that the outer gear bottom remains on the centre line after resizing.
It checks chronological note stacking across Separate speed changes and backward seeks, with notes
in front of the judgement circle and its overlays. Counted rolls, big rolls, balloons and DengDeng also
verify remaining-hit visibility/decrement, extra-hit clamping, completion hiding and reset.
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

## Gameplay pause menu and transport

After a solution build, run in an x64 developer PowerShell with `FMOD_ROOT` set:

```powershell
./Tests/Presentation/RunGameplayPauseTests.ps1 -Configuration Debug
./Tests/Presentation/RunGameplayPauseTests.ps1 -Configuration Release
```

The test links production pause/menu/controller/audio objects and uses MSVC `/analyze`.
It verifies Penpot row dimensions/gaps, keyboard focus, bounded adjustments, pointer Resume/Retry/Retire,
selector clicks that only change values, language changes,
portrait/wide resizing, preparation gating and owned Canvas cleanup. Hidden D3D12
execution checks clock freezing, Resume preserving grades, four-measure Rewind with
meter/BPM changes, score restoration, Retry eligibility, Retire MISS counts and control logs.
A clip backend probe verifies source seek positions and DSP scheduling before chart zero;
a 40ms async-seek probe rejects early unpause and verifies frozen time, one-shot commands,
QPC/DSP lead alignment, no hit-sound replay, failure handling and Retry resource reuse.
A test-only second mode uses the same IPlaySessionController lifecycle and polymorphic result;
Ending transfers its result exactly once. A separate real FMOD NoSound MP3 test repeats
source-zero/positive/near-end seeks and checks the actual source end.
PlayPauseTests also covers rewind inside Buzz, repeated cancellation, preserved input traces,
and forced zero-score MISS for all seven long-note types. These are automated intents and
rendering/audio checks, not physical ESC/arrow-key testing, audible listening or manual screen review.

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

## External UTF-8 language files

After building the Client, run `RunLanguageFileTests.ps1 -Configuration Debug` and `Release`.
The runner checks that literal UI keys have authoritative English defaults, then compiles
with `/W4 /analyze` and exercises BOM/Unicode/escapes/invalid UTF-8, entry isolation,
English fallback without either external file, partial English/Korean packs, malformed
format arguments, valid argument reordering, immutable string-view ownership and replacement
on the next catalog construction. Fixtures stay in ignored build folders; user languages
and Option.ini are not edited. Existing screen regressions cover the injected catalog
through the actual menu/option/editor/result/gameplay views.

## Editor audio view regression

After building both configurations, run in an x64 Visual Studio developer PowerShell.
The script obtains the editor object list from the Client project and compiles the editor
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
The committed Ogg Vorbis tone fixture also runs through the real analysis worker
and audio tab. The test checks visible waveform/music FFT/hit FFT rectangles;
no installed Vorbis codec or FFmpeg is required. Domain audio tests additionally
check duration, frequency/level, opposite stereo phase, Unicode/renamed paths,
bounded cancellation and truncated-stream errors.
Audio range tests drag through actual Canvas receivers (including a narrow resized
viewport), check ordered source times/localized ms labels and all three overlays,
and verify FFT tile reuse and unchanged chart revision. Rate tests cover the slider's
0.25/2.00 endpoints, deterministic DSP/cue timing, negative pre-roll, live changes,
pending versus already audible hits, pause/resume and invalid values. Actual FMOD
NoSound playback also changes rates across repeated MP3 pause/seek/resume cycles.
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
Twelve actual MP3 pause/resume cycles cover reused stream initialization and target seeks separately,
including negative pre-roll and seeks made while paused. The timeline stays frozen until both preparations finish.
EditorModeTests checks time zoom at five scales, stable lane/head dimensions and zoomed snapping,
unchanged chart timing/revision and consecutive Z/X, last-tail/music-end destinations,
missing/stale duration rejection, retry and manual-seek/direction reset behavior.
Q cancels an unfinished placement and selects notes; inspector cycles stay within each of the six
tool groups, retain paired endpoint IDs and reject cross-group drafts without modifying the chart.
Selection tests cover real Taiko drag ranges, empty margins, note overwrite with/without
blanks, grouped Undo/Redo restoring overwritten originals, Ctrl/Shift timing/effect
row selection and metadata history. Realtime wheel navigation follows the beat grid
without changing chart revision. Domain tests additionally cover the 32-action cap,
saved-state dirty tracking, failed edits retaining redo and paired long-note restoration.
Additional checks cover preview failure retry suppression, selection recovery, owned voice cleanup,
and eight consecutive cancelled analysis jobs followed by successful real audio analysis.
Drawing assertions do not initialize a GPU;
this does not replace visual inspection, listening or mouse testing in the app.

## Options panel and Option.ini regression

Resolution changes use the window service immediately, with rollback when INI saving fails.
The suite checks primary/auxiliary key capture, default-key conflicts, reserved game controls,
ESC cancellation, integer global offsets, invalid drafts and persistent mode profiles.
The rhythm suite's OptionTimingTests checks resolved input/held mappings and us timing translation
across notes, ticks, BPM/delay, bars and YME, without changing saved chart data.
Engine SDK `--window-control` verifies actual hidden native resizing and renderer notifications.

After building both configurations, run in an x64 Visual Studio developer PowerShell
with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Debug
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Release
```

This links the production panel/settings objects. It checks UTF-8 settings round trips,
panel/field contrast, closing expanded popups on ESC, repeated ESC during animation,
immediate close before the first animation step, stale selection rejection and reopening,
invalid values, all three audio ComboBoxes, sparse driver indices, output-scoped lists,
failed switches and rollback after a read-only INI blocks saving. Test files are isolated
in a unique temporary directory; the user's Option.ini and skins are not changed.
Songs settings cover UTF-8 paths, persistence, the default root and EXE-relative resolution.
The native folder-selection dialog still requires manual input validation.
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
that Option.ini stays beside the EXE. A saved Songs override remains EXE-relative
even when CWD points to the other tree, and an empty override restores default discovery.
Fixtures stay under ignored build output;
no user settings, songs, skins or embedded cache are changed. The test source
uses MSVC Code Analysis. Catalog tests separately check the AngelDream YMP/YME pairs.

## Result graphs and actual Scene transition

After a solution build, run in an x64 Visual Studio developer PowerShell with `FMOD_ROOT`:

```powershell
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Debug
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Release
```

This links the production Result Scene/view/factory/graphs and gameplay objects.
It reads real embedded rank PNG metadata and checks all 11 ranks at 21 exact/below-threshold
six-decimal accuracy values, including 100% Y, 99.999999% PERFECT and the E/F boundary.
Rank images retain their aspect ratio inside the existing frame across resize and Canvas teardown.
It checks six-decimal accuracy text, Korean/English strings, signed scatter axes,
1ms histogram rounding/spacing, per-grade stacked colors matching scatter points,
peak totals, sparse outliers, minimum pixel widths without bin merging, empty samples,
narrow/wide layout, pointer-router return/retry actions,
disabled replay and catalog metadata snapshots,
and one immutable graph batch for 10,003 samples. It then runs D3D12/FMOD with a
hidden window and a one-note regression session: the production gameplay Scene
waits three seconds after automatic judgement, moves its result to the lazy Result
factory, and destroys its own Canvas. Clicking retry constructs a fresh gameplay
session after destroying Result and its Canvas; the return route then cleans gameplay.
The test source uses MSVC Code Analysis and does not write user options/songs/skins.
This is automation, not a manual pixel/physical keyboard/mouse/listening check.
ESC routing is connected in production; its interrupted scoring and tied-us input
order are covered by domain tests, without claiming a physical ESC test.

## Skin manifest and scene images

```powershell
./Tests/Presentation/RunSkinTests.ps1 -Configuration Debug
./Tests/Presentation/RunSkinTests.ps1 -Configuration Release
```

Links production SkinManifest/selection/binding and Logo/MusicSelect views in an isolated
deployment directory, without writing user skins or Option.ini. Checks UTF-8/BOM/CRLF/Unicode paths,
scene-folder/image relativity, invalid paths/encoding/duplicates, per-component embedded fallback,
partial skins, cache/reload, button states, full-screen logo replacement, procedural-fill
hiding/restoration and virtualized song-card bindings using the real bundled catalog.
The test source uses MSVC Code Analysis. Native captures/manual input are reported separately.

## New difficulty metadata and file creation

After building the solution, run `RunNewDifficultyTests.ps1 -Configuration Debug` and `Release`.
The test links production creation/dialog objects with MSVC `/W4 /analyze`. It uses an isolated
ignored `build/new-difficulty-tests/<pid>` Songs fixture. It checks Unicode metadata, default
empty Taiko timing, sibling YME and Songs-relative YMM paths, no overwrites, invalid names/BPM,
JudgeLevel 1.00..100.00 in 0.01 steps, cancellation, correcting invalid fields in the same native
dialog, and rediscovery by SongCatalog. Timers drive the owned dialog automatically; this is
not physical mouse/keyboard validation. The real user Songs, Skins and Option.ini are untouched.
MusicSelectViewTests additionally checks Editor-only plus buttons and their creation intent.
EditorMetronomeTests checks cached grid generation, tempo/meter/offset/delay changes and
separation of playback clicks from the original hit-sound analysis markers.
