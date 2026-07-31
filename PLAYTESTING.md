# Playtesting Escape from Arulco

Thank you for testing this early prototype. Specific reproduction steps are much more
useful than a general statement that something did not work.

> **v0.0.1.23 STABLE CHECKPOINT / EARLY ALPHA:** source, Windows package, automated
> suite and startup path are the tested baseline. This checklist contains broader
> gameplay acceptance scenarios and is not a statement that every map/state combination
> has passed. Mark each result PASS, FAIL or NOT REACHED in your report.

## Setup

- Windows x64
- A legally owned JA2 or JA2 Gold installation
- The ZIP from the latest GitHub release, fully extracted
- Windowed mode recommended

The packaged launcher includes its own FLTK and image-codec DLLs. Windows, the GPU
driver and MSYS2 are not expected to provide `libfltk-1.4.dll` or related files.

Never copy the playtest files over the original JA2 installation. Keep the extracted
playtest in its own folder and point the launcher to the original game directory.

## Suggested test pass

1. Start a fresh campaign and complete the single in-world creator using a custom
   name, attributes and two traits. Verify no laptop creator or forced inventory opens.
2. Scroll and zoom the world while the creator, OS//0 multitool and movable windows
   are visible. The screen-space controls must not scroll with the world and the mouse
   cursor must remain usable. Double-click and drag the multitool, open its five groups,
   reload and verify the tool and movable-window positions are preserved.
3. Hold the right mouse button over two different owned mercs. Verify the correct merc
   is selected and both graphical JA2 button frames appear.
4. Click the star in the second frame. Verify the movable Asset Library opens first,
   `ACTION ICONS` exposes the old symbol atlas, and a selected symbol becomes the
   star/library button on the next long-hold.
5. Use stealth, weapon mode and reload from the extended frame.
6. Right-click a sandbag, door, container, stone field and large scenery object. Use
   `GOD / CATALOG ASSET`, give it a name and verify category/material/role and footprint.
7. Save, reopen the same object and verify its catalog entry persists. For a multi-tile
   object, click another segment and verify it resolves to the same base entry.
8. Mark an asset buildable and verify the action panel calls it a placeable blueprint.
9. Inspect `%APPDATA%\JA2\AssetCatalog\os0-assets.tsv`; it should contain no graphics,
   only shareable metadata.
10. Equip, swap and drop items using drag and drop.
11. Right-click weapons and exercise every enabled weapon action.
12. Open several containers; verify each contains material plus useful or damaged gear.
13. Double-click timber, stone, scrap and soil; verify the `SECT` stockpile increases.
14. Dismantle a wooden door, fence or furniture object and loot its material output.
15. Salvage stone/debris and right-click nearby grass or dirt to use `DIG`.
16. Build all three upgrades in `SECT`; verify the workshop/depot bonuses affect yields,
    then click the built Field Shelter and verify the current team recovers.
17. Carry a movable container and place it on another valid tile.
18. Aim while panels are open and verify that they do not obstruct the shot.
19. Save, reload and verify stockpile, upgrades, changed terrain and removed structures.
20. Hover empty ground, a container, movable scenery, a contact and a hostile. Verify
    the cursor changes automatically; middle-click must cycle only valid actions.
21. Right-click an owned merc and run `AI / RUN TO COVER + STANCE`. In combat it should
    prefer threat-aware cover; in an empty test sector it should choose nearby geometry.
22. Carry a catalogued portable asset. Verify its real sprite moves above the merc and
    the destination remains separately marked.
23. Shoot catalogued wood, stone and metal salvage/resource assets. Verify coloured
    impact chips, eventual destruction and material loot; ordinary walls must remain.
24. Open a fresh container once by double-click and a second fresh container through
    right-click `OPEN CONTENTS`; both paths must seed and display loot exactly once.
25. Select several different world assets. Verify each OS0 window shows the object's
    real pixel silhouette and the world marker settles without restarting on hover.
26. Choose `DETAILS / ATTACHMENTS` on ground and inventory items. Verify the movable
    Inspector updates immediately and lists ammunition/attachments without opening a
    duplicate item-details or container window.
27. Click `STRATEGIC MAP`, switch among `BASE`, `ARULCO MAP` and `TEAM`, select map
    sectors, then select and center an operator from the team list without leaving play.
28. At 2x zoom, scroll to all four map limits and verify the true edge is visible and
    world interaction remains aligned with the cursor.
