/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "OS0_IngameUI.h"

#include "Animation_Control.h"
#include "ArmourModel.h"
#include "Campaign_Types.h"
#include "Cursors.h"
#include "Directories.h"
#include "Dialogue_Control.h"
#include "Faces.h"
#include "FileMan.h"
#include "Font.h"
#include "Font_Control.h"
#include "GameInstance.h"
#include "GameScreen.h"
#include "GameVersion.h"
#include "HImage.h"
#include "Handle_Items.h"
#include "Handle_UI.h"
#include "Input.h"
#include "ItemModel.h"
#include "Interface_Dialogue.h"
#include "Interface_Items.h"
#include "Interface_Panels.h"
#include "Isometric_Utils.h"
#include "Items.h"
#include "Lighting.h"
#include "Local.h"
#include "MercPortrait.h"
#include "MouseSystem.h"
#include "Overhead.h"
#include "PathAI.h"
#include "Physics.h"
#include "Points.h"
#include "Render_Dirty.h"
#include "RenderWorld.h"
#include "ScreenIDs.h"
#include "SaveLoadMap.h"
#include "SGPFile.h"
#include "Soldier_Profile.h"
#include "Soldier_Find.h"
#include "Soldier_Control.h"
#include "StrategicMap.h"
#include "Structure.h"
#include "Squads.h"
#include "Timer_Control.h"
#include "TileDef.h"
#include "UILayout.h"
#include "VObject.h"
#include "VObject_Blitters.h"
#include "VSurface.h"
#include "Video.h"
#include "Weapons.h"
#include "World_Items.h"
#include "WorldMan.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string_theory/format>
#include <vector>


namespace
{
	constexpr INT16 PANE_W = 465;
	constexpr INT16 BAG_H = 184;
	constexpr INT16 GRABBER_H = 16;
	constexpr INT16 TAB_Y = BAG_H - 22;
	constexpr INT16 INVENTORY_X = 145;
	constexpr INT16 CONTINUE_X = PANE_W - 125;
	constexpr INT16 PANEL_HEADER_H = 17;
	constexpr INT16 CONTEXT_PANEL_W = 190;
	constexpr INT16 CONTEXT_PANEL_H = 112;
	constexpr INT16 TOOLS_PANEL_W = 168;
	constexpr INT16 TOOLS_PANEL_H = 126;
	constexpr INT16 ACTIONS_PANEL_W = 194;
	constexpr INT16 ACTIONS_PANEL_H = 174;
	constexpr INT16 FEEDBACK_PANEL_W = 390;
	constexpr INT16 FEEDBACK_PANEL_H = 220;
	constexpr INT16 SECTOR_PANEL_W = 260;
	constexpr INT16 SECTOR_PANEL_H = 154;
	constexpr INT16 GOD_LIBRARY_W = 234;
	constexpr INT16 GOD_LIBRARY_H = 112;
	constexpr INT16 ASSET_CATALOG_W = 318;
	constexpr INT16 ASSET_CATALOG_H = 194;
	constexpr size_t PANEL_DOCK_COUNT = 7;

	enum class ResourceKind : UINT8
	{
		TIMBER,
		STONE,
		SCRAP,
		SOIL,
		COUNT
	};

	enum class ComputerMode
	{
		INFO,
		CONTENTS,
		BUILD,
		OPS
	};

	enum class ContentsMode
	{
		SOLDIER,
		WORLD
	};

	enum class ContextAction : UINT8
	{
		INSPECT,
		CONTENTS,
		BUILD,
		CARRY,
		TALK,
		ATTACK,
		STAND,
		CROUCH,
		PRONE,
		STEALTH,
		WEAPON_MODE,
		RELOAD,
		SWAP_HANDS,
		UNLOAD,
		DETAILS,
		EQUIP_ITEM,
		MOVE_ITEM,
		PICK_UP,
		DIG,
		SALVAGE,
		CATALOG
	};

	enum class AssetCategory : UINT8
	{
		UNKNOWN, SANDBAG, DOOR, CONTAINER, STONE, DEBRIS, FURNITURE,
		TREE, WALL, FLOOR, RESOURCE, DECOR, WORKSTATION, COUNT
	};

	enum class AssetMaterial : UINT8
	{
		AUTO, WOOD, STONE, METAL, SAND, EARTH, ORGANIC, FABRIC,
		COMPOSITE, COUNT
	};

	enum class AssetRole : UINT8
	{
		DECOR, SALVAGE, STORAGE, BUILDING_PART, CRAFTING_STATION,
		RESOURCE_NODE, BARRIER, COUNT
	};

	struct AssetCatalogRecord
	{
		INT16 tileset;
		UINT16 tileIndex;
		AssetCategory category;
		AssetMaterial material;
		AssetRole role;
		UINT8 width;
		UINT8 height;
		BOOLEAN buildable;
		ST::string label;
	};

	struct ContextEntry
	{
		ContextAction action;
		ST::string label;
		BOOLEAN enabled;
	};

	enum class FloatingPanelId : UINT8
	{
		CONTEXT,
		TOOLS,
		ACTIONS,
		OBJECT_INVENTORY,
		SECTOR,
		FEEDBACK,
		COUNT
	};

	struct SalvageProfile
	{
		ST::string displayName;
		ResourceKind resource;
		UINT8 amount;
		BOOLEAN salvageable;
	};

	struct SectorUpgrade
	{
		const char* name;
		const char* benefit;
		UINT8 timber;
		UINT8 stone;
		UINT8 scrap;
		UINT8 soil;
		UINT32 flag;
	};

	struct FloatingPanel
	{
		INT16 x;
		INT16 y;
		INT16 w;
		INT16 h;
		BOOLEAN visible;
		BOOLEAN dragging;
		BOOLEAN visibleBeforeAim;
	};

	struct SlotLayout
	{
		INT8 slot;
		INT16 x;
		INT16 y;
		INT16 w;
		INT16 h;
	};

	constexpr std::array<SlotLayout, NUM_INV_SLOTS> gSlots{{
		{ HELMETPOS,       INVENTORY_X + 128,  23, HEAD_INV_SLOT_WIDTH, HEAD_INV_SLOT_HEIGHT },
		{ VESTPOS,         INVENTORY_X + 128,  52, VEST_INV_SLOT_WIDTH, VEST_INV_SLOT_HEIGHT },
		{ LEGPOS,          INVENTORY_X + 128, 112, LEGS_INV_SLOT_WIDTH, LEGS_INV_SLOT_HEIGHT },
		{ HEAD1POS,        INVENTORY_X +  10,  23, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ HEAD2POS,        INVENTORY_X +  46,  23, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ HANDPOS,         INVENTORY_X +  10,  78, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ SECONDHANDPOS,   INVENTORY_X +  10, 105, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ BIGPOCK1POS,     INVENTORY_X + 252,  23, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ BIGPOCK2POS,     INVENTORY_X + 252,  50, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ BIGPOCK3POS,     INVENTORY_X + 252,  77, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ BIGPOCK4POS,     INVENTORY_X + 252, 104, BIG_INV_SLOT_WIDTH,  BIG_INV_SLOT_HEIGHT },
		{ SMALLPOCK1POS,   INVENTORY_X + 180,  23, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK2POS,   INVENTORY_X + 180,  50, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK3POS,   INVENTORY_X + 180,  77, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK4POS,   INVENTORY_X + 180, 104, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK5POS,   INVENTORY_X + 216,  23, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK6POS,   INVENTORY_X + 216,  50, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK7POS,   INVENTORY_X + 216,  77, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT },
		{ SMALLPOCK8POS,   INVENTORY_X + 216, 104, SM_INV_SLOT_WIDTH,   SM_INV_SLOT_HEIGHT }
	}};

	BOOLEAN gInitialized = FALSE;
	BOOLEAN gBagVisible = TRUE;
	BOOLEAN gInventoryVisible = FALSE;
	BOOLEAN gLootVisible = FALSE;
	BOOLEAN gContextVisible = FALSE;
	ContentsMode gContentsMode = ContentsMode::SOLDIER;
	BOOLEAN gHoverVisible = FALSE;
	BOOLEAN gBagDragging = FALSE;
	BOOLEAN gOrbDragging = FALSE;
	BOOLEAN gOrbMoved = FALSE;
	BOOLEAN gAimAutoCollapsed = FALSE;
	BOOLEAN gBagVisibleBeforeAim = FALSE;
	INT8 gLootDragCandidate = -1;
	INT16 gLootDragStartX = 0;
	INT16 gLootDragStartY = 0;
	UINT32 gLootIgnoreInputUntil = 0;
	UINT32 gPanelInteractionGuardUntil = 0;
	BOOLEAN gBagVisibleBeforeTalk = TRUE;
	BOOLEAN gTalkDocked = FALSE;
	BOOLEAN gFieldToolIssued = FALSE;
	INT16 gBagX = 300;
	INT16 gBagY = 20;
	INT16 gInventoryX = 650;
	INT16 gInventoryY = 20;
	INT16 gLootX = 300;
	INT16 gLootY = 180;
	INT16 gOrbX = 8;
	INT16 gOrbY = 316;
	INT16 gContextX = 0;
	INT16 gContextY = 0;
	INT16 gHoverX = 0;
	INT16 gHoverY = 0;
	INT16 gDragOffsetX = 0;
	INT16 gDragOffsetY = 0;
	ComputerMode gMode = ComputerMode::INFO;
	SOLDIERTYPE* gInspectedSoldier = nullptr;
	// The equipment window is an independent live view. World inspection and
	// container selection must never silently retarget or close it.
	SOLDIERTYPE* gInventorySoldier = nullptr;
	GridNo gInspectedGridNo = NOWHERE;
	UINT8 gInspectedLevel = 0;
	UINT16 gInspectedTileIndex = NO_TILE;
	BOOLEAN gTutorialActive = TRUE;
	UINT8 gTutorialStep = 0;
	ST::string gTutorialName = "Operator";
	INT16 gTutorialStatPoints = 100;
	std::array<INT8, 10> gTutorialStatValues{{ 55, 55, 55, 55, 55, 55, 55, 55, 55, 55 }};
	std::array<SkillTrait, 2> gTutorialTraits{{ NO_SKILLTRAIT, NO_SKILLTRAIT }};
	BOOLEAN gWorldMovePending = FALSE;
	BOOLEAN gWorldMoveWalking = FALSE;
	GridNo gWorldMoveSource = NOWHERE;
	GridNo gWorldMoveDestination = NOWHERE;
	GridNo gWorldMoveActionGrid = NOWHERE;
	UINT16 gWorldMoveTileIndex = NO_TILE;
	SOLDIERTYPE* gWorldMoveCarrier = nullptr;
	UINT8 gWorldMoveOldShade = DEFAULT_SHADE_LEVEL;
	BOOLEAN gWorldMoveSourceShaded = FALSE;
	UINT8 gWorldZoom = 1;
	UINT8 gCursorAction = 0;
	BOOLEAN gGodLibraryVisible = FALSE;
	BOOLEAN gAssetCatalogVisible = FALSE;
	BOOLEAN gAssetCatalogNameEditing = FALSE;
	UINT8 gGodMenuIcon = 0;
	INT16 gGodLibraryX = 0;
	INT16 gGodLibraryY = 0;
	INT16 gAssetCatalogX = 0;
	INT16 gAssetCatalogY = 0;
	AssetCatalogRecord gCatalogDraft{};
	std::vector<AssetCatalogRecord> gAssetCatalog;
	SGPVSurface* gWorldZoomBuffer = nullptr;
	std::array<FloatingPanel, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanels{{
		{ 8, 36, CONTEXT_PANEL_W, CONTEXT_PANEL_H, FALSE, FALSE, FALSE },
		{ 8, 154, TOOLS_PANEL_W, TOOLS_PANEL_H, TRUE, FALSE, FALSE },
		{ 438, 36, ACTIONS_PANEL_W, ACTIONS_PANEL_H, FALSE, FALSE, FALSE },
		{ 88, 118, PANE_W, BAG_H, FALSE, FALSE, FALSE },
		{ 190, 72, SECTOR_PANEL_W, SECTOR_PANEL_H, FALSE, FALSE, FALSE },
		{ 120, 70, FEEDBACK_PANEL_W, FEEDBACK_PANEL_H, FALSE, FALSE, FALSE }
	}};
	std::array<ContextEntry, 8> gPanelActionEntries;
	size_t gPanelActionEntryCount = 0;

	void OutlineBox(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour);
	void ContextActionCallback(MOUSE_REGION* region, UINT32 reason);
	void OperationsActionCallback(MOUSE_REGION* region, UINT32 reason);
	void GodIconCallback(MOUSE_REGION* region, UINT32 reason);
	void AssetCatalogCallback(MOUSE_REGION* region, UINT32 reason);
	void SetBagRegionsEnabled(BOOLEAN enabled);
	void PositionBagRegions();
	BOOLEAN AssetCatalogKeyboardHook(InputAtom* event);
	ST::string SerializeAssetCatalog();
	void RecordFeedbackEvent(const ST::string& event);
	BOOLEAN TutorialKeyboardHook(InputAtom* event);

	SGPVObject* gPortrait = nullptr;
	SGPVSurface* gInspectorPreview = nullptr;
	SGPVObject* gGodNewIcons = nullptr;
	SGPVObject* gGodDoorIcons = nullptr;
	SGPVObject* gGodButtonFrame = nullptr;
	MOUSE_REGION gGodLibraryBlock;
	std::array<MOUSE_REGION, 25> gGodIconRegions;
	MOUSE_REGION gAssetCatalogBlock;
	std::array<MOUSE_REGION, 11> gAssetCatalogRegions;
	MOUSE_REGION gContextBlock;
	ProfileID gPortraitProfile = NO_PROFILE;
	MOUSE_REGION gBagBlock;
	MOUSE_REGION gBagGrabber;
	MOUSE_REGION gBagClose;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelBlocks;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelGrabbers;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelCloses;
	std::array<MOUSE_REGION, PANEL_DOCK_COUNT> gPanelDockRegions;
	std::array<MOUSE_REGION, 10> gToolRegions;
	std::array<MOUSE_REGION, 8> gActionPanelRegions;
	std::array<MOUSE_REGION, 4> gFeedbackRegions;
	std::array<MOUSE_REGION, 3> gSectorUpgradeRegions;
	std::array<MOUSE_REGION, NUM_INV_SLOTS> gSlotRegions;
	std::array<MOUSE_REGION, NUM_INV_SLOTS> gObjectSlotRegions;
	MOUSE_REGION gOrbRegion;
	MOUSE_REGION gTutorialContinue;
	std::array<MOUSE_REGION, 20> gTutorialStats;
	std::array<MOUSE_REGION, 15> gTutorialTraitRegions;
	std::array<MOUSE_REGION, 5> gOpsActionRegions;
	std::array<MOUSE_REGION, 12> gContextRegions;
	std::array<MOUSE_REGION, 12> gLootRegions;
	std::array<INT32, 12> gLootWorldItems;
	std::array<ST::string, NUM_INV_SLOTS> gSlotHelp;
	std::array<ST::string, NUM_INV_SLOTS> gObjectSlotHelp;
	std::array<ST::string, 12> gLootHelp;
	GridNo gLootGridNo = NOWHERE;
	UINT8 gLootLevel = 0;
	UINT16 gLootTileIndex = NO_TILE;
	SOLDIERTYPE* gContextSoldier = nullptr;
	GridNo gContextGridNo = NOWHERE;
	UINT8 gContextLevel = 0;
	UINT16 gContextTileIndex = NO_TILE;
	INT8 gContextInventorySlot = NO_SLOT;
	INT32 gContextWorldItemIndex = -1;
	std::array<ContextEntry, 12> gContextEntries;
	size_t gContextEntryCount = 0;
	ST::string gContextTitle = "CONTEXT";
	ST::string gHoverTitle;
	ST::string gHoverDetail;
	UINT8 gFeedbackCategory = 0;
	BOOLEAN gFeedbackEditing = FALSE;
	ST::string gFeedbackText;
	ST::string gFeedbackStatus = "CLICK TEXT AREA TO WRITE";
	ST::string gFeedbackLastFile;
	std::array<ST::string, 20> gFeedbackEvents;
	size_t gFeedbackEventNext = 0;
	size_t gFeedbackEventCount = 0;

	constexpr std::array<const char*, 5> gFeedbackCategories{{
		"BUG", "CONTROLS", "VISUAL", "IDEA", "CRASH / STABILITY"
	}};

	constexpr std::array<const char*, 24> gGodIconNames{{
		"RUN", "WALK", "CROUCH", "PRONE", "LOOK", "CANCEL",
		"HAND / LOOT", "TALK", "TARGET", "KNIFE", "FIRST AID", "PUNCH",
		"EXPLOSIVE", "TOOLKIT", "WIRE CUTTER", "CROWBAR", "KEY",
		"KEYRING", "OPEN", "EXAMINE", "BREACH", "DISARM", "LOCKPICK", "BOOT"
	}};

	constexpr std::array<const char*, static_cast<size_t>(AssetCategory::COUNT)>
		gAssetCategoryNames{{
			"UNKNOWN", "SANDBAG", "DOOR", "CONTAINER", "STONE", "DEBRIS",
			"FURNITURE", "TREE", "WALL", "FLOOR", "RESOURCE", "DECOR",
			"WORKSTATION"
		}};
	constexpr std::array<const char*, static_cast<size_t>(AssetMaterial::COUNT)>
		gAssetMaterialNames{{
			"AUTO", "WOOD", "STONE", "METAL", "SAND", "EARTH",
			"ORGANIC", "FABRIC", "COMPOSITE"
		}};
	constexpr std::array<const char*, static_cast<size_t>(AssetRole::COUNT)>
		gAssetRoleNames{{
			"DECOR", "SALVAGE", "STORAGE", "BUILDING PART", "CRAFTING STATION",
			"RESOURCE NODE", "BARRIER"
		}};
	constexpr const char* ASSET_CATALOG_PATH = "AssetCatalog/os0-assets.tsv";
	constexpr const char* BUILTIN_ASSET_CATALOG_PATH = "os0-assets.tsv";

	constexpr UINT8 RESOURCE_BITS = 5;
	constexpr UINT32 RESOURCE_VALUE_MASK = (1u << RESOURCE_BITS) - 1u;
	constexpr std::array<UINT8, static_cast<size_t>(ResourceKind::COUNT)>
		gResourceShifts{{ 8, 13, 18, 23 }};
	constexpr UINT32 OS0_UPGRADE_SHELTER = 0x10000000u;
	constexpr UINT32 OS0_UPGRADE_WORKSHOP = 0x20000000u;
	constexpr UINT32 OS0_UPGRADE_DEPOT = 0x40000000u;
	constexpr UINT8 OS0_CONTAINER_MARKER = 0xE0;
	constexpr std::array<SectorUpgrade, 3> gSectorUpgrades{{
		{ "FIELD SHELTER", "RECOVERY NODE", 8, 4, 0, 4, OS0_UPGRADE_SHELTER },
		{ "SALVAGE WORKSHOP", "+1 SALVAGE YIELD", 4, 4, 8, 0, OS0_UPGRADE_WORKSHOP },
		{ "SECURE DEPOT", "+1 CONTAINER MATERIAL", 6, 3, 6, 2, OS0_UPGRADE_DEPOT }
	}};

	constexpr std::array<const char*, 10> gTutorialStatNames{{
		"HEALTH", "AGILITY", "DEXTERITY", "STRENGTH", "WISDOM",
		"LEADERSHIP", "MARKSMANSHIP", "MEDICAL", "MECHANICAL", "EXPLOSIVES"
	}};
	constexpr std::array<SkillTrait, 15> gTutorialTraitValues{{
		LOCKPICKING, HANDTOHAND, ELECTRONICS, NIGHTOPS, THROWING,
		TEACHING, HEAVY_WEAPS, AUTO_WEAPS, STEALTHY, AMBIDEXT,
		THIEF, MARTIALARTS, KNIFING, ONROOF, CAMOUFLAGED
	}};
	constexpr std::array<const char*, 15> gTutorialTraitNames{{
		"LOCKPICKING", "HAND TO HAND", "ELECTRONICS", "NIGHT OPS", "THROWING",
		"TEACHING", "HEAVY WEAPONS", "AUTO WEAPONS", "STEALTH", "AMBIDEXTROUS",
		"THIEF", "MARTIAL ARTS", "KNIFING", "ROOFTOP", "CAMOUFLAGE"
	}};

	void MoveRegion(MOUSE_REGION& r, INT16 x, INT16 y)
	{
		const INT16 w = r.W();
		const INT16 h = r.H();
		r.RegionTopLeftX = x;
		r.RegionTopLeftY = y;
		r.RegionBottomRightX = x + w;
		r.RegionBottomRightY = y + h;
	}

	INT16 PanelDockStartX()
	{
		const INT16 dockWidth = static_cast<INT16>(PANEL_DOCK_COUNT * 44);
		if (gOrbX + 34 + dockWidth <= gsVIEWPORT_END_X) return gOrbX + 34;
		return std::max<INT16>(0, gOrbX - dockWidth - 6);
	}

	BOOLEAN CanAccessSoldierContents(SOLDIERTYPE const* soldier)
	{
		if (!soldier) return FALSE;
		if (soldier->bTeam == OUR_TEAM) return TRUE;
		SOLDIERTYPE const* const selected = GetSelectedMan();
		return selected &&
			PythSpacesAway(selected->sGridNo, soldier->sGridNo) <= 2 &&
			(soldier->bLife < OKLIFE || soldier->uiStatusFlags & SOLDIER_DEAD);
	}

	UINT32& CurrentSectorFacilities()
	{
		return SectorInfo[gWorldSector.AsByte()].uiFacilitiesFlags;
	}

	UINT16 ResourceItem(ResourceKind kind)
	{
		switch (kind)
		{
			case ResourceKind::TIMBER: return OS0_TIMBER;
			case ResourceKind::STONE:  return OS0_STONE;
			case ResourceKind::SCRAP:  return OS0_SCRAP;
			case ResourceKind::SOIL:   return OS0_SOIL;
			case ResourceKind::COUNT:  break;
		}
		return NOTHING;
	}

	const char* ResourceName(ResourceKind kind)
	{
		switch (kind)
		{
			case ResourceKind::TIMBER: return "TIMBER";
			case ResourceKind::STONE:  return "STONE";
			case ResourceKind::SCRAP:  return "SCRAP";
			case ResourceKind::SOIL:   return "SOIL";
			case ResourceKind::COUNT:  break;
		}
		return "MATERIAL";
	}

	BOOLEAN IsResourceItem(UINT16 item)
	{
		return item == OS0_TIMBER || item == OS0_STONE ||
			item == OS0_SCRAP || item == OS0_SOIL;
	}

	ResourceKind ResourceFromItem(UINT16 item)
	{
		if (item == OS0_TIMBER) return ResourceKind::TIMBER;
		if (item == OS0_STONE) return ResourceKind::STONE;
		if (item == OS0_SCRAP) return ResourceKind::SCRAP;
		return ResourceKind::SOIL;
	}

	UINT8 SectorResource(ResourceKind kind)
	{
		const UINT8 shift = gResourceShifts[static_cast<size_t>(kind)];
		return static_cast<UINT8>((CurrentSectorFacilities() >> shift) &
			RESOURCE_VALUE_MASK);
	}

	void SetSectorResource(ResourceKind kind, UINT8 value)
	{
		const UINT8 shift = gResourceShifts[static_cast<size_t>(kind)];
		const UINT32 mask = RESOURCE_VALUE_MASK << shift;
		UINT32& facilities = CurrentSectorFacilities();
		facilities = (facilities & ~mask) |
			((static_cast<UINT32>(std::min<UINT8>(value,
				static_cast<UINT8>(RESOURCE_VALUE_MASK)))) << shift);
	}

	void AddSectorResource(ResourceKind kind, UINT8 amount)
	{
		SetSectorResource(kind, static_cast<UINT8>(std::min<UINT16>(
			RESOURCE_VALUE_MASK, SectorResource(kind) + amount)));
	}

	BOOLEAN HasUpgrade(SectorUpgrade const& upgrade)
	{
		return (CurrentSectorFacilities() & upgrade.flag) != 0;
	}

	BOOLEAN CanBuildUpgrade(SectorUpgrade const& upgrade)
	{
		return !HasUpgrade(upgrade) &&
			SectorResource(ResourceKind::TIMBER) >= upgrade.timber &&
			SectorResource(ResourceKind::STONE) >= upgrade.stone &&
			SectorResource(ResourceKind::SCRAP) >= upgrade.scrap &&
			SectorResource(ResourceKind::SOIL) >= upgrade.soil;
	}

	BOOLEAN BuildSectorUpgrade(size_t index)
	{
		if (index >= gSectorUpgrades.size()) return FALSE;
		SectorUpgrade const& upgrade = gSectorUpgrades[index];
		if (!CanBuildUpgrade(upgrade)) return FALSE;
		SetSectorResource(ResourceKind::TIMBER,
			SectorResource(ResourceKind::TIMBER) - upgrade.timber);
		SetSectorResource(ResourceKind::STONE,
			SectorResource(ResourceKind::STONE) - upgrade.stone);
		SetSectorResource(ResourceKind::SCRAP,
			SectorResource(ResourceKind::SCRAP) - upgrade.scrap);
		SetSectorResource(ResourceKind::SOIL,
			SectorResource(ResourceKind::SOIL) - upgrade.soil);
		CurrentSectorFacilities() |= upgrade.flag;
		RecordFeedbackEvent(ST::format("SECTOR UPGRADE {} {}",
			gWorldSector.AsShortString(), upgrade.name));
		return TRUE;
	}

	const char* ContextActionName(ContextAction action)
	{
		switch (action)
		{
			case ContextAction::INSPECT: return "INSPECT";
			case ContextAction::CONTENTS: return "CONTENTS";
			case ContextAction::BUILD: return "BUILD";
			case ContextAction::CARRY: return "CARRY";
			case ContextAction::TALK: return "TALK";
			case ContextAction::ATTACK: return "ATTACK";
			case ContextAction::STAND: return "STAND";
			case ContextAction::CROUCH: return "CROUCH";
			case ContextAction::PRONE: return "PRONE";
			case ContextAction::STEALTH: return "STEALTH";
			case ContextAction::WEAPON_MODE: return "WEAPON_MODE";
			case ContextAction::RELOAD: return "RELOAD";
			case ContextAction::SWAP_HANDS: return "SWAP_HANDS";
			case ContextAction::UNLOAD: return "UNLOAD";
			case ContextAction::DETAILS: return "DETAILS";
			case ContextAction::EQUIP_ITEM: return "EQUIP_ITEM";
			case ContextAction::MOVE_ITEM: return "MOVE_ITEM";
			case ContextAction::PICK_UP: return "PICK_UP";
			case ContextAction::DIG: return "DIG";
			case ContextAction::SALVAGE: return "SALVAGE";
			case ContextAction::CATALOG: return "CATALOG";
		}
		return "UNKNOWN";
	}

