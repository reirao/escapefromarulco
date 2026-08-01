#pragma once

#include "JA2Types.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_theory/string>
#include <variant>
#include <vector>

// The realtime editor deliberately exposes stable value objects only.  UI code
// must never retain LEVELNODE, STRUCTURE, WORLDITEM or SOLDIERTYPE pointers
// across update(), because a queued world swap invalidates them.

enum class OS0EditorCategory : UINT8
{
	TERRAIN,
	WATER,
	CLIFFS,
	NATURE,
	PROPS,
	BUILDINGS,
	WALLS,
	DOORS,
	DECORATIONS,
	FLOORS,
	ROOFS,
	ROOF_OBJECTS,
	ROADS,
	DEBRIS,
	FENCES,
	VEHICLES,
	SHADOWS,
	WEAPONS,
	AMMUNITION,
	ARMOUR,
	TOOLS,
	CONSUMABLES,
	ITEMS,
	NPCS,
	SYSTEM,
	COUNT
};

enum class OS0EditorLayer : UINT8
{
	AUTO,
	LAND,
	OBJECT,
	STRUCTURE,
	SHADOW,
	ROOF,
	ON_ROOF,
	TOPMOST
};

enum class OS0EditorAction : UINT8
{
	NEW_BLANK_MAP,
	LOAD_MAP,
	PLACE_TILE,
	PLACE_ITEM,
	PLACE_NPC,
	REMOVE,
	SAVE_MAP,
	REBUILD_CATALOGS,
	COUNT
};

enum class OS0EditorCommandType : UINT8
{
	NEW_BLANK_MAP,
	LOAD_MAP,
	PLACE_TILE,
	PLACE_ITEM,
	PLACE_NPC,
	APPLY_RECIPE,
	REMOVE,
	SAVE_MAP,
	REBUILD_CATALOGS
};

enum class OS0EditorRemoveKind : UINT8
{
	TILE,
	WORLD_ITEM,
	NPC
};

enum class OS0EditorItemVisibility : INT8
{
	VISIBLE = 1,
	HIDDEN = -1
};

struct OS0EditorCategoryRecord
{
	OS0EditorCategory id = OS0EditorCategory::SYSTEM;
	ST::string label;
	UINT16 order = 0;
};

struct OS0EditorTileRecord
{
	INT16 tileset = -1;
	UINT16 tileIndex = 0;
	UINT16 tileType = 0;
	UINT16 regionIndex = 0;
	OS0EditorCategory category = OS0EditorCategory::PROPS;
	OS0EditorLayer layer = OS0EditorLayer::STRUCTURE;
	ST::string label;
	UINT32 flags = 0;
	UINT8 wallOrientation = 0;
	UINT8 footprintWidth = 1;
	UINT8 footprintHeight = 1;
	BOOLEAN placeable = FALSE;
	BOOLEAN animated = FALSE;
	BOOLEAN multiTile = FALSE;
	BOOLEAN hasShadowBuddy = FALSE;
};

struct OS0EditorItemRecord
{
	UINT16 itemIndex = 0;
	UINT32 itemClass = 0;
	OS0EditorCategory category = OS0EditorCategory::ITEMS;
	ST::string internalName;
	ST::string label;
	UINT8 weight = 0;
	UINT8 perPocket = 0;
	UINT16 flags = 0;
};

struct OS0EditorNpcTemplate
{
	UINT16 id = 0;
	// 0xffff denotes a generated actor archetype. Other values are real JA2
	// profile ids and produce a detailed, saveable map placement.
	UINT16 profileId = 0xffff;
	ST::string label;
	INT8 team = 0;
	UINT8 soldierClass = 0;
	INT8 bodyType = -1;
	INT8 orders = 0;
	INT8 attitude = 0;
	UINT8 civilianGroup = 0;
};

struct OS0EditorActionRecord
{
	OS0EditorAction id = OS0EditorAction::NEW_BLANK_MAP;
	OS0EditorCategory category = OS0EditorCategory::SYSTEM;
	ST::string label;
	ST::string explanation;
	BOOLEAN requiresWorld = FALSE;
	BOOLEAN invalidatesWorldPointers = FALSE;
};

struct OS0EditorCatalog
{
	INT16 tileset = -1;
	std::uint64_t generation = 0;
	std::vector<OS0EditorCategoryRecord> categories;
	std::vector<OS0EditorTileRecord> tiles;
	std::vector<OS0EditorItemRecord> items;
	std::vector<OS0EditorNpcTemplate> npcTemplates;
	std::vector<OS0EditorActionRecord> actions;
};

