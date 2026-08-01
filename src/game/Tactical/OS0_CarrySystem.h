#pragma once

#include "Isometric_Utils.h"
#include "Soldier_Control.h"
#include "WorldDef.h"

#include <cstdint>

enum class OS0CarryPhase : UINT8
{
	IDLE,
	TARGETING,
	WALKING
};

enum class OS0CarryMode : UINT8
{
	GRAB,
	CARRY,
	PUSH,
	PULL,
	THROW
};

enum class OS0CarryCancelReason : UINT8
{
	NONE,
	CARRIER_UNAVAILABLE,
	CARRIER_CHANGED,
	CARRIER_DEAD,
	SECTOR_CHANGED,
	LEVEL_CHANGED,
	CARRIER_OUT_OF_REACH,
	LOAD_CHANGED,
	OBJECT_CHANGED,
	PATH_FAILED
};

inline const char* OS0CarryCancelReasonName(
	OS0CarryCancelReason reason) noexcept
{
	switch (reason)
	{
		case OS0CarryCancelReason::NONE: return "NONE";
		case OS0CarryCancelReason::CARRIER_UNAVAILABLE:
			return "CARRIER UNAVAILABLE";
		case OS0CarryCancelReason::CARRIER_CHANGED: return "CARRIER CHANGED";
		case OS0CarryCancelReason::CARRIER_DEAD: return "CARRIER DEAD";
		case OS0CarryCancelReason::SECTOR_CHANGED: return "SECTOR CHANGED";
		case OS0CarryCancelReason::LEVEL_CHANGED: return "LEVEL CHANGED";
		case OS0CarryCancelReason::CARRIER_OUT_OF_REACH: return "OUT OF REACH";
		case OS0CarryCancelReason::LOAD_CHANGED: return "LOAD CHANGED";
		case OS0CarryCancelReason::OBJECT_CHANGED: return "OBJECT CHANGED";
		case OS0CarryCancelReason::PATH_FAILED: return "PATH FAILED";
	}
	return "UNKNOWN";
}

struct OS0CarryContinuationFacts
{
	BOOLEAN carrierAvailable = TRUE;
	BOOLEAN carrierIdentityMatches = TRUE;
	BOOLEAN carrierAlive = TRUE;
	BOOLEAN sameSector = TRUE;
	BOOLEAN sameLevel = TRUE;
	BOOLEAN carrierInReach = TRUE;
	BOOLEAN carrierCanManipulate = TRUE;
	BOOLEAN objectAvailable = TRUE;
	BOOLEAN objectIdentityMatches = TRUE;
	BOOLEAN pathValid = TRUE;
};

struct OS0CarryState
{
	OS0CarryPhase phase = OS0CarryPhase::IDLE;
	OS0CarryMode mode = OS0CarryMode::CARRY;
	GridNo source = NOWHERE;
	GridNo destination = NOWHERE;
	GridNo actionGrid = NOWHERE;
	// After a successful push the structure is already committed one cell ahead,
	// while the carrier still has to occupy the vacated source.  This route is
	// owned by the same grab and must finish before another physical step starts.
	GridNo followUpGrid = NOWHERE;
	UINT8 sourceLevel = 0;
	UINT8 destinationLevel = 0;
	UINT16 tileIndex = NO_TILE;
	// Positional handles are recyclable. A carry owns both the live soldier
	// incarnation and the canonical base structure for its entire lifecycle.
	SoldierID carrier = NOBODY;
	UINT32 carrierInstanceId = 0;
	UINT16 structureId = 0;
	GridNo structureBaseGridNo = NOWHERE;
	// Exact buddy-shadow node owned by the bound source structure.  Shadows have
	// no native back-pointer, so positional matching alone can hide a neighbour.
	std::uintptr_t shadowInstance = 0;
	BOOLEAN lifted = FALSE;
	// A persistent grab binds carrier and object across multiple movement steps.
	// mode records the physical step currently inferred (push/pull/carry).
	BOOLEAN persistentGrab = FALSE;
	// Direct pointer drags are one physical gesture.  An invalid release cancels
	// them instead of leaving an invisible TARGETING latch on the cursor.
	BOOLEAN pointerDrag = FALSE;

	BOOLEAN active() const noexcept { return phase != OS0CarryPhase::IDLE; }
	BOOLEAN pending() const noexcept { return phase == OS0CarryPhase::TARGETING; }
	BOOLEAN walking() const noexcept { return phase == OS0CarryPhase::WALKING; }
	BOOLEAN repositioning() const noexcept
	{
		return pending() && followUpGrid >= 0 && followUpGrid < WORLD_MAX;
	}