	void RecordFeedbackEvent(const ST::string& event)
	{
		try
		{
			gFeedbackEvents[gFeedbackEventNext] = ST::format("{}ms  {}",
				GetJA2Clock(), event);
		}
		catch (...)
		{
			// Diagnostics must never be capable of terminating gameplay.
			gFeedbackEvents[gFeedbackEventNext] = event;
		}
		gFeedbackEventNext = (gFeedbackEventNext + 1) % gFeedbackEvents.size();
		gFeedbackEventCount = std::min(gFeedbackEventCount + 1,
			gFeedbackEvents.size());
	}

	ST::string ReadEngineLogTail()
	{
		const char* temp = std::getenv("TEMP");
		if (!temp) temp = std::getenv("TMPDIR");
		if (!temp) return "Engine log path unavailable.";
		try
		{
			const ST::string path = FileMan::joinPaths(temp, "ja2.log");
			AutoSGPFile file{ FileMan::openForReading(path) };
			constexpr UINT32 MAX_LOG_BYTES = 32768;
			const UINT32 size = file->size();
			if (size > MAX_LOG_BYTES)
				file->seek(static_cast<INT32>(size - MAX_LOG_BYTES), FILE_SEEK_FROM_START);
			return file->readStringToEnd();
		}
		catch (...)
		{
			return "Engine log could not be read while the game was running.";
		}
	}

	BOOLEAN WriteFeedbackReport()
	{
		try
		{
			std::time_t const now = std::time(nullptr);
			std::tm local{};
			if (std::tm const* const value = std::localtime(&now)) local = *value;
			char stamp[32]{};
			std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
			const ST::string relativePath = ST::format(
				"Feedback/os0-feedback-{}.txt", stamp);
			GCM->userPrivateFiles()->createDir("Feedback");

			SOLDIERTYPE const* const selected = GetSelectedMan();
			ST::string report = ST::format(
				"ESCAPE FROM ARULCO - PLAYTEST FEEDBACK\n"
				"======================================\n"
				"Created: {}\n"
				"Version: {} / {}\n"
				"Category: {}\n"
				"Sector: {}\n"
				"Runtime: {} ms\n"
				"World zoom: {}\n"
				"Cursor tool: {}\n\n",
				stamp, g_version_label, g_version_number,
				gFeedbackCategories[gFeedbackCategory],
				gWorldSector.AsShortString(), GetJA2Clock(), gWorldZoom,
				gCursorAction);
			if (selected)
			{
				report += ST::format(
					"Selected merc: {}\nGrid: {} / level {}\nHP: {}/{} / AP: {}\n\n",
					selected->name, selected->sGridNo, selected->bLevel,
					selected->bLife, selected->bLifeMax, selected->bActionPoints);
			}
			report += ST::format(
				"Inspected target: {}\nGrid: {} / level {} / tile {}\n"
				"Panels: character={} context={} tools={} actions={} object={}\n\n"
				"TESTER DESCRIPTION\n------------------\n{}\n\n"
				"RECENT OS0 EVENTS\n-----------------\n",
				gContextTitle, gInspectedGridNo, gInspectedLevel,
				gInspectedTileIndex, gBagVisible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLS)].visible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible,
				gFeedbackText.empty() ? "(No description entered.)" : gFeedbackText);

			for (size_t i = 0; i < gFeedbackEventCount; ++i)
			{
				const size_t index = (gFeedbackEventNext + gFeedbackEvents.size() -
					gFeedbackEventCount + i) % gFeedbackEvents.size();
				report += gFeedbackEvents[index] + "\n";
			}
			report += "\nASSET CATALOG SNAPSHOT\n----------------------\n";
			report += SerializeAssetCatalog();
			report += "\nENGINE LOG (LAST 32 KB)\n-----------------------\n";
			report += ReadEngineLogTail();
			report += "\n";

