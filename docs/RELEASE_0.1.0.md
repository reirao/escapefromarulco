# Escape from Arulco 0.1 - Interaction and Physical Inventory Preview

`v0.1.0` is the first public 0.1 checkpoint for Escape from Arulco. It promotes the
current object-first tactical sandbox, character creation, contextual interaction,
physical inventory and world-manipulation work into one reproducible Windows x64
playtest package.

> **Fragile-tested early alpha.** This is a community test build, not a finished or
> balanced campaign. The automated core and Windows package gate passed, but the full
> matrix of JA2 maps, tilesets, save histories and combined mouse states still needs
> manual coverage. Please report PASS, FAIL or NOT REACHED with exact reproduction
> steps.

## Validation for this tag

- Windows MinGW64/Ninja RelWithDebInfo game and launcher build: passed.
- Complete registered automated suite: **229/229 tests passed in 76 suites**.
- Source whitespace/error check: passed; Git reports only local line-ending notices.
- Independent read-only audits found no remaining concrete P0/P1 in item transfer,
  viewport release ownership, carry rendering or open/closed container move replay.
- Portable package gate rebuilds from this clean tagged commit, embeds its commit in
  `BUILD_INFO.txt`, includes a SHA-256 sidecar and excludes original JA2 data.
- Full manual runtime acceptance remains requested from community testers.

## What is new since 0.0.1.23

### One exact item-transfer lifecycle

- Gesture ownership, exact-source binding and commit/cancel transaction are separated
  into reusable modules instead of competing inventory and viewport callbacks.
- Every world item keeps its exact index, complete object fingerprint and mutation
  revision through hover, `F`, RMB, MMB, double click, pickup and drag.
- Cross-region mouse release and lost-focus recovery dispatch at most one destination.
- Invalid or occupied destinations never silently swap or lose an item. The exact
  source is restored when it still exists; otherwise the complete object remains held
  or is recovered at a valid carrier's feet.
- A character-centred equipment projection exposes real hands, armour, face and pocket
  destinations while an object is held.

### Carry, containers and persistence

- Carry binds one exact soldier instance and one exact structure from selection until
  collision-valid commit; stale or changed targets fail closed.
- The source sprite is suppressed only while a real cursor- or actor-centred carry
  projection replaces it.
- Container contents move transactionally with their owner and retain their exact
  hidden/visible placement metadata across sector reconstruction.
- Structure relocation is one atomic map-temp record. Open and closed partner graphics
  are treated as one persistent identity, preventing source-crate and loot duplication
  after moving an open container and reloading the sector.
- Loose roof items remain draggable; unsupported roof-structure carry is deliberately
  rejected instead of writing an invalid ground-layer move.

### Input, direct control and workspace hardening

- Viewport presses, UI child regions and native JA2 item cursors share one physical
  release boundary.
- Stationary hover invalidates when world items mutate, so reused pool indices cannot
  redirect a later action.
- Mouse-facing Normal/Combat control, WASD/Shift/Q/E suppression and layered Escape
  cancellation were hardened against stale native events.
- Managed windows, equipment projection, loot surfaces and the movable multitool share
  the current camera/zoom projection and consistent pointer ownership.

## Included prototype systems

- Direct enemy-free tactical sandbox start and in-world operator creator.
- Guided crate interaction tutorial and contextual `F`/RMB/MMB object actions.
- Character, equipment, pocket and spatial container drag-and-drop views.
- Movable structures, material-aware salvage/digging and persistent sector resources.
- Asset catalogue, live world editor, tactical zoom, direct control and feedback logs.

See `FEATURE_WIRING.md` for the audited truth table and `PLAYTESTING.md` for the exact
Golden Path and extended manual scenarios.

## Not included

- Original Jagged Alliance 2 data. Testers need a legal JA2 or JA2 Gold installation.
- JA2 1.13 integration, network multiplayer/co-op or a finished strategic campaign.
- General rigid-body physics, arbitrary blueprint construction or nested persistent
  backpack containers.

## Install and report

Extract the complete ZIP, run `START_PLAYTEST.cmd`, select the legal JA2 installation
and matching language in the launcher, then begin with the Golden Path in
`PLAYTESTING.md`. Use the in-game `REPORT` tab, or run `COLLECT_LAST_CRASH.cmd` after a
crash and attach the generated `reports` folder to a GitHub issue.
