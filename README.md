# Escape from Arulco

An experimental, non-commercial rework of *Jagged Alliance 2* built on
[JA2 Stracciatella](https://github.com/ja2-stracciatella/ja2-stracciatella).

> **Early alpha:** this build is intentionally unfinished. Expect rough controls,
> incomplete systems and crashes. The purpose of this release is hands-on testing.

## Download and play

1. Open the [v0.1.0-alpha release](https://github.com/reirao/escapefromarulco/releases/tag/v0.1.0-alpha).
2. Download `Escape-from-Arulco-Playtest-2026-07-28.zip`.
3. Extract the complete ZIP into a writable folder.
4. Run `START_PLAYTEST.cmd`.
5. In the launcher, select your own legally installed JA2 or JA2 Gold directory.
6. Select the correct game language and start in windowed mode.

The playtest includes the Windows executable, launcher, runtime DLLs, externalized
Stracciatella data, test instructions and crash-log collector. It does **not** contain
the original JA2 game data. Windows x64 and an existing JA2 installation are required.

## Current prototype

- Direct tactical-sector start without helicopter arrival or starting enemies
- In-world character creation with freely selected attributes and traits
- Movable OS0 windows for character, context, tools, actions and object inventory
- Character and container inventories with item drag and drop
- Context-sensitive object, weapon, loot and stance actions
- Movable world structures and early material/physics properties
- Experimental terrain digging that removes a surface and exposes soil
- Zoomed tactical view
- Built-in playtest feedback reports

This is a systems prototype, not yet a balanced campaign.

## Controls and feedback

- **Right click:** context menu for the object or terrain under the cursor
- **Middle click:** cycle the current cursor action
- **Double click:** open a character, container or world object
- **FB:** open the feedback window, including during character creation

In the feedback window, choose a category, describe the problem and press
`SAVE REPORT`. Reports are saved to `%APPDATA%\JA2\Feedback` and include relevant
game context, recent OS0 actions and the tail of the engine log.

If the game crashes before a report can be saved, run `COLLECT_LAST_CRASH.cmd` from
the extracted playtest folder and attach the generated `reports` folder to a
[bug report](https://github.com/reirao/escapefromarulco/issues/new/choose).

See [PLAYTESTING.md](PLAYTESTING.md) for the full test checklist.

## Building from source

The project retains the Stracciatella build system. See [COMPILATION.md](COMPILATION.md).
The tested Windows build uses MSYS2 UCRT64/MinGW64, CMake and Ninja.

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