29. Hover and click across the full visible silhouettes of a large wreck, tree, debris
    heap and multi-tile structure. The same object must remain identified on every
    opaque section; transparent gaps should continue to select the world behind it.
30. Right-click world objects and terrain. Verify the object-anchored icon fan stays
    still, follows camera/zoom movement, shows an action label on hover and every
    enabled icon is directly clickable. Character and item-specific text menus should
    continue to work after closing the icon fan.
31. Right-click an owned merc. Verify a circular icon ring surrounds that merc and
    `INVENTORY / EQUIPMENT` expands the real helmet, face, armour, legs and hand slots
    above the actor. Drag an item between one of those slots and the world/pocket UI.
32. Click `PACK` in the exploded equipment view. Verify the real pocket slots unfold
    beside the actor without a rectangular character window or duplicate body slots.
33. Drag a slot containing multiple objects. Choose `+`, `-`, `ALL`, `TAKE` and cancel
    in separate attempts; only the selected count should attach to the item cursor.
34. Reposition one light and one heavy movable object. The light object should be held
    above the merc; the heavier object should trail behind as `DRAG`. Invalid placement
    must leave the original intact, while a valid move should log strength practice.
35. Carry a weapon across every visible OS0 element. Its item cursor must stay visible.
    Hover the merc and test `TAKE IN HANDS`, `PUT IN PACK` and `DROP HERE`, including
    a case where both hands and all pockets are occupied.
36. Open a crate. Verify its own world sprite is the centre of the view and each real
    contained item unfolds around it. Close/reopen it and confirm no duplicate panel
    appears and contents persist.
37. Hover several assets without clicking. Verify the live preview follows the target
    while selection and the existing object action animation remain unchanged.
38. Click the small computer symbol, drag its single OS window, and exercise `BASE`,
    `ARULCO`, `TEAM` and `REPORT`. Trigger the exit-sector dialog and verify no fixed
    legacy minimap appears; the real sector radar stays inside the movable OS window.
39. Open the star Asset Library and cycle `ALL`, `UNCATALOGUED` and `DEBRIS / SALVAGE`.
    Verify each card uses the real tile sprite and shows count, material, size and tool.
40. Left-click an asset card to center its map instance. Right-click it, save a custom
    classification and verify the library returns with the card marked `DB`.
41. Hover wood, stone, organic, sand and metal debris. Verify the immediate inspector
    shows durability, yield and the correct debug tool with `READY` or `MISSING`.
42. Walk while equipment and pockets are expanded. Symbols must follow the animation
    continuously without jumping once per tile and without rectangular slot frames.
43. Begin dragging an item while the character UI, star library or computer is open.
    No window may close, move or auto-collapse merely because the item cursor exists.
44. Open `TERRAIN` from the expanded multitool's God tools. Move the editor across other OS0 windows,
    click and drag each header in both overlap orders, and verify the visible front
    window always receives the click without jumping or losing the drag.
45. In `TILES`, cycle category filters and place/remove one prop, wall and roof tile.
    Change related variants and the native target layer; the preview, hover help and
    actual placed asset must agree.
46. Paint normal terrain and shoreline water with radius 0, 2 and 8, then switch to
    smoothing. Low/deep water must say `PAINT ONLY` instead of queueing a failed
    smooth command. Place road macros 0 and 31 away from the map edge.
47. In `ITEMS`, change quantity and place a stack. In `NPCS`, place a generic actor
    and a profile NPC in two facings. Save `live-editor.dat`, change the world, choose
    `SYSTEM / LOAD MAP`, and verify items, actors, facing and squad survive the round trip.
48. Finally press `EMPTY MAP` once and cancel by using another control; nothing should
    change. Confirm it with two clicks on a second attempt. If creation or loading is
    forced to fail, the previous map, squad, camera and ambient sound must recover.
49. Leave the pointer still over a crate, ground item, scenery asset and bare terrain,
    moving the camera between attempts. Press plain `F`: the freshly hovered relation
    must open its contextual action surface without automatically executing pickup,
    attack, dig, salvage, carry or any other leaf action. `Alt+F` must retain vanilla
    tracking mode.
50. Hover the owned merc and press `F`, then repeat through the multitool's `INFO`
    group. Both routes must open the same hub root with `ACTIONS`,
    `ABILITIES / TALENTS`, `EQUIPMENT`, `GROUP` and `GOD`. Abilities/talents must be
    real executable engine/OS0 actions rather than decorative or promised skills.
