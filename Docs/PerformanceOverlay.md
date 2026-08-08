# ColoredCubeGame performance overlay

`ColoredCubeGame` owns the optional FPS/UPS overlay.  It loads the two Client
font assets in `OnClientInitialized`, keeps the formatted text locally, and
releases the font handles in `OnClientShuttingDown`.

During `OnClientUpdated`, F1 toggles visibility only for this Client.  The
same hook consumes `UpdateContext::performance` when its `measurementIndex`
changes and updates the FPS and UPS strings.

During `OnClientRendered`, which runs after `SceneManager` renders the active
scene, the game submits two right-aligned `TextDrawCommand`s through the
`RenderContext` text service.  The shadow and colors are Client choices; the
Engine only schedules frames and provides the measured data.

Use `--show-performance-overlay` to start the overlay visible for a visual
capture.  In ordinary runs it starts hidden and F1 controls it.
