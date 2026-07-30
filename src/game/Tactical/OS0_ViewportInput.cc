#include "OS0_ViewportInput.h"

#include "Handle_UI.h"
#include "English.h"
#include "Input.h"
#include "Interactive_Tiles.h"
#include "Interface.h"
#include "Interface_Dialogue.h"
#include "Interface_Items.h"
#include "Isometric_Utils.h"
#include "MouseSystem.h"
#include "Map_Screen_Interface.h"
#include "OS0_IngameUI.h"
#include "Overhead.h"
#include "Soldier_Find.h"
#include "UILayout.h"

namespace
{
	OS0ViewportGestureState gPointerGestures;

	UINT16 ResolveOS0WorldTarget(GridNo& gridNo)
	{
		if (gUIFullTarget && gUIFullTarget->bActive)
		{
			gridNo = gUIFullTarget->sGridNo;
			return NO_TILE;
		}
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

BOOLEAN OS0TriggerHoveredInteraction()
{
	// F belongs to the tactical world, not to a window hidden underneath the
	// pointer. Consume the intent while a modal/managed UI surface owns it.
	if (OS0CreatorIsActive() ||
		OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos)) return TRUE;
	if (gfInTalkPanel || (gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
		fShowAssignmentMenu ||
		InItemDescriptionBox() || InItemStackPopup() || InKeyRingPopup())
		return TRUE;
	if (gusMouseXPos < gsVIEWPORT_START_X || gusMouseXPos >= gsVIEWPORT_END_X ||
		gusMouseYPos < gsVIEWPORT_WINDOW_START_Y ||
		gusMouseYPos >= OS0WorldViewportBottom()) return TRUE;

	// Do not trust guiCurrentCursorGridNo/gUIFullTarget here: scrolling can move
	// the world beneath a stationary pointer without delivering a mouse event.
	INT16 worldX;
	INT16 worldY;
	if (!GetMouseXY(&worldX, &worldY))
	{
		return OS0ActivateHoveredInteraction(nullptr, NOWHERE,
			gsInterfaceLevel, NO_TILE, gusMouseXPos, gusMouseYPos);
	}
	GridNo gridNo = MAPROWCOLTOPOS(worldY, worldX);
	SOLDIERTYPE* target = nullptr;
	// Vanilla deliberately omits the selected merc from FindSoldier while the
	// attack cursor is active.  F is a perception command, so resolve that actor
	// directly from the current screen pixel before asking the mode-dependent
	// vanilla target finder.  This also keeps Shift+F from degrading to its
	// grid-only search path.
	if (SOLDIERTYPE* const selected = GetSelectedMan();
		selected && selected->bActive && selected->bTeam == OUR_TEAM &&
		selected->bLevel == gsInterfaceLevel &&
		!(selected->uiStatusFlags &
			(SOLDIER_DEAD | SOLDIER_PASSENGER | SOLDIER_DRIVER)) &&
		IsPointInSoldierBoundingBox(selected, gusMouseXPos, gusMouseYPos))
	{
		target = selected;
		gridNo = selected->sGridNo;
	}
	else
	{
		target = FindSoldier(gridNo, FINDSOLDIERSAMELEVEL(gsInterfaceLevel));
	}
	// The vanilla interactive-tile pointer is a render cache and can still refer
	// to the pre-scroll frame. F resolves the current screen pixel directly, but
	// only when no actor already owns the relation; an overlapping prop must not
	// rewrite a freshly resolved character target.
	UINT16 tileIndex = NO_TILE;
	if (!target)
	{
		FindOS0WorldAssetAtScreen(&gridNo, gsInterfaceLevel, &tileIndex,
			gusMouseXPos, gusMouseYPos);
	}
	else
	{
		gridNo = target->sGridNo;
	}
	return OS0ActivateHoveredInteraction(target, gridNo,
		target ? target->bLevel : gsInterfaceLevel,
		tileIndex, gusMouseXPos, gusMouseYPos);
}

BOOLEAN OS0HandleViewportPointerEvent(MOUSE_REGION*, UINT32 reason)
{
	// Character creation is one modal OS//0 flow. Pointer input outside its own
	// high-priority regions must not zoom, scroll or act on the tactical world.
	if (OS0CreatorIsActive()) return TRUE;
	// Mouse regions normally win before the viewport callback. The manager is
	// the authoritative fallback for overlapping/moved windows and modals, so a
	// stale region can no longer leak zoom or world actions through the UI.
	if (OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos)) return TRUE;
	// TacticalViewPortTouchCallback refreshed the native target immediately
	// before handing the physical event to OS//0. Keep that single projection;
	// doing the soldier/structure lookup twice made combat clicks needlessly hot.
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
		const BOOLEAN owned = OS0OwnsViewportPrimaryButton();
		gPointerGestures.beginPrimary(owned);
		if (owned)
		{
			// The legacy RT/TB poller must never see half of a combat/tool click.
			fLeftButtonDown = FALSE;
			return TRUE;
		}
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
		if (gPointerGestures.ownsPrimary() || OS0OwnsViewportPrimaryButton())
			return TRUE;
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
		const BOOLEAN owned = gPointerGestures.releasePrimary();
		GridNo gridNo = guiCurrentCursorGridNo;
		const UINT16 tileIndex = ResolveOS0WorldTarget(gridNo);
		if (owned)
		{
			if (OS0HandlePendingWorldMove(guiCurrentCursorGridNo)) return TRUE;
			const BOOLEAN heldItem = gpItemPointer != nullptr;
			if (OS0HandleCursorAction(gUIFullTarget, gridNo, gsInterfaceLevel,
				tileIndex) && heldItem)
				gPointerGestures.markHeldItemReleaseHandled();
			// A gesture is bound to the owner chosen on button-down. Mode changes,
			// lost-mouse resets or re-entry while held can never turn a vanilla MOVE
			// click into a late OS//0 shot/tool action on release.
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

BOOLEAN OS0ConsumeViewportPrimaryGesture()
{
	return gPointerGestures.consumePrimaryGesture();
}

BOOLEAN OS0OwnsViewportContextButtons()
{
	return TRUE;
}

void OS0ResetViewportPointerGestures()
{
	gPointerGestures.reset();
}
