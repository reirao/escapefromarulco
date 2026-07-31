# Escape from Arulco 0.0.1.13 - Fragile Tested Playtest

This release keeps the existing `v0.0.1.13` number and replaces its earlier
checkpoint with the current repository state.

> **FRAGILE / TESTED:** the Windows build compiles and its automated test suite
> passes, but the integrated gameplay paths are still an early-alpha playtest.
> Map content, long sessions, save migration and the full combinations of mouse,
> keyboard, combat and window state have not been exhaustively tested. Expect
> rough edges, incomplete interactions and crashes.

## Status language

- **TESTED** means a reproducible automated build, unit test, package inspection or
  launch check was completed for this exact commit.
- **FRAGILE** means the feature is present and available for playtesting, but still
  needs hands-on coverage across maps and state combinations.
- **NOT IMPLEMENTED** means the repository contains no finished gameplay path and
  the item is not a release promise.

The feature-by-feature source audit is in `FEATURE_WIRING.md`. The longer list in
`PLAYTESTING.md` is an acceptance checklist for testers, not a claim that every
listed scenario has already passed.

## Current fragile-tested slice

- The game starts in a player-owned tactical sandbox without the helicopter intro
  or starting enemies.
- The in-world creator has a validated callsign, default attribute distribution,
  freely editable attributes, selectable male/female JA2 tactical body sprites and
  two selectable JA2 traits. The generated operator is muted.
- A short crate tutorial exercises the intended chain: hover, `F` or right click,
  choose `CONTENTS`, automatically approach if necessary, then take an item.
- `F` resolves the relation under the current pointer. On the owned merc it opens
  the categorized character hub; on a world object or terrain it opens the same
  registry-backed contextual action surface used by right click.
- Environment actions are resolved from one target-bound relation. Entries report
  ready, missing tool, too heavy, unavailable or move-to-range state instead of
  silently executing against a stale object.
- Out-of-range `CONTENTS`, pickup, digging, salvage and manipulation actions can
  queue a native JA2 approach path and revalidate actor, target and relation before
  execution. WASD, combat entry, target loss, path interruption and timeout cancel it.
- The former bottom strip is now one movable OS//0 multitool. Double-click expands
  or minimizes it; its five groups are Interaction, Salvage, Inspect, Info and God,
  plus the Normal/Combat control switch. Its position is persisted.
- Plain Escape cancels one topmost OS//0 layer per physical key press: modal/radial,
  held item, world manipulation, queued approach, then cursor/combat state.
- Normal and Combat direct control use mouse-facing, realtime WASD movement,
  Shift sprint and Q/E turning. Combat mode routes an ordinary loaded firearm to
  one left-click shot; native burst, throw and trajectory confirmation remains.
- Character hub, equipment/pockets, spatial container contents, item drag/drop,
  movable windows, object hover/inspection, asset catalog, environment tools,
  sector resources, live editor, radar/strategy window, zoom and feedback reports
  remain available as experimental systems.

## Validation for this replacement

- Windows MinGW64/Ninja game and unit-test builds: **TESTED**.
- Automated tests: **178/178 passed**.
- Portable package dependency and content inspection: **TESTED**; SDL2, FLTK,
  image-codec and MinGW runtime DLLs include `libwinpthread-1.dll`.
- Windowed 12-second launch smoke check: **TESTED**; the executable remained alive,
  loaded the configured JA2 data and wrote no fatal/error entry before test shutdown.
- Gameplay scenarios in `PLAYTESTING.md`: **FRAGILE / community testing requested**.

## Known boundaries

- This is not a stable campaign release. Existing saves and UI layout files may
  expose migration defects; a fresh test profile/save is recommended.
- Object recognition depends on JA2 tileset and structure metadata. Not every map
  sprite is categorized, openable, movable, salvageable or correctly tooled yet.
- The crate tutorial depends on finding a usable openable structure in the loaded
  sector and remains a fragile integration test.
- Direct control still drives JA2's tile/path and animation systems. It is not a
  continuous free-movement or modern action-shooter controller.
- Material, damage and carry behaviour is a lightweight gameplay model, not a
  general rigid-body physics engine.
- The live editor is a debug authoring tool with map-level recovery, not complete
  transactional undo/redo. Test it on disposable sectors and saves.
- The movable strategy window does not yet replace travel plotting, assignments,
  militia and finance from the legacy Map Screen.
- Removable nested backpacks with independent saved contents are not implemented;
  `PACK` currently exposes the merc's real JA2 pocket slots.
- Player-economy construction of arbitrary catalogued assets is not implemented.
- JA2 1.13 code/data integration and network multiplayer/co-op are not included.
- Original Jagged Alliance 2 data is not distributed. A legally installed JA2 or
  JA2 Gold copy is required.

Use the in-game `REPORT` tab when filing an issue and attach the generated report.
The release ZIP includes `PLAYTESTING.md` and `COLLECT_LAST_CRASH.cmd`.
