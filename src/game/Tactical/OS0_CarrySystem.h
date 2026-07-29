#pragma once

#include "Isometric_Utils.h"
#include "Soldier_Control.h"
#include "WorldDef.h"

enum class OS0CarryPhase : UINT8
{
	IDLE,
	TARGETING,
	WALKING
};

enum class OS0CarryMode : UINT8
{
	CARRY,
	PUSH,
	PULL,
	THROW
};

enum class OS0CarryCancelReason : UINT8
{
	NONE,
	CARRIER_UNAVAILABLE,
	CARRIER_DEAD,
	SECTOR_CHANGED,
	LEVEL_CHANGED,
	PATH_FAILED
};

struct OS0CarryContinuationFacts
{
	BOOLEAN carrierAvailable = TRUE;
	BOOLEAN carrierAlive = TRUE;
	BOOLEAN sameSector = TRUE;
	BOOLEAN sameLevel = TRUE;
	BOOLEAN pathValid = TRUE;
};

struct OS0CarryState
{
	OS0CarryPhase phase = OS0CarryPhase::IDLE;
	OS0CarryMode mode = OS0CarryMode::CARRY;
	GridNo source = NOWHERE;
	GridNo destination = NOWHERE;
	GridNo actionGrid = NOWHERE;
	UINT8 sourceLevel = 0;
	UINT8 destinationLevel = 0;
	UINT16 tileIndex = NO_TILE;
	SoldierID carrier = NOBODY;
	UINT8 oldShade = DEFAULT_SHADE_LEVEL;
	BOOLEAN sourceShaded = FALSE;
	BOOLEAN lifted = FALSE;

	BOOLEAN active() const noexcept { return phase != OS0CarryPhase::IDLE; }
	BOOLEAN pending() const noexcept { return phase == OS0CarryPhase::TARGETING; }
	BOOLEAN walking() const noexcept { return phase == OS0CarryPhase::WALKING; }

	BOOLEAN begin(GridNo newSource, UINT8 newLevel, UINT16 newTile,
		SoldierID newCarrier, OS0CarryMode newMode = OS0CarryMode::CARRY) noexcept
	{
		if (newSource < 0 || newSource >= WORLD_MAX || newLevel > 1 ||
			newTile >= NUMBEROFTILES || newCarrier == NOBODY)
		{
			reset();
			return FALSE;
		}
		reset();
		phase = OS0CarryPhase::TARGETING;
		mode = newMode;
		source = newSource;
		sourceLevel = newLevel;
		tileIndex = newTile;
		carrier = newCarrier;
		return TRUE;
	}

	BOOLEAN beginWalk(GridNo newDestination, UINT8 newLevel,
		GridNo newActionGrid) noexcept
	{
		if (!pending() || newDestination < 0 || newDestination >= WORLD_MAX ||
			newDestination == source || newLevel > 1 || newActionGrid < 0 ||
			newActionGrid >= WORLD_MAX)
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
	if (!facts.carrierAlive) return OS0CarryCancelReason::CARRIER_DEAD;
	if (!facts.sameSector) return OS0CarryCancelReason::SECTOR_CHANGED;
	if (!facts.sameLevel) return OS0CarryCancelReason::LEVEL_CHANGED;
	if (state.walking() && !facts.pathValid)
		return OS0CarryCancelReason::PATH_FAILED;
	return OS0CarryCancelReason::NONE;
}