struct OS0EditorBlankMapRequest
{
	// -1 keeps the active tileset, or uses the engine default if none is loaded.
	INT16 tileset = -1;
	// Region index within FIRSTTEXTURE, zero based.
	UINT16 groundRegion = 0;
	// NOWHERE selects the geometric map centre.
	INT16 centerGridNo = -1;
};

struct OS0EditorLoadRequest
{
	// A file stem or filename, never an absolute path. It is sanitized and
	// loaded exclusively from userPrivateFiles()/OS0/maps.
	ST::string name = "live-editor";
};

struct OS0EditorTilePlacement
{
	INT16 gridNo = -1;
	UINT16 tileIndex = 0;
	// -1 accepts the active tileset.  Catalog-driven UI should copy the
	// record's tileset here so a queued tileset swap cannot reinterpret it.
	INT16 tileset = -1;
	OS0EditorLayer layer = OS0EditorLayer::AUTO;
	BOOLEAN replaceExisting = FALSE;
	BOOLEAN validateCollision = TRUE;
};

struct OS0EditorItemPlacement
{
	INT16 gridNo = -1;
	UINT16 itemIndex = 0;
	UINT8 quantity = 1;
	INT8 condition = 100;
	UINT8 level = 0;
	OS0EditorItemVisibility visibility = OS0EditorItemVisibility::VISIBLE;
};

struct OS0EditorNpcPlacement
{
	INT16 gridNo = -1;
	UINT16 templateId = 0;
	INT8 direction = 0;
	BOOLEAN onRoof = FALSE;
	INT8 relativeAttributeLevel = 2;
	INT8 relativeEquipmentLevel = 2;
};

// Typed authoring recipes sit above individual tile placement.  They reuse
// the original JA2 editor algorithms, but are immutable values which can be
// queued safely and resolved against the live world at the frame boundary.
struct OS0EditorTerrainPaintRecipe
{
	INT16 gridNo = -1;
	// A tile type (FIRSTTEXTURE..DEEPWATERTEXTURE), not a tile index.
	UINT16 textureType = 0xffff;
	UINT8 radius = 0;
};

struct OS0EditorTerrainSmoothRecipe
{
	INT16 gridNo = -1;
	// 0xffff smooths every normal terrain texture plus shoreline water.
	UINT16 textureType = 0xffff;
	UINT8 radius = 1;
	BOOLEAN force = TRUE;
};

struct OS0EditorRoadRecipe
{
	INT16 gridNo = -1;
	// Native Road_Smoothing macro id (L1..TE, 0..31).
	UINT8 macroId = 0;
};

using OS0EditorWorldRecipe = std::variant<
	OS0EditorTerrainPaintRecipe,
	OS0EditorTerrainSmoothRecipe,
	OS0EditorRoadRecipe>;

struct OS0EditorRecipeRequest
{
	OS0EditorWorldRecipe recipe;
};

struct OS0EditorRemoveRequest
{
	OS0EditorRemoveKind kind = OS0EditorRemoveKind::TILE;
	INT16 gridNo = -1;
	OS0EditorLayer layer = OS0EditorLayer::AUTO;
	UINT16 tileIndex = 0;
	// Every destructive request is bound to the exact world generation that
	// was visible when the player clicked.  This prevents a queued erase from
	// deleting a replacement which happens to reuse the same numeric handle.
	UINT32 expectedWorldRevision = 0;
	// Optional active-tileset guard for TILE handles. Catalog-driven callers
	// should provide it to reject a stale numeric index after a world swap.
	INT16 tileset = -1;
	// Used only for WORLD_ITEM.  A stable numeric handle is revalidated when
	// the command executes; callers must not keep a WORLDITEM pointer.
	INT32 worldItemIndex = -1;
	UINT16 expectedItemIndex = 0xffff;
	INT16 expectedItemGridNo = -1;
	// Used only for NPC.
	UINT16 soldierId = 0xffff;
	UINT32 expectedNpcInstanceId = 0;
	INT8 expectedNpcTeam = -1;
	INT16 expectedNpcGridNo = -1;
};

struct OS0EditorSaveRequest
{
	// A file stem or filename, never an absolute path.  It is sanitized and
	// written below userPrivateFiles()/OS0/maps.
	ST::string name = "untitled";
};

struct OS0EditorCatalogRebuildRequest
{
};