			AutoSGPFile file{ GCM->userPrivateFiles()->openForWriting(relativePath, true) };
			file->write(report.c_str(), report.size());
			gFeedbackLastFile = ST::format("os0-feedback-{}.txt", stamp);
			gFeedbackStatus = "SAVED - SEND THIS TXT FILE";
			RecordFeedbackEvent(ST::format("FEEDBACK SAVED {}", gFeedbackLastFile));
			return TRUE;
		}
		catch (...)
		{
			gFeedbackStatus = "SAVE FAILED - CHECK WRITE ACCESS";
			return FALSE;
		}
	}

	AssetCatalogRecord* FindAssetCatalogRecord(INT16 tileset, UINT16 tileIndex)
	{
		for (AssetCatalogRecord& record : gAssetCatalog)
			if (record.tileset == tileset && record.tileIndex == tileIndex)
				return &record;
		return nullptr;
	}

	AssetCatalogRecord const* FindAssetCatalogRecordConst(INT16 tileset,
		UINT16 tileIndex)
	{
		for (AssetCatalogRecord const& record : gAssetCatalog)
			if (record.tileset == tileset && record.tileIndex == tileIndex)
				return &record;
		return nullptr;
	}

	ST::string SerializeAssetCatalog()
	{
		ST::string output =
			"# Escape from Arulco asset catalog v1\n"
			"# tileset tile category material role width height buildable label\n";
		for (AssetCatalogRecord const& record : gAssetCatalog)
		{
			output += ST::format("{} {} {} {} {} {} {} {}\t{}\n",
				record.tileset, record.tileIndex,
				static_cast<UINT8>(record.category),
				static_cast<UINT8>(record.material),
				static_cast<UINT8>(record.role), record.width, record.height,
				record.buildable ? 1 : 0, record.label);
		}
		return output;
	}

	BOOLEAN WriteAssetCatalog()
	{
		try
		{
			GCM->userPrivateFiles()->createDir("AssetCatalog");
			ST::string const output = SerializeAssetCatalog();
			AutoSGPFile file{
				GCM->userPrivateFiles()->openForWriting(ASSET_CATALOG_PATH, true) };
			file->write(output.c_str(), output.size());
			return TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}

	void LoadAssetCatalog()
	{
		gAssetCatalog.clear();
		auto merge = [](const ST::string& contents)
		{
			std::istringstream stream(contents.c_str());
			std::string line;
			while (std::getline(stream, line))
			{
				if (line.empty() || line[0] == '#') continue;
				std::istringstream row(line);
				int tileset, tile, category, material, role, width, height, buildable;
				if (!(row >> tileset >> tile >> category >> material >> role >>
					width >> height >> buildable)) continue;
				std::string label;
				std::getline(row, label);
				const size_t first = label.find_first_not_of(" \t");
				if (first != std::string::npos) label.erase(0, first);
				else label = "UNNAMED ASSET";
				if (tile < 0 || tile >= NUMBEROFTILES ||
					category < 0 || category >= static_cast<int>(AssetCategory::COUNT) ||
					material < 0 || material >= static_cast<int>(AssetMaterial::COUNT) ||
					role < 0 || role >= static_cast<int>(AssetRole::COUNT)) continue;
				AssetCatalogRecord parsed{ static_cast<INT16>(tileset),
					static_cast<UINT16>(tile), static_cast<AssetCategory>(category),
					static_cast<AssetMaterial>(material), static_cast<AssetRole>(role),
					static_cast<UINT8>(std::clamp(width, 1, 12)),
					static_cast<UINT8>(std::clamp(height, 1, 12)),
					buildable != 0, ST::string(label) };
				if (AssetCatalogRecord* existing = FindAssetCatalogRecord(
					parsed.tileset, parsed.tileIndex)) *existing = parsed;
				else gAssetCatalog.push_back(parsed);
			}
		};
		try
		{
			if (GCM->doesGameResExists(BUILTIN_ASSET_CATALOG_PATH))
			{
				AutoSGPFile builtIn{
					GCM->openGameResForReading(BUILTIN_ASSET_CATALOG_PATH) };
				merge(builtIn->readStringToEnd());
			}
		}
		catch (...)
		{
			RecordFeedbackEvent("BUILT-IN ASSET CATALOG LOAD FAILED");
		}
		try
		{
			AutoSGPFile user{
				GCM->userPrivateFiles()->openForReading(ASSET_CATALOG_PATH) };
			merge(user->readStringToEnd());
		}
		catch (...)
		{
			// The user catalog is created by the first SAVE; absence is normal.
		}
	}

	void StopFeedbackEditing()
	{
		if (!gFeedbackEditing) return;
		gFeedbackEditing = FALSE;
		SetUIKeyboardHook(gTutorialActive && gTutorialStep == 1 ?
			TutorialKeyboardHook : nullptr);
	}

	BOOLEAN FeedbackKeyboardHook(InputAtom* event)
	{
		const FloatingPanel& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::FEEDBACK)];
		if (!panel.visible || !gFeedbackEditing) return FALSE;
		if (event->usEvent == TEXT_INPUT)
		{
			for (char32_t c : event->codepoints)
			{
				if (gFeedbackText.to_utf32().size() >= 800) break;
				if (c >= U' ' && c != U'<' && c != U'>') gFeedbackText += c;
			}
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usEvent != KEY_DOWN && event->usEvent != KEY_REPEAT) return TRUE;
		if (event->usParam == SDLK_BACKSPACE)
		{
			ST::utf32_buffer chars = gFeedbackText.to_utf32();
			if (!chars.empty())
				gFeedbackText = ST::string::from_utf32(chars.data(), chars.size() - 1);
		}
		else if (event->usParam == SDLK_RETURN)
		{
			if (gFeedbackText.to_utf32().size() < 800) gFeedbackText += '\n';
		}
		else if (event->usParam == SDLK_ESCAPE)
		{
			StopFeedbackEditing();
		}
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	BOOLEAN AssetCatalogKeyboardHook(InputAtom* event)
	{
		if (!gAssetCatalogVisible || !gAssetCatalogNameEditing) return FALSE;
		if (event->usEvent == TEXT_INPUT)
		{
			for (char32_t c : event->codepoints)
			{
				if (gCatalogDraft.label.to_utf32().size() >= 28) break;
				if (c >= U' ' && c != U'\t' && c != U'<' && c != U'>')
					gCatalogDraft.label += c;
			}
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usEvent != KEY_DOWN && event->usEvent != KEY_REPEAT) return TRUE;
		if (event->usParam == SDLK_BACKSPACE)
		{
			ST::utf32_buffer chars = gCatalogDraft.label.to_utf32();
			if (!chars.empty())
				gCatalogDraft.label = ST::string::from_utf32(
					chars.data(), chars.size() - 1);
		}
		else if (event->usParam == SDLK_RETURN || event->usParam == SDLK_ESCAPE)
		{
			gAssetCatalogNameEditing = FALSE;
			SetUIKeyboardHook(nullptr);
		}
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	STRUCTURE* WorldStructureAt(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX) return nullptr;
		LEVELNODE* node = level == 0 ? gpWorldLevelData[gridNo].pStructHead :
			gpWorldLevelData[gridNo].pOnRoofHead;
		for (; node; node = node->pNext)
		{
			if (node->pStructureData &&
				(tileIndex >= NUMBEROFTILES || node->usIndex == tileIndex))
			{
				return FindBaseStructure(node->pStructureData);
			}
		}
		return nullptr;
	}

	UINT16 CanonicalAssetTileIndex(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		STRUCTURE* const base = WorldStructureAt(gridNo, level, tileIndex);
		if (!base || base->sGridNo < 0 || base->sGridNo >= WORLD_MAX) return tileIndex;
		LEVELNODE* node = level == 0 ? gpWorldLevelData[base->sGridNo].pStructHead :
			gpWorldLevelData[base->sGridNo].pOnRoofHead;
		for (; node; node = node->pNext)
		{
			if (node->pStructureData &&
				FindBaseStructure(node->pStructureData) == base &&
				node->pStructureData->fFlags & STRUCTURE_BASE_TILE)
				return node->usIndex;
		}
		return tileIndex;
	}

	AssetMaterial InferAssetMaterial(STRUCTURE const* structure)
	{
		if (!structure) return AssetMaterial::AUTO;
		const ST::string material = GetWorldPhysicsProfile(structure).materialName;
		if (material == "WOOD" || material == "FURNITURE") return AssetMaterial::WOOD;
		if (material == "STONE" || material == "CERAMIC") return AssetMaterial::STONE;
		if (material == "LIGHT METAL" || material == "HEAVY METAL") return AssetMaterial::METAL;
		if (material == "SAND") return AssetMaterial::SAND;
		if (material == "ORGANIC") return AssetMaterial::ORGANIC;
		if (material == "CLOTH") return AssetMaterial::FABRIC;
		return AssetMaterial::COMPOSITE;
	}

	AssetCatalogRecord MakeDefaultCatalogRecord(GridNo gridNo, UINT8 level,
		UINT16 tileIndex)
	{
		AssetCatalogRecord record{
			static_cast<INT16>(giCurrentTilesetID),
			CanonicalAssetTileIndex(gridNo, level, tileIndex),
			AssetCategory::UNKNOWN, AssetMaterial::AUTO, AssetRole::DECOR,
			1, 1, FALSE, "UNNAMED ASSET"
		};
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		if (!structure)
		{
			if (tileIndex < NUMBEROFTILES)
			{
				switch (GetTileType(tileIndex))
				{
					case DEBRISROCKS:
						record.category = AssetCategory::STONE;
						record.material = AssetMaterial::STONE;
						record.role = AssetRole::RESOURCE_NODE;
						break;
					case DEBRISWOOD:
					case DEBRISWEEDS:
					case DEBRISGRASS:
					case DEBRISSAND:
					case DEBRISMISC:
					case DEBRIS2MISC:
						record.category = AssetCategory::DEBRIS;
						record.role = AssetRole::SALVAGE;
						break;
					default: break;
				}
			}
			record.label = gAssetCategoryNames[static_cast<size_t>(record.category)];
			return record;
		}

		record.material = InferAssetMaterial(structure);
		if (structure->fFlags & STRUCTURE_ANYDOOR)
		{
			record.category = AssetCategory::DOOR;
			record.role = AssetRole::BUILDING_PART;
		}
		else if (structure->fFlags & STRUCTURE_OPENABLE)
		{
			record.category = AssetCategory::CONTAINER;
			record.role = AssetRole::STORAGE;
			record.buildable = TRUE;
		}
		else if (structure->fFlags & STRUCTURE_TREE)
		{
			record.category = AssetCategory::TREE;
			record.role = AssetRole::RESOURCE_NODE;
		}
		else if (structure->fFlags & STRUCTURE_WALLSTUFF)
		{
			record.category = AssetCategory::WALL;
			record.role = AssetRole::BUILDING_PART;
		}
		else if (record.material == AssetMaterial::SAND)
		{
			record.category = AssetCategory::SANDBAG;
			record.role = AssetRole::BARRIER;
			record.buildable = TRUE;
		}
		else
		{
			record.category = AssetCategory::FURNITURE;
			record.role = AssetRole::SALVAGE;
		}

		if (structure->pDBStructureRef && structure->pDBStructureRef->pDBStructure)
		{
			INT8 minX = 0, maxX = 0, minY = 0, maxY = 0;
			for (DB_STRUCTURE_TILE const* tile : structure->pDBStructureRef->Tiles())
			{
				if (!tile) continue;
				minX = std::min(minX, tile->bXPosRelToBase);
				maxX = std::max(maxX, tile->bXPosRelToBase);
				minY = std::min(minY, tile->bYPosRelToBase);
				maxY = std::max(maxY, tile->bYPosRelToBase);
			}
			record.width = static_cast<UINT8>(std::clamp<INT16>(maxX - minX + 1, 1, 12));
			record.height = static_cast<UINT8>(std::clamp<INT16>(maxY - minY + 1, 1, 12));
		}
		record.label = gAssetCategoryNames[static_cast<size_t>(record.category)];
		return record;
	}

	void OpenAssetCatalog(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX || tileIndex >= NUMBEROFTILES) return;
		const UINT16 catalogTile = CanonicalAssetTileIndex(gridNo, level, tileIndex);
		if (AssetCatalogRecord const* const existing = FindAssetCatalogRecordConst(
			static_cast<INT16>(giCurrentTilesetID), catalogTile))
			gCatalogDraft = *existing;
		else
			gCatalogDraft = MakeDefaultCatalogRecord(gridNo, level, tileIndex);
		gAssetCatalogVisible = TRUE;
		gGodLibraryVisible = FALSE;
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	LEVELNODE* WorldLevelNodeAt(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX || tileIndex >= NUMBEROFTILES)
			return nullptr;
		LEVELNODE* node = level == 0 ? gpWorldLevelData[gridNo].pStructHead :
			gpWorldLevelData[gridNo].pOnRoofHead;
		for (; node; node = node->pNext)
		{
			if (node->usIndex == tileIndex && node->pStructureData)
				return node;
		}
		return nullptr;
	}

	void RestoreWorldMoveShade()
	{
		if (!gWorldMoveSourceShaded) return;
		if (LEVELNODE* const node = WorldLevelNodeAt(gWorldMoveSource, 0,
			gWorldMoveTileIndex))
		{
			node->ubShadeLevel = gWorldMoveOldShade;
		}
		gWorldMoveSourceShaded = FALSE;
	}

	void ShadeWorldMoveSource()
	{
		RestoreWorldMoveShade();
		if (LEVELNODE* const node = WorldLevelNodeAt(gWorldMoveSource, 0,
			gWorldMoveTileIndex))
		{
			// Interactive-tile hover temporarily uses shade 0. Never preserve that
			// highlight as the object's post-carry lighting value.
			gWorldMoveOldShade = std::max(node->ubShadeLevel,
				node->ubNaturalShadeLevel);
			node->ubShadeLevel = SHADE_MIN;
			gWorldMoveSourceShaded = TRUE;
		}
	}

	void ClearWorldMoveState()
	{
		RestoreWorldMoveShade();
		gWorldMovePending = FALSE;
		gWorldMoveWalking = FALSE;
		gWorldMoveSource = NOWHERE;
		gWorldMoveDestination = NOWHERE;
		gWorldMoveActionGrid = NOWHERE;
		gWorldMoveTileIndex = NO_TILE;
		gWorldMoveCarrier = nullptr;
	}

	GridNo FindCarryActionGrid(SOLDIERTYPE* carrier, GridNo destination)
	{
		if (!carrier) return NOWHERE;
		constexpr std::array<WorldDirections, 4> directions{{
			NORTH, EAST, SOUTH, WEST
		}};
		GridNo best = NOWHERE;
		INT16 bestCost = 32767;
		for (WorldDirections direction : directions)
		{
			const GridNo candidate = NewGridNo(destination, DirectionInc(direction));
			if (candidate == destination || candidate < 0 || candidate >= WORLD_MAX)
				continue;
			if (candidate == carrier->sGridNo) return candidate;
			if (NewOKDestination(carrier, candidate, TRUE,
				carrier->bLevel) <= 0) continue;
			const INT16 cost = PlotPath(carrier, candidate, NO_COPYROUTE, NO_PLOT,
				carrier->usUIMovementMode, 0);
			if (cost > 0 && cost < bestCost)
			{
				best = candidate;
				bestCost = cost;
			}
		}
		return best;
	}

	BOOLEAN IsInspectedWorldAssetNear()
	{
		SOLDIERTYPE const* const selected = GetSelectedMan();
		return selected && gInspectedGridNo >= 0 && gInspectedGridNo < WORLD_MAX &&
			PythSpacesAway(selected->sGridNo, gInspectedGridNo) <= 2;
	}

	BOOLEAN IsWorldAssetMovableAt(GridNo gridNo, UINT8 level, UINT16 tileIndex,
		SOLDIERTYPE const* carrier)
	{
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		return carrier && gridNo >= 0 && gridNo < WORLD_MAX && level == 0 &&
			PythSpacesAway(carrier->sGridNo, gridNo) <= 2 && structure &&
			structure->fFlags & STRUCTURE_BASE_TILE &&
			CanSoldierMoveWorldStructure(carrier, structure);
	}

	BOOLEAN IsInspectedWorldAssetMovable()
	{
		return IsWorldAssetMovableAt(gInspectedGridNo, gInspectedLevel,
			gInspectedTileIndex, GetSelectedMan());
	}

	BOOLEAN BeginInspectedWorldMove()
	{
		if (!IsInspectedWorldAssetNear() || !IsInspectedWorldAssetMovable() ||
			gInspectedTileIndex >= NUMBEROFTILES) return FALSE;
		ClearWorldMoveState();
		gCursorAction = 2;
		gWorldMovePending = TRUE;
		gWorldMoveSource = gInspectedGridNo;
		gWorldMoveTileIndex = gInspectedTileIndex;
		gWorldMoveCarrier = GetSelectedMan();
		ShadeWorldMoveSource();
		gLootVisible = FALSE;
		guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
		return TRUE;
	}

	void CaptureInspectorPreview(GridNo gridNo, UINT8 level)
	{
		if (!gInspectorPreview)
			gInspectorPreview = AddVideoSurface(64, 64, PIXEL_DEPTH);
		ColorFillVideoSurfaceArea(gInspectorPreview, 0, 0, 63, 63,
			Get16BPPColor(FROMRGB(4, 7, 7)));
		INT16 previewX;
		INT16 previewY;
		GetGridNoScreenPos(gridNo, level, &previewX, &previewY);
		OS0MapWorldToDisplayScreen(&previewX, &previewY);
		const SGPBox source{
			static_cast<UINT16>(std::clamp<INT16>(
				previewX - 32, 0, std::max<INT16>(0, SCREEN_WIDTH - 64))),
			static_cast<UINT16>(std::clamp<INT16>(
				previewY - 32, 0, std::max<INT16>(0, SCREEN_HEIGHT - 64))),
			64, 64
		};
		// Events run after the previous frame was composed. Do not recursively
		// capture an OS//0 window when it happens to cover the inspected asset.
		const auto overlaps = [&source](INT16 x, INT16 y, INT16 w, INT16 h)
		{
			return source.x < x + w && source.x + source.w > x &&
				source.y < y + h && source.y + source.h > y;
		};
		if ((gBagVisible && overlaps(gBagX, gBagY, PANE_W, BAG_H)) ||
			overlaps(gOrbX, gOrbY, 28, 28) ||
			(gContextVisible && overlaps(gContextX, gContextY, 168,
				static_cast<INT16>(20 + gContextEntryCount * 18)))) return;
		BltVideoSurface(gInspectorPreview, FRAME_BUFFER, 0, 0, &source);
	}

	UINT16 ResolveWorldTileIndex(GridNo gridNo, UINT8 level, UINT16 supplied)
	{
		if (supplied < NUMBEROFTILES) return supplied;
		if (gridNo < 0 || gridNo >= WORLD_MAX) return NO_TILE;
		LEVELNODE* node = level == 0 ?
			gpWorldLevelData[gridNo].pStructHead :
			gpWorldLevelData[gridNo].pOnRoofHead;
		LEVELNODE* fallback = nullptr;
		for (; node; node = node->pNext)
		{
			if (node->usIndex < NUMBEROFTILES && node->pStructureData &&
				!(node->pStructureData->fFlags & STRUCTURE_PERSON))
			{
				if (!fallback) fallback = node;
				const StructureFlags flags = node->pStructureData->fFlags;
				if (flags & (STRUCTURE_OPENABLE | STRUCTURE_MOBILE | STRUCTURE_GENERIC))
					return node->usIndex;
			}
		}
		if (fallback) return fallback->usIndex;

		// Scenery such as rubble, furniture details and resource props often
		// lives on the object layer without STRUCTURE data. Make those assets
		// selectable too, while excluding JA2's item-pool nodes and floor tiles.
		for (node = gpWorldLevelData[gridNo].pObjectHead; node; node = node->pNext)
		{
			if (node->usIndex < NUMBEROFTILES &&
				!(node->uiFlags & (LEVELNODE_ITEM | LEVELNODE_HIDDEN)))
				return node->usIndex;
		}
		return NO_TILE;
	}

	void CloseContextMenu()
	{
		gContextVisible = FALSE;
		gHoverVisible = FALSE;
		gContextEntryCount = 0;
		gContextInventorySlot = NO_SLOT;
		gContextWorldItemIndex = -1;
		gContextBlock.Disable();
		for (MOUSE_REGION& r : gContextRegions) r.Disable();
	}

	void AddContextEntry(ContextAction action, const ST::string& label,
		BOOLEAN enabled = TRUE)
	{
		if (gContextEntryCount >= gContextEntries.size()) return;
		gContextEntries[gContextEntryCount++] = { action, label, enabled };
	}

	void AddPanelAction(ContextAction action, const ST::string& label,
		BOOLEAN enabled = TRUE)
	{
		if (gPanelActionEntryCount >= gPanelActionEntries.size()) return;
		gPanelActionEntries[gPanelActionEntryCount++] = { action, label, enabled };
	}

	const char* TerrainPhysicsName(TerrainTypeDefines terrain)
	{
		switch (terrain)
		{
			case LOW_GRASS:  return "LOW GRASS / TOPSOIL";
			case HIGH_GRASS: return "HIGH GRASS / TOPSOIL";
			case DIRT_ROAD:  return "COMPACTED SOIL";
			case FLAT_GROUND:return "EXPOSED SOIL";
			case FLAT_FLOOR: return "FLOOR / FOUNDATION";
			case PAVED_ROAD: return "PAVED SURFACE";
			case TRAIN_TRACKS:return "TRACK BED";
			case LOW_WATER:
			case MED_WATER:
			case DEEP_WATER: return "WATER / SATURATED";
			default:         return "UNKNOWN GROUND";
		}
	}

	SalvageProfile DescribeWorldAsset(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		SalvageProfile result{ "WORLD ASSET / FIXED", ResourceKind::SCRAP, 0, FALSE };
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		if (AssetCatalogRecord const* const custom = FindAssetCatalogRecordConst(
			static_cast<INT16>(giCurrentTilesetID),
			CanonicalAssetTileIndex(gridNo, level, tileIndex)))
		{
			AssetMaterial material = custom->material;
			if (material == AssetMaterial::AUTO) material = InferAssetMaterial(structure);
			ResourceKind resource = ResourceKind::SCRAP;
			if (material == AssetMaterial::WOOD || material == AssetMaterial::ORGANIC ||
				material == AssetMaterial::FABRIC) resource = ResourceKind::TIMBER;
			else if (material == AssetMaterial::STONE || material == AssetMaterial::SAND)
				resource = ResourceKind::STONE;
			else if (material == AssetMaterial::EARTH) resource = ResourceKind::SOIL;
			const BOOLEAN salvageable = custom->role == AssetRole::SALVAGE ||
				custom->role == AssetRole::RESOURCE_NODE;
			const UINT8 amount = salvageable ? static_cast<UINT8>(std::clamp<INT16>(
				custom->width * custom->height, 1, 8)) : 0;
			return { custom->label.empty() ?
				gAssetCategoryNames[static_cast<size_t>(custom->category)] : custom->label,
				resource, amount, salvageable };
		}
		if (!structure)
		{
			if (tileIndex >= NUMBEROFTILES) return result;
			switch (GetTileType(tileIndex))
			{
				case DEBRISROCKS:
					return { "LOOSE STONE DEPOSIT", ResourceKind::STONE, 2, TRUE };
				case DEBRISWOOD:
					return { "WOOD DEBRIS", ResourceKind::TIMBER, 2, TRUE };
				case DEBRISWEEDS:
				case DEBRISGRASS:
				case DEBRISSAND:
					return { "ORGANIC GROUND DEBRIS", ResourceKind::SOIL, 1, TRUE };
				case DEBRISMISC:
				case DEBRIS2MISC:
				case FIRSTEXPLDEBRIS:
				case SECONDEXPLDEBRIS:
					return { "BROKEN SCRAP DEBRIS", ResourceKind::SCRAP, 2, TRUE };
				default:
					return result;
			}
		}

		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		ST::string const material = physics.materialName;
		ResourceKind resource = ResourceKind::SCRAP;
		if (material == "WOOD" || material == "FURNITURE" ||
			material == "ORGANIC") resource = ResourceKind::TIMBER;
		else if (material == "STONE" || material == "CERAMIC" ||
			material == "SAND") resource = ResourceKind::STONE;
		const UINT8 amount = static_cast<UINT8>(std::clamp<INT32>(
			static_cast<INT32>(physics.massKg / 20.0f) + 1, 1, 8));
		const char* adjective = resource == ResourceKind::TIMBER ? "WOODEN" :
			resource == ResourceKind::STONE ? "STONE" : "METAL";

		if (structure->fFlags & STRUCTURE_ANYDOOR)
			return { ST::format("{} DOOR", adjective), resource, amount, TRUE };
		if (structure->fFlags & STRUCTURE_TREE)
			return { "TREE / TIMBER SOURCE", ResourceKind::TIMBER, amount, TRUE };
		if (structure->fFlags & STRUCTURE_ANYFENCE)
			return { ST::format("{} FENCE", adjective), resource, amount, TRUE };
		if (structure->fFlags & STRUCTURE_OPENABLE)
			return { ST::format("{} CONTAINER", adjective), resource, amount, TRUE };
		if (structure->fFlags & (STRUCTURE_WALLSTUFF | STRUCTURE_ROOF |
			STRUCTURE_SWITCH | STRUCTURE_VEHICLE | STRUCTURE_LIGHTSOURCE))
			return { ST::format("{} STRUCTURE / FIXED", adjective), resource, 0, FALSE };
		if (structure->fFlags & STRUCTURE_BASE_TILE &&
			structure->pDBStructureRef &&
			structure->pDBStructureRef->pDBStructure->ubNumberOfTiles == 1)
			return { ST::format("{} RESOURCE OBJECT", adjective), resource, amount, TRUE };
		return { ST::format("{} WORLD STRUCTURE", adjective), resource, 0, FALSE };
	}

	void AddResourceItemToPool(GridNo gridNo, UINT8 level, ResourceKind kind,
		UINT8 amount)
	{
		if (amount == 0 || kind == ResourceKind::COUNT) return;
		OBJECTTYPE resource{};
		CreateItems(ResourceItem(kind), 100,
			std::min<UINT8>(amount, MAX_OBJECTS_PER_SLOT), &resource);
		AddItemToPool(gridNo, &resource, HIDDEN_IN_OBJECT, level, 0, -1);
	}

	BOOLEAN IsContainerSeedMarker(WORLDITEM const& item)
	{
		return item.fExists && item.o.usItem == ACTION_ITEM &&
			item.o.bActionValue == 0 && item.o.ubTolerance == OS0_CONTAINER_MARKER;
	}

	void EnsureContainerLoot(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		if (!structure || !(structure->fFlags & STRUCTURE_OPENABLE) ||
			structure->fFlags & STRUCTURE_ANYDOOR) return;

		BOOLEAN marked = FALSE;
		BOOLEAN ordinaryLoot = FALSE;
		for (ITEM_POOL* item = GetItemPool(gridNo, level); item; item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (IsContainerSeedMarker(worldItem)) marked = TRUE;
			else if (worldItem.fExists && worldItem.o.usItem != NOTHING &&
				worldItem.o.usItem != OWNERSHIP && worldItem.o.usItem != ACTION_ITEM)
				ordinaryLoot = TRUE;
		}
		if (marked) return;

		const UINT32 hash = static_cast<UINT32>(gridNo) * 2654435761u ^
			static_cast<UINT32>(tileIndex) * 2246822519u ^
			static_cast<UINT32>(gWorldSector.AsByte()) * 3266489917u;
		SalvageProfile const asset = DescribeWorldAsset(gridNo, level, tileIndex);
		UINT8 primary = static_cast<UINT8>(1 + hash % 3);
		if (CurrentSectorFacilities() & OS0_UPGRADE_DEPOT) ++primary;
		AddResourceItemToPool(gridNo, level, asset.resource, primary);
		AddResourceItemToPool(gridNo, level,
			(hash & 1) ? ResourceKind::SCRAP : ResourceKind::SOIL,
			static_cast<UINT8>(1 + (hash >> 4) % 2));

		if (!ordinaryLoot || (hash % 3) == 0)
		{
			constexpr std::array<UINT16, 8> useful{{
				CANTEEN, FIRSTAIDKIT, ALCOHOL, BREAK_LIGHT,
				WIRECUTTERS, TOOLKIT, CROWBAR, LOCKSMITHKIT
			}};
			OBJECTTYPE object{};
			CreateItem(useful[(hash >> 8) % useful.size()],
				static_cast<INT8>(45 + (hash >> 12) % 51), &object);
			AddItemToPool(gridNo, &object, HIDDEN_IN_OBJECT, level, 0, -1);
		}

		OBJECTTYPE marker{};
		CreateItem(ACTION_ITEM, 100, &marker);
		marker.bActionValue = 0;
		marker.ubTolerance = OS0_CONTAINER_MARKER;
		AddItemToPool(gridNo, &marker, HIDDEN_ITEM, level, 0, -1);
		RecordFeedbackEvent(ST::format("CONTAINER SEEDED grid {} tile {}",
			gridNo, tileIndex));
	}

	BOOLEAN StoreResourceWorldItem(INT32 itemIndex)
	{
		if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
			return FALSE;
		WORLDITEM& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || !IsResourceItem(worldItem.o.usItem)) return FALSE;
		ResourceKind const kind = ResourceFromItem(worldItem.o.usItem);
		const UINT8 amount = worldItem.o.ubNumberOfObjects;
		if (amount == 0 || SectorResource(kind) + amount > RESOURCE_VALUE_MASK)
			return FALSE;
		AddSectorResource(kind, amount);
		RemoveItemFromPool(worldItem);
		RecordFeedbackEvent(ST::format("STOCKPILE +{} {}", amount,
			ResourceName(kind)));
		return TRUE;
	}

	BOOLEAN StoreResourceInventoryItem(SOLDIERTYPE* soldier, INT8 slot)
	{
		if (!soldier || slot < 0 || slot >= NUM_INV_SLOTS ||
			!IsResourceItem(soldier->inv[slot].usItem)) return FALSE;
		OBJECTTYPE& object = soldier->inv[slot];
		ResourceKind const kind = ResourceFromItem(object.usItem);
		const UINT8 amount = object.ubNumberOfObjects;
		if (amount == 0 || SectorResource(kind) + amount > RESOURCE_VALUE_MASK)
			return FALSE;
		AddSectorResource(kind, amount);
		DeleteObj(&object);
		RecordFeedbackEvent(ST::format("STOCKPILE +{} {} FROM PACK", amount,
			ResourceName(kind)));
		return TRUE;
	}

	BOOLEAN HasDiggingTool(SOLDIERTYPE const* soldier)
	{
		// Vanilla has no shovel item ID. CROWBAR is the save-compatible field-tool
		// fallback until the dedicated OS//0 shovel asset is externalized.
		return soldier && FindUsableObj(soldier, CROWBAR) != NO_SLOT;
	}

	void EnsureFieldShovel(SOLDIERTYPE* soldier)
	{
		if (!soldier || HasDiggingTool(soldier)) return;
		OBJECTTYPE shovel{};
		CreateItem(CROWBAR, 100, &shovel);
		AutoPlaceObject(soldier, &shovel, TRUE);
	}

	BOOLEAN CanDigTerrainAt(SOLDIERTYPE const* soldier, GridNo gridNo)
	{
		if (!soldier || gridNo < 0 || gridNo >= WORLD_MAX || soldier->bLevel != 0 ||
			PythSpacesAway(soldier->sGridNo, gridNo) > 2 || !HasDiggingTool(soldier) ||
			!gpWorldLevelData[gridNo].pLandHead) return FALSE;
		TerrainTypeDefines const terrain = GetTerrainType(gridNo);
		return terrain == FLAT_GROUND || terrain == DIRT_ROAD ||
			terrain == LOW_GRASS || terrain == HIGH_GRASS;
	}

	BOOLEAN CanSalvageWorldAsset(SOLDIERTYPE const* soldier, GridNo gridNo,
		UINT8 level, UINT16 tileIndex)
	{
		if (!soldier || !HasDiggingTool(soldier) || level != 0 ||
			gridNo < 0 || gridNo >= WORLD_MAX ||
			PythSpacesAway(soldier->sGridNo, gridNo) > 2 ||
			tileIndex >= NUMBEROFTILES) return FALSE;
		return DescribeWorldAsset(gridNo, level, tileIndex).salvageable;
	}

	BOOLEAN SalvageWorldAsset(SOLDIERTYPE* soldier, GridNo gridNo,
		UINT8 level, UINT16 tileIndex)
	{
		if (!CanSalvageWorldAsset(soldier, gridNo, level, tileIndex)) return FALSE;
		SalvageProfile profile = DescribeWorldAsset(gridNo, level, tileIndex);
		GridNo dropGrid = gridNo;
		BOOLEAN removed = FALSE;
		ApplyMapChangesToMapTempFile recordChange;
		if (STRUCTURE* const structure = WorldStructureAt(gridNo, level, tileIndex))
		{
			STRUCTURE* const base = FindBaseStructure(structure);
			if (!base) return FALSE;
			dropGrid = base->sGridNo;
			LEVELNODE* const node = FindLevelNodeBasedOnStructure(base);
			if (!node) return FALSE;
			RemoveStructFromLevelNode(dropGrid, node);
			removed = TRUE;
		}
		else
		{
			removed = RemoveObject(gridNo, tileIndex);
		}
		if (!removed) return FALSE;

		UINT8 amount = profile.amount;
		if (CurrentSectorFacilities() & OS0_UPGRADE_WORKSHOP)
			amount = std::min<UINT8>(MAX_OBJECTS_PER_SLOT,
				static_cast<UINT8>(amount + 1));
		AddResourceItemToPool(dropGrid, level, profile.resource, amount);
		DeductPoints(soldier, 10, 180);
		RecompileLocalMovementCosts(dropGrid);
		InvalidateWorldRedundency();
		gLootGridNo = dropGrid;
		gLootLevel = level;
		gLootTileIndex = NO_TILE;
		gInspectedGridNo = dropGrid;
		gInspectedLevel = level;
		gInspectedTileIndex = NO_TILE;
		gContextTitle = ST::format("SALVAGE / {}", ResourceName(profile.resource));
		gContentsMode = ContentsMode::WORLD;
		gLootVisible = TRUE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = TRUE;
		RecordFeedbackEvent(ST::format("SALVAGE {} +{} {} grid {}",
			profile.displayName, amount, ResourceName(profile.resource), dropGrid));
		return TRUE;
	}

	BOOLEAN DigTerrainAt(SOLDIERTYPE* soldier, GridNo gridNo, UINT16 tileIndex)
	{
		if (!CanDigTerrainAt(soldier, gridNo)) return FALSE;
		LEVELNODE* const surface = gpWorldLevelData[gridNo].pLandHead;
		if (!surface) return FALSE;
		const UINT16 oldIndex = surface->usIndex;
		const UINT32 oldType = GetTileType(oldIndex);
		const BOOLEAN hasBuriedLayer = surface->pNext != nullptr;

		ApplyMapChangesToMapTempFile recordChange;
		BOOLEAN changed = FALSE;
		if (tileIndex < NUMBEROFTILES)
		{
			for (LEVELNODE const* object = gpWorldLevelData[gridNo].pObjectHead;
				object; object = object->pNext)
			{
				if (object->usIndex == tileIndex &&
					!(object->uiFlags & (LEVELNODE_ITEM | LEVELNODE_HIDDEN)))
				{
					changed = RemoveObject(gridNo, tileIndex);
					break;
				}
			}
		}

		// A single FIRSTTEXTURE tile already represents exposed mineral soil. A
		// deeper voxel/pit layer is the next milestone; never delete the mandatory
		// base node. Surface overlays are peeled off, otherwise grass/road ground is
		// replaced with the current tileset's bare-soil texture.
		if (hasBuriedLayer || oldType != FIRSTTEXTURE)
		{
			RemoveLandFromMapTempFile(gridNo, oldIndex);
			RemoveLand(gridNo, oldIndex);
			changed = TRUE;
			if (!hasBuriedLayer)
			{
				const UINT16 soilIndex = static_cast<UINT16>(
					gTileTypeStartIndex[FIRSTTEXTURE] + (gridNo % 10));
				AddLandToHead(gridNo, soilIndex);
				AddLandToMapTempFile(gridNo, soilIndex);
			}
		}
		if (!changed) return FALSE;

		ResourceKind const yield = oldType == FIRSTTEXTURE ?
			ResourceKind::STONE : ResourceKind::SOIL;
		AddResourceItemToPool(gridNo, 0, yield,
			static_cast<UINT8>(1 + (gridNo % 2)));
		DeductPoints(soldier, 8, 120);
		RecompileLocalMovementCosts(gridNo);
		InvalidateWorldRedundency();
		SetRenderFlags(RENDER_FLAG_FULL);
		RecordFeedbackEvent(ST::format("DIG SUCCESS grid {} old tile {}",
			gridNo, oldIndex));
		gLootGridNo = gridNo;
		gLootLevel = 0;
		gLootTileIndex = NO_TILE;
		gContentsMode = ContentsMode::WORLD;
		gLootVisible = TRUE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = TRUE;
		return TRUE;
	}

	void RefreshPanelActions()
	{
		gPanelActionEntryCount = 0;
		SOLDIERTYPE* const subject = gInspectedSoldier;
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (subject)
		{
			const BOOLEAN own = subject->bTeam == OUR_TEAM;
			AddPanelAction(ContextAction::INSPECT, "INSPECT / SHEET");
			AddPanelAction(ContextAction::CONTENTS,
				own ? "OPEN CHARACTER" : "LOOT / CONTENTS",
				CanAccessSoldierContents(subject));
			if (own)
			{
				AddPanelAction(ContextAction::STAND, "STANCE / STAND");
				AddPanelAction(ContextAction::CROUCH, "STANCE / CROUCH");
				AddPanelAction(ContextAction::PRONE, "STANCE / PRONE");
				AddPanelAction(ContextAction::STEALTH,
					subject->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON");
				const OBJECTTYPE& hand = subject->inv[HANDPOS];
				if (hand.usItem != NOTHING &&
					GCM->getItem(hand.usItem)->getItemClass() == IC_GUN)
				{
					AddPanelAction(ContextAction::WEAPON_MODE, "WEAPON MODE");
					AddPanelAction(ContextAction::RELOAD, "RELOAD WEAPON");
				}
			}
			else
			{
				AddPanelAction(ContextAction::TALK, "TALK", subject->bLife >= OKLIFE);
				const BOOLEAN armed = selected &&
					selected->inv[HANDPOS].usItem != NOTHING &&
					GCM->getItem(selected->inv[HANDPOS].usItem)->isWeapon();
				AddPanelAction(ContextAction::ATTACK, "ATTACK", armed);
			}
			return;
		}

		if (gInspectedGridNo < 0 || gInspectedGridNo >= WORLD_MAX) return;
		const BOOLEAN near = IsInspectedWorldAssetNear();
		const BOOLEAN hasItems = GetItemPool(gInspectedGridNo, gInspectedLevel) != nullptr;
		const BOOLEAN hasAsset = gInspectedTileIndex < NUMBEROFTILES;
		const BOOLEAN hasTerrain = gInspectedLevel == 0 &&
			gpWorldLevelData[gInspectedGridNo].pLandHead != nullptr;
		AddPanelAction(ContextAction::INSPECT, "INSPECT / INFO");
		if (hasItems || hasAsset)
			AddPanelAction(ContextAction::CONTENTS, "OPEN OBJECT INVENTORY", near);
		if (hasItems)
			AddPanelAction(ContextAction::PICK_UP,
				near ? "PICK UP" : "APPROACH & PICK UP");
		if (hasAsset)
		{
			AddPanelAction(ContextAction::CATALOG, "GOD / CATALOG ASSET");
			STRUCTURE const* const structure = WorldStructureAt(gInspectedGridNo,
				gInspectedLevel, gInspectedTileIndex);
			if (structure)
			{
				WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
				const BOOLEAN movable = near && IsInspectedWorldAssetMovable();
				AddPanelAction(ContextAction::CARRY,
					!physics.portableObject ? "CARRY / FIXED" :
					movable ? "CARRY / REPOSITION" : "CARRY / TOO HEAVY",
					movable);
				AssetCatalogRecord const* const catalog = FindAssetCatalogRecordConst(
					static_cast<INT16>(giCurrentTilesetID), CanonicalAssetTileIndex(
						gInspectedGridNo, gInspectedLevel, gInspectedTileIndex));
				AddPanelAction(ContextAction::BUILD,
					catalog && catalog->buildable ?
						"BLUEPRINT / PLACEABLE" : "TOOLS / REQUIREMENTS");
			}
			SalvageProfile const salvage = DescribeWorldAsset(gInspectedGridNo,
				gInspectedLevel, gInspectedTileIndex);
			if (salvage.salvageable)
			{
				const BOOLEAN tool = HasDiggingTool(selected);
				AddPanelAction(ContextAction::SALVAGE,
					!tool ? "DISMANTLE / NEED FIELD TOOL" :
					ST::format("DISMANTLE / +{} {}", salvage.amount,
						ResourceName(salvage.resource)),
					CanSalvageWorldAsset(selected, gInspectedGridNo,
						gInspectedLevel, gInspectedTileIndex));
			}
		}
		if (hasTerrain && (!hasAsset || !WorldStructureAt(gInspectedGridNo,
			gInspectedLevel, gInspectedTileIndex)))
		{
			const BOOLEAN tool = HasDiggingTool(selected);
			const BOOLEAN diggable = CanDigTerrainAt(selected, gInspectedGridNo);
			AddPanelAction(ContextAction::DIG,
				!tool ? "DIG / NEED FIELD SHOVEL" :
				diggable ? "DIG / REMOVE SURFACE" : "DIG / GROUND EXPOSED",
				diggable);
		}
	}

	void PositionContextRegions()
	{
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		MoveRegion(gContextBlock, gContextX, gContextY);
		gContextBlock.RegionBottomRightY = gContextY + height;
		for (size_t i = 0; i < gContextRegions.size(); ++i)
		{
			MoveRegion(gContextRegions[i], gContextX + 4,
				gContextY + 17 + static_cast<INT16>(i) * 18);
		}
	}

	void GetWorldZoomRects(SGPBox& source, SGPBox& destination)
	{
		destination = {
			gsVIEWPORT_START_X,
			gsVIEWPORT_WINDOW_START_Y,
			static_cast<UINT16>(gsVIEWPORT_END_X - gsVIEWPORT_START_X),
			static_cast<UINT16>(gsVIEWPORT_WINDOW_END_Y - gsVIEWPORT_WINDOW_START_Y)
		};
		source.w = destination.w / gWorldZoom;
		source.h = destination.h / gWorldZoom;
		source.x = std::clamp<INT16>(
			g_ui.m_tacticalMapCenterX - source.w / 2,
			destination.x, destination.x + destination.w - source.w);
		source.y = std::clamp<INT16>(
			g_ui.m_tacticalMapCenterY - source.h / 2,
			destination.y, destination.y + destination.h - source.h);
	}

	void SetBagRegionsEnabled(BOOLEAN enabled)
	{
		auto setVisible = [enabled](MOUSE_REGION& r, BOOLEAN visible)
		{
			if (enabled && visible) r.Enable();
			else r.Disable();
		};
		setVisible(gBagBlock, gBagVisible);
		setVisible(gBagGrabber, gBagVisible);
		// Keep close control active as long as the panel is visible.
		setVisible(gBagClose, gBagVisible);
		setVisible(gContextBlock, gContextVisible);
		setVisible(gGodLibraryBlock, gGodLibraryVisible);
		for (MOUSE_REGION& r : gGodIconRegions)
			setVisible(r, gGodLibraryVisible);
		setVisible(gAssetCatalogBlock, gAssetCatalogVisible);
		for (MOUSE_REGION& r : gAssetCatalogRegions)
			setVisible(r, gAssetCatalogVisible);
		const BOOLEAN showContentInventory = gBagVisible && !gContextVisible;
		const FloatingPanel& objectPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)];
		const FloatingPanel& feedbackPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::FEEDBACK)];
		const BOOLEAN showContentLoot = objectPanel.visible && !gContextVisible;
		for (size_t i = 0; i < gFloatingPanels.size(); ++i)
		{
			const BOOLEAN visible = gFloatingPanels[i].visible &&
				(!gTutorialActive || i == static_cast<size_t>(FloatingPanelId::FEEDBACK));
			setVisible(gFloatingPanelBlocks[i], visible);
			setVisible(gFloatingPanelGrabbers[i], visible);
			setVisible(gFloatingPanelCloses[i], visible);
		}
		for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
		{
			const BOOLEAN tutorialFeedback = gTutorialActive &&
				i == static_cast<size_t>(FloatingPanelId::FEEDBACK) + 1;
			setVisible(gPanelDockRegions[i], !gAimAutoCollapsed &&
				(!gTutorialActive || tutorialFeedback));
		}
		const FloatingPanel& toolsPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLS)];
		for (MOUSE_REGION& r : gToolRegions)
			setVisible(r, toolsPanel.visible && !gContextVisible && !gTutorialActive);
		const FloatingPanel& actionsPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)];
		for (size_t i = 0; i < gActionPanelRegions.size(); ++i)
			setVisible(gActionPanelRegions[i], actionsPanel.visible && !gContextVisible &&
				!gTutorialActive && i < gPanelActionEntryCount);
		for (MOUSE_REGION& r : gFeedbackRegions)
			setVisible(r, feedbackPanel.visible && !gContextVisible);
		const FloatingPanel& sectorPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		for (MOUSE_REGION& r : gSectorUpgradeRegions)
			setVisible(r, sectorPanel.visible && !gContextVisible && !gTutorialActive);
		for (size_t i = 0; i < gContextRegions.size(); ++i)
		{
			if (enabled && gContextVisible && i < gContextEntryCount)
				gContextRegions[i].Enable();
			else
				gContextRegions[i].Disable();
		}
		for (MOUSE_REGION& r : gSlotRegions)
		{
			if (enabled && gBagVisible &&
				(!gTutorialActive || gTutorialStep == 4) &&
				!gContextVisible &&
				showContentInventory &&
				CanAccessSoldierContents(gInventorySoldier ?
					gInventorySoldier : GetSelectedMan())) r.Enable();
			else r.Disable();
		}
		for (MOUSE_REGION& r : gObjectSlotRegions)
		{
			if (enabled && objectPanel.visible && !gContextVisible &&
				gContentsMode == ContentsMode::SOLDIER && gInspectedSoldier &&
				gInspectedSoldier->bTeam != OUR_TEAM &&
				CanAccessSoldierContents(gInspectedSoldier)) r.Enable();
			else r.Disable();
		}
		for (MOUSE_REGION& r : gLootRegions)
		{
			if (enabled && gLootVisible && !gContextVisible && showContentLoot)
				r.Enable();
			else r.Disable();
		}
		for (MOUSE_REGION& r : gOpsActionRegions)
		{
			r.Disable();
		}
		if (enabled && gTutorialActive && !gContextVisible && !feedbackPanel.visible)
		{
			gTutorialContinue.Enable();
			for (MOUSE_REGION& r : gTutorialStats)
			{
				if (gTutorialStep == 2) r.Enable();
				else r.Disable();
			}
			for (MOUSE_REGION& r : gTutorialTraitRegions)
			{
				if (gTutorialStep == 3) r.Enable();
				else r.Disable();
			}
		}
		else
		{
			gTutorialContinue.Disable();
			for (MOUSE_REGION& r : gTutorialStats) r.Disable();
			for (MOUSE_REGION& r : gTutorialTraitRegions) r.Disable();
		}
	}

	void PositionBagRegions()
	{
		MoveRegion(gBagBlock, gBagX, gBagY);
		MoveRegion(gBagGrabber, gBagX, gBagY);
		MoveRegion(gBagClose, gBagX + PANE_W - 16, gBagY + 1);
		gInventoryX = gBagX;
		gInventoryY = gBagY;
		FloatingPanel& objectPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)];
		gLootX = objectPanel.x;
		gLootY = objectPanel.y;
		for (size_t i = 0; i < gSlots.size(); ++i)
		{
			MoveRegion(gSlotRegions[i], gInventoryX + gSlots[i].x, gInventoryY + gSlots[i].y);
			MoveRegion(gObjectSlotRegions[i], gLootX + gSlots[i].x, gLootY + gSlots[i].y);
		}
		MoveRegion(gTutorialContinue, gBagX + CONTINUE_X, gBagY + BAG_H - 24);
		for (size_t i = 0; i < gTutorialStats.size(); ++i)
		{
			const size_t stat = i / 2;
			const INT16 row = static_cast<INT16>(stat % 5);
			const INT16 controlX = (i % 2 == 0) ? 92 : 143;
			const INT16 columnX = stat < 5 ? 0 : 230;
			MoveRegion(gTutorialStats[i], gBagX + columnX + controlX,
				gBagY + 24 + row * 18);
		}
		for (size_t i = 0; i < gTutorialTraitRegions.size(); ++i)
		{
			const BOOLEAN right = i >= 8;
			const INT16 row = static_cast<INT16>(right ? i - 8 : i);
			MoveRegion(gTutorialTraitRegions[i],
				gBagX + (right ? 230 : 0) + 14,
				gBagY + 24 + row * 14);
		}
		for (size_t i = 0; i < gOpsActionRegions.size(); ++i)
		{
			MoveRegion(gOpsActionRegions[i], gBagX + 14,
				gBagY + 26 + static_cast<INT16>(i) * 20);
		}
		for (size_t i = 0; i < gLootRegions.size(); ++i)
		{
			const INT16 column = static_cast<INT16>(i % 6);
			const INT16 row = static_cast<INT16>(i / 6);
			MoveRegion(gLootRegions[i],
				gLootX + 14 + column * 62,
				gLootY + 25 + row * 31);
		}
		for (size_t i = 0; i < gFloatingPanels.size(); ++i)
		{
			const FloatingPanel& panel = gFloatingPanels[i];
			MoveRegion(gFloatingPanelBlocks[i], panel.x, panel.y);
			gFloatingPanelBlocks[i].RegionBottomRightX = panel.x + panel.w;
			gFloatingPanelBlocks[i].RegionBottomRightY = panel.y + panel.h;
			MoveRegion(gFloatingPanelGrabbers[i], panel.x, panel.y);
			gFloatingPanelGrabbers[i].RegionBottomRightX = panel.x + panel.w;
			MoveRegion(gFloatingPanelCloses[i], panel.x + panel.w - 16, panel.y + 1);
		}
		MoveRegion(gGodLibraryBlock, gGodLibraryX, gGodLibraryY);
		gGodLibraryBlock.RegionBottomRightX = gGodLibraryX + GOD_LIBRARY_W;
		gGodLibraryBlock.RegionBottomRightY = gGodLibraryY + GOD_LIBRARY_H;
		for (size_t i = 0; i < gGodIconRegions.size(); ++i)
		{
			// 24 selectable symbols fill the first eight cells of each JA2
			// 3x3 frame.  The final region occupies the last cell as CLOSE.
			const size_t cell = i < 24 ?
				(i / 8) * 9 + (i % 8) : 26;
			const INT16 frame = static_cast<INT16>(cell / 9);
			const INT16 local = static_cast<INT16>(cell % 9);
			MoveRegion(gGodIconRegions[i],
				gGodLibraryX + frame * 78 + 9 + (local % 3) * 20,
				gGodLibraryY + 25 + (local / 3) * 20);
		}
		MoveRegion(gAssetCatalogBlock, gAssetCatalogX, gAssetCatalogY);
		gAssetCatalogBlock.RegionBottomRightX = gAssetCatalogX + ASSET_CATALOG_W;
		gAssetCatalogBlock.RegionBottomRightY = gAssetCatalogY + ASSET_CATALOG_H;
		const std::array<SGPBox, 11> catalogRects{{
			{ 8, 25, 302, 18 }, { 8, 47, 302, 18 }, { 8, 69, 302, 18 },
			{ 8, 91, 302, 18 }, { 8, 116, 64, 18 }, { 77, 116, 64, 18 },
			{ 169, 116, 64, 18 }, { 238, 116, 64, 18 },
			{ 8, 139, 302, 18 }, { 182, 164, 128, 21 }, { 8, 164, 86, 21 }
		}};
		for (size_t i = 0; i < gAssetCatalogRegions.size(); ++i)
		{
			SGPBox const& rect = catalogRects[i];
			MoveRegion(gAssetCatalogRegions[i], gAssetCatalogX + rect.x,
				gAssetCatalogY + rect.y);
			gAssetCatalogRegions[i].RegionBottomRightX =
				gAssetCatalogX + rect.x + rect.w;
			gAssetCatalogRegions[i].RegionBottomRightY =
				gAssetCatalogY + rect.y + rect.h;
		}
		for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
		{
			const INT16 dockX = gTutorialActive &&
				i == static_cast<size_t>(FloatingPanelId::FEEDBACK) + 1 ?
				PanelDockStartX() :
				PanelDockStartX() + static_cast<INT16>(i) * 44;
			MoveRegion(gPanelDockRegions[i], dockX,
				gOrbY + 5);
		}
		const FloatingPanel& tools =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLS)];
		for (size_t i = 0; i < gToolRegions.size(); ++i)
			MoveRegion(gToolRegions[i], tools.x + 7,
				tools.y + 21 + static_cast<INT16>(i) * 10);
		const FloatingPanel& actions =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)];
		for (size_t i = 0; i < gActionPanelRegions.size(); ++i)
			MoveRegion(gActionPanelRegions[i], actions.x + 7,
				actions.y + 21 + static_cast<INT16>(i) * 18);
		const FloatingPanel& feedback =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::FEEDBACK)];
		MoveRegion(gFeedbackRegions[0], feedback.x + 8, feedback.y + 24);
		MoveRegion(gFeedbackRegions[1], feedback.x + 8, feedback.y + 49);
		MoveRegion(gFeedbackRegions[2], feedback.x + feedback.w - 126,
			feedback.y + feedback.h - 28);
		MoveRegion(gFeedbackRegions[3], feedback.x + 8,
			feedback.y + feedback.h - 28);
		const FloatingPanel& sector =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		for (size_t i = 0; i < gSectorUpgradeRegions.size(); ++i)
			MoveRegion(gSectorUpgradeRegions[i], sector.x + 8,
				sector.y + 56 + static_cast<INT16>(i) * 29);
		if (gContextVisible) PositionContextRegions();
	}

	BOOLEAN UpdateWindowDragging()
	{
		BOOLEAN moved = FALSE;
		if (gBagDragging)
		{
			const INT16 x = std::clamp<INT16>(gusMouseXPos - gDragOffsetX, 0,
				std::max<INT16>(0, gsVIEWPORT_END_X - PANE_W));
			const INT16 y = std::clamp<INT16>(gusMouseYPos - gDragOffsetY, 0,
				std::max<INT16>(0, gsVIEWPORT_END_Y - BAG_H));
			moved = x != gBagX || y != gBagY;
			gBagX = x;
			gBagY = y;
		}
		for (FloatingPanel& panel : gFloatingPanels)
		{
			if (!panel.dragging) continue;
			const INT16 x = std::clamp<INT16>(gusMouseXPos - gDragOffsetX, 0,
				std::max<INT16>(0, gsVIEWPORT_END_X - panel.w));
			const INT16 y = std::clamp<INT16>(gusMouseYPos - gDragOffsetY, 0,
				std::max<INT16>(0, gsVIEWPORT_END_Y - panel.h));
			moved |= x != panel.x || y != panel.y;
			panel.x = x;
			panel.y = y;
		}
		if (gOrbDragging)
		{
			const INT16 x = std::clamp<INT16>(gusMouseXPos - gDragOffsetX, 0,
				std::max<INT16>(0, gsVIEWPORT_END_X - 28));
			const INT16 y = std::clamp<INT16>(gusMouseYPos - gDragOffsetY, 0,
				std::max<INT16>(0, gsVIEWPORT_END_Y - 28));
			const BOOLEAN orbMoved = x != gOrbX || y != gOrbY;
			gOrbMoved |= orbMoved;
			moved |= orbMoved;
			gOrbX = x;
			gOrbY = y;
		}
		if (moved)
		{
			// Synchronize all hit regions exactly once per rendered frame. Updating
			// them for every raw mouse event made regions and pixels race each other.
			MoveRegion(gOrbRegion, gOrbX, gOrbY);
			PositionBagRegions();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		return moved;
	}

	void BagBlockCallback(MOUSE_REGION*, UINT32)
	{
		// Intentionally consumes clicks so they do not reach the tactical world.
	}

	void BagGrabberCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			gBagDragging = TRUE;
			gDragOffsetX = gusMouseXPos - gBagX;
			gDragOffsetY = gusMouseYPos - gBagY;
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
			SetRenderFlags(RENDER_FLAG_FULL);
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			gBagDragging = FALSE;
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void BagCloseCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		gBagVisible = FALSE;
		// Keep a short guard so the same click that closes the panel cannot
		// instantly reopen it through stale panel-side input.
		gPanelInteractionGuardUntil = GetJA2Clock() + 180;
		CloseContextMenu();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void FloatingPanelGrabberCallback(MOUSE_REGION* region, UINT32 reason)
	{
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gFloatingPanels.size()) return;
		FloatingPanel& panel = gFloatingPanels[index];
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			panel.dragging = TRUE;
			gDragOffsetX = gusMouseXPos - panel.x;
			gDragOffsetY = gusMouseYPos - panel.y;
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
			SetRenderFlags(RENDER_FLAG_FULL);
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			panel.dragging = FALSE;
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void FloatingPanelCloseCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gFloatingPanels.size()) return;
		gFloatingPanels[index].visible = FALSE;
		if (index == static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY))
			gLootVisible = FALSE;
		if (index == static_cast<size_t>(FloatingPanelId::FEEDBACK))
			StopFeedbackEditing();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void PanelDockCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (gTutorialActive &&
			index != static_cast<size_t>(FloatingPanelId::FEEDBACK) + 1) return;
		if (index == 0)
		{
			gBagVisible = !gBagVisible;
		}
		else if (index <= gFloatingPanels.size())
		{
			FloatingPanel& panel = gFloatingPanels[index - 1];
			if (index == static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY) + 1 &&
				gLootGridNo == NOWHERE &&
				!(gInspectedSoldier && CanAccessSoldierContents(gInspectedSoldier))) return;
			panel.visible = !panel.visible;
			if (index == static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY) + 1)
				gLootVisible = panel.visible && IsInspectedWorldAssetNear();
			if (index == static_cast<size_t>(FloatingPanelId::FEEDBACK) + 1 &&
				!panel.visible) StopFeedbackEditing();
		}
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void FeedbackCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		switch (index)
		{
			case 0:
				gFeedbackCategory = static_cast<UINT8>(
					(gFeedbackCategory + 1) % gFeedbackCategories.size());
				break;
			case 1:
				gFeedbackEditing = TRUE;
				gFeedbackStatus = "TYPING - ESC STOPS INPUT";
				SetUIKeyboardHook(FeedbackKeyboardHook);
				break;
			case 2:
				StopFeedbackEditing();
				WriteFeedbackReport();
				break;
			case 3:
				gFeedbackText.clear();
				gFeedbackStatus = "REPORT TEXT CLEARED";
				break;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void SectorUpgradeCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (BuildSectorUpgrade(index))
		{
			fInterfacePanelDirty = DIRTYLEVEL2;
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void ApplyCursorTool(UINT8 action)
	{
		gCursorAction = action;
		switch (action)
		{
			case 0: guiPendingOverrideEvent = A_CHANGE_TO_MOVE;     break;
			case 1: guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE; break;
			case 2: guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE; break;
			case 3: guiPendingOverrideEvent = LC_CHANGE_TO_LOOK;    break;
			case 4: guiPendingOverrideEvent = T_CHANGE_TO_TALKING;  break;
			case 5: guiPendingOverrideEvent = M_CHANGE_TO_ACTION;   break;
		}
		if (action != 2) ClearWorldMoveState();
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ToolPanelCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index < 6)
		{
			ApplyCursorTool(static_cast<UINT8>(index));
		}
		else if (index < 10)
		{
			OperationsActionCallback(&gOpsActionRegions[index - 6], reason);
		}
	}

	void ActionPanelCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gPanelActionEntryCount || !gPanelActionEntries[index].enabled) return;
		gContextEntryCount = gPanelActionEntryCount;
		for (size_t i = 0; i < gPanelActionEntryCount; ++i)
			gContextEntries[i] = gPanelActionEntries[i];
		gContextSoldier = gInspectedSoldier;
		gContextGridNo = gInspectedSoldier ? gInspectedSoldier->sGridNo : gInspectedGridNo;
		gContextLevel = gInspectedSoldier ? gInspectedSoldier->bLevel : gInspectedLevel;
		gContextTileIndex = gInspectedTileIndex;
		gContextInventorySlot = NO_SLOT;
		gContextWorldItemIndex = -1;
		gContextVisible = TRUE;
		ContextActionCallback(region, reason);

		const ContextAction action = gPanelActionEntries[index].action;
		if (action == ContextAction::CONTENTS && !gInspectedSoldier)
		{
			FloatingPanel& objectPanel =
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)];
			objectPanel.visible = gLootVisible;
		}
		RefreshPanelActions();
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
	}

	INT8 PreferredEquipmentSlot(SOLDIERTYPE const* soldier, OBJECTTYPE const& object)
	{
		if (!soldier || object.usItem == NOTHING) return NO_SLOT;
		ItemModel const* const item = GCM->getItem(object.usItem);
		if (item->isWeapon()) return HANDPOS;
		if (item->isArmour())
		{
			switch (item->asArmour()->getArmourClass())
			{
				case ARMOURCLASS_HELMET:   return HELMETPOS;
				case ARMOURCLASS_VEST:     return VESTPOS;
				case ARMOURCLASS_LEGGINGS: return LEGPOS;
				default:                   return NO_SLOT;
			}
		}
		if (item->isFace())
		{
			return soldier->inv[HEAD1POS].usItem == NOTHING ? HEAD1POS : HEAD2POS;
		}
		return NO_SLOT;
	}

	BOOLEAN EquipObject(SOLDIERTYPE* soldier, OBJECTTYPE* object, INT8 returnSlot)
	{
		if (!soldier || !object) return FALSE;
		const INT8 target = PreferredEquipmentSlot(soldier, *object);
		if (target == NO_SLOT) return FALSE;
		if (target == returnSlot)
		{
			return PlaceObject(soldier, returnSlot, object);
		}
		if (!PlaceObject(soldier, target, object)) return FALSE;

		// PlaceObject returns displaced equipment in object. Put it where the
		// right-clicked item came from, then fall back to another valid pocket.
		if (object->usItem != NOTHING)
		{
			if (returnSlot == NO_SLOT || !PlaceObject(soldier, returnSlot, object))
			{
				AutoPlaceObject(soldier, object, FALSE);
			}
		}
		return TRUE;
	}

	void OpenInventoryItemContext(SOLDIERTYPE* soldier, INT8 slot)
	{
		if (!soldier || slot < 0 || slot >= NUM_INV_SLOTS ||
			soldier->inv[slot].usItem == NOTHING) return;
		CloseContextMenu();
		gContextSoldier = soldier;
		gContextGridNo = soldier->sGridNo;
		gContextLevel = soldier->bLevel;
		gContextTileIndex = NO_TILE;
		gContextInventorySlot = slot;
		ItemModel const* const item = GCM->getItem(soldier->inv[slot].usItem);
		gContextTitle = item->getName();

		const BOOLEAN own = soldier->bTeam == OUR_TEAM;
		AddContextEntry(ContextAction::DETAILS, "DETAILS / ATTACHMENTS");
		if (own)
		{
			if (IsResourceItem(soldier->inv[slot].usItem))
				AddContextEntry(ContextAction::PICK_UP, "DEPOSIT IN SECTOR STOCKPILE");
			else
				AddContextEntry(ContextAction::EQUIP_ITEM, "EQUIP / USE");
			AddContextEntry(ContextAction::MOVE_ITEM, "MOVE / DRAG");
			if (item->getItemClass() == IC_GUN)
			{
				const char* const mode =
					soldier->bWeaponMode == WM_BURST ? "BURST" :
					soldier->bWeaponMode == WM_ATTACHED ? "ATTACHED" : "SINGLE";
				AddContextEntry(ContextAction::WEAPON_MODE,
					ST::format("FIRE MODE / {}", mode));
				AddContextEntry(ContextAction::RELOAD, "RELOAD");
				AddContextEntry(ContextAction::UNLOAD,
					ST::format("UNLOAD MAGAZINE / {}", soldier->inv[slot].ubGunShotsLeft),
					soldier->inv[slot].ubGunShotsLeft > 0);
				AddContextEntry(ContextAction::SWAP_HANDS, "SWAP HANDS");
			}
		}
		else
		{
			AddContextEntry(ContextAction::PICK_UP, "LOOT / PACK",
				CanAccessSoldierContents(soldier));
			AddContextEntry(ContextAction::MOVE_ITEM, "MOVE / DRAG",
				CanAccessSoldierContents(soldier));
		}

		const INT16 width = 168;
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		gContextX = std::clamp<INT16>(gusMouseXPos, 0,
			std::max<INT16>(0, gsVIEWPORT_END_X - width));
		gContextY = std::clamp<INT16>(gusMouseYPos, 0,
			std::max<INT16>(0, gsVIEWPORT_END_Y - height));
		gContextVisible = TRUE;
		PositionContextRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void OpenLootItemContext(INT32 itemIndex)
	{
		if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
			return;
		WORLDITEM& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return;
		CloseContextMenu();
		gContextSoldier = nullptr;
		gContextGridNo = gLootGridNo;
		gContextLevel = gLootLevel;
		gContextTileIndex = gLootTileIndex;
		gContextWorldItemIndex = itemIndex;
		ItemModel const* const item = GCM->getItem(worldItem.o.usItem);
		gContextTitle = item->getName();
		const BOOLEAN near = IsInspectedWorldAssetNear();
		AddContextEntry(ContextAction::DETAILS, "DETAILS / ATTACHMENTS");
		const BOOLEAN resource = IsResourceItem(worldItem.o.usItem);
		AddContextEntry(ContextAction::PICK_UP,
			resource ? "DEPOSIT IN SECTOR STOCKPILE" :
				(near ? "PACK / PICK UP" : "APPROACH & PICK UP"), near);
		if (!resource) AddContextEntry(ContextAction::EQUIP_ITEM, "EQUIP", near);
		AddContextEntry(ContextAction::MOVE_ITEM, "MOVE / DRAG", near);
		if (item->getItemClass() == IC_GUN)
		{
			AddContextEntry(ContextAction::WEAPON_MODE,
				"EQUIP & CHANGE FIRE MODE", near);
			AddContextEntry(ContextAction::UNLOAD,
				ST::format("UNLOAD MAGAZINE / {}", worldItem.o.ubGunShotsLeft),
				near && worldItem.o.ubGunShotsLeft > 0);
		}

		const INT16 width = 168;
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		gContextX = std::clamp<INT16>(gusMouseXPos, 0,
			std::max<INT16>(0, gsVIEWPORT_END_X - width));
		gContextY = std::clamp<INT16>(gusMouseYPos, 0,
			std::max<INT16>(0, gsVIEWPORT_END_Y - height));
		gContextVisible = TRUE;
		PositionContextRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ContextActionCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gContextVisible) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gContextEntryCount || !gContextEntries[index].enabled) return;

		const ContextAction action = gContextEntries[index].action;
		RecordFeedbackEvent(ST::format("ACTION {} grid {} tile {}",
			ContextActionName(action), gContextGridNo, gContextTileIndex));
		SOLDIERTYPE* const selected = GetSelectedMan();
		SOLDIERTYPE* const subject = gContextSoldier ?
			gContextSoldier : selected;
		switch (action)
		{
			case ContextAction::INSPECT:
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
				gMode = ComputerMode::INFO;
				break;
			case ContextAction::CONTENTS:
				gMode = ComputerMode::CONTENTS;
				if (gContextSoldier)
				{
					gContentsMode = ContentsMode::SOLDIER;
					gInventoryVisible = CanAccessSoldierContents(gContextSoldier);
					if (gContextSoldier->bTeam == OUR_TEAM)
					{
						gInventorySoldier = gContextSoldier;
						gBagVisible = gInventoryVisible;
					}
					else
					{
						gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible =
							gInventoryVisible;
					}
				}
				else
				{
					gContentsMode = ContentsMode::WORLD;
					gLootVisible = IsInspectedWorldAssetNear();
					gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = TRUE;
				}
				break;
			case ContextAction::BUILD:
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)].visible = TRUE;
				gMode = ComputerMode::BUILD;
				break;
			case ContextAction::CARRY:
				BeginInspectedWorldMove();
				break;
			case ContextAction::TALK:
				guiPendingOverrideEvent = T_CHANGE_TO_TALKING;
				break;
			case ContextAction::ATTACK:
				guiPendingOverrideEvent = M_CHANGE_TO_ACTION;
				break;
			case ContextAction::STAND:
				if (subject && subject->bTeam == OUR_TEAM)
					ChangeSoldierStance(subject, ANIM_STAND);
				break;
			case ContextAction::CROUCH:
				if (subject && subject->bTeam == OUR_TEAM)
					ChangeSoldierStance(subject, ANIM_CROUCH);
				break;
			case ContextAction::PRONE:
				if (subject && subject->bTeam == OUR_TEAM)
					ChangeSoldierStance(subject, ANIM_PRONE);
				break;
			case ContextAction::STEALTH:
				if (subject && subject->bTeam == OUR_TEAM)
					subject->bStealthMode = !subject->bStealthMode;
				break;
			case ContextAction::WEAPON_MODE:
				if (selected && gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists && worldItem.o.usItem != NOTHING)
					{
						OBJECTTYPE object = worldItem.o;
						RemoveItemFromPool(worldItem);
						EquipObject(selected, &object, NO_SLOT);
						if (object.usItem != NOTHING)
							AddItemToPool(gContextGridNo, &object, VISIBLE,
								gContextLevel, 0, -1);
						ChangeWeaponMode(selected);
					}
				}
				else if (subject && subject->bTeam == OUR_TEAM)
				{
					if (gContextInventorySlot != NO_SLOT &&
						gContextInventorySlot != HANDPOS)
					{
						OBJECTTYPE object{};
						GetObjFrom(&subject->inv[gContextInventorySlot], 0, &object);
						if (object.usItem != NOTHING)
						{
							EquipObject(subject, &object, gContextInventorySlot);
							if (object.usItem != NOTHING)
								PlaceObject(subject, gContextInventorySlot, &object);
						}
					}
					ChangeWeaponMode(subject);
				}
				break;
			case ContextAction::RELOAD:
				if (subject && subject->bTeam == OUR_TEAM) AutoReload(subject);
				break;
			case ContextAction::SWAP_HANDS:
				if (subject && subject->bTeam == OUR_TEAM) SwapHandItems(subject);
				break;
			case ContextAction::UNLOAD:
			{
				OBJECTTYPE* gun = nullptr;
				if (gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists) gun = &worldItem.o;
				}
				else if (subject && gContextInventorySlot != NO_SLOT)
				{
					gun = &subject->inv[gContextInventorySlot];
				}
				else if (subject)
				{
					gun = &subject->inv[HANDPOS];
				}
				OBJECTTYPE ammo{};
				if (gun && EmptyWeaponMagazine(gun, &ammo) && ammo.usItem != NOTHING)
				{
					if (!selected || !AutoPlaceObject(selected, &ammo, TRUE))
					{
						const GridNo dropGrid = selected ?
							selected->sGridNo : gContextGridNo;
						if (dropGrid >= 0 && dropGrid < WORLD_MAX)
							AddItemToPool(dropGrid, &ammo, VISIBLE,
								selected ? selected->bLevel : gContextLevel, 0, -1);
					}
				}
				break;
			}
			case ContextAction::DETAILS:
			{
				const INT16 detailX = std::clamp<INT16>(
					gContextX, 0, std::max<INT16>(0, SCREEN_WIDTH - 320));
				const INT16 detailY = std::clamp<INT16>(
					gContextY, 0, std::max<INT16>(0, SCREEN_HEIGHT - 200));
				if (gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists)
						InternalInitItemDescriptionBox(&worldItem.o,
							detailX, detailY, 0, selected);
				}
				else if (subject && gContextInventorySlot != NO_SLOT)
				{
					InitItemDescriptionBox(subject,
						static_cast<UINT8>(gContextInventorySlot),
						detailX, detailY, 0);
				}
				break;
			}
			case ContextAction::EQUIP_ITEM:
				if (selected && gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists && worldItem.o.usItem != NOTHING)
					{
						OBJECTTYPE object = worldItem.o;
						RemoveItemFromPool(worldItem);
						EquipObject(selected, &object, NO_SLOT);
						if (object.usItem != NOTHING)
							AddItemToPool(gContextGridNo, &object, VISIBLE,
								gContextLevel, 0, -1);
					}
				}
				else if (subject && subject->bTeam == OUR_TEAM &&
					gContextInventorySlot != NO_SLOT)
				{
					OBJECTTYPE object{};
					GetObjFrom(&subject->inv[gContextInventorySlot], 0, &object);
					if (object.usItem != NOTHING)
					{
						EquipObject(subject, &object, gContextInventorySlot);
						if (object.usItem != NOTHING)
							PlaceObject(subject, gContextInventorySlot, &object);
					}
				}
				break;
			case ContextAction::MOVE_ITEM:
				if (!gpItemPointer && selected && gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists && worldItem.o.usItem != NOTHING)
					{
						OBJECTTYPE object = worldItem.o;
						RemoveItemFromPool(worldItem);
						InternalBeginItemPointer(selected, &object, NO_SLOT);
					}
				}
				else if (subject && gContextInventorySlot != NO_SLOT &&
					subject->inv[gContextInventorySlot].usItem != NOTHING)
				{
					if (subject == selected)
						BeginItemPointer(subject, gContextInventorySlot);
					else if (selected && CanAccessSoldierContents(subject))
					{
						OBJECTTYPE object{};
						GetObjFrom(&subject->inv[gContextInventorySlot], 0, &object);
						if (object.usItem != NOTHING)
							InternalBeginItemPointer(selected, &object, NO_SLOT);
					}
				}
				break;
			case ContextAction::PICK_UP:
				if (!selected) break;
				if (gContextSoldier && gContextInventorySlot != NO_SLOT &&
					CanAccessSoldierContents(gContextSoldier))
				{
					if (StoreResourceInventoryItem(gContextSoldier,
						gContextInventorySlot)) break;
					OBJECTTYPE object{};
					GetObjFrom(&gContextSoldier->inv[gContextInventorySlot], 0, &object);
					if (object.usItem != NOTHING)
					{
						AutoPlaceObject(selected, &object, TRUE);
						if (object.usItem != NOTHING)
							PlaceObject(gContextSoldier, gContextInventorySlot, &object);
					}
				}
				else if (gContextGridNo >= 0 && gContextGridNo < WORLD_MAX)
				{
					const INT32 itemIndex = gContextWorldItemIndex >= 0 ?
						gContextWorldItemIndex :
						(GetItemPool(gContextGridNo, gContextLevel) ?
							GetItemPool(gContextGridNo, gContextLevel)->iItemIndex : -1);
					if (itemIndex >= 0 &&
						static_cast<size_t>(itemIndex) < gWorldItems.size())
					{
						WORLDITEM const& worldItem = GetWorldItem(itemIndex);
						if (worldItem.fExists && worldItem.o.usItem != NOTHING)
						{
							if (IsResourceItem(worldItem.o.usItem))
							{
								StoreResourceWorldItem(itemIndex);
								break;
							}
							// Let JA2 own approach path, animation, AP cost, traps and
							// inventory overflow. OS//0 only selects the exact object.
							SoldierPickupItem(selected, itemIndex, gContextGridNo,
								ITEM_IGNORE_Z_LEVEL);
						}
					}
				}
				break;
			case ContextAction::DIG:
				if (selected && DigTerrainAt(selected, gContextGridNo,
					gContextTileIndex))
				{
					gContextTitle = "EXPOSED SOIL";
					gInspectedGridNo = gContextGridNo;
					gInspectedLevel = 0;
					gInspectedTileIndex = NO_TILE;
					CaptureInspectorPreview(gInspectedGridNo, 0);
					RefreshPanelActions();
				}
				break;
			case ContextAction::SALVAGE:
				if (selected && SalvageWorldAsset(selected, gContextGridNo,
					gContextLevel, gContextTileIndex))
				{
					RefreshPanelActions();
				}
				break;
			case ContextAction::CATALOG:
				OpenAssetCatalog(gContextGridNo, gContextLevel, gContextTileIndex);
				break;
		}
		CloseContextMenu();
		SetBagRegionsEnabled(TRUE);
		fInterfacePanelDirty = DIRTYLEVEL2;
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void SlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
		SOLDIERTYPE* const soldier = gInventorySoldier ?
			gInventorySoldier : GetSelectedMan();
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!soldier || !CanAccessSoldierContents(soldier)) return;
		const INT8 slot = static_cast<INT8>(region->GetUserData<0>());

		if ((reason & MSYS_CALLBACK_REASON_RBUTTON_UP) &&
			gpItemPointer == nullptr && soldier->inv[slot].usItem != NOTHING)
		{
			OpenInventoryItemContext(soldier, slot);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) &&
			gpItemPointer == nullptr && soldier->inv[slot].usItem != NOTHING)
		{
			// Hand control to JA2. Its native item cursor can place this object
			// in another pocket, give/throw it, or drop it onto a world tile.
			if (soldier == selected)
			{
				BeginItemPointer(soldier, slot);
			}
			else if (selected)
			{
				OBJECTTYPE looted{};
				GetObjFrom(&soldier->inv[slot], 0, &looted);
				if (looted.usItem != NOTHING) InternalBeginItemPointer(selected, &looted, NO_SLOT);
			}
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (PlaceObject(soldier, slot, gpItemPointer) &&
				gpItemPointer->ubNumberOfObjects == 0)
			{
				EndItemPointer();
			}
		}
	}

	void ObjectSoldierSlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
		SOLDIERTYPE* const body = gInspectedSoldier;
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!body || body->bTeam == OUR_TEAM || !selected ||
			!CanAccessSoldierContents(body)) return;
		const INT8 slot = static_cast<INT8>(region->GetUserData<0>());
		if ((reason & MSYS_CALLBACK_REASON_RBUTTON_UP) && !gpItemPointer &&
			body->inv[slot].usItem != NOTHING)
		{
			OpenInventoryItemContext(body, slot);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && !gpItemPointer &&
			body->inv[slot].usItem != NOTHING)
		{
			OBJECTTYPE looted{};
			GetObjFrom(&body->inv[slot], 0, &looted);
			if (looted.usItem != NOTHING)
				InternalBeginItemPointer(selected, &looted, NO_SLOT);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (PlaceObject(body, slot, gpItemPointer) &&
				gpItemPointer->ubNumberOfObjects == 0) EndItemPointer();
		}
	}

	void LootSlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		// The double-click which opened this window may still be completing.
		// Never let that same physical gesture activate or close its contents.
		if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
		if (GetJA2Clock() < gLootIgnoreInputUntil) return;
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!selected || gLootGridNo < 0 || gLootGridNo >= WORLD_MAX) return;
		if (PythSpacesAway(selected->sGridNo, gLootGridNo) > 2)
		{
			gLootVisible = FALSE;
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		const size_t slot = static_cast<size_t>(region->GetUserData<0>());
		if (slot >= gLootWorldItems.size()) return;

		if (reason & MSYS_CALLBACK_REASON_LBUTTON_DOUBLECLICK)
		{
			gLootDragCandidate = -1;
			const INT32 itemIndex = gLootWorldItems[slot];
			if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size()) return;
			WORLDITEM& worldItem = GetWorldItem(itemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return;

			if (IsResourceItem(worldItem.o.usItem))
				StoreResourceWorldItem(itemIndex);
			else
				SoldierPickupItem(selected, itemIndex, gLootGridNo,
					ITEM_IGNORE_Z_LEVEL);
			fInterfacePanelDirty = DIRTYLEVEL2;
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		if ((reason & MSYS_CALLBACK_REASON_RBUTTON_UP) && !gpItemPointer)
		{
			gLootDragCandidate = -1;
			const INT32 itemIndex = gLootWorldItems[slot];
			OpenLootItemContext(itemIndex);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && !gpItemPointer)
		{
			// Do not remove the item yet. A double-click starts with this same
			// event, so drag begins only after the pointer actually moves.
			gLootDragCandidate = static_cast<INT8>(slot);
			gLootDragStartX = gusMouseXPos;
			gLootDragStartY = gusMouseYPos;
		}
		else if ((reason & MSYS_CALLBACK_REASON_MOVE) && !gpItemPointer &&
			gLootDragCandidate == static_cast<INT8>(slot) &&
			(region->ButtonState & MSYS_LEFT_BUTTON) &&
			(std::abs(gusMouseXPos - gLootDragStartX) >= 4 ||
			 std::abs(gusMouseYPos - gLootDragStartY) >= 4))
		{
			const INT32 itemIndex = gLootWorldItems[slot];
			gLootDragCandidate = -1;
			if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size()) return;
			WORLDITEM& worldItem = GetWorldItem(itemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return;
			OBJECTTYPE object = worldItem.o;
			RemoveItemFromPool(worldItem);
			InternalBeginItemPointer(selected, &object, NO_SLOT);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (AddItemToPool(gLootGridNo, gpItemPointer, VISIBLE,
				gLootLevel, 0, -1) >= 0) EndItemPointer();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			gLootDragCandidate = -1;
		}
	}

	void ApplyTutorialName()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier || gTutorialName.empty()) return;
		const ST::string name = gTutorialName.left(16);
		soldier->name = name;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		profile.zNickname = name;
		profile.zName = name;
	}

	void ApplyTutorialStats()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier) return;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		soldier->bLifeMax      = profile.bLifeMax      = gTutorialStatValues[0];
		soldier->bLife         = profile.bLife         = gTutorialStatValues[0];
		soldier->bAgility      = profile.bAgility      = gTutorialStatValues[1];
		soldier->bDexterity    = profile.bDexterity    = gTutorialStatValues[2];
		soldier->bStrength     = profile.bStrength     = gTutorialStatValues[3];
		soldier->bWisdom       = profile.bWisdom       = gTutorialStatValues[4];
		soldier->bLeadership   = profile.bLeadership   = gTutorialStatValues[5];
		soldier->bMarksmanship = profile.bMarksmanship = gTutorialStatValues[6];
		soldier->bMedical      = profile.bMedical      = gTutorialStatValues[7];
		soldier->bMechanical   = profile.bMechanical   = gTutorialStatValues[8];
		soldier->bExplosive    = profile.bExplosive    = gTutorialStatValues[9];
	}

	void ApplyTutorialTraits()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier) return;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		// These are freely selected specialties, not a predefined class.
		profile.bSkillTrait = static_cast<INT8>(gTutorialTraits[0]);
		profile.bSkillTrait2 = static_cast<INT8>(gTutorialTraits[1]);
		soldier->ubSkillTrait1 = static_cast<UINT8>(gTutorialTraits[0]);
		soldier->ubSkillTrait2 = static_cast<UINT8>(gTutorialTraits[1]);
	}

	BOOLEAN TutorialKeyboardHook(InputAtom* event)
	{
		if (!gTutorialActive || gTutorialStep != 1) return FALSE;
		if (event->usEvent == TEXT_INPUT)
		{
			for (char32_t c : event->codepoints)
			{
				if (gTutorialName.to_utf32().size() >= 16) break;
				if (c >= U' ' && c != U'<' && c != U'>') gTutorialName += c;
			}
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usEvent != KEY_DOWN && event->usEvent != KEY_REPEAT) return FALSE;
		if (event->usParam == SDLK_BACKSPACE)
		{
			ST::utf32_buffer chars = gTutorialName.to_utf32();
			if (!chars.empty())
			{
				gTutorialName = ST::string::from_utf32(chars.data(), chars.size() - 1);
			}
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usParam == SDLK_RETURN && !gTutorialName.empty())
		{
			ApplyTutorialName();
			gTutorialStep = 2;
			gInventoryVisible = TRUE;
			gInventorySoldier = GetSelectedMan();
			for (MOUSE_REGION& r : gTutorialStats) r.Enable();
			SetUIKeyboardHook(nullptr);
			PositionBagRegions();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		return TRUE;
	}

	void AdvanceTutorial()
	{
		switch (gTutorialStep)
		{
			case 0:
				gTutorialStep = 1;
				gTutorialName.clear();
				SetUIKeyboardHook(TutorialKeyboardHook);
				break;
			case 1:
				if (gTutorialName.empty()) return;
				ApplyTutorialName();
				gTutorialStep = 2;
				gInventoryVisible = TRUE;
				gInventorySoldier = GetSelectedMan();
				for (MOUSE_REGION& r : gTutorialStats) r.Enable();
				SetUIKeyboardHook(nullptr);
				break;
			case 2:
				ApplyTutorialStats();
				gTutorialStep = 3;
				for (MOUSE_REGION& r : gTutorialStats) r.Disable();
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Enable();
				break;
			case 3:
				ApplyTutorialTraits();
				EnsureFieldShovel(GetSelectedMan());
				gFieldToolIssued = TRUE;
				gTutorialStep = 4;
				gMode = ComputerMode::CONTENTS;
				gContentsMode = ContentsMode::SOLDIER;
				gInventoryVisible = TRUE;
				gInventorySoldier = GetSelectedMan();
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Disable();
				break;
			case 4:
				gTutorialStep = 5;
				gMode = ComputerMode::INFO;
				gInventoryVisible = FALSE;
				break;
			case 5:
				gTutorialActive = FALSE;
				gMode = ComputerMode::CONTENTS;
				gContentsMode = ContentsMode::SOLDIER;
				gInspectedSoldier = GetSelectedMan();
				gInventorySoldier = gInspectedSoldier;
				gInventoryVisible = gInventorySoldier != nullptr;
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
				RefreshPanelActions();
				SetUIKeyboardHook(nullptr);
				SetBagRegionsEnabled(TRUE);
				break;
			default: break;
		}
		PositionBagRegions();
		// Re-evaluate every tutorial hitbox after changing steps. This makes the
		// visible BEGIN/CONFIRM button and the newly revealed controls active in
		// the same frame instead of leaving the previous step's regions behind.
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void TutorialContinueCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP) AdvanceTutorial();
	}

	void TutorialStatCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t stat = static_cast<size_t>(region->GetUserData<0>());
		const BOOLEAN increase = region->GetUserData<1>() != 0;
		if (stat >= gTutorialStatValues.size()) return;
		if (increase)
		{
			if (gTutorialStatPoints >= 5 && gTutorialStatValues[stat] <= 80)
			{
				gTutorialStatValues[stat] += 5;
				gTutorialStatPoints -= 5;
			}
		}
		else if (gTutorialStatValues[stat] >= 40)
		{
			gTutorialStatValues[stat] -= 5;
			gTutorialStatPoints += 5;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void TutorialTraitCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gTutorialTraitValues.size()) return;
		const SkillTrait trait = gTutorialTraitValues[index];
		for (SkillTrait& selected : gTutorialTraits)
		{
			if (selected == trait)
			{
				selected = NO_SKILLTRAIT;
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
		}
		for (SkillTrait& selected : gTutorialTraits)
		{
			if (selected == NO_SKILLTRAIT)
			{
				selected = trait;
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
		}
		// With both slots occupied, the newest choice replaces the older one.
		gTutorialTraits[0] = gTutorialTraits[1];
		gTutorialTraits[1] = trait;
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void SelectAdjacentSquad(INT8 direction)
	{
		const INT32 current = CurrentSquad();
		for (INT32 offset = 1; offset <= NUMBER_OF_SQUADS; ++offset)
		{
			const INT32 candidate = (current + direction * offset +
				NUMBER_OF_SQUADS * 2) % NUMBER_OF_SQUADS;
			if (IsSquadOnCurrentTacticalMap(candidate))
			{
				SetCurrentSquad(candidate, FALSE);
				if (SOLDIERTYPE* const selected = GetSelectedMan())
				{
					gInspectedSoldier = selected;
					gInspectedGridNo = NOWHERE;
					LocateSoldier(selected, DONTSETLOCATOR);
				}
				break;
			}
		}
	}

	void OperationsActionCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		switch (region->GetUserData<0>())
		{
			case 0:
				GoToMapScreenFromTactical();
				return;
			case 1:
				gfBeginEndTurn = TRUE;
				break;
			case 2:
				SelectAdjacentSquad(-1);
				break;
			case 3:
				SelectAdjacentSquad(1);
				break;
			case 4:
				gInspectedSoldier = GetSelectedMan();
				gInventorySoldier = gInspectedSoldier;
				gInspectedGridNo = NOWHERE;
				gInventoryVisible = gInspectedSoldier != nullptr;
				gContentsMode = ContentsMode::SOLDIER;
				gMode = ComputerMode::CONTENTS;
				break;
		}
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void OrbCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			gOrbDragging = TRUE;
			gOrbMoved = FALSE;
			gDragOffsetX = gusMouseXPos - gOrbX;
			gDragOffsetY = gusMouseYPos - gOrbY;
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
			SetRenderFlags(RENDER_FLAG_FULL);
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			gOrbDragging = FALSE;
			if (!gOrbMoved)
			{
				gBagVisible = !gBagVisible;
				if (gBagVisible) gPanelInteractionGuardUntil = 0;
				SetBagRegionsEnabled(TRUE);
			}
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void DrawOrb()
	{
		// A small diegetic inventory token rather than a navigation panel.
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gOrbX + 5, gOrbY, gOrbX + 22, gOrbY + 27, 0);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gOrbX, gOrbY + 5, gOrbX + 27, gOrbY + 22, 0);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gOrbX + 6, gOrbY + 9, "OS0");
		if (gWorldZoom > 1)
		{
			SetFontForeground(FONT_WHITE);
			MPrint(gOrbX + 7, gOrbY + 17, "Z2");
		}
		if (!gAimAutoCollapsed)
		{
			const std::array<const char*, PANEL_DOCK_COUNT> labels{{
				"CHAR", "CTX", "TOOL", "ACT", "OBJ", "SECT", "FB"
			}};
			const INT16 startX = PanelDockStartX();
			const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
			const size_t first = gTutorialActive ?
				static_cast<size_t>(FloatingPanelId::FEEDBACK) + 1 : 0;
			for (size_t i = first; i < labels.size(); ++i)
			{
				const INT16 x = gTutorialActive ? startX :
					startX + static_cast<INT16>(i) * 44;
				OutlineBox(x, gOrbY + 5, 41, 17, red);
				SetFontForeground(FONT_MCOLOR_LTGRAY);
				MPrint(x + 4, gOrbY + 10, labels[i]);
			}
			InvalidateRegion(startX, gOrbY + 4,
				startX + static_cast<INT16>(gTutorialActive ? 44 :
					PANEL_DOCK_COUNT * 44), gOrbY + 23);
		}
		InvalidateRegion(gOrbX, gOrbY, gOrbX + 28, gOrbY + 28);
	}

	void EnsurePortrait(SOLDIERTYPE* soldier)
	{
		if (!soldier || soldier->ubProfile == gPortraitProfile) return;
		if (gPortrait) DeleteVideoObject(gPortrait);
		gPortrait = nullptr;
		gPortraitProfile = soldier->ubProfile;
		// Randomly generated enemies and civilians use NO_PROFILE (200). Calling
		// GetProfile for them aborts the process; their panel uses the pixel
		// silhouette instead of pretending they have a mercenary portrait.
		if (soldier->ubProfile >= NUM_PROFILES) return;
		try
		{
			gPortrait = Load33Portrait(GetProfile(soldier->ubProfile));
		}
		catch (...)
		{
			gPortrait = nullptr;
		}
	}

	void OutlineBox(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour)
	{
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + w - 1, y, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + h - 1, x + w - 1, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - 1, y, x + w - 1, y + h - 1, colour);
	}

	void DrawOS0Shell()
	{
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 dark = Get16BPPColor(FROMRGB(5, 8, 8));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX, gBagY,
			gBagX + PANE_W - 1, gBagY + BAG_H - 1, dark);
		OutlineBox(gBagX, gBagY, PANE_W, BAG_H, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX + 1, gBagY + 17,
			gBagX + PANE_W - 2, gBagY + 17, red);
	}

	void DrawFloatingPanelShell(FloatingPanel const& panel, const ST::string& title)
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x, panel.y,
			panel.x + panel.w - 1, panel.y + panel.h - 1, dark);
		OutlineBox(panel.x, panel.y, panel.w, panel.h, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x + 1,
			panel.y + PANEL_HEADER_H, panel.x + panel.w - 2,
			panel.y + PANEL_HEADER_H, red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(panel.x + 6, panel.y + 5, title);
		MPrint(panel.x + panel.w - 13, panel.y + 5, "X");
	}

	void DrawContextPanel()
	{
		const FloatingPanel& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)];
		if (!panel.visible) return;
		DrawFloatingPanelShell(panel, "CONTEXT / TARGET");
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		if (gInspectedSoldier)
		{
			SetFontForeground(FONT_WHITE);
			MPrint(panel.x + 8, panel.y + 25, gInspectedSoldier->name.left(22));
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(panel.x + 8, panel.y + 42,
				ST::format("HP {}/{}  AP {}", gInspectedSoldier->bLife,
					gInspectedSoldier->bLifeMax, gInspectedSoldier->bActionPoints));
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 8, panel.y + 59,
				gInspectedSoldier->bTeam == OUR_TEAM ? "ALLY / CONTROLLABLE" :
				"CONTACT / CONTEXT ONLY");
			MPrint(panel.x + 8, panel.y + 76,
				ST::format("GRID {}  LEVEL {}", gInspectedSoldier->sGridNo,
					gInspectedSoldier->bLevel));
		}
		else if (gInspectedGridNo >= 0 && gInspectedGridNo < WORLD_MAX)
		{
			if (gInspectorPreview)
				BltVideoSurface(FRAME_BUFFER, gInspectorPreview,
					panel.x + 8, panel.y + 24, nullptr);
			SetFontForeground(FONT_WHITE);
			MPrint(panel.x + 80, panel.y + 27, gContextTitle.left(18));
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 80, panel.y + 44,
				ST::format("GRID {}", gInspectedGridNo));
			MPrint(panel.x + 80, panel.y + 58,
				ST::format("TILE {}", gInspectedTileIndex));
			SetFontForeground(IsInspectedWorldAssetNear() ? FONT_WHITE : FONT_MCOLOR_RED);
			MPrint(panel.x + 80, panel.y + 76,
				IsInspectedWorldAssetNear() ? "IN RANGE" : "OUT OF RANGE");
			if (AssetCatalogRecord const* const catalog =
				FindAssetCatalogRecordConst(static_cast<INT16>(giCurrentTilesetID),
					CanonicalAssetTileIndex(gInspectedGridNo, gInspectedLevel,
						gInspectedTileIndex)))
			{
				SetFontForeground(FONT_MCOLOR_RED);
				MPrint(panel.x + 8, panel.y + 96,
					ST::format("DB {} {}x{} {}",
						gAssetCategoryNames[static_cast<size_t>(catalog->category)],
						catalog->width, catalog->height,
						catalog->buildable ? "BUILD" : "WORLD").left(29));
			}
			else if (STRUCTURE const* const structure = WorldStructureAt(
				gInspectedGridNo, gInspectedLevel, gInspectedTileIndex))
			{
				WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
				SetFontForeground(FONT_MCOLOR_LTGRAY);
				MPrint(panel.x + 8, panel.y + 96,
					ST::format("{}  {}KG  HP{}  F{}", physics.materialName,
						static_cast<INT32>(physics.massKg + 0.5f), physics.integrity,
						static_cast<INT32>(physics.friction * 100.0f)));
			}
			else if (gInspectedLevel == 0 &&
				gpWorldLevelData[gInspectedGridNo].pLandHead)
			{
				SetFontForeground(FONT_MCOLOR_LTGRAY);
				MPrint(panel.x + 8, panel.y + 96,
					TerrainPhysicsName(GetTerrainType(gInspectedGridNo)));
			}
		}
		else
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 8, panel.y + 29, "NO OBJECT SELECTED");
			MPrint(panel.x + 8, panel.y + 47, "CLICK OR RIGHT-CLICK A TARGET");
		}
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawToolsPanel()
	{
		const FloatingPanel& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLS)];
		if (!panel.visible) return;
		DrawFloatingPanelShell(panel, "TOOLS / CURSOR + FIELD");
		const std::array<const char*, 10> labels{{
			"MOVE", "USE / LOOT", "CARRY", "LOOK", "TALK", "ATTACK",
			"STRATEGIC MAP", "END TURN", "PREV SQUAD", "NEXT SQUAD"
		}};
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		for (size_t i = 0; i < labels.size(); ++i)
		{
			SetFontForeground(i < 6 && gCursorAction == i ?
				FONT_MCOLOR_RED : FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 9, panel.y + 24 + static_cast<INT16>(i) * 10,
				ST::format("{} {}", i < 6 && gCursorAction == i ? ">" : " ", labels[i]));
		}
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawActionsPanel()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)];
		if (!panel.visible) return;
		RefreshPanelActions();
		DrawFloatingPanelShell(panel, "ACTIONS / SELECTED OBJECT");
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		if (gPanelActionEntryCount == 0)
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 8, panel.y + 29, "NO CONTEXT ACTIONS");
		}
		for (size_t i = 0; i < gPanelActionEntryCount; ++i)
		{
			SetFontForeground(gPanelActionEntries[i].enabled ?
				FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(panel.x + 9, panel.y + 25 + static_cast<INT16>(i) * 18,
				ST::format("[>] {}", gPanelActionEntries[i].label).left(30));
		}
		SetBagRegionsEnabled(TRUE);
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawSectorPanel()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		if (!panel.visible) return;
		DrawFloatingPanelShell(panel,
			ST::format("SECTOR / {}", gWorldSector.AsShortString()));
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(panel.x + 8, panel.y + 24, "STOCKPILE");
		SetFontForeground(FONT_WHITE);
		MPrint(panel.x + 70, panel.y + 24,
			ST::format("W{} S{} R{} E{}",
				SectorResource(ResourceKind::TIMBER),
				SectorResource(ResourceKind::STONE),
				SectorResource(ResourceKind::SCRAP),
				SectorResource(ResourceKind::SOIL)));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(panel.x + 8, panel.y + 39,
			"DOUBLE-CLICK MATERIAL TO DEPOSIT");

		for (size_t i = 0; i < gSectorUpgrades.size(); ++i)
		{
			SectorUpgrade const& upgrade = gSectorUpgrades[i];
			const INT16 y = panel.y + 55 + static_cast<INT16>(i) * 29;
			const BOOLEAN built = HasUpgrade(upgrade);
			const BOOLEAN ready = CanBuildUpgrade(upgrade);
			OutlineBox(panel.x + 8, y, panel.w - 16, 26,
				ready ? Get16BPPColor(FROMRGB(205, 12, 12)) : red);
			SetFontForeground(built ? FONT_MCOLOR_RED :
				ready ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(panel.x + 13, y + 4,
				ST::format("{} {}", built ? "[BUILT]" : "[BUILD]", upgrade.name));
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 13, y + 14,
				built ? upgrade.benefit : ST::format("W{} S{} R{} E{} / {}",
					upgrade.timber, upgrade.stone, upgrade.scrap, upgrade.soil,
					upgrade.benefit));
		}
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawFeedbackPanel()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::FEEDBACK)];
		if (!panel.visible) return;
		DrawFloatingPanelShell(panel, "PLAYTEST / FEEDBACK REPORT");
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 brightRed = Get16BPPColor(FROMRGB(205, 12, 12));
		OutlineBox(panel.x + 8, panel.y + 24, panel.w - 16, 18, red);
		OutlineBox(panel.x + 8, panel.y + 49, panel.w - 16, 104,
			gFeedbackEditing ? brightRed : red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(panel.x + 13, panel.y + 29,
			ST::format("CATEGORY  < {} >", gFeedbackCategories[gFeedbackCategory]));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		if (gFeedbackText.empty())
		{
			MPrint(panel.x + 14, panel.y + 59,
				"CLICK HERE. DESCRIBE WHAT YOU DID,");
			MPrint(panel.x + 14, panel.y + 72,
				"WHAT HAPPENED, AND WHAT YOU EXPECTED.");
		}
		else
		{
			ST::utf32_buffer const chars = gFeedbackText.to_utf32();
			size_t cursor = chars.size() > 250 ? chars.size() - 250 : 0;
			for (INT16 line = 0; line < 6 && cursor < chars.size(); ++line)
			{
				const size_t limit = std::min(chars.size(), cursor + 50);
				size_t end = cursor;
				while (end < limit && chars[end] != U'\n') ++end;
				ST::string const text = end > cursor ?
					ST::string::from_utf32(chars.data() + cursor, end - cursor) : ST::string{};
				MPrint(panel.x + 14, panel.y + 58 + line * 14, text);
				cursor = end < chars.size() && chars[end] == U'\n' ? end + 1 : end;
			}
		}
		SetFontForeground(gFeedbackEditing ? FONT_WHITE : FONT_MCOLOR_LTGRAY);
		MPrint(panel.x + 10, panel.y + 160, gFeedbackStatus.left(55));
		if (!gFeedbackLastFile.empty())
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(panel.x + 10, panel.y + 173, gFeedbackLastFile.left(55));
		}
		OutlineBox(panel.x + 8, panel.y + panel.h - 28, 76, 19, red);
		OutlineBox(panel.x + panel.w - 126, panel.y + panel.h - 28, 118, 19,
			brightRed);
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(panel.x + 26, panel.y + panel.h - 23, "CLEAR");
		SetFontForeground(FONT_WHITE);
		MPrint(panel.x + panel.w - 108, panel.y + panel.h - 23, "SAVE REPORT");
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void TutorialText(INT16 x, INT16 y, const ST::string& text, UINT8 color = FONT_WHITE)
	{
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(color);
		MPrint(gBagX + x, gBagY + y, text);
	}

	void DrawGearMode(SOLDIERTYPE* soldier);

	void DrawTutorial()
	{
		DrawOS0Shell();
		TutorialText(15, 9, "OS//0 FIELD COMPUTER", FONT_MCOLOR_RED);

		switch (gTutorialStep)
		{
			case 0:
				TutorialText(15, 30, "WELCOME TO ARULCO", FONT_MCOLOR_RED);
				TutorialText(15, 47, "No extraction. No support. This sector is yours.");
				TutorialText(15, 59, "First we establish your operator identity.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "BEGIN", FONT_MCOLOR_RED);
				break;
			case 1:
				TutorialText(15, 31, "IDENTITY / ENTER YOUR CALLSIGN", FONT_MCOLOR_RED);
				TutorialText(15, 51, ST::format("> {}_", gTutorialName), FONT_WHITE);
				TutorialText(15, 69, "Type a name. ENTER or CONFIRM continues.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "CONFIRM", FONT_MCOLOR_RED);
				break;
			case 2:
				TutorialText(15, 9, "BUILD OPERATOR / FREE PARAMETERS", FONT_MCOLOR_RED);
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				SetFontForeground(FONT_WHITE);
				MPrint(gInventoryX + PANE_W - 105, gInventoryY + 9,
					ST::format("POINTS {}", gTutorialStatPoints));
				for (size_t i = 0; i < gTutorialStatNames.size(); ++i)
				{
					const INT16 baseX = gBagX + (i < 5 ? 0 : 230);
					const INT16 baseY = gBagY;
					const INT16 y = 27 + static_cast<INT16>(i % 5) * 18;
					SetFontForeground(FONT_WHITE);
					MPrint(baseX + 15, baseY + y, gTutorialStatNames[i]);
					SetFontForeground(FONT_MCOLOR_RED);
					MPrint(baseX + 96, baseY + y, "-");
					SetFontForeground(FONT_WHITE);
					MPrint(baseX + 113, baseY + y, ST::format("{}", gTutorialStatValues[i]));
					SetFontForeground(FONT_MCOLOR_RED);
					MPrint(baseX + 147, baseY + y, "+");
				}
				TutorialText(CONTINUE_X + 5, BAG_H - 17, "CONFIRM STATS", FONT_MCOLOR_RED);
				break;
			case 3:
				TutorialText(15, 9, "SPECIALTIES / CHOOSE UP TO TWO", FONT_MCOLOR_RED);
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				for (size_t i = 0; i < gTutorialTraitValues.size(); ++i)
				{
					const BOOLEAN right = i >= 8;
					const INT16 row = static_cast<INT16>(right ? i - 8 : i);
					const BOOLEAN selected = gTutorialTraits[0] == gTutorialTraitValues[i] ||
						gTutorialTraits[1] == gTutorialTraitValues[i];
					SetFontForeground(selected ? FONT_MCOLOR_RED : FONT_WHITE);
					MPrint(gBagX + (right ? 230 : 0) + 15,
						gBagY + 27 + row * 14,
						ST::format("{} {}", selected ? "[X]" : "[ ]",
							gTutorialTraitNames[i]));
				}
				TutorialText(15, BAG_H - 32, "0-2 FREE CHOICES / NO CLASS", FONT_MCOLOR_LTGRAY);
				TutorialText(CONTINUE_X + 5, BAG_H - 17, "CONFIRM SKILLS", FONT_MCOLOR_RED);
				break;
			case 4:
				TutorialText(15, 27, "FIELD EQUIPMENT", FONT_MCOLOR_RED);
				TutorialText(15, 45, "Click an item to attach it to the mouse.");
				TutorialText(15, 57, "Drop it into a slot, onto a unit, or into the world.");
				TutorialText(15, 69, "Double-click a body or container to inspect its loot.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "NEXT", FONT_MCOLOR_RED);
				break;
			case 5:
				TutorialText(15, 25, "LIVE CONTROL", FONT_MCOLOR_RED);
				TutorialText(15, 42, "LEFT: move / select       DOUBLE: inspect");
				TutorialText(15, 55, "RIGHT: context actions    MIDDLE: cycle action");
				TutorialText(15, 68, "SHIFT + MIDDLE: cancel action / center operator");
				TutorialText(15, 81, "Use PICK UP on nearby objects. Reach the red marker.");
				TutorialText(CONTINUE_X + 3, BAG_H - 17, "ENTER ARULCO", FONT_MCOLOR_RED);
				break;
		}
		InvalidateRegion(gBagX, gBagY, gBagX + PANE_W, gBagY + BAG_H);
	}

	ST::string TraitLabel(UINT8 trait)
	{
		for (size_t i = 0; i < gTutorialTraitValues.size(); ++i)
		{
			if (static_cast<UINT8>(gTutorialTraitValues[i]) == trait)
				return gTutorialTraitNames[i];
		}
		return "NONE";
	}

	void DrawCharacterSummary(SOLDIERTYPE* soldier)
	{
		if (!soldier) return;
		EnsurePortrait(soldier);
		if (gPortrait)
			BltVideoObject(FRAME_BUFFER, gPortrait, 0, gBagX + 9, gBagY + 22);

		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_WHITE);
		MPrint(gBagX + 8, gBagY + 5, soldier->name.left(18));

		const std::array<const char*, 10> labels{{
			"HP", "AGI", "DEX", "STR", "WIS",
			"LDR", "MRK", "MED", "MEC", "EXP"
		}};
		const std::array<INT16, 10> values{{
			soldier->bLife, soldier->bAgility, soldier->bDexterity,
			soldier->bStrength, soldier->bWisdom, soldier->bLeadership,
			soldier->bMarksmanship, soldier->bMedical,
			soldier->bMechanical, soldier->bExplosive
		}};
		for (size_t i = 0; i < labels.size(); ++i)
		{
			const INT16 column = static_cast<INT16>(i / 5);
			const INT16 row = static_cast<INT16>(i % 5);
			SetFontForeground(i == 0 ? FONT_MCOLOR_RED : FONT_MCOLOR_LTGRAY);
			MPrint(gBagX + 64 + column * 39, gBagY + 22 + row * 13,
				ST::format("{}{}", labels[i], values[i]));
		}

		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gBagX + 8, gBagY + 90, "PERKS");
		SetFontForeground(FONT_WHITE);
		MPrint(gBagX + 8, gBagY + 104,
			ST::format("1 {}", TraitLabel(soldier->ubSkillTrait1)).left(21));
		MPrint(gBagX + 8, gBagY + 117,
			ST::format("2 {}", TraitLabel(soldier->ubSkillTrait2)).left(21));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(gBagX + 8, gBagY + 136,
			soldier->bTeam == OUR_TEAM ? "PLAYER CHARACTER" : "OBSERVED CONTACT");
	}

	void DrawGearMode(SOLDIERTYPE* soldier)
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		const UINT16 slotDark = Get16BPPColor(FROMRGB(12, 17, 17));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gInventoryX, gInventoryY,
			gInventoryX + PANE_W - 1, gInventoryY + BAG_H - 1, dark);
		OutlineBox(gInventoryX, gInventoryY, PANE_W, BAG_H, red);

		// The native inventory slots remain the actual interaction targets.  The
		// surrounding groups only make their body function legible at a glance.
		OutlineBox(gInventoryX + 4, gInventoryY + 17, INVENTORY_X - 8, 124, mutedRed);
		OutlineBox(gInventoryX + INVENTORY_X + 4, gInventoryY + 17, 108, 124, mutedRed);
		OutlineBox(gInventoryX + INVENTORY_X + 118, gInventoryY + 17, 53, 124, mutedRed);
		OutlineBox(gInventoryX + INVENTORY_X + 176, gInventoryY + 17, 141, 124, mutedRed);
		for (const SlotLayout& slot : gSlots)
		{
			ColorFillVideoSurfaceArea(FRAME_BUFFER,
				gInventoryX + slot.x + 1, gInventoryY + slot.y + 1,
				gInventoryX + slot.x + slot.w - 2,
				gInventoryY + slot.y + slot.h - 2, slotDark);
			OutlineBox(gInventoryX + slot.x, gInventoryY + slot.y,
				slot.w, slot.h, red);
		}

		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gInventoryX + INVENTORY_X + 8, gInventoryY + 5, "GEAR");
		MPrint(gInventoryX + INVENTORY_X + 121, gInventoryY + 5, "BODY");
		MPrint(gInventoryX + INVENTORY_X + 180, gInventoryY + 5, "PACK / POCKETS");
		DrawCharacterSummary(soldier);
		if (!CanAccessSoldierContents(soldier))
		{
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gInventoryX + 14, gInventoryY + 45,
				"CONTENTS LOCKED / TARGET ACTIVE OR TOO FAR");
			return;
		}
		if (soldier)
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			if (!gTutorialActive)
				MPrint(gInventoryX + INVENTORY_X + 8, gInventoryY + BAG_H - 30,
					soldier->name.left(18));
			for (size_t i = 0; i < gSlots.size(); ++i)
			{
				const SlotLayout& slot = gSlots[i];
				const OBJECTTYPE& object = soldier->inv[slot.slot];
				ST::string help;
				if (object.usItem != NOTHING)
				{
					ItemModel const* const item = GCM->getItem(object.usItem);
					help = ST::format("{}\nZustand: {}%   Anzahl: {}\n"
						"Rechtsklick: anlegen",
						item->getName(), object.bStatus[0], object.ubNumberOfObjects);
					INVRenderItem(FRAME_BUFFER, soldier, object,
						gInventoryX + slot.x, gInventoryY + slot.y, slot.w, slot.h,
						DIRTYLEVEL2, 0, SGP_TRANSPARENT);
				}
				if (help != gSlotHelp[i])
				{
					gSlotHelp[i] = help;
					gSlotRegions[i].SetFastHelpText(help);
				}
			}
		}

	}

	void DrawObjectSoldierInventory(SOLDIERTYPE* body)
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		const UINT16 slotDark = Get16BPPColor(FROMRGB(12, 17, 17));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gLootX, gLootY,
			gLootX + PANE_W - 1, gLootY + BAG_H - 1, dark);
		OutlineBox(gLootX, gLootY, PANE_W, BAG_H, red);
		OutlineBox(gLootX + 4, gLootY + 17, INVENTORY_X - 8, 124, mutedRed);
		OutlineBox(gLootX + INVENTORY_X + 4, gLootY + 17, 108, 124, mutedRed);
		OutlineBox(gLootX + INVENTORY_X + 118, gLootY + 17, 53, 124, mutedRed);
		OutlineBox(gLootX + INVENTORY_X + 176, gLootY + 17, 141, 124, mutedRed);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gLootX + 8, gLootY + 5,
			ST::format("CONTACT INVENTORY / {}", body ? body->name : "LOST").left(55));
		MPrint(gLootX + PANE_W - 13, gLootY + 5, "X");
		if (!body || !CanAccessSoldierContents(body))
		{
			MPrint(gLootX + 12, gLootY + 34, "CONTENTS LOCKED / TARGET ACTIVE OR TOO FAR");
			return;
		}
		SetFontForeground(FONT_WHITE);
		MPrint(gLootX + 8, gLootY + 26, body->name.left(18));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(gLootX + 8, gLootY + 43,
			ST::format("HP {}/{}", body->bLife, body->bLifeMax));
		MPrint(gLootX + 8, gLootY + 59, "DRAG ITEMS TO YOUR CHARACTER");
		MPrint(gLootX + 8, gLootY + 75, "RIGHT CLICK: ITEM ACTIONS");
		for (size_t i = 0; i < gSlots.size(); ++i)
		{
			const SlotLayout& slot = gSlots[i];
			ColorFillVideoSurfaceArea(FRAME_BUFFER,
				gLootX + slot.x + 1, gLootY + slot.y + 1,
				gLootX + slot.x + slot.w - 2, gLootY + slot.y + slot.h - 2,
				slotDark);
			OutlineBox(gLootX + slot.x, gLootY + slot.y, slot.w, slot.h, red);
			OBJECTTYPE const& object = body->inv[slot.slot];
			ST::string help;
			if (object.usItem != NOTHING)
			{
				ItemModel const* const item = GCM->getItem(object.usItem);
				help = ST::format("{}\nZustand: {}%   Anzahl: {}\nDrag: loot",
					item->getName(), object.bStatus[0], object.ubNumberOfObjects);
				INVRenderItem(FRAME_BUFFER, body, object,
					gLootX + slot.x, gLootY + slot.y, slot.w, slot.h,
					DIRTYLEVEL2, 0, SGP_TRANSPARENT);
			}
			if (help != gObjectSlotHelp[i])
			{
				gObjectSlotHelp[i] = help;
				gObjectSlotRegions[i].SetFastHelpText(help);
			}
		}
		InvalidateRegion(gLootX, gLootY, gLootX + PANE_W, gLootY + BAG_H);
	}

	void DrawLootMode()
	{
		gLootWorldItems.fill(-1);
		std::array<ST::string, 12> nextHelp;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 slotDark = Get16BPPColor(FROMRGB(12, 17, 17));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gLootX, gLootY,
			gLootX + PANE_W - 1, gLootY + BAG_H - 1,
			Get16BPPColor(FROMRGB(4, 7, 7)));
		OutlineBox(gLootX, gLootY, PANE_W, BAG_H, red);
		OutlineBox(gLootX + 7, gLootY + 17, PANE_W - 14, TAB_Y - 20, mutedRed);
		if (gLootGridNo < 0 || gLootGridNo >= WORLD_MAX)
		{
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gLootX + 15, gLootY + 30, "CONTAINER SIGNAL LOST");
			return;
		}
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gLootX + 12, gLootY + 5,
			ST::format("OBJECT INVENTORY / {}", gContextTitle).left(56));
		MPrint(gLootX + PANE_W - 13, gLootY + 5, "X");
		if (!gLootVisible)
		{
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gLootX + 15, gLootY + 42, "CONTENTS LOCKED / MOVE WITHIN 2 TILES");
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(gLootX + 15, gLootY + 60, "RIGHT CLICK: CONTEXT / APPROACH");
			return;
		}
		OutlineBox(gLootX + PANE_W - 57, gLootY + 23, 45, 43, red);
		// Do not blit an arbitrary world tile into the UI. Interactive nodes may
		// reference cached/animated subimages whose lifetime belongs to the world
		// renderer; using them here caused the container-open crash. A stable
		// pixel glyph identifies the source while its actual items use JA2 icons.
		OutlineBox(gLootX + PANE_W - 47, gLootY + 34, 24, 18, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gLootX + PANE_W - 43, gLootY + 30,
			gLootX + PANE_W - 28, gLootY + 34, red);

		size_t slot = 0;
		for (ITEM_POOL* item = GetItemPool(gLootGridNo, gLootLevel);
			item && slot < gLootWorldItems.size(); item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM& worldItem = GetWorldItem(item->iItemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING ||
				worldItem.o.usItem == OWNERSHIP || worldItem.o.usItem == ACTION_ITEM) continue;

			const INT16 x = gLootX + 14 + static_cast<INT16>(slot % 6) * 62;
			const INT16 y = gLootY + 25 + static_cast<INT16>(slot / 6) * 31;
			ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 1, y + 1,
				x + 57, y + 26, slotDark);
			OutlineBox(x, y, 59, 28, red);
			INVRenderItem(FRAME_BUFFER, GetSelectedMan(), worldItem.o,
				x + 2, y + 2, 55, 24, DIRTYLEVEL2, 0, SGP_TRANSPARENT);
			gLootWorldItems[slot] = item->iItemIndex;
			ItemModel const* const itemModel = GCM->getItem(worldItem.o.usItem);
			nextHelp[slot] = ST::format("{}\nZustand: {}%   Anzahl: {}\n"
				"Doppelklick: einpacken   Rechtsklick: anlegen",
				itemModel->getName(), worldItem.o.bStatus[0],
				worldItem.o.ubNumberOfObjects);
			++slot;
		}
		for (size_t i = 0; i < gLootRegions.size(); ++i)
		{
			if (nextHelp[i] != gLootHelp[i])
			{
				gLootHelp[i] = nextHelp[i];
				gLootRegions[i].SetFastHelpText(nextHelp[i]);
			}
		}

		if (slot == 0)
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(gLootX + 15, gLootY + 35, "EMPTY");
		}
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gLootX + 12, gLootY + BAG_H - 30,
			"DOUBLE: PACK   RIGHT: EQUIP   DRAG: MOVE");
	}

	void DrawBag()
	{
		SOLDIERTYPE* const inventorySoldier = gInventorySoldier ?
			gInventorySoldier : GetSelectedMan();
		if (gTutorialActive)
		{
			if (!gBagVisible) return;
			if (gTutorialStep == 4)
			{
				DrawGearMode(inventorySoldier);
				TutorialText(12, BAG_H - 32, "DRAG ITEMS / RIGHT CLICK FOR ACTIONS",
					FONT_MCOLOR_LTGRAY);
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "NEXT", FONT_MCOLOR_RED);
			}
			else
			{
				DrawTutorial();
			}
			InvalidateRegion(gBagX, gBagY, gBagX + PANE_W, gBagY + BAG_H);
			return;
		}
		if (gBagVisible)
		{
			// CHARACTER is no longer a tab or a mutable context target. It is a
			// persistent RPG sheet for the controlled/explicitly opened merc.
			DrawGearMode(inventorySoldier ? inventorySoldier : GetSelectedMan());
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gBagX + 8, gBagY + BAG_H - 14, "CHARACTER");
			MPrint(gBagX + PANE_W - 13, gBagY + 4, "X");
			InvalidateRegion(gBagX, gBagY, gBagX + PANE_W, gBagY + BAG_H);
		}
	}

	void DrawContextMenu()
	{
		if (!gContextVisible || gContextEntryCount == 0) return;
		const INT16 width = 168;
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gContextX, gContextY,
			gContextX + width - 1, gContextY + height - 1, dark);
		OutlineBox(gContextX, gContextY, width, height, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gContextX + 1, gContextY + 15,
			gContextX + width - 2, gContextY + 15, mutedRed);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gContextX + 6, gContextY + 4, gContextTitle.left(24));
		for (size_t i = 0; i < gContextEntryCount; ++i)
		{
			SetFontForeground(gContextEntries[i].enabled ?
				FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(gContextX + 8,
				gContextY + 21 + static_cast<INT16>(i) * 18,
				gContextEntries[i].label);
		}
		InvalidateRegion(gContextX, gContextY,
			gContextX + width, gContextY + height);
	}

	void DrawHoverInspector()
	{
		if (!gHoverVisible || gContextVisible || gTutorialActive) return;
		auto inside = [](INT16 x, INT16 y, INT16 w, INT16 h)
		{
			return gusMouseXPos >= x && gusMouseXPos < x + w &&
				gusMouseYPos >= y && gusMouseYPos < y + h;
		};
		if ((gBagVisible && inside(gBagX, gBagY, PANE_W, BAG_H)) ||
			(gInventoryVisible && inside(gInventoryX, gInventoryY, PANE_W, BAG_H)) ||
			(gLootVisible && inside(gLootX, gLootY, PANE_W, BAG_H))) return;

		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gHoverX, gHoverY,
			gHoverX + 193, gHoverY + 36, dark);
		OutlineBox(gHoverX, gHoverY, 194, 37, red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gHoverX + 6, gHoverY + 6, gHoverTitle.left(28));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(gHoverX + 6, gHoverY + 20, gHoverDetail.left(34));
		InvalidateRegion(gHoverX, gHoverY, gHoverX + 194, gHoverY + 37);
	}

	void DrawWorldSelection()
	{
		const GridNo gridNo = gInspectedSoldier ?
			gInspectedSoldier->sGridNo : gInspectedGridNo;
		const UINT8 level = gInspectedSoldier ?
			gInspectedSoldier->bLevel : gInspectedLevel;
		if (gridNo < 0 || gridNo >= WORLD_MAX) return;
		INT16 x;
		INT16 y;
		GetGridNoScreenPos(gridNo, level, &x, &y);
		OS0MapWorldToDisplayScreen(&x, &y);
		if (x < gsVIEWPORT_START_X + 18 || x > gsVIEWPORT_END_X - 18 ||
			y < gsVIEWPORT_WINDOW_START_Y + 14 ||
			y > gsVIEWPORT_WINDOW_END_Y - 16) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		OutlineBox(x - 15, y - 10, 8, 8, red);
		OutlineBox(x + 8, y - 10, 8, 8, red);
		OutlineBox(x - 15, y + 5, 8, 8, red);
		OutlineBox(x + 8, y + 5, 8, 8, red);
		InvalidateRegion(x - 17, y - 12, x + 18, y + 15);
	}

	BOOLEAN FinalizeWorldMove()
	{
		if (gWorldMoveSource < 0 || gWorldMoveSource >= WORLD_MAX ||
			gWorldMoveDestination < 0 || gWorldMoveDestination >= WORLD_MAX ||
			gWorldMoveDestination == gWorldMoveSource ||
			gWorldMoveTileIndex >= NUMBEROFTILES) return FALSE;

		STRUCTURE* const structure = WorldStructureAt(gWorldMoveSource, 0,
			gWorldMoveTileIndex);
		LEVELNODE* const sourceNode = WorldLevelNodeAt(gWorldMoveSource, 0,
			gWorldMoveTileIndex);
		if (!structure || !sourceNode || !structure->pDBStructureRef ||
			!OkayToAddStructureToWorld(gWorldMoveDestination, 0,
				structure->pDBStructureRef, INVALID_STRUCTURE_ID)) return FALSE;

		try
		{
			// The destination is created before the source is touched. A rejected
			// placement therefore cannot eat the original crate or its contents.
			RestoreWorldMoveShade();
			LEVELNODE* const destinationNode =
				AddStructToTail(gWorldMoveDestination, gWorldMoveTileIndex);
			if (!destinationNode) return FALSE;
			// Keep the lighting initialized by the destination tile. Copying the
			// source palette state made carried objects change colour after placing.
			RemoveStructFromLevelNode(gWorldMoveSource, sourceNode);
			MoveItemPools(gWorldMoveSource, gWorldMoveDestination);
			RecompileLocalMovementCosts(gWorldMoveSource);
			RecompileLocalMovementCosts(gWorldMoveDestination);
			ErasePath();
			gfPlotNewMovement = TRUE;
			if (gLootGridNo == gWorldMoveSource)
				gLootGridNo = gWorldMoveDestination;
			if (gInspectedGridNo == gWorldMoveSource)
			{
				gInspectedGridNo = gWorldMoveDestination;
				gLootGridNo = gWorldMoveDestination;
				CaptureInspectorPreview(gWorldMoveDestination, gInspectedLevel);
			}
			return TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}

	void UpdateWorldMove()
	{
		// JA2's interactive-tile hover restores its own shade when the cursor
		// leaves the source. Reassert the carry shade until placement/cancel.
		if ((gWorldMovePending || gWorldMoveWalking) && gWorldMoveSourceShaded)
		{
			if (LEVELNODE* const node = WorldLevelNodeAt(gWorldMoveSource, 0,
				gWorldMoveTileIndex)) node->ubShadeLevel = SHADE_MIN;
		}
		if (!gWorldMoveWalking) return;
		if (!gWorldMoveCarrier || !gWorldMoveCarrier->bActive ||
			gWorldMoveCarrier->bLife <= 0)
		{
			ClearWorldMoveState();
			return;
		}
		if (gWorldMoveCarrier->sGridNo != gWorldMoveActionGrid) return;

		if (!FinalizeWorldMove())
		{
			gWorldMoveWalking = FALSE;
			gWorldMovePending = TRUE;
			gWorldMoveDestination = NOWHERE;
			gWorldMoveActionGrid = NOWHERE;
			gCursorAction = 2;
			ShadeWorldMoveSource();
			guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		ClearWorldMoveState();
		gCursorAction = 0;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawCarryGhost(INT16 x, INT16 y)
	{
		if (x < gsVIEWPORT_START_X + 15 || x > gsVIEWPORT_END_X - 15 ||
			y < gsVIEWPORT_WINDOW_START_Y + 12 ||
			y > gsVIEWPORT_WINDOW_END_Y - 12) return;
		const UINT16 grey = Get16BPPColor(FROMRGB(112, 116, 116));
		const UINT16 darkGrey = Get16BPPColor(FROMRGB(42, 46, 46));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x - 12, y - 8, x + 12, y + 8,
			darkGrey);
		OutlineBox(x - 12, y - 8, 25, 17, grey);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x - 9, y - 5, x + 9, y - 4,
			grey);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x - 9, y + 4, x + 9, y + 5,
			grey);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x - 1, y - 5, x + 1, y + 5,
			grey);
		InvalidateRegion(x - 14, y - 10, x + 14, y + 10);
	}

	void DrawActionMenu()
	{
		if (gCursorAction != 2 && !gWorldMovePending && !gWorldMoveWalking) return;
		if (gWorldMovePending)
			DrawCarryGhost(gusMouseXPos, gusMouseYPos);
		else if (gWorldMoveWalking && gWorldMoveDestination >= 0)
		{
			INT16 x;
			INT16 y;
			GetGridNoScreenPos(gWorldMoveDestination, 0, &x, &y);
			OS0MapWorldToDisplayScreen(&x, &y);
			DrawCarryGhost(x, y);
		}
		else
		{
			const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
			// Opposing pixel corners identify ENVIRONMENT CARRY without
			// resurrecting a label or radial panel.
			OutlineBox(gusMouseXPos - 10, gusMouseYPos - 10, 7, 7, red);
			OutlineBox(gusMouseXPos + 4, gusMouseYPos + 4, 7, 7, red);
			InvalidateRegion(gusMouseXPos - 13, gusMouseYPos - 13,
				gusMouseXPos + 13, gusMouseYPos + 13);
		}
	}

	void GodIconCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gGodLibraryVisible)
			return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index < gGodIconNames.size())
		{
			gGodMenuIcon = static_cast<UINT8>(index);
			RecordFeedbackEvent(ST::format("GOD ICON {} / {}",
				index, gGodIconNames[index]));
		}
		gGodLibraryVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawGodIconLibrary()
	{
		if (!gGodLibraryVisible || !gGodNewIcons || !gGodDoorIcons ||
			!gGodButtonFrame) return;

		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gGodLibraryX, gGodLibraryY,
			gGodLibraryX + GOD_LIBRARY_W - 1, gGodLibraryY + GOD_LIBRARY_H - 1,
			dark);
		OutlineBox(gGodLibraryX, gGodLibraryY, GOD_LIBRARY_W, GOD_LIBRARY_H, red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gGodLibraryX + 6, gGodLibraryY + 5,
			"GOD MODE / JA2 ICON LIBRARY");

		for (INT16 frame = 0; frame < 3; ++frame)
			BltVideoObject(FRAME_BUFFER, gGodButtonFrame, 0,
				gGodLibraryX + frame * 78, gGodLibraryY + 17);

		for (size_t i = 0; i < gGodIconNames.size(); ++i)
		{
			const size_t cell = (i / 8) * 9 + (i % 8);
			const INT16 frame = static_cast<INT16>(cell / 9);
			const INT16 local = static_cast<INT16>(cell % 9);
			const INT16 x = gGodLibraryX + frame * 78 + 9 + (local % 3) * 20;
			const INT16 y = gGodLibraryY + 25 + (local / 3) * 20;
			if (i < 15)
				BltVideoObject(FRAME_BUFFER, gGodNewIcons,
					static_cast<UINT16>(i * 3), x, y);
			else
			{
				constexpr std::array<UINT16, 9> doorFrames{{
					0, 3, 6, 9, 12, 15, 18, 21, 25
				}};
				BltVideoObject(FRAME_BUFFER, gGodDoorIcons,
					doorFrames[i - 15], x, y);
			}
			if (i == gGodMenuIcon) OutlineBox(x - 1, y - 1, 20, 20, red);
		}

		// The original cancel glyph closes the atlas without changing selection.
		BltVideoObject(FRAME_BUFFER, gGodNewIcons, 15,
			gGodLibraryX + 2 * 78 + 49, gGodLibraryY + 65);
		SetFontForeground(FONT_WHITE);
		MPrint(gGodLibraryX + 7, gGodLibraryY + 98,
			ST::format("SELECTED {} / {}", gGodMenuIcon,
				gGodIconNames[gGodMenuIcon]).left(36));
		InvalidateRegion(gGodLibraryX, gGodLibraryY,
			gGodLibraryX + GOD_LIBRARY_W, gGodLibraryY + GOD_LIBRARY_H);
	}

	void AssetCatalogCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gAssetCatalogVisible)
			return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		switch (index)
		{
			case 0:
				gAssetCatalogNameEditing = TRUE;
				SetUIKeyboardHook(AssetCatalogKeyboardHook);
				break;
			case 1:
				gCatalogDraft.category = static_cast<AssetCategory>(
					(static_cast<UINT8>(gCatalogDraft.category) + 1) %
					static_cast<UINT8>(AssetCategory::COUNT));
				break;
			case 2:
				gCatalogDraft.material = static_cast<AssetMaterial>(
					(static_cast<UINT8>(gCatalogDraft.material) + 1) %
					static_cast<UINT8>(AssetMaterial::COUNT));
				break;
			case 3:
				gCatalogDraft.role = static_cast<AssetRole>(
					(static_cast<UINT8>(gCatalogDraft.role) + 1) %
					static_cast<UINT8>(AssetRole::COUNT));
				break;
			case 4: gCatalogDraft.width = std::max<UINT8>(1, gCatalogDraft.width - 1); break;
			case 5: gCatalogDraft.width = std::min<UINT8>(12, gCatalogDraft.width + 1); break;
			case 6: gCatalogDraft.height = std::max<UINT8>(1, gCatalogDraft.height - 1); break;
			case 7: gCatalogDraft.height = std::min<UINT8>(12, gCatalogDraft.height + 1); break;
			case 8: gCatalogDraft.buildable = !gCatalogDraft.buildable; break;
			case 9:
			{
				AssetCatalogRecord* record = FindAssetCatalogRecord(
					gCatalogDraft.tileset, gCatalogDraft.tileIndex);
				if (record) *record = gCatalogDraft;
				else gAssetCatalog.push_back(gCatalogDraft);
				const BOOLEAN saved = WriteAssetCatalog();
				RecordFeedbackEvent(ST::format("ASSET CATALOG {} tile {} category {} {}x{}",
					saved ? "SAVED" : "FAILED", gCatalogDraft.tileIndex,
					gAssetCategoryNames[static_cast<size_t>(gCatalogDraft.category)],
					gCatalogDraft.width, gCatalogDraft.height));
				gAssetCatalogVisible = FALSE;
				gAssetCatalogNameEditing = FALSE;
				SetUIKeyboardHook(nullptr);
				if (gInspectedGridNo >= 0 && gInspectedTileIndex < NUMBEROFTILES)
					gContextTitle = DescribeWorldAsset(gInspectedGridNo,
						gInspectedLevel, gInspectedTileIndex).displayName;
				RefreshPanelActions();
				break;
			}
			case 10:
				gAssetCatalogVisible = FALSE;
				gAssetCatalogNameEditing = FALSE;
				SetUIKeyboardHook(nullptr);
				break;
			default: break;
		}
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawAssetCatalog()
	{
		if (!gAssetCatalogVisible) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 darkRed = Get16BPPColor(FROMRGB(78, 5, 5));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gAssetCatalogX, gAssetCatalogY,
			gAssetCatalogX + ASSET_CATALOG_W - 1,
			gAssetCatalogY + ASSET_CATALOG_H - 1, dark);
		OutlineBox(gAssetCatalogX, gAssetCatalogY,
			ASSET_CATALOG_W, ASSET_CATALOG_H, red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gAssetCatalogX + 7, gAssetCatalogY + 6,
			ST::format("GOD / ASSET DB  TS{} TILE{}",
				gCatalogDraft.tileset, gCatalogDraft.tileIndex));

		auto row = [&](INT16 y, const ST::string& text)
		{
			OutlineBox(gAssetCatalogX + 8, gAssetCatalogY + y, 302, 18, darkRed);
			SetFontForeground(FONT_WHITE);
			MPrint(gAssetCatalogX + 14, gAssetCatalogY + y + 5, text);
		};
		row(25, ST::format("NAME      {}{}",
			gAssetCatalogNameEditing ? "> " : "  ",
			gCatalogDraft.label.empty() ? "UNNAMED ASSET" : gCatalogDraft.label));
		row(47, ST::format("CATEGORY  < {} >",
			gAssetCategoryNames[static_cast<size_t>(gCatalogDraft.category)]));
		row(69, ST::format("MATERIAL  < {} >",
			gAssetMaterialNames[static_cast<size_t>(gCatalogDraft.material)]));
		row(91, ST::format("ROLE      < {} >",
			gAssetRoleNames[static_cast<size_t>(gCatalogDraft.role)]));

		const std::array<ST::string, 4> sizeLabels{{
			"WIDTH -", "WIDTH +", "HEIGHT -", "HEIGHT +"
		}};
		const std::array<INT16, 4> sizeX{{ 8, 77, 169, 238 }};
		for (size_t i = 0; i < sizeLabels.size(); ++i)
		{
			OutlineBox(gAssetCatalogX + sizeX[i], gAssetCatalogY + 116, 64, 18, darkRed);
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(gAssetCatalogX + sizeX[i] + 5, gAssetCatalogY + 121, sizeLabels[i]);
		}
		SetFontForeground(FONT_WHITE);
		MPrint(gAssetCatalogX + 145, gAssetCatalogY + 121,
			ST::format("{}x{}", gCatalogDraft.width, gCatalogDraft.height));
		row(139, ST::format("BUILDABLE / PLACEABLE  < {} >",
			gCatalogDraft.buildable ? "YES" : "NO"));
		OutlineBox(gAssetCatalogX + 8, gAssetCatalogY + 164, 86, 21, darkRed);
		OutlineBox(gAssetCatalogX + 182, gAssetCatalogY + 164, 128, 21, red);
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(gAssetCatalogX + 31, gAssetCatalogY + 171, "CANCEL");
		SetFontForeground(FONT_WHITE);
		MPrint(gAssetCatalogX + 207, gAssetCatalogY + 171, "SAVE TO DATABASE");
		InvalidateRegion(gAssetCatalogX, gAssetCatalogY,
			gAssetCatalogX + ASSET_CATALOG_W, gAssetCatalogY + ASSET_CATALOG_H);
	}
}


