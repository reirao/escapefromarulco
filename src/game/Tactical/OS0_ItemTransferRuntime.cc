#include "OS0_ItemTransferRuntime.h"

#include "ContentManager.h"
#include "GameInstance.h"
#include "Handle_Items.h"
#include "Interface_Items.h"
#include "ItemModel.h"
#include "Items.h"
#include "Overhead.h"
#include "Soldier_Control.h"
#include "Structure.h"
#include "TileDat.h"
#include "WorldDef.h"
#include "World_Items.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <iterator>


namespace
{
	std::atomic<std::uint64_t> gNextTransferIdentity{ 1 };
	OS0ItemTransferRuntime gItemTransferRuntime;

	std::uint64_t IssueTransferIdentity() noexcept
	{
		std::uint64_t identity =
			gNextTransferIdentity.fetch_add(1, std::memory_order_relaxed);
		// Zero is reserved for "no identity".  This also makes the practically
		// unreachable wrap-around deterministic rather than ambiguous.
		if (identity == 0)
			identity = gNextTransferIdentity.fetch_add(1,
				std::memory_order_relaxed);
		return identity;
	}

	bool HasNativeItemPointer() noexcept
	{
		return gpItemPointer && gpItemPointer->usItem != NOTHING &&
			gpItemPointer->ubNumberOfObjects > 0;
	}

	bool ValidSpatialOrigin(std::int32_t const gridNo,
		std::int8_t const level, std::int8_t const visibility) noexcept
	{
		return gridNo >= 0 && gridNo < WORLD_MAX &&
			level >= 0 && level <= 1 &&
			visibility >= HIDDEN_ITEM && visibility <= VISIBLE;
	}

	SOLDIERTYPE* ResolveActor(OS0ItemTransferOrigin const& origin) noexcept
	{
		if (origin.actorId >= TOTAL_SOLDIERS) return nullptr;
		SOLDIERTYPE* const actor = &GetMan(origin.actorId);
		if (!actor->bActive ||
			actor->uiUniqueSoldierIdValue != origin.actorInstanceId)
			return nullptr;
		return actor;
	}

	STRUCTURE const* ResolveContainerBase(std::int32_t const gridNo,
		std::int8_t const level, std::uint16_t const tileIndex) noexcept
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX || level < 0 || level > 1 ||
			tileIndex >= NUMBEROFTILES) return nullptr;
		MAP_ELEMENT const& mapElement = gpWorldLevelData[gridNo];
		LEVELNODE const* node = level == 0 ? mapElement.pStructHead :
			mapElement.pOnRoofHead;
		for (; node; node = node->pNext)
		{
			if (node->usIndex != tileIndex || !node->pStructureData) continue;
			STRUCTURE const* const base = FindBaseStructure(node->pStructureData);
			if (base && base->fFlags & STRUCTURE_OPENABLE &&
				!(base->fFlags & STRUCTURE_ANYDOOR)) return base;
		}
		return nullptr;
	}
}


bool OS0SameObjectRepresentation(OBJECTTYPE const& lhs,
	OBJECTTYPE const& rhs) noexcept
{
	// OBJECTTYPE has one alignment padding byte before its union.  Comparing
	// the whole struct would make an unchanged slot fail nondeterministically
	// when that irrelevant byte differs.  Compare the complete union payload
	// and every persisted field, but deliberately ignore structural padding.
	constexpr size_t payloadSize = offsetof(OBJECTTYPE, usAttachItem) -
		offsetof(OBJECTTYPE, bStatus);
	auto const* const lhsBytes =
		reinterpret_cast<unsigned char const*>(&lhs);
	auto const* const rhsBytes =
		reinterpret_cast<unsigned char const*>(&rhs);
	return lhs.usItem == rhs.usItem &&
		lhs.ubNumberOfObjects == rhs.ubNumberOfObjects &&
		std::memcmp(lhsBytes + offsetof(OBJECTTYPE, bStatus),
			rhsBytes + offsetof(OBJECTTYPE, bStatus), payloadSize) == 0 &&
		std::equal(std::begin(lhs.usAttachItem),
			std::end(lhs.usAttachItem), std::begin(rhs.usAttachItem)) &&
		std::equal(std::begin(lhs.bAttachStatus),
			std::end(lhs.bAttachStatus), std::begin(rhs.bAttachStatus)) &&
		lhs.fFlags == rhs.fFlags && lhs.ubMission == rhs.ubMission &&
		lhs.bTrap == rhs.bTrap && lhs.ubImprintID == rhs.ubImprintID &&
		lhs.ubWeight == rhs.ubWeight && lhs.fUsed == rhs.fUsed;
}