51. Enter every character-hub category. Switching pages must not close the hub;
    `BACK` must return to the root. Trigger one harmless leaf action and verify it
    executes exactly once. While a modal hub surface has focus, other OS0 windows
    may disappear only temporarily and must return with their prior visibility and
    position when the modal closes. Repeat by pressing `Escape` at the root.
52. At both 1x and 2x zoom, expand the multitool and click its TARGET/WALK symbol.
    `COMBAT` must aim at the mouse, keep the exit symbol visible, move with WASD and
    fire an ordinary loaded firearm exactly once per LMB gesture. Switch back to
    `NORMAL` during movement and immediately after a shot; neither input nor camera
    may freeze, duplicate the shot or leave the native attack cursor behind.
53. Enter turn-based combat and tap W/A/S/D rapidly, including a new direction while
    the previous one-tile animation is still running. Each accepted tap must execute
    once with native AP cost; held diagonal keys must not zigzag. Test Q/E, Shift+W,
    no-AP movement, enemy turn and an interrupt. Locked inputs must wait or be ignored
    without activating legacy JA2 shortcuts. At 2x zoom, start/end combat and scroll
    into every map edge; the crop must not jump and cursor/world targeting must align.
54. In a fresh test state, choose both tactical body options in the creator and verify
    the live JA2 sprite changes. Keep the default attribute distribution, then edit one
    value and select two traits. Camouflage must visibly apply when chosen; the created
    operator must not play tactical speech.
55. Follow the marked-crate tutorial exactly. Hovering the marked container must
    advance the prompt; `F` or RMB must expose `CONTENTS`; choosing it while distant
    must walk into range and open the spatial contents exactly once. Taking one item
    must complete the tutorial. Report the map/tileset if no crate can be assigned.
56. Repeat the distant container flow, then cancel separately with WASD, Escape,
    Combat mode, a changed/removed target and an interrupted native path. No cancelled
    route may later open or mutate the old target. An immediately adjacent action must
    still execute once without an unnecessary path.
57. Test blocked relations: digging without a shovel, salvage without its tool, moving
    an overweight object and using an invalidated target. The UI must show the reason
    and must not enqueue movement or affect another nearby object.
58. Stack several active layers, then press and release Escape once per layer. Each
    physical press must cancel only one layer in order: radial/modal, item on cursor,
    carry/manipulation, pending approach, then action/combat state. Key release must
    not silently cancel a second layer.
59. In Normal and Combat, move the pointer around a stationary operator and verify the
    facing follows it. Hold W continuously, add/release Shift, and steer with Q/E; the
    character must retain JA2 path/animation ownership without invoking old A/S/D
    shortcuts. Treat discontinuities at tile/animation boundaries as a bug report.
60. Drag one item from a crate across multiple overlapping windows and release it in a
    merc hand, body slot, pack and world tile in separate attempts. Exactly one target
    may react. Releasing over blank panel space must keep the item held and must not
    drop it into the world behind the panel.
61. Fill both hands and every pocket, then drag another object onto the merc. Only
    valid relations may be shown; a rejected hand/pack/body choice must retain the
    object on the cursor. Open `MORE OPTIONS` and verify every displayed alternative
    is legal for that item and actor.
62. Begin a drag in one inventory/loot region and release in another without pausing.
    Immediately click an unrelated control; it must react normally. Repeat once by
    releasing outside the visible OS0 controls and once by Alt-Tabbing while holding
    the button. No stale drag, false double-click or blocked next click may remain.

## Writing a useful report

Open the small field computer and choose `REPORT`, then select the closest category
and include:

- what you were trying to do;
- exact mouse button or key used;
- the selected merc and target object;
- what happened;
- what you expected;
- whether it happens every time.

Press `SAVE REPORT`, then attach the TXT file from `%APPDATA%\JA2\Feedback` to a
GitHub issue. Screenshots are welcome, but the TXT report is more important.
The report already contains the complete asset-catalog snapshot. The crash collector
also copies the raw `os0-assets.tsv`, which is useful for reviewing larger batches.

## Crash reports

After a crash, do not start the game again first. Run `COLLECT_LAST_CRASH.cmd` from
the extracted playtest. Attach the resulting `reports` directory to a crash issue.
If an emergency save exists, attach that as well when GitHub accepts the file type.

Reports include gameplay metadata and logs, but no original JA2 archives, passwords
or personal documents.
