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

The code is experimental and does not represent an official JA2 Stracciatella release.

## v0.1.1-alpha hotfix

- Removed an unsupported String Theory alignment specifier that crashed on the first
  recorded context action with `Unexpected character in format string`.
- Made diagnostic event capture exception-safe so logging cannot terminate gameplay.
- Moved draggable-window coordinate and hit-region synchronization to one update per
  rendered frame, eliminating raw mouse-event/render races and reducing drag artifacts.
