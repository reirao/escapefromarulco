# Feature wiring audit

This matrix compares public prototype claims with the source published as the
`v0.1.0` fragile-tested checkpoint. Later hardening is current source behaviour and
must be validated again before another package is published.

Status terms are deliberately narrow:

- **Implemented / manual verification required** means the route exists in source,
  but the complete interaction has not been certified across tilesets, saves, input
  sequences and combat states.
- **Automated core / manual integration pending** means focused value/state/policy
  tests cover the named core while the JA2 mouse, map and rendering adapters still
  require the same manual verification.
- **Published package verified** refers only to the tagged `v0.1.0` package inputs,
  not to an arbitrary newer working tree.

`PLAYTESTING.md` remains an acceptance checklist, not a record that every scenario
passed. Items under **Not implemented yet** are explicitly outside the current scope.

| Feature | Runtime status | Wiring / persistence |
| --- | --- | --- |
| Direct tactical start | Implemented / manual verification required | `JAScreens` starts the OS0 sandbox; the bootstrap creates the operator, clears enemies, marks the sector controlled and enters `GAME_SCREEN`. |
| Character creation | Implemented / manual verification required | One modal tactical creator owns the validated callsign, ten default/freely editable attributes, male/female JA2 tactical body selection and two freely selected traits. Completion persists in OS0 session state, applies permanent camouflage when selected, mutes the operator and does not reopen the duplicate laptop wrapper. |
| Field interaction tutorial | Implemented / manual verification required | A monotonic model locates an openable structure, marks it, and advances only through target-matched hover, action menu, `CONTENTS`, approach/open and item-taken events. Availability still depends on the current map containing a recognizable container. |
| UI runtime and viewport | Implemented / manual verification required | One runtime owns creator/panel transitions and one layout owns the full tactical viewport. The former fixed bottom dock is suppressed; one movable/minimizable multitool is projected in screen space over the world. |
| Object-first workspace | Implemented / manual verification required | Duplicate context/tools/actions/object panels and their hidden hit regions are retired. Inventory, loot, equipment, libraries, Sector, Inspector, Toolbox, Environment and the live editor all use one template/state manager; character inventory remains optional. |
| Unified window focus and persistence | Automated core / manual integration pending | `OS0WindowManager` owns bounds, visibility, suspension, drag, clamp, saved position and render Z order. The native JA2 child mouse-region list is projected from that same order, including immediate focus transfer when a header drag begins. |
| Object-derived interaction | Implemented / manual verification required | Hover selection uses the real map sprite, the stable world marker and the object-anchored icon fan. Container contents remain spatially arranged around their owning object instead of a generic list window. |
| Pixel-accurate scenery selection | Implemented / manual verification required | Hover and all mouse-button actions scan nearby visible object/structure sprites by opaque pixel instead of relying only on the ground cell. Multi-tile structures resolve to their canonical base object. |
| Character inventory drag/drop | Automated core / manual integration pending | JA2 remains the item carrier; `OS0ItemTransferController` owns the gesture and `OS0ItemTransferRuntime` owns a stable exact-source transaction. A destination commits only after accepting the full object. Invalid, full or occupied destinations keep it held; OS0 never infers an implicit slot swap. Cancel restores the exact valid source or leaves the complete item on the cursor. |
| Container inventory drag/drop | Automated core / manual integration pending | Double-click and right-click `OPEN CONTENTS` share one path; the container's own scaled world sprite becomes the centre of its spatial item layout. Source identity includes the owning container asset, and cross-region/lost-focus releases are recovered without invoking the world behind a panel. |
| Deterministic container loot | Implemented / manual verification required | First open seeds material and useful/damaged equipment. The seed marker is committed only with successful generation; a failed marker write rolls back that attempt so reopening can retry without accumulating duplicates. |
| Context actions | Implemented / manual verification required | Object, terrain, character, loot and weapon entries call real JA2 or OS0 actions rather than display-only labels. |
| Categorized character hub | Implemented / manual verification required | Plain `F` over the owned merc and the multitool `INFO` group enter the same registry-driven hub. Its root exposes `ACTIONS`, `ABILITIES / TALENTS`, `EQUIPMENT`, `GROUP` and `GOD`; abilities/talents are listed only when backed by an executable engine or OS0 action. Page selection remains inside the hub, `BACK` returns to its root and only leaf actions dispatch gameplay commands. |
| Character radial actions | Implemented / manual verification required | Right-clicking a merc anchors a circular JA2-icon action ring to that actor. Every icon owns a stable mouse region and keeps the detailed action name as hover text. |
| In-world equipment view | Implemented / manual verification required | Equipment expands around the merc and PACK unfolds only real JA2 slots. The shared relation policy checks item class, two-handed conflicts, body slot, stack capacity and carried weight, exposes one direct preferred destination and places alternatives behind `MORE OPTIONS`. Occupied destinations are not silently swapped. |
| Stack quantity transfer | Automated core / manual integration pending | Dragging a stack opens an OS0 quantity dialog with single-step, all, take and cancel controls before attaching the chosen count to JA2's native item cursor. |
| Mouse-event transfer boundary | Automated core / manual integration pending | A post-dispatch MouseSystem observer recovers cross-region `UP`, clears releases outside all regions, resets invalid double-click state and reconciles SDL focus loss. The drop target is projected freshly at physical release. |
| Perception trigger / hover / middle-click cycle | Implemented / manual verification required | Plain `F` freshly resolves the exact hovered relation and opens its contextual actions without auto-executing a leaf action. `F` over the owned merc instead opens the shared character hub; `Alt+F` remains vanilla tracking. Hover uses the viewport movement callback and middle click cycles the same target's valid action set. |
| Relational action and approach queue | Automated core / manual integration pending | Hover, `F`, RMB, MMB, environment panels and execution consume one ordered target-bound resolver. Physical actions report immediate, move-to-range or blocked state; queued approaches use native pathing and revalidate actor, sector, level, target and relation before the action runs. |
| Layered cancellation | Automated core / manual integration pending | One physical Escape press chooses one highest-priority layer: modal/drag, held item, world manipulation, queued approach, then cursor/combat. Player movement, combat entry, target mutation, path interruption and timeout also cancel owned pending work. |
| Hub modal focus | Implemented / manual verification required | Opening a modal hub surface temporarily suspends other OS0 windows without destroying their visibility or saved layout. Closing the modal restores the previously visible windows; category navigation itself does not close the hub. |
| Long-hold graphical merc menu | Implemented / manual verification required | JA2's original 3x3 movement frame remains active and a second frame invokes character, inventory, stealth, weapon, reload and icon-library actions. |
| AI run-to-cover command | Automated core / manual integration pending | Uses JA2's cover evaluator, a peaceful-sector geometry fallback, engine pathing and a prone/crouch arrival stance. |
| God icon atlas | Implemented / manual verification required | Loads 24 symbols from JA2 STI resources and feeds the selected symbol back into the extended command menu. |
| Star-menu asset library | Implemented / manual verification required | Scans the live sector, deduplicates real structure/object tile sprites, filters all/uncatalogued/debris records, focuses instances and opens the persistent catalog editor. |
| Debris tool profiles | Implemented / manual verification required | Hover and catalog cards expose category, resolved material, footprint, durability, yield and the required shovel/crowbar/cutters/toolkit/cutting tool. Debug startup issues the save-compatible test tools. |
| Animation-following gear UI | Implemented / manual verification required | Equipment, pocket and item-transfer symbols anchor to JA2's interpolated soldier screen position instead of snapping with `GridNo`; item-pointer mode no longer triggers aim auto-collapse. The conventional character sheet remains an explicitly toggled alternate inventory. |
| Embedded map/minimap management | Implemented / manual verification required | The movable computer reads `StrategicMap`/`SectorInfo` directly for control, operators, enemies and militia, while the tactical minimap is blitted from JA2's real radar asset. |
| Asset catalog | Automated core / manual integration pending | Records tileset/base-tile key, inferred footprint, custom name/category/material/role/size and buildable metadata to a user TSV; local data overrides the bundled catalog. |
| Material/physics profile | Automated core / manual integration pending | Engine structure data determines mass, friction, restitution, integrity and carrying capacity. This is not a general rigid-body simulation. |
| Structure carry/reposition | Automated core / manual integration pending | Material mass, strength and wounds determine eligibility and whether the merc lifts or drags. The real tile sprite follows above or behind the actor, placement stays transactional through JA2 collision checks, and successful handling trains persistent strength sub-points. |
| Weapon impact chips and asset durability | Automated core / manual integration pending | OS0 owns damage for salvageable/resource assets before vanilla can invalidate the structure pointer; critical geometry remains JA2-owned. Destroyed map objects and material drops persist. |
| Salvage and surface digging | Implemented / manual verification required | Nearby tool-gated actions remove map objects/surface layers through map-temp recording and create physical timber, stone, scrap or soil stacks. Digging is surface editing, not deep voxel terrain. |
| Sector stockpiles/upgrades | Automated core / manual integration pending | Four resource counters and three upgrade flags use reserved bits in saved `SECTORINFO`. Workshop and depot alter yields; the built shelter can be clicked to recover the current team. |
| Tactical zoom | Automated core / manual integration pending | World rendering and display/world input coordinates share the same zoom transform. Screen projection is refreshed after camera scroll and before world rendering; a viewport-signature cache barrier handles geometry changes. Actual map-edge coverage still requires manual checks at each zoom. |
| Movable OS//0 multitool | Automated core / manual integration pending | One persisted screen-space anchor expands left or right into Normal/Combat, Interaction, Salvage, Inspect, Info and God controls. Drag moves it; double-click minimizes it to one icon or restores it. This replaces the old fixed multi-button strip. |
| NORMAL / COMBAT control mode | Automated core / manual integration pending | One stateful multitool switch owns the canonical mode. UI-consumed movement keys are suppressed until release; held Shift is the sprint modifier; post-scroll pointer-facing and guarded native-event projection retain JA2 tile/path animation. LMB direct fire is limited to a valid ordinary single-shot firearm path; burst/throw/trajectory retain native confirmation. |
| Realtime field editor | Automated core / manual integration pending | `TERRAIN` opens a movable catalog UI for the active tileset, editor-safe items, generic actors and NPC/RPC profiles. Category filters, variants, layers, quantity and facing feed typed frame-boundary commands; terrain paint/smooth and all 32 road macros reuse guarded native JA2 editor algorithms. |
| Empty/load/save world workflow | Automated core / manual integration pending | `EMPTY MAP` uses two-click confirmation; save/load uses `%APPDATA%\JA2\OS0\maps\live-editor.dat`. World replacement preserves the player squad and creates a temporary map snapshot before teardown, restoring map, squad, selection, camera and ambience on failure. This is not full tactical-state undo. |
| Live tactical strategy window | Implemented / manual verification required | `STRATEGIC MAP` opens a movable live window with base upgrades, the 16x16 control map and a clickable team roster. Travel plotting, assignments, militia and finance still remain future ports from the legacy Map Screen. |
| Feedback reports | Implemented / manual verification required | Writes tester text, game state, recent OS0 events, engine-log tail and asset-catalog snapshot under `%APPDATA%\JA2\Feedback`. |
| Portable Windows runtime | Published package verified | The tagged playtest runtime includes FLTK, image-codec, SDL and MinGW DLL dependencies; testers do not need MSYS2, developer tools or GPU-specific libraries. Any newly assembled archive must pass package inspection again. |

## Not implemented yet

- Player-economy blueprint construction of arbitrary catalogued assets. The debug live
  editor can author active-tileset assets, while `buildable/placeable` in normal play
  still classifies a future blueprint and changes its action/inspector label.
- General edit-history undo/redo. World replacement has a recovery snapshot, but map
  `.dat` files cannot restore transient projectiles, physics events or active bomb timers.
- A deep, voxel-like terrain volume or general-purpose rigid-body physics engine.
- JA2 1.13 data/code integration.
- Network multiplayer or synchronized co-op simulation.
- Removable nested backpack items with their own independent saved contents. `PACK`
  currently exposes the merc's real persistent pocket slots as one container.
- Continuous rigid-body pushing/rotation for map structures. Movement currently keeps
  the original map node alive until a collision-valid destination is committed and
  animates lift/drag along the merc; it does not solve structure collisions every frame.
- A balanced campaign, enemy progression or production economy.
- Live travel plotting, assignments, militia management and finance inside OS0 windows.

These are roadmap items, not current playtest promises.
