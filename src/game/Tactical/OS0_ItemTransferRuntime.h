#pragma once

#include "OS0_ItemTransferTransaction.h"

#include "Item_Types.h"

#include <cstdint>


enum class OS0ItemTransferCancelResult : std::uint8_t
{
	RESTORED,
	NO_ACTIVE_TRANSFER,
	NATIVE_ITEM_MISSING,
	NATIVE_ITEM_CHANGED,
	RESTORE_FAILED_ITEM_HELD
};


// Engine adapter for one OS0 item-transfer transaction.
//
// A begin* call is made immediately after JA2 has detached the source object
// and installed it as gpItemPointer.  The adapter associates that native
// cursor carrier with a monotonically issued, session-unique identity; no
// identity data is written into OBJECTTYPE and its save-compatible layout is
// therefore untouched.
//
// commit() is called only after a destination has accepted the complete source
// item.  reset() never changes JA2's native item pointer.  cancel() is the only
// operation which restores a source: it either restores the exact actor slot
// or exact world position, or leaves the complete item on the cursor.
class OS0ItemTransferRuntime
{
public:
	bool beginInventory(std::uint16_t actorId,
		std::uint64_t actorInstanceId, std::int16_t slot) noexcept;
	bool beginWorld(std::int32_t gridNo, std::int8_t level,
		std::int8_t bVisible, std::uint16_t usFlags,
		std::int8_t bRenderZHeightAboveLevel) noexcept;
	bool beginContainer(std::int32_t gridNo, std::int8_t level,
		std::uint16_t containerTileIndex, std::int8_t bVisible,
		std::uint16_t usFlags, std::int8_t bRenderZHeightAboveLevel) noexcept;

	// Binds metadata captured before detachment to the complete OBJECTTYPE now
	// held by JA2.  The item identity and full native snapshot are issued here,
	// after BeginItemPointer has established gpItemPointer.
	bool bindAfterDetach(OS0ItemTransferOrigin origin) noexcept;

	bool commit() noexcept;
	OS0ItemTransferCancelResult cancel() noexcept;
	void reset() noexcept;

	bool held() const noexcept { return transaction_.held(); }
	bool hasOrigin() const noexcept { return transaction_.hasOrigin(); }
	bool nativePointerStillBound() const noexcept;
	OS0ItemTransferOrigin const& origin() const noexcept
	{
		return transaction_.origin();
	}
	OS0TransferredItemIdentity identity() const noexcept
	{
		return transaction_.origin().item;
	}

private:
	bool beginSpatial(OS0ItemTransferOriginKind kind, std::int32_t gridNo,
		std::int8_t level, std::uint16_t containerTileIndex,
		std::int8_t bVisible, std::uint16_t usFlags,
		std::int8_t bRenderZHeightAboveLevel) noexcept;
	bool restoreInventory(OS0ItemTransferOrigin const& origin) noexcept;
	bool restoreSpatial(OS0ItemTransferOrigin const& origin) noexcept;
	bool containerIdentityStillValid(
		OS0ItemTransferOrigin const& origin) const noexcept;
	void clearBinding() noexcept;

	OS0ItemTransferTransaction transaction_;
	OBJECTTYPE const* nativePointerCarrier_ = nullptr;
	OBJECTTYPE heldItemAfterDetach_{};
	bool hasHeldItemSnapshot_ = false;
	OBJECTTYPE inventorySlotAfterDetach_{};
	bool hasInventorySlotSnapshot_ = false;
};


OS0ItemTransferRuntime& OS0GetItemTransferRuntime();

// Compares every persisted OBJECTTYPE field while deliberately ignoring the
// one structural padding byte before its union.  Native item transfers and
// deferred pickup guards share this definition so they cannot disagree about
// whether an item was replaced while an action was in flight.
bool OS0SameObjectRepresentation(OBJECTTYPE const& lhs,
	OBJECTTYPE const& rhs) noexcept;
