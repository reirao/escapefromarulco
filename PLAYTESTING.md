# Playtesting Escape from Arulco

Thank you for testing this early prototype. Specific reproduction steps are much more
useful than a general statement that something did not work.

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

1. Complete character creation using a custom name, attributes and two traits.
2. Move every OS0 window, close it and reopen it from the bottom dock.
3. Hold the right mouse button over two different owned mercs. Verify the correct merc
   is selected and both graphical JA2 button frames appear.
4. Open the God icon library from the second frame, select several symbols and verify
   the selected symbol becomes the God/library button on the next long-hold.
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
26. Choose `DETAILS / ATTACHMENTS` on ground and inventory items. Verify the OS0 detail
    window opens immediately, remains responsive and lists ammunition/attachments.
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

## Writing a useful report

Open `FB`, choose the closest category and include:

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
