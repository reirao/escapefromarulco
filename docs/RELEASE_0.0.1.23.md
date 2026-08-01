# Escape from Arulco 0.0.1.23 - Stable Interaction Checkpoint

`v0.0.1.23` is the new stable source and Windows playtest checkpoint for
Escape from Arulco. It supersedes `v0.0.1.12` as the recommended baseline and
contains every change from the experimental releases through `v0.0.1.13`, plus
the item-transfer, mouse-input, persistence and hot-path hardening listed below.

> **Stable checkpoint does not mean finished campaign.** It means that this exact
> source revision, Windows runtime, automated test suite, package composition and
> startup path form the reproducible reference build. Gameplay systems explicitly
> labelled experimental still need community coverage across maps, saves and long
> sessions.

## Validation recorded for the published tag

The following numbers describe the source that was packaged and tagged as
`v0.0.1.23`. They are historical release evidence, not the validation result for a
newer local working tree; later source must rerun its complete registered suite.

- Windows MinGW64/Ninja unit-test build: passed.
- Automated suite at the published tag: **191/191 tests passed** across 68 suites.
- Windows RelWithDebInfo playtest build: passed.
- Twelve-second windowed startup smoke test with a configured JA2 Gold installation:
  process remained alive and the session log contained no error or fatal entry.
- Source whitespace/error check: passed.
- Portable Windows package: includes launcher, game executable, SDL/FLTK/image-codec
  and MinGW runtime dependencies, externalized data, mods, documentation and crash
  collector. Original Jagged Alliance 2 data is not included.

## What changed after v0.0.1.13

### One physical item-transfer transaction

- Added `OS0ItemTransferController`, the single state machine for source press,
  drag threshold, held item and final release ownership.
- Inventory slots, actor equipment, pockets, spatial container loot, relation
  choices and world drops no longer process the same release independently.
- A source click cannot become a delayed drag. A drag starts after a real movement
  threshold or when the held pointer leaves the source mouse region.
- Native item cursors created by legacy JA2 paths, stack confirmation or restored
  state are adopted into the same controller.
- Invalid destinations leave the object on the cursor. They do not silently drop,
  duplicate or delete the remainder.
- Empty panel areas explicitly consume the transfer attempt while preserving the
  held object, so the world behind a window cannot receive the same release.
- While an object is held, unrelated close, drag, tab, catalog, feedback, editor,
  dock and combat controls are centrally disabled.

### Relational actor inventory rules

- Added one engine-independent transfer policy for primary hand, secondary hand,
  body equipment, pack and drop-at-feet.
- Body selection respects helmet, vest, leggings and face slots.
- Hand selection rejects objects without active hand use and respects two-handed
  weapon conflicts.
- Pack selection checks every real JA2 pocket, stack limit and remaining quantity.
- Added carried-weight validation before an actor accepts an object.
- Pack placement finishes compatible stacks, then uses small pockets, then large
  pockets to preserve scarce capacity.
- The one semantically preferred valid relation is shown directly. Additional valid
  destinations appear only after `MORE OPTIONS`, reducing visual overload while
  keeping every legal choice accessible.
- The policy exposes a safe automatic preference for future behaviour/AI modules,
  but the current player-facing UI does not silently execute ambiguous choices.

### Mouse and viewport hardening

- Added a post-dispatch MouseSystem event observer. JA2's legacy region dispatcher
  intentionally withheld `UP` when a press started in one region and ended in
  another; OS0 now routes that physical release once to the actual destination.
- Releases outside all registered regions clear the original click capture, so the
  next click cannot be blocked by a stale or removed region.
- Cross-region release resets invalid double-click/double-tap history instead of
  retaining a dead source pointer.
- Alt-Tab/focus-loss recovery closes controller ownership when SDL clears the
  physical button without delivering a final `UP` event.
- The actor, grid, level and tile underneath the cursor are resolved again when the
  player releases an item. Camera movement, zoom, animated actors and moving windows
  therefore cannot redirect a drop to an old hover target.
- Inventory and modal transitions explicitly invalidate cached world-hover
  projections before the next frame.
- JA2's external item cursor is initialized immediately when a native item pointer
  begins, avoiding a one-frame ordinary-cursor race.

### UI consolidation and performance

- Removed the duplicate floating item-details window. Item details remain live
  inspector content; character inventory, character equipment and object contents
  remain separate intentional surfaces.
