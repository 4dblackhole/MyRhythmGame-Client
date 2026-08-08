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

## Verification

After changes, initialize the submodule and rebuild
`MyRhythmGame-Client.sln` in Debug and Release x64. Run the resulting
`MyRhythmGame.exe --smoke-test` in both configurations.
