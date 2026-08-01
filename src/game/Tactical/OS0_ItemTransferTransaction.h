#pragma once

#include <cstdint>


// Stable identity supplied by the tactical integration.  instanceId must be a
// session-unique value which follows the held object; it must never be a
// WORLDITEM vector index, an address or another position-dependent handle.
struct OS0TransferredItemIdentity
{
	std::uint16_t itemType = UINT16_MAX;
	std::uint64_t instanceId = 0;

	constexpr bool valid() const noexcept
	{
		return itemType != UINT16_MAX && instanceId != 0;
	}

	bool operator==(OS0TransferredItemIdentity const&) const noexcept = default;
};


enum class OS0ItemTransferOriginKind : std::uint8_t
{
	NONE,
	INVENTORY,
	WORLD,
	CONTAINER
};


// Value-only source snapshot.  It deliberately contains no engine pointers or
// mutable pool indices, so it remains meaningful while windows close, vectors
// move and the cursor crosses UI surfaces.
struct OS0ItemTransferOrigin
{
	OS0ItemTransferOriginKind kind = OS0ItemTransferOriginKind::NONE;
	OS0TransferredItemIdentity item;

	// INVENTORY source identity.
	std::uint16_t actorId = UINT16_MAX;
	std::uint64_t actorInstanceId = 0;
	std::int16_t slot = -1;

	// WORLD and CONTAINER source identity.  The three pool-rendering values
	// mirror WORLDITEM exactly; cancel must never silently substitute the
	// defaults used by AddItemToPool.
	std::int32_t gridNo = -1;
	std::int8_t level = -1;
	std::int8_t bVisible = 0;
	std::uint16_t usFlags = 0;
	std::int8_t bRenderZHeightAboveLevel = 0;

	// A container is an asset, not merely an item-pool coordinate.  Retaining
	// the map structure identity lets the engine adapter fail closed if that
	// asset was removed or replaced while its item was held.  usStructureID is
	// monotonically issued by the structure engine for the current session;
	// baseGridNo disambiguates every tile of a multi-tile asset.
	std::uint16_t containerTileIndex = UINT16_MAX;
	std::uint16_t containerStructureId = 0;
	std::int32_t containerBaseGridNo = -1;

	static constexpr OS0ItemTransferOrigin Inventory(std::uint16_t actor,
		std::uint64_t actorInstance, std::int16_t inventorySlot,
		OS0TransferredItemIdentity identity) noexcept
	{
		OS0ItemTransferOrigin source;
		source.kind = OS0ItemTransferOriginKind::INVENTORY;
		source.item = identity;
		source.actorId = actor;
		source.actorInstanceId = actorInstance;
		source.slot = inventorySlot;
		return source;
	}

	static constexpr OS0ItemTransferOrigin World(std::int32_t sourceGridNo,
		std::int8_t sourceLevel, std::int8_t sourceVisibility,
		std::uint16_t sourceFlags, std::int8_t sourceRenderZHeight,
		OS0TransferredItemIdentity identity = {}) noexcept
	{
		return Spatial(OS0ItemTransferOriginKind::WORLD, sourceGridNo,
			sourceLevel, sourceVisibility, sourceFlags, sourceRenderZHeight,
			UINT16_MAX, identity);
	}

	static constexpr OS0ItemTransferOrigin Container(
		std::int32_t sourceGridNo, std::int8_t sourceLevel,
		std::uint16_t sourceTileIndex, std::int8_t sourceVisibility,
		std::uint16_t sourceFlags, std::int8_t sourceRenderZHeight,
		OS0TransferredItemIdentity identity = {},
		std::uint16_t sourceStructureId = 0,
		std::int32_t sourceBaseGridNo = -1) noexcept
	{
		return Spatial(OS0ItemTransferOriginKind::CONTAINER, sourceGridNo,
			sourceLevel, sourceVisibility, sourceFlags, sourceRenderZHeight,
			sourceTileIndex, identity, sourceStructureId, sourceBaseGridNo);
	}

