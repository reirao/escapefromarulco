#include "OS0_RealtimeEditor.h"

#include "Animation_Data.h"
#include "Ambient_Control.h"
#include "Bullets.h"
#include "ContentManager.h"
#include "DirFs.h"
#include "Edit_Sys.h"
#include "Editor_Undo.h"
#include "Environment.h"
#include "Event_Pump.h"
#include "Explosion_Control.h"
#include "GameInstance.h"
#include "GameScreen.h"
#include "Handle_Items.h"
#include "Interface.h"
#include "Isometric_Utils.h"
#include "ItemModel.h"
#include "Items.h"
#include "Lighting.h"
#include "Map_Information.h"
#include "MercProfile.h"
#include "MercProfileInfo.h"
#include "OS0_AssetCatalogService.h"
#include "OppList.h"
#include "Overhead.h"
#include "PreBattle_Interface.h"
#include "Physics.h"
#include "RenderWorld.h"
#include "Road_Smoothing.h"
#include "SGPFile.h"
#include "Simple_Render_Utils.h"
#include "Smooth.h"
#include "Soldier_Add.h"
#include "Soldier_Control.h"
#include "Soldier_Create.h"
#include "Soldier_Find.h"
#include "Soldier_Init_List.h"
#include "Soldier_Profile.h"
#include "Strategic.h"
#include "Strategic_AI.h"
#include "StrategicMap.h"
#include "Structure.h"
#include "Sys_Globals.h"
#include "TileDat.h"
#include "TileDef.h"
#include "World_Items.h"
#include "WorldDat.h"
#include "WorldDef.h"
#include "WorldMan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <string>
#include <string_theory/format>
#include <type_traits>
#include <utility>

std::vector<INT16> OS0ResolveEditorBrushGridNos(INT16 const centerGridNo,
	UINT8 const radius)
{
	std::vector<INT16> result;
	if (centerGridNo < 0 || centerGridNo >= WORLD_MAX) return result;

	INT32 const centerRow = centerGridNo / WORLD_COLS;
	INT32 const centerColumn = centerGridNo % WORLD_COLS;
	INT32 const extent = radius;
	INT32 const firstRow = std::max<INT32>(0, centerRow - extent);
	INT32 const lastRow = std::min<INT32>(WORLD_ROWS - 1, centerRow + extent);
	INT32 const firstColumn = std::max<INT32>(0, centerColumn - extent);
	INT32 const lastColumn = std::min<INT32>(WORLD_COLS - 1,
		centerColumn + extent);
	result.reserve(static_cast<size_t>((lastRow - firstRow + 1) *
		(lastColumn - firstColumn + 1)));
	for (INT32 row = firstRow; row <= lastRow; ++row)
	{
		for (INT32 column = firstColumn; column <= lastColumn; ++column)
		{
			result.push_back(static_cast<INT16>(row * WORLD_COLS + column));
		}
	}
	return result;
}

BOOLEAN OS0EditorRoadMacroFitsWorld(INT16 const anchorGridNo,
	UINT8 const macroId)
{
	if (anchorGridNo < 0 || anchorGridNo >= WORLD_MAX ||
		macroId >= NUM_ROAD_MACROS)
	{
		return FALSE;
	}
	// The native macro bank reaches five rows/two columns above-left and one
	// row below its anchor.  This deliberately conservative envelope rejects a
	// few legal border placements, but prevents the legacy linear offsets from
	// wrapping into another row or escaping gpWorldLevelData.
	INT32 const row = anchorGridNo / WORLD_COLS;
	INT32 const column = anchorGridNo % WORLD_COLS;
	return row >= 5 && row < WORLD_ROWS - 1 && column >= 5;
}

namespace
{
	constexpr UINT16 INVALID_NPC_ID = 0xffff;
	constexpr UINT16 MAX_PENDING_COMMANDS = 1024;
	constexpr UINT8 MAX_TERRAIN_PAINT_RADIUS = 8;
	constexpr UINT8 MAX_TERRAIN_SMOOTH_RADIUS = 16;
	constexpr UINT16 ALL_SMOOTHABLE_TERRAIN = 0xffff;

	bool IsValidGridNo(INT32 const gridNo)
	{
		return 0 <= gridNo && gridNo < WORLD_MAX;
	}

	bool IsPaintableTerrainType(UINT16 const type)
	{
		return FIRSTTEXTURE <= type && type <= DEEPWATERTEXTURE &&
		gNumTilesPerType[type] != 0;
	}

	bool IsSmoothableTerrainType(UINT16 const type)
	{
		return ((FIRSTTEXTURE <= type && type <= SEVENTHTEXTURE) ||
			type == REGWATERTEXTURE) && gNumTilesPerType[type] != 0;
	}

	class CurrentPasteScope
	{
	public:
		explicit CurrentPasteScope(UINT16 const textureType) : previous_(CurrentPaste)
		{
			CurrentPaste = textureType;
		}
		~CurrentPasteScope() { CurrentPaste = previous_; }
		CurrentPasteScope(CurrentPasteScope const&) = delete;
		CurrentPasteScope& operator=(CurrentPasteScope const&) = delete;

	private:
		UINT16 previous_;
	};

	bool IsShadowType(UINT16 const type)
	{
		return type == FIRSTCLIFFSHADOW ||
			(FIRSTSHADOW <= type && type <= FOURTHFULLSHADOW) ||
			(FIRSTDOORSHADOW <= type && type <= FOURTHDOORSHADOW) ||
			type == FENCESHADOW ||
			(FIRSTVEHICLESHADOW <= type && type <= SECONDVEHICLESHADOW) ||
			(FIRSTDEBRISSTRUCTSHADOW <= type && type <= SECONDDEBRISSTRUCTSHADOW) ||
			(NINTHOSTRUCTSHADOW <= type && type <= TENTHOSTRUCTSHADOW) ||
			(FIRSTLARGEEXPDEBRISSHADOW <= type && type <= SECONDLARGEEXPDEBRISSHADOW);
	}

	bool IsObjectType(UINT16 const type)
	{
		return type == FIRSTCLIFFHANG || type == ANOTHERDEBRIS ||
			type == ROADPIECES || type == FIRSTROAD ||
			(DEBRISROCKS <= type && type <= DEBRISMISC) ||
			type == DEBRIS2MISC ||
			(FIRSTEXPLDEBRIS <= type && type <= SECONDEXPLDEBRIS) ||
			type == HUMANBLOOD || type == CREATUREBLOOD;
	}

	bool IsDirectlySaveableTileType(UINT16 const type)
	{
		// World serialization explicitly forbids the revealed-roof, item,
		// animation and UI ranges after FIRSTSWITCHES.  MOCKFLOOR and the
		// slant-roof ceiling are runtime helper graphics, not authored assets.
		return type <= FIRSTSWITCHES && type != MOCKFLOOR &&
			type != SLANTROOFCEILING;
	}

	bool IsManagedWallShadow(UINT16 const tileIndex)
	{
		if (tileIndex >= NUMBEROFTILES) return false;
		TILE_ELEMENT const& tile = gTileDatabase[tileIndex];
		return FIRSTWALL <= tile.fType && tile.fType <= LASTWALL &&
			(tile.usRegionIndex == 29 || tile.usRegionIndex == 30);
	}

	OS0EditorLayer DefaultLayer(UINT16 const tileIndex)
	{
		TILE_ELEMENT const& tile = gTileDatabase[tileIndex];
		UINT16 const type = tile.fType;
		if (IsManagedWallShadow(tileIndex)) return OS0EditorLayer::SHADOW;
		if (tile.uiFlags & AFRAME_TILE) return OS0EditorLayer::ROOF;
		if ((FIRSTTEXTURE <= type && type <= DEEPWATERTEXTURE) ||
			(FIRSTFLOOR <= type && type <= LASTFLOOR) || type == MOCKFLOOR)
		{
			return OS0EditorLayer::LAND;
		}
		if (IsObjectType(type)) return OS0EditorLayer::OBJECT;
		if (IsShadowType(type)) return OS0EditorLayer::SHADOW;
		if ((FIRSTROOF <= type && type <= LASTROOF) ||
			(FIRSTSLANTROOF <= type && type <= LASTSLANTROOF) ||
			(FIRSTHIGHROOF <= type && type <= SECONDHIGHROOF) ||
			type == SLANTROOFCEILING)
		{
			return OS0EditorLayer::ROOF;
		}
		if (FIRSTONROOF <= type && type <= SECONDONROOF)
		{
			return OS0EditorLayer::ON_ROOF;
		}
		return OS0EditorLayer::STRUCTURE;
	}

