# MyRhythmGame-Client working agreement

This file applies to the entire repository except the independently versioned
`Dependencies/MRG-Engine` submodule.

## Session onboarding

- Read `Docs/SessionHandoff.md` and the routing table in `Docs/README.md`.
  Select only the feature document and source subtree needed for the request.
  Do not read every document or the whole engine SDK for routine Client work.
  Read `Docs/ExecutionFlow.md` for startup, transitions, or lifetime changes;
  read engine `Docs/EngineOverview.md` only for engine API/ownership work.
- Use `Docs/ChangeImpactMap.md` for the smallest implementation/test boundary.
  Read declarations and direct callers before expanding the search.
- Use `Docs/Examples/ColoredCube/EngineFeatureExamples.md` for archived
  mesh/collision/widget recipes and the Visual2D documents for Sprite, Canvas,
  input, PNG, and Z-order work.
- Keep `README.md`, `Docs/README.md`, and affected feature documents in sync
  when a major Client flow, example, control, or engine integration changes.

## Scope discipline

- When a request is underspecified, implement the smallest reversible change
  that satisfies its explicit acceptance criteria. Do not interpret missing
  detail as permission to add speculative features, generalized subsystems,
  placeholder data, extra screens, samples, or abstractions.
- Inspect and reuse the current architecture before creating a new class,
  project, interface, asset, or framework layer. Add an extension point only
  when the requested behavior uses it now or the user explicitly asks for it.
- Keep refactoring local to the code touched by the request unless the user
  explicitly asks for a broader refactor. Do not turn a focused fix into a
  repository-wide cleanup.
- If requirements are insufficient or two reasonable interpretations would
  materially change behavior, ownership, public API, file format, or design,
  STOP the affected implementation and ask the user a concrete question.
  Do not implement a speculative interpretation while waiting. This applies
  to every contributor and future session. Independent specified work may
  continue. For immaterial details choose the smallest reversible change.
- Suggestions and future possibilities are not implementation requirements.
  Report them separately and stop once the requested behavior and proportional
  verification are complete.
- Do not invent songs, records, profile statistics, devices, or other runtime
  data to make a screen look populated. Use real data when available and an
  explicit empty state otherwise.

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
- Build song selection from `FingerDrum.Chart/Catalog/SongCatalog`; preserve
  the YMM/YMP relative-path relationship and keep list rows limited to title
  and artist. Pass selected paths through `GameplayLaunchRequest` rather than
  retaining a gameplay Scene.
- Author scroll-lane presentation in its default local direction and rotate
  the lane parent for a game mode. Keep lane backgrounds, effects, judgement
  lines, note layers and long-note parts under that parent instead of computing
  independent screen-space positions for each child.
- Keep Client `.vcxproj.filters` paths synchronized with physical directories.
- Give each Scene its own directory with `SceneName.h/.cpp` and `Submodules/`.
  The Scene composes responsibility-owning classes through small calls; moving
  the same giant class into several files is not sufficient. Keep the same
  physical/filter tree for domain families such as `Note/Submodules/`.
  Put shared contracts above leaves; never include implementation `.cpp` files.
- Preserve umbrella headers where needed for compatibility; new leaf code
  should include only the contracts it uses.
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

- Work directly on `main` unless the user explicitly requests a separate
  branch. Do not create a task branch by default.
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
- When the user explicitly requests a task branch, publish it after validation,
  create or update its PR, mark it ready, and merge it into `main`. For combined
  changes, merge the engine first, update the Client gitlink to the resulting
  engine `main` commit, and only then merge the Client.
- Never claim that `main` is current while a required PR remains open. Do not
  bypass failed checks, merge conflicts, or branch protection; report such a
  blocker instead. Preserve unrelated working-tree files while synchronizing.

## Verification

Use `Docs/Verification.md` for canonical commands. Code changes require
Debug/Release x64 solution builds, logic tests and all four Client smoke routes.
Engine changes additionally require the relevant `ColoredCubeGame` route.
Report automated checks separately from actual visual/manual validation.