void InitializeOS0IngameUI()
{
	if (gInitialized) return;

	const INT16 centerX = std::max<INT16>(0, (gsVIEWPORT_END_X - PANE_W) / 2);
	const INT16 centerY = std::max<INT16>(0, (gsVIEWPORT_END_Y - BAG_H) / 2);
	gBagX = centerX;
	gBagY = std::max<INT16>(0, gsVIEWPORT_END_Y - BAG_H - 8);
	gInventoryX = gBagX;
	gInventoryY = gBagY;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)] =
		{ 8, 8, CONTEXT_PANEL_W, CONTEXT_PANEL_H, FALSE, FALSE, FALSE };
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLS)] =
		{ 8, std::min<INT16>(gsVIEWPORT_END_Y - TOOLS_PANEL_H,
			CONTEXT_PANEL_H + 14), TOOLS_PANEL_W, TOOLS_PANEL_H, TRUE, FALSE, FALSE };
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)] =
		{ std::max<INT16>(0, gsVIEWPORT_END_X - ACTIONS_PANEL_W - 8), 8,
			ACTIONS_PANEL_W, ACTIONS_PANEL_H, FALSE, FALSE, FALSE };
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)] =
		{ centerX, std::max<INT16>(8, centerY - 40), PANE_W, BAG_H,
			FALSE, FALSE, FALSE };
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)] =
		{ std::max<INT16>(0, (gsVIEWPORT_END_X - SECTOR_PANEL_W) / 2), 24,
			SECTOR_PANEL_W, SECTOR_PANEL_H, FALSE, FALSE, FALSE };
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::FEEDBACK)] =
		{ std::max<INT16>(0, (gsVIEWPORT_END_X - FEEDBACK_PANEL_W) / 2),
			std::max<INT16>(8, (gsVIEWPORT_END_Y - FEEDBACK_PANEL_H) / 2),
			FEEDBACK_PANEL_W, FEEDBACK_PANEL_H, FALSE, FALSE, FALSE };
	gLootX = gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].x;
	gLootY = gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].y;
	gOrbX = 8;
	gOrbY = std::max<INT16>(0, gsVIEWPORT_END_Y - 34);
	gGodLibraryX = std::max<INT16>(0,
		(gsVIEWPORT_END_X - GOD_LIBRARY_W) / 2);
	gGodLibraryY = std::max<INT16>(4,
		(gsVIEWPORT_WINDOW_END_Y - GOD_LIBRARY_H) / 2);
	gAssetCatalogX = std::max<INT16>(0,
		(gsVIEWPORT_END_X - ASSET_CATALOG_W) / 2);
	gAssetCatalogY = std::max<INT16>(4,
		(gsVIEWPORT_WINDOW_END_Y - ASSET_CATALOG_H) / 2);
	LoadAssetCatalog();
	gGodNewIcons = AddVideoObjectFromFile(INTERFACEDIR "/newicons3.sti");
	gGodDoorIcons = AddVideoObjectFromFile(INTERFACEDIR "/door_op2.sti");
	gGodButtonFrame = AddVideoObjectFromFile(INTERFACEDIR "/button_frame.sti");

	MSYS_DefineRegion(&gBagBlock, 0, 0, PANE_W, BAG_H,
		MSYS_PRIORITY_HIGH, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
	MSYS_DefineRegion(&gBagGrabber, 0, 0, PANE_W, GRABBER_H,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, BagGrabberCallback, BagGrabberCallback);
	MSYS_DefineRegion(&gBagClose, 0, 0, 15, 15,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagCloseCallback);
	MSYS_DefineRegion(&gContextBlock, 0, 0, 168, 200,
		MSYS_PRIORITY_HIGH, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
	MSYS_DefineRegion(&gGodLibraryBlock, 0, 0, GOD_LIBRARY_W, GOD_LIBRARY_H,
		MSYS_PRIORITY_HIGH, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
	for (size_t i = 0; i < gGodIconRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gGodIconRegions[i], 0, 0, 20, 20,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			GodIconCallback);
		gGodIconRegions[i].SetUserData<0>(i);
		gGodIconRegions[i].Disable();
	}
	MSYS_DefineRegion(&gAssetCatalogBlock, 0, 0,
		ASSET_CATALOG_W, ASSET_CATALOG_H, MSYS_PRIORITY_HIGH,
		CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
	for (size_t i = 0; i < gAssetCatalogRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gAssetCatalogRegions[i], 0, 0, 20, 18,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			AssetCatalogCallback);
		gAssetCatalogRegions[i].SetUserData<0>(i);
		gAssetCatalogRegions[i].Disable();
	}
	for (size_t i = 0; i < gContextRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gContextRegions[i], 0, 0, 160, 17,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ContextActionCallback);
		gContextRegions[i].SetUserData<0>(i);
		gContextRegions[i].Disable();
	}
	for (size_t i = 0; i < gFloatingPanels.size(); ++i)
	{
		FloatingPanel const& panel = gFloatingPanels[i];
		MSYS_DefineRegion(&gFloatingPanelBlocks[i], 0, 0, panel.w, panel.h,
			MSYS_PRIORITY_HIGH, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
		MSYS_DefineRegion(&gFloatingPanelGrabbers[i], 0, 0, panel.w, PANEL_HEADER_H,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
			FloatingPanelGrabberCallback, FloatingPanelGrabberCallback);
		gFloatingPanelGrabbers[i].SetUserData<0>(i);
		MSYS_DefineRegion(&gFloatingPanelCloses[i], 0, 0, 15, 15,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			FloatingPanelCloseCallback);
		gFloatingPanelCloses[i].SetUserData<0>(i);
	}
	for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gPanelDockRegions[i], 0, 0, 41, 17,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			PanelDockCallback);
		gPanelDockRegions[i].SetUserData<0>(i);
	}
	for (size_t i = 0; i < gToolRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gToolRegions[i], 0, 0, TOOLS_PANEL_W - 14, 10,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ToolPanelCallback);
		gToolRegions[i].SetUserData<0>(i);
	}
	for (size_t i = 0; i < gActionPanelRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gActionPanelRegions[i], 0, 0, ACTIONS_PANEL_W - 14, 17,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ActionPanelCallback);
		gActionPanelRegions[i].SetUserData<0>(i);
	}
	for (size_t i = 0; i < gFeedbackRegions.size(); ++i)
	{
		const INT16 width = i == 0 || i == 1 ? FEEDBACK_PANEL_W - 16 :
			i == 2 ? 118 : 76;
		const INT16 height = i == 1 ? 104 : 19;
		MSYS_DefineRegion(&gFeedbackRegions[i], 0, 0, width, height,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			FeedbackCallback);
		gFeedbackRegions[i].SetUserData<0>(i);
		gFeedbackRegions[i].Disable();
	}
	for (size_t i = 0; i < gSectorUpgradeRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gSectorUpgradeRegions[i], 0, 0,
			SECTOR_PANEL_W - 16, 26, MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
			MSYS_NO_CALLBACK, SectorUpgradeCallback);
		gSectorUpgradeRegions[i].SetUserData<0>(i);
		gSectorUpgradeRegions[i].Disable();
	}
	for (size_t i = 0; i < gSlots.size(); ++i)
	{
		const SlotLayout& slot = gSlots[i];
		MSYS_DefineRegion(&gSlotRegions[i], 0, 0, slot.w, slot.h,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, SlotCallback);
		gSlotRegions[i].SetUserData<0>(slot.slot);
		MSYS_DefineRegion(&gObjectSlotRegions[i], 0, 0, slot.w, slot.h,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ObjectSoldierSlotCallback);
		gObjectSlotRegions[i].SetUserData<0>(slot.slot);
	}

	MSYS_DefineRegion(&gOrbRegion, gOrbX, gOrbY, gOrbX + 28, gOrbY + 28,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, OrbCallback, OrbCallback);
	MSYS_DefineRegion(&gTutorialContinue, 0, 0, 110, 22,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, TutorialContinueCallback);
	for (size_t i = 0; i < gTutorialStats.size(); ++i)
	{
		MSYS_DefineRegion(&gTutorialStats[i], 0, 0, 18, 14,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, TutorialStatCallback);
		gTutorialStats[i].SetUserData<0>(i / 2);
		gTutorialStats[i].SetUserData<1>(i % 2);
		gTutorialStats[i].Disable();
	}
	for (size_t i = 0; i < gTutorialTraitRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gTutorialTraitRegions[i], 0, 0, 145, 12,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			TutorialTraitCallback);
		gTutorialTraitRegions[i].SetUserData<0>(i);
		gTutorialTraitRegions[i].Disable();
	}
	for (size_t i = 0; i < gOpsActionRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gOpsActionRegions[i], 0, 0, 296, 18,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			OperationsActionCallback);
		gOpsActionRegions[i].SetUserData<0>(i);
		gOpsActionRegions[i].Disable();
	}
	for (size_t i = 0; i < gLootRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gLootRegions[i], 0, 0, 59, 28,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
			LootSlotCallback, LootSlotCallback);
		gLootRegions[i].SetUserData<0>(i);
		gLootRegions[i].Disable();
	}

	PositionBagRegions();
	SetBagRegionsEnabled(gBagVisible);
	gInitialized = TRUE;
	SetRenderFlags(RENDER_FLAG_FULL);
}


