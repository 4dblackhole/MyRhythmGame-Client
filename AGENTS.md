# MyRhythmGame-Client working agreement

This file applies to the entire repository except the independently versioned
`Dependencies/MRG-Engine` submodule.

## Session onboarding

- Read `Docs/README.md` at the start of a new Client session. Then read
  `Dependencies/MRG-Engine/Docs/EngineOverview.md` for the engine feature and
  ownership index, and `Docs/ExecutionFlow.md` for this Client's concrete
  startup, frame, Scene, and shutdown flow.
- Use `Docs/Examples/ColoredCube/EngineFeatureExamples.md` for archived
  mesh/collision/widget recipes and the Visual2D documents for Sprite, Canvas,
  input, PNG, and Z-order work.
- Keep `README.md`, `Docs/README.md`, and affected feature documents in sync
  when a major Client flow, example, control, or engine integration changes.

## Purpose and boundary

- This private repository contains game-specific code and assets.
- The reusable engine is pinned as the private `Dependencies/MRG-Engine`
  submodule. Engine changes belong in that repository, not in a detached
  submodule checkout here.
- Client source may include only `MRG_Core.h` from the engine and may reference
  only `MRG.Core.vcxproj`.
- Do not commit FMOD SDK files, import libraries, or runtime DLLs.

## Client rules

- Keep FingerDrum Scene IDs in `Client/GameFlow/FingerDrumSceneIds.h`; retain
  archived example routes under `Client/Examples/ColoredCube/GameFlow/`.
- Register Scene factories through `SceneManager::RegisterScene` with explicit
  `KeepAlive` or `DestroyOnExit` retention and use deferred `ChangeScene` for
  all transitions.
- Register every gameplay-mode Scene as `DestroyOnExit`. Keep only its factory
  while inactive, create the concrete mode Scene on entry, and return only
  result data before leaving so `Shutdown` can destroy all play-session state.
- Use Win32 Virtual-Key values with the engine input API; do not add a Client
  key enum for physical keyboard input.
- Preserve timestamped Raw Input events for future rhythm judgement.
- Keep one `RhythmTimer` per active play session. Convert timestamped QPC
  input and DSP scheduling through that clock; rendering time is never a
  judgement source. Re-anchor DSP time after a gameplay pause.
- Keep note rules backend-neutral. They emit semantic `NoteEvent` and
  `AudioCueRequest` values; only the Client audio router maps them to engine
  clips, buses, and effects.
- Compile beat/BPM positions to integer microseconds when a chart session is
  loaded. Per-frame gameplay code must not repeatedly integrate tempo maps.
- Keep YMP focused on playable score data and YME focused on visual/audio
  automation. New game modes implement `IPlayGameMode` and own parsing-to-note
  construction policy.
- Keep Client `.vcxproj.filters` paths synchronized with physical directories.
- Bundle each redistributed font license beside the font asset.
- Keep game option behavior in Client code. Compose it from engine
  `Visual2DNode` components, Canvas actions, and surface mapping rather than
  adding game-specific widget classes to the engine.
- Keep functions readable as they grow. If a function performs multiple
  operations, extract each operation into a clearly named helper/private
  function. If a long function still represents one cohesive operation, add
  short section comments at each meaningful phase boundary to explain the
  intent and required ordering; do not add comments that merely restate an
  obvious statement.

## Repository synchronization

- Treat synchronization as part of every completed engine/Client task. Fetch
  both repositories, verify the local work branch matches its upstream, and
  report exact commit IDs when handing work back.
- When the engine commit changes, first make that commit available on the
  engine remote, then update and commit the `Dependencies/MRG-Engine` gitlink.
  Verify that `git ls-files -s Dependencies/MRG-Engine` and
  `git -C Dependencies/MRG-Engine rev-parse HEAD` name the same commit.
- After approved PRs are merged, fast-forward the Client `main`, run
  `git submodule sync --recursive` and `git submodule update --init
  --recursive`, and verify that both repositories are on the intended latest
  commits before declaring the task complete.
- After validation succeeds, publish the task branch, create or update its PR,
  mark it ready, and merge it into `main` as part of the normal completion
  workflow. For combined changes, merge the engine first, update the Client
  gitlink to the resulting engine `main` commit, and only then merge the Client.
- Never claim that `main` is current while a required PR remains open. Do not
  bypass failed checks, merge conflicts, or branch protection; report such a
  blocker instead. Preserve unrelated working-tree files while synchronizing.

## Verification

After changes, initialize the submodule and rebuild
`MyRhythmGame-Client.sln` in Debug and Release x64. Run the resulting
`MyRhythmGame.exe --smoke-test` in both configurations.
