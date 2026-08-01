#include "OS0_ViewportInput.h"

#include "Animation_Control.h"
#include "Handle_UI.h"
#include "English.h"
#include "Game_Clock.h"
#include "Input.h"
#include "Interactive_Tiles.h"
#include "Interface.h"
#include "Interface_Control.h"
#include "Interface_Dialogue.h"
#include "Interface_Items.h"
#include "Isometric_Utils.h"
#include "MouseSystem.h"
#include "Map_Screen_Interface.h"
#include "OS0_DirectControl.h"
#include "OS0_IngameUI.h"
#include "OS0_ItemTransferController.h"
#include "OS0_PointerSnapshot.h"
#include "Overhead.h"
#include "RenderWorld.h"
#include "Soldier_Find.h"
#include "UILayout.h"
#include "World_Items.h"
#include "WorldMan.h"

namespace
{
	OS0ViewportGestureState gPointerGestures;
	OS0ViewportDoubleTapState gDoubleTap;
	OS0ViewportWorldDragState gWorldDrag;
	struct PointerProjection
	{
		INT16 mouseX = -1;
		INT16 mouseY = -1;
		INT16 renderX = 0;
		INT16 renderY = 0;
		UINT8 level = 0xff;
		UINT8 zoom = 0;
		UINT16 heldItem = NOTHING;
		UINT8 heldCount = 0;
		UINT32 worldRevision = 0;
		UINT32 worldItemRevision = 0;
		UINT8 actorId = NOBODY;
		UINT32 actorInstanceId = 0;
		BOOLEAN valid = FALSE;
	};
	PointerProjection gHoverProjection;
	UINT32 gNextNearbyActorMotionProbeAt = 0;
	BOOLEAN gNearbyActorWasMoving = FALSE;
	UINT32 gRightPressStartedAt = 0;
	UINT8 gRightPressActorId = NOBODY;
	UINT32 gRightPressActorInstanceId = 0;
	INT16 gRightPressScreenX = 0;
	INT16 gRightPressScreenY = 0;
	GridNo gRightPressGridNo = NOWHERE;
	UINT8 gRightPressLevel = 0;
	UINT16 gRightPressTileIndex = NO_TILE;
	INT32 gRightPressWorldItemIndex = -1;
	UINT32 gRightPressWorldRevision = 0;
	UINT32 gRightPressWorldItemRevision = 0;

	void ClearRightPressTarget() noexcept
	{
		gRightPressStartedAt = 0;
		gRightPressActorId = NOBODY;
		gRightPressActorInstanceId = 0;
		gRightPressScreenX = 0;
		gRightPressScreenY = 0;
		gRightPressGridNo = NOWHERE;
		gRightPressLevel = 0;
		gRightPressTileIndex = NO_TILE;
		gRightPressWorldItemIndex = -1;
		gRightPressWorldRevision = 0;
		gRightPressWorldItemRevision = 0;
	}

	void TryOpenRightHoldCharacterMenu()
	{
		if (!gPointerGestures.rightPressActive() ||
			gPointerGestures.rightHoldWasHandled() ||
			!(gViewportRegion.ButtonState & MSYS_RIGHT_BUTTON) ||
			gRightPressActorId == NOBODY || gRightPressActorInstanceId == 0 ||
			GetJA2Clock() - gRightPressStartedAt < 300 || gpItemPointer ||
			AreWeInAUIMenu()) return;
		SOLDIERTYPE* const actor = ID2Soldier(gRightPressActorId);
		if (!actor || !actor->bActive ||
			actor->uiUniqueSoldierIdValue != gRightPressActorInstanceId ||
			actor->bTeam != OUR_TEAM || actor->bLife < OKLIFE ||
			(actor->uiStatusFlags & SOLDIER_VEHICLE)) return;
		SelectSoldier(actor, SELSOLDIER_FROM_UI);
		guiPendingOverrideEvent = U_MOVEMENT_MENU;
		gPointerGestures.markRightHoldHandled();
	}

}