	OS0EditorCategory TileCategory(UINT16 const type)
	{
		if (FIRSTTEXTURE <= type && type <= SEVENTHTEXTURE)
			return OS0EditorCategory::TERRAIN;
		if (REGWATERTEXTURE <= type && type <= DEEPWATERTEXTURE)
			return OS0EditorCategory::WATER;
		if (FIRSTCLIFFHANG <= type && type <= FIRSTCLIFFSHADOW)
			return type == FIRSTCLIFFSHADOW ? OS0EditorCategory::SHADOWS :
				OS0EditorCategory::CLIFFS;
		if (FIRSTFULLSTRUCT <= type && type <= FOURTHFULLSTRUCT)
			return OS0EditorCategory::NATURE;
		if (FIRSTOSTRUCT <= type && type <= EIGHTOSTRUCT)
			return OS0EditorCategory::PROPS;
		if (IsShadowType(type)) return OS0EditorCategory::SHADOWS;
		if (FIRSTWALL <= type && type <= LASTWALL)
			return OS0EditorCategory::WALLS;
		if (FIRSTDOOR <= type && type <= LASTDOOR)
			return OS0EditorCategory::DOORS;
		if (type == FOURTHWINDOW) return OS0EditorCategory::WALLS;
		if ((FIRSTDECORATIONS <= type && type <= LASTDECORATIONS) ||
			(FIRSTWALLDECAL <= type && type <= LASTWALLDECAL) ||
			(FIFTHWALLDECAL <= type && type <= EIGTHWALLDECAL) ||
			type == FIRSTSWITCHES)
		{
			return OS0EditorCategory::DECORATIONS;
		}
		if (FIRSTFLOOR <= type && type <= LASTFLOOR)
			return OS0EditorCategory::FLOORS;
		if ((FIRSTROOF <= type && type <= LASTSLANTROOF) ||
			(FIRSTHIGHROOF <= type && type <= SECONDHIGHROOF) ||
			type == SLANTROOFCEILING)
		{
			return OS0EditorCategory::ROOFS;
		}
		if (FIRSTONROOF <= type && type <= SECONDONROOF)
			return OS0EditorCategory::ROOF_OBJECTS;
		if ((FIRSTISTRUCT <= type && type <= LASTISTRUCT) ||
			(FIFTHISTRUCT <= type && type <= EIGHTISTRUCT))
		{
			return OS0EditorCategory::BUILDINGS;
		}
		if (type == FIRSTROAD || type == ROADPIECES)
			return OS0EditorCategory::ROADS;
		if ((DEBRISROCKS <= type && type <= DEBRISMISC) ||
			type == DEBRIS2MISC || type == ANOTHERDEBRIS ||
			(FIRSTDEBRISSTRUCT <= type && type <= SECONDDEBRISSTRUCT) ||
			(FIRSTEXPLDEBRIS <= type && type <= SECONDLARGEEXPDEBRIS) ||
			type == HUMANBLOOD || type == CREATUREBLOOD)
		{
			return OS0EditorCategory::DEBRIS;
		}
		if (type == FENCESTRUCT) return OS0EditorCategory::FENCES;
		if (FIRSTVEHICLE <= type && type <= SECONDVEHICLE)
			return OS0EditorCategory::VEHICLES;
		if (type == ANIOSTRUCT) return OS0EditorCategory::NATURE;
		return OS0EditorCategory::PROPS;
	}

	OS0EditorCategory ItemCategory(ItemModel const& item)
	{
		if (item.isAmmo()) return OS0EditorCategory::AMMUNITION;
		if (item.isArmour() || item.isFace()) return OS0EditorCategory::ARMOUR;
		if (item.isWeapon()) return OS0EditorCategory::WEAPONS;
		if (item.isKit()) return OS0EditorCategory::TOOLS;
		if (item.isMedkit() || item.isMoney()) return OS0EditorCategory::CONSUMABLES;
		return OS0EditorCategory::ITEMS;
	}

	OS0EditorCategory EditorCategory(OS0AssetCategory const category,
		OS0EditorCategory const fallback)
	{
		switch (category)
		{
			case OS0AssetCategory::SANDBAG: return OS0EditorCategory::FENCES;
			case OS0AssetCategory::DOOR: return OS0EditorCategory::DOORS;
			case OS0AssetCategory::CONTAINER: return OS0EditorCategory::PROPS;
			case OS0AssetCategory::STONE: return OS0EditorCategory::NATURE;
			case OS0AssetCategory::DEBRIS: return OS0EditorCategory::DEBRIS;
			case OS0AssetCategory::FURNITURE: return OS0EditorCategory::PROPS;
			case OS0AssetCategory::TREE: return OS0EditorCategory::NATURE;
			case OS0AssetCategory::WALL: return OS0EditorCategory::WALLS;
			case OS0AssetCategory::FLOOR: return OS0EditorCategory::FLOORS;
			case OS0AssetCategory::RESOURCE: return OS0EditorCategory::NATURE;
			case OS0AssetCategory::DECOR: return OS0EditorCategory::DECORATIONS;
			case OS0AssetCategory::WORKSTATION: return OS0EditorCategory::BUILDINGS;
			case OS0AssetCategory::UNKNOWN:
			case OS0AssetCategory::COUNT: return fallback;
		}
		return fallback;
	}

	std::pair<UINT8, UINT8> Footprint(TILE_ELEMENT const& tile)
	{
		DB_STRUCTURE_REF const* const ref = tile.pDBStructureRef;
		if (ref == nullptr || ref->pDBStructure == nullptr ||
			ref->pDBStructure->ubNumberOfTiles == 0)
		{
			return { 1, 1 };
		}

		INT16 minX = 0;
		INT16 maxX = 0;
		INT16 minY = 0;
		INT16 maxY = 0;
		for (DB_STRUCTURE_TILE const* const part : ref->Tiles())
		{
			if (part == nullptr) continue;
			minX = std::min<INT16>(minX, part->bXPosRelToBase);
			maxX = std::max<INT16>(maxX, part->bXPosRelToBase);
			minY = std::min<INT16>(minY, part->bYPosRelToBase);
			maxY = std::max<INT16>(maxY, part->bYPosRelToBase);
		}
		return {
			static_cast<UINT8>(std::clamp<INT16>(maxX - minX + 1, 1, 255)),
			static_cast<UINT8>(std::clamp<INT16>(maxY - minY + 1, 1, 255))
		};
	}

	std::vector<OS0EditorCategoryRecord> BuildCategoryCatalog()
	{
		static constexpr std::array<const char*,
			static_cast<size_t>(OS0EditorCategory::COUNT)> labels{
			"Terrain", "Water", "Cliffs", "Nature", "Props", "Buildings",
			"Walls", "Doors", "Decorations", "Floors", "Roofs", "Roof objects",
			"Roads", "Debris", "Fences", "Vehicles", "Shadows", "Weapons",
			"Ammunition", "Armour", "Tools", "Consumables", "Items", "NPCs",
			"System"
		};
		std::vector<OS0EditorCategoryRecord> result;
		result.reserve(labels.size());
		for (size_t i = 0; i < labels.size(); ++i)
		{
			result.push_back({ static_cast<OS0EditorCategory>(i), labels[i],
				static_cast<UINT16>(i) });
		}
		return result;
	}

	std::vector<OS0EditorActionRecord> BuildActionCatalog()
	{
		return {
			{ OS0EditorAction::NEW_BLANK_MAP, OS0EditorCategory::SYSTEM,
				"New blank map", "Replace the live tactical world at the next frame boundary.",
				FALSE, TRUE },
			{ OS0EditorAction::LOAD_MAP, OS0EditorCategory::SYSTEM,
				"Load live-editor", "Load OS0/maps/live-editor.dat and preserve the active squad.",
				FALSE, TRUE },
			{ OS0EditorAction::PLACE_TILE, OS0EditorCategory::BUILDINGS,
				"Place world asset", "Place a catalogued tile on its native engine layer.",
				TRUE, FALSE },
			{ OS0EditorAction::PLACE_ITEM, OS0EditorCategory::ITEMS,
				"Place item", "Create an item model and add it to the world item pool.",
				TRUE, FALSE },
			{ OS0EditorAction::PLACE_NPC, OS0EditorCategory::NPCS,
				"Place NPC", "Create both a persistent map placement and its live soldier.",
				TRUE, FALSE },
			{ OS0EditorAction::REMOVE, OS0EditorCategory::SYSTEM,
				"Remove selected", "Remove one revalidated tile, item or NPC handle.",
				TRUE, FALSE },
			{ OS0EditorAction::SAVE_MAP, OS0EditorCategory::SYSTEM,
				"Save map", "Save a complete .dat below the private OS0/maps directory.",
				TRUE, FALSE },
			{ OS0EditorAction::REBUILD_CATALOGS, OS0EditorCategory::SYSTEM,
				"Refresh catalogs", "Re-read active tileset, items and editor templates.",
				FALSE, FALSE }
		};
	}

	std::vector<OS0EditorNpcTemplate> BuildNpcCatalog()
	{
		std::vector<OS0EditorNpcTemplate> result{
			{ 1, INVALID_NPC_ID, "Enemy administrator", ENEMY_TEAM, SOLDIER_CLASS_ADMINISTRATOR,
				BODY_RANDOM, STATIONARY, DEFENSIVE, NON_CIV_GROUP },
			{ 2, INVALID_NPC_ID, "Enemy soldier", ENEMY_TEAM, SOLDIER_CLASS_ARMY,
				BODY_RANDOM, ONGUARD, DEFENSIVE, NON_CIV_GROUP },
			{ 3, INVALID_NPC_ID, "Enemy elite", ENEMY_TEAM, SOLDIER_CLASS_ELITE,
				BODY_RANDOM, ONGUARD, CUNNINGAID, NON_CIV_GROUP },
			{ 4, INVALID_NPC_ID, "Green militia", MILITIA_TEAM, SOLDIER_CLASS_GREEN_MILITIA,
				BODY_RANDOM, ONGUARD, DEFENSIVE, NON_CIV_GROUP },
			{ 5, INVALID_NPC_ID, "Regular militia", MILITIA_TEAM, SOLDIER_CLASS_REG_MILITIA,
				BODY_RANDOM, ONGUARD, DEFENSIVE, NON_CIV_GROUP },
			{ 6, INVALID_NPC_ID, "Elite militia", MILITIA_TEAM, SOLDIER_CLASS_ELITE_MILITIA,
				BODY_RANDOM, ONGUARD, BRAVEAID, NON_CIV_GROUP },
			{ 7, INVALID_NPC_ID, "Civilian", CIV_TEAM, SOLDIER_CLASS_NONE,
				BODY_RANDOM, STATIONARY, DEFENSIVE, NON_CIV_GROUP },
			{ 8, INVALID_NPC_ID, "Bloodcat", CREATURE_TEAM, SOLDIER_CLASS_NONE,
				BLOODCAT, ONGUARD, AGGRESSIVE, NON_CIV_GROUP },
			{ 9, INVALID_NPC_ID, "Adult female creature", CREATURE_TEAM, SOLDIER_CLASS_NONE,
				ADULTFEMALEMONSTER, ONGUARD, AGGRESSIVE, NON_CIV_GROUP },
			{ 10, INVALID_NPC_ID, "Adult male creature", CREATURE_TEAM, SOLDIER_CLASS_NONE,
				AM_MONSTER, ONGUARD, AGGRESSIVE, NON_CIV_GROUP }
		};
		if (GCM == nullptr) return result;
		for (MercProfile const* const profile : GCM->listMercProfiles())
		{
			if (profile == nullptr || !profile->isNPCorRPC()) continue;
			ProfileID const profileId = profile->getID();
			MERCPROFILESTRUCT const& source = profile->getStruct();
			ST::string label = source.zNickname;
			if (label.empty()) label = source.zName;
			if (label.empty()) label = profile->getInfo().internalName;
			result.push_back({ static_cast<UINT16>(0x100 + profileId), profileId,
				std::move(label), CIV_TEAM, SOLDIER_CLASS_NONE,
				static_cast<INT8>(source.ubBodyType), STATIONARY, DEFENSIVE,
				NON_CIV_GROUP });
		}
		return result;
	}

