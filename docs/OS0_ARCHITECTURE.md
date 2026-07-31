# Escape from Arulco / OS0 tactical architecture

> **Current status:** this is the intended ownership model and an implementation
> audit, not a claim of campaign completeness. The current `v0.0.1.23` runtime is
> the **STABLE CHECKPOINT / EARLY ALPHA**; remaining integration debt is listed below and the public
> feature truth is maintained in `FEATURE_WIRING.md`.

This document describes the current ownership boundaries of the newest local
playtest build. It is deliberately a dependency map, not a proposed file list.

## Canonical state

JA2 remains the only canonical simulation:

- soldiers and their inventory are `SOLDIERTYPE` instances;
- world items remain `gWorldItems` / item pools;
- structures remain world `STRUCTURE` and `LEVELNODE` data;
- sectors remain `SectorInfo` plus OS0 save data attached to the normal save;
- pathing, animation, AP, stance and weapon execution remain engine services.

OS0 may hold interaction progress, additional durability and presentation state.
It must not mirror soldier inventories, structures or sector ownership.

## Current dependency and state map

| Area / representative functions | Reads | Mutates | Engine calls | Current owner | Boundary status |
| --- | --- | --- | --- | --- | --- |
| `InitializeOS0IngameUI`, `ShutdownOS0IngameUI` | layout/config files, tactical globals | UI surfaces and regions; sector exit cancels transient orders/carry | mouse-region and video-object lifecycle | `OS0_IngameUI.cc` plus `OS0TacticalSession` | durable state is independent of UI lifetime |
| `UpdateOS0TacticalSession` | canonical session and JA2 tactical state | carry, cover, direct control, visual-event queue | pathing, stance, UI-mode events | tactical update path from `GameScreen.cc` | simulation is independent of panel visibility |
| `RenderOS0IngameUI` and `Draw*` | selection, panels, inventory, world state | render surfaces, dirty flags and presentation caches only | framebuffer, item rendering | `OS0_IngameUI.cc` | gameplay updates removed from renderer |
| `UpdateWorldMove`, `OS0HandlePendingWorldMove` | stable carrier ID, source structure, destination | `OS0CarryState`, final canonical structure placement | pathing, structure mutation, movement costs | session state plus UI integration adapter | state transitions validated in `OS0_CarrySystem.h`; engine adapter remains debt |
| `CommandRunToCover`, `UpdateCoverCommands` | soldiers and local cover | an order collection keyed by `SoldierID` | pathing, stance | `OS0_CoverOrderSystem` plus update adapter | supports concurrent orders and deterministic cancellation |
| `OS0ApplyWorldAssetDamage`, durability queries | sector/level asset key, structure, material/catalog | durable damage records, canonical structures, item pools, visual events | structure removal and movement costs | `OS0_AssetDamageSystem` | weapon/world code has no overlay dependency |
| resource deposits and sector upgrades | versioned sector record | bounded resources and separate upgrade flags | strategic sector migration only | `OS0_SectorEconomySystem` and `OS0_WorldInteractionSystem` | normal JA2 save-state extension; legacy bits migrated once |
| catalog load/save/resolve | tileset/world asset, TSV overrides | session catalog and local override file | VFS/file I/O | `OS0_AssetCatalogService` | later user rows override built-in identity keys |
| relation/action resolution | bound actor/item/asset/terrain facts | none | world queries | `OS0_ActionRegistry::ResolveOS0InteractionActions` | hover, F, RMB, MMB and execution share one ordered target-bound resolver |
| pending world action | stable actor ID, target binding, action and destination | owned native route, then canonical target mutation | pathing and selected action | UI integration adapter plus registry | revalidates actor/sector/level/target/relation; cancellation reducer is unit-tested but engine integration remains fragile |
| `AddContextEntry`, `RefreshPanelActions` | target, catalog, inventory | presentation entry arrays | world/inventory queries | context UI | full menu/action-panel projection is remaining debt |
| `ContextActionCallback`, `OS0HandleCursorAction` | typed action and target | canonical simulation and UI projection | items, stance, weapon, world mutation | one typed execution callback plus cursor adapter | no numeric action meanings remain; callback extraction remains debt |
| `OS0CycleCursorAction`, `ApplyCursorTool` | registry cursor descriptors | session cursor/attack state | JA2 UI mode events | session cursor state and action registry | completed typed mapping (`MOVE`, `USE`, `CARRY`, etc.) |
| `ProjectControlModeToEngine` | canonical NORMAL/COMBAT state | pending safe projection | JA2 MOVE/ACTION events | OS0 session state | never overwrites firing, movement, interrupt or lock events; normalizes legacy cursor modes before combat |
| `OS0ViewportGestureState` | button-down owner | primary gesture/release hand-off | RT/TB legacy mouse pollers | viewport input adapter | one physical LMB gesture has exactly one owner and one release |
| inventory and item transfer | real soldier inventory, native item pointer and physical mouse gesture | real inventory and world pools | `PlaceObject`, item-pointer and world-drop services | `OS0ItemTransferController` for ownership plus `OS0_ItemRelations` for policy | one source, one held object and one release target; invalid destinations retain the native cursor |
| mouse-region release bridge | physical mouse/touch event and current native region | transfer ownership only | post-dispatch `MouseSystemHook` observer | `MouseSystem` bridge plus viewport adapter | recovers cross-region/outside-region/focus-loss release without changing JA2 inventory ownership |
| panel positioning/dragging/regions | `OS0WindowManager`, persisted layout and window geometry | visibility, drag positions, focus and canonical Z order | mouse system plus `OS0_MouseRegionZOrder` | window manager plus native region adapter | child controls are projected into manager Z order without recreating callbacks or drag capture |
| feedback/report functions | event ring and text input | report state and log file | keyboard hook / file I/O | UI file | `FeedbackSink` |
| zoom mapping | zoom state and viewport | zoom buffer | framebuffer and coordinate transforms | UI file | viewport service owned by tactical session |

