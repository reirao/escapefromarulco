#include "OS0_AssetDamageSystem.h"
#include "OS0_AssetCatalogService.h"

#include "Items.h"
#include "ContentManager.h"
#include "GameInstance.h"
#include "ItemModel.h"
#include "OS0_TacticalSession.h"
#include "Physics.h"
#include "RenderWorld.h"
#include "SaveLoadMap.h"
#include "StrategicMap.h"
#include "Structure.h"
#include "World_Items.h"
#include "WorldDef.h"
#include "WorldMan.h"

#include <algorithm>
#include <array>
#include <string_theory/format>

namespace
{
	OS0AssetDamageRecord* FindRecord(std::vector<OS0AssetDamageRecord>& records,
		OS0AssetKey const& key)
	{
		auto const found = std::find_if(records.begin(), records.end(),
			[&](OS0AssetDamageRecord const& value) { return value.key == key; });
		return found == records.end() ? nullptr : &*found;
	}

	OS0AssetDamageRecord const* FindRecord(
		std::vector<OS0AssetDamageRecord> const& records, OS0AssetKey const& key)
	{
		auto const found = std::find_if(records.begin(), records.end(),
			[&](OS0AssetDamageRecord const& value) { return value.key == key; });
		return found == records.end() ? nullptr : &*found;
	}

	LEVELNODE* LevelNodeForStructure(STRUCTURE const* structure, UINT8 level)
	{
		if (!structure || structure->sGridNo < 0 || structure->sGridNo >= WORLD_MAX)
			return nullptr;
		LEVELNODE* node = level == 0 ?
			gpWorldLevelData[structure->sGridNo].pStructHead :
			gpWorldLevelData[structure->sGridNo].pOnRoofHead;
		for (; node; node = node->pNext)
		{
			if (node->pStructureData &&
				FindBaseStructure(node->pStructureData) == structure &&
				node->pStructureData->fFlags & STRUCTURE_BASE_TILE)
				return node;
		}
		return nullptr;
	}

	OS0AssetMaterial InferMaterial(STRUCTURE const* structure)
	{
		if (!structure) return OS0AssetMaterial::AUTO;
		ST::string const material = GetWorldPhysicsProfile(structure).materialName;
		if (material == "WOOD" || material == "FURNITURE") return OS0AssetMaterial::WOOD;
		if (material == "STONE" || material == "CERAMIC") return OS0AssetMaterial::STONE;
		if (material == "LIGHT METAL" || material == "HEAVY METAL") return OS0AssetMaterial::METAL;
		if (material == "SAND") return OS0AssetMaterial::SAND;
		if (material == "ORGANIC") return OS0AssetMaterial::ORGANIC;
		if (material == "CLOTH") return OS0AssetMaterial::FABRIC;
		return OS0AssetMaterial::COMPOSITE;
	}

	OS0ResourceKind ResourceForMaterial(OS0AssetMaterial material)
	{
		if (material == OS0AssetMaterial::WOOD ||
			material == OS0AssetMaterial::ORGANIC ||
			material == OS0AssetMaterial::FABRIC) return OS0ResourceKind::TIMBER;
		if (material == OS0AssetMaterial::STONE ||
			material == OS0AssetMaterial::SAND) return OS0ResourceKind::STONE;
		if (material == OS0AssetMaterial::EARTH) return OS0ResourceKind::SOIL;
		return OS0ResourceKind::SCRAP;
	}

	UINT16 ResourceItem(OS0ResourceKind resource)
	{
		switch (resource)
		{
			case OS0ResourceKind::TIMBER: return OS0_TIMBER;
			case OS0ResourceKind::STONE:  return OS0_STONE;
			case OS0ResourceKind::SCRAP:  return OS0_SCRAP;
			case OS0ResourceKind::SOIL:   return OS0_SOIL;
			case OS0ResourceKind::COUNT:  return NOTHING;
		}
		return NOTHING;
	}

	const char* ResourceName(OS0ResourceKind resource)
	{
		switch (resource)
		{
			case OS0ResourceKind::TIMBER: return "TIMBER";
			case OS0ResourceKind::STONE:  return "STONE";
			case OS0ResourceKind::SCRAP:  return "SCRAP";
			case OS0ResourceKind::SOIL:   return "SOIL";
			case OS0ResourceKind::COUNT:  return "MATERIAL";
		}
		return "MATERIAL";
	}

