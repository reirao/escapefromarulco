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
#include "OS0_ItemTransferController.h"
#include "Overhead.h"
#include "RenderWorld.h"
#include "Soldier_Find.h"
#include "UILayout.h"

namespace
{
	OS0ViewportGestureState gPointerGestures;
	struct PointerProjection
	{
		INT16 mouseX = -1;
		INT16 mouseY = -1;
		INT16 renderX = 0;
		INT16 renderY = 0;
		UINT8 level = 0xff;
		UINT8 zoom = 0;
		BOOLEAN valid = FALSE;
	};
	PointerProjection gHoverProjection;

	BOOLEAN ProjectCurrentPointerToGrid(GridNo& gridNo)
	{
		// GetMouseXY intentionally rejects the pointer whenever a higher-priority
		// mouse region owns it. OS//0's object affordances are such regions, but
		// they still represent the tactical world beneath them. F therefore needs
		// the same direct, zoom-aware projection used by realtime facing.
		INT16 screenX = static_cast<INT16>(gusMouseXPos);
		INT16 screenY = static_cast<INT16>(gusMouseYPos);
		OS0MapDisplayToWorldScreen(&screenX, &screenY);
		const INT16 offsetX = static_cast<INT16>(
			screenX - g_ui.m_tacticalMapCenterX);
		const INT16 offsetY = static_cast<INT16>(
			screenY - g_ui.m_tacticalMapCenterY + 10);
		INT16 cellX;
		INT16 cellY;
		FromScreenToCellCoordinates(offsetX, offsetY, &cellX, &cellY);
		const INT32 worldX = static_cast<INT32>(gsRenderCenterX) + cellX;
		const INT32 worldY = static_cast<INT32>(gsRenderCenterY) + cellY;
		if (worldX < 0 || worldX >= WORLD_COORD_COLS ||
			worldY < 0 || worldY >= WORLD_COORD_ROWS) return FALSE;
		const INT16 column = static_cast<INT16>(worldX / CELL_X_SIZE);
		const INT16 row = static_cast<INT16>(worldY / CELL_Y_SIZE);
		gridNo = MAPROWCOLTOPOS(row, column);
		return gridNo >= 0 && gridNo < WORLD_MAX;
	}

	BOOLEAN ResolveCurrentPointerTarget(SOLDIERTYPE*& target, GridNo& gridNo,
		UINT8& level, UINT16& tileIndex)
	{
		target = nullptr;
		gridNo = NOWHERE;
		level = gsInterfaceLevel;
		tileIndex = NO_TILE;
		if (!ProjectCurrentPointerToGrid(gridNo)) return FALSE;

		// Vanilla deliberately omits the selected merc from FindSoldier in some
		// cursor modes. Perception is mode-independent, so use the rendered actor
		// bounds before falling back to the grid lookup.
		if (SOLDIERTYPE* const selected = GetSelectedMan();
			selected && selected->bActive && selected->bTeam == OUR_TEAM &&
			selected->bLevel == level &&
			!(selected->uiStatusFlags &
				(SOLDIER_DEAD | SOLDIER_PASSENGER | SOLDIER_DRIVER)) &&
			IsPointInSoldierBoundingBox(selected, gusMouseXPos, gusMouseYPos))
		{
			target = selected;
			gridNo = selected->sGridNo;
			level = selected->bLevel;
			return TRUE;
		}
		target = FindSoldier(gridNo, FINDSOLDIERSAMELEVEL(level));
		if (target)
		{
			gridNo = target->sGridNo;
			level = target->bLevel;
			return TRUE;
		}
		FindOS0WorldAssetAtScreen(&gridNo, level, &tileIndex,
			gusMouseXPos, gusMouseYPos);
		return gridNo >= 0 && gridNo < WORLD_MAX;
	}

	UINT16 ResolveOS0WorldTarget(GridNo& gridNo)
	{
		if (gUIFullTarget && gUIFullTarget->bActive)
		{
			gridNo = gUIFullTarget->sGridNo;
			return NO_TILE;
		}
		// RMB/MMB must use the same zoom-aware projection as F and live hover.
		// guiCurrentCursorGridNo and the vanilla interactive-tile cache can lag a
		// frame while scrolling, which previously made the radial appear missing.
		ProjectCurrentPointerToGrid(gridNo);
		UINT16 tileIndex = NO_TILE;
		FindOS0WorldAssetAtScreen(&gridNo, gsInterfaceLevel, &tileIndex,
			gusMouseXPos, gusMouseYPos);
		return tileIndex;
	}
}

BOOLEAN OS0TriggerHoveredInteraction()
{
	// F belongs to the tactical world, not to a window hidden underneath the
	// pointer. Consume the intent while a modal/managed UI surface owns it.
	if (OS0CreatorIsActive() ||
		OS0BlocksKeyboardWorldInputAt(gusMouseXPos, gusMouseYPos)) return TRUE;
	if (gfInTalkPanel || (gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
		fShowAssignmentMenu ||
		InItemDescriptionBox() || InItemStackPopup() || InKeyRingPopup())
		return TRUE;
	if (gusMouseXPos < gsVIEWPORT_START_X || gusMouseXPos >= gsVIEWPORT_END_X ||
		gusMouseYPos < gsVIEWPORT_WINDOW_START_Y ||
		gusMouseYPos >= OS0WorldViewportBottom()) return TRUE;
	// If the pointer is on the small world-attached OPEN icon, retain the exact
	// bound crate relation instead of projecting the ground behind the glyph.
	if (OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos) &&
		OS0ActivateCurrentHoverInteraction(gusMouseXPos, gusMouseYPos))
		return TRUE;

	// Do not trust guiCurrentCursorGridNo/gUIFullTarget here: scrolling can move
	// the world beneath a stationary pointer without delivering a mouse event.
	SOLDIERTYPE* target = nullptr;
	GridNo gridNo = NOWHERE;
	UINT8 level = gsInterfaceLevel;
	UINT16 tileIndex = NO_TILE;
	if (!ResolveCurrentPointerTarget(target, gridNo, level, tileIndex))
	{
		return OS0ActivateHoveredInteraction(nullptr, NOWHERE,
			gsInterfaceLevel, NO_TILE, gusMouseXPos, gusMouseYPos);
	}
	return OS0ActivateHoveredInteraction(target, gridNo,
		level,
		tileIndex, gusMouseXPos, gusMouseYPos);
}