bool OS0ItemTransferRuntime::beginInventory(
	std::uint16_t const actorId, std::uint64_t const actorInstanceId,
	std::int16_t const slot) noexcept
{
	if (!HasNativeItemPointer() || actorId >= TOTAL_SOLDIERS ||
		actorInstanceId == 0 || slot < 0 || slot >= NUM_INV_SLOTS)
		return false;

	SOLDIERTYPE* const actor = &GetMan(actorId);
	if (!actor->bActive || actor->uiUniqueSoldierIdValue != actorInstanceId ||
		gpItemPointerSoldier != actor || gbItemPointerSrcSlot != slot)
		return false;

	OS0ItemTransferOrigin const origin = OS0ItemTransferOrigin::Inventory(
		actorId, actorInstanceId, slot, {});
	return bindAfterDetach(origin);
}


bool OS0ItemTransferRuntime::beginWorld(std::int32_t const gridNo,
	std::int8_t const level, std::int8_t const bVisible,
	std::uint16_t const usFlags,
	std::int8_t const bRenderZHeightAboveLevel) noexcept
{
	return beginSpatial(OS0ItemTransferOriginKind::WORLD, gridNo, level,
		UINT16_MAX, bVisible, usFlags, bRenderZHeightAboveLevel);
}


bool OS0ItemTransferRuntime::beginContainer(std::int32_t const gridNo,
	std::int8_t const level, std::uint16_t const containerTileIndex,
	std::int8_t const bVisible, std::uint16_t const usFlags,
	std::int8_t const bRenderZHeightAboveLevel) noexcept
{
	return beginSpatial(OS0ItemTransferOriginKind::CONTAINER, gridNo, level,
		containerTileIndex, bVisible, usFlags, bRenderZHeightAboveLevel);
}


bool OS0ItemTransferRuntime::beginSpatial(
	OS0ItemTransferOriginKind const kind, std::int32_t const gridNo,
	std::int8_t const level, std::uint16_t const containerTileIndex,
	std::int8_t const bVisible, std::uint16_t const usFlags,
	std::int8_t const bRenderZHeightAboveLevel) noexcept
{
	if (!HasNativeItemPointer() ||
		(kind != OS0ItemTransferOriginKind::WORLD &&
			kind != OS0ItemTransferOriginKind::CONTAINER) ||
		!ValidSpatialOrigin(gridNo, level, bVisible) ||
		(kind == OS0ItemTransferOriginKind::CONTAINER &&
			containerTileIndex >= NUMBEROFTILES))
		return false;

	STRUCTURE const* const container =
		kind == OS0ItemTransferOriginKind::CONTAINER ?
			ResolveContainerBase(gridNo, level, containerTileIndex) : nullptr;
	OS0ItemTransferOrigin const origin =
		kind == OS0ItemTransferOriginKind::WORLD ?
		OS0ItemTransferOrigin::World(gridNo, level, bVisible, usFlags,
			bRenderZHeightAboveLevel) :
		OS0ItemTransferOrigin::Container(gridNo, level, containerTileIndex,
			bVisible, usFlags, bRenderZHeightAboveLevel, {},
			container ? container->usStructureID : UINT16_MAX,
			container ? StructureBaseGridNo(container) : -1);
	return bindAfterDetach(origin);
}


