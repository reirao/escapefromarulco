# Escape from Arulco 0.0.1.11

> **Historical checkpoint:** this document records an older package and is not the
> current support or stability statement. Use `RELEASE_0.0.1.23.md` for the present
> **FRAGILE / TESTED** prerelease status.

This release consolidates the experimental `v0.5.x-alpha` work into a smaller,
testable baseline. The version reset is intentional: `0.0.1.11` describes the
Escape from Arulco playtest, while the embedded Stracciatella engine keeps its own
independent version.

## What changed

- One modal in-world creator now owns callsign, attributes and two traits.
- The duplicate laptop/OS0 creator path has been removed.
- One runtime owns UI transitions and one layout owns the tactical viewport.
- The fixed bottom command dock is no longer part of scrolling or zoom rendering.
- Character inventory is optional and opens only through the Character command.
- Sector, Inspector and Toolbox are the only general movable workspace windows.
- Old invisible context/tools/actions/object/feedback panel regions are gone.
- Feedback remains available in the Strategy window's Report tab.
- Window positions use stable named records and migrate old numeric layouts.
- Cursor, creator modality and exit-sector input use the same world boundary.

## Validation

- Windows MinGW release build completed.
- 147 unit tests passed.
- A 12-second launch smoke test completed without an engine error, segmentation
  fault or format-string failure.

## Test focus

Please concentrate on fresh character creation, camera scrolling and zoom near every
map edge, the fixed bottom dock, reopening/moving OS0 windows, optional character
inventory, world-container interaction and held-item cursors. Report exact input and
attach the generated feedback TXT or crash collector output.

## Requirements

The Windows package contains the executable, launcher and required runtime DLLs. It
does not contain original Jagged Alliance 2 data. A legally owned JA2 or JA2 Gold
installation is required.
