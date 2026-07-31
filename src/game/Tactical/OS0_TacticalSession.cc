#include "OS0_TacticalSession.h"

#include "SaveLoadGameStates.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace
{
	constexpr const char* STATE_VERSION_KEY = "escape-from-arulco:state-version";
	constexpr const char* ASSET_DAMAGE_KEY = "escape-from-arulco:asset-damage-v1";
	constexpr const char* SECTOR_ECONOMY_KEY = "escape-from-arulco:sector-economy-v1";
	constexpr const char* CREATOR_COMPLETE_KEY =
		"escape-from-arulco:creator-complete-v1";
	constexpr const char* FIELD_TUTORIAL_COMPLETE_KEY =
		"escape-from-arulco:field-tutorial-complete-v1";
	constexpr INT32 STATE_VERSION = 4;
	constexpr size_t ASSET_DAMAGE_FIELDS = 8;
	constexpr size_t SECTOR_ECONOMY_FIELDS = 9;

	OS0TacticalSession gSession;
}

void OS0TacticalSession::endTacticalSector()
{
	state_.coverOrders.clear();
	state_.carry.reset();
	// Cursor/control mode is tactical-frame state. Carrying COMBAT into the next
	// sector leaves the fresh viewport in an aim mode it never requested.
	state_.cursor = {};
	state_.pendingVisualEvents.clear();
	state_.pendingDiagnostics.clear();
}

void OS0TacticalSession::resetCampaign()
{
	state_ = OS0TacticalState{};
}

void OS0TacticalSession::storePersistentState(SavedGameStates& states) const
{
	states.Set(STATE_VERSION_KEY, STATE_VERSION);
	states.Set(CREATOR_COMPLETE_KEY, state_.creatorCompleted ? 1 : 0);
	states.Set(FIELD_TUTORIAL_COMPLETE_KEY,
		state_.fieldTutorialCompleted ? 1 : 0);
	std::vector<int32_t> values;
	values.reserve(state_.assetDamage.records().size() * ASSET_DAMAGE_FIELDS);
	for (OS0AssetDamageRecord const& record : state_.assetDamage.records())
	{
		values.push_back(record.key.sectorX);
		values.push_back(record.key.sectorY);
		values.push_back(record.key.sectorZ);
		values.push_back(record.key.gridNo);
		values.push_back(record.key.level);
		values.push_back(record.key.tileIndex);
		values.push_back(record.remaining);
		values.push_back(record.maximum);
	}
	states.SetVector(ASSET_DAMAGE_KEY, std::move(values));

	std::vector<int32_t> economy;
	economy.reserve(state_.sectorEconomy.records().size() * SECTOR_ECONOMY_FIELDS);
	for (OS0SectorEconomyRecord const& record : state_.sectorEconomy.records())
	{
		economy.push_back(record.sector.x);
		economy.push_back(record.sector.y);
		economy.push_back(record.sector.z);
		for (UINT16 resource : record.resources) economy.push_back(resource);
		economy.push_back(static_cast<int32_t>(record.upgrades));
		economy.push_back(record.legacyMigrated ? 1 : 0);
	}
	states.SetVector(SECTOR_ECONOMY_KEY, std::move(economy));
}

