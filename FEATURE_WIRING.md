# Feature wiring audit

This matrix compares the public prototype claims with the actual runtime path in
the source. It was last verified for `v0.0.1.12`.

| Feature | Runtime status | Wiring / persistence |
| --- | --- | --- |
| Direct tactical start | Verified | `JAScreens` starts the OS0 sandbox; the bootstrap creates the operator, clears enemies, marks the sector controlled and enters `GAME_SCREEN`. |
| Character creation | Verified | One modal tactical creator owns the validated callsign, ten attributes and two freely selected traits. Completion persists in OS0 session state; the duplicate laptop wrapper has been removed. |
| UI runtime and viewport | Verified | One runtime owns creator/panel transitions and one layout owns the fixed 38-pixel command dock. World rendering, scrolling and exit regions stop above that dock. |
| Object-first workspace | Verified | Duplicate context/tools/actions/object panels and their hidden hit regions are retired. Inventory, loot, equipment, libraries, Sector, Inspector, Toolbox, Environment and the live editor all use one template/state manager; character inventory remains optional. |
| Unified window focus and persistence | Verified after manager migration | `OS0WindowManager` owns bounds, visibility, suspension, drag, clamp, saved position and render Z order. The native JA2 child mouse-region list is projected from that same order, including immediate focus transfer when a header drag begins. |
| Object-derived interaction | Verified | Hover selection uses the real map sprite, the stable world marker and the object-anchored icon fan. Container contents remain spatially arranged around their owning object instead of a generic list window. |
| Pixel-accurate scenery selection | Verified | Hover and all mouse-button actions scan nearby visible object/structure sprites by opaque pixel instead of relying only on the ground cell. Multi-tile structures resolve to their canonical base object. |
| Character inventory drag/drop | Verified | Uses JA2's item-pointer and placement functions, so slot rules and item stacks remain engine-owned. |
| Container inventory drag/drop | Verified | Double-click and right-click `OPEN CONTENTS` share one path; the container's own scaled world sprite becomes the centre of its spatial item layout. |
| Deterministic container loot | Verified | First open seeds material and useful/damaged equipment; an invisible world-item marker prevents refilling and is saved with sector world items. |
| Context actions | Verified | Object, terrain, character, loot and weapon entries call real JA2 or OS0 actions rather than display-only labels. |
| Character radial actions | Verified | Right-clicking a merc anchors a circular JA2-icon action ring to that actor. Every icon owns a stable mouse region and keeps the detailed action name as hover text. |
| In-world equipment view | Verified as an initial RPG inventory layer | Equipment expands around the merc and PACK unfolds only the real pocket slots. Dragging an item onto a character exposes registry-driven hand, pack and drop intents with safe swap/fallback behavior. |
| Stack quantity transfer | Verified for player inventory/equipment | Dragging a stack opens an OS0 quantity dialog with single-step, all, take and cancel controls before attaching the chosen count to JA2's native item cursor. |
| Hover cursor / middle-click cycle | Verified after audit fix | Hover is connected to the viewport movement callback; middle click cycles the valid action set for the current target. |
| Long-hold graphical merc menu | Verified | JA2's original 3x3 movement frame remains active and a second frame invokes character, inventory, stealth, weapon, reload and icon-library actions. |
| AI run-to-cover command | Verified | Uses JA2's cover evaluator, a peaceful-sector geometry fallback, engine pathing and a prone/crouch arrival stance. |
| God icon atlas | Verified | Loads 24 symbols from JA2 STI resources and feeds the selected symbol back into the extended command menu. |
| Star-menu asset library | Verified | Scans the live sector, deduplicates real structure/object tile sprites, filters all/uncatalogued/debris records, focuses instances and opens the persistent catalog editor. |
| Debris tool profiles | Verified | Hover and catalog cards expose category, resolved material, footprint, durability, yield and the required shovel/crowbar/cutters/toolkit/cutting tool. Debug startup issues the save-compatible test tools. |
| Animation-following gear UI | Verified | Equipment, pocket and item-transfer symbols anchor to JA2's interpolated soldier screen position instead of snapping with `GridNo`; item-pointer mode no longer triggers aim auto-collapse. The conventional character sheet remains an explicitly toggled alternate inventory. |
| Embedded map/minimap management | Verified | The movable computer reads `StrategicMap`/`SectorInfo` directly for control, operators, enemies and militia, while the tactical minimap is blitted from JA2's real radar asset. |
| Asset catalog | Verified | Records tileset/base-tile key, inferred footprint, custom name/category/material/role/size and buildable metadata to a user TSV; local data overrides the bundled catalog. |
| Material/physics profile | Verified as an early gameplay model | Engine structure data determines mass, friction, restitution, integrity and carrying capacity. This is not a general rigid-body simulation. |
| Structure carry/reposition | Verified for eligible single-tile structures | Material mass, strength and wounds determine eligibility and whether the merc lifts or drags. The real tile sprite follows above or behind the actor, placement stays transactional through JA2 collision checks, and successful handling trains persistent strength sub-points. |
| Weapon impact chips and asset durability | Verified after audit fix | OS0 owns damage for salvageable/resource assets before vanilla can invalidate the structure pointer; critical geometry remains JA2-owned. Destroyed map objects and material drops persist. |
| Salvage and surface digging | Verified | Nearby tool-gated actions remove map objects/surface layers through map-temp recording and create physical timber, stone, scrap or soil stacks. Digging is surface editing, not deep voxel terrain. |
| Sector stockpiles/upgrades | Verified | Four resource counters and three upgrade flags use reserved bits in saved `SECTORINFO`. Workshop and depot alter yields; the built shelter can be clicked to recover the current team. |
| Tactical zoom | Verified | World rendering and display/world input coordinates share the same zoom transform; edge-aware crop bias exposes the actual map boundary when JA2's camera reaches it. The command dock is not part of the zoomed or scrollable viewport. |
| Realtime field editor | Verified as a sandbox authoring layer | `TERRAIN` opens a movable catalog UI for the active tileset, editor-safe items, generic actors and NPC/RPC profiles. Category filters, variants, layers, quantity and facing feed typed frame-boundary commands; terrain paint/smooth and all 32 road macros reuse guarded native JA2 editor algorithms. |
| Empty/load/save world workflow | Verified with map-level recovery | `EMPTY MAP` uses two-click confirmation; save/load uses `%APPDATA%\JA2\OS0\maps\live-editor.dat`. World replacement preserves the player squad and creates a temporary map snapshot before teardown, restoring map, squad, selection, camera and ambience on failure. This is not full tactical-state undo. |
| Live tactical strategy window | Verified as an initial replacement | `STRATEGIC MAP` now opens a movable live window with base upgrades, the 16x16 control map and a clickable team roster. Travel plotting, assignments, militia and finance still remain future ports from the legacy Map Screen. |
| Feedback reports | Verified | Writes tester text, game state, recent OS0 events, engine-log tail and asset-catalog snapshot under `%APPDATA%\JA2\Feedback`. |
| Portable Windows runtime | Verified after package fix | The playtest runtime includes FLTK, image-codec, SDL and MinGW DLL dependencies; testers do not need MSYS2, developer tools or GPU-specific libraries. |

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
