# Escape from Arulco 0.0.1.12 — Stable Runtime Checkpoint

> **Historical checkpoint:** “Stable” in this document is the original checkpoint
> name, not a current stability guarantee. This package is superseded by the
> `v0.0.1.23` **STABLE CHECKPOINT / EARLY ALPHA** release.

This release freezes the current OS0 prototype as the first GitHub **stable**
playtest after the `v0.0.1.11` consolidated baseline. Stable here means that the
published Windows package was rebuilt, its automated suite passed, and the game
completed a launch smoke test. The project itself remains an experimental gameplay
prototype rather than a finished or balanced campaign.

## What changed

- One shared window manager now owns position, visibility, focus, drag capture,
  persistence, Z order and modal suspension for the tactical workspace.
- A typed interaction-mode controller separates normal control, aiming, item drag,
  contextual interaction and realtime field editing.
- Mouse-facing WASD movement, reverse/strafe control, sprint and route replanning are
  stabilized while JA2 continues to own pathfinding and character animation.
- The new movable realtime editor exposes active-tileset graphics, editor-safe items,
  generic actors and NPC/RPC profiles directly inside the live tactical sector.
- Terrain paint/smoothing, native road macros, object placement/removal and guarded
  empty/load/save workflows run through frame-boundary commands with map recovery.
- Pixel-accurate scenery selection, interaction lookup, editor catalogs and inventory
  redraw paths are cached or localized to reduce UI and cursor hitching.
- Mouse-region ordering follows the visible window stack, fixing overlap clicks,
  invisible hit regions and focus changes during window dragging.
- The repository includes `BUILD_AND_START_LOCAL.cmd` for a one-step local configure,
  incremental compile, runtime-DLL refresh and windowed launch.

## Validation

- Windows MinGW64/Ninja release build completed successfully.
- All **171/171** unit tests passed.
- The packaged executable completed a visible windowed launch smoke test and remained
  responsive without a new fatal log entry.

## Known boundaries

- This remains a sandbox systems prototype; campaign balance and progression are not
  release-complete.
- Two-times zoom, active carried/equipment overlays and some particle-heavy scenes can
  still be comparatively expensive on the original software-rendered engine.
- Realtime editor recovery is map-level rollback, not full undo of transient bullets,
  physics events, timers or projectiles.
- JA2 1.13 code/data integration and synchronized multiplayer are not included.
- The package contains no original Jagged Alliance 2 data. A legally installed JA2 or
  JA2 Gold copy is required.

Please use the in-game `REPORT` tab and attach the generated report when filing a
GitHub issue. The full hands-on checklist is in `PLAYTESTING.md`.