- Removed obsolete transient-window and dock-entry bookkeeping left over from the
  former fixed command strip.
- Added bounded, allocation-free `OS0FixedList` storage for contextual actions,
  character action pages, transfer decisions and managed-window render order.
- Merged the legacy cursor/environment adapters into the one relational resolver.
- Replaced small `stable_sort` calls with stable bounded insertion ordering without
  allocator traffic.
- Direct control now copies only the live JA2 route prefix during a speculative
  replan instead of the complete maximum path buffer every 45 ms.
- Cover orders are updated backwards in place instead of copying the entire list
  each tactical frame.

### Save/load and defensive fixes

- OS0 clears transient carry, cover, cursor, pending visual and diagnostic state
  before loading persistent data over an active tactical session.
- Save filenames use portable ISO-8601 `strftime` fields supported by MinGW and
  safely handle failed UTC conversion.
- Realtime-editor enum fallbacks use explicit engine-compatible integer types.
- Removed redundant editor cell-coordinate expressions and several dead presentation
  fields and compatibility functions.

### Added regression coverage

- One source and one release target per item gesture.
- Held item surviving a context choice.
- Click versus drag threshold separation.
- Stack pointer creation on button release.
- Double-click release suppression timing.
- New physical press clearing the previous release latch.
- Focus loss without a delivered mouse release.
- Cross-region physical release recovery.
- Release outside all regions clearing click capture.
- Fixed-capacity action and transfer limits.
- Save-load transient-state cleanup and persistent-state survival.
- Window ordering, layout round-trip and interaction-mode cancellation.

## Complete current feature inventory

### Start and operator creation

- Direct start in the player-owned tactical sandbox without helicopter arrival,
  opening briefing or starting enemies.
- One in-world operator creator with a validated callsign, default or freely edited
  attributes, male/female JA2 tactical body selection and two selectable traits.
- Generated operator is muted; camouflage is applied immediately when selected.
- Obsolete laptop/IMP wrapper paths are retired from the normal start flow.
- Creator completion is stored with the OS0 session and does not reopen on a
  compatible save.
- Marked-crate tutorial covers target perception, `F`/RMB actions, `CONTENTS`,
  automatic approach and taking an item through the real runtime path.

### Shared OS//0 workspace

- One UI runtime and one window manager own visibility, focus, drag capture,
  clamping, persistence, modal suspension, render order and native region order.
- One movable and minimizable OS//0 multitool replaces the former bottom strip.
- Double-click expands or collapses it; its anchor persists by stable layout name.
- Normal/Combat, Interaction, Salvage, Inspect, Info and God commands open their
  real registered functions instead of display-only placeholders.
- Character, Inspector, Toolbox, Environment, Sector, Asset Library, Asset Catalog
  and realtime editor windows use shared geometry and focus rules.
- Character inventory is optional rather than forced after creation, selection or
  container interaction.
- Movable field-computer views expose Base, Arulco map/radar, Team and Report data.
- Built-in feedback reports capture tester text, game state, recent OS0 actions,
  engine-log tail and asset-catalog snapshot.

### Object-first perception and actions

- Opaque-pixel scenery hit-testing recognizes visible object/structure sprites
  across neighbouring cells, including canonical bases of multi-tile assets.
- Live hover inspector shows the real selected sprite, material, footprint,
  durability, salvage yield and required tool when known.
- `F`, right click, middle-click cycling, quick actions and execution all consume
  one target-bound relational action result.
- `F` over the owned merc opens the categorized character hub; `F` over world
  content opens that exact object's action surface. `Alt+F` retains JA2 tracking.
- Context actions report immediate, move-to-range or blocked status, including
  missing tool, too heavy, invalid target, no actor and unavailable.
- Pending physical actions use native pathing, then revalidate actor, sector, level,
  target identity, distance and capability before execution.
- Movement override, combat, target mutation, path failure, timeout and sector change
  cancel pending interaction ownership deterministically.
- Escape unwinds one highest-priority layer per press: modal/radial, held item,
  active manipulation, pending approach, then cursor/combat state.

### Character hub, equipment and containers

- Categorized character hub pages: Actions, Abilities/Talents, Equipment, Group and
  God; entries are shown only when backed by executable engine/OS0 behaviour.
- Circular actor-anchored right-click command ring using original JA2 interface art.
- Extended JA2 long-hold movement frame for character, inventory, stealth, weapon,
  reload, stance and God/library commands.
