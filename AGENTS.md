# MyRhythmGame-Client working agreement

This file applies to the entire repository except the independently versioned
`Dependencies/MRG-Engine` submodule.

## Purpose and boundary

- This private repository contains game-specific code and assets.
- The reusable engine is pinned as the private `Dependencies/MRG-Engine`
  submodule. Engine changes belong in that repository, not in a detached
  submodule checkout here.
- Client source may include only `MRG_Core.h` from the engine and may reference
  only `MRG.Core.vcxproj`.
- Do not commit FMOD SDK files, import libraries, or runtime DLLs.

## Client rules

- Keep game-specific Scene IDs in `Client/GameFlow/SceneIds.h`.
- Register Scene factories through `SceneManager::RegisterScene` with explicit
  `KeepAlive` or `DestroyOnExit` retention and use deferred `ChangeScene` for
  all transitions.
- Use Win32 Virtual-Key values with the engine input API; do not add a Client
  key enum for physical keyboard input.
- Preserve timestamped Raw Input events for future rhythm judgement.
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
