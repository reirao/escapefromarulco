#pragma once

#include "Types.h"


enum class OS0ItemTransferSurface : UINT8
{
	NONE,
	INVENTORY,
	LOOT,
	RELATION,
	WORLD,
	EXTERNAL
};


enum class OS0ItemTransferPhase : UINT8
{
	IDLE,
	SOURCE_PRESSED,
	ITEM_HELD
};


enum class OS0ItemReleaseClaim : UINT8
{
	NONE,
	SOURCE_CLICK,
	ITEM
};


// Owns the complete physical gesture around JA2's native item pointer.
//
// The engine cursor remains the authoritative OBJECTTYPE carrier. This small
// state machine supplies the ownership JA2's original panel callbacks assume:
// one source press, one held item and at most one release target. It deliberately
// contains no UI or inventory policy so container, actor and world surfaces all
// share the same invariants without depending on each other.
class OS0ItemTransferController
{
public:
	BOOLEAN beginSourcePress(OS0ItemTransferSurface surface, UINT32 sourceId,
		INT16 screenX, INT16 screenY) noexcept;
	BOOLEAN sourceMatches(OS0ItemTransferSurface surface,
		UINT32 sourceId) const noexcept;
	BOOLEAN dragThresholdReached(OS0ItemTransferSurface surface,
		UINT32 sourceId, INT16 screenX, INT16 screenY,
		INT16 threshold = 4) const noexcept;
	BOOLEAN markItemHeld(OS0ItemTransferSurface surface,
		UINT32 sourceId) noexcept;

	// Used when an item cursor was created by a legacy/native path. Adopting it
	// does not claim a mouse release; beginHeldGesture does that on the next DOWN.
	void adoptExternalHeldItem() noexcept;
	// UI commands such as stack confirmation create the cursor on POINTER_UP.
	// Record that release immediately so the legacy viewport cannot also drop it.
	void adoptExternalHeldItemAfterHandledRelease() noexcept;
	void observePrimaryDown() noexcept;
	BOOLEAN beginHeldGesture() noexcept;
	OS0ItemReleaseClaim claimRelease(OS0ItemTransferSurface target) noexcept;
	void completeItemRelease(BOOLEAN itemStillHeld) noexcept;

	// Double-click/context transitions can invalidate a pending source press.
	// The eventual UP stays consumed, so it cannot leak into a newly opened UI.
	void cancelGestureAndConsumeRelease() noexcept;
	BOOLEAN consumeSuppressedRelease() noexcept;
	void reconcile(BOOLEAN nativeItemHeld) noexcept;
	// SDL can clear its physical button state when the window loses focus
	// without delivering an UP event. Finish only the controller ownership; a
	// native item remains on the cursor for the next explicit drop.
	BOOLEAN recoverLostRelease(BOOLEAN physicalButtonDown,
		BOOLEAN nativeItemHeld) noexcept;

	BOOLEAN itemHeld() const noexcept
	{
		return phase_ == OS0ItemTransferPhase::ITEM_HELD;
	}
	BOOLEAN sourcePressed() const noexcept
	{
		return phase_ == OS0ItemTransferPhase::SOURCE_PRESSED;
	}
	BOOLEAN ownsPhysicalGesture() const noexcept { return buttonDown_; }
	BOOLEAN releaseWasHandled() const noexcept { return releaseHandled_; }
	BOOLEAN waitingForSuppressedRelease() const noexcept
	{
		return suppressNextRelease_;
	}
	BOOLEAN consumeHandledRelease() noexcept;
	OS0ItemTransferPhase phase() const noexcept { return phase_; }
	OS0ItemTransferSurface sourceSurface() const noexcept { return source_; }
	OS0ItemTransferSurface targetSurface() const noexcept { return target_; }
	UINT32 sourceId() const noexcept { return sourceId_; }
	void reset() noexcept;

private:
	void clearTransfer() noexcept;

	OS0ItemTransferPhase phase_ = OS0ItemTransferPhase::IDLE;
	OS0ItemTransferSurface source_ = OS0ItemTransferSurface::NONE;
	OS0ItemTransferSurface target_ = OS0ItemTransferSurface::NONE;
	UINT32 sourceId_ = 0;
	INT16 pressX_ = 0;
	INT16 pressY_ = 0;
	BOOLEAN buttonDown_ = FALSE;
	BOOLEAN releaseHandled_ = FALSE;
	BOOLEAN suppressNextRelease_ = FALSE;
};


OS0ItemTransferController& OS0GetItemTransferController();
