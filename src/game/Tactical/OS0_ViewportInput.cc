#include "OS0_ViewportInput.h"

#include "Handle_UI.h"
#include "English.h"
#include "Input.h"
#include "Interactive_Tiles.h"
#include "Interface.h"
#include "Interface_Items.h"
#include "Isometric_Utils.h"
#include "MouseSystem.h"
#include "OS0_IngameUI.h"
#include "Overhead.h"
#include "Soldier_Find.h"
#include "UILayout.h"

namespace
{
	OS0ViewportGestureState gPointerGestures;

	UINT16 ResolveOS0WorldTarget(GridNo& gridNo)
	{
		INT16 interactiveGrid = NOWHERE;
		LEVELNODE* const node = GetCurInteractiveTileGridNo(&interactiveGrid);
		UINT16 tileIndex = node ? node->usIndex : NO_TILE;
		if (node && interactiveGrid >= 0 && interactiveGrid < WORLD_MAX)
		{
			gridNo = interactiveGrid;
		}
		else
		{
			FindOS0WorldAssetAtScreen(&gridNo, gsInterfaceLevel, &tileIndex,
				gusMouseXPos, gusMouseYPos);
		}
		return tileIndex;
	}
}

BOOLEAN OS0HandleViewportPointerEvent(MOUSE_REGION*, UINT32 reason)
{
	// Character creation is one modal OS//0 flow. Pointer input outside its own
	// high-priority regions must not zoom, scroll or act on the tactical world.
	if (OS0CreatorIsActive()) return TRUE;
	if (reason & MSYS_CALLBACK_REASON_WHEEL_UP)
	{
		OS0AdjustWorldZoom(1);
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_WHEEL_DOWN)
	{
		OS0AdjustWorldZoom(-1);
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_LBUTTON_DWN)
	{
		gPointerGestures.beginPrimary();
	}
	if (reason & MSYS_CALLBACK_REASON_RBUTTON_DWN)
	{
		gPointerGestures.armRight();
		fRightButtonDown = FALSE;
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_MBUTTON_DWN)
	{
		gPointerGestures.armMiddle();
		fMiddleButtonDown = FALSE;
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_LBUTTON_DOUBLECLICK)
	{
		if (gUIFullTarget)
		{
			OS0OpenCharacterPanel(gUIFullTarget);
		}
		else
		{
			GridNo gridNo = guiCurrentCursorGridNo;
			const UINT16 tileIndex = ResolveOS0WorldTarget(gridNo);
			OS0ActivateWorldObject(gridNo, gsInterfaceLevel, tileIndex);
		}
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if (OS0HandlePendingWorldMove(guiCurrentCursorGridNo)) return TRUE;

		GridNo gridNo = guiCurrentCursorGridNo;
		const UINT16 tileIndex = ResolveOS0WorldTarget(gridNo);
		const BOOLEAN heldItem = gpItemPointer != nullptr;
		if (OS0HandleCursorAction(gUIFullTarget, gridNo, gsInterfaceLevel,
			tileIndex))
		{
			if (heldItem) gPointerGestures.markHeldItemReleaseHandled();
			return TRUE;
		}
		// Selection updates OS0 projections but does not steal ordinary JA2
		// movement/selection from the legacy primary-button owner.
		OS0SelectWorldObject(gUIFullTarget, gridNo, gsInterfaceLevel, tileIndex);
	}
	if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		fRightButtonDown = FALSE;
		if (!gPointerGestures.releaseRight()) return TRUE;
		GridNo gridNo = guiCurrentCursorGridNo;
		const UINT16 tileIndex = ResolveOS0WorldTarget(gridNo);
		OS0OpenContextMenu(gUIFullTarget, gridNo, gsInterfaceLevel, tileIndex,
			gusMouseXPos, gusMouseYPos);
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_MBUTTON_UP)
	{
		fMiddleButtonDown = FALSE;
		if (!gPointerGestures.releaseMiddle()) return TRUE;
		GridNo gridNo = guiCurrentCursorGridNo;
		const UINT16 tileIndex = ResolveOS0WorldTarget(gridNo);
		if (_KeyDown(SHIFT))
		{
			OS0CancelCursorAction();
		}
		else if (_KeyDown(CTRL))
		{
			if (SOLDIERTYPE* const selected = GetSelectedMan())
				LocateSoldier(selected, DONTSETLOCATOR);
		}
		else
		{
			OS0CycleCursorAction(gUIFullTarget, gridNo, gsInterfaceLevel,
				tileIndex);
		}
		return TRUE;
	}
	return FALSE;
}

BOOLEAN OS0ConsumeHandledHeldItemRelease()
{
	return gPointerGestures.consumeHeldItemRelease();
}

BOOLEAN OS0OwnsViewportContextButtons()
{
	return TRUE;
}

void OS0ResetViewportPointerGestures()
{
	gPointerGestures.reset();
}