	constexpr bool valid() const noexcept
	{
		if (!item.valid()) return false;
		switch (kind)
		{
			case OS0ItemTransferOriginKind::INVENTORY:
				return actorId != UINT16_MAX && actorInstanceId != 0 &&
					slot >= 0;
			case OS0ItemTransferOriginKind::WORLD:
				return gridNo >= 0 && level >= 0 && level <= 1;
			case OS0ItemTransferOriginKind::CONTAINER:
				return gridNo >= 0 && level >= 0 && level <= 1 &&
					containerTileIndex != UINT16_MAX &&
					containerStructureId != 0 &&
					containerBaseGridNo >= 0;
			case OS0ItemTransferOriginKind::NONE:
				return false;
		}
		return false;
	}

	bool operator==(OS0ItemTransferOrigin const&) const noexcept = default;

private:
	static constexpr OS0ItemTransferOrigin Spatial(
		OS0ItemTransferOriginKind sourceKind, std::int32_t sourceGridNo,
		std::int8_t sourceLevel, std::int8_t sourceVisibility,
		std::uint16_t sourceFlags, std::int8_t sourceRenderZHeight,
		std::uint16_t sourceContainerTileIndex,
		OS0TransferredItemIdentity identity,
		std::uint16_t sourceStructureId = 0,
		std::int32_t sourceBaseGridNo = -1) noexcept
	{
		OS0ItemTransferOrigin source;
		source.kind = sourceKind;
		source.item = identity;
		source.gridNo = sourceGridNo;
		source.level = sourceLevel;
		source.bVisible = sourceVisibility;
		source.usFlags = sourceFlags;
		source.bRenderZHeightAboveLevel = sourceRenderZHeight;
		source.containerTileIndex = sourceContainerTileIndex;
		source.containerStructureId = sourceStructureId;
		source.containerBaseGridNo = sourceBaseGridNo;
		return source;
	}
};


enum class OS0ItemTransactionPhase : std::uint8_t
{
	EMPTY,
	HELD,
	COMMITTED,
	CANCELLED
};


enum class OS0ItemRestorationKind : std::uint8_t
{
	NONE,
	INVENTORY_SLOT,
	WORLD_LOCATION,
	CONTAINER_LOCATION
};


struct OS0ItemRestorationDecision
{
	OS0ItemRestorationKind kind = OS0ItemRestorationKind::NONE;
	OS0ItemTransferOrigin origin;

	constexpr bool required() const noexcept
	{
		return kind != OS0ItemRestorationKind::NONE && origin.valid();
	}
};


// Pure transaction state for one item already being detached/held.
//
// OS0_IngameUI integration contract:
//  1. Before mutating a source, build and begin() its stable origin.
//  2. Let OS0ItemTransferController continue to own only the mouse gesture.
//  3. commit() only after a destination has accepted the complete transfer.
//  4. On cancel, use restorationDecision() verbatim.  Never auto-place into a
//     different slot or grid.  acknowledgeRestored() after exact restoration;
//     if it cannot be restored, resumeAfterFailedRestoration() and leave the
//     object held on the cursor.
//  5. reset() at the start of a genuinely new transfer.
class OS0ItemTransferTransaction
{
public:
	bool begin(OS0ItemTransferOrigin origin) noexcept;
	bool commit() noexcept;
	bool cancel() noexcept;

	OS0ItemRestorationDecision restorationDecision() const noexcept;
	bool acknowledgeRestored() noexcept;
	bool resumeAfterFailedRestoration() noexcept;
	void reset() noexcept;

	OS0ItemTransactionPhase phase() const noexcept { return phase_; }
	bool held() const noexcept
	{
		return phase_ == OS0ItemTransactionPhase::HELD;
	}
	bool hasOrigin() const noexcept { return origin_.valid(); }
	OS0ItemTransferOrigin const& origin() const noexcept { return origin_; }

private:
	OS0ItemTransactionPhase phase_ = OS0ItemTransactionPhase::EMPTY;
	OS0ItemTransferOrigin origin_;
};
