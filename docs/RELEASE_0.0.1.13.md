# Escape from Arulco 0.0.1.13 — Character Interaction Hub

This stable playtest checkpoint makes interaction ownership explicit: plain `F`
perceives the current world relation, while `F` over a character opens that actor's
single categorized command hub. The first `CHARACTER` dock button enters the same
route, so keyboard, dock and radial controls no longer maintain competing menus.

## What changed

- Character actions now live in five pages: `ACTIONS`, `ABILITIES / TALENTS`,
  `EQUIPMENT`, `GROUP` and `GOD`.
- Group commands provide previous/next squad selection, the live team view and safe
  turn completion through real engine actions.
- God commands open the asset library, live editor and icon atlas, issue field tools
  without losing overflow, and safely restore dead or unconscious operators.
- Plain `F` samples the current mouse pixel instead of stale tactical hover caches.
  It recognizes the selected merc even while aiming and keeps character and scenery
  targets separate when their sprites overlap.
- World `F` enables perception and exposes valid contextual actions without silently
  executing pickup, attack, digging, salvage or movement.
- Context hubs now own modal focus while preserving the exact visibility, position
  and contents of inventory, loot, strategy, inspector and editor windows.
- Item details remain pinned after their radial closes; scan-off no longer destroys
  an unrelated tool cursor; assignment and conversation popups block the new route.
- The first tactical dock cell now has an explicit `NORMAL` / `COMBAT` switch.
  `COMBAT` keeps mouse aim active while WASD moves; a single left click fires ordinary
  firearms, while burst and trajectory actions retain their native confirm step.
- Combat and normal mode are desired states projected only when JA2's native event
  queue is safe. Shots, turns, movement and interrupt locks can no longer overwrite
  one another or leave a visible combat mode with a dead cursor.
- OS0 owns the complete primary-button gesture. The old realtime/turn-based pollers
  cannot execute the same click a second time, including after losing the viewport.
- Entering combat invalidates zoom caches when the top-message viewport changes.
  Zoomed scrolling keeps its configured speed and approaches all map edges smoothly.

## Validation

- Windows MinGW64/Ninja game and unit-test builds completed successfully.
- All **172/172** unit tests passed.
- The portable package includes SDL2, FLTK/image-codec and MinGW runtime DLLs,
  including `libwinpthread-1.dll`.
- The packaged executable completed a launch smoke test without a new fatal log entry.

## Known boundaries

- This remains an experimental sandbox prototype rather than a finished campaign.
- The abilities page exposes real attributes and passive JA2 talents; a separate
  active-skill progression system is not implemented yet.
- Realtime editor rollback is map-level recovery, not full undo of active projectiles,
  timers or transient physics state.
- Burst spread, throws and trajectories deliberately use a second confirmation click;
  direct one-click fire currently applies to ordinary non-burst firearm attacks.
- JA2 1.13 code/data integration and synchronized multiplayer are not included.
- Original Jagged Alliance 2 data is not distributed. A legally installed JA2 or
  JA2 Gold copy is required.

Use the in-game `REPORT` tab when filing an issue and attach the generated report.
The complete hands-on checklist is included as `PLAYTESTING.md`.