void ShutdownOS0IngameUI()
{
	if (!gInitialized) return;
	ClearWorldMoveState();
	CloseContextMenu();
	gHoverVisible = FALSE;
	if (gpItemPointer) CancelItemPointer();
	MSYS_RemoveRegion(&gBagBlock);
	MSYS_RemoveRegion(&gBagGrabber);
	MSYS_RemoveRegion(&gBagClose);
	MSYS_RemoveRegion(&gContextBlock);
	MSYS_RemoveRegion(&gGodLibraryBlock);
	for (MOUSE_REGION& r : gGodIconRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gAssetCatalogBlock);
	for (MOUSE_REGION& r : gAssetCatalogRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gContextRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFloatingPanelBlocks) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFloatingPanelGrabbers) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFloatingPanelCloses) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gPanelDockRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gToolRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gActionPanelRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFeedbackRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gSectorUpgradeRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gSlotRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gObjectSlotRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gOrbRegion);
	MSYS_RemoveRegion(&gTutorialContinue);
	for (MOUSE_REGION& r : gTutorialStats) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gTutorialTraitRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gOpsActionRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gLootRegions) MSYS_RemoveRegion(&r);
	if (gPortrait)
	{
		DeleteVideoObject(gPortrait);
		gPortrait = nullptr;
		gPortraitProfile = NO_PROFILE;
	}
	if (gGodNewIcons)
	{
		DeleteVideoObject(gGodNewIcons);
		gGodNewIcons = nullptr;
	}
	if (gGodDoorIcons)
	{
		DeleteVideoObject(gGodDoorIcons);
		gGodDoorIcons = nullptr;
	}
	if (gGodButtonFrame)
	{
		DeleteVideoObject(gGodButtonFrame);
		gGodButtonFrame = nullptr;
	}
	if (gWorldZoomBuffer)
	{
		DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = nullptr;
	}
	if (gInspectorPreview)
	{
		DeleteVideoSurface(gInspectorPreview);
		gInspectorPreview = nullptr;
	}
	SetUIKeyboardHook(nullptr);
	gFeedbackEditing = FALSE;
	gInspectedSoldier = nullptr;
	gInventorySoldier = nullptr;
	gInspectedGridNo = NOWHERE;
	gLootGridNo = NOWHERE;
	gInspectedTileIndex = NO_TILE;
	gLootTileIndex = NO_TILE;
	gInventoryVisible = FALSE;
	gLootVisible = FALSE;
	gLootDragCandidate = -1;
	gPanelInteractionGuardUntil = 0;
	gLootIgnoreInputUntil = 0;
	gAimAutoCollapsed = FALSE;
	gBagVisibleBeforeAim = FALSE;
	gFieldToolIssued = FALSE;
	gContentsMode = ContentsMode::SOLDIER;
	gGodLibraryVisible = FALSE;
	gAssetCatalogVisible = FALSE;
	gAssetCatalogNameEditing = FALSE;
	gInitialized = FALSE;
}