## UI rebuild boundary (2026-07-29)

The tactical shell now has three explicit layers:

1. `OS0_UIRuntime` owns creator stage and independent panel visibility. It also
   owns the one screen-space layout calculation for world, dock and windows.
2. `OS0_ViewportInput` is the only adapter that translates tactical pointer
   gestures into typed OS0 commands. Legacy real-time and turn-based handlers no
   longer implement a second RMB/MMB path.
3. `OS0_IngameUI` is the current engine adapter and renderer. Old field names in
   that file temporarily reference `OS0_UIRuntime`; they are not separate state.

Character creation is a single in-sector flow backed by `OS0_CreatorModel`.
The former `OS0_CREATOR_SCREEN`/IMP wrapper was removed. Its inventory page is
not part of creation: after completion, the real JA2 inventory is an optional
`EQUIPMENT` action in the shared character hub, also reachable through the
the movable multitool's `INFO` group.

The former 38-pixel bottom dock is suppressed. Camera scrolling, world zoom and
south-edge hit testing use the full tactical viewport, while one persisted OS//0
multitool is projected in screen space and can be dragged or minimized to one icon.
Creator input is modal, fixed in screen space and pauses camera scrolling until entry
is complete.

## Shared windows and realtime field editor (2026-07-29)

`OS0WindowManager` is the canonical owner for every tactical window template.
Templates define identity, persistence key, icon, default geometry, minimum size,
presentation mode and behavior flags. Runtime state has one representation for
visibility, drag offset, bounds, suspension and Z order. Aim, modal interaction
and world replacement suspend a window without destroying the player's chosen
visibility or saved position.

The field editor is split across two strict layers:

- `OS0RealtimeEditorSession` owns typed command queues and value-only catalogs.
  Catalogs are projected from the active JA2 tileset, the content manager's real
  item database, generic actor templates and the game's NPC/RPC profiles.
- `OS0RealtimeEditorUI` renders previews and dispatches typed requests. It never
  owns a second map, inventory, soldier or structure model.

`EMPTY MAP`, `LOAD MAP` (defaulting to `live-editor.dat`), tile/item/NPC
placement, removal, catalog refresh and map save are executed only by
`UpdateRealtimeEditorSession` at the tactical frame boundary. Terrain authoring
is also typed: paint recipes use JA2's native
texture-stack algorithm, smoothing recipes call the original terrain/shoreline
smoother, and road recipes apply guarded native road macros. Recipe payloads are
immutable values; native editor globals are scoped inside the engine adapter.

The UI projects those catalogs through per-palette category filters. It exposes
the active native layer, related tile variants, terrain brush radius and
paint/smooth mode, road macro variants, item quantity, and eight-way NPC
rotation/facing.
These are request parameters only; the UI still owns no parallel world model.

Before a world-replacing command runs, the integration adapter clears every UI
cache that may contain a world pointer and suspends managed windows. The engine
then tears down events, bullets, physics, timed bombs and ambient state before
creating or loading the replacement world and reinserting the real player team.
Loaded map placements are instantiated after the squad and under a scoped
authoring mode, so profile and quest filters cannot change a save/load round
trip. Commands queued behind an attempted world swap are rejected because their
handles were resolved against the previous world generation.

Removal requests carry expected tileset/item/NPC identity and position values;
the engine revalidates those handles immediately before mutation. Map saves use
a private `%APPDATA%/JA2/OS0/maps` target with temporary and backup files rather
than writing directly over the last known-good map. `LOAD MAP` reads only a
sanitized filename from that same private directory.

