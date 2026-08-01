#pragma once

#include "JA2Types.h"

struct MOUSE_REGION;

struct OS0ViewportTapIdentity
{
	INT8 actorId = -1;
	UINT32 actorInstanceId = 0;
	GridNo gridNo = -1;
	UINT8 level = 0;
	UINT16 tileIndex = 0xffff;
	INT32 worldItemIndex = -1;
	UINT32 worldRevision = 0;
	UINT32 worldItemRevision = 0;

	bool operator==(OS0ViewportTapIdentity const&) const noexcept = default;
};

// JA2 detects double clicks per mouse region; the tactical viewport is one huge
// region. Add the missing spatial and target identity boundary before a second
// click is allowed to become OPEN instead of an unrelated MOVE.
struct OS0ViewportDoubleTapState
{
	BOOLEAN valid = FALSE;
	UINT32 lastAt = 0;
	INT16 lastX = 0;
	INT16 lastY = 0;
	OS0ViewportTapIdentity lastIdentity{};

	BOOLEAN observe(UINT32 now, INT16 x, INT16 y,
		OS0ViewportTapIdentity const& identity,
		BOOLEAN engineDoubleSignal) noexcept
	{
		const INT32 dx = static_cast<INT32>(x) - lastX;
		const INT32 dy = static_cast<INT32>(y) - lastY;
		const BOOLEAN accepted = engineDoubleSignal && valid &&
			now - lastAt <= 400 && dx * dx + dy * dy <= 36 &&
			identity == lastIdentity;
		valid = !accepted;
		lastAt = now;
		lastX = x;
		lastY = y;
		lastIdentity = identity;
		return accepted;
	}

	void reset() noexcept { *this = {}; }
};

// A normal click remains JA2-owned.  Only crossing the drag threshold promotes
// the immutable world target captured on DOWN into an OS0-owned gesture.
struct OS0ViewportWorldDragState
{
	BOOLEAN armed = FALSE;
	BOOLEAN active = FALSE;
	INT16 pressX = 0;
	INT16 pressY = 0;
	OS0ViewportTapIdentity source{};

	void arm(INT16 x, INT16 y, OS0ViewportTapIdentity const& identity) noexcept
	{
		armed = TRUE;
		active = FALSE;
		pressX = x;
		pressY = y;
		source = identity;
	}

	BOOLEAN thresholdReached(INT16 x, INT16 y, INT16 threshold = 5) const noexcept
	{
		if (!armed || active) return FALSE;
		const INT32 dx = static_cast<INT32>(x) - pressX;
		const INT32 dy = static_cast<INT32>(y) - pressY;
		return dx >= threshold || dx <= -threshold ||
			dy >= threshold || dy <= -threshold;
	}

	void activate() noexcept { if (armed) active = TRUE; }
	void reset() noexcept { *this = {}; }
};

struct OS0ViewportGestureState
{
	BOOLEAN rightArmed = FALSE;
	BOOLEAN rightHoldHandled = FALSE;
	BOOLEAN middleArmed = FALSE;
	BOOLEAN primaryOwned = FALSE;
	BOOLEAN primaryCancelledUntilRelease = FALSE;
	BOOLEAN handledPrimaryRelease = FALSE;
	BOOLEAN handledHeldItemRelease = FALSE;