void RenderOS0IngameUI()
{
	if (!gInitialized) InitializeOS0IngameUI();
	UpdateWindowDragging();
	if (!gTutorialActive && !gFieldToolIssued)
	{
		if (SOLDIERTYPE* const selected = GetSelectedMan())
		{
			EnsureFieldShovel(selected);
			gFieldToolIssued = TRUE;
		}
	}
	if (!gTutorialActive)
	{
		const BOOLEAN aiming = gCurrentUIMode == ACTION_MODE ||
			gCurrentUIMode == CONFIRM_ACTION_MODE;
		if (aiming && !gAimAutoCollapsed)
		{
			StopFeedbackEditing();
			gBagVisibleBeforeAim = gBagVisible;
			for (FloatingPanel& panel : gFloatingPanels)
			{
				panel.visibleBeforeAim = panel.visible;
				panel.visible = FALSE;
			}
			gAimAutoCollapsed = TRUE;
			gBagVisible = FALSE;
			gGodLibraryVisible = FALSE;
			gAssetCatalogVisible = FALSE;
			gAssetCatalogNameEditing = FALSE;
			SetUIKeyboardHook(nullptr);
			CloseContextMenu();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if (!aiming && gAimAutoCollapsed)
		{
			gAimAutoCollapsed = FALSE;
			gBagVisible = gBagVisibleBeforeAim;
			gBagVisibleBeforeAim = FALSE;
			for (FloatingPanel& panel : gFloatingPanels)
			{
				panel.visible = panel.visibleBeforeAim;
				panel.visibleBeforeAim = FALSE;
			}
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
	UpdateWorldMove();
	if (gLootVisible && !IsInspectedWorldAssetNear())
	{
		gLootVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
	}
	if (!gTutorialActive && gInspectedGridNo == NOWHERE)
	{
		// An allied inspector is a live view of the current selection. Enemy
		// and world-asset inspectors stay pinned until another object is opened.
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (selected && (!gInspectedSoldier ||
			gInspectedSoldier->bTeam == OUR_TEAM))
		{
			gInspectedSoldier = selected;
			gInventorySoldier = selected;
		}
	}
	if (gTutorialActive)
	{
		// The vanilla IMP arrival quote ("find Miguel...") conflicts with the
		// playable introduction. Keep the tutorial quiet without muting later
		// player-triggered conversations.
		EmptyDialogueQueue();
		StopAnyCurrentlyTalkingSpeech();
	}
	if (gBagVisible) DrawBag();
	if (!gTutorialActive)
	{
		DrawContextPanel();
		DrawToolsPanel();
		DrawActionsPanel();
		DrawSectorPanel();
		const FloatingPanel& objectPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)];
		if (objectPanel.visible)
		{
			if (gContentsMode == ContentsMode::SOLDIER && gInspectedSoldier &&
				gInspectedSoldier->bTeam != OUR_TEAM)
				DrawObjectSoldierInventory(gInspectedSoldier);
			else DrawLootMode();
		}
	}
	DrawWorldSelection();
	DrawOrb();
	DrawActionMenu();
	DrawHoverInspector();
	DrawContextMenu();
	DrawFeedbackPanel();
	DrawGodIconLibrary();
	DrawAssetCatalog();

	// The tactical renderer uses dirty rectangles. A full refresh while moving
	// prevents the "hall of mirrors" trails visible in the previous prototype.
	BOOLEAN floatingPanelDragging = FALSE;
	for (FloatingPanel const& panel : gFloatingPanels)
		floatingPanelDragging |= panel.dragging;
	if (gContextVisible || gHoverVisible || gGodLibraryVisible ||
		gAssetCatalogVisible || gBagDragging || floatingPanelDragging ||
		gOrbDragging || gWorldMovePending ||
		gWorldMoveWalking)
		SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0OpenCharacterPanel(SOLDIERTYPE* soldier)
{
	if (!soldier || GetJA2Clock() < gPanelInteractionGuardUntil) return;
	CloseContextMenu();
	gInspectedSoldier = soldier;
	gContentsMode = ContentsMode::SOLDIER;
	gInspectedGridNo = NOWHERE;
	const BOOLEAN contentsAvailable = CanAccessSoldierContents(soldier);
	gMode = contentsAvailable ? ComputerMode::CONTENTS : ComputerMode::INFO;
	if (soldier->bTeam == OUR_TEAM)
	{
		gInventorySoldier = soldier;
		gBagVisible = TRUE;
	}
	gInventoryVisible = contentsAvailable;
	gLootVisible = FALSE;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
	if (soldier->bTeam != OUR_TEAM && contentsAvailable)
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = TRUE;
	RefreshPanelActions();
	gPanelInteractionGuardUntil = GetJA2Clock() + 140;
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0ExecuteCharacterQuickAction(SOLDIERTYPE* soldier,
	OS0CharacterQuickAction action)
{
	if (action == OS0CharacterQuickAction::ICON_LIBRARY)
	{
		CloseContextMenu();
		gGodLibraryVisible = TRUE;
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		return;
	}
	if (!soldier || soldier->bTeam != OUR_TEAM) return;

	switch (action)
	{
		case OS0CharacterQuickAction::CHARACTER:
			CloseContextMenu();
			gInspectedSoldier = soldier;
			gInventorySoldier = soldier;
			gInspectedGridNo = NOWHERE;
			gContentsMode = ContentsMode::SOLDIER;
			gMode = ComputerMode::INFO;
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
			RefreshPanelActions();
			break;
		case OS0CharacterQuickAction::INVENTORY:
			OS0OpenCharacterPanel(soldier);
			return;
		case OS0CharacterQuickAction::STEALTH:
			soldier->bStealthMode = !soldier->bStealthMode;
			break;
		case OS0CharacterQuickAction::WEAPON_MODE:
			if (soldier->inv[HANDPOS].usItem != NOTHING &&
				GCM->getItem(soldier->inv[HANDPOS].usItem)->isGun())
				ChangeWeaponMode(soldier);
			break;
		case OS0CharacterQuickAction::RELOAD:
			if (soldier->inv[HANDPOS].usItem != NOTHING &&
				GCM->getItem(soldier->inv[HANDPOS].usItem)->isGun())
				AutoReload(soldier);
			break;
		case OS0CharacterQuickAction::ICON_LIBRARY:
			break;
	}
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}


UINT8 OS0GetGodMenuIcon()
{
	return gGodMenuIcon;
}

BOOLEAN OS0SelectWorldObject(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex)
{
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return FALSE;
	CloseContextMenu();
	if (target)
	{
		gInspectedSoldier = target;
		gInspectedGridNo = NOWHERE;
		gContentsMode = ContentsMode::SOLDIER;
		if (target->bTeam == OUR_TEAM) gInventorySoldier = target;
		gInventoryVisible = CanAccessSoldierContents(target);
		gMode = target->bTeam == OUR_TEAM ?
			ComputerMode::CONTENTS : ComputerMode::INFO;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = FALSE;
		RefreshPanelActions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		// Let JA2 keep its normal allied selection mechanics. Contacts are
		// inspection-only and therefore consume the click.
		return target->bTeam != OUR_TEAM;
	}
	if (gridNo < 0 || gridNo >= WORLD_MAX) return FALSE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	RecordFeedbackEvent(ST::format("CONTEXT grid {} level {} tile {}{}",
		gridNo, level, tileIndex, target ? " merc" : ""));
	const BOOLEAN hasItems = GetItemPool(gridNo, level) != nullptr;
	const BOOLEAN hasAsset = tileIndex < NUMBEROFTILES;
	if (!hasItems && !hasAsset) return FALSE;
	gContextTitle = hasAsset ? DescribeWorldAsset(gridNo, level, tileIndex).displayName :
		(hasItems ? "GROUND ITEMS" : "WORLD ASSET");
	const BOOLEAN sameWorldSelection =
		gInspectedSoldier == nullptr &&
		gInspectedGridNo == gridNo &&
		gInspectedLevel == level &&
		gInspectedTileIndex == tileIndex;

	gInspectedSoldier = nullptr;
	gInspectedGridNo = gridNo;
	gInspectedLevel = level;
	gInspectedTileIndex = tileIndex;
	gLootGridNo = gridNo;
	gLootLevel = level;
	gLootTileIndex = tileIndex;
	gMode = ComputerMode::CONTENTS;
	gContentsMode = ContentsMode::WORLD;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
	// Selection must not spawn a window underneath the first click. Otherwise
	// that new region swallows the second half of a double-click. Crucially, a
	// trailing LBUTTON_UP after a double-click is the *same* selection and must
	// not close the loot window that the double-click has just opened.
	if (!sameWorldSelection)
	{
		gLootVisible = FALSE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = FALSE;
	}
	CaptureInspectorPreview(gridNo, level);
	RefreshPanelActions();
	PositionBagRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}

void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
	ITEM_POOL* const pool = validGrid ? GetItemPool(gridNo, level) : nullptr;
	const BOOLEAN hasAsset = validGrid && tileIndex < NUMBEROFTILES;
	if (!target && !pool && !hasAsset)
	{
		if (gHoverVisible) SetRenderFlags(RENDER_FLAG_FULL);
		gHoverVisible = FALSE;
		return;
	}

	if (target)
	{
		gHoverTitle = target->name;
		gHoverDetail = ST::format("HP {}/{}  {}  RMB ACTIONS",
			target->bLife, target->bLifeMax,
			target->bTeam == OUR_TEAM ? "OPERATOR" : "CONTACT");
	}
	else if (pool && pool->iItemIndex >= 0 &&
		static_cast<size_t>(pool->iItemIndex) < gWorldItems.size())
	{
		WORLDITEM const& worldItem = GetWorldItem(pool->iItemIndex);
		gHoverTitle = worldItem.o.usItem != NOTHING ?
			GCM->getItem(worldItem.o.usItem)->getName() : "GROUND ITEMS";
		gHoverDetail = "LOOT  LMB SELECT  RMB ACTIONS";
	}
	else
	{
		gHoverTitle = DescribeWorldAsset(gridNo, level, tileIndex).displayName;
		gHoverDetail = "LMB SELECT  RMB ACTIONS  DOUBLE OPEN";
	}

	constexpr INT16 width = 194;
	constexpr INT16 height = 37;
	const INT16 proposedX = screenX + 16 + width <= gsVIEWPORT_END_X ?
		screenX + 16 : screenX - width - 10;
	const INT16 proposedY = screenY + 18 + height <= gsVIEWPORT_WINDOW_END_Y ?
		screenY + 18 : screenY - height - 10;
	gHoverX = std::clamp<INT16>(proposedX, gsVIEWPORT_START_X,
		std::max<INT16>(gsVIEWPORT_START_X, gsVIEWPORT_END_X - width));
	gHoverY = std::clamp<INT16>(proposedY, gsVIEWPORT_WINDOW_START_Y,
		std::max<INT16>(gsVIEWPORT_WINDOW_START_Y,
			gsVIEWPORT_WINDOW_END_Y - height));
	gHoverVisible = TRUE;
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY)
{
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	CloseContextMenu();
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	gContextSoldier = target;
	gContextGridNo = gridNo;
	gContextLevel = level;
	gContextTileIndex = tileIndex;
	gContextInventorySlot = NO_SLOT;

	SOLDIERTYPE* const selected = GetSelectedMan();
	const BOOLEAN hasItems = gridNo >= 0 && gridNo < WORLD_MAX &&
		GetItemPool(gridNo, level) != nullptr;
	const BOOLEAN hasAsset = tileIndex < NUMBEROFTILES;
	const BOOLEAN hasTerrain = gridNo >= 0 && gridNo < WORLD_MAX && level == 0 &&
		gpWorldLevelData[gridNo].pLandHead != nullptr;

	if (target)
	{
		OS0OpenCharacterPanel(target);
		gContextSoldier = target;
		gContextTitle = target->name;
		const BOOLEAN own = target->bTeam == OUR_TEAM;
		AddContextEntry(ContextAction::INSPECT, "INSPECT / INFO");
		AddContextEntry(ContextAction::CONTENTS,
			own ? "INVENTORY" : "LOOT / CONTENTS",
			CanAccessSoldierContents(target));
		if (own)
		{
			AddContextEntry(ContextAction::STAND, "STANCE / STAND");
			AddContextEntry(ContextAction::CROUCH, "STANCE / CROUCH");
			AddContextEntry(ContextAction::PRONE, "STANCE / PRONE");
			AddContextEntry(ContextAction::STEALTH,
				target->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON");
			const OBJECTTYPE& hand = target->inv[HANDPOS];
			const BOOLEAN gunReady = hand.usItem != NOTHING &&
				GCM->getItem(hand.usItem)->getItemClass() == IC_GUN;
			if (gunReady)
			{
				const char* const mode =
					target->bWeaponMode == WM_BURST ? "BURST" :
					target->bWeaponMode == WM_ATTACHED ? "ATTACHED" : "SINGLE";
				AddContextEntry(ContextAction::WEAPON_MODE,
					ST::format("WEAPON MODE / {}", mode));
				AddContextEntry(ContextAction::RELOAD, "RELOAD WEAPON");
				AddContextEntry(ContextAction::UNLOAD,
					ST::format("UNLOAD MAGAZINE / {}", hand.ubGunShotsLeft),
					hand.ubGunShotsLeft > 0);
				AddContextEntry(ContextAction::SWAP_HANDS, "SWAP HANDS");
			}
		}
		else
		{
			AddContextEntry(ContextAction::TALK, "TALK",
				target->bLife >= OKLIFE);
			const BOOLEAN armed = selected &&
				selected->inv[HANDPOS].usItem != NOTHING &&
				GCM->getItem(selected->inv[HANDPOS].usItem)->isWeapon();
			AddContextEntry(ContextAction::ATTACK, "ATTACK", armed);
		}
	}
	else if (hasItems || hasAsset)
	{
		OS0SelectWorldObject(nullptr, gridNo, level, tileIndex);
		gContextSoldier = nullptr;
		gContextGridNo = gridNo;
		gContextLevel = level;
		gContextTileIndex = tileIndex;
		ITEM_POOL* const pool = hasItems ? GetItemPool(gridNo, level) : nullptr;
		gContextWorldItemIndex = pool ? pool->iItemIndex : -1;
		STRUCTURE const* const structure = hasAsset ?
			WorldStructureAt(gridNo, level, tileIndex) : nullptr;
		gContextTitle = hasAsset ? DescribeWorldAsset(gridNo, level, tileIndex).displayName :
			(hasItems ? "GROUND ITEMS" : "WORLD ASSET");
		const BOOLEAN near = IsInspectedWorldAssetNear();
		AddContextEntry(ContextAction::INSPECT, "INSPECT / INFO");
		if (hasAsset)
			AddContextEntry(ContextAction::CATALOG, "GOD / CATALOG ASSET");
		if (hasItems || (structure && structure->fFlags & STRUCTURE_OPENABLE &&
			!(structure->fFlags & STRUCTURE_ANYDOOR)))
			AddContextEntry(ContextAction::CONTENTS, "OPEN CONTENTS", near);
		if (hasItems)
			AddContextEntry(ContextAction::PICK_UP,
				near ? "PICK UP" : "APPROACH & PICK UP");
		if (hasAsset && structure)
		{
			AddContextEntry(ContextAction::CARRY, "CARRY / REPOSITION",
				near && IsInspectedWorldAssetMovable());
			AssetCatalogRecord const* const catalog = FindAssetCatalogRecordConst(
				static_cast<INT16>(giCurrentTilesetID),
				CanonicalAssetTileIndex(gridNo, level, tileIndex));
			AddContextEntry(ContextAction::BUILD,
				catalog && catalog->buildable ?
					"BLUEPRINT / PLACEABLE" : "BUILD / SALVAGE REQUIREMENTS");
		}
		if (hasAsset)
		{
			SalvageProfile const salvage = DescribeWorldAsset(gridNo, level, tileIndex);
			if (salvage.salvageable)
			{
				const BOOLEAN tool = HasDiggingTool(selected);
				AddContextEntry(ContextAction::SALVAGE,
					!tool ? "DISMANTLE / NEED FIELD TOOL" :
					ST::format("DISMANTLE / +{} {}", salvage.amount,
						ResourceName(salvage.resource)),
					CanSalvageWorldAsset(selected, gridNo, level, tileIndex));
			}
		}
		if (level == 0 && gpWorldLevelData[gridNo].pLandHead && !structure)
		{
			const BOOLEAN tool = HasDiggingTool(selected);
			const BOOLEAN diggable = CanDigTerrainAt(selected, gridNo);
			AddContextEntry(ContextAction::DIG,
				!tool ? "DIG / NEED FIELD SHOVEL" :
				diggable ? "DIG / REMOVE SURFACE" : "DIG / GROUND EXPOSED",
				diggable);
		}
	}
	else if (hasTerrain)
	{
		gContextSoldier = nullptr;
		gContextGridNo = gridNo;
		gContextLevel = 0;
		gContextTileIndex = NO_TILE;
		gContextWorldItemIndex = -1;
		gInspectedSoldier = nullptr;
		gInspectedGridNo = gridNo;
		gInspectedLevel = 0;
		gInspectedTileIndex = NO_TILE;
		gContentsMode = ContentsMode::WORLD;
		gContextTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
		gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
		CaptureInspectorPreview(gridNo, 0);
		AddContextEntry(ContextAction::INSPECT, "INSPECT / GROUND");
		const BOOLEAN tool = HasDiggingTool(selected);
		const BOOLEAN diggable = CanDigTerrainAt(selected, gridNo);
		AddContextEntry(ContextAction::DIG,
			!tool ? "DIG / NEED FIELD SHOVEL" :
			diggable ? "DIG / REMOVE SURFACE" : "DIG / GROUND EXPOSED",
			diggable);
		RefreshPanelActions();
	}
	else if (selected)
	{
		gContextSoldier = selected;
		gContextGridNo = selected->sGridNo;
		gInspectedSoldier = selected;
		gInspectedGridNo = NOWHERE;
		gContextTitle = selected->name;
		AddContextEntry(ContextAction::INSPECT, "INSPECT / INFO");
		AddContextEntry(ContextAction::CONTENTS, "INVENTORY");
		AddContextEntry(ContextAction::STAND, "STANCE / STAND");
		AddContextEntry(ContextAction::CROUCH, "STANCE / CROUCH");
		AddContextEntry(ContextAction::PRONE, "STANCE / PRONE");
		AddContextEntry(ContextAction::STEALTH,
			selected->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON");
		const OBJECTTYPE& hand = selected->inv[HANDPOS];
		if (hand.usItem != NOTHING &&
			GCM->getItem(hand.usItem)->getItemClass() == IC_GUN)
		{
			AddContextEntry(ContextAction::WEAPON_MODE, "WEAPON MODE");
			AddContextEntry(ContextAction::RELOAD, "RELOAD WEAPON");
			AddContextEntry(ContextAction::UNLOAD,
				ST::format("UNLOAD MAGAZINE / {}", hand.ubGunShotsLeft),
				hand.ubGunShotsLeft > 0);
			AddContextEntry(ContextAction::SWAP_HANDS, "SWAP HANDS");
		}
	}
	if (gContextEntryCount == 0) return;

	const INT16 width = 168;
	const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
	gContextX = std::clamp<INT16>(screenX, 0,
		std::max<INT16>(0, gsVIEWPORT_END_X - width));
	gContextY = std::clamp<INT16>(screenY, 0,
		std::max<INT16>(0, gsVIEWPORT_END_Y - height));
	gContextVisible = TRUE;
	PositionContextRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	std::array<UINT8, 6> available{};
	size_t count = 0;
	available[count++] = 0; // Movement is always a valid escape mode.

	SOLDIERTYPE* const selected = GetSelectedMan();
	const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
	const BOOLEAN hasItems = validGrid && GetItemPool(gridNo, level) != nullptr;
	const BOOLEAN hasInteractive = validGrid && tileIndex < NUMBEROFTILES;
	const BOOLEAN movable = IsWorldAssetMovableAt(gridNo, level, tileIndex,
		selected);

	if (hasItems || hasInteractive || (target && target->bTeam == OUR_TEAM))
		available[count++] = 1; // Hand / use / loot.
	if (movable) available[count++] = 2; // Carry environment object.
	if (validGrid || target) available[count++] = 3; // Look.
	if (target && target->bTeam != OUR_TEAM) available[count++] = 4; // Talk.
	if (target && target->bTeam != OUR_TEAM && selected &&
		selected->inv[HANDPOS].usItem != NOTHING &&
		GCM->getItem(selected->inv[HANDPOS].usItem)->isWeapon())
	{
		available[count++] = 5; // Armed action.
	}

	size_t current = count;
	for (size_t i = 0; i < count; ++i)
	{
		if (available[i] == gCursorAction)
		{
			current = i;
			break;
		}
	}
	gCursorAction = available[current == count ? (count > 1 ? 1 : 0) :
		(current + 1) % count];
	switch (gCursorAction)
	{
		case 0: guiPendingOverrideEvent = A_CHANGE_TO_MOVE;       break;
		case 1: guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;   break;
		case 2:
			// Environment carry mode deliberately reuses JA2's grab hand.
			// OS0HandleCursorAction owns the click while this state is active.
			guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
			break;
		case 3: guiPendingOverrideEvent = LC_CHANGE_TO_LOOK;      break;
		case 4: guiPendingOverrideEvent = T_CHANGE_TO_TALKING;    break;
		case 5: guiPendingOverrideEvent = M_CHANGE_TO_ACTION;     break;
	}
	if (gCursorAction != 2) ClearWorldMoveState();
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0CancelCursorAction()
{
	gCursorAction = 0;
	ClearWorldMoveState();
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
	SetRenderFlags(RENDER_FLAG_FULL);
}

BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex)
{
	// A panel opened by a double-click can receive a trailing button-up from the
	// same physical gesture. Consume it before vanilla UI code sees it.
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return TRUE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	if (gCursorAction == 1)
	{
		if (target)
		{
			OS0OpenCharacterPanel(target);
			return TRUE;
		}
		if (gridNo >= 0 && gridNo < WORLD_MAX &&
			(GetItemPool(gridNo, level) || tileIndex < NUMBEROFTILES))
		{
			OS0ActivateWorldObject(gridNo, level, tileIndex);
			return TRUE;
		}
		return FALSE;
	}
	if (gCursorAction == 3)
	{
		const BOOLEAN hasInspectable = target ||
			(gridNo >= 0 && gridNo < WORLD_MAX &&
				(GetItemPool(gridNo, level) || tileIndex < NUMBEROFTILES));
		OS0SelectWorldObject(target, gridNo, level, tileIndex);
		return hasInspectable;
	}
	if (gCursorAction != 2) return FALSE;
	if (gridNo < 0 || gridNo >= WORLD_MAX || tileIndex >= NUMBEROFTILES)
		return TRUE;

	SOLDIERTYPE* const selected = GetSelectedMan();
	if (!selected || PythSpacesAway(selected->sGridNo, gridNo) > 2)
		return TRUE;

	if (!IsWorldAssetMovableAt(gridNo, level, tileIndex, selected))
	{
		return TRUE;
	}

	ClearWorldMoveState();
	gWorldMovePending = TRUE;
	gWorldMoveSource = gridNo;
	gWorldMoveTileIndex = tileIndex;
	gWorldMoveCarrier = selected;
	ShadeWorldMoveSource();
	gLootVisible = FALSE;
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}


void OS0OpenWorldContainer(GridNo gridNo, UINT8 level, UINT16 tileIndex)
{
	if (gridNo < 0 || gridNo >= WORLD_MAX) return;
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	CloseContextMenu();
	gContextTitle = tileIndex < NUMBEROFTILES ?
		DescribeWorldAsset(gridNo, level, tileIndex).displayName : "GROUND ITEMS";
	EnsureContainerLoot(gridNo, level, tileIndex);
	gLootGridNo = gridNo;
	gLootLevel = level;
	gLootTileIndex = tileIndex;
	gInspectedSoldier = nullptr;
	gInspectedGridNo = gridNo;
	gInspectedLevel = level;
	gInspectedTileIndex = tileIndex;
	gMode = ComputerMode::CONTENTS;
	gContentsMode = ContentsMode::WORLD;
	const BOOLEAN hasContents = GetItemPool(gridNo, level) != nullptr ||
		WorldStructureAt(gridNo, level, tileIndex) != nullptr;
	gLootVisible = hasContents && IsInspectedWorldAssetNear();
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::CONTEXT)].visible = TRUE;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::ACTIONS)].visible = TRUE;
	gFloatingPanels[static_cast<size_t>(FloatingPanelId::OBJECT_INVENTORY)].visible = TRUE;
	gInventoryVisible = TRUE;
	gPanelInteractionGuardUntil = GetJA2Clock() + 140;
	if (gLootVisible) gLootIgnoreInputUntil = GetJA2Clock() + 300;

	CaptureInspectorPreview(gridNo, level);
	RefreshPanelActions();
	PositionBagRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0ActivateWorldObject(GridNo gridNo, UINT8 level, UINT16 tileIndex)
{
	if (gridNo < 0 || gridNo >= WORLD_MAX) return;
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
	ITEM_POOL* const pool = GetItemPool(gridNo, level);

	// A loose world item has a simple default: approach and pick it up. A
	// container keeps its spatial contents window because it may hold many
	// independently positioned objects.
	if (pool && (!structure || !(structure->fFlags & STRUCTURE_OPENABLE)))
	{
		for (ITEM_POOL* item = pool; item; item = item->pNext)
		{
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING ||
				worldItem.bVisible == HIDDEN_ITEM) continue;
			if (SOLDIERTYPE* const selected = GetSelectedMan())
				SoldierPickupItem(selected, item->iItemIndex, gridNo,
					ITEM_IGNORE_Z_LEVEL);
			return;
		}
	}
	if (pool || (structure && structure->fFlags & STRUCTURE_OPENABLE &&
		!(structure->fFlags & STRUCTURE_ANYDOOR)))
	{
		OS0OpenWorldContainer(gridNo, level, tileIndex);
	}
	else
	{
		// Decorative/resource assets have no inventory, but double-click still
		// gives them a first-class inspector instead of an empty fake container.
		OS0SelectWorldObject(nullptr, gridNo, level, tileIndex);
		gBagVisible = TRUE;
		gMode = ComputerMode::INFO;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}
}


