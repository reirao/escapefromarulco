# Modification notice

This repository is a modified derivative of JA2 Stracciatella. Work on the current
prototype was performed from 2026-07-24 through 2026-07-28.

Prominent notices were added to modified source files in accordance with the included
Strategy First source-code license agreement.

## Main changes

- Replaced the normal opening flow with a direct tactical-sector prototype start.
- Added the OS0 character-creation flow and selectable attributes and traits.
- Reworked tactical UI ownership, visibility and input routing.
- Added draggable character, context, tools, actions and object-inventory windows.
- Added world-object selection, contextual actions, looting and drag-and-drop behavior.
- Added structure carrying/repositioning and preliminary physical material profiles.
- Added tactical zoom handling and coordinate conversion.
- Added early terrain digging with persistent land/object map changes.
- Added in-game playtest feedback reports with contextual event and engine logs.
- Disabled or bypassed obsolete laptop/IMP paths used by the original game flow.
- Added deterministic first-open container loot without world-item locator flashes.
- Added distinct timber, stone, scrap and topsoil item models in unused item slots.
- Classified interactive world assets by structure flags and physical material.
- Added persistent dismantling, harvesting and material-producing terrain actions.
- Added save-compatible per-sector stockpiles and three buildable sector upgrades.

The code is experimental and does not represent an official JA2 Stracciatella release.

## v0.1.1-alpha hotfix

- Removed an unsupported String Theory alignment specifier that crashed on the first
  recorded context action with `Unexpected character in format string`.
- Made diagnostic event capture exception-safe so logging cannot terminate gameplay.
- Moved draggable-window coordinate and hit-region synchronization to one update per
  rendered frame, eliminating raw mouse-event/render races and reducing drag artifacts.

## v0.2.0-alpha resource loop

- Wooden and metal doors, fences, furniture, containers, stone deposits and debris now
  receive distinct inspector names and material-aware actions.
- Nearby salvageable assets can be dismantled with the field tool. The world structure
  is removed through the engine map-change system and produces physical material stacks.
- Digging produces topsoil or stone instead of only changing the ground graphic.
- Containers are seeded once with deterministic materials plus useful or damaged gear;
  an invisible savegame marker prevents repeated refilling after they are emptied.
- Double-clicking a material in object inventory deposits it into the current sector.
- The new `SECT` panel builds a Field Shelter, Salvage Workshop and Secure Depot. Stockpile
  counts and completed upgrades are stored in the sector information saved by JA2.

## v0.3.0-alpha graphical command surface

- Replaced the tactical assignment popup on merc right-button long-hold with one
  consistent, graphical character command surface based on JA2's original 3x3 menu.
- The original movement, stance, look, talk, hand and action buttons remain intact.
- A second in-game frame adds character sheet, inventory, stealth, weapon mode,
  reload and God-mode icon-library actions.
- Long-holding over another owned merc now selects that merc before opening the menu.
- Added an in-game atlas of the 24 reusable action and door symbols contained in
  JA2's `newicons3.sti` and `door_op2.sti`; the selected symbol becomes the God button.

## v0.4.0-alpha community asset catalog

- Added `GOD / CATALOG ASSET` to world-object context and action menus.
- Assets are keyed by tileset plus canonical base-tile ID, so multi-tile scenery is
  recorded once rather than once for every clicked segment.
- The editor proposes category, material and the real structure footprint, then allows
  the player to set a free name, category, material, role, width, height and whether
  the asset is buildable/placeable.
- Catalog entries immediately override generic inspector names and feed material,
  resource type and footprint into salvage classification and blueprint labels.
- The shareable database is stored at `%APPDATA%\JA2\AssetCatalog\os0-assets.tsv`.
- Feedback reports embed the complete catalog snapshot, and the report collector copies
  the raw TSV. Reviewed community records can be promoted into the version-controlled
  `assets/externalized/os0-assets.tsv`; local records load afterward as overrides.

## v0.5.0-alpha contextual field interaction

- Hover now selects the most useful cursor for the target: use/loot for containers,
  carry for portable scenery, inspect for fixed assets, talk for contacts and attack
  for armed hostile targets.