	BOOLEAN begin(GridNo newSource, UINT8 newLevel, UINT16 newTile,
		SoldierID newCarrier, UINT32 newCarrierInstanceId,
		UINT16 newStructureId, GridNo newStructureBaseGridNo,
		OS0CarryMode newMode = OS0CarryMode::CARRY,
		BOOLEAN newPointerDrag = FALSE) noexcept
	{
		if (newSource < 0 || newSource >= WORLD_MAX || newLevel > 1 ||
			newTile >= NUMBEROFTILES || newCarrier == NOBODY ||
			newCarrierInstanceId == 0 || newStructureId == 0 ||
			newStructureBaseGridNo < 0 ||
			newStructureBaseGridNo >= WORLD_MAX)
		{
			reset();
			return FALSE;
		}
		reset();
		phase = OS0CarryPhase::TARGETING;
		mode = newMode;
		persistentGrab = newMode == OS0CarryMode::GRAB;
		pointerDrag = newPointerDrag;
		source = newSource;
		sourceLevel = newLevel;
		tileIndex = newTile;
		carrier = newCarrier;
		carrierInstanceId = newCarrierInstanceId;
		structureId = newStructureId;
		structureBaseGridNo = newStructureBaseGridNo;
		return TRUE;
	}

	BOOLEAN boundToCarrier(SoldierID observedCarrier,
		UINT32 observedInstanceId) const noexcept
	{
		return active() && carrier == observedCarrier && carrierInstanceId != 0 &&
			carrierInstanceId == observedInstanceId;
	}

	BOOLEAN boundToStructure(UINT16 observedStructureId,
		GridNo observedBaseGridNo) const noexcept
	{
		return active() && structureId != 0 && structureBaseGridNo >= 0 &&
			structureBaseGridNo < WORLD_MAX && structureId == observedStructureId &&
			structureBaseGridNo == observedBaseGridNo;
	}

	BOOLEAN beginWalk(GridNo newDestination, UINT8 newLevel,
		GridNo newActionGrid, UINT32 observedCarrierInstanceId,
		UINT16 observedStructureId, GridNo observedStructureBaseGridNo) noexcept
	{
		if (!pending() || newDestination < 0 || newDestination >= WORLD_MAX ||
			newDestination == source || newLevel > 1 || newActionGrid < 0 ||
			newActionGrid >= WORLD_MAX ||
			carrierInstanceId != observedCarrierInstanceId ||
			!boundToStructure(observedStructureId, observedStructureBaseGridNo))
			return FALSE;
		destination = newDestination;
		destinationLevel = newLevel;
		actionGrid = newActionGrid;
		phase = OS0CarryPhase::WALKING;
		return TRUE;
	}

	void reset() noexcept
	{
		*this = OS0CarryState{};
	}
};

inline OS0CarryCancelReason OS0ValidateCarryContinuation(
	OS0CarryState const& state, OS0CarryContinuationFacts const& facts) noexcept
{
	if (!state.active()) return OS0CarryCancelReason::NONE;
	if (!facts.carrierAvailable) return OS0CarryCancelReason::CARRIER_UNAVAILABLE;
	if (state.carrierInstanceId == 0 || !facts.carrierIdentityMatches)
		return OS0CarryCancelReason::CARRIER_CHANGED;
	if (!facts.carrierAlive) return OS0CarryCancelReason::CARRIER_DEAD;
	if (!facts.sameSector) return OS0CarryCancelReason::SECTOR_CHANGED;
	if (!facts.sameLevel) return OS0CarryCancelReason::LEVEL_CHANGED;
	if (!facts.carrierInReach)
		return OS0CarryCancelReason::CARRIER_OUT_OF_REACH;
	if (!facts.carrierCanManipulate)
		return OS0CarryCancelReason::LOAD_CHANGED;
	if (!facts.objectAvailable) return OS0CarryCancelReason::OBJECT_CHANGED;
	if (state.structureBaseGridNo < 0 ||
		state.structureBaseGridNo >= WORLD_MAX || !facts.objectIdentityMatches)
		return OS0CarryCancelReason::OBJECT_CHANGED;
	if ((state.walking() || state.repositioning()) && !facts.pathValid)
		return OS0CarryCancelReason::PATH_FAILED;
	return OS0CarryCancelReason::NONE;
}
