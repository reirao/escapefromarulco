# Feature wiring audit

This matrix compares the public prototype claims with the actual runtime path in
the source. It was last verified for `v0.5.1-alpha`.

| Feature | Runtime status | Wiring / persistence |
| --- | --- | --- |
| Direct tactical start | Verified | `JAScreens` starts the OS0 sandbox; the bootstrap creates the operator, clears enemies, marks the sector controlled and enters `GAME_SCREEN`. |
| Character creation | Verified | Name, ten attributes and two freely selected traits are written to both the live soldier and merc profile. |
| Artwork workspace and movable/minimizable windows | Verified | A compact OS0 launcher expands into an eight-module command bar controlling character, context, tools, actions, object inventory, live strategy and feedback windows. Positions persist in the user profile and scale across resolutions; panels keep independent drag/close regions and the event log uses real recorded OS0 actions. |
| Object-derived UI symbols | Verified | The selected world tile is rendered once into an OS0-owned pixel surface and reused by context, interaction and container windows. Selection uses a stable in-world marker, while right-click opens a real object-anchored icon fan with independent clickable regions and hover labels. |
| Pixel-accurate scenery selection | Verified | Hover and all mouse-button actions scan nearby visible object/structure sprites by opaque pixel instead of relying only on the ground cell. Multi-tile structures resolve to their canonical base object. |
| Character inventory drag/drop | Verified | Uses JA2's item-pointer and placement functions, so slot rules and item stacks remain engine-owned. |
| Container inventory drag/drop | Verified | Double-click, right-click `OPEN CONTENTS` and the actions panel now share one open path. |
| Deterministic container loot | Verified | First open seeds material and useful/damaged equipment; an invisible world-item marker prevents refilling and is saved with sector world items. |
| Context actions | Verified | Object, terrain, character, loot and weapon entries call real JA2 or OS0 actions rather than display-only labels. |
| Hover cursor / middle-click cycle | Verified after audit fix | Hover is connected to the viewport movement callback; middle click cycles the valid action set for the current target. |
| Long-hold graphical merc menu | Verified | JA2's original 3x3 movement frame remains active and a second frame invokes character, inventory, stealth, weapon, reload and icon-library actions. |
| AI run-to-cover command | Verified | Uses JA2's cover evaluator, a peaceful-sector geometry fallback, engine pathing and a prone/crouch arrival stance. |
| God icon atlas | Verified | Loads 24 symbols from JA2 STI resources and feeds the selected symbol back into the extended command menu. |
| Asset catalog | Verified | Records tileset/base-tile key, inferred footprint, custom name/category/material/role/size and buildable metadata to a user TSV; local data overrides the bundled catalog. |
| Material/physics profile | Verified as an early gameplay model | Engine structure data determines mass, friction, restitution, integrity and carrying capacity. This is not a general rigid-body simulation. |
| Structure carry/reposition | Verified for eligible single-tile structures | Strength/health and structure flags gate carrying; the real tile sprite follows the merc and placement uses JA2 collision checks. |
| Weapon impact chips and asset durability | Verified after audit fix | OS0 owns damage for salvageable/resource assets before vanilla can invalidate the structure pointer; critical geometry remains JA2-owned. Destroyed map objects and material drops persist. |
| Salvage and surface digging | Verified | Nearby tool-gated actions remove map objects/surface layers through map-temp recording and create physical timber, stone, scrap or soil stacks. Digging is surface editing, not deep voxel terrain. |
| Sector stockpiles/upgrades | Verified | Four resource counters and three upgrade flags use reserved bits in saved `SECTORINFO`. Workshop and depot alter yields; the built shelter can be clicked to recover the current team. |
| Tactical zoom | Verified | World rendering and display/world input coordinates share the same zoom transform; edge-aware crop bias exposes the actual map boundary when JA2's camera reaches it. |
| Live tactical strategy window | Verified as an initial replacement | `STRATEGIC MAP` now opens a movable live window with base upgrades, the 16x16 control map and a clickable team roster. Travel plotting, assignments, militia and finance still remain future ports from the legacy Map Screen. |
| Feedback reports | Verified | Writes tester text, game state, recent OS0 events, engine-log tail and asset-catalog snapshot under `%APPDATA%\JA2\Feedback`. |
| Portable Windows runtime | Verified after package fix | The playtest runtime includes FLTK, image-codec, SDL and MinGW DLL dependencies; testers do not need MSYS2, developer tools or GPU-specific libraries. |

## Not implemented yet

- Placement/building of arbitrary catalogued assets. `buildable/placeable` currently
  classifies a future blueprint and changes its action/inspector label.
- A deep, voxel-like terrain volume or general-purpose rigid-body physics engine.
- JA2 1.13 data/code integration.
- Network multiplayer or synchronized co-op simulation.
- A balanced campaign, enemy progression or production economy.
- Live travel plotting, assignments, militia management and finance inside OS0 windows.

These are roadmap items, not current playtest promises.
