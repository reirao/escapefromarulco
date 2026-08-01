#include "OS0_ItemTransferTransaction.h"


bool OS0ItemTransferTransaction::begin(
	OS0ItemTransferOrigin const origin) noexcept
{
	if (phase_ != OS0ItemTransactionPhase::EMPTY || !origin.valid())
		return false;
	origin_ = origin;
	phase_ = OS0ItemTransactionPhase::HELD;
	return true;
}


bool OS0ItemTransferTransaction::commit() noexcept
{
	if (phase_ != OS0ItemTransactionPhase::HELD) return false;
	phase_ = OS0ItemTransactionPhase::COMMITTED;
	// A committed item belongs to its destination.  Retaining the old source
	// would make a later cancel capable of duplicating it.
	origin_ = {};
	return true;
}


bool OS0ItemTransferTransaction::cancel() noexcept
{
	if (phase_ != OS0ItemTransactionPhase::HELD || !origin_.valid())
		return false;
	phase_ = OS0ItemTransactionPhase::CANCELLED;
	return true;
}


OS0ItemRestorationDecision
OS0ItemTransferTransaction::restorationDecision() const noexcept
{
	if (phase_ != OS0ItemTransactionPhase::CANCELLED || !origin_.valid())
		return {};

	OS0ItemRestorationKind restoration = OS0ItemRestorationKind::NONE;
	switch (origin_.kind)
	{
		case OS0ItemTransferOriginKind::INVENTORY:
			restoration = OS0ItemRestorationKind::INVENTORY_SLOT;
			break;
		case OS0ItemTransferOriginKind::WORLD:
			restoration = OS0ItemRestorationKind::WORLD_LOCATION;
			break;
		case OS0ItemTransferOriginKind::CONTAINER:
			restoration = OS0ItemRestorationKind::CONTAINER_LOCATION;
			break;
		case OS0ItemTransferOriginKind::NONE:
			return {};
	}
	return { restoration, origin_ };
}


bool OS0ItemTransferTransaction::acknowledgeRestored() noexcept
{
	if (phase_ != OS0ItemTransactionPhase::CANCELLED || !origin_.valid())
		return false;
	origin_ = {};
	return true;
}


bool OS0ItemTransferTransaction::resumeAfterFailedRestoration() noexcept
{
	if (phase_ != OS0ItemTransactionPhase::CANCELLED || !origin_.valid())
		return false;
	phase_ = OS0ItemTransactionPhase::HELD;
	return true;
}


void OS0ItemTransferTransaction::reset() noexcept
{
	phase_ = OS0ItemTransactionPhase::EMPTY;
	origin_ = {};
}