	void MarkWireframeArea(INT16 const center, INT8 const radius)
	{
		INT16 const centerX = center % WORLD_COLS;
		INT16 const centerY = center / WORLD_COLS;
		for (INT16 dy = -radius; dy <= radius; ++dy)
		{
			INT16 const y = centerY + dy;
			if (y < 0 || y >= WORLD_ROWS) continue;
			for (INT16 dx = -radius; dx <= radius; ++dx)
			{
				INT16 const x = centerX + dx;
				if (x < 0 || x >= WORLD_COLS) continue;
				gpWorldLevelData[y * WORLD_COLS + x].uiFlags |=
					MAPELEMENT_RECALCULATE_WIREFRAMES;
			}
		}
	}

	LEVELNODE* FindNode(LEVELNODE* node, UINT16 const tileIndex)
	{
		for (; node != nullptr; node = node->pNext)
		{
			if (node->usIndex == tileIndex) return node;
		}
		return nullptr;
	}

	bool RemoveDoorShadow(INT16 const gridNo, UINT16 const tileIndex)
	{
		UINT16 const type = gTileDatabase[tileIndex].fType;
		if (type < FIRSTDOOR || type > LASTDOOR) return false;
		UINT16 const shadowType = type - FIRSTDOOR + FIRSTDOORSHADOW;
		UINT16 const shadowIndex = gTileTypeStartIndex[shadowType] +
			gTileDatabase[tileIndex].usRegionIndex;
		LEVELNODE* const shadow = FindNode(gpWorldLevelData[gridNo].pShadowHead,
			shadowIndex);
		return shadow != nullptr && RemoveShadowFromLevelNode(gridNo, shadow);
	}

	ST::string SafeMapPath(ST::string const& requested)
	{
		std::string input = requested.to_std_string();
		if (input.size() >= 4)
		{
			std::string suffix = input.substr(input.size() - 4);
			std::transform(suffix.begin(), suffix.end(), suffix.begin(),
				[](unsigned char const c) { return static_cast<char>(std::tolower(c)); });
			if (suffix == ".dat") input.resize(input.size() - 4);
		}

		std::string safe;
		safe.reserve(std::min<size_t>(input.size(), 64));
		for (unsigned char const c : input)
		{
			if (safe.size() == 64) break;
			if (std::isalnum(c) || c == '-' || c == '_') safe.push_back(c);
			else if ((c == ' ' || c == '.') && !safe.empty() && safe.back() != '_')
				safe.push_back('_');
		}
		while (!safe.empty() && safe.back() == '_') safe.pop_back();
		if (safe.empty()) safe = "untitled";
		return ST::format("OS0/maps/{}.dat", safe);
	}

	struct PlayerInsertionSnapshot
	{
		SoldierID id = NOBODY;
		UINT8 insertionCode = 0;
		UINT16 insertionData = 0;
		INT16 gridNo = NOWHERE;
		INT8 level = 0;
	};

	struct LiveWorldSnapshot
	{
		SoldierID selectedId = NOBODY;
		INT16 renderCenterX = 0;
		INT16 renderCenterY = 0;
		std::vector<PlayerInsertionSnapshot> players;
	};

	SOLDIERTYPE* FindEditorSoldier(SoldierID const id)
	{
		UINT const index = static_cast<UINT>(id);
		return index < TOTAL_SOLDIERS ? &Menptr[index] : nullptr;
	}

	void TrashWorldForRealtimeEditor();

	ST::string WorldSwapBlocker()
	{
		if (!gfWorldLoaded) return "No tactical world is loaded";
		if (!gWorldSector.IsValid())
			return "A valid tactical sector is required for a live world replacement";
		if (gTacticalStatus.uiFlags & INCOMBAT)
			return "Leave turn-based combat before replacing the live map";
		if (gTacticalStatus.uiFlags & (LOADING_SAVED_GAME | ENGAGED_IN_CONV) ||
			gfTacticalTraversal)
		{
			return "The tactical world is in a transition and cannot be replaced yet";
		}
		return {};
	}

	LiveWorldSnapshot CaptureLiveWorldSnapshot()
	{
		LiveWorldSnapshot result;
		result.selectedId = Soldier2ID(GetSelectedMan());
		result.renderCenterX = gsRenderCenterX;
		result.renderCenterY = gsRenderCenterY;
		FOR_EACH_IN_TEAM(soldier, OUR_TEAM)
		{
			if (soldier->sSector != gWorldSector || soldier->fBetweenSectors) continue;
			result.players.push_back({ soldier->ubID,
				soldier->ubStrategicInsertionCode,
				soldier->usStrategicInsertionData,
				soldier->sGridNo,
				soldier->bLevel });
		}
		return result;
	}

	void PrepareLiveWorldReplacement()
	{
		// LightReset destroys the sprite pool. Clear every owning SOLDIERTYPE
		// pointer first, including off-map team members.
		FOR_EACH_IN_TEAM(soldier, OUR_TEAM) DeleteSoldierLight(soldier);
		// These systems may retain pointers into the world. Do not call the
		// strategic unload wrapper: an editor operation must not advance campaign
		// state, quests, loyalty or sector travel.
		ClearEventQueue();
		DeleteAllBullets();
		RemoveAllPhysicsObjects();
		RemoveAllActiveTimedBombs();
		DeleteAllAmbients();
		RemoveMercsInSector();
		TrashWorldForRealtimeEditor();
		LightReset();
	}

	void RestoreLivePlayers(LiveWorldSnapshot const& snapshot,
		INT16 const fallbackGridNo, BOOLEAN const originalPositions)
	{
		INT16 const safeFallback = IsValidGridNo(fallbackGridNo) ? fallbackGridNo :
			static_cast<INT16>(CENTER_GRIDNO);
		for (PlayerInsertionSnapshot const& player : snapshot.players)
		{
			SOLDIERTYPE* const soldier = FindEditorSoldier(player.id);
			if (soldier == nullptr || !soldier->bActive) continue;
			INT16 const target = originalPositions && IsValidGridNo(player.gridNo) ?
				player.gridNo : safeFallback;
			soldier->ubStrategicInsertionCode = INSERTION_CODE_GRIDNO;
			soldier->usStrategicInsertionData = target;
			soldier->bLevel = originalPositions ? player.level : 0;
		}
		auto const previousTacticalFlags = gTacticalStatus.uiFlags;
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		try
		{
			UpdateMercsInSector();
		}
		catch (...)
		{
			gTacticalStatus.uiFlags = previousTacticalFlags;
			throw;
		}
		gTacticalStatus.uiFlags = previousTacticalFlags;
		for (PlayerInsertionSnapshot const& player : snapshot.players)
		{
			SOLDIERTYPE* const soldier = FindEditorSoldier(player.id);
			if (soldier == nullptr || !soldier->bActive) continue;
			soldier->ubStrategicInsertionCode = player.insertionCode;
			soldier->usStrategicInsertionData = player.insertionData;
			InitSoldierOppList(*soldier);
		}
		FOR_EACH_IN_TEAM(soldier, OUR_TEAM)
		{
			if (soldier->bInSector) ReCreateSoldierLight(soldier);
		}

		SOLDIERTYPE* selected = FindEditorSoldier(snapshot.selectedId);
		if (selected != nullptr && selected->bActive && selected->bInSector)
		{
			SelectSoldier(selected, SELSOLDIER_FORCE_RESELECT);
		}
		else if (!snapshot.players.empty())
		{
			selected = FindEditorSoldier(snapshot.players.front().id);
			if (selected != nullptr && selected->bActive && selected->bInSector)
				SelectSoldier(selected, SELSOLDIER_FORCE_RESELECT);
		}

		if (originalPositions)
		{
			SetRenderCenter(snapshot.renderCenterX, snapshot.renderCenterY);
		}
		else
		{
			INT16 centerX;
			INT16 centerY;
			ConvertGridNoToCenterCellXY(safeFallback, &centerX, &centerY);
			SetRenderCenter(centerX, centerY);
		}
		gfGameScreenLocateToSoldier = FALSE;
	}

	ST::string WorldRecoveryPath(std::uint64_t const commandId)
	{
		return ST::format("os0-editor-world-swap-{}.dat", commandId);
	}

	BOOLEAN WriteWorldRecovery(DirFs* const files, ST::string const& path)
	{
		if (files->exists(path)) files->deleteFile(path);
		AutoSGPFile file{ files->openForWriting(path, true) };
		return SaveWorldToSGPFile(file);
	}

	void DeleteWorldRecovery(DirFs* const files, ST::string const& path) noexcept
	{
		try
		{
			if (files != nullptr && files->exists(path)) files->deleteFile(path);
		}
		catch (...) { /* The process temp directory is cleaned independently. */ }
	}

	void InstantiateLoadedEditorPlacements()
	{
		// LoadWorldFromSGPFile restores the persistent placement list, not its live
		// SOLDIERTYPE instances. Mirror the original authoring load path so placed
		// profiles survive a round trip without campaign quest/death/sector filters.
		bool const previousEditMode = gfEditMode;
		gfEditMode = true;
		try
		{
			AddSoldierInitListTeamToWorld(ENEMY_TEAM);
			AddSoldierInitListTeamToWorld(CREATURE_TEAM);
			AddSoldierInitListTeamToWorld(MILITIA_TEAM);
			AddSoldierInitListTeamToWorld(CIV_TEAM);
		}
		catch (...)
		{
			gfEditMode = previousEditMode;
			throw;
		}
		gfEditMode = previousEditMode;
		gTacticalStatus.fEnemyInSector = NumEnemyInSector() > 0;
	}

