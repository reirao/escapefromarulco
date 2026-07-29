# Escape from Arulco / OS0 tactical architecture

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
| `BuildContextCursorActions` | typed target facts | none | world queries | `OS0_ActionRegistry::ResolveOS0CursorActions` | hover and MMB share one ordered resolver |
| `AddContextEntry`, `RefreshPanelActions` | target, catalog, inventory | presentation entry arrays | world/inventory queries | context UI | full menu/action-panel projection is remaining debt |
| `ContextActionCallback`, `OS0HandleCursorAction` | typed action and target | canonical simulation and UI projection | items, stance, weapon, world mutation | one typed execution callback plus cursor adapter | no numeric action meanings remain; callback extraction remains debt |
| `OS0CycleCursorAction`, `ApplyCursorTool` | registry cursor descriptors | session cursor/attack state | JA2 UI mode events | session cursor state and action registry | completed typed mapping (`MOVE`, `USE`, `CARRY`, etc.) |
| inventory and item-transfer callbacks | real soldier inventory, item pointer | real inventory and world pools | `PlaceObject`, `AutoPlaceObject` | UI callback plus `OS0_ItemRelations` | UI callback invokes item-relation service |
| panel positioning/dragging/regions | `OS0UIRuntime`, `OS0UILayout` and window geometry | visibility transitions and drag positions | mouse system | runtime plus engine adapter | one logical owner; engine-region extraction remains |
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
CHARACTER dock command.

The 38-pixel command dock is outside `gsVIEWPORT_END_Y`. Camera scrolling,
world zoom and south-edge hit testing therefore share the same world boundary
and can never translate or steal input from the dock. Creator input is modal,
fixed in screen space and pauses camera scrolling until entry is complete.

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

## Remaining technical debt

- Geometry and mouse-region objects still live in `OS0_IngameUI.cc` because they
  wrap JA2 engine types. Logical visibility, creator flow and dock geometry have
  moved to `OS0_UIRuntime`; remaining geometry can move renderer-by-renderer
  without inventing another state owner.
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
serialization/migration, catalog overrides and carry cancellation.