	struct SalvageYield
	{
		BOOLEAN salvageable;
		OS0ResourceKind resource;
		UINT8 amount;
	};

	SalvageYield ResolveSalvageYield(STRUCTURE const* structure,
		UINT16 canonicalTile, OS0AssetMaterial material)
	{
		if (OS0AssetCatalogRecord const* const catalog =
			OS0FindAssetCatalogRecordConst(static_cast<INT16>(giCurrentTilesetID),
				canonicalTile))
		{
			BOOLEAN const salvageable = catalog->role == OS0AssetRole::SALVAGE ||
				catalog->role == OS0AssetRole::RESOURCE_NODE;
			UINT8 const amount = salvageable ?
				static_cast<UINT8>(std::clamp<INT16>(
					catalog->width * catalog->height, 1, 8)) :
				static_cast<UINT8>(0);
			return { salvageable, ResourceForMaterial(material), amount };
		}

		if (!structure) return { FALSE, OS0ResourceKind::SCRAP, 0 };
		if (structure->fFlags & (STRUCTURE_WALLSTUFF | STRUCTURE_ROOF |
			STRUCTURE_SWITCH | STRUCTURE_VEHICLE | STRUCTURE_LIGHTSOURCE))
			return { FALSE, OS0ResourceKind::SCRAP, 0 };

		BOOLEAN const supported = structure->fFlags & (STRUCTURE_ANYDOOR |
			STRUCTURE_TREE | STRUCTURE_ANYFENCE | STRUCTURE_OPENABLE) ||
			(structure->fFlags & STRUCTURE_BASE_TILE && structure->pDBStructureRef &&
			 structure->pDBStructureRef->pDBStructure->ubNumberOfTiles == 1);
		if (!supported) return { FALSE, OS0ResourceKind::SCRAP, 0 };

		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		return { TRUE, ResourceForMaterial(material),
			static_cast<UINT8>(std::clamp<INT32>(
				static_cast<INT32>(physics.massKg / 20.0f) + 1, 1, 8)) };
	}

	OS0AssetKey MakeAssetKey(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		return { static_cast<UINT8>(gWorldSector.x),
			static_cast<UINT8>(gWorldSector.y), gWorldSector.z,
			gridNo, level, tileIndex };
	}

	void AddResourceItem(GridNo gridNo, UINT8 level, OS0ResourceKind resource,
		UINT8 amount)
	{
		UINT16 const item = ResourceItem(resource);
		if (item == NOTHING || amount == 0) return;
		OBJECTTYPE object{};
		CreateItems(item, 100, std::min<UINT8>(amount, MAX_OBJECTS_PER_SLOT),
			&object);
		AddItemToPool(gridNo, &object, HIDDEN_IN_OBJECT, level, 0, -1);
	}
}

INT16 OS0AssetDamageSystem::durability(OS0AssetKey const& key,
	INT16 maximum) const
{
	OS0AssetDamageRecord const* const record = FindRecord(records_, key);
	return record ? std::max<INT16>(0, record->remaining) : maximum;
}

OS0AssetDamageResult OS0AssetDamageSystem::apply(OS0AssetKey const& key,
	INT16 maximum, INT16 damage)
{
	OS0AssetDamageRecord* record = FindRecord(records_, key);
	if (!record)
	{
		records_.push_back({ key, maximum, maximum });
		record = &records_.back();
	}
	record->maximum = maximum;
	record->remaining = static_cast<INT16>(record->remaining -
		std::max<INT16>(1, damage));
	return { TRUE, record->remaining <= 0,
		std::max<INT16>(0, record->remaining), maximum };
}

void OS0AssetDamageSystem::remove(OS0AssetKey const& key)
{
	records_.erase(std::remove_if(records_.begin(), records_.end(),
		[&](OS0AssetDamageRecord const& value) { return value.key == key; }),
		records_.end());
}

void OS0AssetDamageSystem::move(OS0AssetKey const& source,
	OS0AssetKey const& destination)
{
	if (OS0AssetDamageRecord* const record = FindRecord(records_, source))
		record->key = destination;
}

void OS0AssetDamageSystem::clear()
{
	records_.clear();
}

std::vector<OS0AssetDamageRecord> const&
OS0AssetDamageSystem::records() const noexcept
{
	return records_;
}