	void RestoreLiveWorldAmbience()
	{
		if (giCurrentTilesetID != TILESET_INVALID &&
			giCurrentTilesetID < gubNumTilesets)
		{
			HandleNewSectorAmbience(gTilesets[giCurrentTilesetID].ubAmbientID);
		}
	}

	void LoadWorldRecovery(DirFs* const files, ST::string const& path)
	{
		// LoadWorldFromSGPFile only trashes a world marked as loaded. A failed
		// target load can leave partially allocated nodes before setting that flag.
		// Mark a partial load as live so the shared teardown can release every
		// native node. It also clears any lights/events recreated between the
		// target load and a later placement/reinsertion failure.
		if (!gfWorldLoaded) gfWorldLoaded = TRUE;
		PrepareLiveWorldReplacement();
		AutoSGPFile file{ files->openForReading(path) };
		// PrepareLiveWorldReplacement leaves gfWorldLoaded false, so the loader's
		// internal TrashWorld is a no-op and cannot trigger strategic side effects.
		LoadWorldFromSGPFile(file);
	}

	void TrashWorldForRealtimeEditor()
	{
		// TrashWorld normally consumes the first-battle meanwhile flag.  A
		// visual editor reset is not a campaign transition, so suppress that
		// one embedded strategic side effect and restore it afterwards.
		BOOLEAN const pendingMeanwhile = gfFirstBattleMeanwhileScenePending;
		gfFirstBattleMeanwhileScenePending = FALSE;
		try
		{
			TrashWorld();
		}
		catch (...)
		{
			gfFirstBattleMeanwhileScenePending = pendingMeanwhile;
			throw;
		}
		gfFirstBattleMeanwhileScenePending = pendingMeanwhile;
	}

	OS0EditorCommandResult MakeResult(OS0EditorCommand const& command,
		BOOLEAN const success, BOOLEAN const worldChanged, ST::string message)
	{
		return { command.id, command.type, success, worldChanged,
			std::move(message) };
	}
}

OS0EditorCatalog OS0RealtimeEditorEngineAdapter::rebuildCatalogs(
	std::uint64_t const generation) const
{
	OS0EditorCatalog catalog;
	catalog.generation = generation;
	catalog.tileset = giCurrentTilesetID == TILESET_INVALID ? -1 :
		static_cast<INT16>(giCurrentTilesetID);
	catalog.categories = BuildCategoryCatalog();
	catalog.actions = BuildActionCatalog();
	catalog.npcTemplates = BuildNpcCatalog();

	if (giCurrentTilesetID != TILESET_INVALID)
	{
		for (UINT16 type = 0; type <= FIRSTSWITCHES; ++type)
		{
			if (!IsDirectlySaveableTileType(type)) continue;
			UINT16 const start = gTileTypeStartIndex[type];
			for (UINT16 region = 0; region < gNumTilesPerType[type]; ++region)
			{
				UINT32 const wideIndex = static_cast<UINT32>(start) + region;
				if (wideIndex >= NUMBEROFTILES) break;
				UINT16 const tileIndex = static_cast<UINT16>(wideIndex);
				TILE_ELEMENT const& tile = gTileDatabase[tileIndex];
				if (tile.uiFlags & UNDERFLOW_FILLER) continue;
				if (tile.hTileSurface == nullptr) continue;
				// Every frame carries ANIMATED_TILE.  Only publish the authored
				// animation root, otherwise the UI shows the same object repeatedly.
				if ((tile.uiFlags & ANIMATED_TILE) && tile.pusFrames != nullptr &&
					tile.pusFrames[0] != tileIndex)
				{
					continue;
				}

				auto [width, height] = Footprint(tile);
				OS0EditorLayer const layer = DefaultLayer(tileIndex);
				BOOLEAN const placeable = layer != OS0EditorLayer::SHADOW ||
					IsManagedWallShadow(tileIndex);
				OS0EditorCategory category = TileCategory(type);
				ST::string label = ST::format("{} {}", gTileSurfaceName[type],
					tile.usRegionIndex);
				if (OS0AssetCatalogRecord const* const override =
					OS0FindAssetCatalogRecordConst(catalog.tileset, tileIndex))
				{
					category = EditorCategory(override->category, category);
					width = std::max<UINT8>(1, override->width);
					height = std::max<UINT8>(1, override->height);
					if (!override->label.empty()) label = override->label;
				}
				catalog.tiles.push_back({
					catalog.tileset, tileIndex, type, tile.usRegionIndex,
					category, layer, std::move(label),
					static_cast<UINT32>(tile.uiFlags),
					static_cast<UINT8>(tile.usWallOrientation), width, height,
					placeable, static_cast<BOOLEAN>((tile.uiFlags & ANIMATED_TILE) != 0),
					static_cast<BOOLEAN>(width > 1 || height > 1),
					static_cast<BOOLEAN>((tile.uiFlags & HAS_SHADOW_BUDDY) != 0)
				});
			}
		}
	}

	if (GCM != nullptr && GCM->items() != nullptr)
	{
		for (ItemModel const* const item : *GCM->items())
		{
			if (item == nullptr) continue;
			if ((item->getFlags() & ITEM_NOT_EDITOR) ||
				item->getItemIndex() == SWITCH || item->getItemIndex() == ACTION_ITEM)
			{
				continue;
			}
			ST::string label = item->getName();
			if (label.empty()) label = item->getInternalName();
			catalog.items.push_back({ item->getItemIndex(), item->getItemClass(),
				ItemCategory(*item), item->getInternalName(), std::move(label),
				item->getWeight(), item->getPerPocket(), item->getFlags() });
		}
	}

	std::sort(catalog.tiles.begin(), catalog.tiles.end(),
		[](OS0EditorTileRecord const& lhs, OS0EditorTileRecord const& rhs)
		{
			if (lhs.category != rhs.category) return lhs.category < rhs.category;
			if (lhs.tileType != rhs.tileType) return lhs.tileType < rhs.tileType;
			return lhs.regionIndex < rhs.regionIndex;
		});
	std::sort(catalog.items.begin(), catalog.items.end(),
		[](OS0EditorItemRecord const& lhs, OS0EditorItemRecord const& rhs)
		{
			if (lhs.category != rhs.category) return lhs.category < rhs.category;
			return lhs.itemIndex < rhs.itemIndex;
		});
	return catalog;
}

OS0EditorCommandResult OS0RealtimeEditorEngineAdapter::execute(
	OS0EditorCommand const& command)