BOOLEAN OS0HandlePendingWorldMove(GridNo destination)
{
	if (!gWorldMovePending) return FALSE;
	if (gWorldMoveSource < 0 || gWorldMoveSource >= WORLD_MAX ||
		destination < 0 || destination >= WORLD_MAX ||
		destination == gWorldMoveSource ||
		gWorldMoveTileIndex >= NUMBEROFTILES)
	{
		return TRUE;
	}
	SOLDIERTYPE* const selected = gWorldMoveCarrier;
	if (!selected || !selected->bActive || selected->bLife <= 0)
	{
		ClearWorldMoveState();
		return TRUE;
	}

	STRUCTURE* const structure = WorldStructureAt(gWorldMoveSource, 0,
		gWorldMoveTileIndex);
	constexpr StructureFlags fixed = static_cast<StructureFlags>(
		STRUCTURE_WALLSTUFF | STRUCTURE_ROOF | STRUCTURE_PERSON |
		STRUCTURE_CORPSE | STRUCTURE_TREE | STRUCTURE_ANYFENCE |
		STRUCTURE_SWITCH | STRUCTURE_VEHICLE | STRUCTURE_LIGHTSOURCE);
	if (!structure || !(structure->fFlags & STRUCTURE_BASE_TILE) ||
		structure->fFlags & fixed ||
		!structure->pDBStructureRef ||
		structure->pDBStructureRef->pDBStructure->ubNumberOfTiles != 1)
	{
		ClearWorldMoveState();
		return TRUE;
	}

	// Validate before starting the walk. Invalid tiles leave the crate attached
	// to the cursor, so the player can simply choose another destination.
	if (!OkayToAddStructureToWorld(destination, 0,
		structure->pDBStructureRef, INVALID_STRUCTURE_ID)) return TRUE;
	const GridNo actionGrid = FindCarryActionGrid(selected, destination);
	if (actionGrid == NOWHERE) return TRUE;

	gWorldMoveDestination = destination;
	gWorldMoveActionGrid = actionGrid;
	gWorldMovePending = FALSE;
	gWorldMoveWalking = TRUE;
	gCursorAction = 0;
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;

	if (selected->sGridNo != actionGrid &&
		!EVENT_InternalGetNewSoldierPath(selected, actionGrid,
			selected->usUIMovementMode, TRUE, TRUE))
	{
		gWorldMoveDestination = NOWHERE;
		gWorldMoveActionGrid = NOWHERE;
		gWorldMovePending = TRUE;
		gWorldMoveWalking = FALSE;
		gCursorAction = 2;
		guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
	}
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}