bool OS0ItemTransferRuntime::bindAfterDetach(
	OS0ItemTransferOrigin origin) noexcept
{
	if (!HasNativeItemPointer()) return false;

	// The origin's spatial/actor metadata may be captured before detachment,
	// but identity is deliberately bound to the complete native object only
	// after JA2 has installed it on the cursor.
	origin.item = { gpItemPointer->usItem, IssueTransferIdentity() };
	if (!origin.valid()) return false;

	if (origin.kind == OS0ItemTransferOriginKind::INVENTORY)
	{
		SOLDIERTYPE* const actor = ResolveActor(origin);
		if (!actor || origin.slot < 0 || origin.slot >= NUM_INV_SLOTS)
			return false;
	}
	else
	{
		if (!ValidSpatialOrigin(origin.gridNo, origin.level,
			origin.bVisible)) return false;
		if (origin.kind == OS0ItemTransferOriginKind::CONTAINER &&
			origin.containerTileIndex >= NUMBEROFTILES) return false;
	}

	if (!transaction_.begin(origin)) return false;
	nativePointerCarrier_ = gpItemPointer;
	heldItemAfterDetach_ = *gpItemPointer;
	hasHeldItemSnapshot_ = true;
	inventorySlotAfterDetach_ = {};
	hasInventorySlotSnapshot_ = false;
	if (origin.kind == OS0ItemTransferOriginKind::INVENTORY)
	{
		// BeginItemPointer may detach a single element from a stack.  The
		// remaining stack is still the exact source and must also be stable.
		SOLDIERTYPE* const actor = ResolveActor(origin);
		inventorySlotAfterDetach_ = actor->inv[origin.slot];
		hasInventorySlotSnapshot_ = true;
	}
	return true;
}


bool OS0ItemTransferRuntime::commit() noexcept
{
	if (!transaction_.commit()) return false;
	clearBinding();
	return true;
}


OS0ItemTransferCancelResult OS0ItemTransferRuntime::cancel() noexcept
{
	if (!transaction_.held() || !transaction_.hasOrigin())
		return OS0ItemTransferCancelResult::NO_ACTIVE_TRANSFER;
	if (!gpItemPointer)
		return OS0ItemTransferCancelResult::NATIVE_ITEM_MISSING;
	if (!nativePointerStillBound())
		return OS0ItemTransferCancelResult::NATIVE_ITEM_CHANGED;
	if (!transaction_.cancel())
		return OS0ItemTransferCancelResult::NO_ACTIVE_TRANSFER;

	OS0ItemRestorationDecision const decision =
		transaction_.restorationDecision();
	bool restored = false;
	if (decision.kind == OS0ItemRestorationKind::INVENTORY_SLOT)
		restored = restoreInventory(decision.origin);
	else if (decision.kind == OS0ItemRestorationKind::WORLD_LOCATION ||
		decision.kind == OS0ItemRestorationKind::CONTAINER_LOCATION)
		restored = restoreSpatial(decision.origin);

	if (!restored)
	{
		transaction_.resumeAfterFailedRestoration();
		return OS0ItemTransferCancelResult::RESTORE_FAILED_ITEM_HELD;
	}

	transaction_.acknowledgeRestored();
	clearBinding();
	EndItemPointer();
	return OS0ItemTransferCancelResult::RESTORED;
}


void OS0ItemTransferRuntime::reset() noexcept
{
	transaction_.reset();
	clearBinding();
}


bool OS0ItemTransferRuntime::nativePointerStillBound() const noexcept
{
	OS0TransferredItemIdentity const item = transaction_.origin().item;
	return item.valid() && hasHeldItemSnapshot_ && HasNativeItemPointer() &&
		gpItemPointer == nativePointerCarrier_ &&
		gpItemPointer->usItem == item.itemType &&
		OS0SameObjectRepresentation(*gpItemPointer, heldItemAfterDetach_);
}