try
{
	switch (command.type)
	{
		case OS0EditorCommandType::NEW_BLANK_MAP:
		{
			auto const& request = std::get<OS0EditorBlankMapRequest>(command.payload);
			TileSetID tileset = request.tileset < 0 ? giCurrentTilesetID :
				static_cast<TileSetID>(request.tileset);
			if (tileset == TILESET_INVALID) tileset = GetDefaultTileset();
			if (tileset >= gubNumTilesets)
			{
				return MakeResult(command, FALSE, FALSE, "Invalid tileset id");
			}
			ST::string blocker = WorldSwapBlocker();
			if (!blocker.empty())
				return MakeResult(command, FALSE, FALSE, std::move(blocker));
			if (GCM == nullptr || GCM->tempFiles() == nullptr)
				return MakeResult(command, FALSE, FALSE,
					"No temporary filesystem is available for world-swap recovery");

			INT16 const center = IsValidGridNo(request.centerGridNo) ?
				request.centerGridNo : static_cast<INT16>(CENTER_GRIDNO);
			LiveWorldSnapshot const snapshot = CaptureLiveWorldSnapshot();
			DirFs* const recoveryFiles = GCM->tempFiles();
			ST::string const recoveryPath = WorldRecoveryPath(command.id);
			fullWireframeRebuild_ = TRUE;
			renderDirty_ = TRUE;
			RaiseWorldLand();
			try
			{
				if (!WriteWorldRecovery(recoveryFiles, recoveryPath))
				{
					DeleteWorldRecovery(recoveryFiles, recoveryPath);
					return MakeResult(command, FALSE, TRUE,
						"Blank map cancelled: recovery snapshot serialization failed");
				}
			}
			catch (std::exception const& error)
			{
				DeleteWorldRecovery(recoveryFiles, recoveryPath);
				return MakeResult(command, FALSE, TRUE, ST::format(
					"Blank map cancelled: recovery snapshot failed: {}", error.what()));
			}

			auto restorePreviousWorld = [&](ST::string const& failure)
			{
				catalogInvalidated_ = TRUE;
				renderDirty_ = TRUE;
				try
				{
					LoadWorldRecovery(recoveryFiles, recoveryPath);
					RestoreLivePlayers(snapshot, CENTER_GRIDNO, TRUE);
					InstantiateLoadedEditorPlacements();
					RestoreLiveWorldAmbience();
					DeleteWorldRecovery(recoveryFiles, recoveryPath);
					return MakeResult(command, FALSE, TRUE,
						ST::format("{}; previous world restored", failure));
				}
				catch (std::exception const& recoveryError)
				{
					return MakeResult(command, FALSE, TRUE, ST::format(
						"{}; recovery failed: {} (snapshot: {})", failure,
						recoveryError.what(), recoveryFiles->absolutePath(recoveryPath)));
				}
				catch (...)
				{
					return MakeResult(command, FALSE, TRUE, ST::format(
						"{}; recovery failed (snapshot: {})", failure,
						recoveryFiles->absolutePath(recoveryPath)));
				}
			};

			try
			{
				PrepareLiveWorldReplacement();
				LoadMapTileset(tileset);
				NewWorld();

				UINT16 const groundCount = gNumTilesPerType[FIRSTTEXTURE];
				UINT16 const groundRegion = groundCount == 0 ? 0 :
					std::min<UINT16>(request.groundRegion, groundCount - 1);
				UINT16 const groundTile = gTileTypeStartIndex[FIRSTTEXTURE] + groundRegion;
				for (INT32 gridNo = 0; gridNo < WORLD_MAX; ++gridNo)
				{
					DeleteAllLandLayers(gridNo);
					AddLandToHead(gridNo, groundTile);
				}

				gMapInformation = MAPCREATE_STRUCT{};
				gMapInformation.sNorthGridNo = -1;
				gMapInformation.sEastGridNo = -1;
				gMapInformation.sSouthGridNo = -1;
				gMapInformation.sWestGridNo = -1;
				gMapInformation.sCenterGridNo = center;
				gMapInformation.sIsolatedGridNo = -1;
				gMapInformation.ubMapVersion = gubMinorMapVersion;
				gMapInformation.ubEditorSmoothingType = SMOOTHING_NORMAL;
				gMapInformation.ubRestrictedScrollID = 0;
				gMapInformation.ubNumIndividuals = 0;
				gfBasement = FALSE;
				gfCaves = FALSE;
				gCurrentBackground = FIRSTTEXTURE;
				ubAmbientLightLevel = GetTimeOfDayAmbientLightLevel();
				LightSetBaseLevel(ubAmbientLightLevel);
				InitRenderParams(gMapInformation.ubRestrictedScrollID);
				InitLoadedWorld();
				InitOpponentKnowledgeSystem();
				gTacticalStatus.fEnemyInSector = FALSE;
				gTacticalStatus.fVirginSector = TRUE;
				EndTopMessage();
				RestoreLivePlayers(snapshot, center, FALSE);
				RestoreLiveWorldAmbience();
				DeleteWorldRecovery(recoveryFiles, recoveryPath);
				renderDirty_ = TRUE;
				catalogInvalidated_ = TRUE;
				return MakeResult(command, TRUE, TRUE,
					ST::format("Blank map created with tileset {}", request.tileset < 0 ?
						static_cast<INT16>(tileset) : request.tileset));
			}
			catch (std::exception const& error)
			{
				return restorePreviousWorld(ST::format(
					"Blank map reset failed after world teardown: {}", error.what()));
			}
			catch (...)
			{
				return restorePreviousWorld(
					"Blank map reset failed after world teardown");
			}
		}

		case OS0EditorCommandType::LOAD_MAP:
		{
			auto const& request = std::get<OS0EditorLoadRequest>(command.payload);
			ST::string blocker = WorldSwapBlocker();
			if (!blocker.empty())
				return MakeResult(command, FALSE, FALSE, std::move(blocker));
			if (GCM == nullptr || GCM->userPrivateFiles() == nullptr ||
				GCM->tempFiles() == nullptr)
			{
				return MakeResult(command, FALSE, FALSE,
					"No private or temporary filesystem is available");
			}

			DirFs* const mapFiles = GCM->userPrivateFiles();
			ST::string const mapPath = SafeMapPath(request.name);
			if (!mapFiles->isFile(mapPath))
				return MakeResult(command, FALSE, FALSE,
					ST::format("Map does not exist: {}", mapFiles->absolutePath(mapPath)));
			AutoSGPFile mapFile{ mapFiles->openForReading(mapPath) };
			LiveWorldSnapshot const snapshot = CaptureLiveWorldSnapshot();
			DirFs* const recoveryFiles = GCM->tempFiles();
			ST::string const recoveryPath = WorldRecoveryPath(command.id);
			fullWireframeRebuild_ = TRUE;
			renderDirty_ = TRUE;
			RaiseWorldLand();
			try
			{
				if (!WriteWorldRecovery(recoveryFiles, recoveryPath))
				{
					DeleteWorldRecovery(recoveryFiles, recoveryPath);
					return MakeResult(command, FALSE, TRUE,
						"Map load cancelled: recovery snapshot serialization failed");
				}
			}
			catch (std::exception const& error)
			{
				DeleteWorldRecovery(recoveryFiles, recoveryPath);
				return MakeResult(command, FALSE, TRUE, ST::format(
					"Map load cancelled: recovery snapshot failed: {}", error.what()));
			}

			auto restorePreviousWorld = [&](ST::string const& failure)
			{
				catalogInvalidated_ = TRUE;
				renderDirty_ = TRUE;
				try
				{
					LoadWorldRecovery(recoveryFiles, recoveryPath);
					RestoreLivePlayers(snapshot, CENTER_GRIDNO, TRUE);
					InstantiateLoadedEditorPlacements();
					RestoreLiveWorldAmbience();
					DeleteWorldRecovery(recoveryFiles, recoveryPath);
					return MakeResult(command, FALSE, TRUE,
						ST::format("{}; previous world restored", failure));
				}
				catch (std::exception const& recoveryError)
				{
					return MakeResult(command, FALSE, TRUE, ST::format(
						"{}; recovery failed: {} (snapshot: {})", failure,
						recoveryError.what(), recoveryFiles->absolutePath(recoveryPath)));
				}
				catch (...)
				{
					return MakeResult(command, FALSE, TRUE, ST::format(
						"{}; recovery failed (snapshot: {})", failure,
						recoveryFiles->absolutePath(recoveryPath)));
				}
			};

			try
			{
				PrepareLiveWorldReplacement();
				LoadWorldFromSGPFile(mapFile);
				INT16 const center = IsValidGridNo(gMapInformation.sCenterGridNo) ?
					gMapInformation.sCenterGridNo : static_cast<INT16>(CENTER_GRIDNO);
				RestoreLivePlayers(snapshot, center, FALSE);
				InstantiateLoadedEditorPlacements();
				RestoreLiveWorldAmbience();
				gTacticalStatus.fVirginSector = TRUE;
				EndTopMessage();
				DeleteWorldRecovery(recoveryFiles, recoveryPath);
				catalogInvalidated_ = TRUE;
				renderDirty_ = TRUE;
				return MakeResult(command, TRUE, TRUE,
					ST::format("Loaded {}", mapFiles->absolutePath(mapPath)));
			}
			catch (std::exception const& error)
			{
				return restorePreviousWorld(ST::format(
					"Map load failed after world teardown: {}", error.what()));
			}
			catch (...)
			{
				return restorePreviousWorld("Map load failed after world teardown");
			}
		}

		case OS0EditorCommandType::PLACE_TILE:
		{
			auto const& request = std::get<OS0EditorTilePlacement>(command.payload);
			if (!gfWorldLoaded || !IsValidGridNo(request.gridNo) ||
				request.tileIndex >= NUMBEROFTILES)
			{
				return MakeResult(command, FALSE, FALSE, "Invalid tile placement");
			}
			if (request.tileset >= 0 && request.tileset !=
				static_cast<INT16>(giCurrentTilesetID))
			{
				return MakeResult(command, FALSE, FALSE,
					"Tile belongs to a stale tileset catalog");
			}
			TILE_ELEMENT const& tile = gTileDatabase[request.tileIndex];
			if ((tile.uiFlags & UNDERFLOW_FILLER) ||
				!IsDirectlySaveableTileType(tile.fType))
			{
				return MakeResult(command, FALSE, FALSE,
					"Tile is an internal/filler element and cannot be placed");
			}
			OS0EditorLayer const layer = request.layer == OS0EditorLayer::AUTO ?
				DefaultLayer(request.tileIndex) : request.layer;
			BOOLEAN const wallLike =
				(FIRSTWALL <= tile.fType && tile.fType <= LASTWALL) ||
				(FIRSTDOOR <= tile.fType && tile.fType <= LASTDOOR) ||
				(FIRSTDECORATIONS <= tile.fType && tile.fType <= LASTDECORATIONS);
			if (layer == OS0EditorLayer::SHADOW &&
				!IsManagedWallShadow(request.tileIndex))
			{
				return MakeResult(command, FALSE, FALSE,
					"Shadow tiles are managed by their owning world asset");
			}

			INT16 collisionExclusion = INVALID_STRUCTURE_ID;
			BOOLEAN const collisionLayer = layer == OS0EditorLayer::STRUCTURE ||
				layer == OS0EditorLayer::ON_ROOF;
			if (request.replaceExisting && collisionLayer)
			{
				LEVELNODE* const head = layer == OS0EditorLayer::ON_ROOF ?
					gpWorldLevelData[request.gridNo].pOnRoofHead :
					gpWorldLevelData[request.gridNo].pStructHead;
				LEVELNODE* const existing = FindNode(head, request.tileIndex);
				if (existing != nullptr && existing->pStructureData != nullptr)
				{
					collisionExclusion = static_cast<INT16>(
						existing->pStructureData->usStructureID);
				}
			}
			INT8 const structureLevel = layer == OS0EditorLayer::ON_ROOF ? 1 : 0;
			if (collisionLayer && request.validateCollision &&
				tile.pDBStructureRef != nullptr && !wallLike &&
				!OkayToAddStructureToWorld(request.gridNo, structureLevel,
					tile.pDBStructureRef, collisionExclusion))
			{
				return MakeResult(command, FALSE, FALSE,
					"Structure footprint is blocked");
			}

			AddToUndoList(request.gridNo);
			// Mark before the first mutation.  If a lower-level placement throws
			// after partially changing a multi-part asset, frame finalization still
			// repairs movement, wireframes and rendering.
			dirtyGridNos_.push_back(request.gridNo);
			if (layer == OS0EditorLayer::LAND ||
				(layer == OS0EditorLayer::OBJECT && tile.ubTerrainID != NO_TERRAIN))
			{
				fullMovementRebuild_ = TRUE;
			}
			if (request.replaceExisting)
			{
				switch (layer)
				{
					case OS0EditorLayer::OBJECT:
						RemoveObject(request.gridNo, request.tileIndex); break;
					case OS0EditorLayer::STRUCTURE:
						if (!wallLike) RemoveStruct(request.gridNo, request.tileIndex);
						break;
					case OS0EditorLayer::SHADOW:
					{
						if (LEVELNODE* const node = FindNode(
							gpWorldLevelData[request.gridNo].pShadowHead,
							request.tileIndex))
						{
							RemoveShadowFromLevelNode(request.gridNo, node);
						}
						break;
					}
					case OS0EditorLayer::ROOF:
						RemoveRoof(request.gridNo, request.tileIndex); break;
					case OS0EditorLayer::ON_ROOF:
						RemoveOnRoof(request.gridNo, request.tileIndex);
						if (tile.sBuddyNum >= 0)
						{
							RemoveOnRoof(request.gridNo,
								static_cast<UINT16>(tile.sBuddyNum));
						}
						break;
					case OS0EditorLayer::TOPMOST:
						RemoveTopmost(request.gridNo, request.tileIndex); break;
					default: break;
				}
			}

			switch (layer)
			{
				case OS0EditorLayer::LAND:
					SetLandIndex(request.gridNo, request.tileIndex, tile.fType); break;
				case OS0EditorLayer::OBJECT:
					AddObjectToHead(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::STRUCTURE:
					if (wallLike)
					{
						if (!AddWallToStructLayer(request.gridNo, request.tileIndex,
							request.replaceExisting))
						{
							return MakeResult(command, FALSE, FALSE,
								"A parallel wall already occupies this tile");
						}
						if (!gfBasement && FIRSTDOOR <= tile.fType && tile.fType <= LASTDOOR)
						{
							UINT16 const shadowType = tile.fType - FIRSTDOOR + FIRSTDOORSHADOW;
							AddExclusiveShadow(request.gridNo,
								gTileTypeStartIndex[shadowType] + tile.usRegionIndex);
						}
					}
					else
					{
						AddStructToHead(request.gridNo, request.tileIndex);
						// Cliff faces are authored as a structural bank plus a
						// matching overhang object on the same region index.
						if (tile.fType == FIRSTCLIFF && tile.usRegionIndex <
							gNumTilesPerType[FIRSTCLIFFHANG])
						{
							UINT16 const hanger = gTileTypeStartIndex[FIRSTCLIFFHANG] +
								tile.usRegionIndex;
							if (request.replaceExisting)
								RemoveObject(request.gridNo, hanger);
							AddObjectToHead(request.gridNo, hanger);
						}
					}
					break;
				case OS0EditorLayer::SHADOW:
					AddExclusiveShadow(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::ROOF:
					AddRoofToHead(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::ON_ROOF:
					AddOnRoofToHead(request.gridNo, request.tileIndex);
					if (tile.sBuddyNum >= 0)
						AddOnRoofToHead(request.gridNo,
							static_cast<UINT16>(tile.sBuddyNum));
					break;
				case OS0EditorLayer::TOPMOST:
					AddTopmostToHead(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::AUTO:
					return MakeResult(command, FALSE, FALSE, "Unsupported tile layer");
			}
			return MakeResult(command, TRUE, TRUE, "World asset placed");
		}

		case OS0EditorCommandType::APPLY_RECIPE:
		{
			auto const& request = std::get<OS0EditorRecipeRequest>(command.payload);
			if (!gfWorldLoaded)
				return MakeResult(command, FALSE, FALSE, "No world is loaded");

			if (auto const* const paint =
				std::get_if<OS0EditorTerrainPaintRecipe>(&request.recipe))
			{
				if (!IsValidGridNo(paint->gridNo) ||
					!IsPaintableTerrainType(paint->textureType) ||
					paint->radius > MAX_TERRAIN_PAINT_RADIUS)
				{
					return MakeResult(command, FALSE, FALSE,
						"Invalid terrain paint recipe");
				}
				std::vector<INT16> const grids = OS0ResolveEditorBrushGridNos(
					paint->gridNo, paint->radius);
				// PasteTextureCommon is the canonical JA2 texture-stack algorithm.
				// Scope the editor global so the queued recipe cannot leak selection
				// state into the standalone editor or a later command.
				CurrentPasteScope const selection(paint->textureType);
				for (INT16 const gridNo : grids)
				{
					dirtyGridNos_.push_back(gridNo);
					PasteTextureCommon(gridNo);
				}
				fullMovementRebuild_ = TRUE;
				return MakeResult(command, TRUE, TRUE,
					ST::format("Painted terrain texture {} across {} tile(s)",
						paint->textureType, grids.size()));
			}

			if (auto const* const smooth =
				std::get_if<OS0EditorTerrainSmoothRecipe>(&request.recipe))
			{
				if (!IsValidGridNo(smooth->gridNo) ||
					smooth->radius > MAX_TERRAIN_SMOOTH_RADIUS ||
					(smooth->textureType != ALL_SMOOTHABLE_TERRAIN &&
						!IsSmoothableTerrainType(smooth->textureType)))
				{
					return MakeResult(command, FALSE, FALSE,
						"Invalid terrain smoothing recipe");
				}
				std::vector<INT16> const grids = OS0ResolveEditorBrushGridNos(
					smooth->gridNo, static_cast<UINT8>(std::min<UINT16>(
						MAX_TERRAIN_SMOOTH_RADIUS + 1,
						static_cast<UINT16>(smooth->radius) + 1)));
				dirtyGridNos_.insert(dirtyGridNos_.end(), grids.begin(), grids.end());
				if (smooth->textureType == ALL_SMOOTHABLE_TERRAIN)
				{
					for (UINT16 type = FIRSTTEXTURE; type <= SEVENTHTEXTURE; ++type)
					{
						if (IsSmoothableTerrainType(type))
							SmoothTerrainRadius(smooth->gridNo, type, smooth->radius,
								smooth->force);
					}
					if (IsSmoothableTerrainType(REGWATERTEXTURE))
					{
						SmoothTerrainRadius(smooth->gridNo, REGWATERTEXTURE,
							smooth->radius, smooth->force);
					}
				}
				else
				{
					SmoothTerrainRadius(smooth->gridNo, smooth->textureType,
						smooth->radius, smooth->force);
				}
				fullMovementRebuild_ = TRUE;
				return MakeResult(command, TRUE, TRUE, "Terrain edges smoothed");
			}

			auto const* const road = std::get_if<OS0EditorRoadRecipe>(&request.recipe);
			if (road == nullptr ||
				!OS0EditorRoadMacroFitsWorld(road->gridNo, road->macroId) ||
				gNumTilesPerType[ROADPIECES] == 0)
			{
				return MakeResult(command, FALSE, FALSE, "Invalid road macro recipe");
			}
			// Mark the conservative native macro envelope before mutation so a
			// partial lower-level failure is still finalized safely.
			std::vector<INT16> const grids = OS0ResolveEditorBrushGridNos(
				road->gridNo, 6);
			dirtyGridNos_.insert(dirtyGridNos_.end(), grids.begin(), grids.end());
			PlaceRoadMacroAtGridNo(road->gridNo, road->macroId);
			fullMovementRebuild_ = TRUE;
			return MakeResult(command, TRUE, TRUE,
				ST::format("Placed native road macro {}", road->macroId));
		}

		case OS0EditorCommandType::PLACE_ITEM:
		{
			auto const& request = std::get<OS0EditorItemPlacement>(command.payload);
			if (!gfWorldLoaded || !IsValidGridNo(request.gridNo) ||
				GCM == nullptr || GCM->getItem(request.itemIndex, ItemSystem::nothrow) == nullptr)
			{
				return MakeResult(command, FALSE, FALSE, "Invalid item placement");
			}
			UINT8 const quantity = std::max<UINT8>(1, request.quantity);
			Visibility const visibility = request.visibility ==
				OS0EditorItemVisibility::VISIBLE ? VISIBLE : INVISIBLE;
			for (UINT8 i = 0; i < quantity; ++i)
			{
				OBJECTTYPE item{};
				CreateItem(request.itemIndex,
					static_cast<INT8>(std::clamp<INT16>(request.condition, 1, 100)), &item);
				if (AddItemToPool(request.gridNo, &item, visibility,
					request.level > 0 ? 1 : 0, WORLD_ITEM_REACHABLE, -1) < 0)
				{
					if (i > 0) renderDirty_ = TRUE;
					return MakeResult(command, FALSE, i > 0,
						"The item pool rejected an item");
				}
			}
			renderDirty_ = TRUE;
			return MakeResult(command, TRUE, TRUE,
				ST::format("Placed {} item(s)", quantity));
		}

		case OS0EditorCommandType::PLACE_NPC:
		{
			auto const& request = std::get<OS0EditorNpcPlacement>(command.payload);
			auto const templates = BuildNpcCatalog();
			auto const found = std::find_if(templates.begin(), templates.end(),
				[&](OS0EditorNpcTemplate const& value)
				{ return value.id == request.templateId; });
			if (!gfWorldLoaded || !IsValidGridNo(request.gridNo) ||
				found == templates.end())
			{
				return MakeResult(command, FALSE, FALSE, "Invalid NPC placement");
			}
			if (found->profileId != INVALID_NPC_ID &&
				FindSoldierByProfileID(static_cast<ProfileID>(found->profileId)) != nullptr)
			{
				return MakeResult(command, FALSE, FALSE,
					"That named NPC profile already exists in the live simulation");
			}
			if (!IsLocationSittable(request.gridNo, request.onRoof) ||
				(request.onRoof && !FlatRoofAboveGridNo(request.gridNo)))
			{
				return MakeResult(command, FALSE, FALSE,
					"NPC destination is not sittable");
			}
			if (gMapInformation.ubNumIndividuals >= MAX_NUM_SOLDIERS)
			{
				return MakeResult(command, FALSE, FALSE, "Map NPC limit reached");
			}

			BASIC_SOLDIERCREATE_STRUCT basic{};
			basic.usStartingGridNo = request.gridNo;
			basic.fDetailedPlacement = found->profileId != INVALID_NPC_ID;
			basic.bTeam = found->team;
			basic.bRelativeAttributeLevel = std::clamp<INT8>(
				request.relativeAttributeLevel, 0, 4);
			basic.bRelativeEquipmentLevel = std::clamp<INT8>(
				request.relativeEquipmentLevel, 0, 4);
			basic.bDirection = request.direction >= 0 && request.direction < 8 ?
				request.direction : static_cast<INT8>(NORTHWEST);
			basic.bOrders = found->orders;
			basic.bAttitude = found->attitude;
			basic.bBodyType = found->bodyType;
			basic.fOnRoof = request.onRoof;
			basic.ubSoldierClass = found->soldierClass;
			basic.ubCivilianGroup = found->civilianGroup;
			basic.fPriorityExistance = FALSE;

			SOLDIERCREATE_STRUCT detailed{};
			CreateDetailedPlacementGivenBasicPlacementInfo(&detailed, &basic);
			if (found->profileId != INVALID_NPC_ID)
				detailed.ubProfile = static_cast<ProfileID>(found->profileId);
			detailed.sSector = gWorldSector;
			SOLDIERTYPE* const soldier = TacticalCreateSoldier(detailed);
			if (soldier == nullptr)
			{
				return MakeResult(command, FALSE, FALSE,
					"Tactical soldier allocation failed");
			}
			renderDirty_ = TRUE;

			UseEditorOriginalList();
			SOLDIERINITNODE* const node = AddBasicPlacementToSoldierInitList(basic);
			if (found->profileId != INVALID_NPC_ID)
			{
				node->pDetailedPlacement = new SOLDIERCREATE_STRUCT{};
				*node->pDetailedPlacement = detailed;
			}
			node->pSoldier = soldier;
			node->ubSoldierID = soldier->ubID;
			soldier->bVisible = 1;
			soldier->bLastRenderVisibleValue = 1;
			AddSoldierToSectorNoCalculateDirection(soldier);
			if (request.onRoof) SetSoldierHeight(soldier, SECOND_LEVEL_Z_OFFSET);
			return MakeResult(command, TRUE, TRUE,
				ST::format("Placed NPC template {}", request.templateId));
		}

		case OS0EditorCommandType::REMOVE:
		{
			auto const& request = std::get<OS0EditorRemoveRequest>(command.payload);
			if (!gfWorldLoaded)
				return MakeResult(command, FALSE, FALSE, "No world is loaded");

			if (request.kind == OS0EditorRemoveKind::WORLD_ITEM)
			{
				if (request.worldItemIndex < 0 ||
					static_cast<size_t>(request.worldItemIndex) >= gWorldItems.size() ||
					!GetWorldItem(request.worldItemIndex).fExists)
				{
					return MakeResult(command, FALSE, FALSE,
						"World item handle is stale");
				}
				WORLDITEM& worldItem = GetWorldItem(request.worldItemIndex);
				if ((request.expectedItemIndex != 0xffff &&
					worldItem.o.usItem != request.expectedItemIndex) ||
					(IsValidGridNo(request.expectedItemGridNo) &&
					worldItem.sGridNo != request.expectedItemGridNo))
				{
					return MakeResult(command, FALSE, FALSE,
						"World item slot was reused before removal");
				}
				INT16 const gridNo = worldItem.sGridNo;
				RemoveItemFromPool(worldItem);
				if (IsValidGridNo(gridNo)) renderDirty_ = TRUE;
				return MakeResult(command, TRUE, TRUE, "World item removed");
			}

			if (request.kind == OS0EditorRemoveKind::NPC)
			{
				if (request.soldierId == INVALID_NPC_ID ||
					request.soldierId >= TOTAL_SOLDIERS)
				{
					return MakeResult(command, FALSE, FALSE, "Invalid NPC handle");
				}
				SOLDIERTYPE& soldier = GetMan(request.soldierId);
				if (!soldier.bActive)
				{
					return MakeResult(command, FALSE, FALSE, "NPC handle is stale");
				}
				if (soldier.bTeam == OUR_TEAM)
				{
					return MakeResult(command, FALSE, FALSE,
						"Player soldiers cannot be removed by the map editor");
				}
				if ((request.expectedNpcTeam >= 0 &&
					soldier.bTeam != request.expectedNpcTeam) ||
					(IsValidGridNo(request.expectedNpcGridNo) &&
					soldier.sGridNo != request.expectedNpcGridNo))
				{
					return MakeResult(command, FALSE, FALSE,
						"NPC slot was reused before removal");
				}
				SOLDIERINITNODE* const node = FindSoldierInitNodeBySoldier(soldier);
				if (node == nullptr)
				{
					return MakeResult(command, FALSE, FALSE,
						"NPC has no persistent map placement");
				}
				INT16 const gridNo = soldier.sGridNo;
				RemoveSoldierNodeFromInitList(node);
				if (IsValidGridNo(gridNo)) renderDirty_ = TRUE;
				return MakeResult(command, TRUE, TRUE, "NPC removed");
			}

			if (!IsValidGridNo(request.gridNo) || request.tileIndex >= NUMBEROFTILES)
				return MakeResult(command, FALSE, FALSE, "Invalid tile handle");
			if (request.tileset >= 0 && request.tileset !=
				static_cast<INT16>(giCurrentTilesetID))
			{
				return MakeResult(command, FALSE, FALSE,
					"Tile handle belongs to a stale tileset catalog");
			}
			OS0EditorLayer const layer = request.layer == OS0EditorLayer::AUTO ?
				DefaultLayer(request.tileIndex) : request.layer;
			BOOLEAN removed = FALSE;
			AddToUndoList(request.gridNo);
			switch (layer)
			{
				case OS0EditorLayer::LAND:
					if (gpWorldLevelData[request.gridNo].pLandHead != nullptr &&
						gpWorldLevelData[request.gridNo].pLandHead->pNext == nullptr)
					{
						return MakeResult(command, FALSE, FALSE,
							"The final base-land tile cannot be removed");
					}
					if (FindNode(gpWorldLevelData[request.gridNo].pLandHead,
						request.tileIndex) != nullptr)
					{
						RemoveLand(request.gridNo, request.tileIndex);
						removed = TRUE;
					}
					break;
				case OS0EditorLayer::OBJECT:
					removed = RemoveObject(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::STRUCTURE:
				{
					LEVELNODE* const node = FindNode(
						gpWorldLevelData[request.gridNo].pStructHead, request.tileIndex);
					if (node != nullptr)
					{
						RemoveDoorShadow(request.gridNo, request.tileIndex);
						RemoveStructFromLevelNode(request.gridNo, node);
						TILE_ELEMENT const& tile = gTileDatabase[request.tileIndex];
						if (tile.fType == FIRSTCLIFF && tile.usRegionIndex <
							gNumTilesPerType[FIRSTCLIFFHANG])
						{
							RemoveObject(request.gridNo,
								gTileTypeStartIndex[FIRSTCLIFFHANG] + tile.usRegionIndex);
						}
						removed = TRUE;
					}
					break;
				}
				case OS0EditorLayer::SHADOW:
				{
					LEVELNODE* const node = FindNode(
						gpWorldLevelData[request.gridNo].pShadowHead, request.tileIndex);
					removed = node != nullptr &&
						RemoveShadowFromLevelNode(request.gridNo, node);
					break;
				}
				case OS0EditorLayer::ROOF:
					removed = RemoveRoof(request.gridNo, request.tileIndex); break;
				case OS0EditorLayer::ON_ROOF:
				{
					LEVELNODE* const node = FindNode(
						gpWorldLevelData[request.gridNo].pOnRoofHead, request.tileIndex);
					if (node != nullptr)
					{
						INT16 const buddy = gTileDatabase[request.tileIndex].sBuddyNum;
						if (buddy >= 0)
						{
							if (LEVELNODE* const buddyNode = FindNode(
								gpWorldLevelData[request.gridNo].pOnRoofHead,
								static_cast<UINT16>(buddy)))
							{
								RemoveOnRoofFromLevelNode(request.gridNo, buddyNode);
							}
						}
						removed = RemoveOnRoofFromLevelNode(request.gridNo, node);
					}
					break;
				}
				case OS0EditorLayer::TOPMOST:
				{
					LEVELNODE* const node = FindNode(
						gpWorldLevelData[request.gridNo].pTopmostHead, request.tileIndex);
					removed = node != nullptr &&
						RemoveTopmostFromLevelNode(request.gridNo, node);
					break;
				}
				case OS0EditorLayer::AUTO: break;
			}
			if (!removed)
				return MakeResult(command, FALSE, FALSE, "Tile handle is stale");
			dirtyGridNos_.push_back(request.gridNo);
			TILE_ELEMENT const& removedTile = gTileDatabase[request.tileIndex];
			if (layer == OS0EditorLayer::LAND ||
				(layer == OS0EditorLayer::OBJECT &&
					removedTile.ubTerrainID != NO_TERRAIN))
			{
				fullMovementRebuild_ = TRUE;
			}
			return MakeResult(command, TRUE, TRUE, "World asset removed");
		}

		case OS0EditorCommandType::SAVE_MAP:
		{
			auto const& request = std::get<OS0EditorSaveRequest>(command.payload);
			if (!gfWorldLoaded || GCM == nullptr || GCM->userPrivateFiles() == nullptr)
				return MakeResult(command, FALSE, FALSE, "No world is loaded");
			DirFs* const files = GCM->userPrivateFiles();
			files->createDir("OS0");
			files->createDir("OS0/maps");
			ST::string const relativePath = SafeMapPath(request.name);
			ST::string const temporaryPath = ST::format("{}.tmp-{}", relativePath,
				command.id);
			ST::string const backupPath = ST::format("{}.bak-{}", relativePath,
				command.id);
			BOOLEAN backupMade = FALSE;
			try
			{
				if (files->exists(temporaryPath)) files->deleteFile(temporaryPath);
				if (files->exists(backupPath)) files->deleteFile(backupPath);

				// Saving strips live wireframe nodes and may normalize land height.
				// Always schedule their restoration, including failed writes.
				fullWireframeRebuild_ = TRUE;
				renderDirty_ = TRUE;
				RaiseWorldLand();
				BOOLEAN serialized = FALSE;
				{
					AutoSGPFile file{ files->openForWriting(temporaryPath, true) };
					serialized = SaveWorldToSGPFile(file);
				}
				if (!serialized)
				{
					if (files->exists(temporaryPath)) files->deleteFile(temporaryPath);
					return MakeResult(command, FALSE, TRUE, "Map serialization failed");
				}

				if (files->isFile(relativePath))
				{
					files->moveFile(relativePath, backupPath);
					backupMade = TRUE;
				}
				try
				{
					files->moveFile(temporaryPath, relativePath);
				}
				catch (...)
				{
					if (backupMade && !files->exists(relativePath))
					{
						files->moveFile(backupPath, relativePath);
						backupMade = FALSE;
					}
					throw;
				}
				if (backupMade)
				{
					try { files->deleteFile(backupPath); }
					catch (...) { /* A harmless recovery copy may remain. */ }
				}
				return MakeResult(command, TRUE, TRUE,
					ST::format("Saved {}", files->absolutePath(relativePath)));
			}
			catch (std::exception const& error)
			{
				try
				{
					if (files->exists(temporaryPath)) files->deleteFile(temporaryPath);
					if (backupMade && !files->exists(relativePath))
						files->moveFile(backupPath, relativePath);
				}
				catch (...) { /* Preserve the original save error below. */ }
				return MakeResult(command, FALSE, TRUE,
					ST::format("Map save failed: {}", error.what()));
			}
		}

		case OS0EditorCommandType::REBUILD_CATALOGS:
			catalogInvalidated_ = TRUE;
			return MakeResult(command, TRUE, FALSE,
				"Catalog rebuild queued at the frame boundary");
	}
	return MakeResult(command, FALSE, FALSE, "Unknown editor command");
}
catch (std::exception const& error)
{
	BOOLEAN const worldChanged = renderDirty_ || fullWireframeRebuild_ ||
		fullMovementRebuild_ ||
		!dirtyGridNos_.empty();
	return MakeResult(command, FALSE, worldChanged,
		ST::format("Editor command failed: {}", error.what()));
}
catch (...)
{
	BOOLEAN const worldChanged = renderDirty_ || fullWireframeRebuild_ ||
		fullMovementRebuild_ ||
		!dirtyGridNos_.empty();
	return MakeResult(command, FALSE, worldChanged, "Editor command failed");
}

void OS0RealtimeEditorEngineAdapter::finalizeFrame()
{
	if (!gfWorldLoaded)
	{
		dirtyGridNos_.clear();
		fullWireframeRebuild_ = FALSE;
		fullMovementRebuild_ = FALSE;
		renderDirty_ = FALSE;
		return;
	}
	if (!fullWireframeRebuild_ && !fullMovementRebuild_ &&
		dirtyGridNos_.empty() && !renderDirty_)
	{
		return;
	}

	std::sort(dirtyGridNos_.begin(), dirtyGridNos_.end());
	dirtyGridNos_.erase(std::unique(dirtyGridNos_.begin(), dirtyGridNos_.end()),
		dirtyGridNos_.end());
	if (fullMovementRebuild_)
	{
		// Terrain ids are derived from the top object/land tile.  Local pathing
		// recompilation alone would keep stale terrain after painting or roads.
		CompileWorldMovementCosts();
	}
	if (fullWireframeRebuild_)
	{
		CalculateWorldWireFrameTiles(TRUE);
	}
	else if (!dirtyGridNos_.empty())
	{
		for (INT16 const gridNo : dirtyGridNos_)
		{
			if (!IsValidGridNo(gridNo)) continue;
			if (!fullMovementRebuild_)
				RecompileLocalMovementCostsFromRadius(gridNo, 4);
			MarkWireframeArea(gridNo, 2);
		}
		CalculateWorldWireFrameTiles(FALSE);
	}
	InvalidateWorldRedundency();
	SetRenderFlags(RENDER_FLAG_FULL);
	MarkWorldDirty();
	dirtyGridNos_.clear();
	fullWireframeRebuild_ = FALSE;
	fullMovementRebuild_ = FALSE;
	renderDirty_ = FALSE;
}

BOOLEAN OS0RealtimeEditorEngineAdapter::consumeCatalogInvalidation() noexcept
{
	BOOLEAN const result = catalogInvalidated_;
	catalogInvalidated_ = FALSE;
	return result;
}

template<typename Request>
std::uint64_t OS0RealtimeEditorSession::enqueue(OS0EditorCommandType const type,
	Request&& request)
{
	if (pending_.size() >= MAX_PENDING_COMMANDS)
	{
		status_.lastUpdateSucceeded = FALSE;
		status_.lastMessage = "Realtime editor command queue is full";
		return 0;
	}
	std::uint64_t const id = nextCommandId_++;
	pending_.push_back({ id, type,
		OS0EditorCommandPayload(std::forward<Request>(request)) });
	return id;
}

std::uint64_t OS0RealtimeEditorSession::queueNewBlankMap(
	OS0EditorBlankMapRequest request)
{
	return enqueue(OS0EditorCommandType::NEW_BLANK_MAP, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queueLoad(OS0EditorLoadRequest request)
{
	return enqueue(OS0EditorCommandType::LOAD_MAP, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queuePlaceTile(
	OS0EditorTilePlacement request)
{
	return enqueue(OS0EditorCommandType::PLACE_TILE, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queuePlaceItem(
	OS0EditorItemPlacement request)
{
	return enqueue(OS0EditorCommandType::PLACE_ITEM, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queuePlaceNpc(
	OS0EditorNpcPlacement request)
{
	return enqueue(OS0EditorCommandType::PLACE_NPC, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queuePaintTerrain(
	OS0EditorTerrainPaintRecipe request)
{
	return enqueue(OS0EditorCommandType::APPLY_RECIPE,
		OS0EditorRecipeRequest{ std::move(request) });
}

std::uint64_t OS0RealtimeEditorSession::queueSmoothTerrain(
	OS0EditorTerrainSmoothRecipe request)
{
	return enqueue(OS0EditorCommandType::APPLY_RECIPE,
		OS0EditorRecipeRequest{ std::move(request) });
}

std::uint64_t OS0RealtimeEditorSession::queuePlaceRoad(
	OS0EditorRoadRecipe request)
{
	return enqueue(OS0EditorCommandType::APPLY_RECIPE,
		OS0EditorRecipeRequest{ std::move(request) });
}

std::uint64_t OS0RealtimeEditorSession::queueRemove(
	OS0EditorRemoveRequest request)
{
	return enqueue(OS0EditorCommandType::REMOVE, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queueSave(OS0EditorSaveRequest request)
{
	return enqueue(OS0EditorCommandType::SAVE_MAP, std::move(request));
}

std::uint64_t OS0RealtimeEditorSession::queueRebuildCatalogs()
{
	return enqueue(OS0EditorCommandType::REBUILD_CATALOGS,
		OS0EditorCatalogRebuildRequest{});
}

void OS0RealtimeEditorSession::update()
{
	if (status_.processing) return;
	status_.processing = TRUE;
	try
	{
		if (pending_.empty())
		{
			status_.processing = FALSE;
			return;
		}
		// The tactical session ticks while the editor is hidden. Keep the expensive
		// tile/item/NPC catalog lazy until the editor is opened or a command actually
		// needs it, instead of adding a synchronous hitch to every ordinary game start.
		if (catalog_.generation == 0) rebuildCatalogs();

		status_.lastUpdateSucceeded = TRUE;
		std::deque<OS0EditorCommand> frameCommands;
		frameCommands.swap(pending_);
		BOOLEAN worldSwapWasAttempted = FALSE;
		for (OS0EditorCommand const& command : frameCommands)
		{
			OS0EditorCommandResult result;
			if (worldSwapWasAttempted)
			{
				result = MakeResult(command, FALSE, FALSE,
					"Command cancelled: requeue it from the refreshed world/catalog");
			}
			else
			{
				result = engine_.execute(command);
				if (command.type == OS0EditorCommandType::NEW_BLANK_MAP ||
					command.type == OS0EditorCommandType::LOAD_MAP)
				{
					// Even a failed attempt may have reached TrashWorld().  Never run
					// commands carrying handles from the pre-swap world afterwards.
					worldSwapWasAttempted = TRUE;
				}
			}
			status_.lastUpdateSucceeded =
				static_cast<BOOLEAN>(status_.lastUpdateSucceeded && result.success);
			status_.lastMessage = result.message;
			completed_.push_back(std::move(result));
		}
		engine_.finalizeFrame();
		if (engine_.consumeCatalogInvalidation()) rebuildCatalogs();
	}
	catch (std::exception const& error)
	{
		status_.lastUpdateSucceeded = FALSE;
		status_.lastMessage = ST::format("Realtime editor update failed: {}",
			error.what());
	}
	catch (...)
	{
		status_.lastUpdateSucceeded = FALSE;
		status_.lastMessage = "Realtime editor update failed";
	}
	status_.processing = FALSE;
}

void OS0RealtimeEditorSession::rebuildCatalogs()
{
	catalog_ = engine_.rebuildCatalogs(nextCatalogGeneration_++);
	status_.catalogGeneration = catalog_.generation;
}

std::vector<OS0EditorCommandResult> OS0RealtimeEditorSession::drainResults()
{
	std::vector<OS0EditorCommandResult> result;
	result.swap(completed_);
	return result;
}

BOOLEAN OS0RealtimeEditorSession::hasPendingWorldSwap() const noexcept
{
	return std::any_of(pending_.begin(), pending_.end(),
		[](OS0EditorCommand const& command)
		{
			return command.type == OS0EditorCommandType::NEW_BLANK_MAP ||
				command.type == OS0EditorCommandType::LOAD_MAP;
		});
}

BOOLEAN OS0RealtimeEditorSession::willInvalidateWorldPointers() const noexcept
{
	return hasPendingWorldSwap();
}

OS0RealtimeEditorSession& OS0GetRealtimeEditor()
{
	static OS0RealtimeEditorSession editor;
	return editor;
}