void OS0RefreshWorldHoverFromPointer()
{
	if (OS0CreatorIsActive() ||
		OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos))
	{
		if (gHoverProjection.valid) OS0ClearWorldHover();
		gHoverProjection.valid = FALSE;
		return;
	}
	const BOOLEAN inViewport =
		gusMouseXPos >= gsVIEWPORT_START_X && gusMouseXPos < gsVIEWPORT_END_X &&
		gusMouseYPos >= gsVIEWPORT_WINDOW_START_Y &&
		gusMouseYPos < OS0WorldViewportBottom();
	if (!inViewport)
	{
		if (gHoverProjection.valid) OS0ClearWorldHover();
		gHoverProjection.valid = FALSE;
		return;
	}
	const UINT8 zoom = OS0WorldZoomFactor();
	if (gHoverProjection.valid &&
		gHoverProjection.mouseX == static_cast<INT16>(gusMouseXPos) &&
		gHoverProjection.mouseY == static_cast<INT16>(gusMouseYPos) &&
		gHoverProjection.renderX == gsRenderCenterX &&
		gHoverProjection.renderY == gsRenderCenterY &&
		gHoverProjection.level == gsInterfaceLevel &&
		gHoverProjection.zoom == zoom)
		return;
	gHoverProjection = { static_cast<INT16>(gusMouseXPos),
		static_cast<INT16>(gusMouseYPos), gsRenderCenterX, gsRenderCenterY,
		static_cast<UINT8>(gsInterfaceLevel), zoom, TRUE };

	SOLDIERTYPE* target = nullptr;
	GridNo gridNo = NOWHERE;
	UINT8 level = gsInterfaceLevel;
	UINT16 tileIndex = NO_TILE;
	if (!ResolveCurrentPointerTarget(target, gridNo, level, tileIndex))
	{
		OS0ClearWorldHover();
		return;
	}
	OS0HoverWorldObject(target, gridNo, level, tileIndex,
		gusMouseXPos, gusMouseYPos);
}

void OS0InvalidateWorldHoverProjection()
{
	gHoverProjection.valid = FALSE;
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
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reconcile(gpItemPointer != nullptr);
		if (gpItemPointer) transfers.beginHeldGesture();
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
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reconcile(gpItemPointer != nullptr);
		if (!gpItemPointer && transfers.sourcePressed() &&
			transfers.claimRelease(OS0ItemTransferSurface::WORLD) ==
				OS0ItemReleaseClaim::SOURCE_CLICK)
		{
			// The pointer left its source region before crossing the drag
			// threshold. This is still the source's click, never a world click.
			gPointerGestures.releasePrimary();
			return TRUE;
		}
		if (gpItemPointer && transfers.itemHeld())
		{
			const OS0ItemReleaseClaim claim =
				transfers.claimRelease(OS0ItemTransferSurface::WORLD);
			if (claim == OS0ItemReleaseClaim::ITEM)
			{
				SOLDIERTYPE* target = nullptr;
				GridNo gridNo = NOWHERE;
				UINT8 level = gsInterfaceLevel;
				UINT16 tileIndex = NO_TILE;
				ResolveCurrentPointerTarget(target, gridNo, level, tileIndex);
				// A controller-owned drag never falls through into JA2's second,
				// polled drop path. Unsupported relations leave the item held.
				OS0HandleCursorAction(target, gridNo, level, tileIndex);
				transfers.completeItemRelease(gpItemPointer != nullptr);
				gPointerGestures.markHeldItemReleaseHandled();
				gPointerGestures.releasePrimary();
				return TRUE;
			}
			if (transfers.releaseWasHandled()) return TRUE;
		}
		else if (transfers.releaseWasHandled())
		{
			// A higher-priority inventory/loot region committed this same UP.
			return TRUE;
		}
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
	const BOOLEAN viewportHandled = gPointerGestures.consumeHeldItemRelease();
	const BOOLEAN transferHandled =
		OS0GetItemTransferController().consumeHandledRelease();
	return viewportHandled || transferHandled;
}

BOOLEAN OS0ConsumeViewportPrimaryGesture()
{
	const BOOLEAN viewportHandled = gPointerGestures.consumePrimaryGesture();
	const BOOLEAN transferHandled =
		OS0GetItemTransferController().consumeHandledRelease();
	return viewportHandled || transferHandled;
}

BOOLEAN OS0OwnsViewportContextButtons()
{
	return TRUE;
}

void OS0ResetViewportPointerGestures()
{
	gPointerGestures.reset();
	OS0GetItemTransferController().reset();
	gHoverProjection = {};
}