BOOLEAN OS0TriggerHoveredInteraction()
{
	// F belongs to the tactical world, not to a window hidden underneath the
	// pointer. Consume the intent while a modal/managed UI surface owns it.
	if (OS0CreatorIsActive() ||
		OS0BlocksKeyboardWorldInputAt(gusMouseXPos, gusMouseYPos)) return TRUE;
	if (gfInTalkPanel || (gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
		fShowAssignmentMenu || AreWeInAUIMenu() ||
		InItemDescriptionBox() || InItemStackPopup() || InKeyRingPopup())
		return TRUE;
	if (gusMouseXPos < gsVIEWPORT_START_X || gusMouseXPos >= gsVIEWPORT_END_X ||
		gusMouseYPos < gsVIEWPORT_WINDOW_START_Y ||
		gusMouseYPos >= OS0WorldViewportBottom()) return TRUE;
	if (OS0ActivateCurrentNearbyHintInteraction(gusMouseXPos, gusMouseYPos,
		FALSE)) return TRUE;
	// If the pointer is on the small world-attached OPEN icon, retain the exact
	// bound crate relation instead of projecting the ground behind the glyph.
	if (OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos) &&
		OS0ActivateCurrentHoverInteraction(gusMouseXPos, gusMouseYPos))
		return TRUE;

	// Do not trust guiCurrentCursorGridNo/gUIFullTarget here: scrolling can move
	// the world beneath a stationary pointer without delivering a mouse event.
	OS0PointerSnapshot const pointer =
		OS0CapturePointerSnapshot(gsInterfaceLevel);
	if (!pointer.hasWorldPoint)
	{
		return OS0ActivateHoveredInteraction(nullptr, NOWHERE,
			gsInterfaceLevel, NO_TILE, gusMouseXPos, gusMouseYPos);
	}
	return OS0ActivateHoveredInteraction(pointer.actor, pointer.gridNo,
		pointer.level, pointer.tileIndex, pointer.screenX, pointer.screenY,
		pointer.worldItemIndex);
}

