#include "OS0_ItemTransferController.h"


namespace
{
	OS0ItemTransferController gItemTransfers;
}


BOOLEAN OS0ItemTransferController::beginSourcePress(
	OS0ItemTransferSurface const surface, UINT32 const sourceId,
	INT16 const screenX, INT16 const screenY) noexcept
{
	if (surface == OS0ItemTransferSurface::NONE ||
		phase_ != OS0ItemTransferPhase::IDLE || suppressNextRelease_) return FALSE;
	phase_ = OS0ItemTransferPhase::SOURCE_PRESSED;
	source_ = surface;
	target_ = OS0ItemTransferSurface::NONE;
	sourceId_ = sourceId;
	pressX_ = screenX;
	pressY_ = screenY;
	buttonDown_ = TRUE;
	releaseHandled_ = FALSE;
	suppressNextRelease_ = FALSE;
	return TRUE;
}


BOOLEAN OS0ItemTransferController::sourceMatches(
	OS0ItemTransferSurface const surface, UINT32 const sourceId) const noexcept
{
	return phase_ != OS0ItemTransferPhase::IDLE && source_ == surface &&
		sourceId_ == sourceId;
}


BOOLEAN OS0ItemTransferController::dragThresholdReached(
	OS0ItemTransferSurface const surface, UINT32 const sourceId,
	INT16 const screenX, INT16 const screenY, INT16 const threshold) const noexcept
{
	if (phase_ != OS0ItemTransferPhase::SOURCE_PRESSED || !buttonDown_ ||
		!sourceMatches(surface, sourceId)) return FALSE;
	const int dx = static_cast<int>(screenX) - pressX_;
	const int dy = static_cast<int>(screenY) - pressY_;
	return dx >= threshold || dx <= -threshold ||
		dy >= threshold || dy <= -threshold;
}


BOOLEAN OS0ItemTransferController::markItemHeld(
	OS0ItemTransferSurface const surface, UINT32 const sourceId) noexcept
{
	if (phase_ != OS0ItemTransferPhase::SOURCE_PRESSED || !buttonDown_ ||
		!sourceMatches(surface, sourceId)) return FALSE;
	phase_ = OS0ItemTransferPhase::ITEM_HELD;
	return TRUE;
}


void OS0ItemTransferController::adoptExternalHeldItem() noexcept
{
	if (phase_ == OS0ItemTransferPhase::ITEM_HELD) return;
	clearTransfer();
	phase_ = OS0ItemTransferPhase::ITEM_HELD;
	source_ = OS0ItemTransferSurface::EXTERNAL;
}


void OS0ItemTransferController::adoptExternalHeldItemAfterHandledRelease() noexcept
{
	adoptExternalHeldItem();
	buttonDown_ = FALSE;
	target_ = OS0ItemTransferSurface::NONE;
	releaseHandled_ = TRUE;
	suppressNextRelease_ = FALSE;
}


void OS0ItemTransferController::observePrimaryDown() noexcept
{
	// A handled-release latch belongs to the gesture that just ended. A new
	// physical DOWN is a hard boundary even when the legacy poller has not yet
	// consumed that latch (possible with very fast queued input).
	if (!buttonDown_ && !suppressNextRelease_)
	{
		releaseHandled_ = FALSE;
		target_ = OS0ItemTransferSurface::NONE;
	}
}


BOOLEAN OS0ItemTransferController::beginHeldGesture() noexcept
{
	if (phase_ != OS0ItemTransferPhase::ITEM_HELD || buttonDown_) return FALSE;
	buttonDown_ = TRUE;
	target_ = OS0ItemTransferSurface::NONE;
	releaseHandled_ = FALSE;
	return TRUE;
}


OS0ItemReleaseClaim OS0ItemTransferController::claimRelease(
	OS0ItemTransferSurface const target) noexcept
{
	if (!buttonDown_ || releaseHandled_ ||
		target == OS0ItemTransferSurface::NONE) return OS0ItemReleaseClaim::NONE;
	buttonDown_ = FALSE;
	releaseHandled_ = TRUE;
	target_ = target;
	if (phase_ == OS0ItemTransferPhase::SOURCE_PRESSED)
	{
		phase_ = OS0ItemTransferPhase::IDLE;
		source_ = OS0ItemTransferSurface::NONE;
		sourceId_ = 0;
		return OS0ItemReleaseClaim::SOURCE_CLICK;
	}
	return phase_ == OS0ItemTransferPhase::ITEM_HELD ?
		OS0ItemReleaseClaim::ITEM : OS0ItemReleaseClaim::NONE;
}


void OS0ItemTransferController::completeItemRelease(
	BOOLEAN const itemStillHeld) noexcept
{
	buttonDown_ = FALSE;
	target_ = OS0ItemTransferSurface::NONE;
	if (itemStillHeld)
	{
		phase_ = OS0ItemTransferPhase::ITEM_HELD;
		// A completed drag no longer has a reversible source-slot gesture. The
		// native cursor owns the object until the next explicit relation.
		source_ = OS0ItemTransferSurface::EXTERNAL;
		sourceId_ = 0;
	}
	else
	{
		clearTransfer();
	}
}


void OS0ItemTransferController::cancelGestureAndConsumeRelease() noexcept
{
	clearTransfer();
	buttonDown_ = FALSE;
	// DOUBLECLICK is reported on the second DOWN, and stack split is opened while
	// the original drag button is still held. Do not publish a handled release
	// early: the legacy poller could consume it before the physical UP exists.
	releaseHandled_ = FALSE;
	suppressNextRelease_ = TRUE;
}


BOOLEAN OS0ItemTransferController::consumeSuppressedRelease() noexcept
{
	if (!suppressNextRelease_) return FALSE;
	suppressNextRelease_ = FALSE;
	buttonDown_ = FALSE;
	releaseHandled_ = TRUE;
	return TRUE;
}


void OS0ItemTransferController::reconcile(BOOLEAN const nativeItemHeld) noexcept
{
	if (nativeItemHeld)
	{
		if (phase_ == OS0ItemTransferPhase::IDLE) adoptExternalHeldItem();
	}
	else if (phase_ == OS0ItemTransferPhase::ITEM_HELD)
	{
		clearTransfer();
		buttonDown_ = FALSE;
	}
}


BOOLEAN OS0ItemTransferController::recoverLostRelease(
	BOOLEAN const physicalButtonDown, BOOLEAN const nativeItemHeld) noexcept
{
	if (physicalButtonDown) return FALSE;
	if (suppressNextRelease_) return consumeSuppressedRelease();
	if (!buttonDown_) return FALSE;

	OS0ItemReleaseClaim const claim =
		claimRelease(OS0ItemTransferSurface::EXTERNAL);
	if (claim == OS0ItemReleaseClaim::ITEM)
		completeItemRelease(nativeItemHeld);
	return claim != OS0ItemReleaseClaim::NONE;
}


BOOLEAN OS0ItemTransferController::consumeHandledRelease() noexcept
{
	const BOOLEAN handled = releaseHandled_;
	releaseHandled_ = FALSE;
	return handled;
}


void OS0ItemTransferController::reset() noexcept
{
	*this = {};
}


void OS0ItemTransferController::clearTransfer() noexcept
{
	phase_ = OS0ItemTransferPhase::IDLE;
	source_ = OS0ItemTransferSurface::NONE;
	target_ = OS0ItemTransferSurface::NONE;
	sourceId_ = 0;
	pressX_ = 0;
	pressY_ = 0;
}


OS0ItemTransferController& OS0GetItemTransferController()
{
	return gItemTransfers;
}