using OS0EditorCommandPayload = std::variant<
	OS0EditorBlankMapRequest,
	OS0EditorLoadRequest,
	OS0EditorTilePlacement,
	OS0EditorItemPlacement,
	OS0EditorNpcPlacement,
	OS0EditorRecipeRequest,
	OS0EditorRemoveRequest,
	OS0EditorSaveRequest,
	OS0EditorCatalogRebuildRequest>;

struct OS0EditorCommand
{
	std::uint64_t id = 0;
	OS0EditorCommandType type = OS0EditorCommandType::NEW_BLANK_MAP;
	OS0EditorCommandPayload payload;
};

struct OS0EditorCommandResult
{
	std::uint64_t id = 0;
	OS0EditorCommandType type = OS0EditorCommandType::NEW_BLANK_MAP;
	BOOLEAN success = FALSE;
	BOOLEAN worldChanged = FALSE;
	ST::string message;
};

struct OS0EditorSessionStatus
{
	BOOLEAN processing = FALSE;
	BOOLEAN lastUpdateSucceeded = TRUE;
	std::uint64_t catalogGeneration = 0;
	ST::string lastMessage;
};

// All direct JA2 calls live behind this adapter.  It batches render/pathing
// finalization so a brush stroke does not rebuild the whole scene per tile.
class OS0RealtimeEditorEngineAdapter
{
public:
	OS0EditorCatalog rebuildCatalogs(std::uint64_t generation) const;
	OS0EditorCommandResult execute(OS0EditorCommand const& command);
	void finalizeFrame();
	BOOLEAN consumeCatalogInvalidation() noexcept;

private:
	std::vector<INT16> dirtyGridNos_;
	BOOLEAN fullWireframeRebuild_ = FALSE;
	BOOLEAN fullMovementRebuild_ = FALSE;
	BOOLEAN renderDirty_ = FALSE;
	BOOLEAN catalogInvalidated_ = FALSE;
};

class OS0RealtimeEditorSession
{
public:
	std::uint64_t queueNewBlankMap(OS0EditorBlankMapRequest request = {});
	std::uint64_t queueLoad(OS0EditorLoadRequest request = {});
	std::uint64_t queuePlaceTile(OS0EditorTilePlacement request);
	std::uint64_t queuePlaceItem(OS0EditorItemPlacement request);
	std::uint64_t queuePlaceNpc(OS0EditorNpcPlacement request);
	std::uint64_t queuePaintTerrain(OS0EditorTerrainPaintRecipe request);
	std::uint64_t queueSmoothTerrain(OS0EditorTerrainSmoothRecipe request);
	std::uint64_t queuePlaceRoad(OS0EditorRoadRecipe request);
	std::uint64_t queueRemove(OS0EditorRemoveRequest request);
	std::uint64_t queueSave(OS0EditorSaveRequest request);
	std::uint64_t queueRebuildCatalogs();

	// Call once from a safe Tactical frame boundary, after UI code has cleared
	// pointer-bearing hover/context/loot/carry/item-pointer state when
	// willInvalidateWorldPointers() is true.
	void update();
	// Catalog entries and queued handles are valid only for one loaded tactical
	// world. Clear them at both sides of a sector lifetime while keeping command
	// IDs monotonic, so a late UI result can never alias a command from a new map.
	void resetForTacticalSession();

	void rebuildCatalogs();
	OS0EditorCatalog const& catalog() const noexcept { return catalog_; }
	OS0EditorSessionStatus const& status() const noexcept { return status_; }
	std::vector<OS0EditorCommandResult> drainResults();

	BOOLEAN busy() const noexcept { return status_.processing; }
	std::size_t pendingCount() const noexcept { return pending_.size(); }
	BOOLEAN hasPendingWorldSwap() const noexcept;
	BOOLEAN willInvalidateWorldPointers() const noexcept;

private:
	template<typename Request>
	std::uint64_t enqueue(OS0EditorCommandType type, Request&& request);

	OS0RealtimeEditorEngineAdapter engine_;
	OS0EditorCatalog catalog_;
	OS0EditorSessionStatus status_;
	std::deque<OS0EditorCommand> pending_;
	std::vector<OS0EditorCommandResult> completed_;
	std::uint64_t nextCommandId_ = 1;
	std::uint64_t nextCatalogGeneration_ = 1;
};

OS0RealtimeEditorSession& OS0GetRealtimeEditor();

// Pure geometry guards used by the adapter and unit tests.  They never read or
// mutate JA2 world state.
std::vector<INT16> OS0ResolveEditorBrushGridNos(INT16 centerGridNo,
	UINT8 radius);
BOOLEAN OS0EditorRoadMacroFitsWorld(INT16 anchorGridNo, UINT8 macroId);