Before `EMPTY MAP` or `LOAD MAP` destroys the current native world, the adapter
serializes a temporary map recovery snapshot. A failure after teardown reloads
that snapshot, reinserts the squad before authored NPCs, restores selection,
camera and ambient sound, and refreshes the catalog generation. This is a map
rollback, not a general undo system: queued events, bullets, physics objects and
active bomb timers are intentionally cleared and are not represented by a JA2
map `.dat` file.

Managed windows and all of their child `MOUSE_REGION`s share the manager's
back-to-front order. `OS0_MouseRegionZOrder` projects that order through the
native mouse list using safe region reinsertion; callbacks, user data, enabled
state and an active button capture survive focus changes. The adapter skips the
work when the native list already matches the manager.

## Resolved coupling defects

1. `Weapons.cc` now calls the neutral `OS0ApplyWorldAssetDamage` entry point and
   does not include `OS0_IngameUI.h`.
2. Asset damage is sector-, Z- and tile-aware session state. Tactical UI shutdown
   cancels transient interaction but does not heal assets.
3. `GameScreen.cc` calls `UpdateOS0TacticalSession` before rendering. Carry and
   cover updates no longer execute in `RenderOS0IngameUI`.
4. Cover orders are keyed by stable `SoldierID`; issuing one soldier's order does
   not overwrite another's.
5. Cursor state stores `ContextAction`; the former numeric 0..5 protocol is gone.
6. Resources and upgrades use versioned OS0 records. Only legacy OS0 bits are
   removed from surface `uiFacilitiesFlags`; native facility flags remain intact.
7. Built-in and user asset catalogs are merged by one service, while resource and
   upgrade transactions are owned by gameplay modules rather than draw code.
8. `OS0ItemTransferController` owns the physical item gesture while JA2 remains the
   only object carrier. Inventory, loot, actor relations and world drops cannot claim
   the same release twice.
9. MouseSystem clears stale click capture outside all regions and exposes a
   post-dispatch observer for the legacy cross-region release gap.

## Remaining technical debt

- Geometry and mouse-region objects still live in `OS0_IngameUI.cc` because they
  wrap JA2 engine types. Logical visibility, creator flow and dock geometry have
  moved to `OS0_UIRuntime`; remaining geometry can move renderer-by-renderer
  without inventing another state owner.
- World replacement has a temporary serialized map rollback, but not a full
  tactical-state transaction. Transient events, projectiles, physics and active
  bomb timers are cleared deliberately and cannot be recovered from map data.
  The command queue does not expose a general edit-history or undo operation.
- Tile placement validates before mutation and reports dirty geometry truthfully,
  but an allocator exception after the first native layer mutation cannot yet be
  rolled back atomically.
- Full RMB and persistent action-panel entry construction still duplicate some
  labels and availability checks. Cursor/hover/MMB resolution is unified, but a
  single rich `ActionContext` should eventually project every menu surface.
- Carry phase ownership is extracted and validated, while pathing and final
  structure placement remain an engine adapter in `OS0_IngameUI.cc`.
- Digging, salvage and generated container loot still contain UI projection next
  to engine mutation. Return-value events should separate simulation results from
  opening the corresponding object window.
- Impact particles are event-driven but rendered by the existing overlay file;
  a later `OverlayRenderer` extraction can reduce file size without changing
  ownership.

## Ownership rules for the refactor

`OS0TacticalSession` is the sole owner of OS0 tactical state. Its children have
value semantics; they are not separately allocated singletons. Long-lived actor
identity uses soldier IDs, not `SOLDIERTYPE*`.

```text
OS0TacticalSession
  gameplay
    assetDamage
    coverOrders
    carry
    sectorEconomy view of the current campaign
    assetCatalog
  interaction
    carry
    action/cursor
  events
    transient visual events
    diagnostics
```

Gameplay systems receive the state they own explicitly. The overlay receives a
const session view for rendering. Engine entry points publish gameplay events;
they never include the overlay header.

## Lifecycle

- process/campaign: versioned sector economy and asset-damage persistence;
- tactical sector: cover orders, carry state and world interaction session;
- UI screen: panels, mouse regions, render surfaces, selection and hover state;
- frame update: `UpdateOS0TacticalSession` advances gameplay;
- frame render: `RenderOS0IngameUI` projects the state and updates only
  presentation caches/dirty regions.

Sector exit cancels transient orders/carry but does not heal durable assets.
Save/load serializes durable state through the existing JA2 save stream. Removed
structures discard their damage records; newly placed structures receive a new
stable instance key.

## Verification gates

Every extraction must preserve a compiling intermediate state. The critical
automated tests cover parallel cover orders, order cancellation, level-aware
asset damage, lifecycle persistence, action-resolution parity, economy
serialization/migration, catalog overrides, carry cancellation, window-manager
state, native child-region Z ordering, clipped terrain brushes and guarded road
macro bounds. The v0.0.1.23 gate additionally covers transfer ownership, relation
capacity, double-click/stack boundaries, cross-region release, release outside all
regions and focus-loss recovery; the complete suite contains 191 tests.