void OS0AdjustWorldZoom(INT8 direction)
{
	const UINT8 next = direction > 0 ? 2 : 1;
	if (next == gWorldZoom) return;
	CloseContextMenu();
	gWorldZoom = next;
	ClearWorldMoveState();
	SetRenderFlags(RENDER_FLAG_FULL);
	InvalidateScreen();
}


void OS0PrepareWorldZoom()
{
	if (gWorldZoom <= 1) return;

	// The previous frame contains the enlarged world and the OS//0 overlay.
	// Restore the last clean, unzoomed tactical frame before scrolling and
	// rendering. Otherwise the overlay is sampled into the next zoom frame and
	// recursively leaves the red menu trails seen while moving the camera.
	if (gWorldZoomBuffer &&
		gWorldZoomBuffer->Width() == SCREEN_WIDTH &&
		gWorldZoomBuffer->Height() == SCREEN_HEIGHT)
	{
		const SGPBox full{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
		BltVideoSurface(FRAME_BUFFER, gWorldZoomBuffer, 0, 0, &full);
	}
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0ApplyWorldZoom()
{
	if (gWorldZoom <= 1) return;
	if (!gWorldZoomBuffer ||
		gWorldZoomBuffer->Width() != SCREEN_WIDTH ||
		gWorldZoomBuffer->Height() != SCREEN_HEIGHT)
	{
		if (gWorldZoomBuffer) DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = AddVideoSurface(SCREEN_WIDTH, SCREEN_HEIGHT, PIXEL_DEPTH);
	}
	const SGPBox full{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	BltVideoSurface(gWorldZoomBuffer, FRAME_BUFFER, 0, 0, &full);
	SGPBox source;
	SGPBox destination;
	GetWorldZoomRects(source, destination);
	BltStretchVideoSurface(FRAME_BUFFER, gWorldZoomBuffer, &source, &destination);
	InvalidateRegion(destination.x, destination.y,
		destination.x + destination.w, destination.y + destination.h);
}


void OS0MapDisplayToWorldScreen(INT16* x, INT16* y)
{
	if (!x || !y || gWorldZoom <= 1) return;
	SGPBox source;
	SGPBox destination;
	GetWorldZoomRects(source, destination);
	if (*x < destination.x || *x >= destination.x + destination.w ||
		*y < destination.y || *y >= destination.y + destination.h) return;
	*x = source.x + (*x - destination.x) / gWorldZoom;
	*y = source.y + (*y - destination.y) / gWorldZoom;
}


void OS0MapDisplayToWorldScreen(UINT16* x, UINT16* y)
{
	if (!x || !y) return;
	INT16 mappedX = static_cast<INT16>(*x);
	INT16 mappedY = static_cast<INT16>(*y);
	OS0MapDisplayToWorldScreen(&mappedX, &mappedY);
	*x = static_cast<UINT16>(std::max<INT16>(0, mappedX));
	*y = static_cast<UINT16>(std::max<INT16>(0, mappedY));
}


void OS0MapWorldToDisplayScreen(INT16* x, INT16* y)
{
	if (!x || !y || gWorldZoom <= 1) return;
	SGPBox source;
	SGPBox destination;
	GetWorldZoomRects(source, destination);
	*x = destination.x + (*x - source.x) * gWorldZoom;
	*y = destination.y + (*y - source.y) * gWorldZoom;
}


void OS0PlaceTalkingPanel(INT16 panelWidth, INT16 panelHeight, INT16* x, INT16* y)
{
	if (!x || !y) return;

	if (!gTalkDocked)
	{
		gBagVisibleBeforeTalk = gBagVisible;
		gTalkDocked = TRUE;
	}

	// Conversation replaces TEAM / OPERATOR / GEAR inside the one movable
	// Field Computer footprint. Centre smaller original talk art in that space.
	*x = std::clamp<INT16>(
		gBagX + (PANE_W - panelWidth) / 2,
		0,
		std::max<INT16>(0, gsVIEWPORT_END_X - panelWidth));
	*y = std::clamp<INT16>(
		gBagY,
		0,
		std::max<INT16>(0, gsVIEWPORT_END_Y - panelHeight));

	gBagVisible = FALSE;
	SetBagRegionsEnabled(TRUE);
	gOrbRegion.Disable();
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0TalkingPanelClosed()
{
	if (!gTalkDocked) return;
	gTalkDocked = FALSE;
	gBagVisible = gBagVisibleBeforeTalk;
	SetBagRegionsEnabled(TRUE);
	gOrbRegion.Enable();
	SetRenderFlags(RENDER_FLAG_FULL);
}
