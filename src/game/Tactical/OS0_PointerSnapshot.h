#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;

// One immutable interpretation of the tactical pointer for a physical input
// event.  F, every mouse button, live hover and mouse-facing all use the same
// screen-to-world projection instead of consulting JA2's frame-late cursor
// caches independently.
struct OS0PointerSnapshot
{
	INT16 screenX = 0;
	INT16 screenY = 0;
	INT16 worldX = 0;
	INT16 worldY = 0;
	GridNo gridNo = -1;
	UINT8 level = 0;
	UINT16 tileIndex = 0xffff;
	INT32 worldItemIndex = -1;
	SOLDIERTYPE* actor = nullptr;
	BOOLEAN hasWorldPoint = FALSE;
};

// Projects an arbitrary displayed pixel through the active OS//0 zoom and the
// native JA2 isometric camera.  This is the sole coordinate transform used by
// pointer targeting and direct-control facing.
BOOLEAN OS0ProjectTacticalScreenToWorld(INT16 screenX, INT16 screenY,
	INT16& worldX, INT16& worldY, GridNo* gridNo = nullptr);

// Captures the current pointer once.  Actor bounds take precedence over the
// grid lookup; world assets are then resolved pixel-accurately on that grid.
OS0PointerSnapshot OS0CapturePointerSnapshot(UINT8 level);