void OS0RefreshWorldHoverFromPointer()
{
	// The short RMB radial and the native graphical stance/command surface share
	// one gesture owner. A validated hold wins once; its later release cannot also
	// open the short-press radial.
	TryOpenRightHoldCharacterMenu();
	if (OS0CreatorIsActive())
	{
		if (gHoverProjection.valid) OS0ClearWorldHover();
		gHoverProjection.valid = FALSE;
		return;
	}
	// This glyph is the projected child of the current immutable world binding.
	// Treating it like an unrelated panel clears the binding one frame after
	// GAIN_MOUSE and makes the button disappear before it can be clicked.
	if (OS0HoverQuickActionOwnsPointer(gusMouseXPos, gusMouseYPos)) return;
	// Nearby affordances are projected children of their immutable world
	// bindings as well. Revalidate and preserve that binding while the pointer
	// crosses from the object onto its glyph; otherwise the inspector flickers
	// away one frame after GAIN_MOUSE.
	if (OS0RefreshCurrentNearbyHintHover(gusMouseXPos, gusMouseYPos)) return;
	if (OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos))
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
	const UINT32 worldRevision = WorldTileMutationRevision();
	const UINT32 worldItemRevision = WorldItemMutationRevision();
	const BOOLEAN projectionChanged = gHoverProjection.valid &&
		(gHoverProjection.renderX != gsRenderCenterX ||
		 gHoverProjection.renderY != gsRenderCenterY ||
		 gHoverProjection.zoom != zoom);
	const UINT16 heldItem = gpItemPointer ? gpItemPointer->usItem : NOTHING;
	const UINT8 heldCount = gpItemPointer ? gpItemPointer->ubNumberOfObjects : 0;
	BOOLEAN movingActor = FALSE;
	if (gHoverProjection.actorId != NOBODY)
	{
		SOLDIERTYPE const* const actor = ID2Soldier(gHoverProjection.actorId);
		// A slot can be recycled by the live editor.  The cached actor relation is
		// valid only for the exact instance which produced it.
		movingActor = !actor || !actor->bActive ||
			actor->uiUniqueSoldierIdValue != gHoverProjection.actorInstanceId ||
			(gAnimControl[actor->usAnimState].uiFlags & ANIM_MOVING);
	}
	else
	{
		UINT32 const now = GetJA2Clock();
		if (!gHoverProjection.valid || now >= gNextNearbyActorMotionProbeAt)
		{
			GridNo pointerGrid = NOWHERE;
			INT16 worldX = 0;
			INT16 worldY = 0;
			gNearbyActorWasMoving = FALSE;
			if (OS0ProjectTacticalScreenToWorld(gusMouseXPos, gusMouseYPos,
				worldX, worldY, &pointerGrid))
			{
				FOR_EACH_MERC(i)
				{
					SOLDIERTYPE const* const soldier = *i;
					if (soldier && soldier->bActive &&
						soldier->bLevel == gsInterfaceLevel &&
						PythSpacesAway(soldier->sGridNo, pointerGrid) <= 3 &&
						(gAnimControl[soldier->usAnimState].uiFlags & ANIM_MOVING))
					{
						gNearbyActorWasMoving = TRUE;
						break;
					}
				}
			}
			// Actor proximity is a cheap 30 Hz probe.  The expensive sprite/world
			// hit-test remains frame-accurate only while a nearby actor is moving.
			gNextNearbyActorMotionProbeAt = now + 33;
		}
		movingActor = gNearbyActorWasMoving;
	}
	if (gHoverProjection.valid &&
		gHoverProjection.mouseX == static_cast<INT16>(gusMouseXPos) &&
		gHoverProjection.mouseY == static_cast<INT16>(gusMouseYPos) &&
		gHoverProjection.renderX == gsRenderCenterX &&
		gHoverProjection.renderY == gsRenderCenterY &&
		gHoverProjection.level == gsInterfaceLevel &&
		gHoverProjection.zoom == zoom &&
		gHoverProjection.heldItem == heldItem &&
		gHoverProjection.heldCount == heldCount &&
		gHoverProjection.worldRevision == worldRevision &&
		gHoverProjection.worldItemRevision == worldItemRevision &&
		gHoverProjection.actorId == NOBODY && !movingActor)
		return;
	gHoverProjection = { static_cast<INT16>(gusMouseXPos),
		static_cast<INT16>(gusMouseYPos), gsRenderCenterX, gsRenderCenterY,
		static_cast<UINT8>(gsInterfaceLevel), zoom, heldItem, heldCount,
		worldRevision, worldItemRevision, NOBODY, 0, TRUE };

	OS0PointerSnapshot const pointer =
		OS0CapturePointerSnapshot(gsInterfaceLevel);
	if (!pointer.hasWorldPoint)
	{
		OS0ClearWorldHover();
		return;
	}
	// UpdateOS0TacticalSession runs before ScrollWorld. Reapply the same pointer
	// relation here after the displayed camera is committed so facing never lags
	// one camera frame behind hover/aim. The direct-control owner decides whether
	// facing is currently allowed; turn-based AP ownership remains untouched.
	OS0RefreshDirectControlFacing(GetSelectedMan(), pointer.worldX,
		pointer.worldY, projectionChanged);
	OS0HoverWorldObject(pointer.actor, pointer.gridNo, pointer.level,
		pointer.tileIndex, pointer.screenX, pointer.screenY,
		pointer.worldItemIndex);
	gHoverProjection.actorId = pointer.actor ? Soldier2ID(pointer.actor) : NOBODY;
	gHoverProjection.actorInstanceId = pointer.actor ?
		pointer.actor->uiUniqueSoldierIdValue : 0;
}

void OS0InvalidateWorldHoverProjection()
{
	gHoverProjection.valid = FALSE;
	gNextNearbyActorMotionProbeAt = 0;
	gNearbyActorWasMoving = FALSE;
}

