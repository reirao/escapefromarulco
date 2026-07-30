# Escape from Arulco

An experimental, non-commercial rework of *Jagged Alliance 2* built on
[JA2 Stracciatella](https://github.com/ja2-stracciatella/ja2-stracciatella).

> **Early alpha:** this build is intentionally unfinished. Expect rough controls,
> incomplete systems and crashes. The purpose of this release is hands-on testing.

Current stable playtest: **0.0.1.13**. This checkpoint adds the shared contextual
character hub, audited `F` perception route and explicit NORMAL/COMBAT control mode
on top of the `v0.0.1.12` runtime.

## Download and play

1. Open the [v0.0.1.13 release](https://github.com/reirao/escapefromarulco/releases/tag/v0.0.1.13).
2. Download `Escape-from-Arulco-Playtest-v0.0.1.13.zip`.
3. Extract the complete ZIP into a writable folder.
4. Run `START_PLAYTEST.cmd`.
5. In the launcher, select your own legally installed JA2 or JA2 Gold directory.
6. Select the correct game language and start in windowed mode.

The playtest includes the Windows executable, launcher, runtime DLLs, externalized
Stracciatella data, test instructions and crash-log collector. It does **not** contain
the original JA2 game data. Windows x64 and an existing JA2 installation are required.

## Current prototype

- Direct tactical-sector start without helicopter arrival or starting enemies
- One modal in-world character creator with a validated name, freely distributed
  attributes and two selectable traits; the obsolete laptop creator is retired
- One UI runtime and one viewport layout boundary shared by creator, windows and input
- A fixed 38-pixel command dock outside the scrolling/zoomed tactical world
- Character inventory is optional and opens from the shared character hub or its
  `CHARACTER` command-dock entry
- Object-first character equipment, pocket and container views anchored in the world
- One compact field-computer icon with movable Base/Map/Team/Report, Inspector and
  Toolbox windows whose positions persist by stable names
- Character and container inventories with item drag and drop
- Context-sensitive object, weapon, loot and stance actions
- One shared categorized character hub, opened either with `F` over the owned merc
  or with the first `CHARACTER` dock button. Its root pages are `ACTIONS`,
  `ABILITIES / TALENTS`, `EQUIPMENT`, `GROUP` and `GOD`; ability entries are shown
  only when they invoke a real engine-backed action
- Hover-selected context cursors with middle-click cycling through valid actions
- Player-issued AI command for running to cover and choosing prone/crouched stance
- Extended graphical JA2 long-hold menu for character, inventory and combat actions
- Star-menu God hub with a movable current-sector asset library and the existing
  24-symbol JA2 interface atlas
- Live asset cards with the real tile sprite, occurrence count, material, footprint,
  salvage yield, durability/tool profile and one-click focus/cataloguing
- In-game asset catalog editor with custom names, categories, materials, roles and
  measured multi-tile footprints
- Movable world structures and early material/physics properties
- In-world carried asset sprites with movement bob and destination marker
- Material-coloured impact chips and weapon damage for catalogued resource assets
- Material-aware doors, furniture, fences, stone, debris and resource objects
- Persistent container seeding with useful loot and recoverable materials
- Dismantling and digging that produce timber, stone, scrap and topsoil
- Material-specific debug tools for debris, fences, trees, metal and earth
- Per-sector stockpiles and buildable shelter, workshop and secure depot upgrades
- Movable live-world editor with active-tileset assets, editor-safe items, NPC/RPC
  profiles, category filters, placement/removal, terrain brushes and native road macros
- Empty-map and `live-editor.dat` load/save workflow that keeps the player squad and
  rolls a failed world replacement back to a temporary map snapshot
- Zoomed tactical view
- Explicit NORMAL/COMBAT dock switch with mouse aim, direct single-shot LMB fire,
  realtime WASD movement and AP-safe one-tile WASD control during player turns
- Built-in playtest feedback reports

This is a systems prototype, not yet a balanced campaign.
The exact implementation status and current boundaries are recorded in
[FEATURE_WIRING.md](FEATURE_WIRING.md).

## Controls and feedback

- **F over your merc:** open the same categorized character hub as the first
  `CHARACTER` command-dock button
- **F over a world object or terrain:** perceive the exact hovered relation, enable
  the nearby environment scan and expose its safe object actions without executing
  an action automatically
- **Alt+F:** retain JA2's tracking shortcut
- **Escape:** close the active radial and restore the previously visible windows
- **Right click:** context menu for the object or terrain under the cursor
- **Hold right click:** graphical character/movement menu; its star opens the Asset
  Library/JA2 icon hub and the other symbols control character, gear and combat
- **Middle click:** cycle the current cursor action
- **Double click:** open a character, container or world object
- **Bottom command dock:** open Character, Inspector, world tools, assets, strategy
  and sandbox modules; it remains fixed while the tactical camera moves
- **TARGET/WALK symbol in the first dock cell:** switch between `COMBAT` and `NORMAL`.
  In combat mode the merc follows the mouse, WASD moves, Shift sprints in realtime,
  Q/E turns and LMB fires. Burst/throw/trajectory attacks confirm on the next click.
- **Computer icon:** toggle the Toolbox; the Strategy symbol opens Base, Arulco
  map/radar, Team and Report
- **GOD / CATALOG ASSET:** right-click a world asset and classify it for the shared
  construction/resource database
- **Asset Library:** left-click a card to center its real map instance; right-click the
  card to edit its reusable catalog record
- **TERRAIN / Live Editor:** choose `TILES`, `ITEMS`, `NPCS` or `SYSTEM`, filter a
  category, select `PLACE`/`ERASE`, then click the tactical world. Terrain and water
  use adjustable paint/smooth brushes; roads cycle the original 32 JA2 road macros.
  `EMPTY MAP` requires a second confirmation click. `SYSTEM` contains `LOAD MAP`,
  while the dedicated command saves `%APPDATA%\JA2\OS0\maps\live-editor.dat`.

Asset classifications are written to `%APPDATA%\JA2\AssetCatalog\os0-assets.tsv`.
This file can be shared between testers without distributing original JA2 graphics.
Every saved feedback report embeds a catalog snapshot. Reviewed records are promoted
into the bundled curated database in later builds; a tester's local records override it.

In the computer's `REPORT` tab, choose a category, describe the problem and press
`SAVE REPORT`. Reports are saved to `%APPDATA%\JA2\Feedback` and include relevant
game context, recent OS0 actions and the tail of the engine log.

If the game crashes before a report can be saved, run `COLLECT_LAST_CRASH.cmd` from
the extracted playtest folder and attach the generated `reports` folder to a
[bug report](https://github.com/reirao/escapefromarulco/issues/new/choose).

See [PLAYTESTING.md](PLAYTESTING.md) for the full test checklist.

## Building from source

The project retains the Stracciatella build system. See [COMPILATION.md](COMPILATION.md).
The tested Windows build uses MSYS2 UCRT64/MinGW64, CMake and Ninja.

On the configured Windows development machine, double-click
`BUILD_AND_START_LOCAL.cmd`. It configures `C:\tmp\ja2-sandbox-build`, compiles
only changed files, refreshes the required MinGW/SDL runtime DLLs and starts the
new executable in windowed mode. Command-line arguments override the default
`-window` argument.

```bash
cmake /path/to/escapefromarulco -G Ninja -DWITH_UNITTESTS=OFF
ninja -j2 ja2.exe ja2-launcher.exe
```

The original Stracciatella README is preserved at
[docs/STRACCIATELLA_README.md](docs/STRACCIATELLA_README.md).

## Legal and attribution

- No original *Jagged Alliance 2* data files are included.
- A legally owned JA2 installation is required to play.
- This project is non-commercial.
- The source and derivative build are distributed under the included
  [Strategy First source-code license agreement](SFI%20Source%20Code%20license%20agreement.txt).
- Modified files carry a dated modification notice; the main changes are listed in
  [MODIFICATIONS.md](MODIFICATIONS.md).
- JA2 Stracciatella contributors retain credit for the engine work documented in
  [contributors.txt](contributors.txt).

*Jagged Alliance 2* and associated original assets belong to their respective owners.
