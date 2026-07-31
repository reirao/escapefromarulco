#pragma once

#include "Types.h"

struct MOUSE_REGION;

struct OS0ViewportGestureState
{
	BOOLEAN rightArmed = FALSE;
	BOOLEAN middleArmed = FALSE;
	BOOLEAN primaryOwned = FALSE;
	BOOLEAN handledPrimaryRelease = FALSE;
	BOOLEAN handledHeldItemRelease = FALSE;

	void beginPrimary(BOOLEAN owned = FALSE) noexcept
	{
		primaryOwned = owned;
		handledPrimaryRelease = FALSE;
		handledHeldItemRelease = FALSE;
	}
	BOOLEAN ownsPrimary() const noexcept { return primaryOwned; }
	BOOLEAN releasePrimary() noexcept
	{
		const BOOLEAN owned = primaryOwned;
		primaryOwned = FALSE;
		handledPrimaryRelease = handledPrimaryRelease || owned;
		return owned;
	}
	void armRight() noexcept { rightArmed = TRUE; }
	void armMiddle() noexcept { middleArmed = TRUE; }
	void markHeldItemReleaseHandled() noexcept
	{
		handledHeldItemRelease = TRUE;
	}
	BOOLEAN releaseRight() noexcept
	{
		const BOOLEAN armed = rightArmed;
		rightArmed = FALSE;
		return armed;
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
		if (primaryOwned) return TRUE;
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
// True while OS//0 owns a held primary gesture and once more on its release.
// The legacy RT/TB poller consumes this hand-off to avoid observing the same
// physical click after the viewport callback already executed it.
BOOLEAN OS0ConsumeViewportPrimaryGesture();
BOOLEAN OS0ConsumeHandledHeldItemRelease();
BOOLEAN OS0OwnsViewportContextButtons();
void OS0ResetViewportPointerGestures();