void OS0NotifyWorldMutation()
{
	gHoverProjection.valid = FALSE;
	NotifyWorldTileMutation();
}

UINT32 OS0WorldMutationRevision()
{
	return WorldTileMutationRevision();
}

BOOLEAN OS0HandleViewportPointerEvent(MOUSE_REGION* region, UINT32 reason)
{
	BOOLEAN stableDoubleClick = FALSE;
	if (gPointerGestures.suppressesPrimary() &&
		(reason & (MSYS_CALLBACK_REASON_POINTER_DWN |
			MSYS_CALLBACK_REASON_POINTER_UP |
			MSYS_CALLBACK_REASON_POINTER_DOUBLECLICK)))
	{
		// Re-entering the viewport while the cancelled press is still held must
		// never synthesize a fresh vanilla move/select gesture.
		fLeftButtonDown = FALSE;
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
			gPointerGestures.finishCancelledPrimaryRelease();
		return TRUE;
	}
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
	if (reason & MSYS_CALLBACK_REASON_MOVE)
	{
		if (!gWorldDrag.armed) return FALSE;
		if (!region || !(region->ButtonState & MSYS_LEFT_BUTTON))
		{
			gWorldDrag.reset();
			return FALSE;
		}
		// Promotion may detach a loose item or seed container contents, both of
		// which legitimately advance the world revision. Once active, the drag's
		// exact transaction/carry binding is authoritative; the revision is only
		// an immutable pre-promotion guard.
		if (gWorldDrag.active) return TRUE;
		if (gWorldDrag.source.worldRevision == 0 ||
			gWorldDrag.source.worldRevision != OS0WorldMutationRevision())
		{
			gWorldDrag.reset();
			return FALSE;
		}
		if (gWorldDrag.source.worldItemIndex >= 0 &&
			(gWorldDrag.source.worldItemRevision == 0 ||
			 gWorldDrag.source.worldItemRevision != WorldItemMutationRevision()))
		{
			gWorldDrag.reset();
			return FALSE;
		}
		if (!gWorldDrag.thresholdReached(gusMouseXPos, gusMouseYPos)) return FALSE;
		if (!OS0BeginWorldPointerDrag(gWorldDrag.source.gridNo,
			gWorldDrag.source.level, gWorldDrag.source.tileIndex,
			gWorldDrag.source.worldItemIndex))
		{
			gWorldDrag.reset();
			return FALSE;
		}
		gWorldDrag.activate();
		gPointerGestures.claimPrimary();
		gDoubleTap.reset();
		// The native RT/TB poller saw the initial DOWN while it was still a normal
		// click. Promotion transfers ownership before it can synthesize a MOVE.
		fLeftButtonDown = FALSE;
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
	{
		OS0PointerSnapshot const press =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
		OS0ViewportTapIdentity identity;
		identity.actorId = press.actor ?
			static_cast<INT8>(Soldier2ID(press.actor)) : static_cast<INT8>(-1);
		identity.actorInstanceId = press.actor ?
			press.actor->uiUniqueSoldierIdValue : 0;
		identity.gridNo = press.hasWorldPoint ? press.gridNo : NOWHERE;
		identity.level = press.level;
		identity.tileIndex = press.tileIndex;
		identity.worldItemIndex = press.worldItemIndex;
		identity.worldRevision = OS0WorldMutationRevision();
		identity.worldItemRevision = WorldItemMutationRevision();
		stableDoubleClick = gDoubleTap.observe(GetJA2Clock(), press.screenX,
			press.screenY, identity,
			(reason & MSYS_CALLBACK_REASON_POINTER_DOUBLECLICK) != 0);
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reconcile(gpItemPointer != nullptr);
		if (gpItemPointer) transfers.beginHeldGesture();
		const BOOLEAN owned = OS0OwnsViewportPrimaryButton();
		gWorldDrag.reset();
		if (!owned && !gpItemPointer && !press.actor && press.hasWorldPoint &&
			OS0CanBeginWorldPointerDrag(press.gridNo, press.level,
				press.tileIndex, press.worldItemIndex))
			gWorldDrag.arm(press.screenX, press.screenY, identity);
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
		ClearRightPressTarget();
		gRightPressStartedAt = GetJA2Clock();
		OS0PointerSnapshot const pointer =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
		gRightPressScreenX = pointer.screenX;
		gRightPressScreenY = pointer.screenY;
		gRightPressGridNo = pointer.hasWorldPoint ? pointer.gridNo : NOWHERE;
		gRightPressLevel = pointer.level;
		gRightPressTileIndex = pointer.tileIndex;
		gRightPressWorldItemIndex = pointer.worldItemIndex;
		gRightPressWorldRevision = OS0WorldMutationRevision();
		gRightPressWorldItemRevision = WorldItemMutationRevision();
		if (pointer.actor && pointer.actor->bActive &&
			pointer.actor->bTeam == OUR_TEAM &&
			pointer.actor->uiUniqueSoldierIdValue != 0)
		{
			gRightPressActorId = Soldier2ID(pointer.actor);
			gRightPressActorInstanceId =
				pointer.actor->uiUniqueSoldierIdValue;
		}
		fRightButtonDown = FALSE;
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_MBUTTON_DWN)
	{
		gPointerGestures.armMiddle();
		fMiddleButtonDown = FALSE;
		return TRUE;
	}
	if ((reason & MSYS_CALLBACK_REASON_POINTER_DOUBLECLICK) &&
		stableDoubleClick)
	{
		gWorldDrag.reset();
		if (gPointerGestures.ownsPrimary() || OS0OwnsViewportPrimaryButton())
		{
			gPointerGestures.markPrimaryReleaseHandled();
			return TRUE;
		}
		// The second release belongs to this OS//0 open operation even though the
		// first DOWN was vanilla MOVE-owned. Hand it off once so RT/TB input cannot
		// also interpret the same physical double-click as movement/selection.
		gPointerGestures.markPrimaryReleaseHandled();
		OS0PointerSnapshot const pointer =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
		if (pointer.actor)
		{
			OS0OpenCharacterPanel(pointer.actor);
		}
		else if (pointer.hasWorldPoint)
		{
			OS0ActivateWorldObject(pointer.gridNo, pointer.level,
				pointer.tileIndex, pointer.worldItemIndex);
		}
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		gWorldDrag.reset();
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
				OS0PointerSnapshot const pointer =
					OS0CapturePointerSnapshot(gsInterfaceLevel);
				// A controller-owned drag never falls through into JA2's second,
				// polled drop path. Unsupported relations leave the item held.
				OS0HandleCursorAction(pointer.actor, pointer.gridNo,
					pointer.level, pointer.tileIndex, pointer.worldItemIndex);
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
		OS0PointerSnapshot const pointer =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
		if (owned)
		{
			if (OS0HandlePendingWorldMove(pointer.gridNo)) return TRUE;
			const BOOLEAN heldItem = gpItemPointer != nullptr;
			if (OS0HandleCursorAction(pointer.actor, pointer.gridNo,
				pointer.level, pointer.tileIndex, pointer.worldItemIndex) && heldItem)
				gPointerGestures.markHeldItemReleaseHandled();
			// A gesture is bound to the owner chosen on button-down. Mode changes,
			// lost-mouse resets or re-entry while held can never turn a vanilla MOVE
			// click into a late OS//0 shot/tool action on release.
			return TRUE;
		}
		// Selection updates OS0 projections but does not steal ordinary JA2
		// movement/selection from the legacy primary-button owner.
		OS0SelectWorldObject(pointer.actor, pointer.gridNo, pointer.level,
			pointer.tileIndex);
	}
	if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		fRightButtonDown = FALSE;
		const BOOLEAN shortPress = gPointerGestures.releaseRight();
		SOLDIERTYPE* target = nullptr;
		if (gRightPressActorId != NOBODY && gRightPressActorInstanceId != 0)
		{
			target = ID2Soldier(gRightPressActorId);
			if (!target || !target->bActive ||
				target->uiUniqueSoldierIdValue != gRightPressActorInstanceId)
				target = nullptr;
		}
		const GridNo gridNo = gRightPressGridNo;
		const UINT8 level = gRightPressLevel;
		const UINT16 tileIndex = gRightPressTileIndex;
		const INT32 worldItemIndex = gRightPressWorldItemIndex;
		const UINT32 worldItemRevision = gRightPressWorldItemRevision;
		const INT16 screenX = gRightPressScreenX;
		const INT16 screenY = gRightPressScreenY;
		const INT32 dragX = static_cast<INT32>(gusMouseXPos) - screenX;
		const INT32 dragY = static_cast<INT32>(gusMouseYPos) - screenY;
		const BOOLEAN stablePress = dragX * dragX + dragY * dragY <= 36 &&
			gRightPressWorldRevision != 0 &&
			gRightPressWorldRevision == OS0WorldMutationRevision() &&
			(worldItemIndex < 0 ||
				(worldItemRevision != 0 &&
				 worldItemRevision == WorldItemMutationRevision())) &&
			gridNo != NOWHERE &&
			(gRightPressActorId == NOBODY || target != nullptr);
		ClearRightPressTarget();
		if (!shortPress || !stablePress) return TRUE;
		OS0OpenContextMenu(target, gridNo, level, tileIndex, screenX, screenY,
			worldItemIndex);
		return TRUE;
	}
	if (reason & MSYS_CALLBACK_REASON_MBUTTON_UP)
	{
		fMiddleButtonDown = FALSE;
		if (!gPointerGestures.releaseMiddle()) return TRUE;
		OS0PointerSnapshot const pointer =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
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
			OS0CycleCursorAction(pointer.actor, pointer.gridNo, pointer.level,
				pointer.tileIndex, pointer.worldItemIndex);
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

void OS0CancelViewportPointerGesturesOnLostMouse()
{
	// Quick-action, nearby-hint and equipment regions live inside the tactical
	// viewport. Crossing one of them produces LOST_MOUSE even though the same
	// physical world drag is still held. Preserve its capture until the raw UP;
	// OS0RecoverViewportPointerGestures remains the focus-loss safety net once
	// the physical button is actually released.
	const BOOLEAN physicalPrimaryDown =
		IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown();
	if (gWorldDrag.active && physicalPrimaryDown)
	{
		gDoubleTap.reset();
		ClearRightPressTarget();
		gHoverProjection = {};
		gNextNearbyActorMotionProbeAt = 0;
		gNearbyActorWasMoving = FALSE;
		return;
	}
	if (gWorldDrag.active) OS0CancelWorldPointerDrag();
	gWorldDrag.reset();
	gPointerGestures.cancelOnPointerLost();
	gDoubleTap.reset();
	ClearRightPressTarget();
	gHoverProjection = {};
	gNextNearbyActorMotionProbeAt = 0;
	gNearbyActorWasMoving = FALSE;
}

void OS0RecoverViewportPointerGestures(BOOLEAN const physicalPrimaryDown)
{
	if (!physicalPrimaryDown && gWorldDrag.armed)
	{
		if (gWorldDrag.active) OS0CancelWorldPointerDrag();
		gWorldDrag.reset();
	}
	gPointerGestures.recoverPhysicalPrimaryRelease(physicalPrimaryDown);
}

void OS0ResetViewportPointerGestures()
{
	if (gWorldDrag.active) OS0CancelWorldPointerDrag();
	gWorldDrag.reset();
	gPointerGestures.reset();
	gDoubleTap.reset();
	ClearRightPressTarget();
	// Losing viewport hover is not the end of a physical item gesture.  The
	// pointer may be crossing into inventory/equipment, whose release callback
	// must still be able to claim the same controller-owned button-down.
	gHoverProjection = {};
	gNextNearbyActorMotionProbeAt = 0;
	gNearbyActorWasMoving = FALSE;
}