	void beginPrimary(BOOLEAN owned = FALSE) noexcept
	{
		if (primaryCancelledUntilRelease) return;
		primaryOwned = owned;
		handledPrimaryRelease = FALSE;
		handledHeldItemRelease = FALSE;
	}
	BOOLEAN ownsPrimary() const noexcept { return primaryOwned; }
	void claimPrimary() noexcept
	{
		if (!primaryCancelledUntilRelease) primaryOwned = TRUE;
	}
	BOOLEAN suppressesPrimary() const noexcept
	{
		return primaryCancelledUntilRelease;
	}
	void cancelOnPointerLost() noexcept
	{
		rightArmed = FALSE;
		middleArmed = FALSE;
		if (primaryOwned) primaryCancelledUntilRelease = TRUE;
		primaryOwned = FALSE;
	}
	void finishCancelledPrimaryRelease() noexcept
	{
		if (!primaryCancelledUntilRelease) return;
		primaryCancelledUntilRelease = FALSE;
		handledPrimaryRelease = TRUE;
	}
	void recoverPhysicalPrimaryRelease(BOOLEAN physicalButtonDown) noexcept
	{
		if (physicalButtonDown) return;
		if (primaryCancelledUntilRelease)
			finishCancelledPrimaryRelease();
		if (primaryOwned)
		{
			// Raw UP can be routed to a child/overlapping region after an active
			// viewport drag crossed it. End ownership even when the viewport itself
			// did not receive UP, and keep the one-shot legacy suppression bit.
			primaryOwned = FALSE;
			handledPrimaryRelease = TRUE;
		}
	}
	void markPrimaryReleaseHandled() noexcept
	{
		primaryOwned = FALSE;
		handledPrimaryRelease = TRUE;
	}
	BOOLEAN releasePrimary() noexcept
	{
		const BOOLEAN owned = primaryOwned;
		primaryOwned = FALSE;
		handledPrimaryRelease = handledPrimaryRelease || owned;
		return owned;
	}
	void armRight() noexcept
	{
		rightArmed = TRUE;
		rightHoldHandled = FALSE;
	}
	BOOLEAN rightPressActive() const noexcept { return rightArmed; }
	BOOLEAN rightHoldWasHandled() const noexcept { return rightHoldHandled; }
	void markRightHoldHandled() noexcept
	{
		if (rightArmed) rightHoldHandled = TRUE;
	}
	void armMiddle() noexcept { middleArmed = TRUE; }
	void markHeldItemReleaseHandled() noexcept
	{
		handledHeldItemRelease = TRUE;
	}
	BOOLEAN releaseRight() noexcept
	{
		const BOOLEAN activateShortPress = rightArmed && !rightHoldHandled;
		rightArmed = FALSE;
		rightHoldHandled = FALSE;
		return activateShortPress;
	}
	BOOLEAN releaseMiddle() noexcept
	{
		const BOOLEAN armed = middleArmed;
		middleArmed = FALSE;
		return armed;
	}
	BOOLEAN consumeHeldItemRelease() noexcept
	{
		const BOOLEAN handled = handledHeldItemRelease;
		handledHeldItemRelease = FALSE;
		return handled;
	}
	BOOLEAN consumePrimaryGesture() noexcept
	{
		if (primaryOwned || primaryCancelledUntilRelease) return TRUE;
		const BOOLEAN handled = handledPrimaryRelease;
		handledPrimaryRelease = FALSE;
		return handled;
	}
	void reset() noexcept { *this = {}; }
};

// Owns OS0's pointer gestures inside the tactical viewport.  Mouse-system
// callbacks run before JA2's legacy polled input, so a consumed held-item
// release can be handed off exactly once without time-based debounce guards.
BOOLEAN OS0HandleViewportPointerEvent(MOUSE_REGION* region, UINT32 reason);
// Resolves the current tactical pointer afresh and activates its context. This
// is deliberately a one-shot keyboard intent rather than a polled control key.
BOOLEAN OS0TriggerHoveredInteraction();
// Refreshes the live hover relation when either pointer or camera projection
// changed. Mouse-system MOVE events alone are insufficient because scrolling
// and zooming move the world underneath a stationary cursor.
void OS0RefreshWorldHoverFromPointer();
// Marks the cached pointer-to-world relation stale after an item, structure,
// terrain layer or editor command mutates the world under a stationary cursor.
void OS0InvalidateWorldHoverProjection();
// World mutation and projection invalidation are deliberately distinct: UI
// close/scroll events need a fresh hover without making queued object actions
// stale. Mutators use Notify; identity caches bind to the monotonic revision.
void OS0NotifyWorldMutation();
UINT32 OS0WorldMutationRevision();
// True while OS//0 owns a held primary gesture and once more on its release.
// The legacy RT/TB poller consumes this hand-off to avoid observing the same
// physical click after the viewport callback already executed it.
BOOLEAN OS0ConsumeViewportPrimaryGesture();
BOOLEAN OS0ConsumeHandledHeldItemRelease();
BOOLEAN OS0OwnsViewportContextButtons();
// Pointer loss cancels the semantic action but retains legacy-input
// suppression until the same physical press is released.
void OS0CancelViewportPointerGesturesOnLostMouse();
void OS0RecoverViewportPointerGestures(BOOLEAN physicalPrimaryDown);
void OS0ResetViewportPointerGestures();