void OS0TacticalSession::loadPersistentState(SavedGameStates const& states)
{
	// Save data never owns transient pointers, orders or queued visuals. Clear
	// them before parsing so loading over an active tactical session cannot leak
	// actions from the previous world into the restored one.
	endTacticalSector();
	state_.assetDamage.clear();
	state_.sectorEconomy.clear();
	state_.creatorCompleted = FALSE;
	state_.fieldTutorialCompleted = FALSE;
	if (!states.HasKey(STATE_VERSION_KEY)) return;
	INT32 version = 0;
	try
	{
		version = states.Get<int32_t>(STATE_VERSION_KEY);
		if (version < 1 || version > STATE_VERSION) return;
	}
	catch (...) { return; }
	if (states.HasKey(CREATOR_COMPLETE_KEY))
	{
		try
		{
			state_.creatorCompleted =
				states.Get<int32_t>(CREATOR_COMPLETE_KEY) != 0;
		}
		catch (...) {}
	}
	else if (version < 3)
	{
		// OS//0 v1/v2 saves predate the in-sector creator marker and were made
		// after an operator already existed. Do not force those campaigns through
		// character creation again; only a fresh campaign starts incomplete.
		state_.creatorCompleted = TRUE;
	}
	if (states.HasKey(FIELD_TUTORIAL_COMPLETE_KEY))
	{
		try
		{
			state_.fieldTutorialCompleted =
				states.Get<int32_t>(FIELD_TUTORIAL_COMPLETE_KEY) != 0;
		}
		catch (...) {}
	}

	if (states.HasKey(ASSET_DAMAGE_KEY))
	{
		try
			{
				std::vector<int32_t> const values =
					states.GetVector<int32_t>(ASSET_DAMAGE_KEY);
				if (values.size() % ASSET_DAMAGE_FIELDS != 0)
					throw std::runtime_error("Malformed OS0 asset damage state");
				std::vector<OS0AssetDamageRecord> records;
				for (size_t i = 0; i + ASSET_DAMAGE_FIELDS <= values.size();
					i += ASSET_DAMAGE_FIELDS)
				{
					OS0AssetDamageRecord record{
						{
							static_cast<UINT8>(std::clamp(values[i], 1, 16)),
							static_cast<UINT8>(std::clamp(values[i + 1], 1, 16)),
							static_cast<INT8>(std::clamp(values[i + 2], 0, 3)),
							static_cast<GridNo>(values[i + 3]),
							static_cast<UINT8>(std::clamp(values[i + 4], 0, 1)),
							static_cast<UINT16>(std::clamp(values[i + 5], 0, 65535))
						},
						static_cast<INT16>(std::clamp(values[i + 6], 0, 32767)),
						static_cast<INT16>(std::clamp(values[i + 7], 1, 32767))
					};
					if (record.key.gridNo >= 0 && record.key.gridNo < WORLD_MAX &&
						record.remaining > 0 && record.remaining < record.maximum)
						records.push_back(record);
				}
				state_.assetDamage.replaceRecords(std::move(records));
			}
		catch (...) {}
	}

	if (states.HasKey(SECTOR_ECONOMY_KEY))
	{
		try
		{
			std::vector<int32_t> const values =
				states.GetVector<int32_t>(SECTOR_ECONOMY_KEY);
			if (values.size() % SECTOR_ECONOMY_FIELDS != 0)
				throw std::runtime_error("Malformed OS0 sector economy state");
			std::vector<OS0SectorEconomyRecord> records;
			for (size_t i = 0; i + SECTOR_ECONOMY_FIELDS <= values.size();
				i += SECTOR_ECONOMY_FIELDS)
			{
				OS0SectorEconomyRecord record;
				record.sector = {
					static_cast<UINT8>(std::clamp(values[i], 1, 16)),
					static_cast<UINT8>(std::clamp(values[i + 1], 1, 16)),
					static_cast<INT8>(std::clamp(values[i + 2], 0, 3)) };
				for (size_t resource = 0; resource < record.resources.size(); ++resource)
					record.resources[resource] = static_cast<UINT16>(std::clamp(
						values[i + 3 + resource], 0,
						static_cast<int32_t>(OS0_SECTOR_RESOURCE_MAX)));
				record.upgrades = static_cast<UINT32>(std::max<int32_t>(0,
					values[i + 7]));
				record.legacyMigrated = values[i + 8] != 0;
				records.push_back(record);
			}
			state_.sectorEconomy.replaceRecords(std::move(records));
		}
		catch (...) {}
	}
}

OS0TacticalSession& OS0GetTacticalSession()
{
	return gSession;
}

void OS0StorePersistentState(SavedGameStates& states)
{
	gSession.storePersistentState(states);
}

void OS0LoadPersistentState(SavedGameStates const& states)
{
	gSession.loadPersistentState(states);
}

void OS0ResetCampaignState()
{
	gSession.resetCampaign();
}
