#pragma once

#include "Types.h"

struct MOUSE_REGION;

struct OS0ViewportGestureState
{
	BOOLEAN rightArmed = FALSE;
	BOOLEAN middleArmed = FALSE;
	BOOLEAN handledHeldItemRelease = FALSE;

	void beginPrimary() noexcept { handledHeldItemRelease = FALSE; }
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
	void reset() noexcept { *this = {}; }
};

// Owns OS0's pointer gestures inside the tactical viewport.  Mouse-system
// callbacks run before JA2's legacy polled input, so a consumed held-item
// release can be handed off exactly once without time-based debounce guards.
BOOLEAN OS0HandleViewportPointerEvent(MOUSE_REGION* region, UINT32 reason);
BOOLEAN OS0ConsumeHandledHeldItemRelease();
BOOLEAN OS0OwnsViewportContextButtons();
void OS0ResetViewportPointerGestures();