- In-world equipment explosion for helmet, face, vest, legs, both hands and real
  pocket/pack slots, following the interpolated actor animation position.
- Separate spatial world-container contents built around the owning object sprite.
- Stack quantity dialog before dragging part of a player inventory stack.
- Deterministic first-open container seeding with useful and damaged equipment plus
  material loot; a persistent marker prevents repeated refilling.
- No legacy modal pickup list or locator flash for the OS0 container flow.

### Environment, materials and sector progression

- Material-aware recognition for timber, metal, stone, debris, doors, fences,
  furniture, containers and diggable ground.
- Tool-gated digging, dismantling and harvesting produce topsoil, timber, stone or
  scrap as physical world items.
- Material-specific debug tools support current discovery and community testing.
- Structure mass, friction, restitution, integrity and carrying capacity derive
  from catalog/engine structure data.
- Carry, drag, push, pull and throw relations respect actor strength and wounds;
  placement stays transactional through JA2 collision checks.
- Carried structures retain their source sprite during movement, show lift/drag
  presentation and train persistent strength sub-points after successful handling.
- Material-coloured impact chips and persistent durability for catalogued resource
  assets; critical geometry remains owned by JA2.
- Per-sector timber, stone, scrap and soil stockpiles plus Field Shelter, Salvage
  Workshop and Secure Depot upgrades stored in save-compatible OS0 state.

### Asset catalog and realtime editor

- God-mode asset library scans the live sector, deduplicates canonical assets and
  renders real source sprites with instance counts and catalog metadata.
- Community catalog editor records custom name, category, material, role, footprint
  and buildable/placeable flags to `%APPDATA%\JA2\AssetCatalog\os0-assets.tsv`.
- Local catalog rows override the bundled curated database and feedback reports
  include a shareable snapshot without original JA2 graphics.
- Realtime editor palettes cover active-tileset graphics, editor-safe items,
  generic actors and NPC/RPC profiles with filters, variants, quantity and facing.
- Typed frame-boundary commands handle placement, removal, terrain painting,
  smoothing and all 32 guarded native JA2 road macros.
- Two-click empty-map confirmation and private `live-editor.dat` save/load workflow.
- World replacement preserves the player squad and creates a temporary map snapshot;
  failures restore map, squad, selection, camera and ambience.

### Direct tactical control, combat and zoom

- Explicit Normal/Combat state owned by the OS//0 multitool.
- Mouse-facing operator, realtime WASD movement, Shift sprint and Q/E rotation while
  retaining JA2 pathing, collision and animation ownership.
- Reverse/strafe intent and tile-boundary replanning; turn-based control issues
  AP-safe single-tile commands.
- Ordinary loaded firearm fires once on left-button release in Combat mode; burst,
  throw and trajectory attacks keep their native confirmation rules.
- Tactical zoom shares one world/display transform and a complete viewport-signature
  cache barrier for combat-message geometry and map-edge scrolling.
- AI run-to-cover command uses JA2 cover evaluation/pathing and chooses prone or
  crouched arrival stance.

### Distribution and community testing

- Portable Windows x64 playtest with launcher and required dynamic libraries,
  including `libwinpthread-1.dll` and image-codec dependencies.
- `START_PLAYTEST.cmd`, direct-start helper and crash/report collector are included.
- Local one-command build/start shortcut configures Ninja, builds changed files,
  refreshes runtime DLLs and starts the exact new executable.
- Original JA2 data is never distributed; a legally installed JA2 or JA2 Gold copy
  and the correct language selection are required.

## Explicitly not implemented

- JA2 1.13 code/data integration.
- Network multiplayer or synchronized co-op.
- A complete balanced campaign, enemy progression or production economy.
- Arbitrary player-economy construction from every catalogued asset.
- General rigid-body physics, continuous structure collision/rotation or voxel terrain.
- Nested removable backpacks with independent persistent contents.
- Complete transactional edit undo/redo for projectiles, bombs and tactical events.
- Full strategic travel plotting, assignments, militia and finance inside OS0 windows.

These remain roadmap items and are not promises of the stable checkpoint.

## Playtest and reporting

Extract the whole archive, run `START_PLAYTEST.cmd`, point the launcher to a legally
installed JA2 directory and select the matching language. Use the computer's `REPORT`
tab for reproducible feedback. If the game terminates before a report can be saved,
run `COLLECT_LAST_CRASH.cmd` and attach the generated `reports` folder to the issue.