- Middle click cycles only through actions that are valid for the current target.
- Owned-merc context menus include `AI / RUN TO COVER + STANCE`. The command uses
  JA2's threat-aware cover evaluator and a geometry fallback in peaceful sectors;
  low cover ends prone and taller cover ends crouched.
- Carried structures use their real in-game tile graphic. The graphic bobs over the
  moving merc and a separate marker shows the destination.
- Weapon impacts emit small material-coloured chips. Explicitly catalogued salvage
  and resource assets gain conservative gunfire durability and drop their material
  when destroyed; map-critical structures keep vanilla behavior.

## v0.5.1-alpha wiring audit

- Moved object-hover handling from the viewport button callback to JA2's actual
  viewport movement callback and clear hover state when the pointer leaves the world.
- Routed double-click, context-menu and persistent action-panel container opening
  through the same seeding and inventory initialization path.
- Removed a potential use-after-free in structure impacts by assigning OS0 resource
  durability before vanilla structure damage can remove the target.
- Made the Field Shelter's advertised recovery role functional.
- Added `FEATURE_WIRING.md`, including explicit boundaries for metadata-only
  blueprints, surface digging, early physics, JA2 1.13 and multiplayer.
- Rebuilt the OS0 presentation around the concept artwork: full-width eight-module
  command bar, quiet corner branding, live system/event log, thin red-black field
  panels and a deterministic wide/compact workspace layout.
- The command strip routes every module to its real wired character/gameplay window;
  the first module is now the compact OS0 expand/collapse control.

### Post-v0.5.1 interaction follow-up

- Reserved the 38-pixel OS0 command strip outside the tactical world viewport, so
  zoom rendering, mouse hit conversion and edge scrolling use one shared boundary.
- Zoom no longer cancels an active environment carry operation or restores a stale
  zoom buffer.
- Carried scenery remains previewed at the selected placement tile while the merc
  walks there instead of obscuring the merc sprite.
- Window positions and the expanded/collapsed OS0 state persist in
  `%APPDATA%/JA2/OS0/os0-ui-layout.tsv` and scale safely across resolutions.
- OS0 now starts as a compact launcher and expands into the live eight-module bar.
  The bar, panel headers, squad commands and context actions use original JA2 pixel
  glyphs from `newicons3.sti` and `door_op2.sti`.

### Live-object and strategy follow-up

- World assets now generate their UI symbol from the selected JA2 tile itself. The
  stable OS0-owned pixel copy appears in inspection, action and container windows.
- Selecting an asset plays a short diagrammatic exploded-view animation at its world
  position and then settles into a compact object marker.
- `STRATEGIC MAP` no longer leaves tactical mode. It opens the movable live-strategy
  window with base construction, a clickable 16x16 control map and a team roster that
  can select and center operators in the loaded sector.
- Replaced the freezing vanilla `DETAILS / ATTACHMENTS` modal with a native OS0 item
  view showing condition, stack count, ammunition and installed attachments.
- Zoom cropping now follows the map edge reached by JA2's camera, allowing the actual
  left, right, top and bottom boundaries to become visible at 2x zoom.
- Added opaque-pixel scenery hit-testing across neighbouring object and structure
  layers. Large wrecks, debris, trees and multi-cell props can now be targeted on the
  visible part of their sprite instead of only on their hidden anchor cell.
- Corrected the Windows playtest manifest: the dynamically linked launcher requires
  FLTK, PNG, JPEG and zlib runtime DLLs. They are now bundled beside the executable;
  they are not Windows or graphics-driver components.

## v0.5.3-alpha spatial character inventory

- Replaced the character right-click list with a circular, actor-anchored JA2 icon
  ring while retaining text menus for detailed item and weapon state.
- Added an in-world equipment explosion for helmet, face, armour, legs and both hands.
  Every displayed object is the original saveable JA2 inventory slot and supports the
  native item cursor; PACK toggles the character's real pocket container.
- Added a modal quantity chooser before dragging a multi-object player stack.
- Split environmental handling into lift and drag presentation according to material
  mass versus wound-adjusted strength capacity. The original structure remains alive
  until collision-valid placement commits atomically.
- Successful structure handling awards persistent JA2 strength sub-points, extending
  the engine's existing learn-by-doing system to environmental work.
