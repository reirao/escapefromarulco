#pragma once

#include "OS0_AssetDamageSystem.h"
#include "OS0_ActionRegistry.h"
#include "OS0_CarrySystem.h"
#include "OS0_CoverOrderSystem.h"
#include "OS0_SectorEconomySystem.h"
#include "OS0_WorldTypes.h"

#include <string_theory/string>
#include <vector>

class SavedGameStates;

struct OS0ImpactVisualEvent
{
	GridNo gridNo;
	OS0AssetMaterial material;
	UINT8 intensity;
};

struct OS0CursorState
{
	ContextAction action = ContextAction::MOVE;
};

struct OS0TacticalState
{
	BOOLEAN creatorCompleted = FALSE;
	BOOLEAN fieldTutorialCompleted = FALSE;
	OS0AssetDamageSystem assetDamage;
	OS0CoverOrderSystem coverOrders;
	OS0CarryState carry;
	OS0CursorState cursor;
	OS0SectorEconomySystem sectorEconomy;
	std::vector<OS0AssetCatalogRecord> assetCatalog;
	std::vector<OS0ImpactVisualEvent> pendingVisualEvents;
	std::vector<ST::string> pendingDiagnostics;
};

class OS0TacticalSession
{
public:
	OS0TacticalState& state() noexcept { return state_; }
	OS0TacticalState const& state() const noexcept { return state_; }

	void endTacticalSector();
	void resetCampaign();
	void storePersistentState(SavedGameStates& states) const;
	void loadPersistentState(SavedGameStates const& states);

private:
	OS0TacticalState state_;
};

OS0TacticalSession& OS0GetTacticalSession();
void OS0StorePersistentState(SavedGameStates& states);
void OS0LoadPersistentState(SavedGameStates const& states);
void OS0ResetCampaignState();