void OS0AssetDamageSystem::replaceRecords(
	std::vector<OS0AssetDamageRecord> records)
{
	records_ = std::move(records);
}

INT16 OS0AssetDurabilityMaximum(OS0AssetMaterial material)
{
	switch (material)
	{
		case OS0AssetMaterial::WOOD:
		case OS0AssetMaterial::ORGANIC: return 70;
		case OS0AssetMaterial::STONE:   return 160;
		case OS0AssetMaterial::METAL:   return 210;
		case OS0AssetMaterial::SAND:
		case OS0AssetMaterial::EARTH:   return 110;
		default:                        return 90;
	}
}

INT16 OS0CurrentAssetDurability(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, OS0AssetMaterial material)
{
	return OS0GetTacticalSession().state().assetDamage.durability(
		MakeAssetKey(gridNo, level, tileIndex),
		OS0AssetDurabilityMaximum(material));
}

BOOLEAN OS0ApplyWorldAssetDamage(GridNo gridNo, UINT8 level,
	STRUCTURE* structure, UINT8 impact)
{
	if (!structure || gridNo < 0 || gridNo >= WORLD_MAX) return FALSE;
	STRUCTURE* const base = FindBaseStructure(structure);
	LEVELNODE* const node = LevelNodeForStructure(base, level);
	if (!base || !node || node->usIndex >= NUMBEROFTILES) return FALSE;

	UINT16 const canonicalTile = node->usIndex;
	OS0AssetMaterial material = InferMaterial(base);
	if (OS0AssetCatalogRecord const* const catalog =
		OS0FindAssetCatalogRecordConst(static_cast<INT16>(giCurrentTilesetID),
			canonicalTile))
	{
		if (catalog->material != OS0AssetMaterial::AUTO)
			material = catalog->material;
	}
	SalvageYield const salvage = ResolveSalvageYield(base, canonicalTile,
		material);
	if (!salvage.salvageable) return FALSE;

	OS0TacticalState& state = OS0GetTacticalSession().state();
	OS0AssetKey const key = MakeAssetKey(base->sGridNo, level, canonicalTile);
	INT16 const maximum = OS0AssetDurabilityMaximum(material);
	OS0AssetDamageResult const result = state.assetDamage.apply(key, maximum,
		std::max<INT16>(1, impact / 3));
	state.pendingVisualEvents.push_back({ gridNo, material, impact });
	state.pendingDiagnostics.push_back(ST::format(
		"ASSET HIT grid {} level {} tile {} hp {}/{}", base->sGridNo,
		level, canonicalTile, result.remaining, result.maximum));
	if (!result.destroyed) return TRUE;

	GridNo const baseGrid = base->sGridNo;
	{
		ApplyMapChangesToMapTempFile recordChange;
		RemoveStructFromLevelNode(baseGrid, node);
	}
	AddResourceItem(baseGrid, level, salvage.resource,
		std::max<UINT8>(1, salvage.amount));
	state.assetDamage.remove(key);
	RecompileLocalMovementCosts(baseGrid);
	InvalidateWorldRedundency();
	state.pendingDiagnostics.push_back(ST::format(
		"ASSET DESTROYED grid {} level {} / +{} {}", baseGrid, level,
		std::max<UINT8>(1, salvage.amount), ResourceName(salvage.resource)));
	return TRUE;
}

BOOLEAN OS0ValidateResourceItemDefinitions(ST::string* error)
{
	struct ExpectedItem { UINT16 index; const char* name; };
	constexpr std::array<ExpectedItem, 4> expected{{
		{ OS0_TIMBER, "OS0_TIMBER" }, { OS0_STONE, "OS0_STONE" },
		{ OS0_SCRAP, "OS0_SCRAP" }, { OS0_SOIL, "OS0_SOIL" }
	}};
	try
	{
		for (ExpectedItem const& value : expected)
		{
			ItemModel const* const byIndex = GCM->getItem(value.index);
			ItemModel const* const byName = GCM->getItemByName(value.name);
			if (!byIndex || !byName || byIndex->getInternalName() != value.name ||
				byName->getItemIndex() != value.index)
			{
				if (error) *error = ST::format(
					"OS0 resource item mismatch: index {} must be {}",
					value.index, value.name);
				return FALSE;
			}
		}
	}
	catch (...)
	{
		if (error) *error = "OS0 resource item definitions are missing";
		return FALSE;
	}
	return TRUE;
}
