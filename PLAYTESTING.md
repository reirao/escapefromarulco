# Playtesting Escape from Arulco

Thank you for testing this early prototype. Specific reproduction steps are much more
useful than a general statement that something did not work.

## Setup

- Windows x64
- A legally owned JA2 or JA2 Gold installation
- The ZIP from the latest GitHub release, fully extracted
- Windowed mode recommended

Never copy the playtest files over the original JA2 installation. Keep the extracted
playtest in its own folder and point the launcher to the original game directory.

## Suggested test pass

1. Complete character creation using a custom name, attributes and two traits.
2. Move every OS0 window, close it and reopen it from the bottom dock.
3. Equip, swap and drop items using drag and drop.
4. Right-click weapons and exercise every enabled weapon action.
5. Open several containers; verify each contains material plus useful or damaged gear.
6. Double-click timber, stone, scrap and soil; verify the `SECT` stockpile increases.
7. Dismantle a wooden door, fence or furniture object and loot its material output.
8. Salvage stone/debris and right-click nearby grass or dirt to use `DIG`.
9. Build all three upgrades in `SECT`; verify the workshop/depot bonuses affect yields.
10. Carry a movable container and place it on another valid tile.
11. Aim while panels are open and verify that they do not obstruct the shot.
12. Save, reload and verify stockpile, upgrades, changed terrain and removed structures.

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

## Crash reports

After a crash, do not start the game again first. Run `COLLECT_LAST_CRASH.cmd` from
the extracted playtest. Attach the resulting `reports` directory to a crash issue.
If an emergency save exists, attach that as well when GitHub accepts the file type.

Reports include gameplay metadata and logs, but no original JA2 archives, passwords
or personal documents.