bool OS0ItemTransferRuntime::restoreInventory(
	OS0ItemTransferOrigin const& origin) noexcept
{
	SOLDIERTYPE* const actor = ResolveActor(origin);
	if (!actor || origin.slot < 0 || origin.slot >= NUM_INV_SLOTS ||
		!hasInventorySlotSnapshot_ ||
		!OS0SameObjectRepresentation(actor->inv[origin.slot],
			inventorySlotAfterDetach_))
		return false;

	INT8 const slot = static_cast<INT8>(origin.slot);
	OBJECTTYPE& target = actor->inv[slot];
	if (!CanItemFitInPosition(actor, gpItemPointer, slot, FALSE)) return false;

	UINT8 const slotLimit = std::max<UINT8>(ItemSlotLimit(
		gpItemPointer->usItem, slot), 1);
	if (target.usItem == NOTHING)
	{
		if (gpItemPointer->ubNumberOfObjects > slotLimit) return false;
	}
	else
	{
		if (target.usItem != gpItemPointer->usItem ||
			target.ubNumberOfObjects + gpItemPointer->ubNumberOfObjects >
				slotLimit)
			return false;
		ItemModel const* const item = GCM->getItem(target.usItem);
		if (item->isKey() && target.ubKeyID != gpItemPointer->ubKeyID)
			return false;
	}

	// PlaceObject moves the off-hand object to the cursor when a two-handed
	// weapon is put in an empty main hand.  That would no longer be an exact
	// restoration, so reject it before the engine can mutate either slot.
	if (slot == HANDPOS &&
		GCM->getItem(gpItemPointer->usItem)->isTwoHanded() &&
		actor->inv[SECONDHANDPOS].usItem != NOTHING)
		return false;

	OBJECTTYPE const cursorBefore = *gpItemPointer;
	OBJECTTYPE const targetBefore = target;
	if (!PlaceObject(actor, slot, gpItemPointer)) return false;
	if (gpItemPointer->usItem == NOTHING ||
		gpItemPointer->ubNumberOfObjects == 0)
		return true;

	// All capacity checks above are intentionally conservative.  If an engine
	// edge case still performs a partial placement, roll back both values and
	// retain the complete item on the cursor.
	target = targetBefore;
	*gpItemPointer = cursorBefore;
	return false;
}


bool OS0ItemTransferRuntime::restoreSpatial(
	OS0ItemTransferOrigin const& origin) noexcept
{
	if (!ValidSpatialOrigin(origin.gridNo, origin.level, origin.bVisible))
		return false;
	if (origin.kind == OS0ItemTransferOriginKind::CONTAINER &&
		!containerIdentityStillValid(origin)) return false;

	// AddItemToPool may normalize switch metadata.  Work on a copy so every
	// failure path leaves the native cursor byte-for-byte untouched.
	OBJECTTYPE object = heldItemAfterDetach_;
	INT32 const itemIndex = AddItemToPool(
		static_cast<INT16>(origin.gridNo), &object,
		static_cast<Visibility>(origin.bVisible),
		static_cast<UINT8>(origin.level), origin.usFlags,
		origin.bRenderZHeightAboveLevel);
	if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
		return false;

	WORLDITEM& worldItem = GetWorldItem(itemIndex);
	bool const exact = worldItem.fExists &&
		worldItem.sGridNo == origin.gridNo &&
		worldItem.ubLevel == origin.level &&
		worldItem.bVisible == origin.bVisible &&
		worldItem.usFlags == origin.usFlags &&
		worldItem.bRenderZHeightAboveLevel ==
			origin.bRenderZHeightAboveLevel &&
		OS0SameObjectRepresentation(worldItem.o, heldItemAfterDetach_);
	if (!exact)
	{
		if (worldItem.fExists) RemoveItemFromPool(worldItem);
		return false;
	}
	return true;
}


bool OS0ItemTransferRuntime::containerIdentityStillValid(
	OS0ItemTransferOrigin const& origin) const noexcept
{
	if (origin.kind != OS0ItemTransferOriginKind::CONTAINER ||
		origin.gridNo < 0 || origin.gridNo >= WORLD_MAX ||
		origin.level < 0 || origin.level > 1 ||
		origin.containerTileIndex >= NUMBEROFTILES ||
		origin.containerStructureId == 0 ||
		origin.containerBaseGridNo < 0) return false;

	STRUCTURE const* const base = ResolveContainerBase(origin.gridNo,
		origin.level, origin.containerTileIndex);
	return base && base->usStructureID == origin.containerStructureId &&
		StructureBaseGridNo(base) == origin.containerBaseGridNo;
}


void OS0ItemTransferRuntime::clearBinding() noexcept
{
	nativePointerCarrier_ = nullptr;
	heldItemAfterDetach_ = {};
	hasHeldItemSnapshot_ = false;
	inventorySlotAfterDetach_ = {};
	hasInventorySlotSnapshot_ = false;
}


OS0ItemTransferRuntime& OS0GetItemTransferRuntime()
{
	return gItemTransferRuntime;
}
