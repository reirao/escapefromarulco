/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-30.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "OS0_IngameUI.h"
#include "OS0_ActionRegistry.h"
#include "OS0_AssetCatalogService.h"
#include "OS0_AssetDamageSystem.h"
#include "OS0_CreatorModel.h"
#include "OS0_DirectControl.h"
#include "OS0_FieldTutorial.h"
#include "OS0_ItemRelations.h"
#include "OS0_ItemTransferController.h"
#include "OS0_ItemTransferRuntime.h"
#include "OS0_MouseRegionZOrder.h"
#include "OS0_PointerSnapshot.h"
#include "OS0_RealtimeEditor.h"
#include "OS0_RealtimeEditorUI.h"
#include "OS0_TacticalSession.h"
#include "OS0_UIAssetManager.h"
#include "OS0_UIRuntime.h"
#include "OS0_ViewportInput.h"
#include "OS0_WorldInteractionSystem.h"

#include "Animation_Control.h"
#include "AI.h"
#include "ArmourModel.h"
#include "Auto_Bandage.h"
#include "Campaign.h"
#include "Campaign_Types.h"
#include "Cursors.h"
#include "Cursor_Control.h"
#include "Directories.h"
#include "Dialogue_Control.h"
#include "Faces.h"
#include "FileMan.h"
#include "Font.h"
#include "Font_Control.h"
#include "GameInstance.h"
#include "Game_Clock.h"
#include "GameScreen.h"
#include "GameVersion.h"
#include "HImage.h"
#include "Handle_Items.h"
#include "Handle_UI.h"
#include "Input.h"
#include "ItemModel.h"
#include "Interface.h"
#include "Interface_Control.h"
#include "Interface_Dialogue.h"
#include "Interface_Items.h"
#include "Interface_Panels.h"
#include "Interactive_Tiles.h"
#include "Isometric_Utils.h"
#include "Items.h"
#include "Lighting.h"
#include "Local.h"
#include "Logger.h"
#include "Message.h"
#include "MouseSystem.h"
#include "Overhead.h"
#include "PathAI.h"
#include "Physics.h"
#include "Points.h"
#include "Render_Dirty.h"
#include "RenderWorld.h"
#include "Radar_Screen.h"
#include "ScreenIDs.h"
#include "SaveLoadMap.h"
#include "SGPFile.h"
#include "SkillCheck.h"
#include "Soldier_Profile.h"
#include "Soldier_Find.h"
#include "Soldier_Control.h"
#include "Soldier_Functions.h"
#include "Soldier_Macros.h"
#include "StrategicMap.h"
#include "Structure.h"
#include "SysUtil.h"
#include "Squads.h"
#include "Spread_Burst.h"
#include "Timer_Control.h"
#include "TileDef.h"
#include "Turn_Based_Input.h"
#include "UILayout.h"
#include "UI_Cursors.h"
#include "VObject.h"
#include "VObject_Blitters.h"
#include "VSurface.h"
#include "Video.h"
#include "Weapons.h"
#include "World_Items.h"
#include "WorldMan.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string_theory/format>
#include <utility>
#include <vector>


static BOOLEAN PreserveHeldItemBeforeWorldTeardown();


namespace
{
	constexpr INT16 PANE_W = 465;
	constexpr INT16 BAG_H = 184;
	constexpr INT16 GRABBER_H = 16;
	constexpr INT16 TAB_Y = BAG_H - 22;
	constexpr INT16 INVENTORY_X = 145;
	constexpr INT16 CONTINUE_X = PANE_W - 125;
	constexpr INT16 PANEL_HEADER_H = 19;
	constexpr INT16 SECTOR_PANEL_W = 300;
	constexpr INT16 SECTOR_PANEL_H = 226;
	constexpr INT16 STRATEGIC_CELL = 9;
	constexpr INT16 GOD_LIBRARY_W = 420;
	constexpr INT16 GOD_LIBRARY_H = 250;
	constexpr INT16 ASSET_CATALOG_W = 318;
	constexpr INT16 ASSET_CATALOG_H = 194;
	constexpr INT16 INSPECTOR_W = 310;
	constexpr INT16 INSPECTOR_H = 96;
	constexpr INT16 TOOLBOX_W = 132;
	constexpr INT16 TOOLBOX_H = 132;
	constexpr INT16 ENVIRONMENT_W = 246;
	constexpr INT16 ENVIRONMENT_H = 104;
	constexpr size_t ENVIRONMENT_SKILL_COUNT = 6;
	constexpr size_t NEARBY_HINT_COUNT = 6;
	// One physical OS//0 field computer expands into five semantic workspaces.
	// The old eight-button strip still exists in the command registry, but is no
	// longer a second, competing navigation surface.
	constexpr size_t PANEL_DOCK_COUNT = 5;
	constexpr size_t COMMAND_MODULE_COUNT =
		static_cast<size_t>(OS0UICommand::COUNT);
	constexpr size_t GOD_ICON_COUNT = static_cast<size_t>(OS0UIIcon::COUNT);
	constexpr INT16 COMMAND_BAR_H = OS0UILayout::DOCK_HEIGHT;
	constexpr INT16 BRAND_W = 184;
	constexpr INT16 BRAND_H = 40;
	constexpr INT16 EVENT_LOG_W = 218;
	constexpr INT16 EVENT_LOG_H = 58;
	constexpr INT16 COLLAPSED_OS0_W = 54;
	constexpr const char* UI_LAYOUT_PATH = "OS0/os0-ui-layout.tsv";

	using ResourceKind = OS0ResourceKind;

	enum class SectorPanelMode : UINT8
	{
		BASE,
		MAP,
		TEAM,
		REPORT
	};

	enum class DebugLibraryMode : UINT8
	{
		ASSETS,
		ICONS
	};

	enum class AssetLibraryFilter : UINT8
	{
		ALL,
		UNCATALOGUED,
		DEBRIS,
		COUNT
	};

	enum class FieldToolKind : UINT8
	{
		NONE,
		FIELD_SHOVEL,
		CROWBAR,
		WIRE_CUTTERS,
		TOOLKIT,
		CUTTING_TOOL
	};

	using AssetCategory = OS0AssetCategory;
	using AssetMaterial = OS0AssetMaterial;
	using AssetRole = OS0AssetRole;
	using AssetCatalogRecord = OS0AssetCatalogRecord;

	enum class ContextEntryKind : UINT8
	{
		ACTION,
		CATEGORY,
		BACK
	};

	enum class DeferredContextKind : UINT8
	{
		NONE,
		CHARACTER,
		INVENTORY_ITEM,
		WORLD_ITEM
	};

	// A context menu can remain open while the engine advances animations, queued
	// actions and world-item storage.  Never let its delayed callback act through
	// the raw globals after their subject was removed or an index/slot was reused.
	struct DeferredContextIdentity
	{
		DeferredContextKind kind = DeferredContextKind::NONE;
		UINT8 actorId = NOBODY;
		UINT32 actorInstanceId = 0;
		INT8 inventorySlot = NO_SLOT;
		INT32 worldItemIndex = -1;
		GridNo gridNo = NOWHERE;
		UINT8 level = 0;
		UINT16 tileIndex = NO_TILE;
		std::uint64_t itemFingerprint = 0;
		UINT32 worldItemRevision = 0;
	};

	// Character actions are deliberately grouped at the presentation boundary.
	// ContextAction remains the authoritative gameplay command registry; this
	// enum only describes the five pages owned by a character relation.
	enum class CharacterHubCategory : UINT8
	{
		ACTIONS,
		ABILITIES,
		EQUIPMENT,
		GROUP,
		GOD,
		COUNT
	};

	struct ContextEntry
	{
		ContextAction action = ContextAction::COUNT;
		ST::string label;
		BOOLEAN enabled = FALSE;
		ContextEntryKind kind = ContextEntryKind::ACTION;
		CharacterHubCategory category = CharacterHubCategory::COUNT;
		OS0ActionBinding binding{};
		DeferredContextIdentity deferredIdentity{};
		OS0ActionApproach approach = OS0ActionApproach::IMMEDIATE;
		OS0ActionBlockReason blockReason = OS0ActionBlockReason::NONE;
	};

	struct CharacterActionSpec
	{
		ContextAction action = ContextAction::COUNT;
		CharacterHubCategory category = CharacterHubCategory::ACTIONS;
		ST::string label;
		BOOLEAN enabled = FALSE;
	};

	struct CharacterHubCategoryDescriptor
	{
		CharacterHubCategory category;
		const char* label;
		const char* explanation;
		OS0UIIcon icon;
		ActionCategory accent;
	};

	constexpr std::array<CharacterHubCategoryDescriptor,
		static_cast<size_t>(CharacterHubCategory::COUNT)> CHARACTER_HUB_CATEGORIES{{
		{ CharacterHubCategory::ACTIONS, "ACTIONS",
			"State, posture, stealth, cover and field behaviour.",
			OS0UIIcon::SNEAK, ActionCategory::STANCE },
		{ CharacterHubCategory::ABILITIES, "ABILITIES / TALENTS",
			"Open the real character sheet, attributes and passive talents.",
			OS0UIIcon::EXAMINE, ActionCategory::INFO },
		{ CharacterHubCategory::EQUIPMENT, "EQUIPMENT",
			"Open carried gear and operate the equipped weapon.",
			OS0UIIcon::HAND, ActionCategory::GEAR },
		{ CharacterHubCategory::GROUP, "GROUP",
			"Squad selection, team management and turn control.",
			OS0UIIcon::TALK, ActionCategory::GROUP },
		{ CharacterHubCategory::GOD, "GOD",
			"Debug assets, live editing, tools and safe recovery.",
			OS0UIIcon::KEYRING, ActionCategory::DEBUG }
	}};

	struct ImpactParticle
	{
		GridNo gridNo;
		INT8 velocityX;
		INT8 velocityY;
		UINT8 colourKind;
		UINT32 born;
	};

	using FloatingPanelId = OS0UIWindow;
	using FloatingPanel = OS0UIWindowState;
	using ToolboxModule = OS0UICommand;

	constexpr std::array<ToolboxModule, PANEL_DOCK_COUNT> MULTITOOL_MODULES{{
		ToolboxModule::OBJECT, ToolboxModule::WORLD, ToolboxModule::CHARACTER,
		ToolboxModule::STRATEGY, ToolboxModule::SANDBOX
	}};
	constexpr std::array<OS0UIIcon, PANEL_DOCK_COUNT> MULTITOOL_ICONS{{
		OS0UIIcon::HAND, OS0UIIcon::TOOLKIT, OS0UIIcon::EXAMINE,
		OS0UIIcon::LOOK, OS0UIIcon::KEYRING
	}};
	constexpr std::array<const char*, PANEL_DOCK_COUNT> MULTITOOL_LABELS{{
		"INTERACTION", "SALVAGE", "INSPECT", "INFO", "GOD"
	}};

	struct SalvageProfile
	{
		ST::string displayName;
		ResourceKind resource;
		UINT8 amount;
		BOOLEAN salvageable;
	};

	struct DebugAssetEntry
	{
		UINT16 tileIndex;
		GridNo gridNo;
		UINT8 level;
		UINT16 count;
		AssetCatalogRecord record;
		BOOLEAN catalogued;
	};

	struct NearbyInteractionHint
	{
		GridNo gridNo = NOWHERE;
		UINT8 level = 0;
		UINT16 tileIndex = NO_TILE;
		ContextAction action = ContextAction::INSPECT;
		BOOLEAN enabled = FALSE;
		OS0ActionBinding binding{};
	};

	using SectorUpgrade = OS0SectorUpgradeDefinition;

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

	// These are the body-mounted slots shown in the in-world exploded view.
	// Pockets stay in the backpack window, so every object continues to live in
	// JA2's real, saveable soldier inventory rather than a parallel UI model.
	constexpr std::array<INT8, 7> gExplodedEquipmentSlots{{
		HELMETPOS, HEAD1POS, HEAD2POS, VESTPOS, LEGPOS, HANDPOS, SECONDHANDPOS
	}};
	constexpr std::array<const char*, 7> gExplodedEquipmentLabels{{
		"HELMET", "FACE 1", "FACE 2", "VEST", "LEGS",
		"HAND 1 / PRIMARY", "HAND 2 / OFF HAND"
	}};

	OS0UIRuntime gUIRuntime;
	OS0UILayout gUILayout;
	BOOLEAN gInitialized = FALSE;
	// Visibility is established explicitly during initialization. A global TRUE
	// made selection/load side effects look like random inventory pop-ups.
	BOOLEAN& gBagVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::INVENTORY);
	BOOLEAN& gLootVisible = gUIRuntime.visibilityRef(OS0UIPanel::LOOT);
	BOOLEAN& gContextVisible = gUIRuntime.visibilityRef(OS0UIPanel::CONTEXT);
	BOOLEAN gObjectActionFanVisible = FALSE;
	BOOLEAN gCharacterActionFanVisible = FALSE;
	BOOLEAN gContextModalSuspended = FALSE;
	BOOLEAN& gEquipmentExplodedVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::EQUIPMENT);
	BOOLEAN gEquipmentAutoForHeldItem = FALSE;
	BOOLEAN& gStackSplitVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::STACK_SPLIT);
	SOLDIERTYPE* gEquipmentSoldier = nullptr;
	UINT32 gEquipmentSoldierInstanceId = 0;
	SOLDIERTYPE* gStackSplitSoldier = nullptr;
	INT8 gStackSplitSlot = NO_SLOT;
	DeferredContextIdentity gStackSplitIdentity{};
	UINT8 gStackSplitAmount = 1;
	BOOLEAN gHoverVisible = FALSE;
	BOOLEAN gInspectorPinned = FALSE;
	BOOLEAN gAimAutoCollapsed = FALSE;
	UINT32 gNextCombatProjectionAt = 0;
	OBJECTTYPE const* gLastItemCursorPointer = nullptr;
	UINT16 gLastItemCursorItem = NOTHING;
	UINT32 gNextItemCursorRefreshAt = 0;
	UINT32 gLootIgnoreInputUntil = 0;
	UINT32 gPanelInteractionGuardUntil = 0;
	BOOLEAN gBagVisibleBeforeTalk = TRUE;
	BOOLEAN gTalkDocked = FALSE;
	BOOLEAN gFieldToolIssued = FALSE;
	INT16& gBagX = gUIRuntime.panel(OS0UIPanel::INVENTORY).x;
	INT16& gBagY = gUIRuntime.panel(OS0UIPanel::INVENTORY).y;
	INT16& gInventoryX = gBagX;
	INT16& gInventoryY = gBagY;
	INT16 gOrbX = 0;
	INT16 gOrbY = 316;
	BOOLEAN gMultiToolExpanded = FALSE;
	UINT32 gMultiToolLastClickAt = 0;
	BOOLEAN gMultiToolDragCandidate = FALSE;
	BOOLEAN gMultiToolDragging = FALSE;
	BOOLEAN gEscapeKeyOwned = FALSE;
	INT16 gMultiToolDragStartX = 0;
	INT16 gMultiToolDragStartY = 0;
	INT16 gMultiToolDragOriginX = 0;
	INT16 gMultiToolDragOriginY = 0;
	INT16& gContextX = gUIRuntime.panel(OS0UIPanel::CONTEXT).x;
	INT16& gContextY = gUIRuntime.panel(OS0UIPanel::CONTEXT).y;
	INT16 gEquipmentCentreX = 0;
	INT16 gEquipmentCentreY = 0;
	INT16& gStackSplitX = gUIRuntime.panel(OS0UIPanel::STACK_SPLIT).x;
	INT16& gStackSplitY = gUIRuntime.panel(OS0UIPanel::STACK_SPLIT).y;
	SOLDIERTYPE* gInspectedSoldier = nullptr;
	UINT32 gInspectedSoldierInstanceId = 0;
	// The equipment window is an independent live view. World inspection and
	// container selection must never silently retarget or close it.
	SOLDIERTYPE* gInventorySoldier = nullptr;
	UINT32 gInventorySoldierInstanceId = 0;
	GridNo gInspectedGridNo = NOWHERE;
	UINT8 gInspectedLevel = 0;
	UINT16 gInspectedTileIndex = NO_TILE;
	SectorPanelMode gSectorPanelMode = SectorPanelMode::BASE;
	SGPSector gStrategicSelectedSector{ 9, 1, 0 };
	BOOLEAN& gTutorialActive = gUIRuntime.creatorActiveRef();
	OS0CreatorModel gCreatorModel;
	OS0FieldTutorial gFieldTutorial;
	GridNo gFieldTutorialGridNo = NOWHERE;
	UINT8 gFieldTutorialLevel = 0;
	UINT16 gFieldTutorialTileIndex = NO_TILE;
	size_t gFieldTutorialInitialLootCount = 0;
	UINT32 gFieldTutorialNextSearchAt = 0;
	UINT32 gFieldTutorialCompletedAt = 0;
	BOOLEAN gVideoScrollBeforeCreator = TRUE;
	UINT8 gWorldZoom = 1;
	BOOLEAN gVideoScrollBeforeZoom = TRUE;
	std::uint64_t gWorldProjectionStamp = 0;
	BOOLEAN gWorldProjectionStampValid = FALSE;
	SOLDIERTYPE* gHoverCursorSoldier = nullptr;
	UINT32 gHoverCursorSoldierInstanceId = 0;
	GridNo gHoverCursorGridNo = NOWHERE;
	UINT8 gHoverCursorLevel = 0;
	UINT16 gHoverCursorTileIndex = NO_TILE;
	INT32 gHoverCursorWorldItemIndex = -1;
	UINT16 gHoverCursorHeldItem = NOTHING;
	std::array<ImpactParticle, 48> gImpactParticles{};
	size_t gImpactParticleNext = 0;
	BOOLEAN& gGodLibraryVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::ASSET_LIBRARY);
	BOOLEAN& gAssetCatalogVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::ASSET_CATALOG);
	BOOLEAN gAssetCatalogReturnToLibrary = FALSE;
	BOOLEAN gAssetCatalogNameEditing = FALSE;
	UINT8 gGodMenuIcon = 0;
	DebugLibraryMode gDebugLibraryMode = DebugLibraryMode::ASSETS;
	AssetLibraryFilter gAssetLibraryFilter = AssetLibraryFilter::ALL;
	size_t gAssetLibraryPage = 0;
	INT16& gGodLibraryX =
		gUIRuntime.panel(OS0UIPanel::ASSET_LIBRARY).x;
	INT16& gGodLibraryY =
		gUIRuntime.panel(OS0UIPanel::ASSET_LIBRARY).y;
	INT16& gAssetCatalogX =
		gUIRuntime.panel(OS0UIPanel::ASSET_CATALOG).x;
	INT16& gAssetCatalogY =
		gUIRuntime.panel(OS0UIPanel::ASSET_CATALOG).y;
	AssetCatalogRecord gCatalogDraft{};
	std::vector<DebugAssetEntry> gDebugAssetLibrary;
	UINT8 gDebugAssetLibrarySector = 0xff;
	INT16 gDebugAssetLibraryTileset = -1;
	SGPVSurface* gWorldZoomBuffer = nullptr;
	SGPBox gWorldZoomBufferViewport{};
	BOOLEAN gWorldZoomBufferViewportValid = FALSE;
	class FloatingWindowView
	{
	public:
		explicit FloatingWindowView(OS0UIRuntime& runtime) : runtime_(runtime) {}
		constexpr size_t size() const noexcept
		{
			return static_cast<size_t>(FloatingPanelId::COUNT);
		}
		FloatingPanel& operator[](size_t index) noexcept
		{
			return runtime_.window(static_cast<FloatingPanelId>(index));
		}
		FloatingPanel const& operator[](size_t index) const noexcept
		{
			return runtime_.window(static_cast<FloatingPanelId>(index));
		}
		class iterator
		{
		public:
			iterator(FloatingWindowView& view, size_t index) : view_(view), index_(index) {}
			FloatingPanel& operator*() const { return view_[index_]; }
			iterator& operator++() { ++index_; return *this; }
			BOOLEAN operator!=(iterator const& other) const { return index_ != other.index_; }
		private:
			FloatingWindowView& view_;
			size_t index_;
		};
		iterator begin() { return iterator(*this, 0); }
		iterator end() { return iterator(*this, size()); }
	private:
		OS0UIRuntime& runtime_;
	};
	FloatingWindowView gFloatingPanels{ gUIRuntime };
	std::array<NearbyInteractionHint, NEARBY_HINT_COUNT> gNearbyHints{};
	std::array<ST::string, NEARBY_HINT_COUNT> gNearbyHintHelp{};
	size_t gNearbyHintCount = 0;
	GridNo gNearbyHintActorGridNo = NOWHERE;
	UINT8 gNearbyHintActorId = NOBODY;
	UINT32 gNearbyHintActorInstanceId = 0;
	UINT8 gNearbyHintActorLevel = 0xff;
	UINT32 gNearbyHintToolSignature = 0;
	GridNo gNearbyHintCursorGridNo = NOWHERE;
	UINT32 gNextNearbyHintScanAt = 0;
	BOOLEAN gNearbyScanWasEnabled = FALSE;
	GridNo gEnvironmentGridNo = NOWHERE;
	UINT8 gEnvironmentLevel = 0;
	UINT16 gEnvironmentTileIndex = NO_TILE;
	GridNo gEnvironmentActorGridNo = NOWHERE;
	UINT32 gNextEnvironmentRefreshAt = 0;
	ST::string gEnvironmentTitle = "NO OBJECT SELECTED";
	std::array<ContextEntry, ENVIRONMENT_SKILL_COUNT> gEnvironmentEntries{};
	size_t gEnvironmentEntryCount = 0;
	ContextAction gHoverSuggestedAction = ContextAction::COUNT;
	OS0ActionBinding gHoverActionBinding{};
	size_t gHoverActionCycleIndex = 0;
	BOOLEAN gHoverActionExplicit = FALSE;

	struct PendingWorldAction
	{
		ContextAction action = ContextAction::COUNT;
		OS0ActionBinding binding{};
		UINT8 actorId = NOBODY;
		UINT32 actorInstanceId = 0;
		GridNo destination = NOWHERE;
		UINT32 startedAt = 0;

		BOOLEAN active() const noexcept
		{
			return action != ContextAction::COUNT && actorId != NOBODY &&
				actorInstanceId != 0;
		}
		void reset() noexcept { *this = {}; }
	};

	PendingWorldAction gPendingWorldAction{};

	OS0CarryState& CarryState()
	{
		return OS0GetTacticalSession().state().carry;
	}

	OS0CursorState& CursorState()
	{
		return OS0GetTacticalSession().state().cursor;
	}

	BOOLEAN CombatModeActive()
	{
		return CursorState().action == ContextAction::ATTACK;
	}

	OS0InteractionMode& InteractionMode()
	{
		return gUIRuntime.interactionMode();
	}

	OS0InteractionSurface SurfaceForAction(ContextAction const action)
	{
		if (action == ContextAction::TAKE_COVER)
			return OS0InteractionSurface::BEHAVIOR;
		switch (ContextActionCategory(action))
		{
			case ActionCategory::GEAR:
				return OS0InteractionSurface::EQUIPMENT;
			case ActionCategory::STANCE:
				return OS0InteractionSurface::BEHAVIOR;
			case ActionCategory::WORLD:
			case ActionCategory::INFO:
			case ActionCategory::DEBUG:
				return OS0InteractionSurface::ENVIRONMENT;
			case ActionCategory::MOVEMENT:
			case ActionCategory::COMBAT:
			case ActionCategory::MEDICAL:
			case ActionCategory::SOCIAL:
			case ActionCategory::GROUP:
			case ActionCategory::COUNT:
				return OS0InteractionSurface::ACTIONS;
		}
		return OS0InteractionSurface::ACTIONS;
	}

	void ResetNearbyScanCache()
	{
		gNearbyHintActorGridNo = NOWHERE;
		gNearbyHintActorId = NOBODY;
		gNearbyHintActorInstanceId = 0;
		gNearbyHintActorLevel = 0xff;
		gNearbyHintToolSignature = 0;
		gNearbyHintCursorGridNo = NOWHERE;
		gNextNearbyHintScanAt = 0;
	}

	void SetInteractionForAction(ContextAction const action)
	{
		if (action == ContextAction::ATTACK)
		{
			InteractionMode().beginFight(OS0InteractionSurface::ACTIONS);
		}
		else if (action == ContextAction::MOVE)
		{
			// Leaving FIGHT first restores the remembered surface. Select ACTIONS
			// afterwards so that restoration cannot overwrite the requested normal
			// control surface.
			if (InteractionMode().nearbyScanEnabled())
				InteractionMode().returnToNormal();
			else
				InteractionMode().returnToNormal(OS0InteractionSurface::ACTIONS);
		}
		else
		{
			InteractionMode().beginInteraction(SurfaceForAction(action));
		}
	}

	SOLDIERTYPE* CarryCarrierSlot()
	{
		return CarryState().carrier != NOBODY ?
			ID2Soldier(CarryState().carrier) : nullptr;
	}

	SOLDIERTYPE* CarryCarrier()
	{
		OS0CarryState const& carry = CarryState();
		SOLDIERTYPE* const actor = CarryCarrierSlot();
		return actor && actor->bActive &&
			carry.boundToCarrier(carry.carrier,
				actor->uiUniqueSoldierIdValue) ? actor : nullptr;
	}

	void RefreshHeldItemCursor()
	{
		if (!gpItemPointer)
		{
			gLastItemCursorPointer = nullptr;
			gLastItemCursorItem = NOTHING;
			gNextItemCursorRefreshAt = 0;
			return;
		}
		const UINT32 now = GetJA2Clock();
		if (gLastItemCursorPointer == gpItemPointer &&
			gLastItemCursorItem == gpItemPointer->usItem &&
			now < gNextItemCursorRefreshAt) return;
		SetMouseCursorFromCurrentItem();
		gLastItemCursorPointer = gpItemPointer;
		gLastItemCursorItem = gpItemPointer->usItem;
		// Cursor annotations may change with the hovered destination, but rebuilding
		// the external item sprite every render frame made drag visibly flicker.
		gNextItemCursorRefreshAt = now + 80;
	}

	void OutlineBox(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour);
	void DrawIconCorners(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour);
	void DrawContextActionIcon(ContextAction action, INT16 x, INT16 y);
	void DrawFloatingPanelShell(FloatingPanel const& panel, FloatingPanelId id,
		const ST::string& title);
	void ContextActionCallback(MOUSE_REGION* region, UINT32 reason);
	void EquipmentSlotCallback(MOUSE_REGION* region, UINT32 reason);
	void EquipmentPackCallback(MOUSE_REGION* region, UINT32 reason);
	void ItemTransferIntentCallback(MOUSE_REGION* region, UINT32 reason);
	void ItemTransferMoreCallback(MOUSE_REGION* region, UINT32 reason);
	ItemTransferPolicyDecision CurrentItemTransferDecision(SOLDIERTYPE* actor);
	BOOLEAN ApplyItemTransferIntent(SOLDIERTYPE* actor, ItemTransferIntent intent);
	void StackSplitCallback(MOUSE_REGION* region, UINT32 reason);
	void GodIconCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryGrabberCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryCloseCallback(MOUSE_REGION* region, UINT32 reason);
	void AssetCatalogCallback(MOUSE_REGION* region, UINT32 reason);
	void ToolboxModuleCallback(MOUSE_REGION* region, UINT32 reason);
	void EnvironmentSkillCallback(MOUSE_REGION* region, UINT32 reason);
	void NearbyHintCallback(MOUSE_REGION* region, UINT32 reason);
	void NearbyHintMoveCallback(MOUSE_REGION* region, UINT32 reason);
	void HoverQuickActionMoveCallback(MOUSE_REGION* region, UINT32 reason);
	void HoverQuickActionCallback(MOUSE_REGION* region, UINT32 reason);
	void ActivateToolboxModule(ToolboxModule module);
	void SelectAdjacentSquad(INT8 direction);
	void ApplyCursorTool(ContextAction action);
	void SetBagRegionsEnabled(BOOLEAN enabled);
	size_t RefreshLootWorldItems();
	void UpdateLootProjectionState();
	void SetLootRegionsEnabled(BOOLEAN enabled);
	void PositionBagRegions();
	void PositionLootRegions();
	BOOLEAN GetActorDisplayAnchor(SOLDIERTYPE const* soldier, INT16& x, INT16& y);
	void ClampWindowPositions();
	BOOLEAN SaveUILayout();
	void LoadUILayout();
	BOOLEAN AssetCatalogKeyboardHook(InputAtom* event);
	void RecordFeedbackEvent(const ST::string& event);
	BOOLEAN TutorialKeyboardHook(InputAtom* event);
	void EnsureContainerLoot(GridNo gridNo, UINT8 level, UINT16 tileIndex);
	void CancelWorldMoveState();
	BOOLEAN BeginTrackedWorldItemTransfer(SOLDIERTYPE* actor, INT32 itemIndex,
		UINT16 containerTileIndex, BOOLEAN releaseAlreadyHandled);
	BOOLEAN CanPlaceObjectCompletelyInActorSlot(SOLDIERTYPE* actor,
		INT8 slot, OBJECTTYPE const& object);
	BOOLEAN PlaceObjectCompletelyInActorPack(SOLDIERTYPE* actor,
		OBJECTTYPE* object);
	BOOLEAN EquipInventorySlotAtomically(SOLDIERTYPE* actor, INT8 slot);
	BOOLEAN BeginTrackedInventoryItemTransfer(SOLDIERTYPE* source, INT8 slot,
		SOLDIERTYPE* cursorActor, OBJECTTYPE& detached,
		BOOLEAN releaseAlreadyHandled);
	BOOLEAN MoveInventoryItemToPackAtomically(SOLDIERTYPE* source, INT8 slot,
		SOLDIERTYPE* target);

	SGPVSurface* gInspectorPreview = nullptr;
	SGPVSurface* gAnimatedMercPreview = nullptr;
	SOLDIERTYPE const* gAnimatedMercPreviewSoldier = nullptr;
	SGPVSurface* gAssetLibrarySymbolSurface = nullptr;
	MOUSE_REGION gGodLibraryBlock;
	MOUSE_REGION gGodLibraryGrabber;
	MOUSE_REGION gGodLibraryClose;
	std::array<MOUSE_REGION, GOD_ICON_COUNT + 1> gGodIconRegions;
	std::array<MOUSE_REGION, 12> gAssetLibraryRegions;
	std::array<MOUSE_REGION, static_cast<size_t>(ToolboxModule::COUNT)> gToolboxRegions;
	std::array<MOUSE_REGION, ENVIRONMENT_SKILL_COUNT> gEnvironmentSkillRegions;
	std::array<MOUSE_REGION, NEARBY_HINT_COUNT> gNearbyHintRegions;
	MOUSE_REGION gHoverQuickActionRegion;
	MOUSE_REGION gAssetCatalogBlock;
	std::array<MOUSE_REGION, 11> gAssetCatalogRegions;
	MOUSE_REGION gContextBlock;
	MOUSE_REGION gBagBlock;
	MOUSE_REGION gBagGrabber;
	MOUSE_REGION gBagClose;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelBlocks;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelGrabbers;
	std::array<MOUSE_REGION, static_cast<size_t>(FloatingPanelId::COUNT)> gFloatingPanelCloses;
	std::array<MOUSE_REGION, PANEL_DOCK_COUNT> gPanelDockRegions;
	std::array<MOUSE_REGION, 4> gFeedbackRegions;
	std::array<MOUSE_REGION, 3> gSectorUpgradeRegions;
	std::array<MOUSE_REGION, 4> gSectorTabRegions;
	MOUSE_REGION gStrategicMapRegion;
	MOUSE_REGION gSectorTeamRegion;
	std::array<MOUSE_REGION, NUM_INV_SLOTS> gSlotRegions;
	std::array<MOUSE_REGION, 7> gEquipmentRegions;
	MOUSE_REGION gEquipmentPackRegion;
	std::array<MOUSE_REGION, gOS0ItemTransferIntents.size()> gItemTransferIntentRegions;
	MOUSE_REGION gItemTransferMoreRegion;
	UINT8 gItemTransferTargetId = NOBODY;
	UINT32 gItemTransferTargetInstanceId = 0;
	UINT8 gHeldItemCarrierId = NOBODY;
	UINT32 gHeldItemCarrierInstanceId = 0;
	BOOLEAN gHeldItemRecoveryPending = FALSE;
	BOOLEAN gItemTransferMoreVisible = FALSE;
	MOUSE_REGION gStackSplitBlock;
	std::array<MOUSE_REGION, 5> gStackSplitRegions;
	MOUSE_REGION gOrbRegion;
	MOUSE_REGION gCombatModeRegion;
	MOUSE_REGION gTutorialContinue;
	std::array<MOUSE_REGION, 20> gTutorialStats;
	std::array<MOUSE_REGION, 4> gTutorialBodyRegions;
	std::array<MOUSE_REGION, 15> gTutorialTraitRegions;
	std::array<MOUSE_REGION, 12> gContextRegions;
	std::array<MOUSE_REGION, 12> gLootRegions;
	std::array<INT32, 12> gLootWorldItems;
	std::array<ST::string, NUM_INV_SLOTS> gSlotHelp;
	std::array<ST::string, 7> gEquipmentHelp;
	std::array<ST::string, 12> gLootHelp;
	GridNo gLootGridNo = NOWHERE;
	UINT8 gLootLevel = 0;
	UINT16 gLootTileIndex = NO_TILE;
	UINT8 gLootActorId = NOBODY;
	UINT32 gLootActorInstanceId = 0;
	SOLDIERTYPE* gContextSoldier = nullptr;
	GridNo gContextGridNo = NOWHERE;
	UINT8 gContextLevel = 0;
	UINT16 gContextTileIndex = NO_TILE;
	INT8 gContextInventorySlot = NO_SLOT;
	INT32 gContextWorldItemIndex = -1;
	DeferredContextIdentity gDeferredContextIdentity{};
	std::array<ContextEntry, 12> gContextEntries;
	size_t gContextEntryCount = 0;
	ST::string gContextTitle = "CONTEXT";
	ST::string gHoverTitle;
	ST::string gHoverDetail;
	ST::string gHoverDebugDetail;
	ST::string gHoverQuickActionHelp;
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

	auto const& gSectorUpgrades = OS0SectorUpgrades();

	constexpr std::array<const char*, 10> gTutorialStatNames{{
		"HEALTH", "AGILITY", "DEXTERITY", "STRENGTH", "WISDOM",
		"LEADERSHIP", "MARKSMANSHIP", "MEDICAL", "MECHANICAL", "EXPLOSIVES"
	}};
	constexpr std::array<SoldierBodyType, 4> gTutorialBodyTypes{{
		REGMALE, BIGMALE, STOCKYMALE, REGFEMALE
	}};
	constexpr std::array<const char*, 4> gTutorialBodyNames{{
		"MALE / REG", "MALE / LARGE", "MALE / STOCKY", "FEMALE"
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

	BOOLEAN EnabledRegionContains(MOUSE_REGION const& region, INT16 const x,
		INT16 const y) noexcept
	{
		return (region.uiFlags & MSYS_REGION_ENABLED) &&
			x >= region.RegionTopLeftX && x <= region.RegionBottomRightX &&
			y >= region.RegionTopLeftY && y <= region.RegionBottomRightY;
	}

	BOOLEAN MultiToolOwnsPointerAt(INT16 const x, INT16 const y) noexcept
	{
		if (EnabledRegionContains(gOrbRegion, x, y) ||
			EnabledRegionContains(gCombatModeRegion, x, y)) return TRUE;
		return std::any_of(gPanelDockRegions.begin(), gPanelDockRegions.end(),
			[x, y](MOUSE_REGION const& region)
			{ return EnabledRegionContains(region, x, y); });
	}

	void MixWorldProjectionValue(std::uint64_t& stamp,
		std::uint64_t const value) noexcept
	{
		// FNV-1a is sufficient here: this is a cheap per-frame change detector,
		// not a persisted identity or security boundary.
		stamp ^= value;
		stamp *= 1099511628211ULL;
	}

	void MixWorldProjectionRegion(std::uint64_t& stamp,
		MOUSE_REGION const& region) noexcept
	{
		const BOOLEAN enabled =
			(region.uiFlags & MSYS_REGION_ENABLED) != 0;
		MixWorldProjectionValue(stamp, enabled ? 1 : 0);
		if (!enabled) return;
		MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(
			region.RegionTopLeftX));
		MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(
			region.RegionTopLeftY));
		MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(
			region.RegionBottomRightX));
		MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(
			region.RegionBottomRightY));
	}

	std::uint64_t WorldProjectionStamp()
	{
		std::uint64_t stamp = 14695981039346656037ULL;
		MixWorldProjectionRegion(stamp, gHoverQuickActionRegion);
		for (MOUSE_REGION const& region : gNearbyHintRegions)
			MixWorldProjectionRegion(stamp, region);
		for (MOUSE_REGION const& region : gLootRegions)
			MixWorldProjectionRegion(stamp, region);
		for (MOUSE_REGION const& region : gEquipmentRegions)
			MixWorldProjectionRegion(stamp, region);
		MixWorldProjectionRegion(stamp, gEquipmentPackRegion);
		MixWorldProjectionRegion(stamp, gContextBlock);
		for (MOUSE_REGION const& region : gContextRegions)
			MixWorldProjectionRegion(stamp, region);

		// The selection bracket has no mouse region, but is still attached to the
		// scrolling world. Include its exact draw anchor so movement/zoom clears
		// the pixels from the preceding frame before RenderWorld runs.
		const GridNo selectionGrid = gInspectedSoldier ?
			gInspectedSoldier->sGridNo : gInspectedGridNo;
		const UINT8 selectionLevel = gInspectedSoldier ?
			gInspectedSoldier->bLevel : gInspectedLevel;
		const BOOLEAN selectionVisible = !gContextVisible &&
			selectionGrid >= 0 && selectionGrid < WORLD_MAX;
		MixWorldProjectionValue(stamp, selectionVisible ? 1 : 0);
		if (selectionVisible)
		{
			INT16 x;
			INT16 y;
			GetGridNoScreenPos(selectionGrid, selectionLevel, &x, &y);
			OS0MapWorldToDisplayScreen(&x, &y);
			MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(x));
			MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(y));
		}

		const BOOLEAN tutorialTargetVisible = !gContextVisible &&
			gFieldTutorial.active() && !gTutorialActive &&
			gFieldTutorialGridNo >= 0 && gFieldTutorialGridNo < WORLD_MAX;
		MixWorldProjectionValue(stamp, tutorialTargetVisible ? 1 : 0);
		if (tutorialTargetVisible)
		{
			INT16 x;
			INT16 y;
			GetGridNoScreenPos(gFieldTutorialGridNo, gFieldTutorialLevel, &x, &y);
			OS0MapWorldToDisplayScreen(&x, &y);
			MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(x));
			MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(y));
		}

		// Carry visuals are screen-space projections. Include only values that can
		// move their pixels, so an attached but stationary grab does not force a
		// complete tactical redraw at the frame rate.
		OS0CarryState const& carry = CarryState();
		MixWorldProjectionValue(stamp, carry.active() ? 1 : 0);
		if (carry.active())
		{
			MixWorldProjectionValue(stamp, static_cast<UINT8>(carry.phase));
			MixWorldProjectionValue(stamp, static_cast<UINT8>(carry.mode));
			MixWorldProjectionValue(stamp, carry.lifted ? 1 : 0);
			if (carry.pending() && !carry.persistentGrab)
			{
				MixWorldProjectionValue(stamp, gusMouseXPos);
				MixWorldProjectionValue(stamp, gusMouseYPos);
				MixWorldProjectionValue(stamp, (GetJA2Clock() / 90) % 4);
			}
			else if (SOLDIERTYPE const* const actor = CarryCarrier())
			{
				INT16 x;
				INT16 y;
				if (GetActorDisplayAnchor(actor, x, y))
				{
					MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(x));
					MixWorldProjectionValue(stamp, static_cast<std::uint16_t>(y));
					MixWorldProjectionValue(stamp, actor->bDirection);
				}
				if (carry.walking())
					MixWorldProjectionValue(stamp, (GetJA2Clock() / 90) % 4);
			}
		}
		return stamp;
	}

	INT16 WorkspaceBottom()
	{
		return gUILayout.workspaceBottom();
	}

	void ApplyArtworkWorkspaceLayout(BOOLEAN positionCharacter)
	{
		const INT16 bottom = WorkspaceBottom();
		FloatingPanel& sector =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		FloatingPanel& inspector =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::INSPECTOR)];
		FloatingPanel& toolbox =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLBOX)];
		FloatingPanel& environment =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ENVIRONMENT)];

		sector.x = std::max<INT16>(0, gsVIEWPORT_END_X - sector.w - 8);
		sector.y = std::max<INT16>(8, bottom - sector.h - 8);
		inspector.x = 8;
		inspector.y = std::max<INT16>(8, bottom - inspector.h - 8);
		toolbox.x = std::max<INT16>(8, gsVIEWPORT_END_X - toolbox.w - 8);
		toolbox.y = std::max<INT16>(8, bottom - toolbox.h - 8);
		environment.x = 8;
		environment.y = std::min<INT16>(56,
			std::max<INT16>(8, bottom - environment.h - 8));
		if (positionCharacter)
		{
			gBagX = std::max<INT16>(0, gsVIEWPORT_END_X - PANE_W - 8);
			gBagY = 8;
		}
	}

	void ClampMultiToolPosition()
	{
		const INT16 maxX = std::max<INT16>(0,
			gsVIEWPORT_END_X - COLLAPSED_OS0_W);
		const INT16 minY = std::max<INT16>(0, gsVIEWPORT_WINDOW_START_Y);
		const INT16 maxY = std::max<INT16>(minY,
			gUILayout.worldBottom() - COMMAND_BAR_H);
		gOrbX = std::clamp<INT16>(gOrbX, 0, maxX);
		gOrbY = std::clamp<INT16>(gOrbY, minY, maxY);
	}

	void ClampWindowPositions()
	{
		gUIRuntime.windowManager().clampAll();
		ClampMultiToolPosition();
		gInventoryX = gBagX;
		gInventoryY = gBagY;
	}

	BOOLEAN SaveUILayout()
	{
		try
		{
			GCM->userPrivateFiles()->createDir("OS0");
			ST::string output = gUIRuntime.windowManager().serializeLayout(
				SCREEN_WIDTH, SCREEN_HEIGHT);
			output += ST::format("multitool {} {}\n", gOrbX, gOrbY);
			AutoSGPFile file{
				GCM->userPrivateFiles()->openForWriting(UI_LAYOUT_PATH, true) };
			file->write(output.c_str(), output.size());
			return TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}

	void LoadUILayout()
	{
		INT32 savedWidth = SCREEN_WIDTH;
		INT32 savedHeight = SCREEN_HEIGHT;
		INT32 bagX = gBagX;
		INT32 bagY = gBagY;
		INT32 libraryX = gGodLibraryX;
		INT32 libraryY = gGodLibraryY;
		INT32 orbX = gOrbX;
		INT32 orbY = gOrbY;
		BOOLEAN hasOrbPosition = FALSE;
		std::array<INT32, static_cast<size_t>(FloatingPanelId::COUNT)> panelX{};
		std::array<INT32, static_cast<size_t>(FloatingPanelId::COUNT)> panelY{};
		for (size_t i = 0; i < gFloatingPanels.size(); ++i)
		{
			panelX[i] = gFloatingPanels[i].x;
			panelY[i] = gFloatingPanels[i].y;
		}
		try
		{
			AutoSGPFile file{
				GCM->userPrivateFiles()->openForReading(UI_LAYOUT_PATH) };
			ST::string const savedLayout = file->readStringToEnd();
			std::string const layoutText = savedLayout.to_std_string();
			// The multitool is deliberately not a normal rectangular application
			// window: its width changes as it unfolds. Persist only its anchor and
			// scale that anchor with the rest of the desktop.
			{
				std::istringstream metadata(layoutText);
				std::string line;
				while (std::getline(metadata, line))
				{
					std::istringstream row(line);
					std::string key;
					row >> key;
					if (key == "screen") row >> savedWidth >> savedHeight;
					else if (key == "multitool" && (row >> orbX >> orbY))
						hasOrbPosition = TRUE;
				}
			}
			if (layoutText.find("window ") != std::string::npos)
			{
				gUIRuntime.windowManager().restoreLayout(savedLayout,
					SCREEN_WIDTH, SCREEN_HEIGHT);
				if (hasOrbPosition)
				{
					gOrbX = static_cast<INT16>(orbX * SCREEN_WIDTH /
						std::max<INT32>(1, savedWidth));
					gOrbY = static_cast<INT16>(orbY * SCREEN_HEIGHT /
						std::max<INT32>(1, savedHeight));
				}
				gUIRuntime.hideTransientWorldPanels();
				ClampWindowPositions();
				return;
			}
			std::istringstream stream(savedLayout.c_str());
			std::string line;
			while (std::getline(stream, line))
			{
				if (line.empty() || line[0] == '#') continue;
				std::istringstream row(line);
				std::string key;
				row >> key;
				if (key == "screen") row >> savedWidth >> savedHeight;
				// v1 stored transient toolbox visibility. It is intentionally ignored.
				else if (key == "expanded")
				{
					INT32 ignored = 0;
					row >> ignored;
				}
				else if (key == "bag") row >> bagX >> bagY;
				else if (key == "library") row >> libraryX >> libraryY;
				else if (key == "multitool" && (row >> orbX >> orbY))
					hasOrbPosition = TRUE;
				else if (key == "panel")
				{
					std::string panelName;
					INT32 x = 0;
					INT32 y = 0;
					if (!(row >> panelName >> x >> y)) continue;
					FloatingPanelId const known =
						OS0UIWindowFromPersistenceKey(panelName.c_str());
					size_t index = known == FloatingPanelId::COUNT ?
						gFloatingPanels.size() : static_cast<size_t>(known);
					if (known == FloatingPanelId::COUNT)
					{
						// v1 used the old eight-panel enum. Migrate only the three
						// surviving windows; dead prototype geometry is discarded.
						const INT32 legacy = std::stoi(panelName);
						if (legacy == 4) index = static_cast<size_t>(FloatingPanelId::SECTOR);
						else if (legacy == 6) index = static_cast<size_t>(FloatingPanelId::INSPECTOR);
						else if (legacy == 7) index = static_cast<size_t>(FloatingPanelId::TOOLBOX);
					}
					if (index < gFloatingPanels.size())
					{
						panelX[index] = x;
						panelY[index] = y;
					}
				}
			}
		}
		catch (...)
		{
			return;
		}

		const INT32 safeWidth = std::max<INT32>(1, savedWidth);
		const INT32 safeHeight = std::max<INT32>(1,
			savedHeight - COMMAND_BAR_H);
		auto scaleX = [safeWidth](INT32 value)
		{
			return static_cast<INT16>(value * SCREEN_WIDTH / safeWidth);
		};
		auto scaleY = [safeHeight](INT32 value)
		{
			return static_cast<INT16>(value * WorkspaceBottom() / safeHeight);
		};
		gBagX = scaleX(bagX);
		gBagY = scaleY(bagY);
		gGodLibraryX = scaleX(libraryX);
		gGodLibraryY = scaleY(libraryY);
		if (hasOrbPosition)
		{
			gOrbX = scaleX(orbX);
			gOrbY = static_cast<INT16>(orbY * SCREEN_HEIGHT /
				std::max<INT32>(1, savedHeight));
		}
		for (size_t i = 0; i < gFloatingPanels.size(); ++i)
		{
			gFloatingPanels[i].x = scaleX(panelX[i]);
			gFloatingPanels[i].y = scaleY(panelY[i]);
		}
		// Persist geometry, never transient open/closed state. A toolbox left open
		// in an earlier run must not materialize underneath the first world click.
		gUIRuntime.windowManager().hide(
			gUIRuntime.managedId(FloatingPanelId::TOOLBOX));
		gUIRuntime.windowManager().hide(
			gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
		ClampWindowPositions();
	}

	void BindSoldierView(SOLDIERTYPE*& view, UINT32& instanceId,
		SOLDIERTYPE* soldier) noexcept
	{
		if (!soldier || !soldier->bActive ||
			soldier->uiUniqueSoldierIdValue == 0)
		{
			view = nullptr;
			instanceId = 0;
			return;
		}
		view = soldier;
		instanceId = soldier->uiUniqueSoldierIdValue;
	}

	SOLDIERTYPE* ResolveSoldierView(SOLDIERTYPE*& view,
		UINT32& instanceId) noexcept
	{
		if (!view || instanceId == 0 || !view->bActive ||
			view->uiUniqueSoldierIdValue != instanceId)
		{
			view = nullptr;
			instanceId = 0;
			return nullptr;
		}
		return view;
	}

	void BindInspectedSoldier(SOLDIERTYPE* soldier) noexcept
	{
		BindSoldierView(gInspectedSoldier, gInspectedSoldierInstanceId, soldier);
	}

	void BindInventorySoldier(SOLDIERTYPE* soldier) noexcept
	{
		BindSoldierView(gInventorySoldier, gInventorySoldierInstanceId, soldier);
	}

	void BindEquipmentSoldier(SOLDIERTYPE* soldier) noexcept
	{
		BindSoldierView(gEquipmentSoldier, gEquipmentSoldierInstanceId, soldier);
	}

	BOOLEAN SoldierViewIdentityMatches(SOLDIERTYPE const* soldier) noexcept
	{
		if (!soldier) return FALSE;
		if (soldier == gInspectedSoldier &&
			soldier->uiUniqueSoldierIdValue != gInspectedSoldierInstanceId)
			return FALSE;
		if (soldier == gInventorySoldier &&
			soldier->uiUniqueSoldierIdValue != gInventorySoldierInstanceId)
			return FALSE;
		if (soldier == gEquipmentSoldier &&
			soldier->uiUniqueSoldierIdValue != gEquipmentSoldierInstanceId)
			return FALSE;
		return TRUE;
	}

	BOOLEAN RevalidateSoldierViews() noexcept
	{
		BOOLEAN changed = FALSE;
		if (gInspectedSoldier && !ResolveSoldierView(gInspectedSoldier,
			gInspectedSoldierInstanceId)) changed = TRUE;
		if (gInventorySoldier && !ResolveSoldierView(gInventorySoldier,
			gInventorySoldierInstanceId)) changed = TRUE;
		if (gEquipmentSoldier && !ResolveSoldierView(gEquipmentSoldier,
			gEquipmentSoldierInstanceId))
		{
			gEquipmentExplodedVisible = FALSE;
			changed = TRUE;
		}
		return changed;
	}

	BOOLEAN CanAccessSoldierContents(SOLDIERTYPE const* soldier)
	{
		if (!soldier || !soldier->bActive ||
			soldier->uiUniqueSoldierIdValue == 0 ||
			!SoldierViewIdentityMatches(soldier) ||
			soldier->sSector != gWorldSector ||
			soldier->sGridNo < 0 || soldier->sGridNo >= WORLD_MAX ||
			soldier->bLevel < 0 || soldier->bLevel > 1) return FALSE;
		if (soldier->bTeam == OUR_TEAM) return TRUE;
		SOLDIERTYPE const* const selected = GetSelectedMan();
		return selected && selected->bActive &&
			selected->uiUniqueSoldierIdValue != 0 &&
			selected->sSector == gWorldSector &&
			selected->sGridNo >= 0 && selected->sGridNo < WORLD_MAX &&
			selected->bLevel == soldier->bLevel &&
			PythSpacesAway(selected->sGridNo, soldier->sGridNo) <= 2 &&
			(soldier->bLife < OKLIFE || soldier->uiStatusFlags & SOLDIER_DEAD);
	}

	void ClearItemTransferTarget() noexcept
	{
		gItemTransferTargetId = NOBODY;
		gItemTransferTargetInstanceId = 0;
	}

	void BindItemTransferTarget(SOLDIERTYPE const* actor) noexcept
	{
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0)
		{
			ClearItemTransferTarget();
			return;
		}
		gItemTransferTargetId = Soldier2ID(actor);
		gItemTransferTargetInstanceId = actor->uiUniqueSoldierIdValue;
	}

	SOLDIERTYPE* BoundItemTransferTarget() noexcept
	{
		if (gItemTransferTargetId == NOBODY ||
			gItemTransferTargetInstanceId == 0) return nullptr;
		SOLDIERTYPE* const actor = ID2Soldier(gItemTransferTargetId);
		return actor && actor->bActive &&
			actor->uiUniqueSoldierIdValue == gItemTransferTargetInstanceId ?
			actor : nullptr;
	}

	BOOLEAN HasItemTransferTargetBinding() noexcept
	{
		return gItemTransferTargetId != NOBODY &&
			gItemTransferTargetInstanceId != 0;
	}

	void ClearHeldItemCarrier() noexcept
	{
		gHeldItemCarrierId = NOBODY;
		gHeldItemCarrierInstanceId = 0;
	}

	void BindHeldItemCarrier(SOLDIERTYPE const* actor) noexcept
	{
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0)
		{
			ClearHeldItemCarrier();
			return;
		}
		gHeldItemCarrierId = Soldier2ID(actor);
		gHeldItemCarrierInstanceId = actor->uiUniqueSoldierIdValue;
	}

	SOLDIERTYPE* BoundHeldItemCarrier() noexcept
	{
		if (!gpItemPointer || gHeldItemCarrierId == NOBODY ||
			gHeldItemCarrierInstanceId == 0) return nullptr;
		SOLDIERTYPE* const actor = ID2Soldier(gHeldItemCarrierId);
		return actor && actor->bActive &&
			actor->uiUniqueSoldierIdValue == gHeldItemCarrierInstanceId &&
			gpItemPointerSoldier == actor && actor->sSector == gWorldSector &&
			actor->sGridNo >= 0 && actor->sGridNo < WORLD_MAX ? actor : nullptr;
	}

	BOOLEAN HeldItemRelationInReach(SOLDIERTYPE const* carrier,
		SOLDIERTYPE const* target) noexcept
	{
		return carrier && target && carrier->bActive && target->bActive &&
			carrier->sSector == gWorldSector && target->sSector == gWorldSector &&
			!carrier->fBetweenSectors && !target->fBetweenSectors &&
			carrier->bLevel == target->bLevel &&
			carrier->sGridNo >= 0 && carrier->sGridNo < WORLD_MAX &&
			target->sGridNo >= 0 && target->sGridNo < WORLD_MAX &&
			PythSpacesAway(carrier->sGridNo, target->sGridNo) <= 2;
	}

	BOOLEAN RecoverHeldItemCarrierIfPossible()
	{
		if (!gHeldItemRecoveryPending) return TRUE;
		if (!gpItemPointer)
		{
			gHeldItemRecoveryPending = FALSE;
			ClearHeldItemCarrier();
			return TRUE;
		}

		SOLDIERTYPE* const actor = GetSelectedMan();
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0 ||
			actor->sSector != gWorldSector || actor->sGridNo < 0 ||
			actor->sGridNo >= WORLD_MAX || actor->bLevel < 0 || actor->bLevel > 1)
			return FALSE;

		// A failed teardown preservation deliberately erased the old live-world
		// source. Rebind the still-authoritative native cursor only after the new
		// tactical actor exists; this can neither restore into nor dereference the
		// unloaded world.
		gpItemPointerSoldier = actor;
		gbItemPointerSrcSlot = NO_SLOT;
		OS0GetItemTransferRuntime().reset();
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reset();
		transfers.adoptExternalHeldItemAfterHandledRelease();
		BindHeldItemCarrier(actor);
		gHeldItemRecoveryPending = FALSE;
		RecordFeedbackEvent("HELD ITEM RECOVERED / EXTERNAL SOURCE");
		return TRUE;
	}

	OS0SectorKey CurrentSectorKey()
	{
		return { static_cast<UINT8>(gWorldSector.x),
			static_cast<UINT8>(gWorldSector.y), gWorldSector.z };
	}

	OS0SectorEconomySystem& CurrentSectorEconomy()
	{
		OS0SectorEconomySystem& economy =
			OS0GetTacticalSession().state().sectorEconomy;
		OS0SectorKey const sector = CurrentSectorKey();
		if (!economy.legacyMigrated(sector))
		{
			UINT32 legacy = 0;
			if (gWorldSector.z == 0)
			{
				UINT32& facilities = SectorInfo[gWorldSector.AsByte()].uiFacilitiesFlags;
				legacy = facilities;
				facilities &= ~OS0LegacySectorEconomyMask();
			}
			economy.migrateLegacy(sector, legacy);
		}
		return economy;
	}

	UINT16 SectorResource(ResourceKind kind)
	{
		return CurrentSectorEconomy().resource(CurrentSectorKey(), kind);
	}

	BOOLEAN HasUpgrade(SectorUpgrade const& upgrade)
	{
		return CurrentSectorEconomy().hasUpgrade(CurrentSectorKey(), upgrade.flag);
	}

	BOOLEAN CanBuildUpgrade(SectorUpgrade const& upgrade)
	{
		auto const found = std::find_if(gSectorUpgrades.begin(),
			gSectorUpgrades.end(), [&](SectorUpgrade const& candidate)
			{ return candidate.flag == upgrade.flag; });
		return found != gSectorUpgrades.end() &&
			OS0CanBuildSectorUpgrade(CurrentSectorEconomy(), CurrentSectorKey(),
				static_cast<size_t>(found - gSectorUpgrades.begin()));
	}

	BOOLEAN BuildSectorUpgrade(size_t index)
	{
		if (!OS0BuildSectorUpgrade(CurrentSectorEconomy(), CurrentSectorKey(),
			index)) return FALSE;
		RecordFeedbackEvent(ST::format("SECTOR UPGRADE {} {}",
			gWorldSector.AsShortString(), gSectorUpgrades[index].name));
		return TRUE;
	}

	BOOLEAN RecoverTeamAtFieldShelter()
	{
		if (!HasUpgrade(gSectorUpgrades[0])) return FALSE;
		BOOLEAN recovered = FALSE;
		for (INT32 id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
			id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
		{
			SOLDIERTYPE& soldier = GetMan(id);
			if (!soldier.bActive || soldier.bLife <= 0 ||
				soldier.sSector != gWorldSector || soldier.fBetweenSectors) continue;
			soldier.bLife = soldier.bLifeMax;
			soldier.bBleeding = 0;
			soldier.bBreathMax = 100;
			soldier.bBreath = 100;
			soldier.sBreathRed = 0;
			soldier.fMercCollapsedFlag = FALSE;
			recovered = TRUE;
		}
		if (recovered)
		{
			fInterfacePanelDirty = DIRTYLEVEL2;
			RecordFeedbackEvent(ST::format("FIELD SHELTER RECOVERY {}",
				gWorldSector.AsShortString()));
		}
		return recovered;
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
				ContextActionName(CursorState().action));
			if (selected)
			{
				report += ST::format(
					"Selected merc: {}\nGrid: {} / level {}\nHP: {}/{} / AP: {}\n\n",
					selected->name, selected->sGridNo, selected->bLevel,
					selected->bLife, selected->bLifeMax, selected->bActionPoints);
			}
			report += ST::format(
				"Inspected target: {}\nGrid: {} / level {} / tile {}\n"
				"Panels: character={} radial={} loot={} equipment={} strategy={} "
				"inspector={} toolbox={}\n\n"
				"TESTER DESCRIPTION\n------------------\n{}\n\n"
				"RECENT OS0 EVENTS\n-----------------\n",
				gContextTitle, gInspectedGridNo, gInspectedLevel,
				gInspectedTileIndex, gBagVisible,
				gContextVisible, gLootVisible, gEquipmentExplodedVisible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)].visible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::INSPECTOR)].visible,
				gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLBOX)].visible,
				gFeedbackText.empty() ? "(No description entered.)" : gFeedbackText);

			for (size_t i = 0; i < gFeedbackEventCount; ++i)
			{
				const size_t index = (gFeedbackEventNext + gFeedbackEvents.size() -
					gFeedbackEventCount + i) % gFeedbackEvents.size();
				report += gFeedbackEvents[index] + "\n";
			}
			report += "\nASSET CATALOG SNAPSHOT\n----------------------\n";
			report += OS0SerializeAssetCatalog();
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

	void StopFeedbackEditing()
	{
		if (!gFeedbackEditing) return;
		gFeedbackEditing = FALSE;
		SetUIKeyboardHook(gTutorialActive &&
			gUIRuntime.creatorStage() == OS0CreatorStage::IDENTITY ?
			TutorialKeyboardHook : nullptr);
	}

	BOOLEAN FeedbackKeyboardHook(InputAtom* event)
	{
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::SECTOR)) ||
			gSectorPanelMode != SectorPanelMode::REPORT ||
			!gFeedbackEditing) return FALSE;
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

	BOOLEAN CarryStructureIdentityMatches(OS0CarryState const& carry,
		STRUCTURE const* structure)
	{
		return structure &&
			carry.boundToStructure(structure->usStructureID,
				StructureBaseGridNo(structure));
	}

	void BindCarryShadowInstance()
	{
		OS0CarryState& carry = CarryState();
		carry.shadowInstance = 0;
		if (!carry.active() || carry.source < 0 ||
			carry.source >= WORLD_MAX || carry.sourceLevel != 0 ||
			carry.tileIndex >= NUMBEROFTILES) return;
		TILE_ELEMENT const& tile = gTileDatabase[carry.tileIndex];
		if (!(tile.uiFlags & HAS_SHADOW_BUDDY) || tile.sBuddyNum < 0) return;
		for (LEVELNODE const* shadow =
			gpWorldLevelData[carry.source].pShadowHead;
			shadow; shadow = shadow->pNext)
		{
			if ((shadow->uiFlags & LEVELNODE_BUDDYSHADOW) &&
				shadow->usIndex == static_cast<UINT16>(tile.sBuddyNum))
			{
				carry.shadowInstance = reinterpret_cast<std::uintptr_t>(shadow);
				return;
			}
		}
	}

	STRUCTURE* CarryStructure()
	{
		OS0CarryState const& carry = CarryState();
		if (!carry.active()) return nullptr;
		STRUCTURE* const structure = WorldStructureAt(carry.source,
			carry.sourceLevel, carry.tileIndex);
		return CarryStructureIdentityMatches(carry, structure) ?
			structure : nullptr;
	}

	void BindLootActor(SOLDIERTYPE const* actor)
	{
		gLootActorId = actor && actor->bActive ? Soldier2ID(actor) : NOBODY;
		gLootActorInstanceId = actor && actor->bActive ?
			actor->uiUniqueSoldierIdValue : 0;
	}

	SOLDIERTYPE* BoundLootActor()
	{
		if (gLootActorId == NOBODY || gLootActorInstanceId == 0) return nullptr;
		SOLDIERTYPE* const actor = ID2Soldier(gLootActorId);
		return actor && actor->bActive &&
			actor->uiUniqueSoldierIdValue == gLootActorInstanceId ? actor : nullptr;
	}

	void MixDeferredItemFingerprint(std::uint64_t& hash, void const* data,
		size_t size) noexcept
	{
		auto const* bytes = static_cast<unsigned char const*>(data);
		for (size_t i = 0; i < size; ++i)
		{
			hash ^= bytes[i];
			hash *= 1099511628211ULL;
		}
	}

	std::uint64_t DeferredItemFingerprint(OBJECTTYPE const& object) noexcept
	{
		if (object.usItem == NOTHING || object.ubNumberOfObjects == 0) return 0;
		std::uint64_t hash = 14695981039346656037ULL;
		MixDeferredItemFingerprint(hash, &object.usItem, sizeof(object.usItem));
		MixDeferredItemFingerprint(hash, &object.ubNumberOfObjects,
			sizeof(object.ubNumberOfObjects));
		// OBJECTTYPE contains one structural padding byte before its union. Hash the
		// persisted payload and fields explicitly so an irrelevant padding value can
		// never invalidate a still-identical menu subject.
		constexpr size_t payloadSize = offsetof(OBJECTTYPE, usAttachItem) -
			offsetof(OBJECTTYPE, bStatus);
		auto const* const bytes =
			reinterpret_cast<unsigned char const*>(&object);
		MixDeferredItemFingerprint(hash, bytes + offsetof(OBJECTTYPE, bStatus),
			payloadSize);
		MixDeferredItemFingerprint(hash, object.usAttachItem,
			sizeof(object.usAttachItem));
		MixDeferredItemFingerprint(hash, object.bAttachStatus,
			sizeof(object.bAttachStatus));
		MixDeferredItemFingerprint(hash, &object.fFlags, sizeof(object.fFlags));
		MixDeferredItemFingerprint(hash, &object.ubMission, sizeof(object.ubMission));
		MixDeferredItemFingerprint(hash, &object.bTrap, sizeof(object.bTrap));
		MixDeferredItemFingerprint(hash, &object.ubImprintID,
			sizeof(object.ubImprintID));
		MixDeferredItemFingerprint(hash, &object.ubWeight, sizeof(object.ubWeight));
		MixDeferredItemFingerprint(hash, &object.fUsed, sizeof(object.fUsed));
		return hash;
	}

	OS0ItemSourceIdentity InventorySourcePressIdentity(
		SOLDIERTYPE const* actor, INT8 const slot) noexcept
	{
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0 ||
			slot < 0 || slot >= NUM_INV_SLOTS ||
			actor->inv[slot].usItem == NOTHING) return {};
		OS0ItemSourceIdentity identity;
		identity.actorInstanceId = actor->uiUniqueSoldierIdValue;
		identity.itemFingerprint = DeferredItemFingerprint(actor->inv[slot]);
		return identity;
	}

	OS0ItemSourceIdentity WorldSourcePressIdentity(
		SOLDIERTYPE const* actor, WORLDITEM const& item) noexcept
	{
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0 ||
			!item.fExists || item.o.usItem == NOTHING) return {};
		OS0ItemSourceIdentity identity;
		identity.actorInstanceId = actor->uiUniqueSoldierIdValue;
		identity.itemFingerprint = DeferredItemFingerprint(item.o);
		identity.gridNo = item.sGridNo;
		identity.level = static_cast<INT8>(item.ubLevel);
		identity.visibility = item.bVisible;
		identity.flags = item.usFlags;
		identity.renderZHeight = item.bRenderZHeightAboveLevel;
		identity.worldItemRevision = WorldItemMutationRevision();
		return identity;
	}

	DeferredContextIdentity CharacterContextIdentity(SOLDIERTYPE const* actor)
	{
		DeferredContextIdentity identity;
		if (!actor || !actor->bActive || actor->uiUniqueSoldierIdValue == 0)
			return identity;
		identity.kind = DeferredContextKind::CHARACTER;
		identity.actorId = Soldier2ID(actor);
		identity.actorInstanceId = actor->uiUniqueSoldierIdValue;
		return identity;
	}

	DeferredContextIdentity InventoryContextIdentity(SOLDIERTYPE const* actor,
		INT8 slot)
	{
		DeferredContextIdentity identity = CharacterContextIdentity(actor);
		if (identity.kind == DeferredContextKind::NONE || slot < 0 ||
			slot >= NUM_INV_SLOTS || actor->inv[slot].usItem == NOTHING)
			return {};
		identity.kind = DeferredContextKind::INVENTORY_ITEM;
		identity.inventorySlot = slot;
		identity.itemFingerprint = DeferredItemFingerprint(actor->inv[slot]);
		return identity;
	}

	DeferredContextIdentity WorldItemContextIdentity(SOLDIERTYPE const* actor,
		INT32 itemIndex, UINT16 tileIndex)
	{
		DeferredContextIdentity identity = CharacterContextIdentity(actor);
		if (identity.kind == DeferredContextKind::NONE || itemIndex < 0 ||
			static_cast<size_t>(itemIndex) >= gWorldItems.size()) return {};
		WORLDITEM const& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return {};
		identity.kind = DeferredContextKind::WORLD_ITEM;
		identity.worldItemIndex = itemIndex;
		identity.gridNo = worldItem.sGridNo;
		identity.level = worldItem.ubLevel;
		identity.tileIndex = tileIndex;
		identity.itemFingerprint = DeferredItemFingerprint(worldItem.o);
		identity.worldItemRevision = WorldItemMutationRevision();
		return identity;
	}

	SOLDIERTYPE* DeferredContextActor(DeferredContextIdentity const& identity)
	{
		if (identity.kind == DeferredContextKind::NONE ||
			identity.actorId == NOBODY || identity.actorInstanceId == 0)
			return nullptr;
		SOLDIERTYPE* const actor = ID2Soldier(identity.actorId);
		return actor && actor->bActive &&
			actor->uiUniqueSoldierIdValue == identity.actorInstanceId ?
			actor : nullptr;
	}

	BOOLEAN DeferredContextStillValid(DeferredContextIdentity const& identity)
	{
		if (identity.kind == DeferredContextKind::NONE) return TRUE;
		SOLDIERTYPE* const actor = DeferredContextActor(identity);
		if (!actor) return FALSE;
		if (identity.kind == DeferredContextKind::CHARACTER) return TRUE;
		if (identity.kind == DeferredContextKind::INVENTORY_ITEM)
		{
			return identity.inventorySlot >= 0 &&
				identity.inventorySlot < NUM_INV_SLOTS &&
				DeferredItemFingerprint(actor->inv[identity.inventorySlot]) ==
					identity.itemFingerprint;
		}
		if (identity.kind != DeferredContextKind::WORLD_ITEM ||
			identity.worldItemIndex < 0 ||
			static_cast<size_t>(identity.worldItemIndex) >= gWorldItems.size())
			return FALSE;
		WORLDITEM const& worldItem = GetWorldItem(identity.worldItemIndex);
		return identity.worldItemRevision != 0 &&
			identity.worldItemRevision == WorldItemMutationRevision() &&
			worldItem.fExists && worldItem.sGridNo == identity.gridNo &&
			worldItem.ubLevel == identity.level &&
			DeferredItemFingerprint(worldItem.o) == identity.itemFingerprint;
	}

	BOOLEAN TrackInventoryTransfer(SOLDIERTYPE* actor, INT8 slot)
	{
		if (!actor || !actor->bActive || slot < 0 || slot >= NUM_INV_SLOTS ||
			!gpItemPointer) return FALSE;
		OS0ItemTransferRuntime& transfer = OS0GetItemTransferRuntime();
		transfer.reset();
		if (gpItemPointerSoldier == actor && gbItemPointerSrcSlot == slot)
			return transfer.beginInventory(Soldier2ID(actor),
				actor->uiUniqueSoldierIdValue, slot);
		return transfer.bindAfterDetach(OS0ItemTransferOrigin::Inventory(
			Soldier2ID(actor), actor->uiUniqueSoldierIdValue, slot, {}));
	}

	BOOLEAN TrackSpatialTransfer(OS0ItemTransferOrigin const& origin)
	{
		if (!gpItemPointer) return FALSE;
		OS0ItemTransferRuntime& transfer = OS0GetItemTransferRuntime();
		transfer.reset();
		return transfer.bindAfterDetach(origin);
	}

	void FinishCommittedItemPointer()
	{
		// AddItemToPool copies the object without clearing the native cursor, while
		// PlaceObject normally empties it. Declare destination ownership explicitly
		// before the shared JA2 cleanup so a legitimate in-place item mutation can
		// never be mistaken for a slot swap by EndItemPointer's legacy fallback.
		OS0ItemTransferRuntime& transfer = OS0GetItemTransferRuntime();
		if (transfer.held()) transfer.commit();
		EndItemPointer();
		ClearHeldItemCarrier();
	}

	BOOLEAN RejectHeldItemRelation(SOLDIERTYPE* carrier, const char* reason)
	{
		if (!gpItemPointer) return TRUE;
		OS0ItemTransferRuntime& transfer = OS0GetItemTransferRuntime();
		const OS0ItemTransferCancelResult result = transfer.cancel();
		if (result == OS0ItemTransferCancelResult::RESTORED)
		{
			OS0NotifyWorldMutation();
			ClearHeldItemCarrier();
			ClearItemTransferTarget();
			gItemTransferMoreVisible = FALSE;
			if (gEquipmentAutoForHeldItem)
			{
				gEquipmentExplodedVisible = FALSE;
				BindEquipmentSoldier(nullptr);
				gEquipmentAutoForHeldItem = FALSE;
			}
			RecordFeedbackEvent(ST::format("{} / ITEM RETURNED", reason));
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (result == OS0ItemTransferCancelResult::NATIVE_ITEM_CHANGED)
			transfer.reset();
		if (!gpItemPointer) return TRUE;

		// An external/native cursor has no exact source to restore. Resolve the
		// rejected relation physically at its bound carrier instead of leaving an
		// apparently stuck item or teleporting it to the remote pointer position.
		if (!carrier || !carrier->bActive || carrier->bTeam != OUR_TEAM ||
			carrier->sSector != gWorldSector || carrier->sGridNo < 0 ||
			carrier->sGridNo >= WORLD_MAX)
		{
			carrier = GetSelectedMan();
			if (!carrier || !carrier->bActive || carrier->bTeam != OUR_TEAM ||
				carrier->sSector != gWorldSector || carrier->sGridNo < 0 ||
				carrier->sGridNo >= WORLD_MAX)
			{
				carrier = nullptr;
				FOR_EACH_MERC(i)
				{
					SOLDIERTYPE* const candidate = *i;
					if (!candidate || !candidate->bActive ||
						candidate->bTeam != OUR_TEAM ||
						candidate->sSector != gWorldSector ||
						candidate->sGridNo < 0 || candidate->sGridNo >= WORLD_MAX)
						continue;
					carrier = candidate;
					break;
				}
			}
		}
		if (carrier && carrier->bActive && carrier->sSector == gWorldSector &&
			carrier->sGridNo >= 0 && carrier->sGridNo < WORLD_MAX &&
			AddItemToPool(carrier->sGridNo, gpItemPointer, VISIBLE,
				carrier->bLevel, 0, -1) >= 0)
		{
			OS0NotifyWorldMutation();
			NotifySoldiersToLookforItems();
			FinishCommittedItemPointer();
			ClearItemTransferTarget();
			gItemTransferMoreVisible = FALSE;
			RecordFeedbackEvent(ST::format("{} / DROPPED AT CARRIER", reason));
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}

		RecordFeedbackEvent(ST::format("{} / ITEM KEPT / RECOVERY REQUIRED",
			reason));
		return TRUE;
	}

	OS0ItemTransferOrigin SpatialTransferOrigin(WORLDITEM const& worldItem,
		UINT16 containerTileIndex)
	{
		STRUCTURE const* const container =
			containerTileIndex < NUMBEROFTILES ?
			WorldStructureAt(worldItem.sGridNo, worldItem.ubLevel,
				containerTileIndex) : nullptr;
		const BOOLEAN belongsToContainer = container &&
			(container->fFlags & STRUCTURE_OPENABLE) &&
			!(container->fFlags & STRUCTURE_ANYDOOR);
		return belongsToContainer ?
			OS0ItemTransferOrigin::Container(worldItem.sGridNo,
				worldItem.ubLevel, containerTileIndex, worldItem.bVisible,
				worldItem.usFlags, worldItem.bRenderZHeightAboveLevel, {},
				container->usStructureID, StructureBaseGridNo(container)) :
			OS0ItemTransferOrigin::World(worldItem.sGridNo,
				worldItem.ubLevel, worldItem.bVisible, worldItem.usFlags,
				worldItem.bRenderZHeightAboveLevel);
	}

	BOOLEAN RestoreDetachedSpatialObject(OS0ItemTransferOrigin const& origin,
		OBJECTTYPE* object)
	{
		if (!object || object->usItem == NOTHING || origin.gridNo < 0 ||
			origin.gridNo >= WORLD_MAX || origin.level < 0 || origin.level > 1)
			return FALSE;
		const INT32 restored = AddItemToPool(origin.gridNo, object,
			static_cast<Visibility>(origin.bVisible),
			static_cast<UINT8>(origin.level), origin.usFlags,
			origin.bRenderZHeightAboveLevel);
		if (restored < 0) return FALSE;
		OS0NotifyWorldMutation();
		return TRUE;
	}

	BOOLEAN BeginTrackedWorldItemTransfer(SOLDIERTYPE* actor,
		INT32 const itemIndex, UINT16 const containerTileIndex,
		BOOLEAN const releaseAlreadyHandled)
	{
		if (!actor || gpItemPointer || itemIndex < 0 ||
			static_cast<size_t>(itemIndex) >= gWorldItems.size()) return FALSE;
		WORLDITEM& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return FALSE;
		if (!OS0PrepareWorldItemForDirectDetach(actor, itemIndex,
			worldItem.sGridNo, worldItem.ubLevel)) return FALSE;
		// Item and structure manipulation are exclusive ownership domains.  Never
		// move the container while a detached child still names it as its source.
		if (CarryState().active()) CancelWorldMoveState();

		OS0ItemTransferOrigin const origin =
			SpatialTransferOrigin(worldItem, containerTileIndex);
		OBJECTTYPE object = worldItem.o;
		RemoveItemFromPool(worldItem);
		OS0NotifyWorldMutation();
		InternalBeginItemPointer(actor, &object, NO_SLOT);
		if (!gpItemPointer)
		{
			RestoreDetachedSpatialObject(origin, &object);
			return FALSE;
		}
		if (!TrackSpatialTransfer(origin))
		{
			if (RestoreDetachedSpatialObject(origin, gpItemPointer))
			{
				EndItemPointer();
				ClearHeldItemCarrier();
			}
			else
			{
				OS0ItemTransferController& transfers =
					OS0GetItemTransferController();
				if (releaseAlreadyHandled)
					transfers.adoptExternalHeldItemAfterHandledRelease();
				else
					transfers.adoptExternalHeldItem();
				BindHeldItemCarrier(actor);
			}
			RecordFeedbackEvent(
				"ITEM MOVE ABORTED / SOURCE TRACKING FAILED");
			return FALSE;
		}

		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if (releaseAlreadyHandled)
			transfers.adoptExternalHeldItemAfterHandledRelease();
		else
			transfers.adoptExternalHeldItem();
		BindHeldItemCarrier(actor);
		ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		gEquipmentExplodedVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	BOOLEAN EquipWorldItemDirectly(SOLDIERTYPE* actor, INT32 const itemIndex,
		UINT16 const containerTileIndex, BOOLEAN const releaseAlreadyHandled)
	{
		if (!actor || gpItemPointer || itemIndex < 0 ||
			static_cast<size_t>(itemIndex) >= gWorldItems.size()) return FALSE;
		WORLDITEM& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return FALSE;
		if (!OS0PrepareWorldItemForDirectDetach(actor, itemIndex,
			worldItem.sGridNo, worldItem.ubLevel)) return FALSE;

		const INT8 targetSlot = OS0PreferredEquipmentSlot(actor, worldItem.o);
		if (targetSlot == NO_SLOT)
		{
			RecordFeedbackEvent("EQUIP BLOCKED / ITEM IS NOT EQUIPPABLE");
			return FALSE;
		}
		if (!OS0CanAcceptCarriedObject(actor, worldItem.o))
		{
			RecordFeedbackEvent("LOAD LIMIT 125% / ITEM LEFT IN WORLD");
			return FALSE;
		}
		if (!CanPlaceObjectCompletelyInActorSlot(actor, targetSlot, worldItem.o))
		{
			if (BeginTrackedWorldItemTransfer(actor, itemIndex, containerTileIndex,
				releaseAlreadyHandled))
				RecordFeedbackEvent("EQUIPMENT SLOT OCCUPIED / ITEM HELD");
			return FALSE;
		}
		const OS0ItemTransferOrigin origin =
			SpatialTransferOrigin(worldItem, containerTileIndex);
		const OBJECTTYPE originalObject = worldItem.o;
		const OBJECTTYPE originalTarget = actor->inv[targetSlot];
		OBJECTTYPE object = originalObject;
		RemoveItemFromPool(worldItem);
		OS0NotifyWorldMutation();
		PlaceObject(actor, targetSlot, &object);
		if (object.usItem == NOTHING || object.ubNumberOfObjects == 0) return TRUE;

		// The preflight promised a complete placement. If native inventory rules
		// nevertheless changed underneath us, undo the slot and restore the exact
		// spatial source. Never leave a half-equipped stack behind.
		actor->inv[targetSlot] = originalTarget;
		object = originalObject;
		if (RestoreDetachedSpatialObject(origin, &object))
		{
			RecordFeedbackEvent("EQUIP ABORTED / ITEM RESTORED");
			return FALSE;
		}

		// A failed world restore must still retain ownership. Keep the complete
		// object on the cursor with its original source transaction attached.
		InternalBeginItemPointer(actor, &object, NO_SLOT);
		if (gpItemPointer)
		{
			const BOOLEAN tracked = TrackSpatialTransfer(origin);
			OS0ItemTransferController& transfers = OS0GetItemTransferController();
			if (releaseAlreadyHandled)
				transfers.adoptExternalHeldItemAfterHandledRelease();
			else
				transfers.adoptExternalHeldItem();
			BindHeldItemCarrier(actor);
			RecordFeedbackEvent(tracked ?
				"EQUIP ABORTED / ITEM KEPT ON CURSOR" :
				"EQUIP ABORTED / UNTRACKED ITEM KEPT ON CURSOR");
		}
		else
		{
			RecordFeedbackEvent("EQUIP ABORTED / RECOVERY FAILED");
		}
		return FALSE;
	}

	LEVELNODE* WorldObjectLayerAssetAt(GridNo gridNo, UINT8 level,
		UINT16 tileIndex)
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX || level != 0 ||
			tileIndex >= NUMBEROFTILES) return nullptr;
		for (LEVELNODE* node = gpWorldLevelData[gridNo].pObjectHead;
			node; node = node->pNext)
		{
			if (node->usIndex == tileIndex &&
				IsOS0PersistentWorldAssetNode(node))
				return node;
		}
		return nullptr;
	}

	BOOLEAN WorldAssetExistsAt(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		return gridNo >= 0 && gridNo < WORLD_MAX &&
			tileIndex < NUMBEROFTILES &&
			(WorldStructureAt(gridNo, level, tileIndex) != nullptr ||
			 WorldObjectLayerAssetAt(gridNo, level, tileIndex) != nullptr);
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

	struct CanonicalContainerTarget
	{
		STRUCTURE const* structure = nullptr;
		GridNo gridNo = NOWHERE;
		UINT16 tileIndex = NO_TILE;
	};

	BOOLEAN ResolveCanonicalContainerTarget(GridNo const clickedGridNo,
		UINT8 const level, UINT16 const clickedTileIndex,
		CanonicalContainerTarget& target)
	{
		STRUCTURE const* const structure = WorldStructureAt(clickedGridNo, level,
			clickedTileIndex);
		if (!structure || !(structure->fFlags & STRUCTURE_OPENABLE) ||
			(structure->fFlags & STRUCTURE_ANYDOOR) ||
			structure->sGridNo < 0 || structure->sGridNo >= WORLD_MAX) return FALSE;
		target.structure = structure;
		target.gridNo = structure->sGridNo;
		target.tileIndex = CanonicalAssetTileIndex(clickedGridNo, level,
			clickedTileIndex);
		return target.tileIndex < NUMBEROFTILES;
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
						record.label = "LOOSE STONE DEPOSIT";
						break;
					case DEBRISWOOD:
						record.category = AssetCategory::DEBRIS;
						record.material = AssetMaterial::WOOD;
						record.role = AssetRole::SALVAGE;
						record.label = "WOOD DEBRIS";
						break;
					case DEBRISWEEDS:
					case DEBRISGRASS:
						record.category = AssetCategory::DEBRIS;
						record.material = AssetMaterial::ORGANIC;
						record.role = AssetRole::SALVAGE;
						record.label = "ORGANIC DEBRIS";
						break;
					case DEBRISSAND:
						record.category = AssetCategory::DEBRIS;
						record.material = AssetMaterial::SAND;
						record.role = AssetRole::SALVAGE;
						record.label = "LOOSE SAND / SOIL";
						break;
					case DEBRISMISC:
					case DEBRIS2MISC:
						record.category = AssetCategory::DEBRIS;
						record.material = AssetMaterial::METAL;
						record.role = AssetRole::SALVAGE;
						record.label = "BROKEN SCRAP DEBRIS";
						break;
					default: break;
				}
			}
			if (record.label == "UNNAMED ASSET")
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
		if (AssetCatalogRecord const* const existing = OS0FindAssetCatalogRecordConst(
			static_cast<INT16>(giCurrentTilesetID), catalogTile))
			gCatalogDraft = *existing;
		else
			gCatalogDraft = MakeDefaultCatalogRecord(gridNo, level, tileIndex);
		gAssetCatalogReturnToLibrary = gGodLibraryVisible;
		gUIRuntime.show(OS0UIPanel::ASSET_CATALOG);
		gUIRuntime.hide(OS0UIPanel::ASSET_LIBRARY);
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

	void StopOwnedCarryMovement()
	{
		OS0CarryState const& carry = CarryState();
		if (!carry.walking() && !carry.repositioning()) return;
		SOLDIERTYPE* const carrier = CarryCarrier();
		const GridNo ownedDestination = carry.repositioning() ?
			carry.followUpGrid : carry.actionGrid;
		if (carrier && carrier->bActive &&
			carrier->sFinalDestination == ownedDestination)
			StopSoldier(carrier);
	}

	void ClearWorldMoveState()
	{
		CarryState().reset();
	}

	void CancelWorldMoveState()
	{
		StopOwnedCarryMovement();
		ClearWorldMoveState();
		// Cancellation is a complete input-ownership hand-off. Leaving a CARRY/
		// PUSH/PULL cursor behind makes subsequent empty LMB clicks look consumed
		// even though no carry exists (notably after item pickup or combat start).
		CursorState().action = ContextAction::MOVE;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
		// The carried/source projections are composited after the world.  A hard
		// redraw is the ownership boundary that prevents their last pixels or a
		// suppressed source node surviving cancellation.
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	GridNo FindCarryActionGrid(SOLDIERTYPE* carrier, GridNo destination)
	{
		if (!carrier) return NOWHERE;
		constexpr std::array<WorldDirections, 8> directions{{
			NORTH, NORTHEAST, EAST, SOUTHEAST,
			SOUTH, SOUTHWEST, WEST, NORTHWEST
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
		// FinalizeWorldMove migrates every HIDDEN_IN_OBJECT entry plus the private
		// seed marker as one verified transaction. Initialized and non-empty crates
		// are therefore first-class movable assets, not a permanently blocked case.
		return carrier && gridNo >= 0 && gridNo < WORLD_MAX && level == 0 &&
			structure && structure->fFlags & STRUCTURE_BASE_TILE &&
			structure->pDBStructureRef &&
			structure->pDBStructureRef->pDBStructure->ubNumberOfTiles == 1 &&
			CanSoldierMoveWorldStructure(carrier, structure);
	}

	BOOLEAN IsInspectedWorldAssetManipulationNear()
	{
		SOLDIERTYPE const* const selected = GetSelectedMan();
		return selected && gInspectedGridNo >= 0 &&
			gInspectedGridNo < WORLD_MAX &&
			PythSpacesAway(selected->sGridNo, gInspectedGridNo) <= 1;
	}

	BOOLEAN IsInspectedWorldAssetMovable()
	{
		return IsWorldAssetMovableAt(gInspectedGridNo, gInspectedLevel,
			gInspectedTileIndex, GetSelectedMan());
	}

	ContextAction CarryModeAction(OS0CarryMode mode)
	{
		switch (mode)
		{
			case OS0CarryMode::GRAB: return ContextAction::CARRY;
			case OS0CarryMode::PUSH: return ContextAction::PUSH;
			case OS0CarryMode::PULL: return ContextAction::PULL;
			case OS0CarryMode::THROW: return ContextAction::THROW;
			case OS0CarryMode::CARRY: return ContextAction::CARRY;
		}
		return ContextAction::CARRY;
	}

	OS0CarryMode CarryModeForAction(ContextAction action)
	{
		switch (action)
		{
			case ContextAction::CARRY:
			case ContextAction::PUSH:
			case ContextAction::PULL:
				return OS0CarryMode::GRAB;
			case ContextAction::THROW: return OS0CarryMode::THROW;
			default: return OS0CarryMode::GRAB;
		}
	}

	const char* CarryModeName(OS0CarryMode mode)
	{
		if (mode == OS0CarryMode::GRAB) return "GRAB";
		return ContextActionName(CarryModeAction(mode));
	}

	BOOLEAN CancelPendingWorldAction(const char* reason,
		BOOLEAN stopOwnedMovement);

	BOOLEAN BeginWorldMoveAt(GridNo gridNo, UINT8 level, UINT16 tileIndex,
		OS0CarryMode mode = OS0CarryMode::GRAB, SOLDIERTYPE* carrier = nullptr,
		BOOLEAN pointerDrag = FALSE)
	{
		if (!carrier) carrier = GetSelectedMan();
		if (!carrier || carrier != GetSelectedMan() || !carrier->bActive ||
			carrier->bTeam != OUR_TEAM || !OK_CONTROLLABLE_MERC(carrier) ||
			carrier->bLife < OKLIFE || gpItemPointer ||
			(gTacticalStatus.uiFlags & INCOMBAT) ||
			level != 0 || carrier->bLevel != 0 ||
			gridNo < 0 || gridNo >= WORLD_MAX ||
			PythSpacesAway(carrier->sGridNo, gridNo) > 1 ||
			tileIndex >= NUMBEROFTILES ||
			!IsWorldAssetMovableAt(gridNo, level, tileIndex, carrier))
			return FALSE;
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level,
			tileIndex);
		const GridNo structureBaseGridNo = StructureBaseGridNo(structure);
		if (!structure || structureBaseGridNo < 0 ||
			structureBaseGridNo >= WORLD_MAX ||
			carrier->uiUniqueSoldierIdValue == 0) return FALSE;
		// Deferred actions and physical manipulation must never own the same actor
		// route. Release the old approach before seeding or binding the carry so it
		// cannot execute against the arrival commissioned by this new gesture.
		CancelPendingWorldAction("CARRY STARTED", TRUE);
		if ((structure->fFlags & STRUCTURE_OPENABLE) &&
			!(structure->fFlags & STRUCTURE_ANYDOOR))
		{
			// Materialize the position-derived seed marker before identity moves.
			// Opening before or after carrying must reveal the same contents.
			EnsureContainerLoot(gridNo, level, tileIndex);
		}
		CancelWorldMoveState();
		OS0CarryState& carry = CarryState();
		if (!carry.begin(gridNo, level, tileIndex, Soldier2ID(carrier),
			carrier->uiUniqueSoldierIdValue, structure->usStructureID,
			structureBaseGridNo, mode, pointerDrag))
			return FALSE;
		CursorState().action = CarryModeAction(mode);
		BindCarryShadowInstance();
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		carry.lifted = physics.massKg <=
			GetSoldierWorldCarryCapacityKg(carrier) * 0.55f;
		gLootVisible = FALSE;
		guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
		return TRUE;
	}

	BOOLEAN BeginInspectedWorldMove(OS0CarryMode mode = OS0CarryMode::GRAB)
	{
		if (!IsInspectedWorldAssetManipulationNear() ||
			!IsInspectedWorldAssetMovable())
			return FALSE;
		return BeginWorldMoveAt(gInspectedGridNo, gInspectedLevel,
			gInspectedTileIndex, mode);
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
		if ((gTutorialActive && gBagVisible && overlaps(gBagX, gBagY, PANE_W, BAG_H)) ||
			overlaps(gOrbRegion.RegionTopLeftX, gOrbRegion.RegionTopLeftY,
				gOrbRegion.W(), gOrbRegion.H()) ||
			(gContextVisible && overlaps(gContextX, gContextY, 168,
				static_cast<INT16>(20 + gContextEntryCount * 18)))) return;
		// The tactical save buffer contains the clean rendered world before OS//0
		// affordances and movable windows. Capturing FRAME_BUFFER recursively baked
		// DIG icons into the preview every time the nearby target changed.
		BltVideoSurface(gInspectorPreview, guiSAVEBUFFER, 0, 0, &source);
	}

	void CaptureAnimatedMercPreview(SOLDIERTYPE const* soldier)
	{
		constexpr UINT16 width = 48;
		constexpr UINT16 height = 62;
		if (!soldier) return;
		if (!gAnimatedMercPreview)
		{
			gAnimatedMercPreview = AddVideoSurface(width, height, PIXEL_DEPTH);
			gAnimatedMercPreview->Fill(Get16BPPColor(FROMRGB(4, 7, 7)));
		}
		INT16 anchorX;
		INT16 anchorY;
		if (!GetActorDisplayAnchor(soldier, anchorX, anchorY)) return;
		const INT16 sourceX = std::clamp<INT16>(anchorX - width / 2, 0,
			std::max<INT16>(0, SCREEN_WIDTH - width));
		const INT16 sourceY = std::clamp<INT16>(anchorY - height + 7, 0,
			std::max<INT16>(0, SCREEN_HEIGHT - height));
		if (sourceX < gsVIEWPORT_START_X || sourceX + width > gsVIEWPORT_END_X ||
			sourceY < gsVIEWPORT_WINDOW_START_Y ||
			sourceY + height > gsVIEWPORT_WINDOW_END_Y) return;
		// Keep the last clean tactical frame if the movable window currently covers
		// the actor; otherwise the panel would recursively capture itself.
		if (gBagVisible && sourceX < gBagX + PANE_W && sourceX + width > gBagX &&
			sourceY < gBagY + BAG_H && sourceY + height > gBagY) return;
		const SGPBox source{
			static_cast<UINT16>(sourceX), static_cast<UINT16>(sourceY), width, height
		};
		BltVideoSurface(gAnimatedMercPreview, FRAME_BUFFER, 0, 0, &source);
		gAnimatedMercPreviewSoldier = soldier;
	}

	void DrawWorldItemSprite(OBJECTTYPE const& object, INT16 centreX,
		INT16 centreY)
	{
		if (object.usItem == NOTHING) return;
		const UINT16 tileIndex = GetTileGraphicForItem(GCM->getItem(object.usItem));
		if (tileIndex >= NUMBEROFTILES) return;
		TILE_ELEMENT const& tile = gTileDatabase[tileIndex];
		if (!tile.hTileSurface) return;
		ETRLEObject const& frame =
			tile.hTileSurface->SubregionProperties(tile.usRegionIndex);
		const INT16 x = static_cast<INT16>(centreX - frame.usWidth / 2 -
			frame.sOffsetX);
		const INT16 y = static_cast<INT16>(centreY - frame.usHeight / 2 -
			frame.sOffsetY);
		BltVideoObject(FRAME_BUFFER, tile.hTileSurface, tile.usRegionIndex, x, y);
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

	BOOLEAN FieldTutorialTargetMatches(GridNo const gridNo, UINT8 const level,
		UINT16 const tileIndex)
	{
		if (!gFieldTutorial.active() || level != gFieldTutorialLevel)
			return FALSE;
		if (gridNo == gFieldTutorialGridNo) return TRUE;
		// Large containers occupy more than one grid. Pointer resolution reports
		// the clicked child tile, while the tutorial marker owns the base tile.
		// Compare their canonical base structure so every visible part reacts.
		STRUCTURE const* const structure =
			WorldStructureAt(gridNo, level, tileIndex);
		STRUCTURE* const base = structure ?
			FindBaseStructure(const_cast<STRUCTURE*>(structure)) : nullptr;
		return base && base->sGridNo == gFieldTutorialGridNo;
	}

	BOOLEAN FieldTutorialTargetMatches(OS0ActionBinding const& binding)
	{
		return binding.kind != OS0InteractionTargetKind::NONE &&
			FieldTutorialTargetMatches(binding.gridNo, binding.level,
				binding.tileIndex);
	}

	void NotifyFieldTutorial(OS0FieldTutorialEvent const event)
	{
		if (!gFieldTutorial.active()) return;
		if (!gFieldTutorial.notify(event)) return;
		RecordFeedbackEvent(ST::format("FIELD TUTORIAL / {}",
			gFieldTutorial.heading()));
		if (gFieldTutorial.completed())
		{
			OS0GetTacticalSession().state().fieldTutorialCompleted = TRUE;
			gFieldTutorialCompletedAt = GetJA2Clock();
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void NotifyFieldTutorial(OS0FieldTutorialEvent const event,
		OS0ActionBinding const& binding)
	{
		if (FieldTutorialTargetMatches(binding)) NotifyFieldTutorial(event);
	}

	size_t CountFieldTutorialLoot()
	{
		if (gFieldTutorialGridNo < 0 ||
			gFieldTutorialGridNo >= WORLD_MAX) return 0;
		size_t count = 0;
		for (ITEM_POOL* item = GetItemPool(gFieldTutorialGridNo,
			gFieldTutorialLevel); item; item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size())
				continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (!OS0IsContainerContentItem(worldItem)) continue;
			const BOOLEAN marker = OS0IsContainerSeedMarker(worldItem);
			if (!marker) ++count;
		}
		return count;
	}

	BOOLEAN FieldTutorialTargetValid()
	{
		if (gFieldTutorialGridNo < 0 ||
			gFieldTutorialGridNo >= WORLD_MAX) return FALSE;
		LEVELNODE* node = gFieldTutorialLevel == 0 ?
			gpWorldLevelData[gFieldTutorialGridNo].pStructHead :
			gpWorldLevelData[gFieldTutorialGridNo].pOnRoofHead;
		for (; node; node = node->pNext)
		{
			STRUCTURE const* const structure = node->pStructureData;
			if (!structure || node->usIndex >= NUMBEROFTILES ||
				!(structure->fFlags & STRUCTURE_OPENABLE) ||
				(structure->fFlags & STRUCTURE_ANYDOOR))
				continue;
			// Opening an interactive structure can replace its visual partner.
			// The crate remains the same world object at the same grid, so update
			// its current presentation tile instead of declaring the target lost.
			gFieldTutorialTileIndex = node->usIndex;
			return TRUE;
		}
		return FALSE;
	}

	BOOLEAN FindFieldTutorialContainer(SOLDIERTYPE* actor)
	{
		if (!actor || actor->sGridNo < 0 || actor->sGridNo >= WORLD_MAX)
			return FALSE;
		GridNo bestGrid = NOWHERE;
		UINT16 bestTile = NO_TILE;
		INT16 bestDistance = 32767;
		for (GridNo gridNo = 0; gridNo < WORLD_MAX; ++gridNo)
		{
			for (LEVELNODE* node = gpWorldLevelData[gridNo].pStructHead;
				node; node = node->pNext)
			{
				STRUCTURE* const structure = node->pStructureData;
				if (!structure || node->usIndex >= NUMBEROFTILES)
					continue;
				STRUCTURE* const base = FindBaseStructure(structure);
				if (!base || !(base->fFlags & STRUCTURE_OPENABLE) ||
					(base->fFlags & STRUCTURE_ANYDOOR) ||
					base->sGridNo < 0 || base->sGridNo >= WORLD_MAX)
					continue;
				// Several stock maps do not set STRUCTURE_BASE_TILE consistently on
				// containers. Canonicalize every child to the engine's real base
				// structure instead of making that optional editor flag a requirement.
				const GridNo candidateGrid = base->sGridNo;
				UINT16 candidateTile = node->usIndex;
				for (LEVELNODE* baseNode =
					gpWorldLevelData[candidateGrid].pStructHead;
					baseNode; baseNode = baseNode->pNext)
				{
					if (!baseNode->pStructureData ||
						FindBaseStructure(baseNode->pStructureData) != base)
						continue;
					candidateTile = baseNode->usIndex;
					if (baseNode->pStructureData == base ||
						(baseNode->pStructureData->fFlags & STRUCTURE_BASE_TILE))
						break;
				}
				const INT16 distance =
					PythSpacesAway(actor->sGridNo, candidateGrid);
				if (distance >= bestDistance) continue;
				// The highlighted crate must be one the exact approach resolver can
				// reach. Choosing the nearest sealed/unreachable prop creates an
				// unwinnable tutorial despite a usable crate elsewhere in the sector.
				if (FindCarryActionGrid(actor, candidateGrid) == NOWHERE) continue;
				bestDistance = distance;
				bestGrid = candidateGrid;
				bestTile = candidateTile;
			}
		}
		if (bestGrid == NOWHERE) return FALSE;
		gFieldTutorialGridNo = bestGrid;
		gFieldTutorialLevel = 0;
		gFieldTutorialTileIndex = bestTile;
		gFieldTutorialInitialLootCount = 0;
		NotifyFieldTutorial(OS0FieldTutorialEvent::CONTAINER_ASSIGNED);
		INT16 screenX;
		INT16 screenY;
		GetGridNoScreenPos(bestGrid, 0, &screenX, &screenY);
		OS0MapWorldToDisplayScreen(&screenX, &screenY);
		if (screenX < gsVIEWPORT_START_X || screenX >= gsVIEWPORT_END_X ||
			screenY < gsVIEWPORT_WINDOW_START_Y ||
			screenY >= OS0WorldViewportBottom())
			SlideToLocation(bestGrid);
		RecordFeedbackEvent(ST::format(
			"FIELD TUTORIAL / CRATE GRID {} TILE {}", bestGrid, bestTile));
		return TRUE;
	}

	void ResetFieldTutorialTarget()
	{
		gFieldTutorialGridNo = NOWHERE;
		gFieldTutorialLevel = 0;
		gFieldTutorialTileIndex = NO_TILE;
		gFieldTutorialInitialLootCount = 0;
		gFieldTutorialNextSearchAt = 0;
	}

	void UpdateFieldTutorial()
	{
		OS0TacticalState& state = OS0GetTacticalSession().state();
		if (gTutorialActive) return;
		if (!gFieldTutorial.active())
		{
			if (state.fieldTutorialCompleted) return;
			gFieldTutorial.notify(OS0FieldTutorialEvent::BEGIN);
		}
		if (gFieldTutorial.completed())
		{
			if (gFieldTutorialCompletedAt != 0 &&
				GetJA2Clock() - gFieldTutorialCompletedAt > 7000)
				gFieldTutorial.notify(OS0FieldTutorialEvent::DISMISS);
			return;
		}

		if (gFieldTutorialGridNo != NOWHERE && !FieldTutorialTargetValid())
		{
			ResetFieldTutorialTarget();
			NotifyFieldTutorial(OS0FieldTutorialEvent::TARGET_LOST);
		}
		if (gFieldTutorialGridNo == NOWHERE)
		{
			const UINT32 now = GetJA2Clock();
			if (now < gFieldTutorialNextSearchAt) return;
			gFieldTutorialNextSearchAt = now + 1000;
			FindFieldTutorialContainer(GetSelectedMan());
			return;
		}
		if (gFieldTutorial.stage() == OS0FieldTutorialStage::HOVER_CONTAINER &&
			gHoverCursorSoldier == nullptr &&
			FieldTutorialTargetMatches(gHoverCursorGridNo, gHoverCursorLevel,
				gHoverCursorTileIndex))
			NotifyFieldTutorial(OS0FieldTutorialEvent::CONTAINER_HOVERED);

		if (gFieldTutorial.stage() == OS0FieldTutorialStage::LOOT_CONTAINER)
		{
			const size_t current = CountFieldTutorialLoot();
			if (gFieldTutorialInitialLootCount > 0 &&
				current < gFieldTutorialInitialLootCount && !gpItemPointer)
				NotifyFieldTutorial(OS0FieldTutorialEvent::ITEM_TAKEN);
		}
	}

	CharacterHubCategoryDescriptor const& CharacterHubDescriptor(
		CharacterHubCategory category)
	{
		const size_t index = static_cast<size_t>(category);
		return CHARACTER_HUB_CATEGORIES[index < CHARACTER_HUB_CATEGORIES.size() ?
			index : 0];
	}

	void SetContextHubModal(BOOLEAN active)
	{
		if (gContextModalSuspended == active) return;
		// A radial owns keyboard focus as well as pointer focus.  In particular,
		// do not leave the feedback editor's global keyboard hook alive behind a
		// character or object hub.
		if (active) StopFeedbackEditing();
		OS0WindowManager& windows = gUIRuntime.windowManager();
		windows.setSuspended(OS0WindowSuspendReason::MODAL, active);
		// The context fan is the modal owner, never one of its suspended clients.
		windows.setSuspended(gUIRuntime.managedId(OS0UIPanel::CONTEXT),
			OS0WindowSuspendReason::MODAL, FALSE);
		gContextModalSuspended = active;
	}

	void CloseContextMenu()
	{
		gUIRuntime.hide(OS0UIPanel::CONTEXT);
		gObjectActionFanVisible = FALSE;
		gCharacterActionFanVisible = FALSE;
		gHoverVisible = FALSE;
		gContextEntryCount = 0;
		gContextInventorySlot = NO_SLOT;
		gContextWorldItemIndex = -1;
		gDeferredContextIdentity = {};
		// Closing a radial also clears its hover projection. Force one fresh
		// projection so a stationary pointer immediately rediscovers the object.
		OS0InvalidateWorldHoverProjection();
		gContextBlock.Disable();
		for (MOUSE_REGION& r : gContextRegions) r.Disable();
		SetContextHubModal(FALSE);
		// Re-enable the exact persistent windows which were only suspended by the
		// radial.  Centralizing this closes early-return holes in zoom/selection.
		if (gInitialized) SetBagRegionsEnabled(TRUE);
	}

	void ResetWorldBoundUIState()
	{
		// A conversation temporarily hides the character window.  World teardown
		// must first restore the user's choice; otherwise the next sector inherits
		// a hidden bag and the next talking panel reuses an obsolete snapshot.
		if (gTalkDocked) gBagVisible = gBagVisibleBeforeTalk;
		gTalkDocked = FALSE;
		gBagVisibleBeforeTalk = gBagVisible;

		BindInspectedSoldier(nullptr);
		BindInventorySoldier(nullptr);
		gInspectedGridNo = NOWHERE;
		gInspectedLevel = 0;
		gInspectedTileIndex = NO_TILE;
		gContextSoldier = nullptr;
		gContextGridNo = NOWHERE;
		gContextLevel = 0;
		gContextTileIndex = NO_TILE;
		gContextInventorySlot = NO_SLOT;
		gContextWorldItemIndex = -1;
		gDeferredContextIdentity = {};

		gLootGridNo = NOWHERE;
		gLootLevel = 0;
		gLootTileIndex = NO_TILE;
		BindLootActor(nullptr);
		gLootWorldItems.fill(-1);
		gLootVisible = FALSE;
		gLootIgnoreInputUntil = 0;

		gEquipmentExplodedVisible = FALSE;
		gEquipmentAutoForHeldItem = FALSE;
		BindEquipmentSoldier(nullptr);
		gEquipmentCentreX = 0;
		gEquipmentCentreY = 0;
		gStackSplitVisible = FALSE;
		gStackSplitSoldier = nullptr;
		gStackSplitSlot = NO_SLOT;
		gStackSplitIdentity = {};
		gStackSplitAmount = 1;

		gEnvironmentGridNo = NOWHERE;
		gEnvironmentLevel = 0;
		gEnvironmentTileIndex = NO_TILE;
		gEnvironmentActorGridNo = NOWHERE;
		gNextEnvironmentRefreshAt = 0;
		gEnvironmentTitle = "NO OBJECT SELECTED";
		gEnvironmentEntries = {};
		gEnvironmentEntryCount = 0;

		gNearbyHints = {};
		gNearbyHintHelp = {};
		gNearbyHintCount = 0;
		gNearbyScanWasEnabled = FALSE;
		ResetNearbyScanCache();
		InteractionMode().setNearbyScanEnabled(false);

		gHoverVisible = FALSE;
		gInspectorPinned = FALSE;
		gHoverCursorSoldier = nullptr;
		gHoverCursorSoldierInstanceId = 0;
		gHoverCursorGridNo = NOWHERE;
		gHoverCursorLevel = 0;
		gHoverCursorTileIndex = NO_TILE;
		gHoverCursorWorldItemIndex = -1;
		gHoverCursorHeldItem = NOTHING;
		gHoverSuggestedAction = ContextAction::COUNT;
		gHoverActionBinding = {};
		gHoverActionCycleIndex = 0;
		gHoverActionExplicit = FALSE;
		gHoverTitle.clear();
		gHoverDetail.clear();
		gHoverDebugDetail.clear();
		gHoverQuickActionHelp.clear();
		gAnimatedMercPreviewSoldier = nullptr;
		gPanelInteractionGuardUntil = 0;
		gWorldProjectionStampValid = FALSE;
		OS0InvalidateWorldHoverProjection();
	}

	void AddContextEntry(ContextAction action, const ST::string& label,
		BOOLEAN enabled = TRUE, OS0ActionBinding const& binding = {},
		OS0ActionApproach approach = OS0ActionApproach::IMMEDIATE,
		OS0ActionBlockReason blockReason = OS0ActionBlockReason::NONE)
	{
		if (gContextEntryCount >= gContextEntries.size()) return;
		ContextEntry entry{ action, label, enabled };
		entry.binding = binding;
		entry.deferredIdentity = gDeferredContextIdentity;
		entry.approach = approach;
		entry.blockReason = blockReason;
		size_t insertAt = gContextEntryCount;
		while (insertAt > 0 &&
			static_cast<UINT8>(ContextActionCategory(
				gContextEntries[insertAt - 1].action)) >
			static_cast<UINT8>(ContextActionCategory(action)))
		{
			gContextEntries[insertAt] = gContextEntries[insertAt - 1];
			--insertAt;
		}
		gContextEntries[insertAt] = entry;
		++gContextEntryCount;
	}

	void AddContextCategoryEntry(CharacterHubCategory category,
		const ST::string& label)
	{
		if (gContextEntryCount >= gContextEntries.size()) return;
		ContextEntry& entry = gContextEntries[gContextEntryCount++];
		entry = {};
		entry.kind = ContextEntryKind::CATEGORY;
		entry.category = category;
		entry.deferredIdentity = gDeferredContextIdentity;
		entry.label = label;
		entry.enabled = TRUE;
	}

	void AddContextBackEntry()
	{
		if (gContextEntryCount >= gContextEntries.size()) return;
		ContextEntry& entry = gContextEntries[gContextEntryCount++];
		entry = {};
		entry.kind = ContextEntryKind::BACK;
		entry.deferredIdentity = gDeferredContextIdentity;
		entry.label = "BACK";
		entry.enabled = TRUE;
	}

	OS0FixedList<CharacterActionSpec, 21> BuildCharacterActions(
		SOLDIERTYPE* soldier)
	{
		OS0FixedList<CharacterActionSpec, 21> actions;
		if (!soldier || soldier->bTeam != OUR_TEAM) return actions;
		auto add = [&](ContextAction action, CharacterHubCategory category,
			const ST::string& label, BOOLEAN enabled = TRUE)
		{
			actions.push_back({ action, category, label, enabled });
		};

		const BOOLEAN conscious = soldier->bActive && soldier->bLife >= OKLIFE;
		add(ContextAction::TAKE_COVER, CharacterHubCategory::ACTIONS,
			"RUN TO COVER", conscious);
		add(ContextAction::STAND, CharacterHubCategory::ACTIONS,
			"STANCE / STAND", conscious && IsValidStance(soldier, ANIM_STAND));
		add(ContextAction::CROUCH, CharacterHubCategory::ACTIONS,
			"STANCE / CROUCH", conscious && IsValidStance(soldier, ANIM_CROUCH));
		add(ContextAction::PRONE, CharacterHubCategory::ACTIONS,
			"STANCE / PRONE", conscious && IsValidStance(soldier, ANIM_PRONE));
		add(ContextAction::STEALTH, CharacterHubCategory::ACTIONS,
			soldier->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON", conscious);
		add(ContextAction::AUTO_FIRST_AID, CharacterHubCategory::ACTIONS,
			"AUTO FIRST AID", CanAutoBandage(FALSE));

		// Traits are passive JA2 state. The character sheet is the one honest
		// abilities surface until an active-ability system exists.
		add(ContextAction::INSPECT, CharacterHubCategory::ABILITIES,
			"CHARACTER SHEET / TALENTS");

		add(ContextAction::CONTENTS, CharacterHubCategory::EQUIPMENT,
			"INVENTORY / EQUIPMENT", CanAccessSoldierContents(soldier));
		const OBJECTTYPE& hand = soldier->inv[HANDPOS];
		const OBJECTTYPE& offHand = soldier->inv[SECONDHANDPOS];
		const BOOLEAN gunReady = hand.usItem != NOTHING &&
			GCM->getItem(hand.usItem)->getItemClass() == IC_GUN;
		if (gunReady)
		{
			const char* const mode = soldier->bWeaponMode == WM_BURST ? "BURST" :
				soldier->bWeaponMode == WM_ATTACHED ? "ATTACHED" : "SINGLE";
			add(ContextAction::WEAPON_MODE, CharacterHubCategory::EQUIPMENT,
				ST::format("WEAPON MODE / {}", mode));
			add(ContextAction::RELOAD, CharacterHubCategory::EQUIPMENT,
				"RELOAD WEAPON");
			add(ContextAction::UNLOAD, CharacterHubCategory::EQUIPMENT,
				ST::format("UNLOAD MAGAZINE / {}", hand.ubGunShotsLeft),
				hand.ubGunShotsLeft > 0);
		}
		if (hand.usItem != NOTHING || offHand.usItem != NOTHING)
			add(ContextAction::SWAP_HANDS, CharacterHubCategory::EQUIPMENT,
				"SWAP HANDS");

		add(ContextAction::PREVIOUS_SQUAD, CharacterHubCategory::GROUP,
			"PREVIOUS SQUAD");
		add(ContextAction::NEXT_SQUAD, CharacterHubCategory::GROUP,
			"NEXT SQUAD");
		add(ContextAction::TEAM, CharacterHubCategory::GROUP, "TEAM / SQUADS");
		add(ContextAction::END_TURN, CharacterHubCategory::GROUP, "END TURN",
			(gTacticalStatus.uiFlags & INCOMBAT) != 0);

		add(ContextAction::GOD_ASSETS, CharacterHubCategory::GOD,
			"ASSET LIBRARY");
		add(ContextAction::GOD_EDITOR, CharacterHubCategory::GOD,
			"LIVE WORLD EDITOR");
		add(ContextAction::GOD_ICONS, CharacterHubCategory::GOD,
			"ICON LIBRARY");
		add(ContextAction::GOD_TOOLS, CharacterHubCategory::GOD,
			"GIVE FIELD TOOLS");
		const BOOLEAN needsGodRestore = soldier->bActive &&
			((soldier->uiStatusFlags & SOLDIER_DEAD) ||
			 soldier->bLife < soldier->bLifeMax || soldier->bBleeding > 0 ||
			 soldier->bBreath < 100 || soldier->bCollapsed ||
			 soldier->bBreathCollapsed || soldier->fMercCollapsedFlag);
		add(ContextAction::GOD_REVIVE, CharacterHubCategory::GOD,
			"RESTORE OPERATOR", needsGodRestore);
		return actions;
	}

	void BuildCharacterContextPage(SOLDIERTYPE* soldier,
		CharacterHubCategory category = CharacterHubCategory::COUNT)
	{
		gContextEntryCount = 0;
		const OS0FixedList<CharacterActionSpec, 21> actions =
			BuildCharacterActions(soldier);
		if (category == CharacterHubCategory::COUNT)
		{
			for (CharacterHubCategoryDescriptor const& descriptor :
				CHARACTER_HUB_CATEGORIES)
			{
				const BOOLEAN populated = std::any_of(actions.begin(), actions.end(),
					[&](CharacterActionSpec const& action)
					{ return action.category == descriptor.category; });
				if (!populated) continue;
				const ST::string label = descriptor.category ==
					CharacterHubCategory::ACTIONS ?
					ST::format("{} / {}", descriptor.label,
						OS0InteractionStateName(InteractionMode().state())) :
					ST::string(descriptor.label);
				AddContextCategoryEntry(descriptor.category, label);
			}
			gContextTitle = ST::format("{} / CHARACTER / STATE {}", soldier->name,
				OS0InteractionStateName(InteractionMode().state()));
		}
		else
		{
			for (CharacterActionSpec const& spec : actions)
				if (spec.category == category)
					AddContextEntry(spec.action, spec.label, spec.enabled);
			AddContextBackEntry();
			gContextTitle = ST::format("{} / {}", soldier->name,
				CharacterHubDescriptor(category).label);
		}
	}

	UINT16 ActionCategoryColour(ActionCategory category)
	{
		switch (category)
		{
			case ActionCategory::INFO: return Get16BPPColor(FROMRGB(150, 150, 136));
			case ActionCategory::GEAR: return Get16BPPColor(FROMRGB(184, 126, 32));
			case ActionCategory::MOVEMENT: return Get16BPPColor(FROMRGB(68, 142, 76));
			case ActionCategory::STANCE: return Get16BPPColor(FROMRGB(102, 118, 62));
			case ActionCategory::COMBAT: return Get16BPPColor(FROMRGB(205, 12, 12));
			case ActionCategory::MEDICAL: return Get16BPPColor(FROMRGB(194, 194, 194));
			case ActionCategory::SOCIAL: return Get16BPPColor(FROMRGB(70, 116, 164));
			case ActionCategory::WORLD: return Get16BPPColor(FROMRGB(126, 82, 42));
			case ActionCategory::GROUP: return Get16BPPColor(FROMRGB(82, 116, 132));
			case ActionCategory::DEBUG: return Get16BPPColor(FROMRGB(156, 38, 156));
			case ActionCategory::COUNT: break;
		}
		return Get16BPPColor(FROMRGB(118, 0, 0));
	}

	ActionCategory ContextEntryAccent(ContextEntry const& entry)
	{
		if (entry.kind == ContextEntryKind::CATEGORY)
			return CharacterHubDescriptor(entry.category).accent;
		if (entry.kind == ContextEntryKind::BACK) return ActionCategory::INFO;
		return ContextActionCategory(entry.action);
	}

	OS0UIIcon ContextEntryIcon(ContextEntry const& entry)
	{
		if (entry.kind == ContextEntryKind::CATEGORY)
			return CharacterHubDescriptor(entry.category).icon;
		if (entry.kind == ContextEntryKind::BACK) return OS0UIIcon::CANCEL;
		return ContextActionIcon(entry.action);
	}

	const char* ContextEntryGroupName(ContextEntry const& entry)
	{
		if (entry.kind == ContextEntryKind::CATEGORY)
			return CharacterHubDescriptor(entry.category).label;
		if (entry.kind == ContextEntryKind::BACK) return "NAVIGATION";
		return ActionCategoryName(ContextActionCategory(entry.action));
	}

	const char* ContextEntryExplanation(ContextEntry const& entry)
	{
		if (entry.kind == ContextEntryKind::CATEGORY)
			return CharacterHubDescriptor(entry.category).explanation;
		if (entry.kind == ContextEntryKind::BACK)
			return "Return to the character action categories.";
		return ContextActionExplanation(entry.action);
	}

	void DrawContextEntryIcon(ContextEntry const& entry, INT16 x, INT16 y)
	{
		if (entry.kind == ContextEntryKind::ACTION)
		{
			DrawContextActionIcon(entry.action, x, y);
			return;
		}
		OS0UIAssets().draw(ContextEntryIcon(entry), FRAME_BUFFER, x, y);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + 21, x + 20, y + 22,
			ActionCategoryColour(ContextEntryAccent(entry)));
	}

	void DrawContextActionIcon(ContextAction action, INT16 x, INT16 y)
	{
		OS0UIAssets().draw(ContextActionIcon(action), FRAME_BUFFER, x, y);

		const UINT16 category = ActionCategoryColour(ContextActionCategory(action));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + 21, x + 20, y + 22, category);
		if (action == ContextAction::AUTO_FIRST_AID)
		{
			ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 9, y + 5,
				x + 12, y + 18, category);
			ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 4, y + 10,
				x + 17, y + 13, category);
		}
		else if (action == ContextAction::PUSH || action == ContextAction::PULL ||
			action == ContextAction::THROW)
		{
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_WHITE);
			MPrint(x + 7, y + 7, action == ContextAction::PUSH ? ">" :
				action == ContextAction::PULL ? "<" : "^");
		}
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
		if (AssetCatalogRecord const* const custom = OS0FindAssetCatalogRecordConst(
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

	AssetCatalogRecord ResolveAssetRecord(GridNo gridNo, UINT8 level,
		UINT16 tileIndex, BOOLEAN* catalogued = nullptr)
	{
		const UINT16 canonical = CanonicalAssetTileIndex(gridNo, level, tileIndex);
		if (AssetCatalogRecord const* const record = OS0FindAssetCatalogRecordConst(
			static_cast<INT16>(giCurrentTilesetID), canonical))
		{
			if (catalogued) *catalogued = TRUE;
			return *record;
		}
		if (catalogued) *catalogued = FALSE;
		return MakeDefaultCatalogRecord(gridNo, level, tileIndex);
	}

	AssetMaterial ResolveAssetMaterial(GridNo gridNo, UINT8 level,
		UINT16 tileIndex, AssetCatalogRecord const& record)
	{
		if (record.material != AssetMaterial::AUTO) return record.material;
		return InferAssetMaterial(WorldStructureAt(gridNo, level, tileIndex));
	}

	FieldToolKind RequiredFieldTool(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		AssetCatalogRecord const record = ResolveAssetRecord(gridNo, level, tileIndex);
		AssetMaterial const material = ResolveAssetMaterial(gridNo, level,
			tileIndex, record);
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		if (record.category == AssetCategory::SANDBAG ||
			material == AssetMaterial::SAND || material == AssetMaterial::EARTH ||
			material == AssetMaterial::ORGANIC)
			return FieldToolKind::FIELD_SHOVEL;
		if (record.category == AssetCategory::TREE)
			return FieldToolKind::CUTTING_TOOL;
		if ((structure && structure->fFlags & STRUCTURE_ANYFENCE) ||
			(record.role == AssetRole::BARRIER && material == AssetMaterial::METAL))
			return FieldToolKind::WIRE_CUTTERS;
		if (material == AssetMaterial::METAL ||
			material == AssetMaterial::COMPOSITE)
			return FieldToolKind::TOOLKIT;
		if (material == AssetMaterial::WOOD || material == AssetMaterial::STONE ||
			record.category == AssetCategory::DOOR ||
			record.category == AssetCategory::CONTAINER ||
			record.category == AssetCategory::FURNITURE)
			return FieldToolKind::CROWBAR;
		return FieldToolKind::NONE;
	}

	const char* FieldToolName(FieldToolKind tool)
	{
		switch (tool)
		{
			case FieldToolKind::NONE:          return "HANDS";
			case FieldToolKind::FIELD_SHOVEL:  return "FIELD SHOVEL*";
			case FieldToolKind::CROWBAR:       return "CROWBAR";
			case FieldToolKind::WIRE_CUTTERS:  return "WIRE CUTTERS";
			case FieldToolKind::TOOLKIT:       return "TOOLKIT";
			case FieldToolKind::CUTTING_TOOL:  return "CUTTING TOOL";
		}
		return "TOOL";
	}

	UINT16 FieldToolItem(FieldToolKind tool)
	{
		switch (tool)
		{
			// A dedicated shovel asset is still externalization work. The debug
			// build uses JA2's crowbar as a save-compatible shovel proxy.
			case FieldToolKind::FIELD_SHOVEL:
			case FieldToolKind::CROWBAR:      return CROWBAR;
			case FieldToolKind::WIRE_CUTTERS: return WIRECUTTERS;
			case FieldToolKind::TOOLKIT:      return TOOLKIT;
			case FieldToolKind::CUTTING_TOOL: return COMBAT_KNIFE;
			case FieldToolKind::NONE:         break;
		}
		return NOTHING;
	}

	BOOLEAN HasFieldTool(SOLDIERTYPE const* soldier, FieldToolKind tool)
	{
		if (tool == FieldToolKind::NONE) return TRUE;
		const UINT16 item = FieldToolItem(tool);
		return soldier && item != NOTHING &&
			((gpItemPointer && gpItemPointerSoldier == soldier &&
				gpItemPointer->usItem == item) ||
			 FindUsableObj(soldier, item) != NO_SLOT);
	}

	void EnsureDebugFieldTools(SOLDIERTYPE* soldier)
	{
		if (!soldier || gpItemPointer) return;
		constexpr std::array<UINT16, 4> tools{{
			CROWBAR, WIRECUTTERS, TOOLKIT, COMBAT_KNIFE
		}};
		for (UINT16 item : tools)
		{
			if (FindUsableObj(soldier, item) != NO_SLOT) continue;
			OBJECTTYPE object{};
			CreateItem(item, 100, &object);
			PlaceObjectCompletelyInActorPack(soldier, &object);
			// God tools are never allowed to disappear when the pack is full.  Any
			// remainder becomes a real world item at the operator's feet.
			if (object.usItem != NOTHING)
			{
				const INT32 dropped = soldier->sGridNo >= 0 &&
					soldier->sGridNo < WORLD_MAX ?
					AddItemToPool(soldier->sGridNo, &object, VISIBLE,
						soldier->bLevel, 0, -1) : -1;
				if (dropped >= 0)
				{
					OS0NotifyWorldMutation();
					continue;
				}
				InternalBeginItemPointer(soldier, &object, NO_SLOT);
				if (gpItemPointer)
				{
					OS0GetItemTransferController().
						adoptExternalHeldItemAfterHandledRelease();
					BindHeldItemCarrier(soldier);
					RecordFeedbackEvent("PACK/WORLD FULL / DEBUG TOOL HELD");
				}
				break;
			}
		}
	}

	void RestoreOperatorForGod(SOLDIERTYPE* soldier)
	{
		if (!soldier || !soldier->bActive) return;
		// Native ReviveSoldier must only be used for a merc actually removed from
		// the team.  Calling it for an unconscious living merc increments the
		// team's sector count a second time.
		if (soldier->uiStatusFlags & SOLDIER_DEAD)
		{
			ReviveSoldier(soldier);
		}
		else
		{
			soldier->uiStatusFlags &= ~SOLDIER_DEAD;
			soldier->bLife = soldier->bLifeMax;
			soldier->bBleeding = 0;
			soldier->ubDesiredHeight = ANIM_STAND;
			soldier->fInNonintAnim = FALSE;
			soldier->fRTInNonintAnim = FALSE;
		}
		soldier->bLife = soldier->bLifeMax;
		soldier->bBleeding = 0;
		soldier->bBreathMax = 100;
		soldier->bBreath = 100;
		soldier->sBreathRed = 0;
		soldier->bCollapsed = FALSE;
		soldier->bBreathCollapsed = FALSE;
		soldier->fMercCollapsedFlag = FALSE;
		soldier->ubDesiredHeight = ANIM_STAND;
		EVENT_InitNewSoldierAnim(soldier, STANDING, 0, TRUE);
		EVENT_SetSoldierPosition(soldier, soldier->sGridNo, SSP_NONE);
		fInterfacePanelDirty = DIRTYLEVEL2;
	}

	INT32 AddResourceItemToPool(GridNo gridNo, UINT8 level, ResourceKind kind,
		UINT8 amount, Visibility visibility)
	{
		if (amount == 0 || kind == ResourceKind::COUNT) return -1;
		OBJECTTYPE resource{};
		CreateItems(OS0ResourceItem(kind), 100,
			std::min<UINT8>(amount, MAX_OBJECTS_PER_SLOT), &resource);
		const INT32 itemIndex = AddItemToPool(gridNo, &resource,
			visibility, level, 0, -1);
		if (itemIndex >= 0) OS0NotifyWorldMutation();
		return itemIndex;
	}

	BOOLEAN IsContainerSeedMarker(WORLDITEM const& item)
	{
		return OS0IsContainerSeedMarker(item);
	}

	void EnsureContainerLoot(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		CanonicalContainerTarget target;
		if (!ResolveCanonicalContainerTarget(gridNo, level, tileIndex, target)) return;
		gridNo = target.gridNo;
		tileIndex = target.tileIndex;

		BOOLEAN marked = FALSE;
		BOOLEAN ordinaryLoot = FALSE;
		for (ITEM_POOL* item = GetItemPool(gridNo, level); item; item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (IsContainerSeedMarker(worldItem)) marked = TRUE;
			else if (OS0IsContainerContentItem(worldItem))
				ordinaryLoot = TRUE;
		}
		if (marked) return;

		const UINT32 hash = static_cast<UINT32>(gridNo) * 2654435761u ^
			static_cast<UINT32>(tileIndex) * 2246822519u ^
			static_cast<UINT32>(gWorldSector.AsByte()) * 3266489917u;
		SalvageProfile const asset = DescribeWorldAsset(gridNo, level, tileIndex);
		UINT8 primary = static_cast<UINT8>(1 + hash % 3);
		if (CurrentSectorEconomy().hasUpgrade(CurrentSectorKey(),
			OS0_SECTOR_UPGRADE_DEPOT)) ++primary;
		std::array<INT32, 3> newlyAdded{{ -1, -1, -1 }};
		size_t newlyAddedCount = 0;
		auto rememberAdded = [&newlyAdded, &newlyAddedCount](INT32 const index)
		{
			if (index >= 0 && newlyAddedCount < newlyAdded.size())
				newlyAdded[newlyAddedCount++] = index;
		};
		rememberAdded(AddResourceItemToPool(gridNo, level, asset.resource,
			primary, HIDDEN_IN_OBJECT));
		rememberAdded(AddResourceItemToPool(gridNo, level,
			(hash & 1) ? ResourceKind::SCRAP : ResourceKind::SOIL,
			static_cast<UINT8>(1 + (hash >> 4) % 2), HIDDEN_IN_OBJECT));

		if (!ordinaryLoot || (hash % 3) == 0)
		{
			constexpr std::array<UINT16, 8> useful{{
				CANTEEN, FIRSTAIDKIT, ALCOHOL, BREAK_LIGHT,
				WIRECUTTERS, TOOLKIT, CROWBAR, LOCKSMITHKIT
			}};
			OBJECTTYPE object{};
			CreateItem(useful[(hash >> 8) % useful.size()],
				static_cast<INT8>(45 + (hash >> 12) % 51), &object);
			rememberAdded(AddItemToPool(gridNo, &object, HIDDEN_IN_OBJECT,
				level, 0, -1));
		}
		if (!ordinaryLoot && newlyAddedCount == 0)
		{
			RecordFeedbackEvent(ST::format(
				"CONTAINER SEED FAILED grid {} / RETRY AVAILABLE", gridNo));
			return;
		}

		OBJECTTYPE marker{};
		CreateItem(ACTION_ITEM, 100, &marker);
		marker.bActionValue = 0;
		marker.ubTolerance = OS0_CONTAINER_SEED_MARKER;
		if (AddItemToPool(gridNo, &marker, HIDDEN_ITEM, level, 0, -1) < 0)
		{
			// Without the marker the next open would seed duplicates. Roll back only
			// the objects created by this attempt; pre-existing loot is untouched.
			for (size_t i = 0; i < newlyAddedCount; ++i)
			{
				const INT32 index = newlyAdded[i];
				if (index >= 0 && static_cast<size_t>(index) < gWorldItems.size() &&
					GetWorldItem(index).fExists)
					RemoveItemFromPool(GetWorldItem(index));
			}
			OS0NotifyWorldMutation();
			RecordFeedbackEvent(ST::format(
				"CONTAINER MARK FAILED grid {} / SEED ROLLED BACK", gridNo));
			return;
		}
		OS0NotifyWorldMutation();
		RecordFeedbackEvent(ST::format("CONTAINER SEEDED grid {} tile {}",
			gridNo, tileIndex));
	}

	BOOLEAN StoreResourceWorldItem(SOLDIERTYPE* actor, INT32 itemIndex)
	{
		if (!actor || itemIndex < 0 ||
			static_cast<size_t>(itemIndex) >= gWorldItems.size())
			return FALSE;
		WORLDITEM& worldItem = GetWorldItem(itemIndex);
		if (!worldItem.fExists || !OS0IsResourceItem(worldItem.o.usItem)) return FALSE;
		if (!OS0PrepareWorldItemForDirectDetach(actor, itemIndex,
			worldItem.sGridNo, worldItem.ubLevel)) return FALSE;
		ResourceKind const kind = OS0ResourceFromItem(worldItem.o.usItem);
		const UINT8 amount = worldItem.o.ubNumberOfObjects;
		if (!OS0DepositResources(CurrentSectorEconomy(), CurrentSectorKey(),
			kind, amount)) return FALSE;
		RemoveItemFromPool(worldItem);
		OS0NotifyWorldMutation();
		RecordFeedbackEvent(ST::format("STOCKPILE +{} {}", amount,
			OS0ResourceName(kind)));
		return TRUE;
	}

	BOOLEAN StoreResourceInventoryItem(SOLDIERTYPE* soldier, INT8 slot)
	{
		if (!soldier || slot < 0 || slot >= NUM_INV_SLOTS ||
			!OS0IsResourceItem(soldier->inv[slot].usItem)) return FALSE;
		OBJECTTYPE& object = soldier->inv[slot];
		ResourceKind const kind = OS0ResourceFromItem(object.usItem);
		const UINT8 amount = object.ubNumberOfObjects;
		if (!OS0DepositResources(CurrentSectorEconomy(), CurrentSectorKey(),
			kind, amount)) return FALSE;
		DeleteObj(&object);
		RecordFeedbackEvent(ST::format("STOCKPILE +{} {} FROM PACK", amount,
			OS0ResourceName(kind)));
		return TRUE;
	}

	BOOLEAN HasDiggingTool(SOLDIERTYPE const* soldier)
	{
		// Vanilla has no shovel item ID. CROWBAR is the save-compatible field-tool
		// fallback until the dedicated OS//0 shovel asset is externalized.
		return soldier && ((gpItemPointer && gpItemPointerSoldier == soldier &&
			gpItemPointer->usItem == CROWBAR) ||
			FindUsableObj(soldier, CROWBAR) != NO_SLOT);
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
		if (!soldier || level != 0 ||
			gridNo < 0 || gridNo >= WORLD_MAX ||
			PythSpacesAway(soldier->sGridNo, gridNo) > 2 ||
			!WorldAssetExistsAt(gridNo, level, tileIndex)) return FALSE;
		return DescribeWorldAsset(gridNo, level, tileIndex).salvageable &&
			HasFieldTool(soldier, RequiredFieldTool(gridNo, level, tileIndex));
	}

	OS0EnvironmentActionFacts BuildEnvironmentFacts(GridNo gridNo, UINT8 level,
		UINT16 tileIndex, SOLDIERTYPE const* actor)
	{
		OS0EnvironmentActionFacts facts;
		if (gridNo < 0 || gridNo >= WORLD_MAX) return facts;
		const BOOLEAN hasAsset = WorldAssetExistsAt(gridNo, level, tileIndex);
		STRUCTURE const* const structure = hasAsset ?
			WorldStructureAt(gridNo, level, tileIndex) : nullptr;
		facts.actorAvailable = actor && actor->bActive &&
			actor->bLife >= OKLIFE && actor->bTeam == OUR_TEAM;
		facts.hasAsset = hasAsset;
		// Contents hidden inside a closed container are not loose ground items.
		// Treating the pool head as PICK UP made a crate expose two conflicting
		// relations (OPEN and PICK UP) and could bind a hidden seed marker.
		facts.hasItems = OS0FindActionableLooseWorldItem(gridNo, level) >= 0;
		facts.terrain = level == 0 && gpWorldLevelData[gridNo].pLandHead;
		facts.near = actor && actor->bLevel == level &&
			PythSpacesAway(actor->sGridNo, gridNo) <= 2;
		facts.manipulationNear = actor && actor->bLevel == level &&
			PythSpacesAway(actor->sGridNo, gridNo) <= 1;
		facts.openable = structure &&
			(structure->fFlags & STRUCTURE_OPENABLE) &&
			!(structure->fFlags & STRUCTURE_ANYDOOR);

		constexpr StructureFlags fixed = static_cast<StructureFlags>(
			STRUCTURE_WALLSTUFF | STRUCTURE_ROOF | STRUCTURE_PERSON |
			STRUCTURE_CORPSE | STRUCTURE_TREE | STRUCTURE_ANYFENCE |
			STRUCTURE_SWITCH | STRUCTURE_VEHICLE | STRUCTURE_LIGHTSOURCE);
		facts.moveCandidate = structure && level == 0 &&
			(structure->fFlags & STRUCTURE_BASE_TILE) &&
			!(structure->fFlags & fixed) && structure->pDBStructureRef &&
			structure->pDBStructureRef->pDBStructure->ubNumberOfTiles == 1;
		facts.canMove = facts.moveCandidate && facts.actorAvailable &&
			CanSoldierMoveWorldStructure(actor, structure);
		if (facts.canMove)
		{
			WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
			facts.canThrow = physics.massKg <=
				GetSoldierWorldCarryCapacityKg(actor) * 0.30f;
		}

		SalvageProfile const salvage = facts.hasAsset ?
			DescribeWorldAsset(gridNo, level, tileIndex) :
			SalvageProfile{ "GROUND", ResourceKind::SOIL, 0, FALSE };
		facts.salvageable = salvage.salvageable;
		facts.canSalvage = facts.salvageable && facts.actorAvailable && level == 0 &&
			HasFieldTool(actor, RequiredFieldTool(gridNo, level, tileIndex));
		facts.diggableSurface = facts.terrain && !structure;
		const TerrainTypeDefines terrain = GetTerrainType(gridNo);
		const BOOLEAN diggableTerrain = terrain == FLAT_GROUND ||
			terrain == DIRT_ROAD || terrain == LOW_GRASS ||
			terrain == HIGH_GRASS;
		facts.canDig = facts.diggableSurface && facts.actorAvailable &&
			actor->bLevel == 0 && diggableTerrain && HasDiggingTool(actor);
		AssetCatalogRecord const* const catalog = facts.hasAsset ?
			OS0FindAssetCatalogRecordConst(static_cast<INT16>(giCurrentTilesetID),
				CanonicalAssetTileIndex(gridNo, level, tileIndex)) : nullptr;
		facts.buildable = catalog && catalog->buildable;
		facts.debugCatalog = TRUE;
		return facts;
	}

	INT32 ActionableWorldItemIndexAt(GridNo const gridNo, UINT8 const level,
		INT32 const preferredWorldItemIndex = -1)
	{
		if (preferredWorldItemIndex >= 0)
		{
			if (static_cast<size_t>(preferredWorldItemIndex) < gWorldItems.size())
			{
				WORLDITEM const& preferred = GetWorldItem(preferredWorldItemIndex);
				if (preferred.sGridNo == gridNo && preferred.ubLevel == level &&
					OS0IsActionableLooseWorldItem(preferred))
					return preferredWorldItemIndex;
			}
			// An explicit sprite identity is immutable. If it disappeared, the
			// gesture is stale; never reinterpret it as another pool item.
			return -1;
		}
		return OS0FindActionableLooseWorldItem(gridNo, level);
	}

	OS0ActionBinding BuildWorldItemBinding(INT32 const itemIndex,
		GridNo const gridNo, UINT8 const level, UINT16 const tileIndex)
	{
		if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
			return {};
		WORLDITEM const& worldItem = GetWorldItem(itemIndex);
		if (worldItem.sGridNo != gridNo || worldItem.ubLevel != level ||
			!OS0IsActionableLooseWorldItem(worldItem)) return {};
		OS0ActionBinding binding;
		binding.kind = OS0InteractionTargetKind::WORLD_ITEM;
		binding.gridNo = gridNo;
		binding.level = level;
		binding.tileIndex = tileIndex;
		binding.worldItemIndex = itemIndex;
		binding.worldItemType = worldItem.o.usItem;
		binding.worldItemVisibility = worldItem.bVisible;
		binding.worldItemFlags = worldItem.usFlags;
		binding.worldItemRenderZHeight = worldItem.bRenderZHeightAboveLevel;
		binding.worldItemFingerprint = DeferredItemFingerprint(worldItem.o);
		binding.worldRevision = WorldItemMutationRevision();
		return binding;
	}

	OS0ActionBinding BuildActionBinding(SOLDIERTYPE* target, GridNo gridNo,
		UINT8 level, UINT16 tileIndex, INT32 const preferredWorldItemIndex = -1)
	{
		if (target)
		{
			OS0ActionBinding binding;
			binding.kind = OS0InteractionTargetKind::ACTOR;
			binding.actorId = Soldier2ID(target);
			binding.actorInstanceId = target->uiUniqueSoldierIdValue;
			binding.gridNo = target->sGridNo;
			binding.level = static_cast<UINT8>(target->bLevel);
			binding.tileIndex = NO_TILE;
			return binding;
		}
		if (gridNo < 0 || gridNo >= WORLD_MAX) return {};
			auto assetBinding = [=]()
			{
				OS0ActionBinding binding;
			binding.kind = OS0InteractionTargetKind::WORLD_ASSET;
				binding.gridNo = gridNo;
				binding.level = level;
				binding.tileIndex = tileIndex;
				if (STRUCTURE const* const structure =
					WorldStructureAt(gridNo, level, tileIndex))
				{
					binding.assetHasStructure = TRUE;
					binding.assetStructureId = structure->usStructureID;
					binding.assetBaseGridNo = StructureBaseGridNo(structure);
				}
				else if (LEVELNODE const* const node =
					WorldObjectLayerAssetAt(gridNo, level, tileIndex))
				{
					binding.assetInstance =
						reinterpret_cast<std::uintptr_t>(node);
					binding.worldRevision = OS0WorldMutationRevision();
				}
				return binding;
		};
		// A pixel-hit loose item is more specific than an asset sharing its tile.
		// Preserve that identity through hover, radial, F and execution instead of
		// silently replacing it with the first ITEM_POOL node or the structure.
		if (preferredWorldItemIndex >= 0)
		{
			OS0ActionBinding const preferred = BuildWorldItemBinding(
				preferredWorldItemIndex, gridNo, level, tileIndex);
			return preferred;
		}
		if (WorldAssetExistsAt(gridNo, level, tileIndex)) return assetBinding();
		for (ITEM_POOL* item = GetItemPool(gridNo, level); item; item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (worldItem.sGridNo != gridNo || worldItem.ubLevel != level ||
				!OS0IsActionableLooseWorldItem(worldItem)) continue;
			return BuildWorldItemBinding(item->iItemIndex, gridNo, level,
				tileIndex);
		}
		if (WorldAssetExistsAt(gridNo, level, tileIndex))
			return assetBinding();
		if (level == 0 && gpWorldLevelData[gridNo].pLandHead)
		{
			OS0ActionBinding binding;
			binding.kind = OS0InteractionTargetKind::TERRAIN;
			binding.gridNo = gridNo;
			binding.level = level;
			binding.tileIndex = NO_TILE;
			binding.assetInstance = reinterpret_cast<std::uintptr_t>(
				gpWorldLevelData[gridNo].pLandHead);
			binding.terrainTileIndex = gpWorldLevelData[gridNo].pLandHead->usIndex;
			binding.worldRevision = OS0WorldMutationRevision();
			return binding;
		}
		return {};
	}

	OS0ResolvedActionList ResolveInteractionAtForActor(SOLDIERTYPE* actor,
		SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex,
		INT32 const preferredWorldItemIndex = -1)
	{
		const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
		const INT32 actionableWorldItemIndex = validGrid ?
			ActionableWorldItemIndexAt(gridNo, level, preferredWorldItemIndex) : -1;
		if (!target && preferredWorldItemIndex >= 0 &&
			actionableWorldItemIndex < 0) return {};
		const BOOLEAN hasItems = actionableWorldItemIndex >= 0;
		const BOOLEAN hasAsset = validGrid &&
			WorldAssetExistsAt(gridNo, level, tileIndex);
		STRUCTURE const* const structure = hasAsset ?
			WorldStructureAt(gridNo, level, tileIndex) : nullptr;
		const BOOLEAN openable = structure &&
			(structure->fFlags & STRUCTURE_OPENABLE) &&
			!(structure->fFlags & STRUCTURE_ANYDOOR);
		const BOOLEAN armed = actor &&
			actor->inv[HANDPOS].usItem != NOTHING &&
			GCM->getItem(actor->inv[HANDPOS].usItem)->isWeapon();

		OS0InteractionContext context;
		context.target = BuildActionBinding(target, gridNo, level, tileIndex,
			preferredWorldItemIndex);
		context.cursor = {
			target != nullptr,
			target && target->bTeam == OUR_TEAM,
			target && (target->bTeam == ENEMY_TEAM ||
				target->bTeam == CREATURE_TEAM),
			hasItems, openable,
			hasAsset && IsWorldAssetMovableAt(gridNo, level, tileIndex, actor),
			hasAsset, armed
		};
		context.hasEnvironment = !target && validGrid &&
			(hasItems || hasAsset ||
				(level == 0 && gpWorldLevelData[gridNo].pLandHead));
		if (context.hasEnvironment)
			context.environment = BuildEnvironmentFacts(gridNo, level, tileIndex,
				actor);
		OS0ResolvedActionList actions = ResolveOS0InteractionActions(context);
		// A tile can contain both an openable structure and a loose item. Bind each
		// command to the exact relation it will mutate instead of letting PICK UP
		// fall back to whichever ITEM_POOL node happens to be first later on.
		for (OS0ResolvedAction& resolved : actions)
		{
			if (resolved.action != ContextAction::PICK_UP) continue;
			const INT32 itemIndex = actionableWorldItemIndex;
			if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
			{
				resolved.enabled = FALSE;
				resolved.approach = OS0ActionApproach::IMPOSSIBLE;
				resolved.blockReason = OS0ActionBlockReason::INVALID_TARGET;
				resolved.binding = {};
				continue;
			}
			WORLDITEM const& worldItem = GetWorldItem(itemIndex);
			resolved.binding.kind = OS0InteractionTargetKind::WORLD_ITEM;
			resolved.binding.gridNo = gridNo;
			resolved.binding.level = level;
			resolved.binding.tileIndex = tileIndex;
			resolved.binding.worldItemIndex = itemIndex;
			resolved.binding.worldItemType = worldItem.o.usItem;
			resolved.binding.worldItemVisibility = worldItem.bVisible;
			resolved.binding.worldItemFlags = worldItem.usFlags;
			resolved.binding.worldItemRenderZHeight =
				worldItem.bRenderZHeightAboveLevel;
			resolved.binding.worldItemFingerprint =
				DeferredItemFingerprint(worldItem.o);
			resolved.binding.worldRevision = WorldItemMutationRevision();
		}
		return actions;
	}

	OS0ResolvedActionList ResolveInteractionAt(SOLDIERTYPE* target,
		GridNo gridNo, UINT8 level, UINT16 tileIndex,
		INT32 const preferredWorldItemIndex = -1)
	{
		return ResolveInteractionAtForActor(GetSelectedMan(), target, gridNo,
			level, tileIndex, preferredWorldItemIndex);
	}

	ST::string EnvironmentActionLabel(ContextAction action,
		OS0EnvironmentActionFacts const& facts, GridNo gridNo, UINT8 level,
		UINT16 tileIndex)
	{
		switch (action)
		{
			case ContextAction::CONTENTS:
				return facts.near ? "OPEN CONTENTS" : "OPEN / MOVE CLOSER";
			case ContextAction::PICK_UP:
				return facts.near ? "PICK UP" : "PICK UP / MOVE CLOSER";
			case ContextAction::DIG:
				return !HasDiggingTool(GetSelectedMan()) ?
					"DIG / NEED FIELD SHOVEL" :
					(facts.near ? "DIG / REMOVE SURFACE" :
						"DIG / MOVE CLOSER");
			case ContextAction::SALVAGE:
			{
				FieldToolKind const tool = RequiredFieldTool(gridNo, level, tileIndex);
				if (!HasFieldTool(GetSelectedMan(), tool))
					return ST::format("DISMANTLE / NEED {}", FieldToolName(tool));
				return facts.near ? "DISMANTLE / SALVAGE" :
					"DISMANTLE / MOVE CLOSER";
			}
			case ContextAction::CARRY:
				return facts.manipulationNear ? "CARRY / LIFT + PLACE" :
					"CARRY / MOVE CLOSER";
			case ContextAction::PUSH:
				return facts.manipulationNear ? "PUSH / ONE-TILE STEPS" :
					"PUSH / MOVE CLOSER";
			case ContextAction::PULL:
				return facts.manipulationNear ? "PULL / WALK BACKWARD" :
					"PULL / MOVE CLOSER";
			case ContextAction::THROW:
				return !facts.canThrow ? "THROW / TOO HEAVY" :
					(facts.manipulationNear ? "THROW / CHOOSE LANDING" :
						"THROW / MOVE CLOSER");
			case ContextAction::BUILD:
				return facts.buildable ? "BLUEPRINT / PLACEABLE" :
					"BUILD REQUIREMENTS";
			case ContextAction::INSPECT: return "INSPECT / MATERIAL + CONDITION";
			case ContextAction::CATALOG: return "GOD / CATALOG ASSET";
			default: return ContextActionName(action);
		}
	}

	BOOLEAN IsEnvironmentSkill(ContextAction action)
	{
		return action == ContextAction::DIG || action == ContextAction::SALVAGE ||
			OS0IsManipulationAction(action);
	}

	BOOLEAN RefreshEnvironmentTarget(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		const size_t previousCount = gEnvironmentEntryCount;
		std::array<ContextAction, ENVIRONMENT_SKILL_COUNT> previousActions{};
		std::array<BOOLEAN, ENVIRONMENT_SKILL_COUNT> previousEnabled{};
		for (size_t i = 0; i < previousCount; ++i)
		{
			previousActions[i] = gEnvironmentEntries[i].action;
			previousEnabled[i] = gEnvironmentEntries[i].enabled;
		}
		gEnvironmentGridNo = gridNo;
		gEnvironmentLevel = level;
		gEnvironmentTileIndex = tileIndex;
		gEnvironmentEntryCount = 0;
		SOLDIERTYPE* const actor = GetSelectedMan();
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			level, tileIndex, actor);
		if (facts.hasAsset)
			gEnvironmentTitle = DescribeWorldAsset(gridNo, level, tileIndex).displayName;
		else if (facts.terrain) gEnvironmentTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		else gEnvironmentTitle = facts.hasItems ? "GROUND ITEMS" : "NO OBJECT SELECTED";
		for (OS0ResolvedAction const& resolved :
			ResolveInteractionAt(nullptr, gridNo, level, tileIndex))
		{
			if (!IsEnvironmentSkill(resolved.action) ||
				gEnvironmentEntryCount >= gEnvironmentEntries.size()) continue;
			gEnvironmentEntries[gEnvironmentEntryCount++] = {
				resolved.action,
				EnvironmentActionLabel(resolved.action, facts, gridNo, level,
					tileIndex),
				resolved.enabled };
		}
		gEnvironmentActorGridNo = actor ? actor->sGridNo : NOWHERE;
		gNextEnvironmentRefreshAt = GetJA2Clock() + 250;
		if (previousCount != gEnvironmentEntryCount) return TRUE;
		for (size_t i = 0; i < gEnvironmentEntryCount; ++i)
			if (previousActions[i] != gEnvironmentEntries[i].action ||
				previousEnabled[i] != gEnvironmentEntries[i].enabled)
				return TRUE;
		return FALSE;
	}

	ContextAction PrimaryProximityAction(
		OS0ResolvedActionList const& actions)
	{
		OS0ResolvedAction const* const primary =
			PrimaryOS0InteractionAction(actions);
		return primary ? primary->action : ContextAction::COUNT;
	}

	void AddNearbyInteractionHint(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gNearbyHintCount >= gNearbyHints.size()) return;
		for (size_t i = 0; i < gNearbyHintCount; ++i)
		{
			if (gNearbyHints[i].gridNo == gridNo &&
				gNearbyHints[i].tileIndex == tileIndex) return;
		}
		OS0ResolvedActionList const actions =
			ResolveInteractionAt(nullptr, gridNo, level, tileIndex);
		ContextAction const primary = PrimaryProximityAction(actions);
		if (primary == ContextAction::COUNT) return;
		// Perception is a semantic overview, not one glyph per occupied tile.
		// Radius traversal is nearest-first, so keep only the closest target for
		// each action (one DIG, one CONTENTS, one PICK UP, ...).
		for (size_t i = 0; i < gNearbyHintCount; ++i)
			if (gNearbyHints[i].action == primary) return;
		auto const match = std::find_if(actions.begin(), actions.end(),
			[primary](OS0ResolvedAction const& entry)
			{ return entry.action == primary; });
		gNearbyHints[gNearbyHintCount++] = { gridNo, level, tileIndex, primary,
			match != actions.end() && match->enabled,
			match != actions.end() ? match->binding : OS0ActionBinding{} };
	}

	void UpdateNearbyInteractionHints()
	{
		std::array<NearbyInteractionHint, NEARBY_HINT_COUNT> const previous =
			gNearbyHints;
		const size_t previousCount = gNearbyHintCount;
		auto invalidateIfChanged = [&]()
		{
			BOOLEAN changed = previousCount != gNearbyHintCount;
			for (size_t i = 0; !changed && i < gNearbyHintCount; ++i)
				changed = previous[i].gridNo != gNearbyHints[i].gridNo ||
					previous[i].level != gNearbyHints[i].level ||
					previous[i].tileIndex != gNearbyHints[i].tileIndex ||
					previous[i].action != gNearbyHints[i].action ||
					previous[i].enabled != gNearbyHints[i].enabled ||
					previous[i].binding != gNearbyHints[i].binding;
			if (changed) SetRenderFlags(RENDER_FLAG_FULL);
		};
		gNearbyHintCount = 0;
		const BOOLEAN scanEnabled = InteractionMode().nearbyScanEnabled();
		if (!scanEnabled)
		{
			// Reset the resolver once on the mode edge. Doing this on every frame
			// destroyed the normal hover cache even while SCAN was dormant.
			if (gNearbyScanWasEnabled) ResetNearbyScanCache();
			gNearbyScanWasEnabled = FALSE;
			invalidateIfChanged();
			return;
		}
		if (!gNearbyScanWasEnabled)
		{
			ResetNearbyScanCache();
			gNearbyScanWasEnabled = TRUE;
		}
		SOLDIERTYPE const* const selected = GetSelectedMan();
		if (!selected || selected->bLife < OKLIFE || gTutorialActive ||
			gContextVisible || gAimAutoCollapsed || gpItemPointer ||
			CarryState().active())
		{
			gNextNearbyHintScanAt = 0;
			invalidateIfChanged();
			return;
		}
		const UINT32 now = GetJA2Clock();
		OS0PointerSnapshot const pointer =
			OS0CapturePointerSnapshot(gsInterfaceLevel);
		const GridNo cursorGridNo = pointer.hasWorldPoint ?
			pointer.gridNo : NOWHERE;
		UINT32 toolSignature = 2166136261u;
		for (INT8 slot = 0; slot < NUM_INV_SLOTS; ++slot)
		{
			OBJECTTYPE const& object = selected->inv[slot];
			toolSignature = (toolSignature ^ object.usItem) * 16777619u;
			toolSignature = (toolSignature ^ object.ubNumberOfObjects) * 16777619u;
			toolSignature = (toolSignature ^ static_cast<UINT8>(object.bStatus[0])) *
				16777619u;
		}
		auto addCursorSoilHint = [&]()
		{
			if (gNearbyHintCount < gNearbyHints.size() &&
				cursorGridNo >= 0 && cursorGridNo < WORLD_MAX &&
				PythSpacesAway(selected->sGridNo, cursorGridNo) <= 2 &&
				ResolveWorldTileIndex(cursorGridNo, 0, NO_TILE) >= NUMBEROFTILES)
				AddNearbyInteractionHint(cursorGridNo, 0, NO_TILE);
		};
		if (selected->sGridNo == gNearbyHintActorGridNo &&
			Soldier2ID(selected) == gNearbyHintActorId &&
			selected->uiUniqueSoldierIdValue == gNearbyHintActorInstanceId &&
			selected->bLevel == gNearbyHintActorLevel &&
			toolSignature == gNearbyHintToolSignature &&
			now < gNextNearbyHintScanAt)
		{
			gNearbyHints = previous;
			gNearbyHintCount = previousCount;
			if (cursorGridNo != gNearbyHintCursorGridNo)
			{
				// The expensive radius scan depends only on the actor. Cursor motion
				// changes at most the single terrain/shovel affordance.
				size_t write = 0;
				for (size_t read = 0; read < gNearbyHintCount; ++read)
				{
					NearbyInteractionHint const& hint = gNearbyHints[read];
					if (hint.action == ContextAction::DIG && hint.tileIndex == NO_TILE)
						continue;
					gNearbyHints[write++] = hint;
				}
				gNearbyHintCount = write;
				gNearbyHintCursorGridNo = cursorGridNo;
				addCursorSoilHint();
				invalidateIfChanged();
			}
			return;
		}
		gNearbyHintActorGridNo = selected->sGridNo;
		gNearbyHintActorId = Soldier2ID(selected);
		gNearbyHintActorInstanceId = selected->uiUniqueSoldierIdValue;
		gNearbyHintActorLevel = selected->bLevel;
		gNearbyHintToolSignature = toolSignature;
		gNearbyHintCursorGridNo = cursorGridNo;
		gNextNearbyHintScanAt = now + 120;
		const INT16 centreRow = selected->sGridNo / WORLD_COLS;
		const INT16 centreColumn = selected->sGridNo % WORLD_COLS;
		for (INT16 radius = 0; radius <= 2 &&
			gNearbyHintCount < gNearbyHints.size(); ++radius)
		{
			for (INT16 rowOffset = -radius; rowOffset <= radius; ++rowOffset)
			{
				for (INT16 columnOffset = -radius; columnOffset <= radius;
					++columnOffset)
				{
					if (std::max(std::abs(rowOffset), std::abs(columnOffset)) != radius)
						continue;
					const INT16 row = centreRow + rowOffset;
					const INT16 column = centreColumn + columnOffset;
					if (row < 0 || row >= WORLD_ROWS || column < 0 ||
						column >= WORLD_COLS) continue;
					const GridNo gridNo = row * WORLD_COLS + column;
					const UINT16 tileIndex = ResolveWorldTileIndex(gridNo,
						selected->bLevel, NO_TILE);
					if (tileIndex >= NUMBEROFTILES &&
						!GetItemPool(gridNo, selected->bLevel)) continue;
					AddNearbyInteractionHint(gridNo, selected->bLevel, tileIndex);
				}
			}
		}

		// Soil is ubiquitous, so only the tile currently under the pointer earns a
		// shovel affordance. This avoids surrounding the operator with identical
		// icons while keeping terrain work discoverable.
		addCursorSoilHint();
		invalidateIfChanged();
	}

	BOOLEAN SalvageWorldAsset(SOLDIERTYPE* soldier, GridNo gridNo,
		UINT8 level, UINT16 tileIndex)
	{
		if (!CanSalvageWorldAsset(soldier, gridNo, level, tileIndex))
		{
			RecordFeedbackEvent(
				"SALVAGE FAILED / INVALID ASSET, RANGE OR REQUIRED TOOL");
			return FALSE;
		}
		SalvageProfile profile = DescribeWorldAsset(gridNo, level, tileIndex);
		GridNo dropGrid = gridNo;
		LEVELNODE* structureNode = nullptr;
		BOOLEAN spillsContainer = FALSE;
		UINT16 canonicalDamageTile = tileIndex;
		if (STRUCTURE* const structure = WorldStructureAt(gridNo, level, tileIndex))
		{
			STRUCTURE* const base = FindBaseStructure(structure);
			if (!base)
			{
				RecordFeedbackEvent("SALVAGE FAILED / STRUCTURE HAS NO BASE");
				return FALSE;
			}
			dropGrid = base->sGridNo;
			spillsContainer = (base->fFlags & STRUCTURE_OPENABLE) &&
				!(base->fFlags & STRUCTURE_ANYDOOR);
			structureNode = FindLevelNodeBasedOnStructure(base);
			if (!structureNode)
			{
				RecordFeedbackEvent("SALVAGE FAILED / STRUCTURE NODE MISSING");
				return FALSE;
			}
			canonicalDamageTile = structureNode->usIndex;
		}
		else if (!WorldObjectLayerAssetAt(gridNo, level, tileIndex))
		{
			RecordFeedbackEvent("SALVAGE FAILED / OBJECT-LAYER ASSET CHANGED");
			return FALSE;
		}

		UINT8 amount = profile.amount;
		if (CurrentSectorEconomy().hasUpgrade(CurrentSectorKey(),
			OS0_SECTOR_UPGRADE_WORKSHOP))
			amount = std::min<UINT8>(MAX_OBJECTS_PER_SLOT,
				static_cast<UINT8>(amount + 1));
		const INT32 resourceIndex = AddResourceItemToPool(dropGrid, level,
			profile.resource, amount, VISIBLE);
		if (resourceIndex < 0)
		{
			RecordFeedbackEvent("SALVAGE FAILED / COULD NOT CREATE YIELD");
			return FALSE;
		}

		BOOLEAN removed = FALSE;
		{
			ApplyMapChangesToMapTempFile recordChange;
			if (structureNode)
			{
				RemoveStructFromLevelNode(dropGrid, structureNode);
				removed = TRUE;
			}
			else
			{
				removed = RemoveObject(gridNo, tileIndex);
			}
		}
		if (!removed)
		{
			if (static_cast<size_t>(resourceIndex) < gWorldItems.size() &&
				GetWorldItem(resourceIndex).fExists)
				RemoveItemFromPool(GetWorldItem(resourceIndex));
			OS0NotifyWorldMutation();
			RecordFeedbackEvent("SALVAGE FAILED / ASSET REMOVAL REJECTED");
			return FALSE;
		}
		if (structureNode)
		{
			if (spillsContainer) OS0SpillContainerContents(dropGrid, level);
			OS0ForgetWorldAssetDamage(dropGrid, level, canonicalDamageTile);
		}
		DeductPoints(soldier, 10, 180);
		RecompileLocalMovementCosts(dropGrid);
		InvalidateWorldRedundency();
		gLootGridNo = dropGrid;
		gLootLevel = level;
		gLootTileIndex = NO_TILE;
		BindLootActor(soldier);
		gInspectedGridNo = dropGrid;
		gInspectedLevel = level;
		gInspectedTileIndex = NO_TILE;
		gContextTitle = ST::format("SALVAGE / {}", OS0ResourceName(profile.resource));
		// Salvage yields are physical ground objects, not synthetic container
		// contents. Let world hover/double-click own them instead of opening an
		// empty container projection which filters correctly to HIDDEN_IN_OBJECT.
		gLootVisible = FALSE;
		RecordFeedbackEvent(ST::format("SALVAGE {} +{} {} grid {}",
			profile.displayName, amount, OS0ResourceName(profile.resource), dropGrid));
		OS0NotifyWorldMutation();
		return TRUE;
	}

	BOOLEAN DigTerrainAt(SOLDIERTYPE* soldier, GridNo gridNo, UINT16 tileIndex)
	{
		if (!CanDigTerrainAt(soldier, gridNo))
		{
			RecordFeedbackEvent(
				"DIG FAILED / INVALID SURFACE, RANGE OR FIELD SHOVEL");
			return FALSE;
		}
		LEVELNODE* const surface = gpWorldLevelData[gridNo].pLandHead;
		if (!surface)
		{
			RecordFeedbackEvent("DIG FAILED / SURFACE NODE MISSING");
			return FALSE;
		}
		const UINT16 oldIndex = surface->usIndex;
		const UINT32 oldType = GetTileType(oldIndex);
		const BOOLEAN hasBuriedLayer = surface->pNext != nullptr;
		BOOLEAN hasRemovableObject = FALSE;
		if (tileIndex < NUMBEROFTILES)
		{
			for (LEVELNODE const* object = gpWorldLevelData[gridNo].pObjectHead;
				object; object = object->pNext)
			{
				if (object->usIndex == tileIndex &&
					!(object->uiFlags & (LEVELNODE_ITEM | LEVELNODE_HIDDEN)))
				{
					hasRemovableObject = TRUE;
					break;
				}
			}
		}
		const BOOLEAN changesSurface = hasBuriedLayer || oldType != FIRSTTEXTURE;
		if (!hasRemovableObject && !changesSurface)
		{
			RecordFeedbackEvent("DIG FAILED / SURFACE ALREADY EXPOSED");
			return FALSE;
		}

		ResourceKind const yield = oldType == FIRSTTEXTURE ?
			ResourceKind::STONE : ResourceKind::SOIL;
		const INT32 resourceIndex = AddResourceItemToPool(gridNo, 0, yield,
			static_cast<UINT8>(1 + (gridNo % 2)), VISIBLE);
		if (resourceIndex < 0)
		{
			RecordFeedbackEvent("DIG FAILED / COULD NOT CREATE YIELD");
			return FALSE;
		}

		ApplyMapChangesToMapTempFile recordChange;
		BOOLEAN changed = FALSE;
		if (hasRemovableObject)
		{
			changed = RemoveObject(gridNo, tileIndex);
		}

		// A single FIRSTTEXTURE tile already represents exposed mineral soil. A
		// deeper voxel/pit layer is the next milestone; never delete the mandatory
		// base node. Surface overlays are peeled off, otherwise grass/road ground is
		// replaced with the current tileset's bare-soil texture.
		if (changesSurface)
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
		if (!changed)
		{
			if (static_cast<size_t>(resourceIndex) < gWorldItems.size() &&
				GetWorldItem(resourceIndex).fExists)
				RemoveItemFromPool(GetWorldItem(resourceIndex));
			OS0NotifyWorldMutation();
			RecordFeedbackEvent("DIG FAILED / MAP CHANGE REJECTED");
			return FALSE;
		}
		DeductPoints(soldier, 8, 120);
		RecompileLocalMovementCosts(gridNo);
		InvalidateWorldRedundency();
		SetRenderFlags(RENDER_FLAG_FULL);
		RecordFeedbackEvent(ST::format("DIG SUCCESS grid {} old tile {}",
			gridNo, oldIndex));
		gLootGridNo = gridNo;
		gLootLevel = 0;
		gLootTileIndex = NO_TILE;
		BindLootActor(soldier);
		gLootVisible = FALSE;
		OS0NotifyWorldMutation();
		return TRUE;
	}

	BOOLEAN BindingStillValid(OS0ActionBinding const& binding)
	{
		switch (binding.kind)
		{
			case OS0InteractionTargetKind::ACTOR:
			{
				SOLDIERTYPE* const target = binding.actorId >= 0 ?
					ID2Soldier(static_cast<UINT8>(binding.actorId)) : nullptr;
				return target && target->bActive &&
					target->uiUniqueSoldierIdValue == binding.actorInstanceId;
			}
			case OS0InteractionTargetKind::WORLD_ITEM:
				if (binding.worldItemIndex < 0 ||
					binding.worldRevision == 0 ||
					binding.worldRevision != WorldItemMutationRevision() ||
					static_cast<size_t>(binding.worldItemIndex) >= gWorldItems.size())
					return FALSE;
			{
				WORLDITEM const& worldItem =
					GetWorldItem(binding.worldItemIndex);
				return worldItem.fExists &&
					worldItem.sGridNo == binding.gridNo &&
					worldItem.ubLevel == binding.level &&
					worldItem.o.usItem == binding.worldItemType &&
					worldItem.bVisible == binding.worldItemVisibility &&
					worldItem.usFlags == binding.worldItemFlags &&
					worldItem.bRenderZHeightAboveLevel ==
						binding.worldItemRenderZHeight &&
					DeferredItemFingerprint(worldItem.o) ==
						binding.worldItemFingerprint;
			}
			case OS0InteractionTargetKind::WORLD_ASSET:
			{
				if (binding.assetHasStructure)
				{
					STRUCTURE const* const structure = WorldStructureAt(
						binding.gridNo, binding.level, binding.tileIndex);
					return structure &&
						structure->usStructureID == binding.assetStructureId &&
						StructureBaseGridNo(structure) == binding.assetBaseGridNo;
				}
				LEVELNODE const* const node = WorldObjectLayerAssetAt(
					binding.gridNo, binding.level, binding.tileIndex);
				return node && binding.worldRevision != 0 &&
					binding.worldRevision == OS0WorldMutationRevision() &&
					reinterpret_cast<std::uintptr_t>(node) == binding.assetInstance;
			}
			case OS0InteractionTargetKind::TERRAIN:
				if (binding.gridNo < 0 || binding.gridNo >= WORLD_MAX ||
					binding.level != 0 || binding.worldRevision == 0 ||
					binding.worldRevision != OS0WorldMutationRevision())
					return FALSE;
			{
				LEVELNODE const* const land = gpWorldLevelData[binding.gridNo].pLandHead;
				return land && land->usIndex == binding.terrainTileIndex &&
					reinterpret_cast<std::uintptr_t>(land) == binding.assetInstance;
			}
			case OS0InteractionTargetKind::NONE:
				return FALSE;
		}
		return FALSE;
	}

	BOOLEAN ResolveBoundAction(OS0ActionBinding const& binding,
		ContextAction action, OS0ResolvedAction& result,
		SOLDIERTYPE* actor = nullptr)
	{
		if (!BindingStillValid(binding)) return FALSE;
		if (!actor) actor = GetSelectedMan();
		SOLDIERTYPE* target = nullptr;
		GridNo gridNo = binding.gridNo;
		UINT8 level = binding.level;
		UINT16 tileIndex = binding.tileIndex;
		if (binding.kind == OS0InteractionTargetKind::ACTOR)
		{
			target = ID2Soldier(static_cast<UINT8>(binding.actorId));
			gridNo = target->sGridNo;
			level = target->bLevel;
			tileIndex = NO_TILE;
		}
		OS0ResolvedActionList const actions =
			ResolveInteractionAtForActor(actor, target, gridNo, level, tileIndex);
		OS0ResolvedAction const* const resolved =
			FindOS0ResolvedAction(actions, action);
		if (!resolved) return FALSE;
		result = *resolved;
		result.binding = binding;
		return TRUE;
	}

	BOOLEAN ExecuteBoundWorldAction(ContextAction action,
		OS0ActionBinding const& binding, SOLDIERTYPE* actor = nullptr)
	{
		if (!actor) actor = GetSelectedMan();
		if (!actor || !BindingStillValid(binding) ||
			binding.gridNo < 0 || binding.gridNo >= WORLD_MAX) return FALSE;

		switch (action)
		{
			case ContextAction::CONTENTS:
				OS0OpenWorldContainer(binding.gridNo, binding.level,
					binding.tileIndex, actor);
				return TRUE;
			case ContextAction::PICK_UP:
			{
				if (binding.kind != OS0InteractionTargetKind::WORLD_ITEM)
					return FALSE;
				const INT32 itemIndex = binding.worldItemIndex;
				if (itemIndex < 0 ||
					static_cast<size_t>(itemIndex) >= gWorldItems.size())
					return FALSE;
				WORLDITEM const& worldItem = GetWorldItem(itemIndex);
				if (!worldItem.fExists || worldItem.o.usItem == NOTHING)
					return FALSE;
				if (OS0IsResourceItem(worldItem.o.usItem))
					return StoreResourceWorldItem(actor, itemIndex);
				if (!OS0CanAcceptCarriedObject(actor, worldItem.o))
				{
					RecordFeedbackEvent("LOAD LIMIT 125% / PICKUP REJECTED");
					return FALSE;
				}
				if (OS0CanPackObject(actor, worldItem.o))
				{
					if (!OS0SoldierPickupExactWorldItem(actor, itemIndex,
						binding.gridNo, ITEM_IGNORE_Z_LEVEL))
					{
						RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
						return FALSE;
					}
				}
				else if (!BeginTrackedWorldItemTransfer(actor, itemIndex,
					binding.tileIndex, TRUE))
				{
					RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
					return FALSE;
				}
				else
				{
					RecordFeedbackEvent("PACK FULL / ITEM HELD FOR PLACEMENT");
				}
				return TRUE;
			}
			case ContextAction::DIG:
			{
				const BOOLEAN changed = DigTerrainAt(actor, binding.gridNo,
					binding.tileIndex);
				if (changed)
					RefreshEnvironmentTarget(binding.gridNo, 0, NO_TILE);
				else
					RecordFeedbackEvent("DIG FAILED / TARGET, RANGE OR TOOL CHANGED");
				return changed;
			}
			case ContextAction::SALVAGE:
			{
				const BOOLEAN changed = SalvageWorldAsset(actor, binding.gridNo,
					binding.level, binding.tileIndex);
				if (changed)
					RefreshEnvironmentTarget(binding.gridNo, binding.level,
						NO_TILE);
				else
					RecordFeedbackEvent(
						"SALVAGE FAILED / TARGET, RANGE OR TOOL CHANGED");
				return changed;
			}
			case ContextAction::CARRY:
			case ContextAction::PUSH:
			case ContextAction::PULL:
			case ContextAction::THROW:
				return BeginWorldMoveAt(binding.gridNo, binding.level,
					binding.tileIndex, CarryModeForAction(action), actor);
			case ContextAction::INSPECT:
				return OS0SelectWorldObject(nullptr, binding.gridNo,
					binding.level, binding.tileIndex);
			default:
				return FALSE;
		}
	}

	BOOLEAN CancelPendingWorldAction(const char* reason,
		BOOLEAN stopOwnedMovement = TRUE)
	{
		if (!gPendingWorldAction.active()) return FALSE;
		PendingWorldAction const pending = gPendingWorldAction;
		// Clear ownership before touching the soldier: stopping movement can run
		// native callbacks which must not observe a still-live deferred action.
		gPendingWorldAction.reset();
		SOLDIERTYPE* const actor = ID2Soldier(pending.actorId);
		if (stopOwnedMovement && actor && actor->bActive &&
			actor->uiUniqueSoldierIdValue == pending.actorInstanceId &&
			actor->sFinalDestination == pending.destination)
		{
			StopSoldier(actor);
		}
		RecordFeedbackEvent(ST::format("APPROACH ACTION / {}",
			reason ? reason : "CANCELLED"));
		NotifyFieldTutorial(OS0FieldTutorialEvent::APPROACH_ABORTED,
			pending.binding);
		return TRUE;
	}

	BOOLEAN QueueApproachForAction(OS0ResolvedAction const& resolved,
		SOLDIERTYPE* actor = nullptr)
	{
		if (!resolved.enabled ||
			resolved.approach != OS0ActionApproach::MOVE_TO_RANGE ||
			resolved.binding.kind == OS0InteractionTargetKind::ACTOR)
			return FALSE;
		if (!actor) actor = GetSelectedMan();
		if (!actor || !BindingStillValid(resolved.binding)) return FALSE;

		const GridNo destination = FindCarryActionGrid(actor,
			resolved.binding.gridNo);
		if (destination == NOWHERE)
		{
			RecordFeedbackEvent(ST::format("{} / NO APPROACH PATH",
				ContextActionName(resolved.action)));
			return FALSE;
		}
		// Release the previous route before commissioning the replacement. Doing
		// this after EVENT_InternalGetNewSoldierPath could stop the freshly issued
		// route when both actions happened to share the same approach tile.
		CancelWorldMoveState();
		CancelPendingWorldAction("REPLACED");
		if (!EVENT_InternalGetNewSoldierPath(actor, destination,
			actor->usUIMovementMode, TRUE, actor->fNoAPToFinishMove))
		{
			RecordFeedbackEvent(ST::format("{} / PATH REJECTED",
				ContextActionName(resolved.action)));
			return FALSE;
		}
		gPendingWorldAction = { resolved.action, resolved.binding,
			Soldier2ID(actor), actor->uiUniqueSoldierIdValue,
			destination, GetJA2Clock() };
		NotifyFieldTutorial(OS0FieldTutorialEvent::APPROACH_STARTED,
			resolved.binding);
		RecordFeedbackEvent(ST::format("{} / APPROACHING GRID {}",
			ContextActionName(resolved.action), resolved.binding.gridNo));
		return TRUE;
	}

	BOOLEAN ExecuteOrQueueBoundAction(OS0ResolvedAction const& resolved,
		SOLDIERTYPE* actor = nullptr)
	{
		if (!resolved.enabled)
		{
			RecordFeedbackEvent(ST::format("{} / {}",
				ContextActionName(resolved.action),
				OS0ActionBlockReasonName(resolved.blockReason)));
			return FALSE;
		}
		if (resolved.approach == OS0ActionApproach::MOVE_TO_RANGE)
			return QueueApproachForAction(resolved, actor);
		if (resolved.approach == OS0ActionApproach::IMPOSSIBLE) return FALSE;
		return ExecuteBoundWorldAction(resolved.action, resolved.binding, actor);
	}

	void UpdatePendingWorldAction()
	{
		if (!gPendingWorldAction.active()) return;
		PendingWorldAction const pending = gPendingWorldAction;
		SOLDIERTYPE* const actor = ID2Soldier(pending.actorId);
		if (!actor || !actor->bActive || actor->bLife < OKLIFE ||
			actor->uiUniqueSoldierIdValue != pending.actorInstanceId)
		{
			CancelPendingWorldAction("ACTOR UNAVAILABLE", FALSE);
			return;
		}
		if (actor->sSector != gWorldSector || actor->fBetweenSectors ||
			actor->bLevel != pending.binding.level)
		{
			CancelPendingWorldAction("ACTOR LEFT TARGET", FALSE);
			return;
		}
		if (GetJA2Clock() - pending.startedAt > 30000)
		{
			CancelPendingWorldAction("TIMEOUT");
			return;
		}
		if (!BindingStillValid(pending.binding))
		{
			CancelPendingWorldAction("TARGET CHANGED");
			return;
		}
		const INT16 distance = PythSpacesAway(actor->sGridNo,
			pending.binding.gridNo);
		if (distance > 2)
		{
			const BOOLEAN moving =
				(gAnimControl[actor->usAnimState].uiFlags & ANIM_MOVING) != 0;
			if (actor->sFinalDestination != pending.destination ||
				(!moving && actor->sGridNo == actor->sFinalDestination))
				CancelPendingWorldAction("PATH INTERRUPTED", FALSE);
			return;
		}
		if (gAnimControl[actor->usAnimState].uiFlags & ANIM_MOVING) return;

		OS0ResolvedAction resolved;
		if (!ResolveBoundAction(pending.binding, pending.action, resolved, actor) ||
			!resolved.enabled)
		{
			CancelPendingWorldAction("RELATION CHANGED", FALSE);
			return;
		}
		gPendingWorldAction.reset();
		if (!ExecuteBoundWorldAction(pending.action, pending.binding, actor))
		{
			RecordFeedbackEvent(ST::format("{} / EXECUTION FAILED",
				ContextActionName(pending.action)));
			NotifyFieldTutorial(OS0FieldTutorialEvent::APPROACH_ABORTED,
				pending.binding);
		}
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	INT8 NearbyCoverHeight(GridNo gridNo, UINT8 level)
	{
		INT8 height = 0;
		for (UINT8 direction = 0; direction < NUM_WORLD_DIRECTIONS; ++direction)
		{
			const GridNo adjacent = NewGridNo(gridNo, DirectionInc(direction));
			if (adjacent == gridNo || adjacent < 0 || adjacent >= WORLD_MAX) continue;
			height = std::max<INT8>(height,
				GetTallestStructureHeight(adjacent, level != 0));
		}
		return height;
	}

	GridNo FindFallbackNearbyCover(SOLDIERTYPE* soldier)
	{
		if (!soldier) return NOWHERE;
		const INT16 originX = soldier->sGridNo % WORLD_COLS;
		const INT16 originY = soldier->sGridNo / WORLD_COLS;
		GridNo best = NOWHERE;
		INT16 bestScore = 32000;
		for (INT16 radius = 1; radius <= 8; ++radius)
		{
			for (INT16 dy = -radius; dy <= radius; ++dy)
			{
				for (INT16 dx = -radius; dx <= radius; ++dx)
				{
					if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
					const INT16 x = originX + dx;
					const INT16 y = originY + dy;
					if (x < 0 || x >= WORLD_COLS || y < 0 || y >= WORLD_ROWS) continue;
					const GridNo candidate = y * WORLD_COLS + x;
					if (!NewOKDestination(soldier, candidate, TRUE, soldier->bLevel) ||
						NearbyCoverHeight(candidate, soldier->bLevel) == 0) continue;
					const INT16 path = PlotPath(soldier, candidate, NO_COPYROUTE,
						NO_PLOT, RUNNING, 0);
					if (path <= 0) continue;
					const INT16 score = static_cast<INT16>(path * 4 -
						NearbyCoverHeight(candidate, soldier->bLevel) * 3);
					if (score < bestScore)
					{
						best = candidate;
						bestScore = score;
					}
				}
			}
			if (best != NOWHERE) break;
		}
		return best;
	}

	BOOLEAN CommandRunToCover(SOLDIERTYPE* soldier)
	{
		if (!soldier || soldier->bTeam != OUR_TEAM || soldier->bLife < OKLIFE)
			return FALSE;
		INT32 improvement = 0;
		GridNo destination = FindBestNearbyCover(soldier, MORALE_NORMAL,
			&improvement);
		if (destination == NOWHERE || destination == soldier->sGridNo)
			destination = FindFallbackNearbyCover(soldier);
		if (destination == NOWHERE || destination == soldier->sGridNo)
		{
			RecordFeedbackEvent("AI COVER / NO REACHABLE COVER");
			return FALSE;
		}
		soldier->usUIMovementMode = RUNNING;
		if (!EVENT_InternalGetNewSoldierPath(soldier, destination, RUNNING,
			TRUE, TRUE)) return FALSE;
		OS0GetTacticalSession().state().coverOrders.issue({ soldier->ubID,
			destination, static_cast<UINT8>(soldier->bLevel),
			OS0CoverStance::AUTO });
		RecordFeedbackEvent(ST::format("AI COVER {} -> {}", soldier->name,
			destination));
		return TRUE;
	}

	void UpdateCoverCommands()
	{
		OS0CoverOrderSystem& system =
			OS0GetTacticalSession().state().coverOrders;
		auto const& orders = system.orders();
		// Cancelling erases from the vector. Walking backwards keeps every lower
		// index valid and avoids copying the whole order list every tactical frame.
		for (size_t index = orders.size(); index-- > 0;)
		{
			OS0CoverOrder const order = orders[index];
			SOLDIERTYPE* const soldier = ID2Soldier(order.soldier);
			if (!soldier || !soldier->bActive || soldier->bLife < OKLIFE ||
				soldier->sSector != gWorldSector || soldier->fBetweenSectors ||
				soldier->bLevel != order.level)
			{
				system.cancel(order.soldier);
				continue;
			}
			if (soldier->sGridNo != order.destination)
			{
				if (soldier->sFinalDestination != order.destination)
					system.cancel(order.soldier);
				continue;
			}
			const INT8 coverHeight = NearbyCoverHeight(order.destination,
				soldier->bLevel);
			INT8 stance = coverHeight > 0 && coverHeight < 2 ?
				ANIM_PRONE : ANIM_CROUCH;
			if (order.desiredStance == OS0CoverStance::CROUCH) stance = ANIM_CROUCH;
			else if (order.desiredStance == OS0CoverStance::PRONE) stance = ANIM_PRONE;
			if (IsValidStance(soldier, stance)) ChangeSoldierStance(soldier, stance);
			RecordFeedbackEvent(ST::format("AI COVER ARRIVED {} / {}",
				soldier->name, stance == ANIM_PRONE ? "PRONE" : "CROUCH"));
			system.cancel(order.soldier);
		}
	}

	void PositionContextRegions()
	{
		if (gCharacterActionFanVisible && gContextSoldier)
		{
			constexpr INT16 iconSize = 26;
			struct RingPoint { INT16 x; INT16 y; };
			constexpr std::array<RingPoint, 12> ring{{
				{   0,-122 }, {  61,-106 }, { 106, -61 }, { 122,   0 },
				{ 106,  61 }, {  61, 106 }, {   0, 122 }, { -61, 106 },
				{-106,  61 }, {-122,   0 }, {-106, -61 }, { -61,-106 }
			}};
			INT16 anchorX;
			INT16 anchorY;
			if (!GetActorDisplayAnchor(gContextSoldier, anchorX, anchorY)) return;
			gContextX = std::clamp<INT16>(anchorX,
				gsVIEWPORT_START_X + 138, gsVIEWPORT_END_X - 138);
			gContextY = std::clamp<INT16>(anchorY - 36,
				gsVIEWPORT_WINDOW_START_Y + 138,
				gsVIEWPORT_WINDOW_END_Y - 138);
			gContextBlock.RegionTopLeftX = gContextX - 138;
			gContextBlock.RegionTopLeftY = gContextY - 138;
			gContextBlock.RegionBottomRightX = gContextX + 138;
			gContextBlock.RegionBottomRightY = gContextY + 138;
			gUIRuntime.panel(OS0UIPanel::CONTEXT).w = 276;
			gUIRuntime.panel(OS0UIPanel::CONTEXT).h = 276;
			for (size_t i = 0; i < gContextRegions.size(); ++i)
			{
				if (i >= gContextEntryCount) continue;
				const size_t ringIndex = i * ring.size() / gContextEntryCount;
				const INT16 x = static_cast<INT16>(
					gContextX + ring[ringIndex].x - iconSize / 2);
				const INT16 y = static_cast<INT16>(
					gContextY + ring[ringIndex].y - iconSize / 2);
				gContextRegions[i].RegionTopLeftX = x;
				gContextRegions[i].RegionTopLeftY = y;
				gContextRegions[i].RegionBottomRightX = x + iconSize;
				gContextRegions[i].RegionBottomRightY = y + iconSize;
				gContextRegions[i].SetFastHelpText(ST::format("[{}] {}\n{}",
					ContextEntryGroupName(gContextEntries[i]),
					gContextEntries[i].label,
					ContextEntryExplanation(gContextEntries[i])));
			}
			return;
		}
		if (gObjectActionFanVisible)
		{
			constexpr INT16 iconSize = 26;
			struct RingPoint { INT16 x; INT16 y; };
			constexpr std::array<RingPoint, 12> ring{{
				{   0,-74 }, {  37,-64 }, {  64,-37 }, {  74,  0 },
				{  64, 37 }, {  37, 64 }, {   0, 74 }, { -37, 64 },
				{ -64, 37 }, { -74,  0 }, { -64,-37 }, { -37,-64 }
			}};
			INT16 anchorX = gContextX;
			INT16 anchorY = gContextY;
			if (gContextGridNo >= 0 && gContextGridNo < WORLD_MAX)
			{
				GetGridNoScreenPos(gContextGridNo, gContextLevel,
					&anchorX, &anchorY);
				OS0MapWorldToDisplayScreen(&anchorX, &anchorY);
			}
			gContextX = std::clamp<INT16>(anchorX,
				gsVIEWPORT_START_X + 92, gsVIEWPORT_END_X - 92);
			gContextY = std::clamp<INT16>(anchorY - 20,
				gsVIEWPORT_WINDOW_START_Y + 92,
				gsVIEWPORT_WINDOW_END_Y - 92);
			gContextBlock.RegionTopLeftX = gContextX - 92;
			gContextBlock.RegionTopLeftY = gContextY - 92;
			gContextBlock.RegionBottomRightX = gContextX + 92;
			gContextBlock.RegionBottomRightY = gContextY + 92;
			gUIRuntime.panel(OS0UIPanel::CONTEXT).w = 184;
			gUIRuntime.panel(OS0UIPanel::CONTEXT).h = 184;
			for (size_t i = 0; i < gContextRegions.size(); ++i)
			{
				if (i >= gContextEntryCount) continue;
				const size_t ringIndex = i * ring.size() / gContextEntryCount;
				const INT16 x = static_cast<INT16>(
					gContextX + ring[ringIndex].x - iconSize / 2);
				const INT16 y = static_cast<INT16>(
					gContextY + ring[ringIndex].y - iconSize / 2);
				gContextRegions[i].RegionTopLeftX = x;
				gContextRegions[i].RegionTopLeftY = y;
				gContextRegions[i].RegionBottomRightX = x + iconSize;
				gContextRegions[i].RegionBottomRightY = y + iconSize;
				gContextRegions[i].SetFastHelpText(ST::format("[{}] {}\n{}",
					ActionCategoryName(ContextActionCategory(gContextEntries[i].action)),
					gContextEntries[i].label,
					ContextActionExplanation(gContextEntries[i].action)));
			}
			return;
		}
		constexpr INT16 width = 168;
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		gContextBlock.RegionTopLeftX = gContextX;
		gContextBlock.RegionTopLeftY = gContextY;
		gContextBlock.RegionBottomRightX = gContextX + width;
		gContextBlock.RegionBottomRightY = gContextY + height;
		for (size_t i = 0; i < gContextRegions.size(); ++i)
		{
			const INT16 y = static_cast<INT16>(
				gContextY + 17 + static_cast<INT16>(i) * 18);
			gContextRegions[i].RegionTopLeftX = gContextX + 4;
			gContextRegions[i].RegionTopLeftY = y;
			gContextRegions[i].RegionBottomRightX = gContextX + 164;
			gContextRegions[i].RegionBottomRightY = y + 17;
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
		// JA2 stops its unzoomed render camera at the map boundary. Shift the
		// magnified crop continuously during the final half-viewport of travel;
		// boolean edge flags caused an abrupt 160-pixel jump on the last scroll step.
		const INT16 centeredOffsetX = source.x - destination.x;
		const INT16 centeredOffsetY = source.y - destination.y;
		const INT16 maxOffsetX = destination.w - source.w;
		const INT16 maxOffsetY = destination.h - source.h;
		const INT32 mapWidth = gsRightX - gsLeftX;
		const INT32 mapHeight = gsBottomY - gsTopY;
		const INT32 fromLeft = gsTopLeftWorldX - SCROLL_LEFT_PADDING;
		const INT32 fromRight = mapWidth + SCROLL_RIGHT_PADDING -
			gsBottomRightWorldX;
		const INT32 fromTop = gsTopLeftWorldY - SCROLL_TOP_PADDING;
		const INT32 fromBottom = mapHeight + SCROLL_BOTTOM_PADDING -
			gsBottomRightWorldY;
		INT16 offsetX = centeredOffsetX;
		INT16 offsetY = centeredOffsetY;
		if (fromLeft < centeredOffsetX)
			offsetX = static_cast<INT16>(std::clamp<INT32>(fromLeft, 0,
				centeredOffsetX));
		else if (fromRight < maxOffsetX - centeredOffsetX)
			offsetX = static_cast<INT16>(maxOffsetX - std::clamp<INT32>(
				fromRight, 0, maxOffsetX - centeredOffsetX));
		if (fromTop < centeredOffsetY)
			offsetY = static_cast<INT16>(std::clamp<INT32>(fromTop, 0,
				centeredOffsetY));
		else if (fromBottom < maxOffsetY - centeredOffsetY)
			offsetY = static_cast<INT16>(maxOffsetY - std::clamp<INT32>(
				fromBottom, 0, maxOffsetY - centeredOffsetY));
		source.x = destination.x + offsetX;
		source.y = destination.y + offsetY;
	}

	BOOLEAN SameWorldZoomViewport(SGPBox const& left, SGPBox const& right)
	{
		return left.x == right.x && left.y == right.y &&
			left.w == right.w && left.h == right.h;
	}

	void DiscardWorldZoomBuffer()
	{
		if (gWorldZoomBuffer) DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = nullptr;
		gWorldZoomBufferViewport = {};
		gWorldZoomBufferViewportValid = FALSE;
	}

	void PositionEquipmentRegions()
	{
		if (!gEquipmentExplodedVisible || !gEquipmentSoldier) return;
		struct EquipmentPoint { INT16 x; INT16 y; };
		constexpr std::array<EquipmentPoint, 7> offsets{{
			{ -17, -106 }, { -61, -87 }, { 27, -87 }, { -17, -70 },
			{ -17, -32 }, { -68, -51 }, { 34, -51 }
		}};
		INT16 anchorX;
		INT16 anchorY;
		if (!GetActorDisplayAnchor(gEquipmentSoldier, anchorX, anchorY)) return;
		gEquipmentCentreX = std::clamp<INT16>(anchorX,
			gsVIEWPORT_START_X + 116, gsVIEWPORT_END_X - 116);
		gEquipmentCentreY = std::clamp<INT16>(anchorY,
			gsVIEWPORT_WINDOW_START_Y + 112,
			gsVIEWPORT_WINDOW_END_Y - 34);
		for (size_t i = 0; i < gEquipmentRegions.size(); ++i)
		{
			const INT16 x = gEquipmentCentreX + offsets[i].x;
			const INT16 y = gEquipmentCentreY + offsets[i].y;
			gEquipmentRegions[i].RegionTopLeftX = x;
			gEquipmentRegions[i].RegionTopLeftY = y;
			gEquipmentRegions[i].RegionBottomRightX = x + 34;
			gEquipmentRegions[i].RegionBottomRightY = y + 25;
		}
		gEquipmentPackRegion.RegionTopLeftX = gEquipmentCentreX + 48;
		gEquipmentPackRegion.RegionTopLeftY = gEquipmentCentreY - 17;
		gEquipmentPackRegion.RegionBottomRightX = gEquipmentCentreX + 94;
		gEquipmentPackRegion.RegionBottomRightY = gEquipmentCentreY + 8;
	}

	void PositionItemTransferIntentRegions()
	{
		INT16 anchorX;
		INT16 anchorY;
		SOLDIERTYPE* const target = BoundItemTransferTarget();
		if (!gpItemPointer || !target ||
			!GetActorDisplayAnchor(target, anchorX, anchorY)) return;
		ItemTransferPolicyDecision const decision =
			CurrentItemTransferDecision(target);
		for (size_t i = 0; i < gOS0ItemTransferIntents.size(); ++i)
		{
			const BOOLEAN direct = decision.hasPreferred &&
				decision.preferred == gOS0ItemTransferIntents[i].intent;
			const INT16 offsetX = !gItemTransferMoreVisible && direct ?
				-34 : gOS0ItemTransferIntents[i].offsetX;
			const INT16 offsetY = !gItemTransferMoreVisible && direct ?
				-62 : gOS0ItemTransferIntents[i].offsetY;
			const INT16 x = std::clamp<INT16>(
				anchorX + offsetX,
				gsVIEWPORT_START_X + 2, gsVIEWPORT_END_X - 30);
			const INT16 y = std::clamp<INT16>(
				anchorY + offsetY,
				gsVIEWPORT_WINDOW_START_Y + 2, gsVIEWPORT_WINDOW_END_Y - 30);
			MoveRegion(gItemTransferIntentRegions[i], x, y);
		}
		const INT16 moreX = std::clamp<INT16>(anchorX +
			(gItemTransferMoreVisible ? 58 : 8),
			gsVIEWPORT_START_X + 2, gsVIEWPORT_END_X - 30);
		const INT16 moreY = std::clamp<INT16>(anchorY +
			(gItemTransferMoreVisible ? -112 : -62),
			gsVIEWPORT_WINDOW_START_Y + 2, gsVIEWPORT_WINDOW_END_Y - 30);
		MoveRegion(gItemTransferMoreRegion, moreX, moreY);
	}

	BOOLEAN GetActorDisplayAnchor(SOLDIERTYPE const* soldier, INT16& x, INT16& y)
	{
		if (!soldier || soldier->sGridNo < 0 || soldier->sGridNo >= WORLD_MAX)
			return FALSE;
		// Follow JA2's interpolated animation position rather than GridNo. GridNo
		// advances one tile at a time and made every attached UI element jump.
		GetSoldierScreenPos(soldier, &x, &y);
		if (x == 0 && y == 0)
			GetGridNoScreenPos(soldier->sGridNo, soldier->bLevel, &x, &y);
		else
		{
			x = static_cast<INT16>(x + soldier->sBoundingBoxWidth / 2);
			y = static_cast<INT16>(y + soldier->sBoundingBoxHeight);
		}
		OS0MapWorldToDisplayScreen(&x, &y);
		// GetSoldierScreenPos already projects the engine's continuous dXPos/dYPos.
		// A second UI-only low-pass made attached gear lag and then jump whenever
		// the camera or zoom changed. The actor sprite and its projections now use
		// the same authoritative animation position in the same frame.
		return TRUE;
	}

	void SyncManagedMouseRegionZOrder(OS0WindowManager const& windows)
	{
		// Region addresses are process-stable. Build this projection once, then let
		// the adapter do a no-op whenever manager Z has not changed.
		static std::vector<OS0ManagedMouseRegionGroup> groups;
		if (groups.empty())
		{
			groups.reserve(48);
			auto addOne = [](OS0WindowHandle const window, MOUSE_REGION& region)
			{
				groups.push_back({ window, std::span<MOUSE_REGION>{ &region, 1 }, {} });
			};
			auto addArray = [](OS0WindowHandle const window, auto& regions)
			{
				groups.push_back({ window, std::span<MOUSE_REGION>{ regions }, {} });
			};

			OS0WindowHandle const inventory =
				gUIRuntime.managedId(OS0UIPanel::INVENTORY);
			addOne(inventory, gBagBlock);
			addArray(inventory, gSlotRegions);
			addOne(inventory, gTutorialContinue);
			addArray(inventory, gTutorialStats);
			addArray(inventory, gTutorialBodyRegions);
			addArray(inventory, gTutorialTraitRegions);
			addOne(inventory, gBagGrabber);
			addOne(inventory, gBagClose);

			OS0WindowHandle const context =
				gUIRuntime.managedId(OS0UIPanel::CONTEXT);
			addOne(context, gContextBlock);
			addArray(context, gContextRegions);

			addArray(gUIRuntime.managedId(OS0UIPanel::LOOT), gLootRegions);

			OS0WindowHandle const equipment =
				gUIRuntime.managedId(OS0UIPanel::EQUIPMENT);
			addArray(equipment, gEquipmentRegions);
			addOne(equipment, gEquipmentPackRegion);

			OS0WindowHandle const split =
				gUIRuntime.managedId(OS0UIPanel::STACK_SPLIT);
			addOne(split, gStackSplitBlock);
			addArray(split, gStackSplitRegions);

			OS0WindowHandle const library =
				gUIRuntime.managedId(OS0UIPanel::ASSET_LIBRARY);
			addOne(library, gGodLibraryBlock);
			addArray(library, gAssetLibraryRegions);
			addArray(library, gGodIconRegions);
			addOne(library, gGodLibraryGrabber);
			addOne(library, gGodLibraryClose);

			OS0WindowHandle const catalog =
				gUIRuntime.managedId(OS0UIPanel::ASSET_CATALOG);
			addOne(catalog, gAssetCatalogBlock);
			addArray(catalog, gAssetCatalogRegions);

			auto addFloatingShell = [&](FloatingPanelId const panel)
			{
				size_t const index = static_cast<size_t>(panel);
				OS0WindowHandle const window = gUIRuntime.managedId(panel);
				addOne(window, gFloatingPanelBlocks[index]);
				return window;
			};
			auto addFloatingControls = [&](FloatingPanelId const panel)
			{
				size_t const index = static_cast<size_t>(panel);
				OS0WindowHandle const window = gUIRuntime.managedId(panel);
				addOne(window, gFloatingPanelGrabbers[index]);
				addOne(window, gFloatingPanelCloses[index]);
			};

			OS0WindowHandle const sector = addFloatingShell(FloatingPanelId::SECTOR);
			addArray(sector, gFeedbackRegions);
			addArray(sector, gSectorUpgradeRegions);
			addArray(sector, gSectorTabRegions);
			addOne(sector, gStrategicMapRegion);
			addOne(sector, gSectorTeamRegion);
			addFloatingControls(FloatingPanelId::SECTOR);

			addFloatingShell(FloatingPanelId::INSPECTOR);
			addFloatingControls(FloatingPanelId::INSPECTOR);

			OS0WindowHandle const toolbox = addFloatingShell(FloatingPanelId::TOOLBOX);
			addArray(toolbox, gToolboxRegions);
			addFloatingControls(FloatingPanelId::TOOLBOX);

			OS0WindowHandle const environment =
				addFloatingShell(FloatingPanelId::ENVIRONMENT);
			addArray(environment, gEnvironmentSkillRegions);
			addFloatingControls(FloatingPanelId::ENVIRONMENT);

			OS0WindowHandle const editor =
				addFloatingShell(FloatingPanelId::REALTIME_EDITOR);
			groups.push_back({ editor, {},
				OS0GetRealtimeEditorUI().mouseRegionsBackToFront() });
			addFloatingControls(FloatingPanelId::REALTIME_EDITOR);
		}
		OS0ApplyManagedMouseRegionZOrder(windows, groups);
	}

	size_t RefreshLootWorldItems()
	{
		gLootWorldItems.fill(-1);
		size_t slot = 0;
		if (!gLootVisible || gLootGridNo < 0 || gLootGridNo >= WORLD_MAX)
			return 0;
		for (ITEM_POOL* item = GetItemPool(gLootGridNo, gLootLevel);
			item && slot < gLootWorldItems.size(); item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (!OS0IsContainerContentItem(worldItem)) continue;
			gLootWorldItems[slot++] = item->iItemIndex;
		}
		return slot;
	}

	void SetLootRegionsEnabled(BOOLEAN enabled)
	{
		const BOOLEAN visible = enabled && gLootVisible && !gContextVisible &&
			!gAimAutoCollapsed && !gStackSplitVisible && !gAssetCatalogVisible;
		for (size_t i = 0; i < gLootRegions.size(); ++i)
		{
			MOUSE_REGION& region = gLootRegions[i];
			const UINT16 desiredCursor = gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL;
			if (region.Cursor != desiredCursor) region.ChangeCursor(desiredCursor);
			const BOOLEAN shouldEnable = visible && gLootWorldItems[i] >= 0;
			if (shouldEnable && !(region.uiFlags & MSYS_REGION_ENABLED)) region.Enable();
			else if (!shouldEnable && (region.uiFlags & MSYS_REGION_ENABLED))
				region.Disable();
		}
	}

	void SetBagRegionsEnabled(BOOLEAN enabled)
	{
		// Prime EXTERN_CURSOR before ChangeCursor touches any newly enabled region.
		// RefreshHeldItemCursor is cached, so the normal per-frame path stays cheap.
		RefreshHeldItemCursor();
		OS0WindowManager const& windows = gUIRuntime.windowManager();
		const BOOLEAN bagVisible = gUIRuntime.visible(OS0UIPanel::INVENTORY);
		const BOOLEAN contextVisible = gUIRuntime.visible(OS0UIPanel::CONTEXT);
		const BOOLEAN libraryVisible = gUIRuntime.visible(OS0UIPanel::ASSET_LIBRARY);
		const BOOLEAN catalogVisible = gUIRuntime.visible(OS0UIPanel::ASSET_CATALOG);
		auto setVisible = [enabled, catalogVisible](MOUSE_REGION& r,
			BOOLEAN visible)
		{
			r.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
			if (enabled && visible && !gStackSplitVisible && !catalogVisible) r.Enable();
			else r.Disable();
		};
		auto setCatalogVisible = [enabled](MOUSE_REGION& r, BOOLEAN visible)
		{
			r.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
			if (enabled && visible && !gStackSplitVisible) r.Enable();
			else r.Disable();
		};
		PositionItemTransferIntentRegions();
		// Character creation and the optional RPG inventory share one real,
		// movable window.  It is never opened merely because a merc is selected.
		setVisible(gBagBlock, bagVisible && !gAimAutoCollapsed);
		setVisible(gBagGrabber, bagVisible && !gTutorialActive &&
			!gAimAutoCollapsed);
		// Creation is modal and fixed in screen space; the optional gameplay
		// character sheet remains movable and closable.
		setVisible(gBagClose, bagVisible && !gTutorialActive &&
			!gAimAutoCollapsed);
		setVisible(gContextBlock, contextVisible && !gAimAutoCollapsed);
		setVisible(gGodLibraryBlock, libraryVisible && !gAimAutoCollapsed);
		setVisible(gGodLibraryGrabber, libraryVisible && !gAimAutoCollapsed);
		setVisible(gGodLibraryClose, libraryVisible && !gAimAutoCollapsed);
		for (MOUSE_REGION& r : gGodIconRegions)
			setVisible(r, libraryVisible && !gAimAutoCollapsed &&
				gDebugLibraryMode == DebugLibraryMode::ICONS);
		for (size_t i = 0; i < gAssetLibraryRegions.size(); ++i)
		{
			const BOOLEAN commonTab = i == 6 || i == 7;
			setVisible(gAssetLibraryRegions[i], libraryVisible &&
				!gAimAutoCollapsed &&
				(commonTab || gDebugLibraryMode == DebugLibraryMode::ASSETS));
		}
		setCatalogVisible(gAssetCatalogBlock, catalogVisible && !gAimAutoCollapsed);
		for (MOUSE_REGION& r : gAssetCatalogRegions)
			setCatalogVisible(r, catalogVisible && !gAimAutoCollapsed);
		const BOOLEAN showContentInventory = bagVisible && !contextVisible &&
			!gAimAutoCollapsed;
		const BOOLEAN showContentLoot = gLootVisible && !contextVisible &&
			!gAimAutoCollapsed;
		for (size_t i = 0; i < gFloatingPanels.size(); ++i)
		{
			const BOOLEAN hasPanelContent =
				i != static_cast<size_t>(FloatingPanelId::INSPECTOR) || gHoverVisible;
			const BOOLEAN visible = !gTutorialActive && !gAimAutoCollapsed &&
				windows.visible(gUIRuntime.managedId(
					static_cast<FloatingPanelId>(i))) && hasPanelContent;
			setVisible(gFloatingPanelBlocks[i], visible);
			setVisible(gFloatingPanelGrabbers[i], visible);
			setVisible(gFloatingPanelCloses[i], visible);
		}
		for (MOUSE_REGION& r : gToolboxRegions)
			setVisible(r, windows.visible(gUIRuntime.managedId(
				FloatingPanelId::TOOLBOX)) && !contextVisible && !gTutorialActive);
		for (size_t i = 0; i < gEnvironmentSkillRegions.size(); ++i)
			setVisible(gEnvironmentSkillRegions[i], windows.visible(
				gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT)) &&
				!contextVisible && !gTutorialActive && !gAimAutoCollapsed &&
				i < gEnvironmentEntryCount);
		for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
		{
			setVisible(gPanelDockRegions[i], !gTutorialActive && !gAimAutoCollapsed &&
				!gTalkDocked && gMultiToolExpanded);
		}
		const BOOLEAN dockInputVisible = !gTutorialActive && !gTalkDocked;
		// The physical computer is always the one surviving control. Even while
		// aiming it can be unfolded; a minimized tool must remain exactly one icon.
		setVisible(gOrbRegion, dockInputVisible);
		setVisible(gCombatModeRegion, dockInputVisible && gMultiToolExpanded);
		for (MOUSE_REGION& r : gFeedbackRegions)
			setVisible(r, !gTutorialActive && !contextVisible &&
				windows.visible(gUIRuntime.managedId(FloatingPanelId::SECTOR)) &&
				gSectorPanelMode == SectorPanelMode::REPORT);
		for (MOUSE_REGION& r : gSectorUpgradeRegions)
			setVisible(r, windows.visible(gUIRuntime.managedId(
				FloatingPanelId::SECTOR)) && !contextVisible && !gTutorialActive &&
				gSectorPanelMode == SectorPanelMode::BASE);
		for (MOUSE_REGION& r : gSectorTabRegions)
			setVisible(r, windows.visible(gUIRuntime.managedId(
				FloatingPanelId::SECTOR)) && !contextVisible && !gTutorialActive);
		setVisible(gStrategicMapRegion, windows.visible(gUIRuntime.managedId(
			FloatingPanelId::SECTOR)) && !contextVisible &&
			!gTutorialActive && gSectorPanelMode == SectorPanelMode::MAP);
		setVisible(gSectorTeamRegion, windows.visible(gUIRuntime.managedId(
			FloatingPanelId::SECTOR)) && !contextVisible &&
			!gTutorialActive && gSectorPanelMode == SectorPanelMode::TEAM);
		for (size_t i = 0; i < gContextRegions.size(); ++i)
		{
			gContextRegions[i].ChangeCursor(
				gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
			if (enabled && !gStackSplitVisible && !catalogVisible && contextVisible &&
				i < gContextEntryCount)
				gContextRegions[i].Enable();
			else
				gContextRegions[i].Disable();
		}
		for (size_t i = 0; i < gSlotRegions.size(); ++i)
		{
			MOUSE_REGION& r = gSlotRegions[i];
			r.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
			if (enabled && !gStackSplitVisible && !catalogVisible && bagVisible &&
				!contextVisible &&
				showContentInventory &&
				!gTutorialActive &&
				CanAccessSoldierContents(gInventorySoldier ?
					gInventorySoldier : GetSelectedMan())) r.Enable();
			else r.Disable();
		}
		SetLootRegionsEnabled(enabled && showContentLoot);
		for (MOUSE_REGION& r : gEquipmentRegions)
			setVisible(r, gEquipmentExplodedVisible && gEquipmentSoldier &&
				CanAccessSoldierContents(gEquipmentSoldier) && !contextVisible &&
				!gAimAutoCollapsed);
		setVisible(gEquipmentPackRegion, gEquipmentExplodedVisible &&
			gEquipmentSoldier && gEquipmentSoldier->bTeam == OUR_TEAM &&
			!contextVisible && !gAimAutoCollapsed);
		SOLDIERTYPE* const transferTarget = BoundItemTransferTarget();
		ItemTransferPolicyDecision const transfer =
			CurrentItemTransferDecision(transferTarget);
		if (!transfer.hasAlternatives()) gItemTransferMoreVisible = FALSE;
		for (size_t i = 0; i < gItemTransferIntentRegions.size(); ++i)
		{
			ItemTransferIntent const intent = gOS0ItemTransferIntents[i].intent;
			const BOOLEAN direct = transfer.hasPreferred &&
				transfer.preferred == intent;
			setVisible(gItemTransferIntentRegions[i], !contextVisible &&
				transfer.allows(intent) && (direct || gItemTransferMoreVisible));
		}
		setVisible(gItemTransferMoreRegion, !contextVisible &&
			transfer.hasAlternatives());
		if (enabled && gTutorialActive && !contextVisible && !catalogVisible)
		{
			gTutorialContinue.Enable();
			for (MOUSE_REGION& r : gTutorialStats)
			{
				if (gUIRuntime.creatorStage() == OS0CreatorStage::ATTRIBUTES) r.Enable();
				else r.Disable();
			}
			for (MOUSE_REGION& r : gTutorialBodyRegions)
			{
				if (gUIRuntime.creatorStage() == OS0CreatorStage::IDENTITY) r.Enable();
				else r.Disable();
			}
			for (MOUSE_REGION& r : gTutorialTraitRegions)
			{
				if (gUIRuntime.creatorStage() == OS0CreatorStage::TRAITS) r.Enable();
				else r.Disable();
			}
		}
		else
		{
			gTutorialContinue.Disable();
			for (MOUSE_REGION& r : gTutorialStats) r.Disable();
			for (MOUSE_REGION& r : gTutorialBodyRegions) r.Disable();
			for (MOUSE_REGION& r : gTutorialTraitRegions) r.Disable();
		}
		if (enabled && gStackSplitVisible && !catalogVisible)
		{
			gStackSplitBlock.Enable();
			for (MOUSE_REGION& r : gStackSplitRegions) r.Enable();
		}
		else
		{
			gStackSplitBlock.Disable();
			for (MOUSE_REGION& r : gStackSplitRegions) r.Disable();
		}
		gStackSplitBlock.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
		for (MOUSE_REGION& r : gStackSplitRegions)
			r.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
		if (OS0GetRealtimeEditorUI().initialized())
		{
			OS0GetRealtimeEditorUI().setInputEnabled(enabled &&
				!contextVisible && !gStackSplitVisible && !catalogVisible &&
				!gTutorialActive && !gAimAutoCollapsed && !gpItemPointer);
			OS0GetRealtimeEditorUI().update();
		}
		if (gpItemPointer)
		{
			// Transfer mode has a deliberately small input surface. The slot,
			// equipment, loot and relation regions above remain enabled; every
			// unrelated control is disabled centrally so a drop can never also
			// close, drag, retab or execute the window underneath it.
			gBagGrabber.Disable();
			gBagClose.Disable();
			gGodLibraryGrabber.Disable();
			gGodLibraryClose.Disable();
			for (MOUSE_REGION& r : gGodIconRegions) r.Disable();
			for (MOUSE_REGION& r : gAssetLibraryRegions) r.Disable();
			for (MOUSE_REGION& r : gAssetCatalogRegions) r.Disable();
			for (MOUSE_REGION& r : gContextRegions) r.Disable();
			for (MOUSE_REGION& r : gFloatingPanelGrabbers) r.Disable();
			for (MOUSE_REGION& r : gFloatingPanelCloses) r.Disable();
			for (MOUSE_REGION& r : gToolboxRegions) r.Disable();
			for (MOUSE_REGION& r : gEnvironmentSkillRegions) r.Disable();
			for (MOUSE_REGION& r : gPanelDockRegions) r.Disable();
			for (MOUSE_REGION& r : gFeedbackRegions) r.Disable();
			for (MOUSE_REGION& r : gSectorUpgradeRegions) r.Disable();
			for (MOUSE_REGION& r : gSectorTabRegions) r.Disable();
			gStrategicMapRegion.Disable();
			gSectorTeamRegion.Disable();
			gOrbRegion.Disable();
			gCombatModeRegion.Disable();
		}
		SyncManagedMouseRegionZOrder(windows);
	}

	void PositionLootRegions()
	{
		INT16 objectX = gsVIEWPORT_END_X / 2;
		INT16 objectY = gsVIEWPORT_WINDOW_END_Y / 2;
		if (gLootGridNo >= 0 && gLootGridNo < WORLD_MAX)
		{
			GetGridNoScreenPos(gLootGridNo, gLootLevel, &objectX, &objectY);
			OS0MapWorldToDisplayScreen(&objectX, &objectY);
		}
		constexpr std::array<std::pair<INT16, INT16>, 12> lootOffsets{{
			{ -122, -94 }, { -61, -112 }, { 1, -112 }, { 62, -94 },
			{ -132, -58 }, { 73, -58 }, { -132, -23 }, { 73, -23 },
			{ -122, 14 }, { -61, 30 }, { 1, 30 }, { 62, 14 }
		}};
		for (size_t i = 0; i < gLootRegions.size(); ++i)
		{
			const INT16 x = std::clamp<INT16>(objectX + lootOffsets[i].first,
				gsVIEWPORT_START_X + 2, gsVIEWPORT_END_X - 61);
			const INT16 y = std::clamp<INT16>(objectY + lootOffsets[i].second,
				gsVIEWPORT_WINDOW_START_Y + 2, gsVIEWPORT_WINDOW_END_Y - 30);
			MoveRegion(gLootRegions[i], x, y);
		}
	}

	void PositionBagRegions()
	{
		MoveRegion(gBagBlock, gBagX, gBagY);
		MoveRegion(gBagGrabber, gBagX, gBagY);
		MoveRegion(gBagClose, gBagX + PANE_W - 16, gBagY + 1);
		gInventoryX = gBagX;
		gInventoryY = gBagY;
		for (size_t i = 0; i < gSlots.size(); ++i)
		{
			SlotLayout const& slot = gSlots[i];
			MoveRegion(gSlotRegions[i], gInventoryX + slot.x,
				gInventoryY + slot.y);
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
		for (size_t i = 0; i < gTutorialBodyRegions.size(); ++i)
		{
			MoveRegion(gTutorialBodyRegions[i],
				gBagX + 14 + static_cast<INT16>(i) * 108, gBagY + 88);
		}
		for (size_t i = 0; i < gTutorialTraitRegions.size(); ++i)
		{
			const BOOLEAN right = i >= 8;
			const INT16 row = static_cast<INT16>(right ? i - 8 : i);
			MoveRegion(gTutorialTraitRegions[i],
				gBagX + (right ? 230 : 0) + 14,
				gBagY + 24 + row * 14);
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
		const FloatingPanel& toolboxPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLBOX)];
		for (size_t i = 0; i < gToolboxRegions.size(); ++i)
		{
			const INT16 x = static_cast<INT16>(toolboxPanel.x + 9 + (i % 3) * 39);
			const INT16 y = static_cast<INT16>(toolboxPanel.y + 23 + (i / 3) * 31);
			MoveRegion(gToolboxRegions[i], x, y);
		}
		const FloatingPanel& environmentPanel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ENVIRONMENT)];
		for (size_t i = 0; i < gEnvironmentSkillRegions.size(); ++i)
		{
			const INT16 x = static_cast<INT16>(environmentPanel.x + 8 +
				(i % 3) * 78);
			const INT16 y = static_cast<INT16>(environmentPanel.y + 42 +
				(i / 3) * 29);
			MoveRegion(gEnvironmentSkillRegions[i], x, y);
		}
		MoveRegion(gGodLibraryBlock, gGodLibraryX, gGodLibraryY);
		gGodLibraryBlock.RegionBottomRightX = gGodLibraryX + GOD_LIBRARY_W;
		gGodLibraryBlock.RegionBottomRightY = gGodLibraryY + GOD_LIBRARY_H;
		MoveRegion(gGodLibraryGrabber, gGodLibraryX, gGodLibraryY);
		gGodLibraryGrabber.RegionBottomRightX = gGodLibraryX + GOD_LIBRARY_W;
		MoveRegion(gGodLibraryClose, gGodLibraryX + GOD_LIBRARY_W - 18,
			gGodLibraryY + 2);
		const std::array<SGPBox, 12> libraryRects{{
			{ 8, 49, 196, 54 }, { 216, 49, 196, 54 },
			{ 8, 106, 196, 54 }, { 216, 106, 196, 54 },
			{ 8, 163, 196, 54 }, { 216, 163, 196, 54 },
			{ 8, 24, 68, 18 }, { 80, 24, 82, 18 },
			{ 168, 24, 112, 18 }, { 286, 224, 36, 18 },
			{ 326, 224, 36, 18 }, { 366, 224, 46, 18 }
		}};
		for (size_t i = 0; i < gAssetLibraryRegions.size(); ++i)
		{
			SGPBox const& rect = libraryRects[i];
			MoveRegion(gAssetLibraryRegions[i], gGodLibraryX + rect.x,
				gGodLibraryY + rect.y);
			gAssetLibraryRegions[i].RegionBottomRightX =
				gGodLibraryX + rect.x + rect.w;
			gAssetLibraryRegions[i].RegionBottomRightY =
				gGodLibraryY + rect.y + rect.h;
		}
		for (size_t i = 0; i < gGodIconRegions.size(); ++i)
		{
			// Selectable symbols fill the first eight cells of each JA2
			// 3x3 frame.  The final region occupies the last cell as CLOSE.
			const size_t cell = i < GOD_ICON_COUNT ?
				(i / 8) * 9 + (i % 8) : 26;
			const INT16 frame = static_cast<INT16>(cell / 9);
			const INT16 local = static_cast<INT16>(cell % 9);
			MoveRegion(gGodIconRegions[i],
				gGodLibraryX + 88 + frame * 78 + 9 + (local % 3) * 20,
				gGodLibraryY + 72 + (local / 3) * 20);
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
		constexpr INT16 groupWidth = 44;
		const INT16 unfoldedWidth = static_cast<INT16>(COLLAPSED_OS0_W +
			(PANEL_DOCK_COUNT + 1) * groupWidth);
		// Keep the computer itself under the pointer and unfold towards whichever
		// side has room. Moving it against an edge must never move it a second time.
		const BOOLEAN unfoldRight =
			gOrbX + unfoldedWidth <= gsVIEWPORT_END_X ||
			gOrbX < unfoldedWidth - COLLAPSED_OS0_W;
		for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
		{
			const INT16 x = unfoldRight ?
				static_cast<INT16>(gOrbX + COLLAPSED_OS0_W + groupWidth +
					static_cast<INT16>(i) * groupWidth) :
				static_cast<INT16>(gOrbX -
					static_cast<INT16>(PANEL_DOCK_COUNT + 1 - i) * groupWidth);
			MoveRegion(gPanelDockRegions[i], x, gOrbY);
			gPanelDockRegions[i].RegionBottomRightX = x + groupWidth;
			gPanelDockRegions[i].RegionBottomRightY = gOrbY + COMMAND_BAR_H;
		}
		MoveRegion(gOrbRegion, gOrbX, gOrbY);
		gOrbRegion.RegionBottomRightX = gOrbX + COLLAPSED_OS0_W;
		gOrbRegion.RegionBottomRightY = gOrbY + COMMAND_BAR_H;
		const INT16 combatX = unfoldRight ?
			static_cast<INT16>(gOrbX + COLLAPSED_OS0_W) :
			static_cast<INT16>(gOrbX - groupWidth);
		MoveRegion(gCombatModeRegion, combatX, gOrbY);
		gCombatModeRegion.RegionBottomRightX = combatX + 44;
		gCombatModeRegion.RegionBottomRightY = gOrbY + COMMAND_BAR_H;
		const FloatingPanel& sector =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		const INT16 reportX = sector.x;
		const INT16 reportY = sector.y + 22;
		const INT16 reportW = sector.w;
		const INT16 reportH = sector.h - 22;
		MoveRegion(gFeedbackRegions[0], reportX + 8, reportY + 24);
		MoveRegion(gFeedbackRegions[1], reportX + 8, reportY + 49);
		MoveRegion(gFeedbackRegions[2], reportX + reportW - 126,
			reportY + reportH - 28);
		MoveRegion(gFeedbackRegions[3], reportX + 8,
			reportY + reportH - 28);
		for (size_t i = 0; i < gSectorTabRegions.size(); ++i)
			MoveRegion(gSectorTabRegions[i], sector.x + 8 + static_cast<INT16>(i) * 72,
				sector.y + 23);
		for (size_t i = 0; i < gSectorUpgradeRegions.size(); ++i)
			MoveRegion(gSectorUpgradeRegions[i], sector.x + 8,
				sector.y + 72 + static_cast<INT16>(i) * 39);
		MoveRegion(gStrategicMapRegion, sector.x + 12, sector.y + 55);
		MoveRegion(gSectorTeamRegion, sector.x + 8, sector.y + 55);
		gStackSplitX = std::max<INT16>(0, (gsVIEWPORT_END_X - 224) / 2);
		gStackSplitY = std::max<INT16>(gsVIEWPORT_WINDOW_START_Y,
			(gsVIEWPORT_WINDOW_END_Y - 82) / 2);
		gStackSplitBlock.RegionTopLeftX = 0;
		gStackSplitBlock.RegionTopLeftY = 0;
		gStackSplitBlock.RegionBottomRightX = gsVIEWPORT_END_X;
		gStackSplitBlock.RegionBottomRightY = gsVIEWPORT_END_Y;
		constexpr std::array<INT16, 5> splitX{{ 8, 43, 78, 122, 173 }};
		constexpr std::array<INT16, 5> splitW{{ 30, 30, 39, 46, 43 }};
		for (size_t i = 0; i < gStackSplitRegions.size(); ++i)
		{
			gStackSplitRegions[i].RegionTopLeftX = gStackSplitX + splitX[i];
			gStackSplitRegions[i].RegionTopLeftY = gStackSplitY + 51;
			gStackSplitRegions[i].RegionBottomRightX =
				gStackSplitX + splitX[i] + splitW[i];
			gStackSplitRegions[i].RegionBottomRightY = gStackSplitY + 74;
		}
	}

	BOOLEAN FinishWindowDrag()
	{
		OS0WindowManager& windows = gUIRuntime.windowManager();
		if (windows.draggingWindow() == OS0_INVALID_WINDOW) return FALSE;
		windows.endDrag();
		PositionBagRegions();
		OS0GetRealtimeEditorUI().update();
		SaveUILayout();
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	BOOLEAN UpdateWindowDragging()
	{
		OS0WindowManager& windows = gUIRuntime.windowManager();
		if (windows.draggingWindow() != OS0_INVALID_WINDOW &&
			!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMainFingerDown())
		{
			// MouseSystem does not send POINTER_UP back to a pressed region after
			// the pointer leaves it. Physical button state is the authoritative
			// capture boundary, so end and persist the drag here as well.
			return FinishWindowDrag();
		}
		BOOLEAN const moved = windows.dragTo(
			gusMouseXPos, gusMouseYPos);
		if (moved)
		{
			// Synchronize all hit regions exactly once per rendered frame. Updating
			// them for every raw mouse event made regions and pixels race each other.
			PositionBagRegions();
			OS0GetRealtimeEditorUI().update();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		return moved;
	}

	BOOLEAN UpdateMultiToolDragging()
	{
		if (!gMultiToolDragCandidate) return FALSE;
		BOOLEAN const physicallyDown =
			IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown();
		if (!physicallyDown)
		{
			BOOLEAN const changed = gMultiToolDragging;
			gMultiToolDragCandidate = FALSE;
			gMultiToolDragging = FALSE;
			if (changed)
			{
				PositionBagRegions();
				SaveUILayout();
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
			}
			return changed;
		}
		if (!gMultiToolDragging &&
			(std::abs(gusMouseXPos - gMultiToolDragStartX) >= 4 ||
			 std::abs(gusMouseYPos - gMultiToolDragStartY) >= 4))
		{
			// Promote outside the original region as well.  Touch does not populate
			// MOUSE_REGION::ButtonState, and a fast mouse move can leave the orb
			// before MouseSystem dispatches another in-region MOVE callback.
			gMultiToolDragging = TRUE;
			gMultiToolLastClickAt = 0;
		}
		if (!gMultiToolDragging) return FALSE;
		INT16 const oldX = gOrbX;
		INT16 const oldY = gOrbY;
		gOrbX = static_cast<INT16>(gMultiToolDragOriginX +
			gusMouseXPos - gMultiToolDragStartX);
		gOrbY = static_cast<INT16>(gMultiToolDragOriginY +
			gusMouseYPos - gMultiToolDragStartY);
		ClampMultiToolPosition();
		if (gOrbX == oldX && gOrbY == oldY) return FALSE;
		PositionBagRegions();
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	void BagBlockCallback(MOUSE_REGION*, UINT32 reason)
	{
		// A panel background is an explicit non-target. It consumes the complete
		// held-item gesture but leaves the native item cursor intact. This prevents
		// blank pixels from forwarding the same UP to a world drop behind the panel.
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && gpItemPointer)
		{
			transfers.reconcile(TRUE);
			transfers.beginHeldGesture();
		}
		if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
			transfers.claimRelease(OS0ItemTransferSurface::RELATION) ==
				OS0ItemReleaseClaim::ITEM)
			transfers.completeItemRelease(TRUE);
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && !gpItemPointer)
			transfers.claimRelease(OS0ItemTransferSurface::RELATION);
	}

	void BagGrabberCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			if (gUIRuntime.windowManager().beginDrag(
				gUIRuntime.managedId(OS0UIPanel::INVENTORY),
				gusMouseXPos, gusMouseYPos))
			{
				// Rendering and native JA2 hit testing must switch focus in the
				// same pointer event, otherwise an overlapping old front window
				// can steal the capture before the next tactical frame.
				SyncManagedMouseRegionZOrder(gUIRuntime.windowManager());
			}
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
			SetRenderFlags(RENDER_FLAG_FULL);
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			FinishWindowDrag();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void BagCloseCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		gUIRuntime.hide(OS0UIPanel::INVENTORY);
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
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			if (gUIRuntime.windowManager().beginDrag(gUIRuntime.managedId(
				static_cast<FloatingPanelId>(index)), gusMouseXPos, gusMouseYPos))
				SyncManagedMouseRegionZOrder(gUIRuntime.windowManager());
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
			SetRenderFlags(RENDER_FLAG_FULL);
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			FinishWindowDrag();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void FloatingPanelCloseCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gFloatingPanels.size()) return;
		gUIRuntime.windowManager().hide(gUIRuntime.managedId(
			static_cast<FloatingPanelId>(index)));
		if (index == static_cast<size_t>(FloatingPanelId::INSPECTOR))
		{
			gInspectorPinned = FALSE;
			gHoverVisible = FALSE;
		}
		if (index == static_cast<size_t>(FloatingPanelId::SECTOR))
			StopFeedbackEditing();
		if (index == static_cast<size_t>(FloatingPanelId::REALTIME_EDITOR))
			ApplyCursorTool(ContextAction::MOVE);
		SaveUILayout();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void PanelDockCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || gTutorialActive) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gPanelDockRegions.size()) return;
		// The dock changes workspace. Release any transient radial first so the
		// requested module cannot remain invisibly suspended behind it.
		CloseContextMenu();
		switch (index)
		{
			case 0: // Interaction / perceive nearby object relations.
				ActivateToolboxModule(ToolboxModule::OBJECT);
				break;
			case 1: // Salvage and environment abilities.
				ActivateToolboxModule(ToolboxModule::WORLD);
				break;
			case 2: // Inspect exactly what is currently under the pointer.
				if (gHoverCursorSoldier ||
					(gHoverCursorGridNo >= 0 && gHoverCursorGridNo < WORLD_MAX))
				{
					OS0SelectWorldObject(gHoverCursorSoldier, gHoverCursorGridNo,
						gHoverCursorLevel, gHoverCursorTileIndex);
				}
				else
				{
					gUIRuntime.windowManager().toggle(
						gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
				}
				break;
			case 3: // Character information and RPG inventory.
				OS0OpenCharacterPanel(gHoverCursorSoldier ?
					gHoverCursorSoldier : GetSelectedMan());
				break;
			case 4:
				ActivateToolboxModule(ToolboxModule::SANDBOX);
				break;
			default: break;
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
		const BOOLEAN changed = index == 0 && index < gSectorUpgrades.size() &&
			HasUpgrade(gSectorUpgrades[index]) ?
			RecoverTeamAtFieldShelter() : BuildSectorUpgrade(index);
		if (changed)
		{
			fInterfacePanelDirty = DIRTYLEVEL2;
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void SectorTabCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gSectorTabRegions.size()) return;
		gSectorPanelMode = static_cast<SectorPanelMode>(index);
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void StrategicMapCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		const INT16 column = (gusMouseXPos - (panel.x + 12)) / STRATEGIC_CELL;
		const INT16 row = (gusMouseYPos - (panel.y + 55)) / STRATEGIC_CELL;
		if (column < 0 || column >= 16 || row < 0 || row >= 16) return;
		gStrategicSelectedSector = SGPSector(column + 1, row + 1, 0);
		RecordFeedbackEvent(ST::format("STRATEGIC SELECT {}",
			gStrategicSelectedSector.AsShortString()));
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void SectorTeamCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		const INT16 requestedRow = (gusMouseYPos - (panel.y + 55)) / 18;
		if (requestedRow < 0 || requestedRow >= 8) return;
		INT16 visibleRow = 0;
		for (INT32 id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
			id <= gTacticalStatus.Team[OUR_TEAM].bLastID && visibleRow < 8; ++id)
		{
			SOLDIERTYPE& soldier = GetMan(id);
			if (!soldier.bActive || soldier.bLife <= 0) continue;
			if (visibleRow++ != requestedRow) continue;
			if (soldier.sSector == gWorldSector && !soldier.fBetweenSectors)
			{
				SelectSoldier(&soldier, SELSOLDIER_FORCE_RESELECT);
				LocateSoldier(&soldier, DONTSETLOCATOR);
				BindInspectedSoldier(&soldier);
				BindInventorySoldier(&soldier);
			}
			break;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ApplyCursorTool(ContextAction action)
	{
		if (gPendingWorldAction.active())
			CancelPendingWorldAction("CURSOR OVERRIDE");
		// Release the previous physical owner before installing the next intent.
		// CancelWorldMoveState deliberately normalizes to MOVE; doing it after this
		// assignment used to erase ATTACK/DIG/SALVAGE/USE immediately.
		if (!OS0IsManipulationAction(action) && CarryState().active())
			CancelWorldMoveState();
		CursorState().action = action;
		SetInteractionForAction(action);
		// Native cursor/action state is now projected as a pure function of this
		// control intent every frame. No pending latch can survive a menu mode and
		// steal future primary clicks.
		gNextCombatProjectionAt = 0;
		SetRenderFlags(RENDER_FLAG_FULL);
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
		gDeferredContextIdentity = InventoryContextIdentity(soldier, slot);
		if (gDeferredContextIdentity.kind == DeferredContextKind::NONE) return;
		gObjectActionFanVisible = TRUE;
		ItemModel const* const item = GCM->getItem(soldier->inv[slot].usItem);
		gContextTitle = item->getName();

		const BOOLEAN own = soldier->bTeam == OUR_TEAM;
		AddContextEntry(ContextAction::DETAILS, "DETAILS / ATTACHMENTS");
		if (own)
		{
			if (OS0IsResourceItem(soldier->inv[slot].usItem))
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
		gUIRuntime.show(OS0UIPanel::CONTEXT);
		SetContextHubModal(TRUE);
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
		SOLDIERTYPE* const actor = BoundLootActor();
		gDeferredContextIdentity = WorldItemContextIdentity(actor, itemIndex,
			gLootTileIndex);
		if (gDeferredContextIdentity.kind == DeferredContextKind::NONE) return;
		gContextSoldier = nullptr;
		gContextGridNo = gLootGridNo;
		gContextLevel = gLootLevel;
		gContextTileIndex = gLootTileIndex;
		gContextWorldItemIndex = itemIndex;
		gObjectActionFanVisible = TRUE;
		ItemModel const* const item = GCM->getItem(worldItem.o.usItem);
		gContextTitle = item->getName();
		const BOOLEAN near = IsInspectedWorldAssetNear();
		AddContextEntry(ContextAction::DETAILS, "DETAILS / ATTACHMENTS");
		const BOOLEAN resource = OS0IsResourceItem(worldItem.o.usItem);
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
		gUIRuntime.show(OS0UIPanel::CONTEXT);
		SetContextHubModal(TRUE);
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ContextActionCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gContextVisible) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gContextEntryCount || !gContextEntries[index].enabled) return;
		ContextEntry const entry = gContextEntries[index];
		if (!DeferredContextStillValid(entry.deferredIdentity))
		{
			const ST::string subject = entry.kind == ContextEntryKind::ACTION &&
				entry.action != ContextAction::COUNT ?
				ContextActionName(entry.action) : "CONTEXT";
			RecordFeedbackEvent(ST::format("{} / TARGET CHANGED", subject));
			CloseContextMenu();
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		SOLDIERTYPE* const deferredActor =
			DeferredContextActor(entry.deferredIdentity);
		gDeferredContextIdentity = entry.deferredIdentity;
		switch (entry.deferredIdentity.kind)
		{
			case DeferredContextKind::CHARACTER:
				gContextSoldier = deferredActor;
				break;
			case DeferredContextKind::INVENTORY_ITEM:
				gContextSoldier = deferredActor;
				gContextInventorySlot = entry.deferredIdentity.inventorySlot;
				gContextWorldItemIndex = -1;
				break;
			case DeferredContextKind::WORLD_ITEM:
				gContextSoldier = nullptr;
				gContextGridNo = entry.deferredIdentity.gridNo;
				gContextLevel = entry.deferredIdentity.level;
				gContextTileIndex = entry.deferredIdentity.tileIndex;
				gContextWorldItemIndex = entry.deferredIdentity.worldItemIndex;
				break;
			case DeferredContextKind::NONE:
				break;
		}
		if (entry.kind == ContextEntryKind::CATEGORY)
		{
			BuildCharacterContextPage(gContextSoldier, entry.category);
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		if (entry.kind == ContextEntryKind::BACK)
		{
			BuildCharacterContextPage(gContextSoldier);
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}

		const ContextAction action = entry.action;
		RecordFeedbackEvent(ST::format("ACTION {} grid {} tile {}",
			ContextActionName(action), gContextGridNo, gContextTileIndex));
		if (entry.binding.kind != OS0InteractionTargetKind::NONE)
		{
			SOLDIERTYPE* const actionActor =
				entry.deferredIdentity.kind == DeferredContextKind::WORLD_ITEM ?
					deferredActor : nullptr;
			OS0ResolvedAction current;
			if (!ResolveBoundAction(entry.binding, action, current, actionActor))
			{
				RecordFeedbackEvent(ST::format("{} / TARGET CHANGED",
					ContextActionName(action)));
				CloseContextMenu();
				return;
			}
			if (entry.binding.kind != OS0InteractionTargetKind::ACTOR)
			{
				const BOOLEAN accepted =
					ExecuteOrQueueBoundAction(current, actionActor);
				if (accepted && action == ContextAction::CONTENTS)
					NotifyFieldTutorial(
						OS0FieldTutorialEvent::CONTENTS_SELECTED, entry.binding);
				if (!accepted)
					RecordFeedbackEvent(ST::format("{} / ACTION FAILED",
						ContextActionName(action)));
				CloseContextMenu();
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
		}
		SOLDIERTYPE* const selected =
			entry.deferredIdentity.kind == DeferredContextKind::WORLD_ITEM ?
			deferredActor : GetSelectedMan();
		SOLDIERTYPE* const subject = gContextSoldier ?
			gContextSoldier : selected;
		switch (action)
		{
			case ContextAction::MOVE:
				ApplyCursorTool(ContextAction::MOVE);
				break;
			case ContextAction::USE:
				if (gContextSoldier)
				{
					OS0OpenCharacterPanel(gContextSoldier);
				}
				else if (gContextGridNo >= 0 && gContextGridNo < WORLD_MAX)
				{
					const GridNo gridNo = gContextGridNo;
					const UINT8 level = gContextLevel;
					const UINT16 tileIndex = gContextTileIndex;
					CloseContextMenu();
					OS0ActivateWorldObject(gridNo, level, tileIndex);
					return;
				}
				else
				{
					ApplyCursorTool(ContextAction::USE);
				}
				break;
			case ContextAction::INSPECT:
				if (gContextSoldier)
				{
					OS0OpenCharacterPanel(gContextSoldier);
					return;
				}
				break;
			case ContextAction::CONTENTS:
				if (gContextSoldier)
				{
					const BOOLEAN contentsAvailable =
						CanAccessSoldierContents(gContextSoldier);
					if (gContextSoldier->bTeam == OUR_TEAM)
					{
						BindInventorySoldier(gContextSoldier);
						const BOOLEAN opening = contentsAvailable &&
							(!gEquipmentExplodedVisible ||
							 gEquipmentSoldier != gContextSoldier);
						BindEquipmentSoldier(gContextSoldier);
						gEquipmentAutoForHeldItem = FALSE;
						gEquipmentExplodedVisible = opening;
						// Equipment lives around the actor. PACK is the explicit
						// gateway to the pocket/container window.
						gBagVisible = FALSE;
					}
					else
					{
						// Bodies use the same actor-centred equipment projection as
						// player characters; there is no second loot-window model.
						BindEquipmentSoldier(gContextSoldier);
						gEquipmentAutoForHeldItem = FALSE;
						gEquipmentExplodedVisible = contentsAvailable;
						gBagVisible = FALSE;
					}
				}
				else
				{
					// Right-click, the persistent action panel and double-click
					// must all use the same open path.  Merely exposing the panel
					// skipped deterministic container seeding and made this entry
					// appear empty while double-clicking the same crate worked.
					const GridNo gridNo = gContextGridNo;
					const UINT8 level = gContextLevel;
					const UINT16 tileIndex = gContextTileIndex;
					CloseContextMenu();
					OS0OpenWorldContainer(gridNo, level, tileIndex);
					return;
				}
				break;
			case ContextAction::BUILD:
				gUIRuntime.windowManager().show(
					gUIRuntime.managedId(FloatingPanelId::SECTOR));
				break;
			case ContextAction::CARRY:
			case ContextAction::PUSH:
			case ContextAction::PULL:
			case ContextAction::THROW:
				BeginInspectedWorldMove(CarryModeForAction(action));
				break;
			case ContextAction::TALK:
				guiPendingOverrideEvent = T_CHANGE_TO_TALKING;
				break;
			case ContextAction::ATTACK:
				// Enter the same persistent attack state used by the middle-click
				// cursor cycle. Hover must not immediately replace it with INSPECT.
				ApplyCursorTool(ContextAction::ATTACK);
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
					if (EquipWorldItemDirectly(selected, gContextWorldItemIndex,
						gContextTileIndex, TRUE))
						ChangeWeaponMode(selected);
				}
				else if (subject && subject->bTeam == OUR_TEAM)
				{
					BOOLEAN weaponReady = TRUE;
					if (gContextInventorySlot != NO_SLOT &&
						gContextInventorySlot != HANDPOS)
						weaponReady = EquipInventorySlotAtomically(subject,
							gContextInventorySlot);
					if (weaponReady) ChangeWeaponMode(subject);
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
				OBJECTTYPE const gunBefore = gun ? *gun : OBJECTTYPE{};
				OBJECTTYPE ammo{};
				if (gun && !gpItemPointer &&
					EmptyWeaponMagazine(gun, &ammo) && ammo.usItem != NOTHING)
				{
					const BOOLEAN packReady = selected &&
						OS0CanPackObject(selected, ammo);
					if (packReady)
						PlaceObjectCompletelyInActorPack(selected, &ammo);
					if (ammo.usItem != NOTHING && selected && !gpItemPointer)
					{
						// A magazine is a newly detached physical object, not a UI
						// side effect. If the pack cannot accept it, keep the exact
						// remainder on the cursor for an explicit destination.
						InternalBeginItemPointer(selected, &ammo, NO_SLOT);
						if (gpItemPointer)
						{
							OS0GetItemTransferController().
								adoptExternalHeldItemAfterHandledRelease();
							BindHeldItemCarrier(selected);
							RecordFeedbackEvent(
								"PACK FULL / UNLOADED MAGAZINE HELD");
						}
					}
					if (ammo.usItem != NOTHING && !gpItemPointer)
					{
						const GridNo dropGrid = selected ?
							selected->sGridNo : gContextGridNo;
						const INT32 dropped = dropGrid >= 0 && dropGrid < WORLD_MAX ?
							AddItemToPool(dropGrid, &ammo, VISIBLE,
								selected ? selected->bLevel : gContextLevel, 0, -1) : -1;
						if (dropped >= 0)
						{
							OS0NotifyWorldMutation();
						}
						else
						{
							// Packing is atomic and the cursor never accepted the
							// magazine, so restoring the exact weapon snapshot is the
							// only remaining ownership-preserving result.
							if (gun) *gun = gunBefore;
							ammo = {};
							RecordFeedbackEvent(
								"UNLOAD FAILED / MAGAZINE RESTORED");
						}
					}
				}
				break;
			}
			case ContextAction::DETAILS:
			{
				OBJECTTYPE const* detailObject = nullptr;
				if (gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists) detailObject = &worldItem.o;
				}
				else if (subject && gContextInventorySlot != NO_SLOT)
				{
					detailObject = &subject->inv[gContextInventorySlot];
				}
			if (detailObject && detailObject->usItem != NOTHING)
			{
				const ST::string detailsName =
					GCM->getItem(detailObject->usItem)->getName();
				// Release the radial first: its normal close clears transient hover
				// content.  Publish the pinned details afterwards so they survive.
				CloseContextMenu();
				gHoverTitle = detailsName;
				gHoverDetail = ST::format("ITEM {} / CONDITION {}% / STACK {}",
					detailObject->usItem, detailObject->bStatus[0],
					detailObject->ubNumberOfObjects);
				gHoverDebugDetail = "RMB ACTIONS / DRAG TO CHARACTER OR CONTAINER";
				gHoverVisible = TRUE;
				gInspectorPinned = TRUE;
				gUIRuntime.windowManager().show(gUIRuntime.managedId(
					FloatingPanelId::INSPECTOR));
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
				break;
			}
			case ContextAction::EQUIP_ITEM:
				if (selected && gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					EquipWorldItemDirectly(selected, gContextWorldItemIndex,
						gContextTileIndex, TRUE);
				}
				else if (subject && subject->bTeam == OUR_TEAM &&
					gContextInventorySlot != NO_SLOT)
					EquipInventorySlotAtomically(subject, gContextInventorySlot);
				break;
			case ContextAction::MOVE_ITEM:
				if (!gpItemPointer && selected && gContextWorldItemIndex >= 0 &&
					static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
				{
					BeginTrackedWorldItemTransfer(selected, gContextWorldItemIndex,
						gContextTileIndex, TRUE);
				}
				else if (subject && gContextInventorySlot != NO_SLOT &&
					subject->inv[gContextInventorySlot].usItem != NOTHING)
				{
					if (selected && CanAccessSoldierContents(subject) && !gpItemPointer)
					{
						OBJECTTYPE object{};
						GetObjFrom(&subject->inv[gContextInventorySlot], 0, &object);
						if (object.usItem != NOTHING)
							BeginTrackedInventoryItemTransfer(subject,
								gContextInventorySlot, selected, object, TRUE);
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
					MoveInventoryItemToPackAtomically(gContextSoldier,
						gContextInventorySlot, selected);
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
							if (OS0IsResourceItem(worldItem.o.usItem))
							{
								StoreResourceWorldItem(selected, itemIndex);
								break;
							}
							BOOLEAN accepted =
								OS0CanAcceptCarriedObject(selected, worldItem.o);
							if (accepted && OS0CanPackObject(selected, worldItem.o))
							{
								if (!OS0SoldierPickupExactWorldItem(selected, itemIndex,
									gContextGridNo, ITEM_IGNORE_Z_LEVEL))
								{
									accepted = FALSE;
									RecordFeedbackEvent(
										"PICKUP FAILED / ITEM CHANGED");
								}
							}
							else if (accepted && BeginTrackedWorldItemTransfer(selected,
								itemIndex, gContextTileIndex, TRUE))
								RecordFeedbackEvent(
									"PACK FULL / ITEM HELD FOR PLACEMENT");
							else
								RecordFeedbackEvent(
									accepted ? "PICKUP FAILED / ITEM CHANGED" :
									"LOAD LIMIT 125% / PICKUP REJECTED");
							if (accepted)
							{
								gInspectedGridNo = NOWHERE;
								gInspectedTileIndex = NO_TILE;
								gLootVisible = FALSE;
							}
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
					RefreshEnvironmentTarget(gContextGridNo, 0, NO_TILE);
				}
				break;
			case ContextAction::SALVAGE:
				if (selected && SalvageWorldAsset(selected, gContextGridNo,
					gContextLevel, gContextTileIndex))
					RefreshEnvironmentTarget(gContextGridNo, gContextLevel, NO_TILE);
				break;
			case ContextAction::CATALOG:
				OpenAssetCatalog(gContextGridNo, gContextLevel, gContextTileIndex);
				break;
			case ContextAction::TAKE_COVER:
				CommandRunToCover(subject);
				break;
			case ContextAction::AUTO_FIRST_AID:
				if (CanAutoBandage(FALSE)) BeginAutoBandage();
				break;
			case ContextAction::PREVIOUS_SQUAD:
				SelectAdjacentSquad(-1);
				break;
			case ContextAction::NEXT_SQUAD:
				SelectAdjacentSquad(1);
				break;
			case ContextAction::TEAM:
				gSectorPanelMode = SectorPanelMode::TEAM;
				gUIRuntime.windowManager().show(
					gUIRuntime.managedId(FloatingPanelId::SECTOR));
				break;
			case ContextAction::END_TURN:
				if (gTacticalStatus.uiFlags & INCOMBAT) gfBeginEndTurn = TRUE;
				break;
			case ContextAction::GOD_ASSETS:
				ActivateToolboxModule(ToolboxModule::ASSETS);
				break;
			case ContextAction::GOD_EDITOR:
			{
				// A leaf labelled "LIVE WORLD EDITOR" is an open command, not a
				// toggle which unexpectedly closes an already requested editor.
				InteractionMode().beginInteraction(
					OS0InteractionSurface::ENVIRONMENT);
				OS0WindowHandle const editor = gUIRuntime.managedId(
					OS0UIWindow::REALTIME_EDITOR);
				gUIRuntime.windowManager().show(editor);
				if (gpItemPointer) CancelItemPointer();
				ApplyCursorTool(ContextAction::INSPECT);
				break;
			}
			case ContextAction::GOD_ICONS:
				ActivateToolboxModule(ToolboxModule::SANDBOX);
				break;
			case ContextAction::GOD_TOOLS:
				EnsureDebugFieldTools(subject);
				break;
			case ContextAction::GOD_REVIVE:
				RestoreOperatorForGod(subject);
				break;
			case ContextAction::COUNT:
				break;
		}
		CloseContextMenu();
		SetBagRegionsEnabled(TRUE);
		fInterfacePanelDirty = DIRTYLEVEL2;
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void BeginStackSplit(SOLDIERTYPE* soldier, INT8 slot)
	{
		if (!soldier || slot < 0 || slot >= NUM_INV_SLOTS ||
			soldier->inv[slot].ubNumberOfObjects <= 1) return;
		DeferredContextIdentity const identity =
			InventoryContextIdentity(soldier, slot);
		if (identity.kind != DeferredContextKind::INVENTORY_ITEM) return;
		CloseContextMenu();
		gStackSplitSoldier = soldier;
		gStackSplitSlot = slot;
		gStackSplitIdentity = identity;
		gStackSplitAmount = 1;
		gUIRuntime.show(OS0UIPanel::STACK_SPLIT);
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void CloseStackSplit()
	{
		gUIRuntime.hide(OS0UIPanel::STACK_SPLIT);
		gStackSplitSoldier = nullptr;
		gStackSplitSlot = NO_SLOT;
		gStackSplitIdentity = {};
		gStackSplitAmount = 1;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ConfirmStackSplit()
	{
		if (!gStackSplitSoldier || gStackSplitSlot < 0 ||
			gStackSplitSlot >= NUM_INV_SLOTS || gpItemPointer) return;
		if (!DeferredContextStillValid(gStackSplitIdentity) ||
			DeferredContextActor(gStackSplitIdentity) != gStackSplitSoldier ||
			gStackSplitIdentity.inventorySlot != gStackSplitSlot)
		{
			RecordFeedbackEvent("STACK MOVE ABORTED / SOURCE CHANGED");
			CloseStackSplit();
			return;
		}
		OBJECTTYPE& source = gStackSplitSoldier->inv[gStackSplitSlot];
		const UINT8 amount = std::min(gStackSplitAmount,
			source.ubNumberOfObjects);
		if (amount == 0) return;
		OBJECTTYPE moving{};
		for (UINT8 i = 0; i < amount; ++i)
		{
			OBJECTTYPE one{};
			GetObjFrom(&source, 0, &one);
			if (i == 0) moving = one;
			else StackObjs(&one, &moving, 1);
		}
		SOLDIERTYPE* const soldier = gStackSplitSoldier;
		const INT8 slot = gStackSplitSlot;
		gUIRuntime.hide(OS0UIPanel::STACK_SPLIT);
		gStackSplitSoldier = nullptr;
		gStackSplitSlot = NO_SLOT;
		gStackSplitIdentity = {};
		if (BeginTrackedInventoryItemTransfer(soldier, slot, soldier, moving, TRUE))
			RecordFeedbackEvent(ST::format("STACK MOVE {} OBJECTS", amount));
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void StackSplitCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) ||
			!gStackSplitVisible || !gStackSplitSoldier ||
			gStackSplitSlot < 0) return;
		if (!DeferredContextStillValid(gStackSplitIdentity) ||
			DeferredContextActor(gStackSplitIdentity) != gStackSplitSoldier)
		{
			CloseStackSplit();
			return;
		}
		const size_t action = static_cast<size_t>(region->GetUserData<0>());
		const UINT8 maximum = gStackSplitSoldier->inv[gStackSplitSlot].ubNumberOfObjects;
		switch (action)
		{
			case 0: if (gStackSplitAmount > 1) --gStackSplitAmount; break;
			case 1: if (gStackSplitAmount < maximum) ++gStackSplitAmount; break;
			case 2: gStackSplitAmount = maximum; break;
			case 3: ConfirmStackSplit(); return;
			case 4: CloseStackSplit(); return;
			default: return;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void EquipmentPackCallback(MOUSE_REGION*, UINT32 reason)
	{
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && gpItemPointer)
		{
			transfers.reconcile(TRUE);
			transfers.beginHeldGesture();
			return;
		}
		if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (transfers.claimRelease(OS0ItemTransferSurface::RELATION) !=
				OS0ItemReleaseClaim::ITEM) return;
			if (gEquipmentExplodedVisible && gEquipmentSoldier &&
				CurrentItemTransferDecision(gEquipmentSoldier).allows(
					ItemTransferIntent::PACK))
				ApplyItemTransferIntent(gEquipmentSoldier, ItemTransferIntent::PACK);
			else
				RecordFeedbackEvent("PACK / ITEM DOES NOT FIT");
			transfers.completeItemRelease(gpItemPointer != nullptr);
			return;
		}
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) ||
			!gEquipmentExplodedVisible || !gEquipmentSoldier) return;
		BindInventorySoldier(gEquipmentSoldier);
		gUIRuntime.toggle(OS0UIPanel::INVENTORY);
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	BOOLEAN DropPointerAtActor(SOLDIERTYPE* actor)
	{
		if (!gpItemPointer || !actor) return FALSE;
		if (AddItemToPool(actor->sGridNo, gpItemPointer, VISIBLE,
			actor->bLevel, 0, -1) < 0)
		{
			RecordFeedbackEvent("DROP FAILED / ITEM KEPT ON CURSOR");
			return FALSE;
		}
		OS0NotifyWorldMutation();
		NotifySoldiersToLookforItems();
		FinishCommittedItemPointer();
		return TRUE;
	}

	INT8 ItemTransferIntentSlot(SOLDIERTYPE const* actor, ItemTransferIntent intent)
	{
		if (!actor || !gpItemPointer || gpItemPointer->usItem == NOTHING)
			return NO_SLOT;
		switch (intent)
		{
			case ItemTransferIntent::PRIMARY_HAND: return HANDPOS;
			case ItemTransferIntent::SECONDARY_HAND: return SECONDHANDPOS;
			case ItemTransferIntent::BODY:
			{
				const INT8 slot = OS0PreferredEquipmentSlot(actor, *gpItemPointer);
				return slot == HANDPOS || slot == SECONDHANDPOS ? NO_SLOT : slot;
			}
			case ItemTransferIntent::PACK:
			case ItemTransferIntent::DROP: return NO_SLOT;
		}
		return NO_SLOT;
	}

	BOOLEAN CanPlaceObjectCompletelyInActorSlot(SOLDIERTYPE* actor,
		INT8 const slot, OBJECTTYPE const& object)
	{
		if (!actor || slot < 0 || slot >= NUM_INV_SLOTS ||
			object.usItem == NOTHING || object.ubNumberOfObjects == 0) return FALSE;
		OBJECTTYPE preview = object;
		if (!CanItemFitInPosition(actor, &preview, slot, FALSE)) return FALSE;
		OBJECTTYPE const& target = actor->inv[slot];
		if (target.usItem != NOTHING)
		{
			if (target.usItem != object.usItem) return FALSE;
			const UINT8 limit = ItemSlotLimit(object.usItem, slot);
			if (limit == 0 || target.ubNumberOfObjects +
				object.ubNumberOfObjects > limit) return FALSE;
			if (GCM->getItem(target.usItem)->isKey() &&
				target.ubKeyID != object.ubKeyID) return FALSE;
		}
		return !(slot == HANDPOS &&
			GCM->getItem(object.usItem)->isTwoHanded() &&
			actor->inv[SECONDHANDPOS].usItem != NOTHING);
	}

	BOOLEAN RestoreDetachedInventoryObject(SOLDIERTYPE* actor, INT8 const slot,
		OBJECTTYPE* object)
	{
		if (!actor || !object || object->usItem == NOTHING ||
			!CanPlaceObjectCompletelyInActorSlot(actor, slot, *object)) return FALSE;
		OBJECTTYPE& target = actor->inv[slot];
		const OBJECTTYPE targetBefore = target;
		const OBJECTTYPE objectBefore = *object;
		if (!PlaceObject(actor, slot, object)) return FALSE;
		if (object->usItem == NOTHING || object->ubNumberOfObjects == 0) return TRUE;
		target = targetBefore;
		*object = objectBefore;
		return FALSE;
	}

	BOOLEAN BeginTrackedInventoryItemTransfer(SOLDIERTYPE* source,
		INT8 const slot, SOLDIERTYPE* cursorActor, OBJECTTYPE& detached,
		BOOLEAN const releaseAlreadyHandled)
	{
		if (!source || slot < 0 || slot >= NUM_INV_SLOTS ||
			detached.usItem == NOTHING) return FALSE;
		if (!cursorActor || gpItemPointer)
		{
			RestoreDetachedInventoryObject(source, slot, &detached);
			return FALSE;
		}
		if (CarryState().active()) CancelWorldMoveState();
		InternalBeginItemPointer(cursorActor, &detached,
			source == cursorActor ? slot : NO_SLOT);
		if (!gpItemPointer)
		{
			if (!RestoreDetachedInventoryObject(source, slot, &detached))
				RecordFeedbackEvent(
					"ITEM MOVE FAILED / SOURCE RESTORE FAILED");
			return FALSE;
		}

		if (TrackInventoryTransfer(source, slot))
		{
			OS0ItemTransferController& transfers = OS0GetItemTransferController();
			if (releaseAlreadyHandled)
				transfers.adoptExternalHeldItemAfterHandledRelease();
			else
				transfers.adoptExternalHeldItem();
			BindHeldItemCarrier(cursorActor);
			return TRUE;
		}

		// Tracking is a prerequisite for cancellable drag. Put the item back before
		// exposing any more UI. If exact restoration itself fails, retain the native
		// pointer and adopt it as an explicitly untracked held object.
		if (RestoreDetachedInventoryObject(source, slot, gpItemPointer))
		{
			EndItemPointer();
			RecordFeedbackEvent("ITEM MOVE ABORTED / SOURCE RESTORED");
		}
		else
		{
			OS0ItemTransferController& transfers = OS0GetItemTransferController();
			if (releaseAlreadyHandled)
				transfers.adoptExternalHeldItemAfterHandledRelease();
			else
				transfers.adoptExternalHeldItem();
			BindHeldItemCarrier(cursorActor);
			RecordFeedbackEvent("ITEM HELD / EXACT SOURCE TRACKING FAILED");
		}
		return FALSE;
	}

	BOOLEAN EquipInventorySlotAtomically(SOLDIERTYPE* actor, INT8 const slot)
	{
		if (!actor || slot < 0 || slot >= NUM_INV_SLOTS ||
			actor->inv[slot].usItem == NOTHING) return FALSE;
		std::array<OBJECTTYPE, NUM_INV_SLOTS> inventoryBefore{};
		for (INT8 index = 0; index < NUM_INV_SLOTS; ++index)
			inventoryBefore[index] = actor->inv[index];

		OBJECTTYPE object{};
		GetObjFrom(&actor->inv[slot], 0, &object);
		if (object.usItem != NOTHING &&
			OS0EquipObject(actor, &object, slot) &&
			(object.usItem == NOTHING || object.ubNumberOfObjects == 0))
			return TRUE;

		for (INT8 index = 0; index < NUM_INV_SLOTS; ++index)
			actor->inv[index] = inventoryBefore[index];
		RecordFeedbackEvent("EQUIP BLOCKED / INVENTORY UNCHANGED");
		return FALSE;
	}

	BOOLEAN PlaceObjectCompletelyInActorPack(SOLDIERTYPE* actor,
		OBJECTTYPE* object)
	{
		if (!actor || !object || !OS0CanPackObject(actor, *object)) return FALSE;
		const OBJECTTYPE objectBefore = *object;
		std::array<OBJECTTYPE, NUM_INV_SLOTS> inventoryBefore{};
		for (INT8 slot = BIGPOCK1POS; slot <= SMALLPOCK8POS; ++slot)
			inventoryBefore[slot] = actor->inv[slot];

		auto fillRange = [actor, object](INT8 first, INT8 last,
			BOOLEAN existingStack)
		{
			for (INT8 slot = first; slot <= last && object->usItem != NOTHING &&
				object->ubNumberOfObjects > 0; ++slot)
			{
				OBJECTTYPE const& stored = actor->inv[slot];
				const BOOLEAN sameStack = stored.usItem == object->usItem;
				if (existingStack ? !sameStack : stored.usItem != NOTHING) continue;
				if (sameStack && GCM->getItem(stored.usItem)->isKey() &&
					stored.ubKeyID != object->ubKeyID) continue;
				if (ItemSlotLimit(object->usItem, slot) == 0) continue;
				PlaceObject(actor, slot, object);
			}
		};
		fillRange(BIGPOCK1POS, SMALLPOCK8POS, TRUE);
		fillRange(SMALLPOCK1POS, SMALLPOCK8POS, FALSE);
		fillRange(BIGPOCK1POS, BIGPOCK4POS, FALSE);
		if (object->usItem == NOTHING || object->ubNumberOfObjects == 0)
			return TRUE;

		for (INT8 slot = BIGPOCK1POS; slot <= SMALLPOCK8POS; ++slot)
			actor->inv[slot] = inventoryBefore[slot];
		*object = objectBefore;
		return FALSE;
	}

	BOOLEAN MoveInventoryItemToPackAtomically(SOLDIERTYPE* source,
		INT8 const slot, SOLDIERTYPE* target)
	{
		if (!source || !target || slot < 0 || slot >= NUM_INV_SLOTS ||
			source->inv[slot].usItem == NOTHING) return FALSE;
		std::array<OBJECTTYPE, NUM_INV_SLOTS> sourceBefore{};
		std::array<OBJECTTYPE, NUM_INV_SLOTS> targetBefore{};
		for (INT8 index = 0; index < NUM_INV_SLOTS; ++index)
		{
			sourceBefore[index] = source->inv[index];
			targetBefore[index] = target->inv[index];
		}

		OBJECTTYPE object{};
		GetObjFrom(&source->inv[slot], 0, &object);
		if (object.usItem != NOTHING && OS0CanAcceptCarriedObject(target, object) &&
			PlaceObjectCompletelyInActorPack(target, &object)) return TRUE;

		for (INT8 index = 0; index < NUM_INV_SLOTS; ++index)
		{
			source->inv[index] = sourceBefore[index];
			target->inv[index] = targetBefore[index];
		}
		RecordFeedbackEvent("PACK TRANSFER BLOCKED / INVENTORIES UNCHANGED");
		return FALSE;
	}

	BOOLEAN ItemTransferIntentAllowed(SOLDIERTYPE* actor, ItemTransferIntent intent)
	{
		if (!actor || !gpItemPointer || !CanAccessSoldierContents(actor) ||
			!HeldItemRelationInReach(BoundHeldItemCarrier(), actor)) return FALSE;
		if (intent == ItemTransferIntent::DROP) return TRUE;
		if (!OS0CanAcceptCarriedObject(actor, *gpItemPointer)) return FALSE;
		if (intent == ItemTransferIntent::PACK)
			return OS0CanPackObject(actor, *gpItemPointer);
		if ((intent == ItemTransferIntent::PRIMARY_HAND ||
			intent == ItemTransferIntent::SECONDARY_HAND) &&
			!OS0HasActiveHandUse(*gpItemPointer)) return FALSE;
		const INT8 slot = ItemTransferIntentSlot(actor, intent);
		if (slot == NO_SLOT) return FALSE;
		return CanPlaceObjectCompletelyInActorSlot(actor, slot, *gpItemPointer);
	}

	ItemTransferPolicyDecision CurrentItemTransferDecision(SOLDIERTYPE* actor)
	{
		ItemTransferPolicyInput input;
		input.carryingItem = gpItemPointer && gpItemPointer->usItem != NOTHING;
		input.targetAvailable = actor != nullptr;
		input.targetAccessible = actor && CanAccessSoldierContents(actor) &&
			HeldItemRelationInReach(BoundHeldItemCarrier(), actor);
		if (input.carryingItem && input.targetAccessible)
		{
			for (ItemTransferIntentSpec const& spec : gOS0ItemTransferIntents)
				input.allowed[static_cast<size_t>(spec.intent)] =
					ItemTransferIntentAllowed(actor, spec.intent);
		}
		return ResolveItemTransferPolicy(input);
	}

	ST::string ItemTransferIntentLabel(SOLDIERTYPE* actor, ItemTransferIntent intent)
	{
		if (!actor || !gpItemPointer) return "NO TARGET";
		if (intent == ItemTransferIntent::PACK)
			return ST::format("PACK / LOAD {}% -> {}%", CalculateCarriedWeight(actor),
				CalculateCarriedWeight(actor) + OS0AddedCarryPercent(actor, *gpItemPointer));
		if (intent == ItemTransferIntent::DROP) return "DROP AT FEET / WORLD ITEM";
		const INT8 slot = ItemTransferIntentSlot(actor, intent);
		if (intent == ItemTransferIntent::BODY && slot == NO_SLOT)
			return "BODY / ITEM IS NOT WEARABLE";
		if (slot == HANDPOS || slot == SECONDHANDPOS)
		{
			ItemModel const* const item = GCM->getItem(gpItemPointer->usItem);
			const char* const hand = slot == HANDPOS ? "HAND 1" : "HAND 2";
			if (slot == SECONDHANDPOS && item->isTwoHanded())
				return "HAND 2 / BLOCKED BY TWO-HAND ITEM";
			if (actor->inv[slot].usItem != NOTHING)
				return ST::format("{} / {}", hand,
					CanPlaceObjectCompletelyInActorSlot(actor, slot, *gpItemPointer) ?
						"MERGE STACK" : "OCCUPIED");
			return ST::format("{} / {}", hand,
				item->isTwoHanded() ? "TWO-HAND GRIP" : "TAKE ITEM");
		}
		if (slot != NO_SLOT)
		{
			auto const found = std::find(gExplodedEquipmentSlots.begin(),
				gExplodedEquipmentSlots.end(), slot);
			const char* const slotName = found == gExplodedEquipmentSlots.end() ?
				"BODY" : gExplodedEquipmentLabels[static_cast<size_t>(
					std::distance(gExplodedEquipmentSlots.begin(), found))];
			return ST::format("EQUIP {} / {}", slotName,
				actor->inv[slot].usItem == NOTHING ? "FREE" :
				CanPlaceObjectCompletelyInActorSlot(actor, slot, *gpItemPointer) ?
					"MERGE STACK" : "OCCUPIED");
		}
		return "INCOMPATIBLE";
	}

	BOOLEAN PlacePointerInActorSlot(SOLDIERTYPE* actor, INT8 slot)
	{
		if (!actor || !gpItemPointer || slot == NO_SLOT) return FALSE;
		OBJECTTYPE const cursorBefore = *gpItemPointer;
		OBJECTTYPE& target = actor->inv[slot];
		OBJECTTYPE const targetBefore = target;
		if (!CanPlaceObjectCompletelyInActorSlot(actor, slot, *gpItemPointer))
			return FALSE;

		// OS//0 never performs JA2's implicit slot swap. A swap changes the object on
		// the native cursor while its transfer transaction still names the original
		// item; that was the root of the duplicated/vanishing-slot glitches. An
		// occupied slot only accepts a complete compatible stack merge.
		if (!PlaceObject(actor, slot, gpItemPointer)) return FALSE;
		if (gpItemPointer->usItem != NOTHING &&
			gpItemPointer->ubNumberOfObjects > 0)
		{
			// A supposedly complete placement became partial inside the engine. Undo
			// both native values and leave the exact source transaction held.
			target = targetBefore;
			*gpItemPointer = cursorBefore;
			return FALSE;
		}
		FinishCommittedItemPointer();
		return TRUE;
	}

	BOOLEAN PlacePointerInActorPack(SOLDIERTYPE* actor)
	{
		return actor && gpItemPointer &&
			PlaceObjectCompletelyInActorPack(actor, gpItemPointer);
	}

	BOOLEAN ApplyItemTransferIntent(SOLDIERTYPE* actor, ItemTransferIntent intent)
	{
		if (!actor || !gpItemPointer ||
			!CurrentItemTransferDecision(actor).allows(intent)) return FALSE;
		BOOLEAN applied = FALSE;
		switch (intent)
		{
			case ItemTransferIntent::PRIMARY_HAND:
				applied = PlacePointerInActorSlot(actor, HANDPOS);
				break;
			case ItemTransferIntent::SECONDARY_HAND:
				applied = PlacePointerInActorSlot(actor, SECONDHANDPOS);
				break;
			case ItemTransferIntent::BODY:
				applied = PlacePointerInActorSlot(actor,
					ItemTransferIntentSlot(actor, intent));
				break;
			case ItemTransferIntent::PACK:
				// The policy has already verified enough pocket capacity. If an engine
				// edge case still leaves a remainder, keep it on the cursor instead of
				// silently dropping it at the actor's feet.
				applied = PlacePointerInActorPack(actor);
				break;
			case ItemTransferIntent::DROP:
				applied = DropPointerAtActor(actor);
				break;
		}
		if (gpItemPointer && gpItemPointer->ubNumberOfObjects == 0)
			FinishCommittedItemPointer();
		if (!gpItemPointer) ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		return applied;
	}

	void ItemTransferIntentCallback(MOUSE_REGION* region, UINT32 reason)
	{
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			transfers.reconcile(gpItemPointer != nullptr);
			transfers.beginHeldGesture();
			return;
		}
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gpItemPointer) return;
		if (transfers.claimRelease(OS0ItemTransferSurface::RELATION) !=
			OS0ItemReleaseClaim::ITEM) return;
		SOLDIERTYPE* const actor = BoundItemTransferTarget();
		if (!actor || !CanAccessSoldierContents(actor) ||
			!HeldItemRelationInReach(BoundHeldItemCarrier(), actor))
		{
			ClearItemTransferTarget();
			transfers.completeItemRelease(TRUE);
			return;
		}
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gOS0ItemTransferIntents.size())
		{
			transfers.completeItemRelease(TRUE);
			return;
		}
		const ItemTransferIntent intent = gOS0ItemTransferIntents[index].intent;
		ItemTransferPolicyDecision const decision =
			CurrentItemTransferDecision(actor);
		if (!decision.allows(intent))
		{
			RecordFeedbackEvent(ST::format("TRANSFER BLOCKED / {}",
				ItemTransferIntentLabel(actor, intent)));
			transfers.completeItemRelease(TRUE);
			return;
		}
		ApplyItemTransferIntent(actor, intent);
		transfers.completeItemRelease(gpItemPointer != nullptr);
	}

	void ItemTransferMoreCallback(MOUSE_REGION*, UINT32 reason)
	{
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			transfers.reconcile(gpItemPointer != nullptr);
			transfers.beginHeldGesture();
			return;
		}
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gpItemPointer) return;
		if (transfers.claimRelease(OS0ItemTransferSurface::RELATION) !=
			OS0ItemReleaseClaim::ITEM) return;
		SOLDIERTYPE* const target = BoundItemTransferTarget();
		if (!target || !CanAccessSoldierContents(target))
		{
			ClearItemTransferTarget();
			transfers.completeItemRelease(TRUE);
			return;
		}
		ItemTransferPolicyDecision const decision =
			CurrentItemTransferDecision(target);
		if (!decision.hasAlternatives())
		{
			transfers.completeItemRelease(TRUE);
			return;
		}
		gItemTransferMoreVisible = !gItemTransferMoreVisible;
		PositionItemTransferIntentRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		transfers.completeItemRelease(TRUE);
	}

	void SlotCallbackForSoldier(MOUSE_REGION* region, UINT32 reason,
		SOLDIERTYPE* soldier)
	{
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		if (GetJA2Clock() < gPanelInteractionGuardUntil)
		{
			if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
				transfers.claimRelease(OS0ItemTransferSurface::INVENTORY) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!soldier || !CanAccessSoldierContents(soldier))
		{
			if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
				transfers.claimRelease(OS0ItemTransferSurface::INVENTORY) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		const INT8 slot = static_cast<INT8>(region->GetUserData<0>());
		const UINT32 sourceId = (static_cast<UINT32>(soldier->ubID) << 8) |
			static_cast<UINT8>(slot);
		const OS0ItemSourceIdentity sourceIdentity =
			InventorySourcePressIdentity(soldier, slot);

		if ((reason & MSYS_CALLBACK_REASON_RBUTTON_UP) &&
			gpItemPointer == nullptr && soldier->inv[slot].usItem != NOTHING)
		{
			OpenInventoryItemContext(soldier, slot);
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			if (gpItemPointer)
			{
				transfers.reconcile(TRUE);
				transfers.beginHeldGesture();
				return;
			}
			if (soldier->inv[slot].usItem != NOTHING)
				transfers.beginSourcePress(OS0ItemTransferSurface::INVENTORY,
					sourceId, gusMouseXPos, gusMouseYPos, sourceIdentity);
		}
		else if (!gpItemPointer &&
			transfers.sourceMatches(OS0ItemTransferSurface::INVENTORY, sourceId,
				sourceIdentity) &&
			(((reason & MSYS_CALLBACK_REASON_MOVE) &&
				(region->ButtonState & MSYS_LEFT_BUTTON) &&
				transfers.dragThresholdReached(OS0ItemTransferSurface::INVENTORY,
					sourceId, gusMouseXPos, gusMouseYPos, 4, sourceIdentity)) ||
			 ((reason & MSYS_CALLBACK_REASON_LOST_MOUSE) &&
				(IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown()))))
		{
			if (soldier->inv[slot].usItem == NOTHING)
			{
				transfers.cancelGestureAndConsumeRelease();
				return;
			}
			if (soldier == selected && soldier->inv[slot].ubNumberOfObjects > 1)
			{
				// Quantity selection is its own modal transaction. It consumes this
				// drag before creating a new native item pointer.
				transfers.cancelGestureAndConsumeRelease();
				BeginStackSplit(soldier, slot);
				return;
			}
			if (!selected)
			{
				transfers.cancelGestureAndConsumeRelease();
				return;
			}
			OBJECTTYPE detached{};
			GetObjFrom(&soldier->inv[slot], 0, &detached);
			if (detached.usItem != NOTHING &&
				BeginTrackedInventoryItemTransfer(soldier, slot, selected,
					detached, FALSE))
			{
				transfers.markItemHeld(OS0ItemTransferSurface::INVENTORY, sourceId,
					sourceIdentity);
			}
			else
				transfers.cancelGestureAndConsumeRelease();
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (transfers.claimRelease(OS0ItemTransferSurface::INVENTORY) !=
				OS0ItemReleaseClaim::ITEM) return;
			if (!OS0CanAcceptCarriedObject(soldier, *gpItemPointer))
				RecordFeedbackEvent("LOAD LIMIT 125% / SLOT REJECTED");
			else if (!PlacePointerInActorSlot(soldier, slot))
				RecordFeedbackEvent("ITEM / SLOT RELATION NOT COMPATIBLE");
			transfers.completeItemRelease(gpItemPointer != nullptr);
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			transfers.claimRelease(OS0ItemTransferSurface::INVENTORY);
		}
	}

	void UpdateLootProjectionState()
	{
		const size_t slotCount = RefreshLootWorldItems();
		if (!gLootVisible) return;
		if (slotCount == 0)
		{
			// An empty physical container is represented by its world sprite alone.
			gLootVisible = FALSE;
			SetLootRegionsEnabled(FALSE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		for (size_t slot = 0; slot < gLootRegions.size(); ++slot)
		{
			ST::string help;
			const INT32 itemIndex = gLootWorldItems[slot];
			if (itemIndex >= 0 && static_cast<size_t>(itemIndex) < gWorldItems.size())
			{
				WORLDITEM const& worldItem = GetWorldItem(itemIndex);
				if (worldItem.fExists && worldItem.o.usItem != NOTHING)
				{
					ItemModel const* const model =
						GCM->getItem(worldItem.o.usItem);
					help = ST::format("{} / {}% / x{}\n"
						"DOUBLE: PACK  RIGHT: OPTIONS  DRAG: MOVE",
						model->getName(), worldItem.o.bStatus[0],
						worldItem.o.ubNumberOfObjects);
				}
			}
			if (help != gLootHelp[slot])
			{
				gLootHelp[slot] = help;
				gLootRegions[slot].SetFastHelpText(help);
			}
		}
	}

	void SlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		SlotCallbackForSoldier(region, reason, gInventorySoldier ?
			gInventorySoldier : GetSelectedMan());
	}

	void EquipmentSlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		// Exploded equipment is attached to its own actor. It must never inherit
		// whichever soldier happens to own the independent RPG inventory window.
		SlotCallbackForSoldier(region, reason, gEquipmentSoldier);
	}

	void LootSlotCallback(MOUSE_REGION* region, UINT32 reason)
	{
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		// The double-click which opened this window may still be completing.
		// Never let that same physical gesture activate or close its contents.
		if (GetJA2Clock() < gPanelInteractionGuardUntil ||
			GetJA2Clock() < gLootIgnoreInputUntil)
		{
			if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
				transfers.claimRelease(OS0ItemTransferSurface::LOOT) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		SOLDIERTYPE* const selected = BoundLootActor();
		if (!selected || gLootGridNo < 0 || gLootGridNo >= WORLD_MAX)
		{
			if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
				transfers.claimRelease(OS0ItemTransferSurface::LOOT) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		if (PythSpacesAway(selected->sGridNo, gLootGridNo) > 2)
		{
			if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer &&
				transfers.claimRelease(OS0ItemTransferSurface::LOOT) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			gLootVisible = FALSE;
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		const size_t slot = static_cast<size_t>(region->GetUserData<0>());
		if (slot >= gLootWorldItems.size()) return;
		const INT32 itemIndex = gLootWorldItems[slot];
		const UINT32 sourceId = itemIndex >= 0 ? static_cast<UINT32>(itemIndex) :
			static_cast<UINT32>(slot);
		OS0ItemSourceIdentity sourceIdentity{};
		if (itemIndex >= 0 && static_cast<size_t>(itemIndex) < gWorldItems.size())
			sourceIdentity = WorldSourcePressIdentity(selected,
				GetWorldItem(itemIndex));

		if (reason & MSYS_CALLBACK_REASON_LBUTTON_DOUBLECLICK)
		{
			transfers.cancelGestureAndConsumeRelease();
			if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size()) return;
			WORLDITEM& worldItem = GetWorldItem(itemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return;

			if (OS0IsResourceItem(worldItem.o.usItem))
				StoreResourceWorldItem(selected, itemIndex);
			else if (!OS0CanAcceptCarriedObject(selected, worldItem.o))
				RecordFeedbackEvent("LOAD LIMIT 125% / PICKUP REJECTED");
			else if (OS0CanPackObject(selected, worldItem.o))
			{
				if (!OS0SoldierPickupExactWorldItem(selected, itemIndex,
					gLootGridNo, ITEM_IGNORE_Z_LEVEL))
					RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
			}
			else if (BeginTrackedWorldItemTransfer(selected, itemIndex,
				gLootTileIndex, FALSE))
				RecordFeedbackEvent("PACK FULL / ITEM HELD FOR PLACEMENT");
			else
				RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
			fInterfacePanelDirty = DIRTYLEVEL2;
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		if ((reason & MSYS_CALLBACK_REASON_RBUTTON_UP) && !gpItemPointer)
		{
			OpenLootItemContext(itemIndex);
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			if (gpItemPointer)
			{
				transfers.reconcile(TRUE);
				transfers.beginHeldGesture();
			}
			else
			{
				// Do not remove the item yet. A double-click starts with this same
				// event, so drag begins only after the pointer actually moves.
				transfers.beginSourcePress(OS0ItemTransferSurface::LOOT,
					sourceId, gusMouseXPos, gusMouseYPos, sourceIdentity);
			}
		}
		else if (!gpItemPointer &&
			transfers.sourceMatches(OS0ItemTransferSurface::LOOT, sourceId,
				sourceIdentity) &&
			(((reason & MSYS_CALLBACK_REASON_MOVE) &&
				(region->ButtonState & MSYS_LEFT_BUTTON) &&
				transfers.dragThresholdReached(OS0ItemTransferSurface::LOOT,
					sourceId, gusMouseXPos, gusMouseYPos, 4, sourceIdentity)) ||
			 ((reason & MSYS_CALLBACK_REASON_LOST_MOUSE) &&
				(IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown()))))
		{
			if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size()) return;
			WORLDITEM& worldItem = GetWorldItem(itemIndex);
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING) return;
			if (!OS0PrepareWorldItemForDirectDetach(selected, itemIndex,
				worldItem.sGridNo, worldItem.ubLevel))
			{
				transfers.cancelGestureAndConsumeRelease();
				RecordFeedbackEvent(
					"CONTAINER DETACH BLOCKED / TRAP OR TARGET CHANGED");
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
			OS0ItemTransferOrigin const origin =
				SpatialTransferOrigin(worldItem, gLootTileIndex);
			OBJECTTYPE object = worldItem.o;
			RemoveItemFromPool(worldItem);
			OS0NotifyWorldMutation();
			InternalBeginItemPointer(selected, &object, NO_SLOT);
			if (!gpItemPointer)
			{
				RestoreDetachedSpatialObject(origin, &object);
				transfers.cancelGestureAndConsumeRelease();
				return;
			}
			if (!TrackSpatialTransfer(origin))
			{
				if (RestoreDetachedSpatialObject(origin, gpItemPointer))
				{
					EndItemPointer();
					ClearHeldItemCarrier();
					transfers.cancelGestureAndConsumeRelease();
				}
				else
				{
					BindHeldItemCarrier(selected);
					transfers.adoptExternalHeldItem();
				}
				RecordFeedbackEvent(
					"CONTAINER DRAG ABORTED / SOURCE TRACKING FAILED");
				return;
			}
			transfers.markItemHeld(OS0ItemTransferSurface::LOOT, sourceId,
				sourceIdentity);
			BindHeldItemCarrier(selected);
			// Taking an item from a container enters one clear state: item held,
			// container still open. Destination affordances only appear when the
			// cursor actually reaches an accessible actor.
			ClearItemTransferTarget();
			gItemTransferMoreVisible = FALSE;
			gEquipmentExplodedVisible = FALSE;
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gpItemPointer)
		{
			if (transfers.claimRelease(OS0ItemTransferSurface::LOOT) !=
				OS0ItemReleaseClaim::ITEM) return;
			STRUCTURE const* const container =
				gLootTileIndex < NUMBEROFTILES ?
				WorldStructureAt(gLootGridNo, gLootLevel, gLootTileIndex) : nullptr;
			const Visibility visibility = container &&
				(container->fFlags & STRUCTURE_OPENABLE) &&
				!(container->fFlags & STRUCTURE_ANYDOOR) &&
				!(container->fFlags & STRUCTURE_OPEN) ?
				HIDDEN_IN_OBJECT : VISIBLE;
			if (AddItemToPool(gLootGridNo, gpItemPointer, visibility,
				gLootLevel, 0, -1) >= 0)
			{
				OS0NotifyWorldMutation();
				FinishCommittedItemPointer();
			}
			transfers.completeItemRelease(gpItemPointer != nullptr);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			transfers.claimRelease(OS0ItemTransferSurface::LOOT);
		}
	}

	void ApplyTutorialName()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier || gCreatorModel.callsign().empty()) return;
		const ST::string name = gCreatorModel.callsign().left(16);
		soldier->name = name;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		profile.zNickname = name;
		profile.zName = name;
	}

	void ApplyTutorialBody()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier || soldier->ubProfile == NO_PROFILE) return;
		SoldierBodyType const body = gCreatorModel.bodyType();
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		profile.ubBodyType = static_cast<UINT8>(body);
		profile.bSex = body == REGFEMALE ? FEMALE : MALE;
		// The creator chooses an actual JA2 tactical body family, not a portrait
		// skin. Rebuild the live merc surface immediately so the player sees the
		// selected sprite moving behind the creator window.
		if (soldier->ubBodyType != static_cast<UINT8>(body))
		{
			soldier->ubBodyType = static_cast<UINT8>(body);
			soldier->usAniFrame = 0;
			SetSoldierAnimationSurface(soldier, STANDING);
			CreateSoldierPalettes(*soldier);
			EVENT_InitNewSoldierAnim(soldier, STANDING, 0, TRUE);
		}
		// This operator is deliberately silent. SOLDIER_MUTE also suppresses the
		// vanilla tactical battle-sound path.
		soldier->uiStatusFlags |= SOLDIER_MUTE;
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ApplyTutorialStats()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier) return;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		auto const& stats = gCreatorModel.stats();
		soldier->bLifeMax      = profile.bLifeMax      = stats[0];
		soldier->bLife         = profile.bLife         = stats[0];
		soldier->bAgility      = profile.bAgility      = stats[1];
		soldier->bDexterity    = profile.bDexterity    = stats[2];
		soldier->bStrength     = profile.bStrength     = stats[3];
		soldier->bWisdom       = profile.bWisdom       = stats[4];
		soldier->bLeadership   = profile.bLeadership   = stats[5];
		soldier->bMarksmanship = profile.bMarksmanship = stats[6];
		soldier->bMedical      = profile.bMedical      = stats[7];
		soldier->bMechanical   = profile.bMechanical   = stats[8];
		soldier->bExplosive    = profile.bExplosive    = stats[9];
	}

	void ApplyTutorialTraits()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier) return;
		MERCPROFILESTRUCT& profile = GetProfile(soldier->ubProfile);
		// These are freely selected specialties, not a predefined class.
		auto const& traits = gCreatorModel.traits();
		profile.bSkillTrait = static_cast<INT8>(traits[0]);
		profile.bSkillTrait2 = static_cast<INT8>(traits[1]);
		soldier->ubSkillTrait1 = static_cast<UINT8>(traits[0]);
		soldier->ubSkillTrait2 = static_cast<UINT8>(traits[1]);
		const BOOLEAN camouflage = traits[0] == CAMOUFLAGED ||
			traits[1] == CAMOUFLAGED;
		const INT8 desiredCamo = camouflage ? 100 : 0;
		if (soldier->bCamo != desiredCamo)
		{
			soldier->bCamo = desiredCamo;
			// The CAMOUFLAGED trait is a permanent 100% tactical effect in JA2.
			// Soldier creation normally applies it before palettes exist; our live
			// creator edits an existing merc and therefore must rebuild them here.
			CreateSoldierPalettes(*soldier);
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
	}

	BOOLEAN PointerInsideItemTransferContext(SOLDIERTYPE const* actor)
	{
		INT16 anchorX;
		INT16 anchorY;
		if (!actor || !GetActorDisplayAnchor(actor, anchorX, anchorY)) return FALSE;
		// Keep the context alive while the pointer travels from the body to one of
		// its radial destinations. This fixes the old vanish-before-click behaviour
		// without turning the projection into a persistent window.
		const INT32 dx = static_cast<INT32>(gusMouseXPos) - anchorX;
		const INT32 dy = static_cast<INT32>(gusMouseYPos) - anchorY;
		return std::abs(dx) <= 132 && dy >= -132 && dy <= 42;
	}

	void EnsureSelectedOperatorTraitEffects()
	{
		SOLDIERTYPE* const soldier = GetSelectedMan();
		if (!soldier || !soldier->bActive || soldier->bTeam != OUR_TEAM ||
			!HAS_SKILL_TRAIT(soldier, CAMOUFLAGED) || soldier->bCamo == 100)
			return;
		soldier->bCamo = 100;
		CreateSoldierPalettes(*soldier);
		fInterfacePanelDirty = DIRTYLEVEL2;
		RecordFeedbackEvent("CAMOUFLAGE TRAIT / LIVE EFFECT 100% APPLIED");
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	BOOLEAN TutorialKeyboardHook(InputAtom* event)
	{
		if (!gTutorialActive ||
			gUIRuntime.creatorStage() != OS0CreatorStage::IDENTITY) return FALSE;
		if (event->usEvent == TEXT_INPUT)
		{
			for (char32_t c : event->codepoints)
				gCreatorModel.appendCallsign(c);
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usEvent != KEY_DOWN && event->usEvent != KEY_REPEAT) return FALSE;
		if (event->usParam == SDLK_BACKSPACE)
		{
			gCreatorModel.backspaceCallsign();
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (event->usParam == SDLK_RETURN && !gCreatorModel.callsign().empty())
		{
			ApplyTutorialName();
			ApplyTutorialBody();
			gUIRuntime.advanceCreator();
			BindInventorySoldier(GetSelectedMan());
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
		switch (gUIRuntime.creatorStage())
		{
			case OS0CreatorStage::WELCOME:
				gUIRuntime.advanceCreator();
				SetUIKeyboardHook(TutorialKeyboardHook);
				break;
			case OS0CreatorStage::IDENTITY:
				if (gCreatorModel.callsign().empty()) return;
				ApplyTutorialName();
				ApplyTutorialBody();
				gUIRuntime.advanceCreator();
				BindInventorySoldier(GetSelectedMan());
				for (MOUSE_REGION& r : gTutorialStats) r.Enable();
				SetUIKeyboardHook(nullptr);
				break;
			case OS0CreatorStage::ATTRIBUTES:
				ApplyTutorialStats();
				gUIRuntime.advanceCreator();
				for (MOUSE_REGION& r : gTutorialStats) r.Disable();
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Enable();
				break;
			case OS0CreatorStage::TRAITS:
				ApplyTutorialTraits();
				EnsureDebugFieldTools(GetSelectedMan());
				gFieldToolIssued = TRUE;
				// Inventory is a gameplay tool, not a compulsory creator page.
				// Continue directly to the control briefing and leave it closed on entry.
				gUIRuntime.advanceCreator();
				BindInventorySoldier(GetSelectedMan());
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Disable();
				break;
			case OS0CreatorStage::CONTROLS:
				ApplyTutorialBody();
				gUIRuntime.advanceCreator();
				OS0GetTacticalSession().state().creatorCompleted = TRUE;
				gfDoVideoScroll = gVideoScrollBeforeCreator;
				BindInspectedSoldier(GetSelectedMan());
				BindInventorySoldier(gInspectedSoldier);
				ClampWindowPositions();
				SaveUILayout();
				SetUIKeyboardHook(nullptr);
				SetBagRegionsEnabled(TRUE);
				break;
			case OS0CreatorStage::COMPLETE:
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
		gCreatorModel.adjustStat(stat, increase ? 1 : -1);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void TutorialBodyCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) ||
			!gTutorialActive ||
			gUIRuntime.creatorStage() != OS0CreatorStage::IDENTITY) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gTutorialBodyTypes.size()) return;
		gCreatorModel.selectBodyType(gTutorialBodyTypes[index]);
		ApplyTutorialBody();
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void TutorialTraitCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gTutorialTraitValues.size()) return;
		gCreatorModel.toggleTrait(gTutorialTraitValues[index]);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void SelectAdjacentSquad(INT8 direction)
	{
		const INT32 current = CurrentSquad();
		// Do not wrap all the way back to the current squad.  If no other active
		// squad exists, PREVIOUS/NEXT is a clean no-op.
		for (INT32 offset = 1; offset < NUMBER_OF_SQUADS; ++offset)
		{
			const INT32 candidate = (current + direction * offset +
				NUMBER_OF_SQUADS * 2) % NUMBER_OF_SQUADS;
			if (IsSquadOnCurrentTacticalMap(candidate))
			{
				SetCurrentSquad(candidate, FALSE);
				if (SOLDIERTYPE* const selected = GetSelectedMan())
				{
					BindInspectedSoldier(selected);
					gInspectedGridNo = NOWHERE;
					LocateSoldier(selected, DONTSETLOCATOR);
				}
				break;
			}
		}
	}

	void OrbCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			gMultiToolDragCandidate = TRUE;
			gMultiToolDragging = FALSE;
			gMultiToolDragStartX = gusMouseXPos;
			gMultiToolDragStartY = gusMouseYPos;
			gMultiToolDragOriginX = gOrbX;
			gMultiToolDragOriginY = gOrbY;
			return;
		}
		if ((reason & MSYS_CALLBACK_REASON_MOVE) && gMultiToolDragCandidate &&
			(region->ButtonState & MSYS_LEFT_BUTTON))
		{
			if (!gMultiToolDragging &&
				(std::abs(gusMouseXPos - gMultiToolDragStartX) >= 4 ||
				 std::abs(gusMouseYPos - gMultiToolDragStartY) >= 4))
			{
				gMultiToolDragging = TRUE;
				gMultiToolLastClickAt = 0;
			}
			if (gMultiToolDragging)
			{
				gOrbX = static_cast<INT16>(gMultiToolDragOriginX +
					gusMouseXPos - gMultiToolDragStartX);
				gOrbY = static_cast<INT16>(gMultiToolDragOriginY +
					gusMouseYPos - gMultiToolDragStartY);
				ClampMultiToolPosition();
				PositionBagRegions();
				SetRenderFlags(RENDER_FLAG_FULL);
			}
			return;
		}
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		gMultiToolDragCandidate = FALSE;
		if (gMultiToolDragging)
		{
			gMultiToolDragging = FALSE;
			PositionBagRegions();
			SaveUILayout();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		const UINT32 now = GetJA2Clock();
		if (gMultiToolLastClickAt == 0 || now - gMultiToolLastClickAt > 360)
		{
			gMultiToolLastClickAt = now;
			return;
		}
		gMultiToolLastClickAt = 0;
		CloseContextMenu();
		gMultiToolExpanded = !gMultiToolExpanded;
		// The legacy grid toolbox is superseded by the semantic fan above. Never
		// leave it invisibly open behind the collapsed multitool.
		gUIRuntime.windowManager().hide(
			gUIRuntime.managedId(FloatingPanelId::TOOLBOX));
		PositionBagRegions();
		SaveUILayout();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void CombatModeCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || gTutorialActive) return;
		ActivateToolboxModule(ToolboxModule::TACTICAL);
	}

	void ToolboxModuleCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || gTutorialActive) return;
		const ToolboxModule module = static_cast<ToolboxModule>(
			region->GetUserData<0>());
		if (module >= ToolboxModule::COUNT) return;
		ActivateToolboxModule(module);
	}

	void EnvironmentSkillCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || gContextVisible) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gEnvironmentEntryCount ||
			!gEnvironmentEntries[index].enabled ||
			gEnvironmentGridNo < 0 || gEnvironmentGridNo >= WORLD_MAX) return;
		ContextAction const action = gEnvironmentEntries[index].action;
		OS0ResolvedActionList const resolved =
			ResolveInteractionAt(nullptr, gEnvironmentGridNo,
				gEnvironmentLevel, gEnvironmentTileIndex);
		if (OS0ResolvedAction const* const match =
			FindOS0ResolvedAction(resolved, action))
		{
			ExecuteOrQueueBoundAction(*match);
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void NearbyHintCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & (MSYS_CALLBACK_REASON_POINTER_UP |
			MSYS_CALLBACK_REASON_RBUTTON_UP |
			MSYS_CALLBACK_REASON_MBUTTON_UP)) || gContextVisible ||
			gTutorialActive || gAimAutoCollapsed ||
			!InteractionMode().nearbyScanEnabled()) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gNearbyHintCount) return;
		NearbyInteractionHint const hint = gNearbyHints[index];
		if (!BindingStillValid(hint.binding))
		{
			ResetNearbyScanCache();
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		if (reason & MSYS_CALLBACK_REASON_MBUTTON_UP)
		{
			OS0CycleCursorAction(nullptr, hint.gridNo, hint.level, hint.tileIndex,
				hint.binding.worldItemIndex);
			return;
		}
		RefreshEnvironmentTarget(hint.gridNo, hint.level, hint.tileIndex);
		gUIRuntime.windowManager().show(
			gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
		OS0OpenContextMenu(nullptr, hint.gridNo, hint.level, hint.tileIndex,
			region->RegionTopLeftX, region->RegionTopLeftY,
			hint.binding.worldItemIndex);
	}

	void NearbyHintMoveCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_GAIN_MOUSE) ||
			!InteractionMode().nearbyScanEnabled()) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gNearbyHintCount) return;
		NearbyInteractionHint const& hint = gNearbyHints[index];
		// The hint temporarily owns the pointer instead of the viewport. Re-project
		// the same world hover so entering the icon does not close the inspector.
		OS0HoverWorldObject(nullptr, hint.gridNo, hint.level, hint.tileIndex,
			region->RegionTopLeftX + 12, region->RegionTopLeftY + 12,
			hint.binding.worldItemIndex);
	}

	void HoverQuickActionCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & (MSYS_CALLBACK_REASON_POINTER_UP |
			MSYS_CALLBACK_REASON_RBUTTON_UP |
			MSYS_CALLBACK_REASON_MBUTTON_UP)) || gContextVisible ||
			gHoverSuggestedAction == ContextAction::COUNT ||
			gHoverActionBinding.kind == OS0InteractionTargetKind::NONE) return;
		if (!BindingStillValid(gHoverActionBinding))
		{
			OS0ClearWorldHover();
			return;
		}
		if (reason & MSYS_CALLBACK_REASON_MBUTTON_UP)
		{
			SOLDIERTYPE* target = nullptr;
			if (gHoverActionBinding.kind == OS0InteractionTargetKind::ACTOR)
				target = ID2Soldier(static_cast<UINT8>(
					gHoverActionBinding.actorId));
			OS0CycleCursorAction(target, gHoverActionBinding.gridNo,
				gHoverActionBinding.level, gHoverActionBinding.tileIndex,
				gHoverActionBinding.worldItemIndex);
			return;
		}
		if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
		{
			SOLDIERTYPE* target = nullptr;
			if (gHoverActionBinding.kind == OS0InteractionTargetKind::ACTOR)
				target = ID2Soldier(static_cast<UINT8>(
					gHoverActionBinding.actorId));
			gPanelInteractionGuardUntil = 0;
			OS0OpenContextMenu(target, gHoverActionBinding.gridNo,
				gHoverActionBinding.level, gHoverActionBinding.tileIndex,
				gusMouseXPos, gusMouseYPos,
				gHoverActionBinding.worldItemIndex);
			return;
		}
		// The icon already owns an immutable target/action relation. Re-resolving it
		// through the current screen pixel made multi-tile crates alternate between
		// child and base tiles, so OPEN could become a no-op and the tutorial never
		// received its events. Execute the bound relation directly.
		const ContextAction action = gHoverSuggestedAction;
		const OS0ActionBinding binding = gHoverActionBinding;
		gPanelInteractionGuardUntil = 0;
		if (FieldTutorialTargetMatches(binding))
		{
			NotifyFieldTutorial(OS0FieldTutorialEvent::ACTIONS_OPENED);
		}
		if (binding.kind != OS0InteractionTargetKind::ACTOR)
		{
			OS0ResolvedAction resolved;
			if (ResolveBoundAction(binding, action, resolved))
			{
				const BOOLEAN accepted = ExecuteOrQueueBoundAction(resolved);
				if (accepted && action == ContextAction::CONTENTS)
					NotifyFieldTutorial(
						OS0FieldTutorialEvent::CONTENTS_SELECTED, binding);
				if (!accepted)
					RecordFeedbackEvent(ST::format("{} / ACTION FAILED",
						ContextActionName(action)));
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
			}
		}
		// Actor quick actions retain their character-specific dispatch in the
		// shared cursor path.
		gHoverActionExplicit = TRUE;
		OS0HandleCursorAction(gHoverCursorSoldier, gHoverCursorGridNo,
			gHoverCursorLevel, gHoverCursorTileIndex);
	}

	void HoverQuickActionMoveCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_GAIN_MOUSE)) return;
		// Viewport LOST_MOUSE deliberately preserved the binding while crossing
		// into this child. Revalidate before it becomes clickable; native map
		// mutation may have replaced the target between projection and entry.
		if (!BindingStillValid(gHoverActionBinding)) OS0ClearWorldHover();
	}

	BOOLEAN SetNearbyPerceptionEnabled(BOOLEAN enabled)
	{
		if (enabled && !InteractionMode().canScanNearby())
		{
			RecordFeedbackEvent("NEARBY SCAN / UNAVAILABLE IN FIGHT");
			return FALSE;
		}
		const bool changed = InteractionMode().nearbyScanRequested() != !!enabled;
		const bool accepted = enabled ? InteractionMode().beginPerception() :
			InteractionMode().setNearbyScanEnabled(false);
		if (!accepted) return FALSE;
		if (changed) ResetNearbyScanCache();
		if (enabled)
		{
			gUIRuntime.windowManager().show(
				gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
		}
		else
		{
			// Nearby perception is only a visibility/filter mode.  Turning it off
			// must not cancel an independent DIG/USE/ATTACK cursor or a deliberately
			// pinned inspector.
			if (!gInspectorPinned)
				gUIRuntime.windowManager().hide(
					gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
		}
		if (changed)
			RecordFeedbackEvent(enabled ?
				"NEARBY SCAN / ON" : "NEARBY SCAN / OFF");
		return TRUE;
	}

	void ActivateToolboxModule(ToolboxModule module)
	{
		if (module == ToolboxModule::COUNT) return;
		SOLDIERTYPE* const selected = GetSelectedMan();
		switch (GetOS0UICommandDescriptor(module).intent)
		{
			case OS0UICommandIntent::TOGGLE_COMBAT_MODE:
				if (CombatModeActive())
				{
					OS0CancelCursorAction();
					RecordFeedbackEvent("CONTROL MODE / NORMAL");
				}
				else if (!selected || !selected->bActive ||
					selected->bTeam != OUR_TEAM ||
					!OK_CONTROLLABLE_MERC(selected) || gpItemPointer ||
					CarryState().active() || gPendingWorldAction.active() ||
					gStackSplitVisible ||
					gAssetCatalogVisible)
				{
					RecordFeedbackEvent("COMBAT MODE BLOCKED / FINISH CURRENT ACTION");
					ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE,
						"Finish the current item or world action before entering combat mode.");
				}
				else
				{
					CloseContextMenu();
					ApplyCursorTool(ContextAction::ATTACK);
					RecordFeedbackEvent("WEAPON / READY");
				}
				break;
			case OS0UICommandIntent::OPEN_CHARACTER_HUB:
				if (selected)
				{
					INT16 anchorX = gusMouseXPos;
					INT16 anchorY = gusMouseYPos;
					GetActorDisplayAnchor(selected, anchorX, anchorY);
					OS0OpenContextMenu(selected, selected->sGridNo,
						selected->bLevel, NO_TILE, anchorX, anchorY);
				}
				break;
			case OS0UICommandIntent::OPEN_BEHAVIOR:
				if (selected)
				{
					INT16 anchorX = gusMouseXPos;
					INT16 anchorY = gusMouseYPos;
					GetActorDisplayAnchor(selected, anchorX, anchorY);
					OS0OpenContextMenu(selected, selected->sGridNo,
						selected->bLevel, NO_TILE, anchorX, anchorY);
					BuildCharacterContextPage(selected,
						CharacterHubCategory::ACTIONS);
					gContextTitle = ST::format("{} / BEHAVIOR", selected->name);
					SetBagRegionsEnabled(TRUE);
				}
				InteractionMode().beginInteraction(
					OS0InteractionSurface::BEHAVIOR);
				break;
			case OS0UICommandIntent::TOGGLE_NEARBY_SCAN:
				SetNearbyPerceptionEnabled(
					!InteractionMode().nearbyScanRequested());
				break;
			case OS0UICommandIntent::TOGGLE_ENVIRONMENT:
			{
				const OS0WindowHandle window =
					gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT);
				const BOOLEAN opening =
					!gUIRuntime.windowManager().requestedVisible(window);
				gUIRuntime.windowManager().toggle(window);
				if (opening)
				{
					gUIRuntime.windowManager().show(
						gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
					if (gInspectedGridNo >= 0 && gInspectedGridNo < WORLD_MAX)
						RefreshEnvironmentTarget(gInspectedGridNo, gInspectedLevel,
							gInspectedTileIndex);
				}
				break;
			}
			case OS0UICommandIntent::OPEN_ASSET_LIBRARY:
				InteractionMode().beginInteraction(
					OS0InteractionSurface::ENVIRONMENT);
				gDebugLibraryMode = DebugLibraryMode::ASSETS;
				gUIRuntime.show(OS0UIPanel::ASSET_LIBRARY);
				break;
			case OS0UICommandIntent::TOGGLE_REALTIME_EDITOR:
			{
				InteractionMode().beginInteraction(
					OS0InteractionSurface::ENVIRONMENT);
				OS0WindowHandle const editor = gUIRuntime.managedId(
					OS0UIWindow::REALTIME_EDITOR);
				gUIRuntime.windowManager().toggle(editor);
				if (gUIRuntime.windowManager().requestedVisible(editor) && gpItemPointer)
					CancelItemPointer();
				ApplyCursorTool(gUIRuntime.windowManager().requestedVisible(editor) ?
					ContextAction::INSPECT : ContextAction::MOVE);
				break;
			}
			case OS0UICommandIntent::TOGGLE_STRATEGY:
				gUIRuntime.windowManager().toggle(
					gUIRuntime.managedId(FloatingPanelId::SECTOR));
				break;
			case OS0UICommandIntent::OPEN_ICON_LIBRARY:
				InteractionMode().beginInteraction(
					OS0InteractionSurface::ENVIRONMENT);
				gDebugLibraryMode = DebugLibraryMode::ICONS;
				gUIRuntime.show(OS0UIPanel::ASSET_LIBRARY);
				break;
		}
		RecordFeedbackEvent(ST::format("TOOLBOX MODULE {}",
			static_cast<UINT8>(module)));
		PositionBagRegions();
		SaveUILayout();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	BOOLEAN IsToolboxModuleActive(ToolboxModule const module)
	{
		switch (module)
		{
			case ToolboxModule::TACTICAL:
				return CombatModeActive();
			case ToolboxModule::CHARACTER:
				return (gCharacterActionFanVisible && gContextSoldier &&
					gContextSoldier->bTeam == OUR_TEAM) ||
					gEquipmentExplodedVisible || gBagVisible;
			case ToolboxModule::STEALTH:
				return (GetSelectedMan() && GetSelectedMan()->bStealthMode) ||
					(gCharacterActionFanVisible && gContextSoldier);
			case ToolboxModule::OBJECT:
				return InteractionMode().nearbyScanRequested();
			case ToolboxModule::WORLD:
				return gUIRuntime.windowManager().requestedVisible(
					gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
			case ToolboxModule::ASSETS:
				return gGodLibraryVisible &&
					gDebugLibraryMode == DebugLibraryMode::ASSETS;
			case ToolboxModule::TERRAIN:
				return gUIRuntime.windowManager().requestedVisible(
					gUIRuntime.managedId(FloatingPanelId::REALTIME_EDITOR));
			case ToolboxModule::STRATEGY:
				return gUIRuntime.windowManager().requestedVisible(
					gUIRuntime.managedId(FloatingPanelId::SECTOR));
			case ToolboxModule::SANDBOX:
				return gGodLibraryVisible &&
					gDebugLibraryMode == DebugLibraryMode::ICONS;
			case ToolboxModule::COUNT:
				return FALSE;
		}
		return FALSE;
	}

	void DrawOrb()
	{
		const UINT16 black = Get16BPPColor(FROMRGB(2, 3, 3));
		const UINT16 dark = Get16BPPColor(FROMRGB(8, 10, 9));
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 bright = Get16BPPColor(FROMRGB(205, 12, 12));
		const BOOLEAN open = gMultiToolExpanded;
		const BOOLEAN combat = CombatModeActive();
		const BOOLEAN showCombat = !gTutorialActive && open;
		const BOOLEAN showModules = !gTutorialActive && !gAimAutoCollapsed &&
			!gTalkDocked && open;
		const INT16 orbX = gOrbRegion.RegionTopLeftX;
		const INT16 orbY = gOrbRegion.RegionTopLeftY;
		const INT16 orbW = std::max<INT16>(1, gOrbRegion.W());
		const INT16 modeX = gCombatModeRegion.RegionTopLeftX;
		const INT16 modeW = std::max<INT16>(1, gCombatModeRegion.W());
		INT16 left = orbX;
		INT16 right = static_cast<INT16>(orbX + orbW);
		if (showCombat)
		{
			left = std::min<INT16>(left, modeX);
			right = std::max<INT16>(right, modeX + modeW);
		}
		if (showModules)
		{
			for (MOUSE_REGION const& region : gPanelDockRegions)
			{
				left = std::min<INT16>(left, region.RegionTopLeftX);
				right = std::max<INT16>(right, region.RegionBottomRightX);
			}
		}
		// The former screen-wide dock is now a compact physical object over the
		// live world. Repaint only its current footprint so scrolling and dragging
		// cannot leave a black strip or ghost pixels behind.
		ColorFillVideoSurfaceArea(FRAME_BUFFER, left, orbY,
			right, orbY + COMMAND_BAR_H, black);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, left, orbY, right, orbY, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, orbX + 2, orbY + 3,
			orbX + orbW - 2, orbY + COMMAND_BAR_H - 3, black);
		OutlineBox(orbX + 2, orbY + 3, orbW - 3,
			COMMAND_BAR_H - 5, open ? bright : red);
		OS0UIAssets().draw(OS0UIIcon::KEYRING, FRAME_BUFFER,
			orbX + std::max<INT16>(2, (orbW - 20) / 2), orbY + 8);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(open ? FONT_WHITE : FONT_MCOLOR_RED);
		gOrbRegion.SetFastHelpText(
			open ? "OS//0 MULTITOOL / DRAG: MOVE / DOUBLE CLICK: MINIMIZE" :
			"OS//0 MULTITOOL / DRAG: MOVE / DOUBLE CLICK: EXPAND");
		if (showCombat)
		{
			ColorFillVideoSurfaceArea(FRAME_BUFFER, modeX + 1, orbY + 3,
				modeX + modeW - 2, orbY + COMMAND_BAR_H - 3, dark);
			OS0UIAssets().draw(combat ? OS0UIIcon::TARGET : OS0UIIcon::WALK,
				FRAME_BUFFER, modeX + std::max<INT16>(2, (modeW - 20) / 2),
				orbY + 8);
			DrawIconCorners(modeX + 1, orbY + 3,
				std::max<INT16>(8, modeW - 2), COMMAND_BAR_H - 6,
				combat ? bright : red);
			gCombatModeRegion.SetFastHelpText(combat ?
				"COMBAT ACTIVE / CLICK: NORMAL MODE" :
				"NORMAL ACTIVE / CLICK: COMBAT / WASD MOVE / LMB FIRE");
		}
		if (showModules)
		{
			for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
			{
				MOUSE_REGION& region = gPanelDockRegions[i];
				ToolboxModule const module = MULTITOOL_MODULES[i];
				const INT16 x = region.RegionTopLeftX;
				const INT16 w = region.W();
				const BOOLEAN hot = gusMouseXPos >= x &&
					gusMouseXPos <= region.RegionBottomRightX &&
					gusMouseYPos >= orbY &&
					gusMouseYPos <= orbY + COMMAND_BAR_H;
				ColorFillVideoSurfaceArea(FRAME_BUFFER,
					x, orbY + 3, x + w, orbY + COMMAND_BAR_H - 3, dark);
				OS0UIAssets().draw(MULTITOOL_ICONS[i], FRAME_BUFFER,
					x + std::max<INT16>(2, (w - 20) / 2), orbY + 8);
				if (hot || IsToolboxModuleActive(module))
					DrawIconCorners(x + 2, orbY + 3,
					std::max<INT16>(8, w - 4), COMMAND_BAR_H - 6, bright);
				region.SetFastHelpText(ST::format("{} / {}",
					MULTITOOL_LABELS[i], GetOS0UICommandDescriptor(module).tooltip));
			}
		}
		InvalidateRegion(left, orbY, right, orbY + COMMAND_BAR_H);
	}

	void DrawToolbox()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::TOOLBOX)];
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::TOOLBOX)) || gTutorialActive || gAimAutoCollapsed) return;
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 bright = Get16BPPColor(FROMRGB(205, 12, 12));
		DrawFloatingPanelShell(panel, FloatingPanelId::TOOLBOX,
			GetOS0UIWindowDescriptor(FloatingPanelId::TOOLBOX).title);
		ST::string hotLabel = gpItemPointer ? "ITEM HELD / ACTIONS RETURNS IT" :
			ST::format("{} / {}{}",
				OS0InteractionStateName(InteractionMode().state()),
				OS0InteractionSurfaceName(InteractionMode().surface()),
				InteractionMode().nearbyScanEnabled() ? " / SCAN" : "");
		for (size_t i = 0; i < gToolboxRegions.size(); ++i)
		{
			MOUSE_REGION const& region = gToolboxRegions[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 30 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 26;
			const BOOLEAN active = IsToolboxModuleActive(
				static_cast<ToolboxModule>(i));
			OS0UICommandDescriptor const& descriptor =
				GetOS0UICommandDescriptor(static_cast<ToolboxModule>(i));
			OS0UIAssets().draw(descriptor.icon, FRAME_BUFFER, x + 5, y + 3);
			if (active || hot) DrawIconCorners(x, y, 30, 26,
				hot ? bright : red);
			gToolboxRegions[i].SetFastHelpText(descriptor.tooltip);
			if (hot) hotLabel = descriptor.tooltip;
		}
		SetFontForeground(FONT_MCOLOR_DKGRAY);
		MPrint(panel.x + 7, panel.y + panel.h - 12, hotLabel.left(22));
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawArtworkBrand()
	{
		if (gTutorialActive || gAimAutoCollapsed) return;
		const INT16 x = 8;
		const INT16 y = 7;
		const UINT16 dark = Get16BPPColor(FROMRGB(3, 5, 5));
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(88, 10, 8));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y,
			x + BRAND_W - 1, y + BRAND_H - 1, dark);
		OutlineBox(x, y, BRAND_W, BRAND_H, muted);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + 31, y + 1, red);
		SetFont(FONT10ARIALBOLD);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(x + 7, y + 5, "ESCAPE FROM");
		SetFontForeground(FONT_WHITE);
		MPrint(x + 7, y + 17, "ARULCO");
		SetFont(TINYFONT1);
		SetFontForeground(FONT_MCOLOR_DKGRAY);
		MPrint(x + 70, y + 22, "TACTICAL SURVIVAL / OS0");
		InvalidateRegion(x, y, x + BRAND_W, y + BRAND_H);
	}

	void OutlineBox(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour)
	{
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + w - 1, y, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + h - 1, x + w - 1, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - 1, y, x + w - 1, y + h - 1, colour);
	}

	void DrawIconCorners(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour)
	{
		constexpr INT16 arm = 5;
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + arm, y, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x, y + arm, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - arm - 1, y,
			x + w - 1, y, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - 1, y,
			x + w - 1, y + arm, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + h - 1,
			x + arm, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y + h - arm - 1,
			x, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - arm - 1, y + h - 1,
			x + w - 1, y + h - 1, colour);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + w - 1, y + h - arm - 1,
			x + w - 1, y + h - 1, colour);
	}

	void DrawOS0Shell()
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(78, 5, 5));
		const UINT16 dark = Get16BPPColor(FROMRGB(3, 5, 5));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX, gBagY,
			gBagX + PANE_W - 1, gBagY + BAG_H - 1, dark);
		OutlineBox(gBagX, gBagY, PANE_W, BAG_H, muted);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX, gBagY,
			gBagX + 30, gBagY + 1, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX + PANE_W - 31, gBagY,
			gBagX + PANE_W - 1, gBagY + 1, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gBagX + 1, gBagY + 18,
			gBagX + PANE_W - 2, gBagY + 18, muted);
		DrawIconCorners(gBagX + 4, gBagY + 22, PANE_W - 8, BAG_H - 27,
			Get16BPPColor(FROMRGB(35, 13, 10)));
	}

	void DrawFloatingPanelShell(FloatingPanel const& panel, FloatingPanelId id,
		const ST::string& title)
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(78, 5, 5));
		const UINT16 dark = Get16BPPColor(FROMRGB(3, 5, 5));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x, panel.y,
			panel.x + panel.w - 1, panel.y + panel.h - 1, dark);
		OutlineBox(panel.x, panel.y, panel.w, panel.h, muted);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x, panel.y,
			panel.x + 22, panel.y, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x + panel.w - 23, panel.y,
			panel.x + panel.w - 1, panel.y, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x + 1,
			panel.y + PANEL_HEADER_H, panel.x + panel.w - 2,
			panel.y + PANEL_HEADER_H, muted);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		OS0UIAssets().draw(GetOS0UIWindowDescriptor(id).icon,
			FRAME_BUFFER, panel.x + 5, panel.y + 1);
		MPrint(panel.x + 28, panel.y + 5, title);
		MPrint(panel.x + panel.w - 13, panel.y + 5, "X");
	}

	void DrawEnvironmentPanel()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::ENVIRONMENT)];
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::ENVIRONMENT)) || gTutorialActive ||
			gAimAutoCollapsed) return;
		DrawFloatingPanelShell(panel, FloatingPanelId::ENVIRONMENT,
			GetOS0UIWindowDescriptor(FloatingPanelId::ENVIRONMENT).title);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(panel.x + 8, panel.y + 24, gEnvironmentTitle.left(34));
		if (gEnvironmentEntryCount == 0)
		{
			SetFontForeground(FONT_MCOLOR_DKGRAY);
			MPrint(panel.x + 8, panel.y + 49,
				"SELECT OR APPROACH A WORLD OBJECT");
		}
		const UINT16 bright = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 disabled = Get16BPPColor(FROMRGB(38, 38, 38));
		for (size_t i = 0; i < gEnvironmentEntryCount; ++i)
		{
			MOUSE_REGION const& region = gEnvironmentSkillRegions[i];
			ContextEntry const& entry = gEnvironmentEntries[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const BOOLEAN hot = gusMouseXPos >= x &&
				gusMouseXPos <= region.RegionBottomRightX &&
				gusMouseYPos >= y && gusMouseYPos <= region.RegionBottomRightY;
			DrawContextActionIcon(entry.action, x + 2, y + 2);
			if (hot || !entry.enabled)
				DrawIconCorners(x, y, region.W(), region.H(),
					entry.enabled ? bright : disabled);
			SetFontForeground(entry.enabled ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(x + 27, y + 9, ST::string(ContextActionName(entry.action)).left(8));
			gEnvironmentSkillRegions[i].SetFastHelpText(ST::format("{}\n{}",
				entry.label, ContextActionExplanation(entry.action)));
		}
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawSectorPanel()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::SECTOR)];
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::SECTOR))) return;
		DrawFloatingPanelShell(panel, FloatingPanelId::SECTOR,
			ST::format("LIVE STRATEGY / DAY {} / {}:00", GetWorldDay(),
				GetWorldHour()));
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 bright = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 dark = Get16BPPColor(FROMRGB(12, 17, 17));
		const UINT16 friendly = Get16BPPColor(FROMRGB(75, 94, 70));
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		constexpr std::array<const char*, 4> tabs{{
			"BASE", "ARULCO", "TEAM", "REPORT"
		}};
		for (size_t i = 0; i < tabs.size(); ++i)
		{
			const INT16 x = panel.x + 8 + static_cast<INT16>(i) * 72;
			OutlineBox(x, panel.y + 23, 68, 19,
				static_cast<size_t>(gSectorPanelMode) == i ? bright : red);
			SetFontForeground(static_cast<size_t>(gSectorPanelMode) == i ?
				FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(x + 6, panel.y + 29, tabs[i]);
		}

		if (gSectorPanelMode == SectorPanelMode::BASE)
		{
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(panel.x + 8, panel.y + 48,
				ST::format("{} / STOCK  W{} S{} R{} E{}", gWorldSector.AsShortString(),
					SectorResource(ResourceKind::TIMBER),
					SectorResource(ResourceKind::STONE),
					SectorResource(ResourceKind::SCRAP),
					SectorResource(ResourceKind::SOIL)));
			for (size_t i = 0; i < gSectorUpgrades.size(); ++i)
			{
				SectorUpgrade const& upgrade = gSectorUpgrades[i];
				const INT16 y = panel.y + 71 + static_cast<INT16>(i) * 39;
				const BOOLEAN built = HasUpgrade(upgrade);
				const BOOLEAN ready = CanBuildUpgrade(upgrade);
				OutlineBox(panel.x + 8, y, panel.w - 16, 34, ready ? bright : red);
				SetFontForeground(built ? FONT_MCOLOR_RED :
					ready ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
				MPrint(panel.x + 13, y + 5,
					ST::format("{} {}", built && i == 0 ? "[REST]" :
						built ? "[BUILT]" : "[BUILD]", upgrade.name));
				SetFontForeground(FONT_MCOLOR_LTGRAY);
				MPrint(panel.x + 13, y + 19,
					built ? upgrade.benefit : ST::format("W{} S{} R{} E{} / {}",
						upgrade.cost[static_cast<size_t>(ResourceKind::TIMBER)],
						upgrade.cost[static_cast<size_t>(ResourceKind::STONE)],
						upgrade.cost[static_cast<size_t>(ResourceKind::SCRAP)],
						upgrade.cost[static_cast<size_t>(ResourceKind::SOIL)],
						upgrade.benefit));
			}
			SetFontForeground(FONT_MCOLOR_DKGRAY);
			MPrint(panel.x + 8, panel.y + panel.h - 17,
				"MATERIAL: DOUBLE-CLICK TO DEPOSIT");
		}
		else if (gSectorPanelMode == SectorPanelMode::MAP)
		{
			const INT16 mapX = panel.x + 12;
			const INT16 mapY = panel.y + 55;
			for (INT16 row = 0; row < 16; ++row)
			{
				for (INT16 column = 0; column < 16; ++column)
				{
					SGPSector const sector(column + 1, row + 1, 0);
					StrategicMapElement const& strategic =
						StrategicMap[sector.AsStrategicIndex()];
					UINT16 colour = strategic.fEnemyControlled ? red : friendly;
					ColorFillVideoSurfaceArea(FRAME_BUFFER,
						mapX + column * STRATEGIC_CELL + 1,
						mapY + row * STRATEGIC_CELL + 1,
						mapX + column * STRATEGIC_CELL + STRATEGIC_CELL - 2,
						mapY + row * STRATEGIC_CELL + STRATEGIC_CELL - 2, colour);
					if (sector == gWorldSector || sector == gStrategicSelectedSector)
						OutlineBox(mapX + column * STRATEGIC_CELL,
							mapY + row * STRATEGIC_CELL, STRATEGIC_CELL,
							STRATEGIC_CELL, sector == gWorldSector ? bright :
							Get16BPPColor(FROMRGB(210, 190, 145)));
				}
			}
			const INT16 infoX = mapX + 16 * STRATEGIC_CELL + 12;
			StrategicMapElement const& selected =
				StrategicMap[gStrategicSelectedSector.AsStrategicIndex()];
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(infoX, mapY, "SECTOR");
			SetFontForeground(FONT_WHITE);
			MPrint(infoX, mapY + 15, gStrategicSelectedSector.AsShortString());
			SetFontForeground(selected.fEnemyControlled ?
				FONT_MCOLOR_RED : FONT_MCOLOR_LTGRAY);
			MPrint(infoX, mapY + 34, selected.fEnemyControlled ?
				"HOSTILE" : "CONTROLLED");
			INT16 teamCount = 0;
			for (INT32 id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
				id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
			{
				SOLDIERTYPE const& soldier = GetMan(id);
				if (soldier.bActive && soldier.bLife > 0 &&
					soldier.sSector == gStrategicSelectedSector) ++teamCount;
			}
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(infoX, mapY + 53, ST::format("TEAM {}", teamCount));
			SECTORINFO const& sectorInfo =
				SectorInfo[gStrategicSelectedSector.AsByte()];
			const UINT16 enemyCount = static_cast<UINT16>(sectorInfo.ubNumAdmins) +
				sectorInfo.ubNumTroops + sectorInfo.ubNumElites;
			const UINT16 militiaCount =
				static_cast<UINT16>(sectorInfo.ubNumberOfCivsAtLevel[GREEN_MILITIA]) +
				sectorInfo.ubNumberOfCivsAtLevel[REGULAR_MILITIA] +
				sectorInfo.ubNumberOfCivsAtLevel[ELITE_MILITIA];
			SetFontForeground(enemyCount > 0 ? FONT_MCOLOR_RED : FONT_MCOLOR_DKGRAY);
			MPrint(infoX, mapY + 68, ST::format("ENEMY {}", enemyCount));
			SetFontForeground(militiaCount > 0 ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(infoX, mapY + 82, ST::format("MILITIA {}", militiaCount));
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(infoX, mapY + 98, "LIVE MINIMAP");
			OutlineBox(infoX, mapY + 113, RADAR_WINDOW_WIDTH + 4,
				RADAR_WINDOW_HEIGHT + 4, dark);
			BlitRadarScreenImage(FRAME_BUFFER, infoX + 2, mapY + 115);
			SetFontForeground(FONT_MCOLOR_DKGRAY);
			MPrint(infoX, mapY + 164, "REAL MAP ASSET");
		}
		else if (gSectorPanelMode == SectorPanelMode::TEAM)
		{
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(panel.x + 8, panel.y + 48, "OPERATORS / CLICK TO SELECT & CENTER");
			INT16 row = 0;
			for (INT32 id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
				id <= gTacticalStatus.Team[OUR_TEAM].bLastID && row < 8; ++id)
			{
				SOLDIERTYPE const& soldier = GetMan(id);
				if (!soldier.bActive || soldier.bLife <= 0) continue;
				const INT16 y = panel.y + 55 + row * 18;
				OutlineBox(panel.x + 8, y, panel.w - 16, 17,
					&soldier == GetSelectedMan() ? bright : red);
				SetFontForeground(&soldier == GetSelectedMan() ?
					FONT_WHITE : FONT_MCOLOR_LTGRAY);
				MPrint(panel.x + 13, y + 5,
					ST::format("{}  HP {}/{}  {}{}", soldier.name.left(13),
						soldier.bLife, soldier.bLifeMax, soldier.sSector.AsShortString(),
						soldier.fBetweenSectors ? " / MOVING" : ""));
				++row;
			}
			if (row == 0)
			{
				SetFontForeground(FONT_MCOLOR_DKGRAY);
				MPrint(panel.x + 12, panel.y + 66, "NO ACTIVE OPERATORS");
			}
		}
		else
		{
			const INT16 x = panel.x + 8;
			const INT16 y = panel.y + 49;
			OutlineBox(x, y, panel.w - 16, 18, red);
			OutlineBox(x, y + 25, panel.w - 16, 96,
				gFeedbackEditing ? bright : red);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(x + 5, y + 5,
				ST::format("CATEGORY  < {} >",
					gFeedbackCategories[gFeedbackCategory]).left(39));
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			if (gFeedbackText.empty())
			{
				MPrint(x + 6, y + 36, "CLICK HERE AND DESCRIBE WHAT HAPPENED.");
				MPrint(x + 6, y + 50, "THE REPORT INCLUDES THE CURRENT EVENT LOG.");
			}
			else
			{
				ST::utf32_buffer const chars = gFeedbackText.to_utf32();
				size_t cursor = chars.size() > 190 ? chars.size() - 190 : 0;
				for (INT16 line = 0; line < 5 && cursor < chars.size(); ++line)
				{
					const size_t limit = std::min(chars.size(), cursor + 38);
					size_t end = cursor;
					while (end < limit && chars[end] != U'\n') ++end;
					MPrint(x + 6, y + 34 + line * 14,
						end > cursor ? ST::string::from_utf32(
							chars.data() + cursor, end - cursor) : ST::string{});
					cursor = end < chars.size() && chars[end] == U'\n' ? end + 1 : end;
				}
			}
			SetFontForeground(gFeedbackEditing ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
			MPrint(x + 2, panel.y + panel.h - 45, gFeedbackStatus.left(40));
			OutlineBox(x, panel.y + panel.h - 28, 76, 19, red);
			OutlineBox(panel.x + panel.w - 126, panel.y + panel.h - 28,
				118, 19, bright);
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			MPrint(x + 18, panel.y + panel.h - 23, "CLEAR");
			SetFontForeground(FONT_WHITE);
			MPrint(panel.x + panel.w - 108, panel.y + panel.h - 23,
				"SAVE REPORT");
		}
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

		switch (gUIRuntime.creatorStage())
		{
			case OS0CreatorStage::WELCOME:
				TutorialText(15, 30, "WELCOME TO ARULCO", FONT_MCOLOR_RED);
				TutorialText(15, 47, "No extraction. No support. This sector is yours.");
				TutorialText(15, 59, "First we establish your operator identity.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "BEGIN", FONT_MCOLOR_RED);
				break;
			case OS0CreatorStage::IDENTITY:
				TutorialText(15, 24, "IDENTITY / ENTER YOUR CALLSIGN", FONT_MCOLOR_RED);
				TutorialText(15, 43,
					ST::format("> {}_", gCreatorModel.callsign()), FONT_WHITE);
				TutorialText(15, 61, "LIVE BODY ASSET / CLICK TO APPLY", FONT_MCOLOR_RED);
				for (size_t i = 0; i < gTutorialBodyTypes.size(); ++i)
				{
					const INT16 x = 14 + static_cast<INT16>(i) * 108;
					const BOOLEAN selected =
						gCreatorModel.bodyType() == gTutorialBodyTypes[i];
					DrawIconCorners(gBagX + x, gBagY + 88, 103, 30,
						selected ? Get16BPPColor(FROMRGB(210, 24, 18)) :
						Get16BPPColor(FROMRGB(78, 69, 48)));
					TutorialText(x + 6, 98, gTutorialBodyNames[i],
						selected ? FONT_MCOLOR_RED : FONT_WHITE);
				}
				TutorialText(15, 128,
					"Changes the live tactical sprite. Operator voice: MUTED.",
					FONT_MCOLOR_LTGRAY);
				TutorialText(15, 143, "Type a name. ENTER or CONFIRM continues.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "CONFIRM", FONT_MCOLOR_RED);
				break;
			case OS0CreatorStage::ATTRIBUTES:
				TutorialText(15, 9, "BUILD OPERATOR / FREE PARAMETERS", FONT_MCOLOR_RED);
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				SetFontForeground(FONT_WHITE);
				MPrint(gInventoryX + PANE_W - 105, gInventoryY + 9,
					ST::format("POINTS {}", gCreatorModel.points()));
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
					MPrint(baseX + 113, baseY + y,
						ST::format("{}", gCreatorModel.stats()[i]));
					SetFontForeground(FONT_MCOLOR_RED);
					MPrint(baseX + 147, baseY + y, "+");
				}
				TutorialText(CONTINUE_X + 5, BAG_H - 17, "CONFIRM STATS", FONT_MCOLOR_RED);
				break;
			case OS0CreatorStage::TRAITS:
				TutorialText(15, 9, "SPECIALTIES / CHOOSE UP TO TWO", FONT_MCOLOR_RED);
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				for (size_t i = 0; i < gTutorialTraitValues.size(); ++i)
				{
					const BOOLEAN right = i >= 8;
					const INT16 row = static_cast<INT16>(right ? i - 8 : i);
					auto const& traits = gCreatorModel.traits();
					const BOOLEAN selected = traits[0] == gTutorialTraitValues[i] ||
						traits[1] == gTutorialTraitValues[i];
					SetFontForeground(selected ? FONT_MCOLOR_RED : FONT_WHITE);
					MPrint(gBagX + (right ? 230 : 0) + 15,
						gBagY + 27 + row * 14,
						ST::format("{} {}", selected ? "[X]" : "[ ]",
							gTutorialTraitNames[i]));
				}
				TutorialText(15, BAG_H - 32, "0-2 FREE CHOICES / NO CLASS", FONT_MCOLOR_LTGRAY);
				TutorialText(CONTINUE_X + 5, BAG_H - 17, "CONFIRM SKILLS", FONT_MCOLOR_RED);
				break;
			case OS0CreatorStage::CONTROLS:
				TutorialText(15, 25, "LIVE CONTROL", FONT_MCOLOR_RED);
				TutorialText(15, 42, "LEFT: move / select       DOUBLE: inspect");
				TutorialText(15, 55, "F: perceive object + open its actions");
				TutorialText(15, 68, "RIGHT: context     MIDDLE: cycle action");
				TutorialText(15, 81, "SHIFT+MIDDLE: cancel / CTRL+MIDDLE: center");
				TutorialText(15, 94, "Use PICK UP on nearby objects. Reach the red marker.");
				TutorialText(CONTINUE_X + 3, BAG_H - 17, "ENTER ARULCO", FONT_MCOLOR_RED);
				break;
			case OS0CreatorStage::COMPLETE:
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
		if (gAnimatedMercPreview && gAnimatedMercPreviewSoldier == soldier)
		{
			BltVideoSurface(FRAME_BUFFER, gAnimatedMercPreview,
				gBagX + 8, gBagY + 21, nullptr);
			DrawIconCorners(gBagX + 7, gBagY + 20, 50, 64,
				Get16BPPColor(FROMRGB(92, 8, 8)));
		}

		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_WHITE);
		MPrint(gBagX + 8, gBagY + 5,
			ST::format("CHARACTER / {}", soldier->name).left(25));
		SetFontForeground(FONT_MCOLOR_DKGRAY);
		MPrint(gBagX + 10, gBagY + 74, "LIVE");

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
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(gInventoryX + INVENTORY_X + 48, gInventoryY + 83,
			"HAND 1 / MAIN");
		MPrint(gInventoryX + INVENTORY_X + 48, gInventoryY + 110,
			"HAND 2 / OFF");
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
				ST::string help = ST::format("{} / EMPTY\nDrop a compatible item here.",
					OS0InventorySlotName(slot.slot));
				if (object.usItem != NOTHING)
				{
					ItemModel const* const item = GCM->getItem(object.usItem);
					help = ST::format("{} / {}\nCondition: {}%   Count: {}\n"
						"RMB: relational actions",
						OS0InventorySlotName(slot.slot), item->getName(), object.bStatus[0],
						object.ubNumberOfObjects);
					INVRenderItem(FRAME_BUFFER, soldier, object,
						gInventoryX + slot.x, gInventoryY + slot.y, slot.w, slot.h,
						DIRTYLEVEL2, 0, SGP_TRANSPARENT);
				}
				if (slot.slot == SECONDHANDPOS &&
					soldier->inv[HANDPOS].usItem != NOTHING &&
					GCM->getItem(soldier->inv[HANDPOS].usItem)->isTwoHanded())
					help = "HAND 2 / RESERVED\nPrimary item requires both hands.";
				if (help != gSlotHelp[i])
				{
					gSlotHelp[i] = help;
					gSlotRegions[i].SetFastHelpText(help);
				}
			}
		}

	}

	void DrawLootMode()
	{
		// Object contents are spatial: the real world sprite remains the centre and
		// its contained objects unfold around it.  This deliberately bypasses the
		// old rectangular "container inventory" window below.
		if (gLootVisible && gLootGridNo >= 0 && gLootGridNo < WORLD_MAX)
		{
			size_t slotCount = 0;
			while (slotCount < gLootWorldItems.size() &&
				gLootWorldItems[slotCount] >= 0) ++slotCount;
			const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
			// The real container is already visible in the world. Do not paint a
			// scaled dark-backed duplicate over it.

			for (size_t slot = 0; slot < slotCount; ++slot)
			{
				const INT32 itemIndex = gLootWorldItems[slot];
				if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
					continue;
				WORLDITEM const& worldItem = GetWorldItem(itemIndex);
				MOUSE_REGION const& region = gLootRegions[slot];
				const INT16 x = region.RegionTopLeftX;
				const INT16 y = region.RegionTopLeftY;
				const INT16 cx = x + 29;
				const INT16 cy = y + 14;
				const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 58 &&
					gusMouseYPos >= y && gusMouseYPos <= y + 27;
				if (hot) DrawIconCorners(x, y, 59, 28, red);
				DrawWorldItemSprite(worldItem.o, cx, cy);
			}
			INT16 dirtyLeft = gsVIEWPORT_END_X;
			INT16 dirtyTop = gsVIEWPORT_WINDOW_END_Y;
			INT16 dirtyRight = gsVIEWPORT_START_X;
			INT16 dirtyBottom = gsVIEWPORT_WINDOW_START_Y;
			for (size_t i = 0; i < slotCount; ++i)
			{
				MOUSE_REGION const& region = gLootRegions[i];
				dirtyLeft = std::min(dirtyLeft, region.RegionTopLeftX);
				dirtyTop = std::min(dirtyTop, region.RegionTopLeftY);
				dirtyRight = std::max(dirtyRight, region.RegionBottomRightX);
				dirtyBottom = std::max(dirtyBottom, region.RegionBottomRightY);
			}
			InvalidateRegion(dirtyLeft - 2, dirtyTop - 2,
				dirtyRight + 2, dirtyBottom + 2);
			return;
		}
	}

	void DrawBag()
	{
		SOLDIERTYPE* const inventorySoldier = gInventorySoldier ?
			gInventorySoldier : GetSelectedMan();
		if (gTutorialActive)
		{
			if (!gBagVisible) return;
			DrawTutorial();
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

	void DrawExplodedEquipment()
	{
		if (!gEquipmentExplodedVisible || gContextVisible || !gEquipmentSoldier ||
			!CanAccessSoldierContents(gEquipmentSoldier)) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		constexpr std::array<OS0UIIcon, 7> slotGlyphs{{
			OS0UIIcon::LOOK, OS0UIIcon::LOOK, OS0UIIcon::LOOK,
			OS0UIIcon::HAND, OS0UIIcon::WALK, OS0UIIcon::TARGET,
			OS0UIIcon::PUNCH
		}};
		constexpr std::array<const char*, 7> slotTags{{
			"HELM", "FACE 1", "FACE 2", "VEST", "LEGS", "HAND 1", "HAND 2"
		}};
		INT16 anchorX;
		INT16 anchorY;
		if (!GetActorDisplayAnchor(gEquipmentSoldier, anchorX, anchorY)) return;
		for (size_t i = 0; i < gEquipmentRegions.size(); ++i)
		{
			MOUSE_REGION const& region = gEquipmentRegions[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const INT16 slotCentreX = x + 17;
			const INT16 slotCentreY = y + 12;
			ColorFillVideoSurfaceArea(FRAME_BUFFER,
				std::min(slotCentreX, anchorX), slotCentreY,
				std::max(slotCentreX, anchorX), slotCentreY, mutedRed);
			const INT16 bodyY = static_cast<INT16>(anchorY - 9);
			ColorFillVideoSurfaceArea(FRAME_BUFFER, anchorX,
				std::min(slotCentreY, bodyY), anchorX,
				std::max(slotCentreY, bodyY), mutedRed);
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 34 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 25;
			DrawIconCorners(x, y, 34, 25, hot ? red : mutedRed);
			const INT8 slot = gExplodedEquipmentSlots[i];
			OBJECTTYPE const& object = gEquipmentSoldier->inv[slot];
			ST::string help = gExplodedEquipmentLabels[i];
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground((slot == HANDPOS || slot == SECONDHANDPOS) ?
				FONT_MCOLOR_RED : FONT_MCOLOR_LTGRAY);
			MPrint(x + 2, y - 8, slotTags[i]);
			if (object.usItem != NOTHING)
			{
				ItemModel const* const item = GCM->getItem(object.usItem);
				help = ST::format("{} / {} / {}%", gExplodedEquipmentLabels[i],
					item->getName(), object.bStatus[0]);
				INVRenderItem(FRAME_BUFFER, gEquipmentSoldier, object,
					x, y, 34, 25, DIRTYLEVEL2, 0, SGP_TRANSPARENT);
			}
			else
			{
				OS0UIAssets().draw(slotGlyphs[i], FRAME_BUFFER, x + 7, y + 3);
			}
			if (slot == SECONDHANDPOS &&
				gEquipmentSoldier->inv[HANDPOS].usItem != NOTHING &&
				GCM->getItem(gEquipmentSoldier->inv[HANDPOS].usItem)->isTwoHanded())
			{
				help = "HAND 2 / RESERVED BY TWO-HAND ITEM";
				DrawIconCorners(x, y, 34, 25, Get16BPPColor(FROMRGB(52, 52, 52)));
				SetFontForeground(FONT_MCOLOR_DKGRAY);
				MPrint(x + 9, y + 9, "2H");
			}
			if (help != gEquipmentHelp[i])
			{
				gEquipmentHelp[i] = help;
				gEquipmentRegions[i].SetFastHelpText(help);
			}
		}

		const INT16 packX = gEquipmentPackRegion.RegionTopLeftX;
		const INT16 packY = gEquipmentPackRegion.RegionTopLeftY;
		OS0UIAssets().draw(OS0UIIcon::HAND, FRAME_BUFFER,
			packX + 13, packY + 3);
		const BOOLEAN packHot = gusMouseXPos >= packX && gusMouseXPos <= packX + 46 &&
			gusMouseYPos >= packY && gusMouseYPos <= packY + 25;
		DrawIconCorners(packX, packY, 46, 25,
			packHot ? red : mutedRed);
		gEquipmentPackRegion.SetFastHelpText(
			ST::format("BACKPACK / LOAD {}% / HARD LIMIT 125% / CLICK TO TOGGLE",
				CalculateCarriedWeight(gEquipmentSoldier)));
		InvalidateRegion(gEquipmentCentreX - 118, gEquipmentCentreY - 155,
			gEquipmentCentreX + 128, gEquipmentCentreY + 4);
	}

	void DrawItemTransferIntents()
	{
		SOLDIERTYPE* const target = BoundItemTransferTarget();
		if (gContextVisible || !gpItemPointer || !target ||
			!CanAccessSoldierContents(target)) return;
		ItemTransferPolicyDecision const decision =
			CurrentItemTransferDecision(target);
		if (decision.actions.empty()) return;
		INT16 anchorX;
		INT16 anchorY;
		if (!GetActorDisplayAnchor(target, anchorX, anchorY)) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(92, 8, 8));
		INT16 dirtyLeft = anchorX;
		INT16 dirtyTop = anchorY;
		INT16 dirtyRight = anchorX;
		INT16 dirtyBottom = anchorY;
		static std::array<ST::string,
			gOS0ItemTransferIntents.size()> helpCache{};
		for (size_t i = 0; i < gOS0ItemTransferIntents.size(); ++i)
		{
			ItemTransferIntent const intent = gOS0ItemTransferIntents[i].intent;
			const BOOLEAN preferred = decision.hasPreferred &&
				decision.preferred == intent;
			if (!decision.allows(intent) ||
				(!preferred && !gItemTransferMoreVisible)) continue;
			MOUSE_REGION const& region = gItemTransferIntentRegions[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const INT16 cx = x + 14;
			const INT16 cy = y + 14;
			dirtyLeft = std::min(dirtyLeft, x);
			dirtyTop = std::min<INT16>(dirtyTop, y - 11);
			dirtyRight = std::max<INT16>(dirtyRight, x + 96);
			dirtyBottom = std::max<INT16>(dirtyBottom, y + 28);
			ColorFillVideoSurfaceArea(FRAME_BUFFER, std::min(anchorX, cx), cy,
				std::max(anchorX, cx), cy, muted);
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 28 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 28;
			DrawIconCorners(x, y, 28, 28,
				(hot || preferred) ? red : muted);
			OS0UIAssets().draw(gOS0ItemTransferIntents[i].icon,
				FRAME_BUFFER, x + 4, y + 4);
			const ST::string relation = ItemTransferIntentLabel(target,
				intent);
			const ST::string help = ST::format("{}\n{}", relation,
				preferred ? "RECOMMENDED / CLICK TO APPLY" : "CLICK TO APPLY");
			if (helpCache[i] != help)
			{
				helpCache[i] = help;
				gItemTransferIntentRegions[i].SetFastHelpText(help);
			}
			if (hot)
			{
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				SetFontForeground(FONT_WHITE);
				MPrint(std::clamp<INT16>(x - 18, gsVIEWPORT_START_X,
					gsVIEWPORT_END_X - 95),
					std::max<INT16>(gsVIEWPORT_WINDOW_START_Y, y - 11),
					relation.left(32));
			}
		}
		if (decision.hasAlternatives())
		{
			MOUSE_REGION const& more = gItemTransferMoreRegion;
			const INT16 x = more.RegionTopLeftX;
			const INT16 y = more.RegionTopLeftY;
			const INT16 cx = x + 14;
			const INT16 cy = y + 14;
			dirtyLeft = std::min(dirtyLeft, x);
			dirtyTop = std::min<INT16>(dirtyTop, y - 11);
			dirtyRight = std::max<INT16>(dirtyRight, x + 88);
			dirtyBottom = std::max<INT16>(dirtyBottom, y + 28);
			ColorFillVideoSurfaceArea(FRAME_BUFFER, std::min(anchorX, cx), cy,
				std::max(anchorX, cx), cy, muted);
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 28 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 28;
			DrawIconCorners(x, y, 28, 28, hot ? red : muted);
			OS0UIAssets().draw(gItemTransferMoreVisible ? OS0UIIcon::CANCEL :
				OS0UIIcon::KEYRING, FRAME_BUFFER, x + 4, y + 4);
			gItemTransferMoreRegion.SetFastHelpText(gItemTransferMoreVisible ?
				"LESS OPTIONS / CLICK TO COLLAPSE" :
				"MORE OPTIONS / CLICK TO EXPAND");
			if (hot)
			{
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				SetFontForeground(FONT_WHITE);
				MPrint(std::clamp<INT16>(x - 18, gsVIEWPORT_START_X,
					gsVIEWPORT_END_X - 95),
					std::max<INT16>(gsVIEWPORT_WINDOW_START_Y, y - 11),
					gItemTransferMoreVisible ? "LESS OPTIONS" : "MORE OPTIONS");
			}
		}
		InvalidateRegion(dirtyLeft - 2, dirtyTop - 2,
			dirtyRight + 2, dirtyBottom + 2);
	}

	void DrawStackSplitDialog()
	{
		if (!gStackSplitVisible || !gStackSplitSoldier ||
			gStackSplitSlot < 0 || gStackSplitSlot >= NUM_INV_SLOTS ||
			!DeferredContextStillValid(gStackSplitIdentity) ||
			DeferredContextActor(gStackSplitIdentity) != gStackSplitSoldier) return;
		OBJECTTYPE const& object = gStackSplitSoldier->inv[gStackSplitSlot];
		// Invalid dialog ownership is repaired in UpdateOS0TacticalSession.
		// Rendering only consumes the validated snapshot for this frame.
		if (object.usItem == NOTHING || object.ubNumberOfObjects <= 1) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 bright = Get16BPPColor(FROMRGB(235, 25, 25));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gStackSplitX, gStackSplitY,
			gStackSplitX + 223, gStackSplitY + 81, dark);
		OutlineBox(gStackSplitX, gStackSplitY, 224, 82, red);
		INVRenderItem(FRAME_BUFFER, gStackSplitSoldier, object,
			gStackSplitX + 8, gStackSplitY + 20, 42, 27,
			DIRTYLEVEL2, 0, SGP_TRANSPARENT);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gStackSplitX + 8, gStackSplitY + 6, "MOVE STACK / QUANTITY");
		SetFontForeground(FONT_WHITE);
		MPrint(gStackSplitX + 56, gStackSplitY + 24,
			GCM->getItem(object.usItem)->getName().left(22));
		MPrint(gStackSplitX + 56, gStackSplitY + 38,
			ST::format("{} OF {}", gStackSplitAmount,
				object.ubNumberOfObjects));
		constexpr std::array<const char*, 5> labels{{ "-", "+", "ALL", "TAKE", "X" }};
		for (size_t i = 0; i < gStackSplitRegions.size(); ++i)
		{
			MOUSE_REGION const& region = gStackSplitRegions[i];
			const BOOLEAN hot = gusMouseXPos >= region.RegionTopLeftX &&
				gusMouseXPos <= region.RegionBottomRightX &&
				gusMouseYPos >= region.RegionTopLeftY &&
				gusMouseYPos <= region.RegionBottomRightY;
			OutlineBox(region.RegionTopLeftX, region.RegionTopLeftY,
				region.W(), region.H(), hot ? bright : red);
			SetFontForeground(hot ? FONT_WHITE : FONT_MCOLOR_LTGRAY);
			MPrint(region.RegionTopLeftX + 6, region.RegionTopLeftY + 8, labels[i]);
		}
		InvalidateRegion(gStackSplitX, gStackSplitY,
			gStackSplitX + 224, gStackSplitY + 82);
	}

	void DrawContextHoverExplanation(size_t index, BOOLEAN characterFan)
	{
		if (index >= gContextEntryCount) return;
		ContextEntry const& entry = gContextEntries[index];
		constexpr INT16 width = 300;
		constexpr INT16 height = 35;
		const INT16 x = std::clamp<INT16>(gContextX - width / 2,
			gsVIEWPORT_START_X,
			std::max<INT16>(gsVIEWPORT_START_X, gsVIEWPORT_END_X - width));
		const INT16 below = characterFan ? gContextY + 147 :
			gContextBlock.RegionBottomRightY + 6;
		const INT16 above = characterFan ? gContextY - 184 :
			gContextBlock.RegionTopLeftY - height - 6;
		const INT16 y = below + height <= gsVIEWPORT_WINDOW_END_Y ? below :
			std::max<INT16>(gsVIEWPORT_WINDOW_START_Y, above);
		const UINT16 dark = Get16BPPColor(FROMRGB(2, 3, 3));
		const UINT16 category = ActionCategoryColour(ContextEntryAccent(entry));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + width - 1,
			y + height - 1, dark);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + width - 1, y + 1,
			category);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(x + 5, y + 5, ST::format("{} / {}",
			ContextEntryGroupName(entry), entry.label).left(47));
		SetFontForeground(entry.enabled ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		MPrint(x + 5, y + 18,
			ST::string(ContextEntryExplanation(entry)).left(50));
		InvalidateRegion(x, y, x + width, y + height);
	}

	void DrawCharacterActionFan()
	{
		constexpr INT16 iconSize = 26;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 disabled = Get16BPPColor(FROMRGB(38, 38, 38));
		size_t hovered = gContextEntryCount;
		for (size_t i = 0; i < gContextEntryCount; ++i)
		{
			MOUSE_REGION const& region = gContextRegions[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const INT16 iconCentreX = x + iconSize / 2;
			const INT16 iconCentreY = y + iconSize / 2;
			ColorFillVideoSurfaceArea(FRAME_BUFFER,
				std::min(gContextX, iconCentreX), gContextY,
				std::max(gContextX, iconCentreX), gContextY, mutedRed);
			ColorFillVideoSurfaceArea(FRAME_BUFFER, iconCentreX,
				std::min(gContextY, iconCentreY), iconCentreX,
				std::max(gContextY, iconCentreY), mutedRed);
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + iconSize &&
				gusMouseYPos >= y && gusMouseYPos <= y + iconSize;
			if (hot) hovered = i;
			if (hot || !gContextEntries[i].enabled)
				DrawIconCorners(x, y, iconSize, iconSize,
					!gContextEntries[i].enabled ? disabled : red);
			DrawContextEntryIcon(gContextEntries[i], x + 3, y + 3);
		}
		if (hovered < gContextEntryCount)
			DrawContextHoverExplanation(hovered, TRUE);
		InvalidateRegion(gContextX - 140, gContextY - 140,
			gContextX + 140, gContextY + 140);
	}

	void DrawContextMenu()
	{
		if (!gContextVisible || gContextEntryCount == 0) return;
		if (gCharacterActionFanVisible)
		{
			DrawCharacterActionFan();
			return;
		}
		if (gObjectActionFanVisible)
		{
			constexpr INT16 iconSize = 26;
			const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
			const UINT16 mutedRed = Get16BPPColor(FROMRGB(92, 8, 8));
			const UINT16 disabled = Get16BPPColor(FROMRGB(38, 38, 38));

			size_t hovered = gContextEntryCount;
			for (size_t i = 0; i < gContextEntryCount; ++i)
			{
				MOUSE_REGION const& region = gContextRegions[i];
				const INT16 x = region.RegionTopLeftX;
				const INT16 y = region.RegionTopLeftY;
				const INT16 iconCentreX = x + iconSize / 2;
				const INT16 iconCentreY = y + iconSize / 2;
				ColorFillVideoSurfaceArea(FRAME_BUFFER,
					std::min(gContextX, iconCentreX), gContextY,
					std::max(gContextX, iconCentreX), gContextY, mutedRed);
				ColorFillVideoSurfaceArea(FRAME_BUFFER, iconCentreX,
					std::min(gContextY, iconCentreY), iconCentreX,
					std::max(gContextY, iconCentreY), mutedRed);
				const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + iconSize &&
					gusMouseYPos >= y && gusMouseYPos <= y + iconSize;
				if (hot) hovered = i;
				if (hot || !gContextEntries[i].enabled)
					DrawIconCorners(x, y, iconSize, iconSize,
						!gContextEntries[i].enabled ? disabled : red);
				DrawContextActionIcon(gContextEntries[i].action, x + 3, y + 3);
			}
			if (hovered < gContextEntryCount)
				DrawContextHoverExplanation(hovered, FALSE);
			InvalidateRegion(gContextBlock.RegionTopLeftX - 2,
				gContextBlock.RegionTopLeftY - 2,
				gContextBlock.RegionBottomRightX + 2,
				gContextBlock.RegionBottomRightY + 2);
			return;
		}
		// Rendering is a pure projection. An ownerless context is invalid and stays
		// invisible instead of mutating itself into a different menu mid-frame.
		return;
	}

	BOOLEAN ProjectHoverQuickActionRegion()
	{
		const BOOLEAN show = gHoverVisible && !gTutorialActive &&
			!gContextVisible && !gAimAutoCollapsed && !gAssetCatalogVisible &&
			!gStackSplitVisible && !gpItemPointer && !CarryState().active() &&
			gHoverSuggestedAction != ContextAction::COUNT &&
			gHoverActionBinding.kind != OS0InteractionTargetKind::NONE;
		if (!show)
		{
			const BOOLEAN changed =
				(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) != 0;
			gHoverQuickActionRegion.Disable();
			return changed;
		}

		INT16 anchorX = 0;
		INT16 anchorY = 0;
		if (gHoverCursorSoldier)
		{
			if (!GetActorDisplayAnchor(gHoverCursorSoldier, anchorX, anchorY))
			{
				const BOOLEAN changed =
					(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) != 0;
				gHoverQuickActionRegion.Disable();
				return changed;
			}
			anchorY -= 18;
		}
		else if (gHoverCursorGridNo >= 0 && gHoverCursorGridNo < WORLD_MAX)
		{
			GetGridNoScreenPos(gHoverCursorGridNo, gHoverCursorLevel,
				&anchorX, &anchorY);
			OS0MapWorldToDisplayScreen(&anchorX, &anchorY);
		}
		else
		{
			const BOOLEAN changed =
				(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) != 0;
			gHoverQuickActionRegion.Disable();
			return changed;
		}

		constexpr INT16 size = 28;
		const INT16 x = static_cast<INT16>(anchorX - 38);
		const INT16 y = static_cast<INT16>(anchorY - 18);
		if (x < gsVIEWPORT_START_X || x + size >= gsVIEWPORT_END_X ||
			y < gsVIEWPORT_WINDOW_START_Y || y + size >= OS0WorldViewportBottom())
		{
			const BOOLEAN changed =
				(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) != 0;
			gHoverQuickActionRegion.Disable();
			return changed;
		}
		const BOOLEAN wasEnabled =
			(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) != 0;
		const BOOLEAN moved =
			gHoverQuickActionRegion.RegionTopLeftX != x ||
			gHoverQuickActionRegion.RegionTopLeftY != y;
		MoveRegion(gHoverQuickActionRegion, x, y);
		if (!wasEnabled)
			gHoverQuickActionRegion.Enable();
		const ST::string help = ST::format(
			"{} / CLICK EXECUTE / MMB NEXT / F ALL ACTIONS\n{}",
			ContextActionName(gHoverSuggestedAction),
			ContextActionExplanation(gHoverSuggestedAction));
		if (help != gHoverQuickActionHelp)
		{
			gHoverQuickActionHelp = help;
			gHoverQuickActionRegion.SetFastHelpText(help);
		}
		return moved || !wasEnabled;
	}

	void DrawHoverQuickAction()
	{
		if (!(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED)) return;
		constexpr INT16 size = 28;
		const INT16 x = gHoverQuickActionRegion.RegionTopLeftX;
		const INT16 y = gHoverQuickActionRegion.RegionTopLeftY;
		const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + size &&
			gusMouseYPos >= y && gusMouseYPos <= y + size;
		const UINT16 accent = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 resting = Get16BPPColor(FROMRGB(76, 63, 38));
		DrawIconCorners(x, y, size, size, hot ? accent : resting);
		DrawContextActionIcon(gHoverSuggestedAction, x + 4, y + 3);
		InvalidateRegion(x - 2, y - 2, x + size + 2, y + size + 2);
	}

	BOOLEAN ProjectNearbyInteractionHintRegions()
	{
		const BOOLEAN showHints = InteractionMode().nearbyScanEnabled() &&
			!gTutorialActive && !gContextVisible && !gAimAutoCollapsed &&
			!gAssetCatalogVisible && !gStackSplitVisible &&
			!gpItemPointer && !CarryState().active();
		BOOLEAN changed = FALSE;
		for (size_t i = 0; i < gNearbyHintRegions.size(); ++i)
		{
			MOUSE_REGION& region = gNearbyHintRegions[i];
			if (!showHints || i >= gNearbyHintCount)
			{
				if (region.uiFlags & MSYS_REGION_ENABLED)
				{
					region.Disable();
					changed = TRUE;
				}
				continue;
			}
			NearbyInteractionHint const& hint = gNearbyHints[i];
			INT16 anchorX;
			INT16 anchorY;
			GetGridNoScreenPos(hint.gridNo, hint.level, &anchorX, &anchorY);
			OS0MapWorldToDisplayScreen(&anchorX, &anchorY);
			const INT16 x = anchorX - 12;
			const INT16 y = anchorY - 42 - static_cast<INT16>((i % 2) * 5);
			if (x < gsVIEWPORT_START_X || x + 24 > gsVIEWPORT_END_X ||
				y < gsVIEWPORT_WINDOW_START_Y ||
				y + 24 > gsVIEWPORT_WINDOW_END_Y)
			{
				if (region.uiFlags & MSYS_REGION_ENABLED)
				{
					region.Disable();
					changed = TRUE;
				}
				continue;
			}
			const BOOLEAN wasEnabled =
				(region.uiFlags & MSYS_REGION_ENABLED) != 0;
			const BOOLEAN moved = region.RegionTopLeftX != x ||
				region.RegionTopLeftY != y;
			MoveRegion(region, x, y);
			if (!wasEnabled) region.Enable();
			changed = changed || moved || !wasEnabled;
			const ST::string help = ST::format(
				"{} / {}\nF / CLICK: OBJECT ACTIONS / MMB: CYCLE\n{}",
				ContextActionName(hint.action), hint.enabled ? "READY" : "REQUIREMENT",
				ContextActionExplanation(hint.action));
			if (gNearbyHintHelp[i] != help)
			{
				gNearbyHintHelp[i] = help;
				region.SetFastHelpText(help);
			}
		}
		return changed;
	}

	void DrawNearbyInteractionHints()
	{
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(72, 58, 34));
		const UINT16 disabled = Get16BPPColor(FROMRGB(45, 45, 38));
		for (size_t i = 0; i < gNearbyHintRegions.size(); ++i)
		{
			MOUSE_REGION const& region = gNearbyHintRegions[i];
			if (!(region.uiFlags & MSYS_REGION_ENABLED) ||
				i >= gNearbyHintCount) continue;
			NearbyInteractionHint const& hint = gNearbyHints[i];
			const INT16 x = region.RegionTopLeftX;
			const INT16 y = region.RegionTopLeftY;
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 24 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 24;
			if (hot || !hint.enabled)
				DrawIconCorners(x, y, 24, 24, hint.enabled ? red : disabled);
			else
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 2, y + 22,
					x + 21, y + 23, muted);
			DrawContextActionIcon(hint.action, x + 2, y + 1);
			InvalidateRegion(x - 2, y - 2, x + 26, y + 26);
		}
	}

	void DrawFieldTutorial()
	{
		if (!gFieldTutorial.active() || gTutorialActive) return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 bright = Get16BPPColor(FROMRGB(255, 64, 32));
		const UINT16 muted = Get16BPPColor(FROMRGB(78, 5, 5));
		const UINT16 dark = Get16BPPColor(FROMRGB(3, 5, 5));

		if (!gContextVisible && gFieldTutorialGridNo >= 0 &&
			gFieldTutorialGridNo < WORLD_MAX)
		{
			INT16 anchorX;
			INT16 anchorY;
			GetGridNoScreenPos(gFieldTutorialGridNo, gFieldTutorialLevel,
				&anchorX, &anchorY);
			OS0MapWorldToDisplayScreen(&anchorX, &anchorY);
			const INT16 x = anchorX - 27;
			const INT16 y = anchorY - 46;
			if (x >= gsVIEWPORT_START_X && x + 54 < gsVIEWPORT_END_X &&
				y >= gsVIEWPORT_WINDOW_START_Y &&
				y + 46 < OS0WorldViewportBottom())
			{
				const UINT16 pulse = ((GetJA2Clock() / 250) & 1) ? bright : red;
				// A full high-contrast bracket follows the actual container grid. The
				// previous tiny corner glyph could disappear into brown crate artwork.
				OutlineBox(x, y, 54, 46, pulse);
				DrawIconCorners(x + 2, y + 2, 50, 42, pulse);
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 16, y,
					x + 38, y + 2, pulse);
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 16, y + 43,
					x + 38, y + 45, pulse);
				DrawContextActionIcon(
					gFieldTutorial.stage() >=
						OS0FieldTutorialStage::LOOT_CONTAINER ?
						ContextAction::PICK_UP : ContextAction::CONTENTS,
					x + 17, y + 11);
				SetFont(TINYFONT1);
				SetFontBackground(FONT_MCOLOR_BLACK);
				SetFontForeground(FONT_MCOLOR_RED);
				MPrint(x + 8, y - 9, "TUTORIAL TARGET");
				InvalidateRegion(x - 2, y - 11, x + 56, y + 48);
			}
		}

		const INT16 width = std::min<INT16>(430,
			std::max<INT16>(280, gsVIEWPORT_END_X - 32));
		const INT16 height = 50;
		const INT16 x = std::max<INT16>(8,
			(gsVIEWPORT_END_X - width) / 2);
		const INT16 y = gsVIEWPORT_WINDOW_START_Y + 8;
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y,
			x + width - 1, y + height - 1, dark);
		OutlineBox(x, y, width, height, muted);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + 34, y + 1, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + width - 35, y,
			x + width - 1, y + 1, red);
		DrawContextActionIcon(
			gFieldTutorial.stage() == OS0FieldTutorialStage::COMPLETE ?
				ContextAction::INSPECT : ContextAction::CONTENTS,
			x + 8, y + 14);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(x + 36, y + 8, gFieldTutorial.heading());
		SetFontForeground(FONT_WHITE);
		MPrint(x + 36, y + 24,
			ST::string(gFieldTutorial.instruction()).left(62));

		const INT16 progressWidth = width - 16;
		const INT16 stage = std::clamp<INT16>(
			static_cast<INT16>(gFieldTutorial.stage()) - 1, 0, 6);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 8, y + height - 7,
			x + 8 + progressWidth, y + height - 6, muted);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 8, y + height - 7,
			x + 8 + progressWidth * stage / 6, y + height - 6, red);
		InvalidateRegion(x, y, x + width, y + height);
	}

	void DrawHoverInspector()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::INSPECTOR)];
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::INSPECTOR)) || !gHoverVisible || gTutorialActive ||
			gAimAutoCollapsed)
			return;
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(92, 8, 8));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x, panel.y,
			panel.x + panel.w - 1, panel.y + panel.h - 1, dark);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x, panel.y,
			panel.x + 30, panel.y + 1, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, panel.x + panel.w - 31, panel.y,
			panel.x + panel.w - 1, panel.y + 1, red);
		if (gInspectorPreview)
		{
			BltVideoSurface(FRAME_BUFFER, gInspectorPreview,
				panel.x + 5, panel.y + 22, nullptr);
			DrawIconCorners(panel.x + 4, panel.y + 21, 66, 66, muted);
		}
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(panel.x + 7, panel.y + 7, "INSPECTOR / LIVE HOVER");
		MPrint(panel.x + panel.w - 13, panel.y + 7, "X");
		MPrint(panel.x + 75, panel.y + 25, gHoverTitle.left(35));
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		MPrint(panel.x + 75, panel.y + 43, gHoverDetail.left(39));
		if (!gHoverDebugDetail.empty())
		{
			SetFontForeground(FONT_WHITE);
			MPrint(panel.x + 75, panel.y + 59, gHoverDebugDetail.left(39));
		}
		SetFontForeground(FONT_MCOLOR_DKGRAY);
		MPrint(panel.x + 75, panel.y + 78,
			gpItemPointer ? "LMB APPLY / SHIFT+MMB RETURN" :
				"F / RMB OPTIONS / MMB CYCLE");
		InvalidateRegion(panel.x, panel.y, panel.x + panel.w, panel.y + panel.h);
	}

	void DrawWorldSelection()
	{
		if (gContextVisible) return;
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

	struct ContainerMoveItem
	{
		INT32 sourceIndex = -1;
		WORLDITEM source{};
	};

	std::vector<ContainerMoveItem> CaptureContainerMoveItems(GridNo const gridNo,
		UINT8 const level)
	{
		std::vector<ContainerMoveItem> contents;
		for (ITEM_POOL* item = GetItemPool(gridNo, level); item; item = item->pNext)
		{
			if (item->iItemIndex < 0 ||
				static_cast<size_t>(item->iItemIndex) >= gWorldItems.size()) continue;
			WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
			if (worldItem.sGridNo != gridNo || worldItem.ubLevel != level ||
				!OS0IsContainerOwnedItem(worldItem)) continue;
			contents.push_back({ item->iItemIndex, worldItem });
		}
		return contents;
	}

	BOOLEAN ContainerMoveSourceStillMatches(
		std::vector<ContainerMoveItem> const& contents)
	{
		for (ContainerMoveItem const& entry : contents)
		{
			if (entry.sourceIndex < 0 ||
				static_cast<size_t>(entry.sourceIndex) >= gWorldItems.size())
				return FALSE;
			WORLDITEM const& current = GetWorldItem(entry.sourceIndex);
			if (!current.fExists || current.sGridNo != entry.source.sGridNo ||
				current.ubLevel != entry.source.ubLevel ||
				current.bVisible != entry.source.bVisible ||
				current.usFlags != entry.source.usFlags ||
				current.bRenderZHeightAboveLevel !=
					entry.source.bRenderZHeightAboveLevel ||
				!OS0SameObjectRepresentation(current.o, entry.source.o)) return FALSE;
		}
		return TRUE;
	}

	void RemoveContainerMoveCopies(std::vector<INT32> const& copies)
	{
		for (INT32 const index : copies)
		{
			if (index >= 0 && static_cast<size_t>(index) < gWorldItems.size() &&
				GetWorldItem(index).fExists) RemoveItemFromPool(GetWorldItem(index));
		}
	}

	BOOLEAN CopyContainerMoveItems(std::vector<ContainerMoveItem> const& contents,
		GridNo const destination, UINT8 const level,
		std::vector<INT32>& copies)
	{
		copies.clear();
		copies.reserve(contents.size());
		try
		{
			for (ContainerMoveItem const& entry : contents)
			{
				OBJECTTYPE object = entry.source.o;
				const INT32 copy = AddItemToPool(destination, &object,
					static_cast<Visibility>(entry.source.bVisible), level,
					entry.source.usFlags,
					entry.source.bRenderZHeightAboveLevel);
				if (copy < 0)
				{
					RemoveContainerMoveCopies(copies);
					copies.clear();
					return FALSE;
				}
				copies.push_back(copy);
			}
		}
		catch (...)
		{
			RemoveContainerMoveCopies(copies);
			copies.clear();
			return FALSE;
		}
		return TRUE;
	}

	BOOLEAN FinalizeWorldMove(UINT16& placedStructureId,
		GridNo& placedStructureBaseGridNo)
	{
		placedStructureId = 0;
		placedStructureBaseGridNo = NOWHERE;
		OS0CarryState& carry = CarryState();
		if (carry.source < 0 || carry.source >= WORLD_MAX ||
			carry.destination < 0 || carry.destination >= WORLD_MAX ||
			carry.destination == carry.source ||
			carry.tileIndex >= NUMBEROFTILES) return FALSE;

		SOLDIERTYPE* const boundCarrier = CarryCarrier();
		STRUCTURE* const structure = CarryStructure();
		LEVELNODE* const sourceNode = WorldLevelNodeAt(carry.source,
			carry.sourceLevel, carry.tileIndex);
		if (!boundCarrier || !structure || !sourceNode ||
			!sourceNode->pStructureData ||
			FindBaseStructure(sourceNode->pStructureData) != structure ||
			!structure->pDBStructureRef ||
			!OkayToAddStructureToWorld(carry.destination, carry.destinationLevel,
				structure->pDBStructureRef, INVALID_STRUCTURE_ID)) return FALSE;
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		const BOOLEAN movesContainer =
			(structure->fFlags & STRUCTURE_OPENABLE) &&
			!(structure->fFlags & STRUCTURE_ANYDOOR);
		std::vector<ContainerMoveItem> const containerContents = movesContainer ?
			CaptureContainerMoveItems(carry.source, carry.sourceLevel) :
			std::vector<ContainerMoveItem>{};

		// Build the live destination first with per-operation persistence disabled.
		// Once all identities and container copies validate, one MOVE_STRUCT record
		// becomes the disk commit boundary. This cannot leave a persisted ADD without
		// its REMOVE when storage fails, and item graphics on the struct render layer
		// are never accidentally serialized as map structures.
		ApplyMapChangesToMapTempFile noIndividualStructureRecords(false);
		auto addLiveStructure = [](GridNo const gridNo,
			UINT16 const tileIndex) -> LEVELNODE*
		{
			try { return AddStructToTail(gridNo, tileIndex); }
			catch (...) { return nullptr; }
		};
		auto removeLiveStructure = [](GridNo const gridNo,
			LEVELNODE* const node) -> BOOLEAN
		{
			if (!node) return FALSE;
			try
			{
				RemoveStructFromLevelNode(gridNo, node);
				return TRUE;
			}
			catch (...) { return FALSE; }
		};
		LEVELNODE* destinationNode = nullptr;
		destinationNode = addLiveStructure(carry.destination,
			carry.tileIndex);
		if (!destinationNode) return FALSE;
		STRUCTURE* const destinationStructure =
			destinationNode->pStructureData ?
			FindBaseStructure(destinationNode->pStructureData) : nullptr;
		if (!destinationStructure ||
			StructureBaseGridNo(destinationStructure) != carry.destination ||
			WorldStructureAt(carry.destination, carry.destinationLevel,
				carry.tileIndex) != destinationStructure ||
			CarryCarrier() != boundCarrier || CarryStructure() != structure ||
			WorldLevelNodeAt(carry.source, carry.sourceLevel,
				carry.tileIndex) != sourceNode)
		{
			removeLiveStructure(carry.destination, destinationNode);
			return FALSE;
		}
		std::vector<INT32> destinationContents;
		if (!CopyContainerMoveItems(containerContents, carry.destination,
			carry.destinationLevel, destinationContents) ||
			!ContainerMoveSourceStillMatches(containerContents) ||
			CarryCarrier() != boundCarrier || CarryStructure() != structure ||
			WorldLevelNodeAt(carry.source, carry.sourceLevel,
				carry.tileIndex) != sourceNode)
		{
			RemoveContainerMoveCopies(destinationContents);
			removeLiveStructure(carry.destination, destinationNode);
			return FALSE;
		}
		try
		{
			ApplyMapChangesToMapTempFile recordMove;
			MoveStructInMapTempFile(carry.source, carry.destination,
				carry.tileIndex);
		}
		catch (...)
		{
			RemoveContainerMoveCopies(destinationContents);
			removeLiveStructure(carry.destination, destinationNode);
			return FALSE;
		}
		// Persistence is committed. Removing the already-validated source now has
		// no I/O path and therefore cannot fail because the map temp file is full or
		// read-only. If a catastrophic allocation exception occurs after this point,
		// replay still converges to the single destination recorded above.
		if (!removeLiveStructure(carry.source, sourceNode)) return FALSE;
		// Source ownership is removed only after both destination structure and
		// every content copy exist. From this point the destination is the sole
		// owner; there is no frame in which a failed move can orphan container loot.
		for (ContainerMoveItem const& entry : containerContents)
			RemoveItemFromPool(GetWorldItem(entry.sourceIndex));
		placedStructureId = destinationStructure->usStructureID;
		placedStructureBaseGridNo = StructureBaseGridNo(destinationStructure);

		// Keep the lighting initialized by the destination tile. Copying the
		// source palette state made carried objects change colour after placing.
		OS0AssetKey const sourceKey{ static_cast<UINT8>(gWorldSector.x),
			static_cast<UINT8>(gWorldSector.y), gWorldSector.z, carry.source,
			carry.sourceLevel, carry.tileIndex };
		OS0AssetKey const destinationKey{ static_cast<UINT8>(gWorldSector.x),
			static_cast<UINT8>(gWorldSector.y), gWorldSector.z,
			carry.destination, carry.destinationLevel, carry.tileIndex };
		OS0GetTacticalSession().state().assetDamage.move(sourceKey,
			destinationKey);
		RecompileLocalMovementCosts(carry.source);
		RecompileLocalMovementCosts(carry.destination);
		ErasePath();
		gfPlotNewMovement = TRUE;
		if (gLootGridNo == carry.source)
			gLootGridNo = carry.destination;
		if (gInspectedGridNo == carry.source)
		{
			gInspectedGridNo = carry.destination;
			gLootGridNo = carry.destination;
			CaptureInspectorPreview(carry.destination, gInspectedLevel);
		}
		if (gEnvironmentGridNo == carry.source)
			RefreshEnvironmentTarget(carry.destination, carry.destinationLevel,
				carry.tileIndex);
		// Heavy handling grows the attribute that was actually used. JA2's
		// StatChange stores sub-points in the merc profile, giving us the same
		// learn-by-doing loop as the rest of the campaign rather than a new XP bar.
		SOLDIERTYPE* const trainingCarrier = CarryCarrier();
		STRUCTURE const* const placedStructure = WorldStructureAt(
			carry.destination, carry.destinationLevel, carry.tileIndex);
		if (trainingCarrier && placedStructure &&
			placedStructure->usStructureID == placedStructureId &&
			StructureBaseGridNo(placedStructure) == placedStructureBaseGridNo)
		{
			const UINT16 practice = static_cast<UINT16>(std::clamp<INT32>(
				static_cast<INT32>(physics.massKg / 10.0f) +
				(carry.lifted ? 1 : 3), 2, 12));
			StatChange(*trainingCarrier, STRAMT, practice, FROM_SUCCESS);
			RecordFeedbackEvent(ST::format("{} {} KG / STR PRACTICE {}",
				CarryModeName(carry.mode),
				static_cast<INT32>(physics.massKg + 0.5f), practice));
			if (carry.mode == OS0CarryMode::THROW)
				OS0GetTacticalSession().state().pendingVisualEvents.push_back({
					carry.destination, OS0AssetMaterial::COMPOSITE, 2 });
		}
		OS0NotifyWorldMutation();
		return TRUE;
	}

	void UpdateWorldMove()
	{
		OS0CarryState& carry = CarryState();
		if (!carry.active()) return;
		if (gTacticalStatus.uiFlags & INCOMBAT)
		{
			RecordFeedbackEvent("CARRY CANCELLED / COMBAT STARTED");
			CancelWorldMoveState();
			return;
		}
		SOLDIERTYPE* const carrierSlot = CarryCarrierSlot();
		const BOOLEAN carrierIdentityMatches = carrierSlot &&
			GetSelectedMan() == carrierSlot &&
			carry.boundToCarrier(carry.carrier,
				carrierSlot->uiUniqueSoldierIdValue);
		SOLDIERTYPE* const carrier = carrierIdentityMatches ?
			carrierSlot : nullptr;
		STRUCTURE const* const source = carry.active() ? WorldStructureAt(
			carry.source, carry.sourceLevel, carry.tileIndex) : nullptr;
		OS0CarryContinuationFacts facts;
		facts.carrierAvailable = carrierSlot && carrierSlot->bActive;
		facts.carrierIdentityMatches = carrierIdentityMatches;
		facts.carrierAlive = carrier && carrier->bLife >= OKLIFE;
		facts.sameSector = carrier && carrier->sSector == gWorldSector &&
			!carrier->fBetweenSectors;
		facts.sameLevel = carrier && carrier->bLevel == carry.sourceLevel;
		// TARGETING represents hands physically attached to the unchanged source.
		// WALKING intentionally leaves that grid while following the owned route;
		// its actionGrid/path identity is validated separately below.
		facts.carrierInReach = carrier && (!carry.pending() ||
			carry.repositioning() ||
			PythSpacesAway(carrier->sGridNo, carry.source) <= 1);
		facts.carrierCanManipulate = carrier && source &&
			CanSoldierMoveWorldStructure(carrier, source);
		facts.objectAvailable = source &&
			(source->fFlags & STRUCTURE_BASE_TILE) != 0;
		facts.objectIdentityMatches =
			CarryStructureIdentityMatches(carry, source);
		const BOOLEAN carrierStillAnimating = carrier &&
			((gAnimControl[carrier->usAnimState].uiFlags & ANIM_MOVING) != 0 ||
			 carrier->fTurningUntilDone ||
			 carrier->usPendingAnimation != NO_PENDING_ANIMATION);
		const GridNo ownedRouteTarget = carry.repositioning() ?
			carry.followUpGrid : carry.actionGrid;
		const BOOLEAN carrierRouteEngaged = carrierStillAnimating ||
			(carrier && carrier->fDelayedMovement);
		facts.pathValid = carrier &&
			((!carry.walking() && !carry.repositioning()) ||
			 carrier->sGridNo == ownedRouteTarget ||
			 (carrierRouteEngaged &&
			  carrier->sFinalDestination == ownedRouteTarget));
		OS0CarryCancelReason const cancellation =
			OS0ValidateCarryContinuation(carry, facts);
		if (cancellation != OS0CarryCancelReason::NONE)
		{
			RecordFeedbackEvent(ST::format("CARRY CANCELLED / REASON {}",
				OS0CarryCancelReasonName(cancellation)));
			CancelWorldMoveState();
			return;
		}
		WORLD_PHYSICS_PROFILE const livePhysics =
			GetWorldPhysicsProfile(source);
		carry.lifted = livePhysics.massKg <=
			GetSoldierWorldCarryCapacityKg(carrier) * 0.55f;
		if (carry.repositioning())
		{
			if (carrier->sGridNo == carry.followUpGrid &&
				!carrierStillAnimating)
			{
				carry.followUpGrid = NOWHERE;
				SetRenderFlags(RENDER_FLAG_FULL);
			}
			return;
		}
		if (!carry.walking()) return;
		if (carrier->sGridNo != carry.actionGrid || carrierStillAnimating)
		{
			return;
		}

		const BOOLEAN keepGrab = carry.persistentGrab;
		const OS0CarryMode completedMode = carry.mode;
		const GridNo previousSource = carry.source;
		const GridNo nextSource = carry.destination;
		const UINT8 nextLevel = carry.destinationLevel;
		const UINT16 nextTile = carry.tileIndex;
		const SoldierID nextCarrier = carry.carrier;
		const UINT32 nextCarrierInstanceId = carry.carrierInstanceId;
		const BOOLEAN nextLifted = carry.lifted;
		UINT16 nextStructureId = 0;
		GridNo nextStructureBaseGridNo = NOWHERE;
		if (!FinalizeWorldMove(nextStructureId, nextStructureBaseGridNo))
		{
			// The actor has already reached an action tile that may be remote from
			// the unchanged source. Retargeting from here would be telekinesis.
			RecordFeedbackEvent("CARRY CANCELLED / COMMIT TARGET CHANGED");
			CancelWorldMoveState();
			CursorState().action = ContextAction::MOVE;
			guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
			return;
		}
		STRUCTURE const* const placedStructure = WorldStructureAt(nextSource,
			nextLevel, nextTile);
		if (keepGrab && placedStructure &&
			placedStructure->usStructureID == nextStructureId &&
			StructureBaseGridNo(placedStructure) == nextStructureBaseGridNo &&
			CarryCarrier() == carrier &&
			carry.begin(nextSource, nextLevel, nextTile, nextCarrier,
				nextCarrierInstanceId, nextStructureId,
				nextStructureBaseGridNo, OS0CarryMode::GRAB))
		{
			BindCarryShadowInstance();
			carry.lifted = nextLifted;
			CursorState().action = ContextAction::CARRY;
			guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
			if (completedMode == OS0CarryMode::PUSH && carrier->sGridNo != previousSource)
			{
				// The object vacates previousSource atomically; only then can native
				// pathfinding move the still-attached actor into that newly free cell.
				if (!EVENT_InternalGetNewSoldierPath(carrier, previousSource,
					carrier->usUIMovementMode, TRUE, TRUE))
				{
					RecordFeedbackEvent("GRAB RELEASED / PUSH FOLLOW-UP BLOCKED");
					CancelWorldMoveState();
					CursorState().action = ContextAction::MOVE;
					guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
					SetRenderFlags(RENDER_FLAG_FULL);
					return;
				}
				carry.followUpGrid = previousSource;
			}
			RecordFeedbackEvent("GRAB / ATTACHED / WASD PUSH-PULL-CARRY / ESC RELEASE");
		}
		else
		{
			ClearWorldMoveState();
			CursorState().action = ContextAction::MOVE;
			guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawCarryGhost(INT16 x, INT16 y, BOOLEAN carried)
	{
		if (x < gsVIEWPORT_START_X || x > gsVIEWPORT_END_X ||
			y < gsVIEWPORT_WINDOW_START_Y || y > gsVIEWPORT_WINDOW_END_Y) return;
		const INT16 bob = static_cast<INT16>((GetJA2Clock() / 90) % 4);
		if (CarryState().tileIndex < NUMBEROFTILES)
		{
			TILE_ELEMENT const& tile = gTileDatabase[CarryState().tileIndex];
			if (tile.hTileSurface)
			{
				ETRLEObject const& frame =
					tile.hTileSurface->SubregionProperties(tile.usRegionIndex);
				const INT16 drawX = static_cast<INT16>(x - frame.usWidth / 2 -
					frame.sOffsetX);
				const INT16 drawY = static_cast<INT16>(y - frame.usHeight -
					frame.sOffsetY + (carried ? -22 : 8) - bob);
				// Tile surfaces are shared by every world instance.  RenderWorld leaves
				// the surface on the last node's shade; an explicit neutral UI shade
				// prevents the carry representation flickering grey between frames.
				tile.hTileSurface->CurrentShade(DEFAULT_SHADE_LEVEL);
				BltVideoObject(FRAME_BUFFER, tile.hTileSurface, tile.usRegionIndex,
					drawX, drawY);
				InvalidateRegion(std::max<INT16>(gsVIEWPORT_START_X, drawX - 2),
					std::max<INT16>(gsVIEWPORT_WINDOW_START_Y, drawY - 2),
					std::min<INT16>(gsVIEWPORT_END_X,
						static_cast<INT16>(drawX + frame.usWidth + 2)),
					std::min<INT16>(gsVIEWPORT_WINDOW_END_Y,
						static_cast<INT16>(drawY + frame.usHeight + 2)));
				return;
			}
		}
		const UINT16 grey = Get16BPPColor(FROMRGB(112, 116, 116));
		OutlineBox(x - 12, y - 8 - bob, 25, 17, grey);
		InvalidateRegion(x - 14, y - 12, x + 14, y + 10);
	}

	void DrawActionMenu()
	{
		// A right-click interaction is the only active spatial menu. Never draw the
		// carry cursor/ghost as a second mini-menu beneath the radial fan.
		OS0CarryState const& carry = CarryState();
		if (gContextVisible ||
			(!OS0IsManipulationAction(CursorState().action) && !carry.active())) return;
		if (carry.pending())
		{
			INT16 labelX = static_cast<INT16>(gusMouseXPos + 14);
			INT16 labelY = static_cast<INT16>(gusMouseYPos - 6);
			if (carry.persistentGrab)
			{
				SOLDIERTYPE* const carrier = CarryCarrier();
				INT16 carrierX;
				INT16 carrierY;
				if (!carrier || !GetActorDisplayAnchor(carrier, carrierX, carrierY))
					return;
				// The object remains a real world node between grab steps. The engine
				// renders it at correct depth; only project the attached hand on the
				// animated carrier instead of teleporting a second copy to the mouse.
				OS0UIAssets().draw(OS0UIIcon::HAND, FRAME_BUFFER,
					carrierX - 8, carrierY - 34);
				labelX = static_cast<INT16>(carrierX + 12);
				labelY = static_cast<INT16>(carrierY - 31);
				InvalidateRegion(carrierX - 10, carrierY - 36,
					carrierX + 12, carrierY - 14);
			}
			else
			{
				DrawCarryGhost(gusMouseXPos, gusMouseYPos, FALSE);
			}
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(labelX, labelY, CarryModeName(carry.mode));
		}
		else if (carry.walking())
		{
			SOLDIERTYPE* const carrier = CarryCarrier();
			if (!carrier) return;
			INT16 carrierX;
			INT16 carrierY;
			if (!GetActorDisplayAnchor(carrier, carrierX, carrierY)) return;
			INT16 baseX;
			INT16 baseY;
			INT16 facingX;
			INT16 facingY;
			GetGridNoScreenPos(carrier->sGridNo, carrier->bLevel, &baseX, &baseY);
			const UINT8 facing = carrier->bDirection < NUM_WORLD_DIRECTIONS ?
				carrier->bDirection : static_cast<UINT8>(NORTH);
			const GridNo facingGrid = NewGridNo(carrier->sGridNo,
				DirectionInc(facing));
			GetGridNoScreenPos(facingGrid, carrier->bLevel, &facingX, &facingY);
			OS0MapWorldToDisplayScreen(&baseX, &baseY);
			OS0MapWorldToDisplayScreen(&facingX, &facingY);
			const INT16 facingDX = std::clamp<INT16>(facingX - baseX, -32, 32);
			const INT16 facingDY = std::clamp<INT16>(facingY - baseY, -18, 18);
			// A lifted object is held slightly forward; a heavy one trails behind.
			// Both follow the actual facing instead of a fixed screen-right offset.
			const INT16 sign = carry.lifted ? 1 : -1;
			INT16 objectX = static_cast<INT16>(carrierX + sign * facingDX / 2);
			INT16 objectY = static_cast<INT16>(carrierY + sign * facingDY / 2);
			DrawCarryGhost(objectX, objectY, carry.lifted);
			OS0UIAssets().draw(OS0UIIcon::HAND, FRAME_BUFFER,
				carrierX - 8, carrierY - 34);
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(carrierX - 13, carrierY - 48, CarryModeName(carry.mode));

			if (carry.destination >= 0)
			{
				INT16 destinationX;
				INT16 destinationY;
				GetGridNoScreenPos(carry.destination, carry.destinationLevel,
					&destinationX, &destinationY);
				OS0MapWorldToDisplayScreen(&destinationX, &destinationY);
				const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
				OutlineBox(destinationX - 9, destinationY - 5, 19, 10, red);
				InvalidateRegion(destinationX - 11, destinationY - 7,
					destinationX + 11, destinationY + 7);
			}
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

	void UpdateImpactParticles()
	{
		const UINT32 now = GetJA2Clock();
		BOOLEAN active = FALSE;
		BOOLEAN expired = FALSE;
		for (ImpactParticle& particle : gImpactParticles)
		{
			if (particle.born == 0) continue;
			if (now - particle.born > 720)
			{
				particle.born = 0;
				expired = TRUE;
				continue;
			}
			active = TRUE;
		}
		// Animated particles move every frame. Request the world clear before it is
		// rendered; doing this from DrawImpactParticles was one frame too late.
		if (active || expired) SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawImpactParticles()
	{
		const UINT32 now = GetJA2Clock();
		for (ImpactParticle const& particle : gImpactParticles)
		{
			if (particle.born == 0) continue;
			const UINT32 age = now - particle.born;
			if (age > 720) continue;
			INT16 x;
			INT16 y;
			GetGridNoScreenPos(particle.gridNo, 0, &x, &y);
			OS0MapWorldToDisplayScreen(&x, &y);
			x = static_cast<INT16>(x + particle.velocityX *
				static_cast<INT32>(age) / 90);
			y = static_cast<INT16>(y - 20 + particle.velocityY *
				static_cast<INT32>(age) / 110 +
				static_cast<INT32>(age * age) / 60000);
			if (x < gsVIEWPORT_START_X || x >= gsVIEWPORT_END_X - 2 ||
				y < gsVIEWPORT_WINDOW_START_Y || y >= gsVIEWPORT_WINDOW_END_Y - 2)
				continue;
			UINT16 colour = Get16BPPColor(FROMRGB(150, 145, 132));
			switch (static_cast<AssetMaterial>(particle.colourKind))
			{
				case AssetMaterial::WOOD:
				case AssetMaterial::ORGANIC:
					colour = Get16BPPColor(FROMRGB(135, 86, 42)); break;
				case AssetMaterial::STONE:
				case AssetMaterial::SAND:
				case AssetMaterial::EARTH:
					colour = Get16BPPColor(FROMRGB(145, 137, 118)); break;
				case AssetMaterial::METAL:
					colour = Get16BPPColor(FROMRGB(235, 188, 62)); break;
				default: break;
			}
			ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + 1, y + 1, colour);
			InvalidateRegion(x - 1, y - 1, x + 3, y + 3);
		}
	}

	void AddDebugAsset(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gridNo < 0 || gridNo >= WORLD_MAX || tileIndex >= NUMBEROFTILES) return;
		const UINT16 canonical = CanonicalAssetTileIndex(gridNo, level, tileIndex);
		for (DebugAssetEntry& entry : gDebugAssetLibrary)
		{
			if (entry.tileIndex == canonical && entry.level == level)
			{
				entry.count = std::min<UINT16>(65535, entry.count + 1);
				return;
			}
		}
		BOOLEAN catalogued = FALSE;
		AssetCatalogRecord const record = ResolveAssetRecord(gridNo, level,
			tileIndex, &catalogued);
		gDebugAssetLibrary.push_back({ canonical, gridNo, level, 1, record,
			catalogued });
	}

	void RebuildDebugAssetLibrary()
	{
		gDebugAssetLibrary.clear();
		for (GridNo gridNo = 0; gridNo < WORLD_MAX; ++gridNo)
		{
			auto scanStructures = [gridNo](LEVELNODE* node, UINT8 level)
			{
				for (; node; node = node->pNext)
				{
					if (!node->pStructureData || node->usIndex >= NUMBEROFTILES ||
						node->pStructureData->fFlags & STRUCTURE_PERSON) continue;
					STRUCTURE* const base = FindBaseStructure(node->pStructureData);
					if (!base || !(node->pStructureData->fFlags & STRUCTURE_BASE_TILE))
						continue;
					AddDebugAsset(base->sGridNo, level, node->usIndex);
				}
			};
			scanStructures(gpWorldLevelData[gridNo].pStructHead, 0);
			scanStructures(gpWorldLevelData[gridNo].pOnRoofHead, 1);
			for (LEVELNODE* node = gpWorldLevelData[gridNo].pObjectHead;
				node; node = node->pNext)
			{
				if (node->usIndex < NUMBEROFTILES &&
					!(node->uiFlags & (LEVELNODE_ITEM | LEVELNODE_HIDDEN)))
					AddDebugAsset(gridNo, 0, node->usIndex);
			}
		}
		std::stable_sort(gDebugAssetLibrary.begin(), gDebugAssetLibrary.end(),
			[](DebugAssetEntry const& left, DebugAssetEntry const& right)
			{
				if (left.catalogued != right.catalogued)
					return left.catalogued < right.catalogued;
				if (left.record.category != right.record.category)
					return left.record.category < right.record.category;
				return left.tileIndex < right.tileIndex;
			});
		gDebugAssetLibrarySector = gWorldSector.AsByte();
		gDebugAssetLibraryTileset = static_cast<INT16>(giCurrentTilesetID);
		gAssetLibraryPage = 0;
		RecordFeedbackEvent(ST::format("ASSET LIBRARY SCAN / {} UNIQUE",
			gDebugAssetLibrary.size()));
	}

	BOOLEAN DebugAssetMatches(DebugAssetEntry const& entry)
	{
		switch (gAssetLibraryFilter)
		{
			case AssetLibraryFilter::ALL: return TRUE;
			case AssetLibraryFilter::UNCATALOGUED: return !entry.catalogued;
			case AssetLibraryFilter::DEBRIS:
				return entry.record.category == AssetCategory::DEBRIS ||
					entry.record.category == AssetCategory::STONE ||
					entry.record.role == AssetRole::SALVAGE ||
					entry.record.role == AssetRole::RESOURCE_NODE;
			case AssetLibraryFilter::COUNT: break;
		}
		return FALSE;
	}

	size_t DebugAssetCount()
	{
		return static_cast<size_t>(std::count_if(gDebugAssetLibrary.begin(),
			gDebugAssetLibrary.end(), DebugAssetMatches));
	}

	DebugAssetEntry* DebugAssetAtCell(size_t cell)
	{
		const size_t wanted = gAssetLibraryPage * 6 + cell;
		size_t visible = 0;
		for (DebugAssetEntry& entry : gDebugAssetLibrary)
		{
			if (!DebugAssetMatches(entry)) continue;
			if (visible++ == wanted) return &entry;
		}
		return nullptr;
	}

	const char* AssetLibraryFilterName()
	{
		switch (gAssetLibraryFilter)
		{
			case AssetLibraryFilter::ALL: return "ALL ASSETS";
			case AssetLibraryFilter::UNCATALOGUED: return "UNCATALOGUED";
			case AssetLibraryFilter::DEBRIS: return "DEBRIS / SALVAGE";
			case AssetLibraryFilter::COUNT: break;
		}
		return "ASSETS";
	}

	void DrawAssetLibrarySymbol(UINT16 tileIndex, INT16 x, INT16 y, INT16 size)
	{
		if (!gAssetLibrarySymbolSurface) return;
		gAssetLibrarySymbolSurface->Fill(Get16BPPColor(FROMRGB(6, 8, 8)));
		if (tileIndex >= NUMBEROFTILES) return;
		TILE_ELEMENT const& tile = gTileDatabase[tileIndex];
		if (!tile.hTileSurface) return;
		ETRLEObject const& frame =
			tile.hTileSurface->SubregionProperties(tile.usRegionIndex);
		const INT16 drawX = static_cast<INT16>(32 - frame.usWidth / 2 -
			frame.sOffsetX);
		const INT16 drawY = static_cast<INT16>(57 - frame.usHeight -
			frame.sOffsetY);
		BltVideoObject(gAssetLibrarySymbolSurface, tile.hTileSurface,
			tile.usRegionIndex, drawX, drawY);
		const SGPBox source{ 0, 0, 64, 64 };
		const SGPBox destination{
			static_cast<UINT16>(std::max<INT16>(0, x)),
			static_cast<UINT16>(std::max<INT16>(0, y)),
			static_cast<UINT16>(size), static_cast<UINT16>(size)
		};
		BltStretchVideoSurface(FRAME_BUFFER, gAssetLibrarySymbolSurface,
			&source, &destination);
	}

	void DebugLibraryGrabberCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_DWN)
		{
			if (gUIRuntime.windowManager().beginDrag(
				gUIRuntime.managedId(OS0UIPanel::ASSET_LIBRARY),
				gusMouseXPos, gusMouseYPos))
				SyncManagedMouseRegionZOrder(gUIRuntime.windowManager());
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			UpdateWindowDragging();
			FinishWindowDrag();
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	void DebugLibraryCloseCallback(MOUSE_REGION*, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;
		gUIRuntime.hide(OS0UIPanel::ASSET_LIBRARY);
		if (gUIRuntime.windowManager().draggingWindow() ==
			gUIRuntime.managedId(OS0UIPanel::ASSET_LIBRARY))
			gUIRuntime.windowManager().cancelDrag();
		SaveUILayout();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DebugLibraryCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & (MSYS_CALLBACK_REASON_POINTER_UP |
			MSYS_CALLBACK_REASON_RBUTTON_UP)) || !gGodLibraryVisible) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index < 6)
		{
			DebugAssetEntry* const entry = DebugAssetAtCell(index);
			if (!entry) return;
			InternalLocateGridNo(entry->gridNo, TRUE);
			OS0SelectWorldObject(nullptr, entry->gridNo, entry->level,
				entry->tileIndex);
			gUIRuntime.show(OS0UIPanel::ASSET_LIBRARY);
			if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
				OpenAssetCatalog(entry->gridNo, entry->level, entry->tileIndex);
			else
				RecordFeedbackEvent(ST::format("ASSET LIBRARY FOCUS / TILE {} GRID {}",
					entry->tileIndex, entry->gridNo));
		}
		else switch (index)
		{
			case 6: gDebugLibraryMode = DebugLibraryMode::ASSETS; break;
			case 7: gDebugLibraryMode = DebugLibraryMode::ICONS; break;
			case 8:
				gAssetLibraryFilter = static_cast<AssetLibraryFilter>(
					(static_cast<UINT8>(gAssetLibraryFilter) + 1) %
					static_cast<UINT8>(AssetLibraryFilter::COUNT));
				gAssetLibraryPage = 0;
				break;
			case 9: if (gAssetLibraryPage > 0) --gAssetLibraryPage; break;
			case 10:
			{
				const size_t count = DebugAssetCount();
				if ((gAssetLibraryPage + 1) * 6 < count) ++gAssetLibraryPage;
				break;
			}
			case 11: RebuildDebugAssetLibrary(); break;
			default: break;
		}
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void GodIconCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gGodLibraryVisible ||
			gDebugLibraryMode != DebugLibraryMode::ICONS)
			return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index < GOD_ICON_COUNT)
		{
			gGodMenuIcon = static_cast<UINT8>(index);
			RecordFeedbackEvent(ST::format("GOD ICON {} / {}",
				index, GetOS0UIIconDescriptor(
					static_cast<OS0UIIcon>(index)).label));
		}
		gGodLibraryVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DrawGodIconLibrary()
	{
		if (!gGodLibraryVisible || !OS0UIAssets().initialized()) return;

		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 bright = Get16BPPColor(FROMRGB(238, 28, 22));
		const UINT16 muted = Get16BPPColor(FROMRGB(74, 9, 8));
		const UINT16 dark = Get16BPPColor(FROMRGB(4, 7, 7));
		const UINT16 card = Get16BPPColor(FROMRGB(9, 12, 11));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, gGodLibraryX, gGodLibraryY,
			gGodLibraryX + GOD_LIBRARY_W - 1, gGodLibraryY + GOD_LIBRARY_H - 1,
			dark);
		OutlineBox(gGodLibraryX, gGodLibraryY, GOD_LIBRARY_W, GOD_LIBRARY_H, red);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(gGodLibraryX + 6, gGodLibraryY + 5,
			"GOD MODE / ASSET REGISTRY");
		MPrint(gGodLibraryX + GOD_LIBRARY_W - 14, gGodLibraryY + 5, "X");
		const BOOLEAN assets = gDebugLibraryMode == DebugLibraryMode::ASSETS;
		SetFontForeground(assets ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		MPrint(gGodLibraryX + 12, gGodLibraryY + 30, "ASSETS");
		SetFontForeground(!assets ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		MPrint(gGodLibraryX + 85, gGodLibraryY + 30, "ACTION ICONS");
		ColorFillVideoSurfaceArea(FRAME_BUFFER,
			gGodLibraryX + (assets ? 8 : 80), gGodLibraryY + 42,
			gGodLibraryX + (assets ? 74 : 160), gGodLibraryY + 42, bright);

		if (assets)
		{
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gGodLibraryX + 174, gGodLibraryY + 30,
				ST::format("FILTER / {}", AssetLibraryFilterName()).left(31));
			for (size_t i = 0; i < 6; ++i)
			{
				DebugAssetEntry* const entry = DebugAssetAtCell(i);
				MOUSE_REGION const& region = gAssetLibraryRegions[i];
				const INT16 x = region.RegionTopLeftX;
				const INT16 y = region.RegionTopLeftY;
				const BOOLEAN hot = gusMouseXPos >= x &&
					gusMouseXPos <= region.RegionBottomRightX &&
					gusMouseYPos >= y && gusMouseYPos <= region.RegionBottomRightY;
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y,
					region.RegionBottomRightX, region.RegionBottomRightY,
					hot ? Get16BPPColor(FROMRGB(18, 17, 14)) : card);
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y,
					x + (hot ? 42 : 18), y, hot ? bright : muted);
				if (!entry)
				{
					SetFontForeground(FONT_MCOLOR_DKGRAY);
					MPrint(x + 8, y + 23, "NO ASSET");
					continue;
				}
				DrawAssetLibrarySymbol(entry->tileIndex, x + 3, y + 4, 45);
				AssetMaterial const material = ResolveAssetMaterial(entry->gridNo,
					entry->level, entry->tileIndex, entry->record);
				FieldToolKind const tool = RequiredFieldTool(entry->gridNo,
					entry->level, entry->tileIndex);
				SalvageProfile const salvage = DescribeWorldAsset(entry->gridNo,
					entry->level, entry->tileIndex);
				STRUCTURE const* const structure = WorldStructureAt(entry->gridNo,
					entry->level, entry->tileIndex);
				const GridNo durabilityGrid = structure ?
					structure->sGridNo : entry->gridNo;
				const INT16 maximum = OS0AssetDurabilityMaximum(material);
				const INT16 current = OS0CurrentAssetDurability(durabilityGrid,
					entry->level, entry->tileIndex, material);
				SetFontForeground(entry->catalogued ? FONT_WHITE : FONT_MCOLOR_RED);
				MPrint(x + 52, y + 5,
					ST::format("T{} {} / {} x{}", entry->tileIndex,
						entry->record.label, entry->catalogued ? "DB" : "AUTO",
						entry->count).left(23));
				SetFontForeground(FONT_MCOLOR_LTGRAY);
				MPrint(x + 52, y + 20,
					ST::format("{} / {} / {}x{}",
						gAssetCategoryNames[static_cast<size_t>(entry->record.category)],
						gAssetMaterialNames[static_cast<size_t>(material)],
						entry->record.width, entry->record.height).left(23));
				SetFontForeground(FONT_MCOLOR_DKGRAY);
				MPrint(x + 52, y + 36,
					ST::format("{} / HP {}/{}{}", FieldToolName(tool), current,
						maximum, salvage.salvageable ? ST::format(" / +{}", salvage.amount) :
							ST::string()).left(23));
				gAssetLibraryRegions[i].SetFastHelpText(
					"LMB: FOCUS ASSET / RMB: OPEN CATALOG EDITOR");
			}
			const size_t count = DebugAssetCount();
			const size_t pages = std::max<size_t>(1, (count + 5) / 6);
			SetFontForeground(FONT_MCOLOR_DKGRAY);
			MPrint(gGodLibraryX + 9, gGodLibraryY + 229,
				ST::format("{} UNIQUE / PAGE {}/{} / RMB EDITS DATABASE",
					count, std::min(gAssetLibraryPage + 1, pages), pages).left(44));
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gGodLibraryX + 294, gGodLibraryY + 230, "<");
			MPrint(gGodLibraryX + 334, gGodLibraryY + 230, ">");
			MPrint(gGodLibraryX + 369, gGodLibraryY + 230, "SCAN");
			InvalidateRegion(gGodLibraryX, gGodLibraryY,
				gGodLibraryX + GOD_LIBRARY_W, gGodLibraryY + GOD_LIBRARY_H);
			return;
		}

		for (INT16 frame = 0; frame < 3; ++frame)
			OS0UIAssets().drawFrame(OS0UIAssetSheet::BUTTON_FRAME, 0,
				FRAME_BUFFER, gGodLibraryX + 88 + frame * 78,
				gGodLibraryY + 64);

		for (size_t i = 0; i < GOD_ICON_COUNT; ++i)
		{
			const size_t cell = (i / 8) * 9 + (i % 8);
			const INT16 frame = static_cast<INT16>(cell / 9);
			const INT16 local = static_cast<INT16>(cell % 9);
			const INT16 x = gGodLibraryX + 88 + frame * 78 + 9 + (local % 3) * 20;
			const INT16 y = gGodLibraryY + 72 + (local / 3) * 20;
			OS0UIAssets().draw(static_cast<OS0UIIcon>(i), FRAME_BUFFER, x, y);
			if (i == gGodMenuIcon) OutlineBox(x - 1, y - 1, 20, 20, red);
		}

		// The original cancel glyph closes the atlas without changing selection.
		OS0UIAssets().draw(OS0UIIcon::CANCEL, FRAME_BUFFER,
			gGodLibraryX + 88 + 2 * 78 + 49, gGodLibraryY + 112);
		SetFontForeground(FONT_WHITE);
		MPrint(gGodLibraryX + 95, gGodLibraryY + 171,
			ST::format("SELECTED {} / {}", gGodMenuIcon,
				GetOS0UIIconDescriptor(
					static_cast<OS0UIIcon>(gGodMenuIcon)).label).left(36));
		SetFontForeground(FONT_MCOLOR_DKGRAY);
		MPrint(gGodLibraryX + 95, gGodLibraryY + 193,
			"VANILLA STI SYMBOL ATLAS / CLICK TO BIND STAR");
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
				OS0UpsertAssetCatalogRecord(gCatalogDraft);
				const BOOLEAN saved = OS0WriteAssetCatalog();
				gDebugAssetLibrarySector = 0xff;
				gDebugAssetLibraryTileset = -1;
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
				gGodLibraryVisible = gAssetCatalogReturnToLibrary;
				gAssetCatalogReturnToLibrary = FALSE;
				break;
			}
			case 10:
				gAssetCatalogVisible = FALSE;
				gAssetCatalogNameEditing = FALSE;
				SetUIKeyboardHook(nullptr);
				gGodLibraryVisible = gAssetCatalogReturnToLibrary;
				gAssetCatalogReturnToLibrary = FALSE;
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

	BOOLEAN DirectControlBlocked()
	{
		return gTutorialActive || gContextVisible || gStackSplitVisible ||
			gAssetCatalogVisible || gFeedbackEditing ||
			AreWeInAUIMenu() || InItemDescriptionBox() ||
			InItemStackPopup() || InKeyRingPopup() ||
			(gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
			OS0GetRealtimeEditorUI().active() ||
			gTacticalStatus.fAutoBandageMode || gpItemPointer ||
			CarryState().active() || gPendingWorldAction.active() ||
			OS0BlocksKeyboardWorldInputAt(gusMouseXPos, gusMouseYPos) ||
			gUIRuntime.windowManager().draggingWindow() !=
				OS0_INVALID_WINDOW;
	}

	void SynchronizeInteractionMode()
	{
		OS0InteractionFrameFacts facts;
		facts.tutorial = gTutorialActive;
		facts.fight = CombatModeActive();
		facts.cursorAction = CursorState().action != ContextAction::MOVE;
		facts.cursorSurface = SurfaceForAction(CursorState().action);
		facts.passiveInteraction = gpItemPointer || CarryState().active() ||
			gPendingWorldAction.active() || gTacticalStatus.fAutoBandageMode;
		InteractionMode().synchronize(facts);
	}

	BOOLEAN NativeControlProjectionBlocked(SOLDIERTYPE const* selected)
	{
		return !selected || !selected->bActive || selected->bTeam != OUR_TEAM ||
			!OK_CONTROLLABLE_MERC(selected) || gfDisableRegionActive ||
			gfUserTurnRegionActive ||
			(gAnimControl[selected->usAnimState].uiFlags &
				(ANIM_FIRE | ANIM_SPECIALMOVE)) ||
			selected->fInNonintAnim || selected->fRTInNonintAnim ||
			selected->ubPendingAction != NO_PENDING_ACTION ||
			selected->fTurningUntilDone ||
			selected->usPendingAnimation != NO_PENDING_ANIMATION ||
			selected->ubPendingStanceChange != NO_PENDING_STANCE ||
			gTacticalStatus.ubAttackBusyCount > 0 ||
			((gTacticalStatus.uiFlags & INCOMBAT) &&
				gTacticalStatus.ubCurrentTeam != OUR_TEAM) ||
			gCurrentUIMode == LOCKUI_MODE ||
			gCurrentUIMode == LOCKOURTURN_UI_MODE ||
			gCurrentUIMode == ENEMYS_TURN_MODE;
	}

	BOOLEAN NativeNormalProjectionBlocked(SOLDIERTYPE const* selected)
	{
		if (gfDisableRegionActive || gfUserTurnRegionActive ||
			gTacticalStatus.ubAttackBusyCount > 0 ||
			((gTacticalStatus.uiFlags & INCOMBAT) &&
				gTacticalStatus.ubCurrentTeam != OUR_TEAM) ||
			gCurrentUIMode == LOCKUI_MODE ||
			gCurrentUIMode == LOCKOURTURN_UI_MODE ||
			gCurrentUIMode == ENEMYS_TURN_MODE) return TRUE;
		if (!selected) return FALSE;
		return (gAnimControl[selected->usAnimState].uiFlags &
				(ANIM_FIRE | ANIM_SPECIALMOVE)) ||
			selected->fInNonintAnim || selected->fRTInNonintAnim ||
			selected->ubPendingAction != NO_PENDING_ACTION ||
			selected->fTurningUntilDone ||
			selected->usPendingAnimation != NO_PENDING_ANIMATION ||
			selected->ubPendingStanceChange != NO_PENDING_STANCE;
	}

	void ProjectControlModeToEngine()
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		const OS0CursorMode desired = ContextActionCursor(CursorState().action);
		if (desired != OS0CursorMode::ATTACK)
		{
			BOOLEAN satisfied = FALSE;
			switch (desired)
			{
				case OS0CursorMode::HAND:
					satisfied = gCurrentUIMode == HANDCURSOR_MODE;
					break;
				case OS0CursorMode::LOOK:
					satisfied = gCurrentUIMode == LOOKCURSOR_MODE;
					break;
				case OS0CursorMode::TALK:
					satisfied = gCurrentUIMode == TALKCURSOR_MODE;
					break;
				case OS0CursorMode::MOVE:
				case OS0CursorMode::NONE:
					satisfied = gCurrentUIMode == MOVE_MODE ||
						gCurrentUIMode == IDLE_MODE;
					break;
				case OS0CursorMode::ATTACK:
					break;
			}
			if (satisfied || guiPendingOverrideEvent != I_DO_NOTHING ||
				NativeNormalProjectionBlocked(selected)) return;

			// Native menus/confirmations retain ownership. Unlike the old pending
			// booleans, waiting here cannot make OS//0 consume their primary click.
			const BOOLEAN projectable =
				gCurrentUIMode == MOVE_MODE || gCurrentUIMode == IDLE_MODE ||
				gCurrentUIMode == ACTION_MODE ||
				gCurrentUIMode == CONFIRM_ACTION_MODE ||
				gCurrentUIMode == HANDCURSOR_MODE ||
				gCurrentUIMode == LOOKCURSOR_MODE ||
				gCurrentUIMode == TALKCURSOR_MODE ||
				gCurrentUIMode == GETTINGITEM_MODE;
			if (!projectable) return;

			if (desired == OS0CursorMode::HAND &&
				(gCurrentUIMode == MOVE_MODE || gCurrentUIMode == IDLE_MODE))
				guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
			else if (desired == OS0CursorMode::LOOK &&
				(gCurrentUIMode == MOVE_MODE || gCurrentUIMode == IDLE_MODE))
				guiPendingOverrideEvent = LC_CHANGE_TO_LOOK;
			else if (desired == OS0CursorMode::TALK &&
				(gCurrentUIMode == MOVE_MODE || gCurrentUIMode == IDLE_MODE))
				guiPendingOverrideEvent = T_CHANGE_TO_TALKING;
			else if (gCurrentUIMode != MOVE_MODE && gCurrentUIMode != IDLE_MODE)
				guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
			return;
		}

		if (gCurrentUIMode == ACTION_MODE ||
			gCurrentUIMode == CONFIRM_ACTION_MODE) return;
		if (NativeControlProjectionBlocked(selected) ||
			guiPendingOverrideEvent != I_DO_NOTHING) return;
		if (gCurrentUIMode == HANDCURSOR_MODE ||
			gCurrentUIMode == LOOKCURSOR_MODE ||
			gCurrentUIMode == TALKCURSOR_MODE ||
			gCurrentUIMode == GETTINGITEM_MODE)
		{
			guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
			return;
		}
		if (gCurrentUIMode != MOVE_MODE && gCurrentUIMode != IDLE_MODE) return;
		const UINT32 now = GetJA2Clock();
		if (now < gNextCombatProjectionAt) return;
		guiPendingOverrideEvent = M_CHANGE_TO_ACTION;
		gNextCombatProjectionAt = now + 100;
	}

	BOOLEAN PrepareRealtimeEditorWorldSwap()
	{
		OS0WindowManager& windows = gUIRuntime.windowManager();
		windows.cancelDrag();
		StopFeedbackEditing();
		if (!PreserveHeldItemBeforeWorldTeardown())
		{
			RecordFeedbackEvent(
				"WORLD SWAP BLOCKED / PLACE OR RETURN HELD ITEM");
			windows.setSuspended(OS0WindowSuspendReason::WORLD_SWAP, FALSE);
			return FALSE;
		}
		windows.setSuspended(OS0WindowSuspendReason::WORLD_SWAP, TRUE);
		ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		CancelWorldMoveState();
		gPendingWorldAction.reset();
		CloseContextMenu();
		OS0TacticalState& tactical = OS0GetTacticalSession().state();
		tactical.coverOrders.clear();
		tactical.pendingVisualEvents.clear();
		tactical.cursor = {};
		gNextCombatProjectionAt = 0;
		OS0ResetDirectControl();
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;

		gUIRuntime.hideTransientWorldPanels();
		gUIRuntime.hide(OS0UIPanel::ASSET_LIBRARY);
		gUIRuntime.hide(OS0UIPanel::ASSET_CATALOG);
		ResetWorldBoundUIState();
		ResetFieldTutorialTarget();
		NotifyFieldTutorial(OS0FieldTutorialEvent::TARGET_LOST);
		gDebugAssetLibrary.clear();
		gDebugAssetLibrarySector = 0xff;
		gDebugAssetLibraryTileset = -1;
		for (ImpactParticle& particle : gImpactParticles) particle.born = 0;
		SetBagRegionsEnabled(TRUE);
		return TRUE;
	}

	void FinishRealtimeEditorWorldSwap(BOOLEAN const succeeded)
	{
		if (succeeded)
		{
			// Damage records are keyed by sector/grid/tile. A replacement map is a
			// new geometry identity and must not inherit durability from its predecessor.
			OS0GetTacticalSession().state().assetDamage.clear();
			gFieldToolIssued = FALSE;
		}
		BindInventorySoldier(GetSelectedMan());
		if (gInventorySoldier && gBagVisible)
			BindInspectedSoldier(gInventorySoldier);
		gUIRuntime.windowManager().setSuspended(
			OS0WindowSuspendReason::WORLD_SWAP, FALSE);
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void UpdateRealtimeEditorSession()
	{
		OS0RealtimeEditorSession& editor = OS0GetRealtimeEditor();
		const BOOLEAN worldSwap = editor.willInvalidateWorldPointers();
		if (worldSwap && !PrepareRealtimeEditorWorldSwap())
		{
			OS0GetRealtimeEditorUI().update();
			return;
		}
		editor.update();
		BOOLEAN worldSwapSucceeded = FALSE;
		BOOLEAN worldChanged = FALSE;
		for (OS0EditorCommandResult const& result : editor.drainResults())
		{
			worldChanged = static_cast<BOOLEAN>(worldChanged || result.worldChanged);
			RecordFeedbackEvent(ST::format("EDITOR {} / {}",
				result.success ? "OK" : "ERROR", result.message));
			if ((result.type == OS0EditorCommandType::NEW_BLANK_MAP ||
				result.type == OS0EditorCommandType::LOAD_MAP) &&
				result.success)
				worldSwapSucceeded = TRUE;
		}
		if (worldChanged) OS0NotifyWorldMutation();
		if (worldSwap) FinishRealtimeEditorWorldSwap(worldSwapSucceeded);
		OS0GetRealtimeEditorUI().update();
		if (OS0GetRealtimeEditorUI().active() && !gpItemPointer)
		{
			ContextAction action = ContextAction::INSPECT;
			switch (OS0GetRealtimeEditorUI().toolState().tool)
			{
				case OS0RealtimeEditorTool::SELECT:
					action = ContextAction::INSPECT; break;
				case OS0RealtimeEditorTool::PLACE:
					action = ContextAction::USE; break;
				case OS0RealtimeEditorTool::ERASE:
					action = ContextAction::SALVAGE; break;
				case OS0RealtimeEditorTool::COUNT: break;
			}
			if (CursorState().action != action) ApplyCursorTool(action);
		}
	}

	void DrawManagedWindow(OS0WindowHandle const id)
	{
		switch (static_cast<OS0ManagedWindow>(id))
		{
			case OS0ManagedWindow::INVENTORY:
				DrawBag();
				break;
			case OS0ManagedWindow::CONTEXT:
				DrawContextMenu();
				break;
			case OS0ManagedWindow::LOOT:
				DrawLootMode();
				break;
			case OS0ManagedWindow::EQUIPMENT:
				DrawExplodedEquipment();
				break;
			case OS0ManagedWindow::STACK_SPLIT:
				DrawStackSplitDialog();
				break;
			case OS0ManagedWindow::ASSET_LIBRARY:
				DrawGodIconLibrary();
				break;
			case OS0ManagedWindow::ASSET_CATALOG:
				DrawAssetCatalog();
				break;
			case OS0ManagedWindow::SECTOR:
				DrawSectorPanel();
				break;
			case OS0ManagedWindow::INSPECTOR:
				DrawHoverInspector();
				break;
			case OS0ManagedWindow::TOOLBOX:
				DrawToolbox();
				break;
			case OS0ManagedWindow::ENVIRONMENT:
				DrawEnvironmentPanel();
				break;
			case OS0ManagedWindow::REALTIME_EDITOR:
				OS0GetRealtimeEditorUI().render();
				break;
			case OS0ManagedWindow::COUNT:
				break;
		}
	}

	void DrawManagedWindows()
	{
		for (OS0WindowHandle const id :
			gUIRuntime.windowManager().renderOrder())
			DrawManagedWindow(id);
	}

	void OS0MouseSystemEventHook(UINT16 const type, UINT32 const button,
		UINT16 const x, UINT16 const y)
	{
		if (!gInitialized) return;
		const BOOLEAN primaryDown =
			(type == MOUSE_BUTTON_DOWN && button == MOUSE_BUTTON_LEFT) ||
			type == TOUCH_FINGER_DOWN;
		const BOOLEAN primaryUp =
			(type == MOUSE_BUTTON_UP && button == MOUSE_BUTTON_LEFT) ||
			type == TOUCH_FINGER_UP;
		if (!primaryDown && !primaryUp) return;
		// MouseSystem deliberately does not deliver UP to the pressed region after
		// the pointer has left it. Preserve the raw multitool's capture until this
		// physical boundary so a fast drag-release cannot fall through as a held
		// item drop or world click behind the orb.
		const BOOLEAN rawMultiToolRelease = primaryUp &&
			(gMultiToolDragCandidate || gMultiToolDragging);
		const BOOLEAN rawMultiToolWasDragging = rawMultiToolRelease &&
			gMultiToolDragging;
		if (rawMultiToolRelease)
		{
			gMultiToolDragCandidate = FALSE;
			gMultiToolDragging = FALSE;
			if (rawMultiToolWasDragging)
			{
				PositionBagRegions();
				SaveUILayout();
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
			}
		}
		// Close a viewport-cancel suppression at the physical event boundary,
		// before any following DOWN in the same MouseSystem batch can begin.
		if (primaryUp) OS0RecoverViewportPointerGestures(FALSE);

		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reconcile(gpItemPointer != nullptr);
		if (primaryDown)
		{
			transfers.observePrimaryDown();
			// A native item cursor can begin outside one of our explicit regions
			// (stack split, context command, save restore). The physical DOWN still
			// starts exactly one controller gesture.
			if (gpItemPointer && !transfers.ownsPhysicalGesture())
				transfers.beginHeldGesture();
			return;
		}
		if (transfers.consumeSuppressedRelease()) return;
		if (transfers.releaseWasHandled()) return;
		if (rawMultiToolRelease)
		{
			if (transfers.ownsPhysicalGesture() &&
				transfers.claimRelease(OS0ItemTransferSurface::RELATION) ==
					OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		if (!transfers.ownsPhysicalGesture()) return;
		if (!gpItemPointer)
		{
			// DOWN belonged to a source slot, but the pointer left before a drag
			// was established. Consume it as a source click, never as a world click.
			transfers.claimRelease(OS0ItemTransferSurface::EXTERNAL);
			return;
		}

		MOUSE_REGION* const current = MSYS_GetCurrentRegion();
		const UINT32 reason = type == TOUCH_FINGER_UP ?
			MSYS_CALLBACK_REASON_TFINGER_UP : MSYS_CALLBACK_REASON_LBUTTON_UP;
		for (MOUSE_REGION& region : gSlotRegions)
		{
			if (current == &region) { SlotCallback(&region, reason); return; }
		}
		for (MOUSE_REGION& region : gEquipmentRegions)
		{
			if (current == &region)
			{
				EquipmentSlotCallback(&region, reason);
				return;
			}
		}
		for (MOUSE_REGION& region : gLootRegions)
		{
			if (current == &region) { LootSlotCallback(&region, reason); return; }
		}
		for (MOUSE_REGION& region : gItemTransferIntentRegions)
		{
			if (current == &region)
			{
				ItemTransferIntentCallback(&region, reason);
				return;
			}
		}
		if (current == &gItemTransferMoreRegion)
		{
			ItemTransferMoreCallback(current, reason);
			return;
		}
		if (current == &gEquipmentPackRegion)
		{
			EquipmentPackCallback(current, reason);
			return;
		}

		if (OS0BlocksWorldInputAt(static_cast<INT16>(x), static_cast<INT16>(y)))
		{
			if (transfers.claimRelease(OS0ItemTransferSurface::RELATION) ==
				OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}
		const BOOLEAN insideWorld = x >= gsVIEWPORT_START_X &&
			x < gsVIEWPORT_END_X && y >= gsVIEWPORT_WINDOW_START_Y &&
			y < OS0WorldViewportBottom();
		if (!insideWorld)
		{
			if (transfers.claimRelease(OS0ItemTransferSurface::EXTERNAL) ==
				OS0ItemReleaseClaim::ITEM)
				transfers.completeItemRelease(TRUE);
			return;
		}

		// The viewport callback normally never receives this UP because JA2 still
		// remembers the original source region. Re-enter its semantic resolver
		// once; it now projects the target afresh instead of trusting stale hover.
		OS0HandleViewportPointerEvent(&gViewportRegion, reason);
		if (!transfers.releaseWasHandled() && transfers.ownsPhysicalGesture() &&
			transfers.claimRelease(OS0ItemTransferSurface::EXTERNAL) ==
				OS0ItemReleaseClaim::ITEM)
			transfers.completeItemRelease(TRUE);
	}
}


static BOOLEAN PreserveHeldItemBeforeWorldTeardown()
{
	OS0ItemTransferRuntime& runtime = OS0GetItemTransferRuntime();
	if (!gpItemPointer)
	{
		runtime.reset();
		OS0GetItemTransferController().reset();
		ClearHeldItemCarrier();
		gHeldItemRecoveryPending = FALSE;
		return TRUE;
	}

	// First choice is the exact source recorded when the object was detached.
	// cancel() either restores that source atomically or deliberately leaves the
	// complete native object held for the lossless fallback below.
	if (runtime.held() &&
		runtime.cancel() == OS0ItemTransferCancelResult::RESTORED)
	{
		OS0GetItemTransferController().reset();
		ClearHeldItemCarrier();
		gHeldItemRecoveryPending = FALSE;
		return TRUE;
	}

	SOLDIERTYPE* actor = BoundHeldItemCarrier();
	if (!actor)
		actor = GetSelectedMan();
	if (actor && actor->bActive && OS0CanPackObject(actor, *gpItemPointer) &&
		PlacePointerInActorPack(actor))
	{
		if (gpItemPointer && gpItemPointer->ubNumberOfObjects == 0)
			FinishCommittedItemPointer();
	}
	if (gpItemPointer)
	{
		const GridNo fallbackGrid = actor && actor->sGridNo >= 0 &&
			actor->sGridNo < WORLD_MAX ? actor->sGridNo : CENTER_GRIDNO;
		const UINT8 fallbackLevel = actor ? actor->bLevel : 0;
		if (AddItemToPool(fallbackGrid, gpItemPointer, VISIBLE,
			fallbackLevel, 0, -1) >= 0)
		{
			FinishCommittedItemPointer();
			OS0NotifyWorldMutation();
		}
	}
	if (gpItemPointer)
	{
		// This is only reachable after both an exact restoration and the engine's
		// world-pool allocation failed. Retain the native pointer rather than
		// claiming a false commit; the diagnostic makes the boundary explicit.
		SLOGE("OS0 could not preserve held item before tactical world teardown");
		return FALSE;
	}
	runtime.reset();
	OS0GetItemTransferController().reset();
	ClearHeldItemCarrier();
	gHeldItemRecoveryPending = FALSE;
	return TRUE;
}


void InitializeOS0IngameUI()
{
	if (gInitialized) return;
	// The editor singleton is process-lived, but every catalog and queued world
	// handle is sector-lived. Invalidate it before the UI can inspect a tileset.
	OS0GetRealtimeEditor().resetForTacticalSession();
	OS0GetItemTransferController().reset();
	OS0GetItemTransferRuntime().reset();
	gNextCombatProjectionAt = 0;
	gWorldProjectionStamp = 0;
	gWorldProjectionStampValid = FALSE;
	gPendingWorldAction.reset();
	gMultiToolExpanded = FALSE;
	gMultiToolLastClickAt = 0;
	gMultiToolDragCandidate = FALSE;
	gMultiToolDragging = FALSE;
	gEscapeKeyOwned = FALSE;
	gUILayout.configure(SCREEN_WIDTH, SCREEN_HEIGHT, gsVIEWPORT_WINDOW_END_Y);
	gUIRuntime.windowManager().setWorkspace(
		{ 0, 0, static_cast<INT16>(SCREEN_WIDTH),
			gUILayout.workspaceBottom() });
	gUIRuntime.enterCampaign(
		OS0GetTacticalSession().state().creatorCompleted);
	gFieldTutorial.reset(
		OS0GetTacticalSession().state().fieldTutorialCompleted);
	ResetFieldTutorialTarget();
	gFieldTutorialCompletedAt = 0;
	if (gTutorialActive)
	{
		gCreatorModel.reset();
		gVideoScrollBeforeCreator = gfDoVideoScroll;
		gfDoVideoScroll = FALSE;
	}
	gOrbX = std::max<INT16>(0, gsVIEWPORT_END_X - COLLAPSED_OS0_W - 4);
	gOrbY = std::max<INT16>(gsVIEWPORT_WINDOW_START_Y,
		gUILayout.worldBottom() - COMMAND_BAR_H - 4);
	const INT16 centerX = std::max<INT16>(0, (gsVIEWPORT_END_X - PANE_W) / 2);
	const INT16 centerY = std::max<INT16>(0, (WorkspaceBottom() - BAG_H) / 2);
	gBagX = centerX;
	gBagY = centerY;
	gInventoryX = gBagX;
	gInventoryY = gBagY;
	gUIRuntime.windowManager().setBounds(
		gUIRuntime.managedId(FloatingPanelId::SECTOR),
		{ std::max<INT16>(0, (gsVIEWPORT_END_X - SECTOR_PANEL_W) / 2), 24,
			SECTOR_PANEL_W, SECTOR_PANEL_H });
	gUIRuntime.windowManager().setBounds(
		gUIRuntime.managedId(FloatingPanelId::INSPECTOR),
		{ 8, std::max<INT16>(8, WorkspaceBottom() - INSPECTOR_H - 8),
			INSPECTOR_W, INSPECTOR_H });
	gUIRuntime.windowManager().setBounds(
		gUIRuntime.managedId(FloatingPanelId::TOOLBOX),
		{ std::max<INT16>(8, gsVIEWPORT_END_X - TOOLBOX_W - 8),
			std::max<INT16>(8, WorkspaceBottom() - TOOLBOX_H - 8),
			TOOLBOX_W, TOOLBOX_H });
	gUIRuntime.windowManager().setBounds(
		gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT),
		{ std::max<INT16>(8, gsVIEWPORT_END_X - ENVIRONMENT_W - 8),
			std::max<INT16>(8, WorkspaceBottom() - TOOLBOX_H -
				ENVIRONMENT_H - 16), ENVIRONMENT_W, ENVIRONMENT_H });
	ApplyArtworkWorkspaceLayout(!gTutorialActive);
	gGodLibraryX = std::max<INT16>(0,
		(gsVIEWPORT_END_X - GOD_LIBRARY_W) / 2);
	gGodLibraryY = std::max<INT16>(4,
		(gsVIEWPORT_WINDOW_END_Y - GOD_LIBRARY_H) / 2);
	LoadUILayout();
	if (gTutorialActive)
	{
		// The creator is a modal screen-space surface, not a remembered gameplay
		// window. Saved character-sheet coordinates must not move it off centre.
		gBagX = centerX;
		gBagY = centerY;
		ClampWindowPositions();
	}
	gAssetCatalogX = std::max<INT16>(0,
		(gsVIEWPORT_END_X - ASSET_CATALOG_W) / 2);
	gAssetCatalogY = std::max<INT16>(4,
		(gsVIEWPORT_WINDOW_END_Y - ASSET_CATALOG_H) / 2);
	OS0LoadAssetCatalog();
	ST::string resourceValidationError;
	if (!OS0ValidateResourceItemDefinitions(&resourceValidationError))
		throw std::runtime_error(resourceValidationError.c_str());
	OS0UIAssets().initialize();
	// Allocate render resources during initialization. Draw calls consume them
	// without changing renderer ownership or UI visibility mid-frame.
	if (!gAssetLibrarySymbolSurface)
		gAssetLibrarySymbolSurface = AddVideoSurface(64, 64, PIXEL_DEPTH);

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
	MSYS_DefineRegion(&gGodLibraryGrabber, 0, 0, GOD_LIBRARY_W, 20,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
		DebugLibraryGrabberCallback, DebugLibraryGrabberCallback);
	MSYS_DefineRegion(&gGodLibraryClose, 0, 0, 16, 16,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		DebugLibraryCloseCallback);
	for (size_t i = 0; i < gGodIconRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gGodIconRegions[i], 0, 0, 20, 20,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			GodIconCallback);
		gGodIconRegions[i].SetUserData<0>(i);
		gGodIconRegions[i].Disable();
	}
	for (size_t i = 0; i < gAssetLibraryRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gAssetLibraryRegions[i], 0, 0, 20, 18,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			DebugLibraryCallback);
		gAssetLibraryRegions[i].SetUserData<0>(i);
		gAssetLibraryRegions[i].Disable();
	}
	for (size_t i = 0; i < gToolboxRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gToolboxRegions[i], 0, 0, 30, 26,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ToolboxModuleCallback);
		gToolboxRegions[i].SetUserData<0>(i);
		gToolboxRegions[i].Disable();
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
	for (size_t i = 0; i < gEnvironmentSkillRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gEnvironmentSkillRegions[i], 0, 0, 72, 26,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			EnvironmentSkillCallback);
		gEnvironmentSkillRegions[i].SetUserData<0>(i);
		gEnvironmentSkillRegions[i].Disable();
	}
	for (size_t i = 0; i < gNearbyHintRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gNearbyHintRegions[i], 0, 0, 24, 24,
			MSYS_PRIORITY_HIGH, CURSOR_NORMAL, NearbyHintMoveCallback,
			NearbyHintCallback);
		gNearbyHintRegions[i].SetUserData<0>(i);
		gNearbyHintRegions[i].Disable();
	}
	MSYS_DefineRegion(&gHoverQuickActionRegion, 0, 0, 28, 28,
		MSYS_PRIORITY_HIGH, CURSOR_NORMAL, HoverQuickActionMoveCallback,
		HoverQuickActionCallback);
	gHoverQuickActionRegion.Disable();
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
		OS0UIRect const dock =
			gUILayout.command(OS0CommandForDockSlot(i));
		MSYS_DefineRegion(&gPanelDockRegions[i], 0, 0,
			dock.w, dock.h,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			PanelDockCallback);
		gPanelDockRegions[i].SetUserData<0>(i);
	}
	for (size_t i = 0; i < gFeedbackRegions.size(); ++i)
	{
		const INT16 width = i == 0 || i == 1 ? SECTOR_PANEL_W - 16 :
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
			SECTOR_PANEL_W - 16, 34, MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
			MSYS_NO_CALLBACK, SectorUpgradeCallback);
		gSectorUpgradeRegions[i].SetUserData<0>(i);
		gSectorUpgradeRegions[i].Disable();
	}
	for (size_t i = 0; i < gSectorTabRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gSectorTabRegions[i], 0, 0, 68, 19,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			SectorTabCallback);
		gSectorTabRegions[i].SetUserData<0>(i);
		gSectorTabRegions[i].Disable();
	}
	MSYS_DefineRegion(&gStrategicMapRegion, 0, 0,
		16 * STRATEGIC_CELL, 16 * STRATEGIC_CELL,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		StrategicMapCallback);
	gStrategicMapRegion.Disable();
	MSYS_DefineRegion(&gSectorTeamRegion, 0, 0, SECTOR_PANEL_W - 16, 150,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		SectorTeamCallback);
	gSectorTeamRegion.Disable();
	for (size_t i = 0; i < gSlots.size(); ++i)
	{
		const SlotLayout& slot = gSlots[i];
		MSYS_DefineRegion(&gSlotRegions[i], 0, 0, slot.w, slot.h,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, SlotCallback, SlotCallback);
		gSlotRegions[i].SetUserData<0>(slot.slot);
	}
	for (size_t i = 0; i < gEquipmentRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gEquipmentRegions[i], 0, 0, 34, 25,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, EquipmentSlotCallback,
			EquipmentSlotCallback);
		gEquipmentRegions[i].SetUserData<0>(gExplodedEquipmentSlots[i]);
		gEquipmentRegions[i].Disable();
	}
	MSYS_DefineRegion(&gEquipmentPackRegion, 0, 0, 46, 25,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		EquipmentPackCallback);
	gEquipmentPackRegion.Disable();
	for (size_t i = 0; i < gItemTransferIntentRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gItemTransferIntentRegions[i], 0, 0, 28, 28,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			ItemTransferIntentCallback);
		gItemTransferIntentRegions[i].SetUserData<0>(i);
		gItemTransferIntentRegions[i].Disable();
	}
	MSYS_DefineRegion(&gItemTransferMoreRegion, 0, 0, 28, 28,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		ItemTransferMoreCallback);
	gItemTransferMoreRegion.Disable();
	MSYS_DefineRegion(&gStackSplitBlock, 0, 0, 224, 82,
		MSYS_PRIORITY_HIGH, CURSOR_NORMAL, MSYS_NO_CALLBACK, BagBlockCallback);
	gStackSplitBlock.Disable();
	for (size_t i = 0; i < gStackSplitRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gStackSplitRegions[i], 0, 0, 30, 23,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			StackSplitCallback);
		gStackSplitRegions[i].SetUserData<0>(i);
		gStackSplitRegions[i].Disable();
	}

	MSYS_DefineRegion(&gOrbRegion, 0, gOrbY, COLLAPSED_OS0_W,
		gOrbY + COMMAND_BAR_H, MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
		OrbCallback, OrbCallback);
	MSYS_DefineRegion(&gCombatModeRegion, COLLAPSED_OS0_W, gOrbY,
		COLLAPSED_OS0_W + 28, gOrbY + COMMAND_BAR_H,
		MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
		CombatModeCallback);
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
	for (size_t i = 0; i < gTutorialBodyRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gTutorialBodyRegions[i], 0, 0, 103, 30,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			TutorialBodyCallback);
		gTutorialBodyRegions[i].SetUserData<0>(i);
		gTutorialBodyRegions[i].Disable();
	}
	for (size_t i = 0; i < gTutorialTraitRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gTutorialTraitRegions[i], 0, 0, 145, 12,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK,
			TutorialTraitCallback);
		gTutorialTraitRegions[i].SetUserData<0>(i);
		gTutorialTraitRegions[i].Disable();
	}
	for (size_t i = 0; i < gLootRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gLootRegions[i], 0, 0, 59, 28,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL,
			LootSlotCallback, LootSlotCallback);
		gLootRegions[i].SetUserData<0>(i);
		gLootRegions[i].Disable();
	}

	OS0GetRealtimeEditorUI().initialize(gUIRuntime.windowManager(),
		gUIRuntime.managedId(OS0UIWindow::REALTIME_EDITOR));
	PositionBagRegions();
	// Input ownership is global to the OS//0 shell, not conditional on the
	// optional character window.  Tying all regions to gBagVisible left the dock
	// dead whenever a completed campaign correctly started with inventory closed.
	SetBagRegionsEnabled(TRUE);
	gInitialized = TRUE;
	RecoverHeldItemCarrierIfPossible();
	// The vanilla tactical viewport starts with VIDEO_NO_CURSOR and only installs
	// its first free/tile cursor after a pointer-movement callback.  OS//0 can
	// enter the sector with the pointer already stationary over the world, which
	// otherwise leaves a zero-sized cursor until some unrelated UI transition.
	// Prime a visible cursor once; normal tactical cursor selection owns every
	// subsequent frame (including aim and held-item cursors).
	if (gpItemPointer) RefreshHeldItemCursor();
	else SetCurrentCursorFromDatabase(CURSOR_NORMAL);
	// Materialize the layout immediately. Drag releases and OS0 expansion keep
	// updating it, while a crash or forced process exit can no longer lose the
	// last successfully loaded/default arrangement.
	SaveUILayout();
	MSYS_SetEventHook(OS0MouseSystemEventHook);
	SetRenderFlags(RENDER_FLAG_FULL);
}


void ShutdownOS0IngameUI()
{
	if (!gInitialized) return;
	MSYS_SetEventHook(nullptr);
	SaveUILayout();
	OS0GetRealtimeEditorUI().shutdown();
	OS0GetRealtimeEditor().resetForTacticalSession();
	CancelWorldMoveState();
	gPendingWorldAction.reset();
	OS0NotifyWorldMutation();
	gFieldTutorial.notify(OS0FieldTutorialEvent::DISMISS);
	ResetFieldTutorialTarget();
	CloseContextMenu();
	const BOOLEAN itemPreserved = PreserveHeldItemBeforeWorldTeardown();
	OS0ResetViewportPointerGestures();
	if (!itemPreserved && gpItemPointer)
	{
		// Shutdown cannot defer the engine's sector teardown. Sever every live-world
		// origin while retaining the only authoritative object on the native cursor.
		// The next tactical initialization binds it to a freshly-created selected
		// actor instead of ever touching an unloaded slot, pool or structure.
		OS0GetItemTransferRuntime().reset();
		OS0ItemTransferController& transfers = OS0GetItemTransferController();
		transfers.reset();
		transfers.adoptExternalHeldItemAfterHandledRelease();
		gpItemPointerSoldier = nullptr;
		gbItemPointerSrcSlot = NO_SLOT;
		ClearHeldItemCarrier();
		gHeldItemRecoveryPending = TRUE;
		SLOGW("OS0 retained held item across tactical teardown without a live source");
		RecordFeedbackEvent("HELD ITEM RETAINED / RECOVERY ON NEXT SECTOR");
	}
	ClearItemTransferTarget();
	gItemTransferMoreVisible = FALSE;
	ResetWorldBoundUIState();
	MSYS_RemoveRegion(&gBagBlock);
	MSYS_RemoveRegion(&gBagGrabber);
	MSYS_RemoveRegion(&gBagClose);
	MSYS_RemoveRegion(&gContextBlock);
	MSYS_RemoveRegion(&gGodLibraryBlock);
	MSYS_RemoveRegion(&gGodLibraryGrabber);
	MSYS_RemoveRegion(&gGodLibraryClose);
	for (MOUSE_REGION& r : gGodIconRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gAssetLibraryRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gToolboxRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gAssetCatalogBlock);
	for (MOUSE_REGION& r : gAssetCatalogRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gContextRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gEnvironmentSkillRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gNearbyHintRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gHoverQuickActionRegion);
	for (MOUSE_REGION& r : gFloatingPanelBlocks) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFloatingPanelGrabbers) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFloatingPanelCloses) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gPanelDockRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gFeedbackRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gSectorUpgradeRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gSectorTabRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gStrategicMapRegion);
	MSYS_RemoveRegion(&gSectorTeamRegion);
	for (MOUSE_REGION& r : gSlotRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gEquipmentRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gEquipmentPackRegion);
	for (MOUSE_REGION& r : gItemTransferIntentRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gItemTransferMoreRegion);
	MSYS_RemoveRegion(&gStackSplitBlock);
	for (MOUSE_REGION& r : gStackSplitRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gOrbRegion);
	MSYS_RemoveRegion(&gCombatModeRegion);
	MSYS_RemoveRegion(&gTutorialContinue);
	for (MOUSE_REGION& r : gTutorialStats) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gTutorialBodyRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gTutorialTraitRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gLootRegions) MSYS_RemoveRegion(&r);
	OS0UIAssets().shutdown();
	DiscardWorldZoomBuffer();
	if (gWorldZoom > 1)
	{
		gfDoVideoScroll = gVideoScrollBeforeZoom;
		gWorldZoom = 1;
	}
	if (gTutorialActive) gfDoVideoScroll = gVideoScrollBeforeCreator;
	if (gInspectorPreview)
	{
		DeleteVideoSurface(gInspectorPreview);
		gInspectorPreview = nullptr;
	}
	if (gAnimatedMercPreview)
	{
		DeleteVideoSurface(gAnimatedMercPreview);
		gAnimatedMercPreview = nullptr;
		gAnimatedMercPreviewSoldier = nullptr;
	}
	if (gAssetLibrarySymbolSurface)
	{
		DeleteVideoSurface(gAssetLibrarySymbolSurface);
		gAssetLibrarySymbolSurface = nullptr;
	}
	SetUIKeyboardHook(nullptr);
	gFeedbackEditing = FALSE;
	gAimAutoCollapsed = FALSE;
	gNextCombatProjectionAt = 0;
	gUIRuntime.windowManager().setSuspended(OS0WindowSuspendReason::AIM, FALSE);
	gFieldToolIssued = FALSE;
	gGodLibraryVisible = FALSE;
	gUIRuntime.windowManager().cancelDrag();
	gAssetCatalogVisible = FALSE;
	gAssetCatalogReturnToLibrary = FALSE;
	gAssetCatalogNameEditing = FALSE;
	OS0ResetDirectControl();
	OS0GetTacticalSession().endTacticalSector();
	gDebugAssetLibrary.clear();
	gDebugAssetLibrarySector = 0xff;
	gDebugAssetLibraryTileset = -1;
	for (ImpactParticle& particle : gImpactParticles) particle.born = 0;
	gWorldProjectionStamp = 0;
	gWorldProjectionStampValid = FALSE;
	gInitialized = FALSE;
}


void OS0RenderAutoFirstAidStatus(BOOLEAN complete, UINT32 elapsedSeconds)
{
	// Auto-bandage owns a modal simulation loop and therefore bypasses the
	// regular tactical/OS//0 frame.  Draw only one compact state indicator here;
	// never revive TEAM_PANEL, SM_PANEL, radar, inventory or the old portrait box.
	constexpr INT16 w = 258;
	constexpr INT16 h = 38;
	const INT16 x = std::max<INT16>(4, (SCREEN_WIDTH - w) / 2);
	const INT16 y = std::max<INT16>(4, std::min<INT16>(
		SCREEN_HEIGHT - h - 5, gsVIEWPORT_END_Y - h - 5));
	const UINT16 black = Get16BPPColor(FROMRGB(2, 3, 3));
	const UINT16 dark = Get16BPPColor(FROMRGB(12, 13, 11));
	const UINT16 red = Get16BPPColor(FROMRGB(132, 0, 0));
	const UINT16 bright = Get16BPPColor(FROMRGB(218, 18, 18));

	ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + w - 1, y + h - 1, black);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + w - 1, y + 1,
		complete ? bright : red);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 5, y + 7, x + 29, y + 31, dark);
	OutlineBox(x, y, w, h, complete ? bright : red);

	// A small first-aid cross reads at native JA2 resolution and needs no new
	// bitmap asset.
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 15, y + 11,
		x + 19, y + 27, bright);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 9, y + 17,
		x + 25, y + 21, bright);

	SetFont(TINYFONT1);
	SetFontBackground(FONT_MCOLOR_BLACK);
	SetFontForeground(complete ? FONT_WHITE : FONT_MCOLOR_RED);
	MPrint(x + 38, y + 7, complete ?
		"AUTO FIRST AID / COMPLETE" : "AUTO FIRST AID / TREATING");
	SetFontForeground(FONT_MCOLOR_LTGRAY);
	const ST::string detail = complete ?
		"ENTER OR SPACE: RETURN TO FIELD" :
		ST::format("{} SEC / ESC: ABORT", elapsedSeconds);
	MPrint(x + 38, y + 21, detail);
	InvalidateRegion(x, y, x + w, y + h);
}


void UpdateOS0TacticalSession()
{
	if (!gInitialized) InitializeOS0IngameUI();
	RecoverHeldItemCarrierIfPossible();
	// Editor callbacks only enqueue stable ids. This is the single tactical
	// frame boundary where those commands may mutate canonical JA2 world state.
	UpdateRealtimeEditorSession();
	OS0TacticalState& state = OS0GetTacticalSession().state();
	BOOLEAN inputRegionsDirty = RevalidateSoldierViews();
	for (OS0ImpactVisualEvent const& event : state.pendingVisualEvents)
	{
		for (UINT8 i = 0; i < 6; ++i)
		{
			ImpactParticle& particle = gImpactParticles[gImpactParticleNext];
			gImpactParticleNext = (gImpactParticleNext + 1) % gImpactParticles.size();
			particle.gridNo = event.gridNo;
			particle.velocityX = static_cast<INT8>(static_cast<INT16>((i * 5 +
				event.gridNo) % 9) - 4);
			particle.velocityY = static_cast<INT8>(-7 - (i % 4));
			particle.colourKind = static_cast<UINT8>(event.material);
			particle.born = std::max<UINT32>(1, GetJA2Clock());
		}
	}
	state.pendingVisualEvents.clear();
	for (ST::string const& diagnostic : state.pendingDiagnostics)
		RecordFeedbackEvent(diagnostic);
	state.pendingDiagnostics.clear();
	UpdateImpactParticles();
	if (gStackSplitVisible)
	{
		const BOOLEAN validStack = gStackSplitSoldier &&
			DeferredContextStillValid(gStackSplitIdentity) &&
			DeferredContextActor(gStackSplitIdentity) == gStackSplitSoldier &&
			gStackSplitSlot >= 0 &&
			gStackSplitSlot < NUM_INV_SLOTS &&
			gStackSplitSoldier->inv[gStackSplitSlot].usItem != NOTHING &&
			gStackSplitSoldier->inv[gStackSplitSlot].ubNumberOfObjects > 1;
		if (!validStack) CloseStackSplit();
	}
	if (gGodLibraryVisible &&
		(gDebugAssetLibrarySector != gWorldSector.AsByte() ||
		 gDebugAssetLibraryTileset != static_cast<INT16>(giCurrentTilesetID)))
	{
		// Sector/tileset changes rebuild the registry at the update boundary.
		// DrawGodIconLibrary must never scan and mutate world-derived state.
		RebuildDebugAssetLibrary();
	}
	if (!gpItemPointer)
	{
		if (gEquipmentAutoForHeldItem)
		{
			gEquipmentExplodedVisible = FALSE;
			BindEquipmentSoldier(nullptr);
			gEquipmentAutoForHeldItem = FALSE;
			inputRegionsDirty = TRUE;
		}
		if (HasItemTransferTargetBinding() || gHeldItemCarrierInstanceId != 0)
		{
			ClearItemTransferTarget();
			ClearHeldItemCarrier();
			gItemTransferMoreVisible = FALSE;
			inputRegionsDirty = TRUE;
		}
	}
	else if (HasItemTransferTargetBinding() && !BoundItemTransferTarget())
	{
		ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		inputRegionsDirty = TRUE;
	}
	else if (gpItemPointer && gHeldItemCarrierInstanceId != 0 &&
		!BoundHeldItemCarrier())
	{
		// The SOLDIERTYPE slot behind the native cursor was retired or reused.
		// Keep the item authoritative on the cursor, but never act through that
		// stale carrier until recovery can bind it to a live selected merc.
		ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		inputRegionsDirty = TRUE;
	}
	fRenderRadarScreen = FALSE;
	if (inputRegionsDirty) SetBagRegionsEnabled(TRUE);
	UpdateWindowDragging();
	UpdateMultiToolDragging();
	if (!gTutorialActive && !gFieldToolIssued)
	{
		if (SOLDIERTYPE* const selected = GetSelectedMan())
		{
			EnsureDebugFieldTools(selected);
			gFieldToolIssued = TRUE;
		}
	}
	if (!gTutorialActive) EnsureSelectedOperatorTraitEffects();
	SynchronizeInteractionMode();
	if (!gTutorialActive)
	{
		OS0RefreshWorldHoverFromPointer();
		UpdateFieldTutorial();
		OS0UpdateDirectControl(GetSelectedMan(), !DirectControlBlocked());
		ProjectControlModeToEngine();
		UpdatePendingWorldAction();
		const BOOLEAN aiming = !gContextVisible && !gpItemPointer &&
			CombatModeActive();
		if (aiming && !gAimAutoCollapsed)
		{
			StopFeedbackEditing();
			gUIRuntime.windowManager().setSuspended(
				OS0WindowSuspendReason::AIM, TRUE);
			gAimAutoCollapsed = TRUE;
			gStackSplitVisible = FALSE;
			gStackSplitSoldier = nullptr;
			gStackSplitSlot = NO_SLOT;
			gStackSplitIdentity = {};
			gAssetCatalogNameEditing = FALSE;
			SetUIKeyboardHook(nullptr);
			CloseContextMenu();
			DiscardWorldZoomBuffer();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if (!aiming && gAimAutoCollapsed)
		{
			gAimAutoCollapsed = FALSE;
			gUIRuntime.windowManager().setSuspended(
				OS0WindowSuspendReason::AIM, FALSE);
			DiscardWorldZoomBuffer();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
	if (gEnvironmentGridNo >= 0 && gEnvironmentGridNo < WORLD_MAX &&
		gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::ENVIRONMENT)))
	{
		SOLDIERTYPE const* const selected = GetSelectedMan();
		const GridNo actorGrid = selected ? selected->sGridNo : NOWHERE;
		if ((actorGrid != gEnvironmentActorGridNo ||
			GetJA2Clock() >= gNextEnvironmentRefreshAt) &&
			RefreshEnvironmentTarget(gEnvironmentGridNo, gEnvironmentLevel,
				gEnvironmentTileIndex))
			SetBagRegionsEnabled(TRUE);
	}
	if (gLootVisible && !IsInspectedWorldAssetNear())
	{
		gLootVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
	}
	UpdateLootProjectionState();
	if (!gTutorialActive && gInspectedGridNo == NOWHERE)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (selected && (!gInspectedSoldier ||
			gInspectedSoldier->bTeam == OUR_TEAM))
		{
			BindInspectedSoldier(selected);
			BindInventorySoldier(selected);
		}
	}
	if (gTutorialActive)
	{
		EmptyDialogueQueue();
		StopAnyCurrentlyTalkingSpeech();
	}
	if (gfInItemPickupMenu)
	{
		// Exact world/container relations replace JA2's modal pickup list. Destroy
		// its buttons and regions at the state-update boundary, never from Draw().
		RemoveItemPickupMenu();
	}

	// Simulation progresses once per tactical frame and is independent of panel
	// visibility. RenderOS0IngameUI only projects the resulting state.
	UpdateWorldMove();
	UpdateCoverCommands();

	// Native JA2 paths (save restore, stack split and cancel) may create or end
	// the cursor outside an OS//0 region callback. Reconcile at the state-update
	// boundary, never while drawing the frame.
	OS0ItemTransferController& transfers = OS0GetItemTransferController();
	transfers.reconcile(gpItemPointer != nullptr);
	OS0RecoverViewportPointerGestures(
		IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown());
	// Focus changes may release SDL's button without a final region event. This
	// recovery runs after input dispatch, so a source can never remain captured.
	transfers.recoverLostRelease(
		IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMainFingerDown(),
		gpItemPointer != nullptr);
	RefreshHeldItemCursor();
}


void OS0PrepareScreenProjection(BOOLEAN const updateDynamicState)
{
	if (!gInitialized) InitializeOS0IngameUI();
	if (updateDynamicState && !gTutorialActive)
	{
		// The camera was committed by ScrollWorld() after UpdateOS0TacticalSession.
		// Resolve the relation under a stationary pointer against this exact frame.
		OS0RefreshWorldHoverFromPointer();
		UpdateNearbyInteractionHints();
	}
	BOOLEAN projectionChanged = ProjectHoverQuickActionRegion();
	projectionChanged = ProjectNearbyInteractionHintRegions() || projectionChanged;
	PositionLootRegions();
	PositionEquipmentRegions();
	PositionItemTransferIntentRegions();
	if (gContextVisible) PositionContextRegions();

	const std::uint64_t stamp = WorldProjectionStamp();
	projectionChanged = !gWorldProjectionStampValid ||
		stamp != gWorldProjectionStamp || projectionChanged;
	gWorldProjectionStamp = stamp;
	gWorldProjectionStampValid = TRUE;
	if (projectionChanged)
		SetRenderFlags(RENDER_FLAG_FULL);
}


void RenderOS0IngameUI()
{
	if (!gInitialized) InitializeOS0IngameUI();
	if (!gTutorialActive && gBagVisible)
		CaptureAnimatedMercPreview(gInventorySoldier ?
			gInventorySoldier : GetSelectedMan());
	if (!gTutorialActive) DrawArtworkBrand();
	DrawWorldSelection();
	DrawHoverQuickAction();
	DrawNearbyInteractionHints();
	DrawImpactParticles();
	if (!gAimAutoCollapsed)
	{
		DrawActionMenu();
		// All registered windows now share one z-order. World-attached effects
		// stay below them; newly focused/modal windows therefore cannot be painted
		// underneath an older hard-coded draw call.
		DrawManagedWindows();
		DrawFieldTutorial();
		// The held-item relation is a cursor projection rather than a window and
		// remains immediately visible above the selected destination. Modals keep
		// exclusive ownership of the frame.
		if (!gStackSplitVisible && !gAssetCatalogVisible)
			DrawItemTransferIntents();
	}
	// The movable multitool remains visible in COMBAT. Minimized it is one icon;
	// unfolding exposes the stateful TARGET/WALK switch again.
	DrawOrb();
}


BOOLEAN OS0CreatorIsActive()
{
	return gInitialized && gTutorialActive;
}


BOOLEAN OS0OwnsViewportPrimaryButton()
{
	return gInitialized && !gTutorialActive &&
		(CombatModeActive() || CursorState().action != ContextAction::MOVE ||
		 gHoverActionExplicit ||
		 gpItemPointer || CarryState().active() ||
		 OS0GetRealtimeEditorUI().active());
}


BOOLEAN OS0CanBeginWorldPointerDrag(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 const worldItemIndex)
{
	if (!gInitialized || gTutorialActive || gpItemPointer ||
		CarryState().active() || (gTacticalStatus.uiFlags & INCOMBAT) ||
		gridNo < 0 || gridNo >= WORLD_MAX || level > 1) return FALSE;
	SOLDIERTYPE* const carrier = GetSelectedMan();
	if (!carrier || !carrier->bActive || carrier->bTeam != OUR_TEAM ||
		!OK_CONTROLLABLE_MERC(carrier) || carrier->bLife < OKLIFE ||
		carrier->bLevel != level ||
		PythSpacesAway(carrier->sGridNo, gridNo) > 1) return FALSE;

	if (worldItemIndex >= 0)
	{
		if (static_cast<size_t>(worldItemIndex) >= gWorldItems.size()) return FALSE;
		WORLDITEM const& worldItem = GetWorldItem(worldItemIndex);
		return worldItem.sGridNo == gridNo && worldItem.ubLevel == level &&
			OS0IsActionableLooseWorldItem(worldItem);
	}
	// Structure persistence currently records the ground struct layer only.
	// Reject roof scenery before a carry can start instead of allowing a drag
	// whose commit cannot remove/recreate the same on-roof node.
	if (level != 0) return FALSE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	return tileIndex < NUMBEROFTILES &&
		IsWorldAssetMovableAt(gridNo, level, tileIndex, carrier);
}


BOOLEAN OS0BeginWorldPointerDrag(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 const worldItemIndex)
{
	if (!OS0CanBeginWorldPointerDrag(gridNo, level, tileIndex, worldItemIndex))
		return FALSE;
	SOLDIERTYPE* const carrier = GetSelectedMan();
	if (worldItemIndex >= 0)
	{
		if (!BeginTrackedWorldItemTransfer(carrier, worldItemIndex, NO_TILE, FALSE))
			return FALSE;
		// The cursor was created after this physical DOWN crossed its threshold.
		// Adopt that already-running gesture so the same UP can commit a target;
		// without this edge the item appears to stick until a second click.
		OS0GetItemTransferController().beginHeldGesture();
		RecordFeedbackEvent("WORLD ITEM / DRAG ACTIVE / DROP ON BODY, PACK OR GROUND");
		return TRUE;
	}

	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	if (!BeginWorldMoveAt(gridNo, level, tileIndex, OS0CarryMode::CARRY,
		carrier, TRUE)) return FALSE;
	SendSoldierSetDesiredDirectionEvent(carrier,
		GetDirectionFromGridNo(gridNo, carrier));
	RecordFeedbackEvent("WORLD ASSET / DRAG ACTIVE / RELEASE TO PLACE");
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}


void OS0CancelWorldPointerDrag()
{
	if (!CarryState().active() || !CarryState().pointerDrag) return;
	CancelWorldMoveState();
	CursorState().action = ContextAction::MOVE;
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
}


BOOLEAN OS0SuppressCarriedWorldNode(GridNo gridNo, UINT8 level,
	LEVELNODE const* node)
{
	OS0CarryState const& carry = CarryState();
	const BOOLEAN hasReplacementProjection = carry.walking() ||
		carry.pointerDrag || (carry.pending() && !carry.persistentGrab);
	if (!carry.active() || !hasReplacementProjection || !node ||
		gridNo != carry.source || level != carry.sourceLevel) return FALSE;
	if ((node->uiFlags & LEVELNODE_BUDDYSHADOW) && carry.shadowInstance != 0)
		return reinterpret_cast<std::uintptr_t>(node) == carry.shadowInstance;
	if (!node->pStructureData || node->usIndex != carry.tileIndex) return FALSE;
	STRUCTURE const* const base = FindBaseStructure(node->pStructureData);
	return base && (base->fFlags & STRUCTURE_BASE_TILE) &&
		carry.boundToStructure(base->usStructureID,
			StructureBaseGridNo(base));
}

BOOLEAN OS0BlocksWorldInputAt(INT16 const screenX, INT16 const screenY)
{
	// Context radials use an actor/world anchor instead of an opaque rectangle,
	// but while open they still own the whole interaction.  This prevents F or a
	// click outside the icons from silently retargeting and rebuilding the hub.
	const BOOLEAN quickActionOwnsPointer =
		OS0HoverQuickActionOwnsPointer(screenX, screenY);
	return gInitialized && (gContextVisible || quickActionOwnsPointer ||
		OS0NearbyHintOwnsPointer(screenX, screenY) ||
		MultiToolOwnsPointerAt(screenX, screenY) ||
		gUIRuntime.windowManager().blocksWorldInputAt(screenX, screenY));
}


BOOLEAN OS0HoverQuickActionOwnsPointer(INT16 const screenX,
	INT16 const screenY)
{
	return gInitialized &&
		(gHoverQuickActionRegion.uiFlags & MSYS_REGION_ENABLED) &&
		screenX >= gHoverQuickActionRegion.RegionTopLeftX &&
		screenX <= gHoverQuickActionRegion.RegionBottomRightX &&
		screenY >= gHoverQuickActionRegion.RegionTopLeftY &&
		screenY <= gHoverQuickActionRegion.RegionBottomRightY;
}


BOOLEAN OS0NearbyHintOwnsPointer(INT16 const screenX, INT16 const screenY)
{
	if (!gInitialized) return FALSE;
	for (size_t i = 0; i < gNearbyHintCount; ++i)
		if (EnabledRegionContains(gNearbyHintRegions[i], screenX, screenY))
			return TRUE;
	return FALSE;
}


BOOLEAN OS0RefreshCurrentNearbyHintHover(INT16 const screenX,
	INT16 const screenY)
{
	if (!gInitialized) return FALSE;
	for (size_t i = 0; i < gNearbyHintCount; ++i)
	{
		MOUSE_REGION const& region = gNearbyHintRegions[i];
		if (!EnabledRegionContains(region, screenX, screenY)) continue;
		NearbyInteractionHint const hint = gNearbyHints[i];
		if (!BindingStillValid(hint.binding))
		{
			// The glyph still owns this frame's pointer event, but it must not
			// preserve or execute a world identity invalidated underneath it.
			ResetNearbyScanCache();
			OS0ClearWorldHover();
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		OS0HoverWorldObject(nullptr, hint.gridNo, hint.level, hint.tileIndex,
			region.RegionTopLeftX + 12, region.RegionTopLeftY + 12,
			hint.binding.worldItemIndex);
		return TRUE;
	}
	return FALSE;
}


BOOLEAN OS0ActivateCurrentNearbyHintInteraction(INT16 const screenX,
	INT16 const screenY, BOOLEAN const cycleAction)
{
	if (!gInitialized) return FALSE;
	for (size_t i = 0; i < gNearbyHintCount; ++i)
	{
		if (!EnabledRegionContains(gNearbyHintRegions[i], screenX, screenY)) continue;
		NearbyInteractionHint const hint = gNearbyHints[i];
		if (!BindingStillValid(hint.binding))
		{
			ResetNearbyScanCache();
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		if (cycleAction)
			OS0CycleCursorAction(nullptr, hint.gridNo, hint.level, hint.tileIndex,
				hint.binding.worldItemIndex);
		else
			OS0ActivateHoveredInteraction(nullptr, hint.gridNo, hint.level,
				hint.tileIndex, screenX, screenY,
				hint.binding.worldItemIndex);
		return TRUE;
	}
	return FALSE;
}


BOOLEAN OS0BlocksKeyboardWorldInputAt(INT16 const screenX,
	INT16 const screenY)
{
	// A world-attached quick-action glyph is a pointer owner, not a keyboard
	// modal. F must still address the object represented by that glyph.
	return gInitialized && (gContextVisible ||
		MultiToolOwnsPointerAt(screenX, screenY) ||
		gUIRuntime.windowManager().blocksWorldInputAt(screenX, screenY));
}


BOOLEAN OS0BlocksMouseEdgeScroll()
{
	return gInitialized &&
		(OS0BlocksWorldInputAt(gusMouseXPos, gusMouseYPos) ||
		 gUIRuntime.windowManager().draggingWindow() != OS0_INVALID_WINDOW ||
		 gMultiToolDragCandidate || gMultiToolDragging);
}


void OS0OpenCharacterPanel(SOLDIERTYPE* soldier)
{
	if (!soldier || GetJA2Clock() < gPanelInteractionGuardUntil) return;
	InteractionMode().beginInteraction(OS0InteractionSurface::EQUIPMENT);
	CloseContextMenu();
	BindInspectedSoldier(soldier);
	gInspectedGridNo = NOWHERE;
	if (soldier->bTeam == OUR_TEAM)
	{
		BindInventorySoldier(soldier);
		gUIRuntime.show(OS0UIPanel::INVENTORY);
	}
	gLootVisible = FALSE;
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
		gDebugLibraryMode = DebugLibraryMode::ASSETS;
		if (gDebugAssetLibrarySector != gWorldSector.AsByte() ||
			gDebugAssetLibraryTileset != static_cast<INT16>(giCurrentTilesetID))
			RebuildDebugAssetLibrary();
		gUIRuntime.show(OS0UIPanel::ASSET_LIBRARY);
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
			BindInspectedSoldier(soldier);
			BindInventorySoldier(soldier);
			gInspectedGridNo = NOWHERE;
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
		InteractionMode().beginInteraction(target->bTeam == OUR_TEAM ?
			OS0InteractionSurface::EQUIPMENT : OS0InteractionSurface::ACTIONS);
		BindInspectedSoldier(target);
		gInspectedGridNo = NOWHERE;
		if (target->bTeam == OUR_TEAM) BindInventorySoldier(target);
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		// Let JA2 keep its normal allied selection mechanics. Contacts are
		// inspection-only and therefore consume the click.
		return target->bTeam != OUR_TEAM;
	}
	if (gridNo < 0 || gridNo >= WORLD_MAX) return FALSE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	CanonicalContainerTarget containerTarget;
	if (ResolveCanonicalContainerTarget(gridNo, level, tileIndex,
		containerTarget))
	{
		gridNo = containerTarget.gridNo;
		tileIndex = containerTarget.tileIndex;
	}
	RecordFeedbackEvent(ST::format("CONTEXT grid {} level {} tile {}{}",
		gridNo, level, tileIndex, target ? " merc" : ""));
	const BOOLEAN hasItems = GetItemPool(gridNo, level) != nullptr;
	const BOOLEAN hasAsset = WorldAssetExistsAt(gridNo, level, tileIndex);
	if (!hasItems && !hasAsset) return FALSE;
	if (!hasAsset) tileIndex = NO_TILE;
	InteractionMode().beginInteraction(OS0InteractionSurface::ENVIRONMENT);
	gContextTitle = hasAsset ? DescribeWorldAsset(gridNo, level, tileIndex).displayName :
		(hasItems ? "GROUND ITEMS" : "WORLD ASSET");
	const BOOLEAN sameWorldSelection =
		gInspectedSoldier == nullptr &&
		gInspectedGridNo == gridNo &&
		gInspectedLevel == level &&
		gInspectedTileIndex == tileIndex;

	BindInspectedSoldier(nullptr);
	gInspectedGridNo = gridNo;
	gInspectedLevel = level;
	gInspectedTileIndex = tileIndex;
	RefreshEnvironmentTarget(gridNo, level, tileIndex);
	gLootGridNo = gridNo;
	gLootLevel = level;
	gLootTileIndex = tileIndex;
	// Selection must not spawn a window underneath the first click. Otherwise
	// that new region swallows the second half of a double-click. Crucially, a
	// trailing LBUTTON_UP after a double-click is the *same* selection and must
	// not close the loot window that the double-click has just opened.
	if (!sameWorldSelection)
	{
		gLootVisible = FALSE;
	}
	CaptureInspectorPreview(gridNo, level);
	PositionBagRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}

void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY, INT32 worldItemIndex)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	GridNo identityGridNo = gridNo;
	UINT16 identityTileIndex = tileIndex;
	CanonicalContainerTarget containerTarget;
	if (!target && ResolveCanonicalContainerTarget(gridNo, level, tileIndex,
		containerTarget))
	{
		identityGridNo = containerTarget.gridNo;
		identityTileIndex = containerTarget.tileIndex;
	}
	const UINT16 heldItem = gpItemPointer ? gpItemPointer->usItem :
		static_cast<UINT16>(NOTHING);
	const UINT32 targetInstanceId = target ? target->uiUniqueSoldierIdValue : 0;
	const BOOLEAN bindingInvalidated =
		gHoverActionBinding.kind != OS0InteractionTargetKind::NONE &&
		!BindingStillValid(gHoverActionBinding);
	const BOOLEAN cursorContextChanged = bindingInvalidated ||
		target != gHoverCursorSoldier ||
		targetInstanceId != gHoverCursorSoldierInstanceId ||
		identityGridNo != gHoverCursorGridNo || level != gHoverCursorLevel ||
		identityTileIndex != gHoverCursorTileIndex ||
		worldItemIndex != gHoverCursorWorldItemIndex;
	const BOOLEAN heldItemChanged = heldItem != gHoverCursorHeldItem;
	const BOOLEAN wasHoverVisible = gHoverVisible;
	if (cursorContextChanged || heldItemChanged)
	{
		gHoverCursorSoldier = target;
		gHoverCursorSoldierInstanceId = targetInstanceId;
		gHoverCursorGridNo = identityGridNo;
		gHoverCursorLevel = level;
		gHoverCursorTileIndex = identityTileIndex;
		gHoverCursorWorldItemIndex = worldItemIndex;
		gHoverSuggestedAction = ContextAction::COUNT;
		gHoverActionBinding = BuildActionBinding(target, gridNo, level, tileIndex,
			worldItemIndex);
		gHoverActionCycleIndex = 0;
		gHoverActionExplicit = FALSE;
		if (!CarryState().active())
		{
			OS0ResolvedActionList const actions =
				ResolveInteractionAt(target, gridNo, level, tileIndex,
					worldItemIndex);
			if (OS0ResolvedAction const* const primary =
				PrimaryOS0InteractionAction(actions))
				gHoverSuggestedAction = primary->action;
		}
	}
	// This relation is evaluated every frame, not only when the world hover key
	// changes. Holding an item is deliberately not permission to open the actor's
	// character sheet or every equipment slot: only the policy-filtered relation
	// symbols are projected while the pointer is actually over an accessible actor.
	if (gpItemPointer && target && CanAccessSoldierContents(target) &&
		HeldItemRelationInReach(BoundHeldItemCarrier(), target))
	{
		const BOOLEAN transferTargetChanged =
			BoundItemTransferTarget() != target;
		BindItemTransferTarget(target);
		if (!gEquipmentExplodedVisible || gEquipmentSoldier != target)
			gEquipmentAutoForHeldItem = TRUE;
		BindEquipmentSoldier(target);
		gEquipmentExplodedVisible = TRUE;
		if (transferTargetChanged)
		{
			gItemTransferMoreVisible = FALSE;
			PositionItemTransferIntentRegions();
			SetBagRegionsEnabled(TRUE);
		}
	}
	else if (gpItemPointer && BoundItemTransferTarget() &&
		!PointerInsideItemTransferContext(BoundItemTransferTarget()))
	{
		ClearItemTransferTarget();
		gItemTransferMoreVisible = FALSE;
		if (gEquipmentAutoForHeldItem)
		{
			gEquipmentExplodedVisible = FALSE;
			BindEquipmentSoldier(nullptr);
			gEquipmentAutoForHeldItem = FALSE;
		}
		SetBagRegionsEnabled(TRUE);
	}
	const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
	const INT32 actionableWorldItemIndex = validGrid ?
		ActionableWorldItemIndexAt(gridNo, level, worldItemIndex) : -1;
	const BOOLEAN hasLooseItem = actionableWorldItemIndex >= 0;
	const BOOLEAN hasAsset = validGrid &&
		WorldAssetExistsAt(gridNo, level, tileIndex);
	const BOOLEAN hasTerrain = validGrid && level == 0 &&
		gpWorldLevelData[gridNo].pLandHead != nullptr;
	if (!target && worldItemIndex >= 0 && !hasLooseItem)
	{
		if (!gInspectorPinned) gHoverVisible = FALSE;
		gHoverActionBinding = {};
		gHoverCursorWorldItemIndex = -1;
		gHoverActionExplicit = FALSE;
		gHoverCursorHeldItem = heldItem;
		if (wasHoverVisible && !gHoverVisible) SetRenderFlags(RENDER_FLAG_FULL);
		return;
	}
	if (!target && !hasLooseItem && !hasAsset && !hasTerrain)
	{
		if (!gInspectorPinned) gHoverVisible = FALSE;
		gHoverActionBinding = {};
		gHoverCursorWorldItemIndex = -1;
		gHoverActionExplicit = FALSE;
		gHoverCursorHeldItem = heldItem;
		if (wasHoverVisible && !gHoverVisible) SetRenderFlags(RENDER_FLAG_FULL);
		return;
	}
	if (!cursorContextChanged && !heldItemChanged && gHoverVisible) return;
	gHoverCursorHeldItem = heldItem;
	if (cursorContextChanged && validGrid)
		CaptureInspectorPreview(gridNo, level);

	if (target)
	{
		const ContextAction displayAction =
			gHoverSuggestedAction != ContextAction::COUNT ?
			gHoverSuggestedAction : CursorState().action;
		gHoverDebugDetail.clear();
		gHoverTitle = target->name;
		gHoverDetail = gpItemPointer ?
			ST::format("{} -> {} / CHOOSE SLOT",
				GCM->getItem(gpItemPointer->usItem)->getName(), target->name) :
			ST::format("{}  HP {}/{}  {}",
				ContextActionName(displayAction), target->bLife, target->bLifeMax,
				target->bTeam == OUR_TEAM ? "OPERATOR" : "CONTACT");
	}
	else if (gHoverActionBinding.kind == OS0InteractionTargetKind::WORLD_ITEM &&
		gHoverActionBinding.worldItemIndex >= 0 &&
		static_cast<size_t>(gHoverActionBinding.worldItemIndex) < gWorldItems.size())
	{
		const ContextAction displayAction =
			gHoverSuggestedAction != ContextAction::COUNT ?
			gHoverSuggestedAction : CursorState().action;
		gHoverDebugDetail.clear();
		WORLDITEM const& worldItem = GetWorldItem(
			gHoverActionBinding.worldItemIndex);
		gHoverTitle = worldItem.o.usItem != NOTHING ?
			GCM->getItem(worldItem.o.usItem)->getName() : "GROUND ITEMS";
		gHoverDetail = ST::format("{}  F / CLICK ICON / MMB CYCLE",
			ContextActionName(displayAction));
	}
	else if (hasAsset)
	{
		SalvageProfile const salvage = DescribeWorldAsset(gridNo, level, tileIndex);
		AssetCatalogRecord const record = ResolveAssetRecord(gridNo, level, tileIndex);
		AssetMaterial const material = ResolveAssetMaterial(gridNo, level,
			tileIndex, record);
		FieldToolKind const required = RequiredFieldTool(gridNo, level, tileIndex);
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		GridNo const durabilityGrid = structure ? structure->sGridNo : gridNo;
		const UINT16 canonical = CanonicalAssetTileIndex(gridNo, level, tileIndex);
		const INT16 maximum = OS0AssetDurabilityMaximum(material);
		const INT16 current = OS0CurrentAssetDurability(durabilityGrid, level,
			canonical, material);
		gHoverTitle = salvage.displayName;
		gHoverDetail = ST::format("{} / {} / {}x{} / HP {}/{}",
			gAssetCategoryNames[static_cast<size_t>(record.category)],
			gAssetMaterialNames[static_cast<size_t>(material)],
			record.width, record.height, current, maximum);
		const ST::string yield = salvage.salvageable ?
			ST::format(" / +{} {}", salvage.amount,
				OS0ResourceName(salvage.resource)) : ST::string(" / FIXED");
		gHoverDebugDetail = ST::format("{} {}{}",
			FieldToolName(required), HasFieldTool(GetSelectedMan(), required) ?
				"READY" : "MISSING", yield);
	}
	else
	{
		gHoverTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		gHoverDetail = ST::format("{} / F ACTIONS / MMB CYCLE",
			gHoverSuggestedAction != ContextAction::COUNT ?
				ContextActionName(gHoverSuggestedAction) : "GROUND");
		gHoverDebugDetail = HasDiggingTool(GetSelectedMan()) ?
			"FIELD SHOVEL READY" : "FIELD SHOVEL MISSING";
	}

	// Content follows the hovered world object; the window does not. Keeping the
	// inspector at its user-chosen, persisted position prevents it from covering
	// the merc and eliminates the cursor-chasing jitter of the prototype.
	(void)screenX;
	(void)screenY;
	gHoverVisible = TRUE;
	// ENVIRONMENT is a live projection of the world relation under the pointer,
	// not a stale selection inspector. Keep the user-positioned window closed
	// when requested, but update it immediately whenever it is visible.
	if (!target && cursorContextChanged &&
		gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::ENVIRONMENT)))
		RefreshEnvironmentTarget(gridNo, level, tileIndex);
}

void OS0ClearWorldHover()
{
	if (gHoverVisible) SetRenderFlags(RENDER_FLAG_FULL);
	if (!gInspectorPinned) gHoverVisible = FALSE;
	gHoverCursorSoldier = nullptr;
	gHoverCursorSoldierInstanceId = 0;
	gHoverCursorGridNo = NOWHERE;
	gHoverCursorLevel = 0;
	gHoverCursorTileIndex = NO_TILE;
	gHoverCursorWorldItemIndex = -1;
	gHoverCursorHeldItem = NOTHING;
	gHoverSuggestedAction = ContextAction::COUNT;
	gHoverActionBinding = {};
	gHoverActionCycleIndex = 0;
	gHoverActionExplicit = FALSE;
}

BOOLEAN OS0ActivateHoveredInteraction(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex, INT16 screenX, INT16 screenY,
	INT32 worldItemIndex)
{
	if (!gInitialized || gTutorialActive) return FALSE;
	// The panel guard only suppresses the trailing mouse-up of a close/double
	// click.  F is a fresh keyboard command and must never be swallowed by it.
	gPanelInteractionGuardUntil = 0;

	if (target && !target->bActive) target = nullptr;
	if (target && (gridNo < 0 || gridNo >= WORLD_MAX))
	{
		gridNo = target->sGridNo;
		level = target->bLevel;
	}
	// A character owns its action hub. Do not turn a deliberate F press over a
	// merc into the global nearby scanner or open the inspector behind the fan.
	if (target)
	{
		OS0OpenContextMenu(target, gridNo, level, tileIndex, screenX, screenY,
			worldItemIndex);
		RecordFeedbackEvent(ST::format("F / CHARACTER HUB / {}", target->name));
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}
	const BOOLEAN hasWorldPoint = gridNo >= 0 && gridNo < WORLD_MAX;
	if (hasWorldPoint)
		tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	const BOOLEAN hasWorldRelation = hasWorldPoint &&
		(GetItemPool(gridNo, level) != nullptr ||
			WorldAssetExistsAt(gridNo, level, tileIndex) ||
			(level == 0 && gpWorldLevelData[gridNo].pLandHead != nullptr));
	if (!hasWorldRelation)
	{
		RecordFeedbackEvent("F / PERCEPTION / NO TARGET");
		return TRUE;
	}

	OS0OpenContextMenu(nullptr, gridNo, level, tileIndex, screenX, screenY,
		worldItemIndex);
	RecordFeedbackEvent(ST::format("F / PERCEIVE / GRID {}", gridNo));
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}


BOOLEAN OS0ActivateCurrentHoverInteraction(INT16 const screenX,
	INT16 const screenY)
{
	if (!gInitialized || !gHoverVisible ||
		gHoverActionBinding.kind == OS0InteractionTargetKind::NONE ||
		!BindingStillValid(gHoverActionBinding)) return FALSE;
	SOLDIERTYPE* target = nullptr;
	if (gHoverActionBinding.kind == OS0InteractionTargetKind::ACTOR)
		target = ID2Soldier(static_cast<UINT8>(gHoverActionBinding.actorId));
	return OS0ActivateHoveredInteraction(target, gHoverActionBinding.gridNo,
		gHoverActionBinding.level, gHoverActionBinding.tileIndex,
		screenX, screenY, gHoverActionBinding.worldItemIndex);
}

void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY, INT32 worldItemIndex)
{
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	// A context hub starts a new explicit interaction.  It cannot coexist with
	// an old route that continues moving a different bound carrier underneath it.
	if (CarryState().active()) CancelWorldMoveState();
	CloseContextMenu();
	// Context is an overlay over the current control intent. Opening or closing
	// it never lowers a ready weapon, cancels a tool, or changes movement mode.
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	gDeferredContextIdentity = target ? CharacterContextIdentity(target) :
		DeferredContextIdentity{};
	gContextSoldier = target;
	gContextGridNo = gridNo;
	gContextLevel = level;
	gContextTileIndex = tileIndex;
	gContextInventorySlot = NO_SLOT;

	SOLDIERTYPE* const selected = GetSelectedMan();
	const INT32 actionableWorldItemIndex =
		gridNo >= 0 && gridNo < WORLD_MAX ?
			ActionableWorldItemIndexAt(gridNo, level, worldItemIndex) : -1;
	const BOOLEAN hasItems = actionableWorldItemIndex >= 0;
	const BOOLEAN hasAsset = WorldAssetExistsAt(gridNo, level, tileIndex);
	const BOOLEAN hasTerrain = gridNo >= 0 && gridNo < WORLD_MAX && level == 0 &&
		gpWorldLevelData[gridNo].pLandHead != nullptr;
	if (!target && worldItemIndex >= 0 && !hasItems) return;

	if (target)
	{
		// Build the one radial interaction state directly. Opening the character
		// sheet first created a second, smaller menu for one frame underneath it.
		// Do not mutate inventory/loot/inspector ownership here.  Those persistent
		// surfaces are only suspended while the hub is open and must return in the
		// exact state in which the player left them.
		gCharacterActionFanVisible = TRUE;
		// A context fan is transient and must not destroy the independent RPG
		// inventory window. Its real slots remain valid drag targets behind/after it.
		gContextSoldier = target;
		gContextTitle = target->name;
		const BOOLEAN own = target->bTeam == OUR_TEAM;
		if (own)
		{
			BuildCharacterContextPage(target);
		}
		else
		{
			for (OS0ResolvedAction const& resolved :
				ResolveInteractionAt(target, gridNo, level, tileIndex))
			{
				if (resolved.action == ContextAction::MOVE) continue;
				AddContextEntry(resolved.action,
					ContextActionName(resolved.action), resolved.enabled,
					resolved.binding, resolved.approach,
					resolved.blockReason);
			}
			if (CanAccessSoldierContents(target))
				AddContextEntry(ContextAction::CONTENTS, "LOOT / CONTENTS");
		}
	}
	else if (hasItems || hasAsset)
	{
		gObjectActionFanVisible = TRUE;
		gContextSoldier = nullptr;
		gContextGridNo = gridNo;
		gContextLevel = level;
		gContextTileIndex = tileIndex;
		gContextWorldItemIndex = actionableWorldItemIndex;
		if (hasItems)
			gDeferredContextIdentity = WorldItemContextIdentity(selected,
				gContextWorldItemIndex, tileIndex);
		gContextTitle = worldItemIndex >= 0 && hasItems &&
			static_cast<size_t>(actionableWorldItemIndex) < gWorldItems.size() ?
			GCM->getItem(GetWorldItem(actionableWorldItemIndex).o.usItem)->getName() :
			(hasAsset ? DescribeWorldAsset(gridNo, level, tileIndex).displayName :
				(hasItems ? "GROUND ITEMS" : "WORLD ASSET"));
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			level, tileIndex, selected);
		for (OS0ResolvedAction const& resolved :
			ResolveInteractionAt(nullptr, gridNo, level, tileIndex,
				actionableWorldItemIndex))
		{
			if (resolved.action == ContextAction::MOVE) continue;
			AddContextEntry(resolved.action,
				EnvironmentActionLabel(resolved.action, facts, gridNo, level,
					tileIndex), resolved.enabled, resolved.binding,
				resolved.approach, resolved.blockReason);
		}
		if (hasItems)
		{
			if (selected && gContextWorldItemIndex >= 0 &&
				static_cast<size_t>(gContextWorldItemIndex) < gWorldItems.size())
			{
				WORLDITEM const& worldItem = GetWorldItem(gContextWorldItemIndex);
				if (worldItem.fExists && worldItem.o.usItem != NOTHING &&
					OS0PreferredEquipmentSlot(selected, worldItem.o) != NO_SLOT)
					AddContextEntry(ContextAction::EQUIP_ITEM,
						"EQUIP DIRECTLY", facts.near &&
						OS0CanAcceptCarriedObject(selected, worldItem.o));
			}
		}
	}
	else if (hasTerrain)
	{
		gObjectActionFanVisible = TRUE;
		gContextSoldier = nullptr;
		gContextGridNo = gridNo;
		gContextLevel = 0;
		gContextTileIndex = NO_TILE;
		gContextWorldItemIndex = -1;
		gContextTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		CaptureInspectorPreview(gridNo, 0);
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			0, NO_TILE, selected);
		for (OS0ResolvedAction const& resolved :
			ResolveInteractionAt(nullptr, gridNo, 0, NO_TILE))
		{
			if (resolved.action == ContextAction::MOVE) continue;
			AddContextEntry(resolved.action,
				EnvironmentActionLabel(resolved.action, facts, gridNo, 0, NO_TILE),
				resolved.enabled, resolved.binding, resolved.approach,
				resolved.blockReason);
		}
	}
	if (gContextEntryCount == 0) return;
	// CloseContextMenu clears stale hover state. Rebuild it explicitly from the
	// same resolved relation so RMB always opens the radial *and* its preview,
	// even when the mouse did not move between button-down and button-up.
	if (!target && (hasItems || hasAsset))
	{
		OS0HoverWorldObject(nullptr, gridNo, level, tileIndex, screenX, screenY,
			actionableWorldItemIndex);
	}
	else if (hasTerrain)
	{
		CaptureInspectorPreview(gridNo, level);
		gHoverTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		gHoverDetail = "GROUND / F OR RMB ACTIONS / MMB CYCLE";
		gHoverDebugDetail = HasDiggingTool(selected) ?
			"FIELD SHOVEL READY" : "FIELD SHOVEL MISSING";
		gHoverVisible = TRUE;
	}
	if (!target)
		gUIRuntime.windowManager().show(
			gUIRuntime.managedId(FloatingPanelId::INSPECTOR));

	if (!gObjectActionFanVisible)
	{
		const INT16 width = 168;
		const INT16 height = static_cast<INT16>(20 + gContextEntryCount * 18);
		gContextX = std::clamp<INT16>(screenX, 0,
			std::max<INT16>(0, gsVIEWPORT_END_X - width));
		gContextY = std::clamp<INT16>(screenY, 0,
			std::max<INT16>(0, gsVIEWPORT_END_Y - height));
	}
	gUIRuntime.show(OS0UIPanel::CONTEXT);
	if (!target && FieldTutorialTargetMatches(gridNo, level, tileIndex))
		NotifyFieldTutorial(OS0FieldTutorialEvent::ACTIONS_OPENED);
	// A radial is the sole transient interaction owner. Persistent windows keep
	// their requested visibility and exact positions, but cannot paint through
	// or steal input until the radial closes.
	SetContextHubModal(TRUE);
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	OS0ResolvedActionList const resolved =
		ResolveInteractionAt(target, gridNo, level, tileIndex, worldItemIndex);
	std::array<ContextAction, 12> available{};
	size_t count = 0;
	for (OS0ResolvedAction const& entry : resolved)
	{
		if (!entry.enabled || entry.action == ContextAction::MOVE ||
			count >= available.size()) continue;
		available[count++] = entry.action;
	}
	if (count == 0) return;

	OS0ActionBinding const binding =
		BuildActionBinding(target, gridNo, level, tileIndex, worldItemIndex);
	size_t current = count;
	if (binding == gHoverActionBinding)
	{
		for (size_t i = 0; i < count; ++i)
			if (available[i] == gHoverSuggestedAction)
			{
				current = i;
				break;
			}
	}
	gHoverActionCycleIndex = current == count ? 0 : (current + 1) % count;
	gHoverSuggestedAction = available[gHoverActionCycleIndex];
	gHoverActionBinding = binding;
	gHoverActionExplicit = TRUE;
	RecordFeedbackEvent(ST::format("RELATION {} / LMB EXECUTE",
		ContextActionName(gHoverSuggestedAction)));
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0CancelCursorAction()
{
	if (gpItemPointer)
	{
		CancelItemPointer();
		OS0NotifyWorldMutation();
		ClearItemTransferTarget();
		if (!gpItemPointer) ClearHeldItemCarrier();
		gItemTransferMoreVisible = FALSE;
		RecordFeedbackEvent(gpItemPointer ?
			"RETURN BLOCKED / ITEM KEPT ON CURSOR" :
			"HELD ITEM RETURNED TO SOURCE");
	}
	CancelPendingWorldAction("CANCELLED");
	CursorState().action = ContextAction::MOVE;
	if (InteractionMode().nearbyScanEnabled()) InteractionMode().returnToNormal();
	else InteractionMode().returnToNormal(OS0InteractionSurface::ACTIONS);
	CancelWorldMoveState();
	gHoverActionExplicit = FALSE;
	gHoverActionCycleIndex = 0;
	OS0ResetDirectControl();
	gNextCombatProjectionAt = 0;
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}

namespace
{
	BOOLEAN CancelTopOS0Interaction()
	{
		OS0WindowManager& windows = gUIRuntime.windowManager();
		OS0CancellationFacts const facts{
			gContextVisible || gStackSplitVisible || gAssetCatalogVisible ||
				windows.draggingWindow() != OS0_INVALID_WINDOW ||
				gMultiToolDragging,
			gpItemPointer != nullptr,
			CarryState().active() != FALSE,
			gPendingWorldAction.active() != FALSE,
			CursorState().action != ContextAction::MOVE ||
				CombatModeActive()
		};
		switch (OS0SelectCancellationLayer(facts))
		{
			case OS0CancellationLayer::MODAL:
				if (gContextVisible) CloseContextMenu();
				else if (gStackSplitVisible) CloseStackSplit();
				else if (gAssetCatalogVisible)
				{
					gUIRuntime.hide(OS0UIPanel::ASSET_CATALOG);
					gAssetCatalogNameEditing = FALSE;
					SetUIKeyboardHook(nullptr);
					gGodLibraryVisible = gAssetCatalogReturnToLibrary;
					gAssetCatalogReturnToLibrary = FALSE;
				}
				else
				{
					windows.cancelDrag();
					gMultiToolDragCandidate = FALSE;
					gMultiToolDragging = FALSE;
					SaveUILayout();
				}
				break;
			case OS0CancellationLayer::HELD_ITEM:
				CancelItemPointer();
				OS0NotifyWorldMutation();
				ClearItemTransferTarget();
				if (!gpItemPointer) ClearHeldItemCarrier();
				gItemTransferMoreVisible = FALSE;
				RecordFeedbackEvent(gpItemPointer ?
					"RETURN BLOCKED / ITEM KEPT ON CURSOR" :
					"HELD ITEM RETURNED TO SOURCE");
				break;
			case OS0CancellationLayer::WORLD_MANIPULATION:
				CancelWorldMoveState();
				CursorState().action = ContextAction::MOVE;
				if (InteractionMode().nearbyScanEnabled())
					InteractionMode().returnToNormal();
				else InteractionMode().returnToNormal(
					OS0InteractionSurface::ACTIONS);
				OS0ResetDirectControl();
				gNextCombatProjectionAt = 0;
				RecordFeedbackEvent("GRAB / RELEASED");
				break;
			case OS0CancellationLayer::APPROACH:
				CancelPendingWorldAction("PLAYER CANCELLED");
				OS0ResetDirectControl();
				break;
			case OS0CancellationLayer::CURSOR_ACTION:
				OS0CancelCursorAction();
				return TRUE;
			case OS0CancellationLayer::NONE:
				return FALSE;
		}
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	BOOLEAN HandleGrabControlKey(UINT32 key, UINT16 eventType)
	{
		OS0CarryState& carry = CarryState();
		if (!carry.active()) return FALSE;
		// A held movement key is owned by GRAB even while the previous physical
		// step is completing. It must never leak into a legacy JA2 shortcut.
		if (eventType != KEY_DOWN && eventType != KEY_REPEAT) return TRUE;
		SOLDIERTYPE* const actor = CarryCarrier();
		if (!actor || actor != GetSelectedMan() || !actor->bActive ||
			actor->bTeam != OUR_TEAM || !OK_CONTROLLABLE_MERC(actor) ||
			actor->bLife < OKLIFE || gpItemPointer ||
			(gTacticalStatus.uiFlags & INCOMBAT))
		{
			RecordFeedbackEvent("CARRY CANCELLED / CONTROL OWNER CHANGED");
			CancelWorldMoveState();
			CursorState().action = ContextAction::MOVE;
			return TRUE;
		}
		if (gContextVisible || gStackSplitVisible || gAssetCatalogVisible ||
			gFeedbackEditing || AreWeInAUIMenu() || InItemDescriptionBox() ||
			InItemStackPopup() || InKeyRingPopup() ||
			(gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
			OS0GetRealtimeEditorUI().active() ||
			gTacticalStatus.fAutoBandageMode ||
			gUIRuntime.windowManager().draggingWindow() != OS0_INVALID_WINDOW)
			return TRUE;
		// The structure has already moved and this short path belongs exclusively
		// to the push follow-up.  Never reinterpret a paused/repeated key as a new
		// grab step until the carrier reaches the vacated source tile.
		if (carry.repositioning()) return TRUE;
		if ((gAnimControl[actor->usAnimState].uiFlags & ANIM_MOVING) ||
			actor->fTurningUntilDone ||
			actor->usPendingAnimation != NO_PENDING_ANIMATION)
			return TRUE;
		const BOOLEAN turnLeft = key == SDLK_q || key == 'Q';
		const BOOLEAN turnRight = key == SDLK_e || key == 'E';
		if (turnLeft || turnRight)
		{
			if (!carry.walking())
			{
				const UINT8 facing = actor->bDirection < NUM_WORLD_DIRECTIONS ?
					actor->bDirection : static_cast<UINT8>(NORTH);
				SendSoldierSetDesiredDirectionEvent(actor,
					turnLeft ? OneCCDirection(facing) : OneCDirection(facing));
			}
			return TRUE;
		}
		if (carry.walking()) return TRUE;

		UINT8 direction = actor->bDirection < NUM_WORLD_DIRECTIONS ?
			actor->bDirection : static_cast<UINT8>(NORTH);
		if (key == SDLK_s || key == 'S') direction = OppositeDirection(direction);
		else if (key == SDLK_a || key == 'A')
			direction = OneCCDirection(OneCCDirection(direction));
		else if (key == SDLK_d || key == 'D')
			direction = OneCDirection(OneCDirection(direction));
		else if (key != SDLK_w && key != 'W') return TRUE;

		const GridNo actorDestination = NewGridNo(actor->sGridNo,
			DirectionInc(direction));
		const BOOLEAN pushingSource = actorDestination == carry.source;
		// The grabbed structure still occupies its source tile during preflight.
		// That tile is intentionally blocked for an ordinary walk, but it becomes
		// the actor's destination only after the push transaction vacates it.
		if (actorDestination == actor->sGridNo ||
			(!pushingSource &&
			 !NewOKDestination(actor, actorDestination, TRUE, actor->bLevel)))
			return TRUE;

		GridNo objectDestination = NOWHERE;
		if (pushingSource)
		{
			// Walking into the grabbed object pushes it one cell ahead; the actor
			// then occupies its old cell.
			carry.mode = OS0CarryMode::PUSH;
			objectDestination = NewGridNo(carry.source, DirectionInc(direction));
		}
		else if (PythSpacesAway(actorDestination, carry.source) >
			PythSpacesAway(actor->sGridNo, carry.source))
		{
			// Walking away keeps the hands attached and pulls the object into the
			// cell the actor just vacated.
			carry.mode = OS0CarryMode::PULL;
			objectDestination = actor->sGridNo;
		}
		else
		{
			// A lateral movement preserves the actor/object offset. Light objects
			// read as carried; heavy ones remain a continuous side drag.
			carry.mode = OS0CarryMode::CARRY;
			objectDestination = NewGridNo(carry.source, DirectionInc(direction));
		}
		if (objectDestination == carry.source || objectDestination < 0 ||
			objectDestination >= WORLD_MAX) return TRUE;
		OS0HandlePendingWorldMove(objectDestination);
		return TRUE;
	}
}

BOOLEAN OS0HandleRealtimeControlKey(UINT32 key, UINT32 keyState,
	UINT16 eventType)
{
	if (key == SDLK_ESCAPE)
	{
		if (eventType == KEY_DOWN)
		{
			gEscapeKeyOwned = CancelTopOS0Interaction();
			return gEscapeKeyOwned;
		}
		if (eventType == KEY_REPEAT) return gEscapeKeyOwned;
		if (eventType == KEY_UP)
		{
			const BOOLEAN owned = gEscapeKeyOwned;
			gEscapeKeyOwned = FALSE;
			return owned;
		}
		return FALSE;
	}
	if (!OS0IsDirectControlKey(key)) return FALSE;
	// Alt/Ctrl combinations remain engine shortcuts. Shift is intentionally part
	// of direct control and promotes a standing movement segment to RUNNING. Its
	// own key-down is consumed as a control state, not routed as a legacy modifier.
	if (keyState & (ALT_DOWN | CTRL_DOWN)) return FALSE;
	if (CarryState().active()) return HandleGrabControlKey(key, eventType);
	if (eventType == KEY_DOWN && gPendingWorldAction.active())
	{
		CancelPendingWorldAction("PLAYER OVERRIDE");
	}
	const BOOLEAN enabled = !DirectControlBlocked() && GetSelectedMan();
	if (gTacticalStatus.uiFlags & INCOMBAT)
	{
		OS0HandleTurnBasedDirectControlKey(GetSelectedMan(), key, eventType,
			enabled);
	}
	// Realtime movement is sampled exactly once by UpdateOS0IngameUI. Calling
	// it again from key events used key-repeat frequency as a second movement
	// clock and produced the visible step/jump behaviour.
	// OS0 owns these keys for the whole tactical session. A blocked context/modal
	// pauses movement but must not leak A/S/D/E back into unrelated JA2 shortcuts.
	return TRUE;
}

BOOLEAN OS0HandleHeldItemAction(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex)
{
	if (!gpItemPointer) return FALSE;
	if (gHeldItemCarrierInstanceId == 0 && gpItemPointerSoldier &&
		gpItemPointerSoldier->bActive &&
		gpItemPointerSoldier->sSector == gWorldSector)
		BindHeldItemCarrier(gpItemPointerSoldier);
	SOLDIERTYPE* const actor = BoundHeldItemCarrier();
	if (!actor)
	{
		return RejectHeldItemRelation(nullptr, "CARRIER CHANGED");
	}
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);

	// The actor is a relation target, not an implicit inventory bucket. Clicking
	// keeps the item held and exposes only policy-approved relations; it never
	// opens the character sheet and the exploded equipment UI as side effects.
	if (target && CanAccessSoldierContents(target))
	{
		if (!HeldItemRelationInReach(actor, target))
			return RejectHeldItemRelation(actor, "ACTOR OUT OF REACH");
		if (BoundItemTransferTarget() != target) gItemTransferMoreVisible = FALSE;
		BindItemTransferTarget(target);
		ItemTransferPolicyDecision const decision =
			CurrentItemTransferDecision(target);
		if (decision.safeToApplyAutomatically && decision.hasPreferred)
		{
			RecordFeedbackEvent(ST::format("ITEM TARGET {} / AUTO {}",
				target->name, ItemTransferIntentLabel(target, decision.preferred)));
			ApplyItemTransferIntent(target, decision.preferred);
			return TRUE;
		}
		PositionItemTransferIntentRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		RecordFeedbackEvent(ST::format("ITEM TARGET {} / CHOOSE RELATION",
			target->name));
		return TRUE;
	}

	if (gridNo < 0 || gridNo >= WORLD_MAX)
		return RejectHeldItemRelation(actor, "INVALID WORLD TARGET");
	const BOOLEAN near = actor->bLevel == level &&
		PythSpacesAway(actor->sGridNo, gridNo) <= 2;
	const BOOLEAN hasAsset = WorldAssetExistsAt(gridNo, level, tileIndex);
	if (hasAsset)
	{
		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		// An openable container is a safe relation target before it is material.
		// A crowbar/tool held over a crate must never dismantle the crate merely
		// because the same item could also be used as a salvage tool.
		if (structure && structure->fFlags & STRUCTURE_OPENABLE &&
			!(structure->fFlags & STRUCTURE_ANYDOOR))
		{
			if (!near)
			{
				return RejectHeldItemRelation(actor,
					"CONTAINER OUT OF REACH");
			}
			CanonicalContainerTarget containerTarget;
			if (!ResolveCanonicalContainerTarget(gridNo, level, tileIndex,
				containerTarget))
			{
				RecordFeedbackEvent(
					"CONTAINER INSERT FAILED / TARGET CHANGED");
				return TRUE;
			}
			const Visibility visibility =
				(containerTarget.structure->fFlags & STRUCTURE_OPEN) ?
				VISIBLE : HIDDEN_IN_OBJECT;
			if (AddItemToPool(containerTarget.gridNo, gpItemPointer, visibility,
				level, 0, -1) < 0)
			{
				RecordFeedbackEvent(
					"CONTAINER INSERT FAILED / ITEM KEPT ON CURSOR");
				return TRUE;
			}
			OS0NotifyWorldMutation();
			FinishCommittedItemPointer();
			ClearItemTransferTarget();
			gItemTransferMoreVisible = FALSE;
			OS0OpenWorldContainer(containerTarget.gridNo, level,
				containerTarget.tileIndex, actor);
			return TRUE;
		}

		const FieldToolKind required = RequiredFieldTool(gridNo, level, tileIndex);
		const BOOLEAN matchingTool = required != FieldToolKind::NONE &&
			gpItemPointer->usItem == FieldToolItem(required);
		if (matchingTool)
		{
			if (!near)
			{
				return RejectHeldItemRelation(actor,
					"TOOL TARGET OUT OF REACH");
			}
			const BOOLEAN applied = SalvageWorldAsset(actor, gridNo, level, tileIndex) ||
				(required == FieldToolKind::FIELD_SHOVEL &&
				 DigTerrainAt(actor, gridNo, tileIndex));
			if (applied)
			{
				RecordFeedbackEvent(ST::format("HELD {} APPLIED AT {}",
					GCM->getItem(gpItemPointer->usItem)->getName(), gridNo));
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
				return TRUE; // Keep the tool on the cursor for the next use.
			}
			RecordFeedbackEvent("HELD TOOL / APPLICATION FAILED / ITEM KEPT");
			return TRUE;
		}
	}

	// Bare soil accepts the field shovel even when no object-layer asset exists.
	if (gpItemPointer->usItem == CROWBAR && !near)
	{
		return RejectHeldItemRelation(actor,
			"FIELD TOOL TARGET OUT OF REACH");
	}
	if (gpItemPointer->usItem == CROWBAR && DigTerrainAt(actor, gridNo, tileIndex))
	{
		RecordFeedbackEvent(ST::format("HELD FIELD SHOVEL APPLIED AT {}", gridNo));
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}

	// Empty ground is a real relation too. Dropping is explicit and never falls
	// through to a swallowed vanilla release after OS//0 claimed the gesture.
	if (!near)
		return RejectHeldItemRelation(actor, "WORLD DROP OUT OF REACH");
	if (AddItemToPool(gridNo, gpItemPointer, VISIBLE, level, 0, -1) < 0)
	{
		RecordFeedbackEvent("WORLD DROP FAILED / ITEM KEPT ON CURSOR");
		return TRUE;
	}
	OS0NotifyWorldMutation();
	FinishCommittedItemPointer();
	ClearItemTransferTarget();
	gItemTransferMoreVisible = FALSE;
	RecordFeedbackEvent(ST::format("ITEM DROPPED / GRID {}", gridNo));
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}

BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex)
{
	// A panel opened by a double-click can receive a trailing button-up from the
	// same physical gesture. Consume it before vanilla UI code sees it.
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return TRUE;
	if (OS0GetRealtimeEditorUI().active())
	{
		tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
		return OS0GetRealtimeEditorUI().handleWorldClick(target, gridNo,
			level, tileIndex);
	}
	if (gpItemPointer && OS0HandleHeldItemAction(target, gridNo, level, tileIndex))
		return TRUE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	if (gHoverActionExplicit)
	{
		if (worldItemIndex < 0 &&
			gHoverActionBinding.kind == OS0InteractionTargetKind::WORLD_ITEM &&
			gHoverActionBinding.gridNo == gridNo &&
			gHoverActionBinding.level == level)
			worldItemIndex = gHoverActionBinding.worldItemIndex;
		OS0ActionBinding const currentBinding =
			BuildActionBinding(target, gridNo, level, tileIndex, worldItemIndex);
		const ContextAction selectedAction = gHoverSuggestedAction;
		gHoverActionExplicit = FALSE;
		if (currentBinding == gHoverActionBinding &&
			selectedAction != ContextAction::COUNT)
		{
			OS0ResolvedAction resolved;
			if (ResolveBoundAction(currentBinding, selectedAction, resolved))
			{
				if (currentBinding.kind == OS0InteractionTargetKind::ACTOR)
				{
					switch (selectedAction)
					{
						case ContextAction::USE:
						case ContextAction::INSPECT:
						case ContextAction::CONTENTS:
							OS0OpenCharacterPanel(target);
							return TRUE;
						case ContextAction::ATTACK:
							ApplyCursorTool(ContextAction::ATTACK);
							return TRUE;
						case ContextAction::TALK:
							ApplyCursorTool(ContextAction::TALK);
							return TRUE;
						default:
							OS0OpenContextMenu(target, gridNo, level, tileIndex,
								gusMouseXPos, gusMouseYPos);
							return TRUE;
					}
				}
				if (currentBinding.kind != OS0InteractionTargetKind::ACTOR)
				{
					if (!ExecuteOrQueueBoundAction(resolved))
						RecordFeedbackEvent(ST::format("{} / ACTION FAILED",
							ContextActionName(selectedAction)));
					return TRUE;
				}
			}
			RecordFeedbackEvent("RELATION CHANGED / ACTION CANCELLED");
			return TRUE;
		}
	}
	if (CursorState().action == ContextAction::MOVE)
	{
		// Deliberately yield to JA2's mature click-path owner. Hover no longer
		// replaces MOVE unless the explicit nearby-scan mode is active, so LMB can
		// once again select a full distant path instead of a one-tile OS0 action.
		InteractionMode().returnToNormal();
		return FALSE;
	}
	if (CursorState().action == ContextAction::ATTACK)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!selected || !selected->bActive ||
			selected->bTeam != OUR_TEAM || !OK_CONTROLLABLE_MERC(selected))
			return TRUE;
		const BOOLEAN engineBusy =
			gTacticalStatus.ubAttackBusyCount > 0 ||
			gfDisableRegionActive || gfUserTurnRegionActive ||
			(gAnimControl[selected->usAnimState].uiFlags &
				(ANIM_MOVING | ANIM_FIRE | ANIM_SPECIALMOVE)) ||
			selected->fInNonintAnim || selected->fRTInNonintAnim ||
			selected->ubPendingAction != NO_PENDING_ACTION ||
			selected->fTurningUntilDone ||
			selected->usPendingAnimation != NO_PENDING_ANIMATION ||
			selected->ubPendingStanceChange != NO_PENDING_STANCE ||
			((gTacticalStatus.uiFlags & INCOMBAT) &&
				gTacticalStatus.ubCurrentTeam != OUR_TEAM) ||
			gCurrentUIMode == LOCKUI_MODE ||
			gCurrentUIMode == LOCKOURTURN_UI_MODE ||
			gCurrentUIMode == ENEMYS_TURN_MODE;
		if (engineBusy || guiPendingOverrideEvent != I_DO_NOTHING) return TRUE;
		if (!UIMouseOnValidAttackLocation(selected)) return TRUE;
		if (HandleUIReloading(selected)) return TRUE;
		if (gCurrentUIMode == CONFIRM_ACTION_MODE)
		{
			// Complex attacks keep JA2's deliberate confirm step. Because OS//0 owns
			// the whole primary gesture, the second release must complete it here
			// instead of being swallowed and re-queuing confirm forever.
			if (!SelectedMercCanAffordAttack()) return TRUE;
			if (selected->bDoBurst) selected->fDoSpread = FALSE;
			guiPendingOverrideEvent = CA_MERC_SHOOT;
			return TRUE;
		}
		const ItemCursor itemCursor = GetActionModeCursor(selected);
		// Every new primary attack starts unrefined. Affordability must be
		// calculated from that state, not from aim time left by a previous shot.
		selected->bShownAimTime = 0;
		if (!SelectedMercCanAffordAttack()) return TRUE;
		selected->sStartGridNo = guiCurrentCursorGridNo;
		if (itemCursor == TARGETCURS && !selected->bDoBurst)
		{
			// A plain firearm owns one complete primary gesture: reset every piece
			// of the old confirm/spread state before dispatching exactly one native
			// shot. AP, LOS, friendly-fire and weapon behavior remain native.
			ResetBurstLocations();
			guiPendingOverrideEvent = CA_MERC_SHOOT;
		}
		else
		{
			// Burst spread, throws and trajectories require JA2's confirm preflight.
			// They still begin on this click, but never reuse stale spread geometry.
			ResetBurstLocations();
			guiPendingOverrideEvent = A_CHANGE_TO_CONFIM_ACTION;
		}
		return TRUE;
	}
	if (CursorState().action == ContextAction::USE)
	{
		if (target)
		{
			OS0OpenCharacterPanel(target);
			return TRUE;
		}
		if (gridNo >= 0 && gridNo < WORLD_MAX &&
			(GetItemPool(gridNo, level) ||
				WorldAssetExistsAt(gridNo, level, tileIndex)))
		{
			OS0ActivateWorldObject(gridNo, level, tileIndex, worldItemIndex);
			return TRUE;
		}
		return FALSE;
	}
	if (CursorState().action == ContextAction::INSPECT)
	{
		const BOOLEAN hasInspectable = target ||
			(gridNo >= 0 && gridNo < WORLD_MAX &&
				(GetItemPool(gridNo, level) ||
					WorldAssetExistsAt(gridNo, level, tileIndex)));
		OS0SelectWorldObject(target, gridNo, level, tileIndex);
		return hasInspectable;
	}
	if (CursorState().action == ContextAction::DIG)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (selected && DigTerrainAt(selected, gridNo, tileIndex))
		{
			RefreshEnvironmentTarget(gridNo, 0, NO_TILE);
			RecordFeedbackEvent(ST::format("DIG / GRID {}", gridNo));
		}
		return TRUE;
	}
	if (CursorState().action == ContextAction::SALVAGE)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (selected && SalvageWorldAsset(selected, gridNo, level, tileIndex))
		{
			RefreshEnvironmentTarget(gridNo, level, NO_TILE);
			RecordFeedbackEvent(ST::format("DISMANTLE / GRID {}", gridNo));
		}
		return TRUE;
	}
	if (!OS0IsManipulationAction(CursorState().action)) return FALSE;
	if (gridNo < 0 || gridNo >= WORLD_MAX || tileIndex >= NUMBEROFTILES)
		return TRUE;

	SOLDIERTYPE* const selected = GetSelectedMan();
	BeginWorldMoveAt(gridNo, level,
		CanonicalAssetTileIndex(gridNo, level, tileIndex),
		CarryModeForAction(CursorState().action), selected);
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}


void OS0OpenWorldContainer(GridNo gridNo, UINT8 level, UINT16 tileIndex,
	SOLDIERTYPE* actor)
{
	if (gridNo < 0 || gridNo >= WORLD_MAX) return;
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	CanonicalContainerTarget containerTarget;
	if (!ResolveCanonicalContainerTarget(gridNo, level, tileIndex,
		containerTarget))
	{
		// Loose ground items are already physical world objects. They never open
		// the container projection; double-click picks the exact sprite up.
		return;
	}
	if (!actor) actor = GetSelectedMan();
	if (!actor || !actor->bActive) return;
	// Range is measured against the clicked footprint tile. A large container's
	// canonical base can be several grids away even while the merc is touching
	// the visible child that initiated this interaction.
	if (actor && PythSpacesAway(actor->sGridNo, gridNo) > 2)
	{
		OS0ResolvedActionList const actions =
			ResolveInteractionAtForActor(actor, nullptr, gridNo, level, tileIndex);
		if (OS0ResolvedAction const* const contents =
			FindOS0ResolvedAction(actions, ContextAction::CONTENTS))
			QueueApproachForAction(*contents, actor);
		return;
	}
	gridNo = containerTarget.gridNo;
	tileIndex = containerTarget.tileIndex;
	BindLootActor(actor);
	InteractionMode().beginInteraction(OS0InteractionSurface::ENVIRONMENT);
	CloseContextMenu();
	gContextTitle = WorldAssetExistsAt(gridNo, level, tileIndex) ?
		DescribeWorldAsset(gridNo, level, tileIndex).displayName : "GROUND ITEMS";
	EnsureContainerLoot(gridNo, level, tileIndex);
	gLootGridNo = gridNo;
	gLootLevel = level;
	gLootTileIndex = tileIndex;
	BindInspectedSoldier(nullptr);
	gInspectedGridNo = gridNo;
	gInspectedLevel = level;
	gInspectedTileIndex = tileIndex;
	const BOOLEAN hasContents = GetItemPool(gridNo, level) != nullptr;
	gLootVisible = hasContents && IsInspectedWorldAssetNear();
	// A container owns only its own spatial loot projection. Opening it must not
	// mutate character-window visibility or fan every body slot around the merc.
	// Transfer context starts only after an item is held over a valid target.
	ClearItemTransferTarget();
	gItemTransferMoreVisible = FALSE;
	gEquipmentExplodedVisible = FALSE;
	gEquipmentAutoForHeldItem = FALSE;
	gPanelInteractionGuardUntil = GetJA2Clock() + 140;
	if (gLootVisible) gLootIgnoreInputUntil = GetJA2Clock() + 300;
	if (FieldTutorialTargetMatches(gridNo, level, tileIndex))
	{
		gFieldTutorialInitialLootCount = CountFieldTutorialLoot();
		NotifyFieldTutorial(OS0FieldTutorialEvent::CONTENTS_OPENED);
	}

	CaptureInspectorPreview(gridNo, level);
	RefreshLootWorldItems();
	PositionBagRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0ActivateWorldObject(GridNo gridNo, UINT8 level, UINT16 tileIndex,
	INT32 worldItemIndex)
{
	if (gridNo < 0 || gridNo >= WORLD_MAX) return;
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
	const BOOLEAN openable = structure &&
		(structure->fFlags & STRUCTURE_OPENABLE) &&
		!(structure->fFlags & STRUCTURE_ANYDOOR);
	const INT32 actionableWorldItemIndex =
		ActionableWorldItemIndexAt(gridNo, level, worldItemIndex);

	// A loose world item has a simple default: approach and pick it up. A
	// container keeps its spatial contents window because it may hold many
	// independently positioned objects.
	if (actionableWorldItemIndex >= 0 && (!openable || worldItemIndex >= 0))
	{
		WORLDITEM const& worldItem = GetWorldItem(actionableWorldItemIndex);
		if (SOLDIERTYPE* const selected = GetSelectedMan())
		{
			BOOLEAN accepted = OS0CanAcceptCarriedObject(selected, worldItem.o);
			if (accepted && OS0CanPackObject(selected, worldItem.o))
			{
				if (!OS0SoldierPickupExactWorldItem(selected,
					actionableWorldItemIndex, gridNo, ITEM_IGNORE_Z_LEVEL))
				{
					accepted = FALSE;
					RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
				}
			}
			else if (accepted && BeginTrackedWorldItemTransfer(selected,
				actionableWorldItemIndex, NO_TILE, FALSE))
				RecordFeedbackEvent("PACK FULL / ITEM HELD FOR PLACEMENT");
			else if (!accepted)
				RecordFeedbackEvent("LOAD LIMIT 125% / PICKUP REJECTED");
			else
				RecordFeedbackEvent("PICKUP FAILED / ITEM CHANGED");
			if (accepted)
			{
				gInspectedGridNo = NOWHERE;
				gInspectedTileIndex = NO_TILE;
				gLootVisible = FALSE;
			}
		}
		return;
	}
	if (worldItemIndex >= 0) return; // Explicit item disappeared; never open scenery.
	if (openable)
	{
		OS0OpenWorldContainer(gridNo, level, tileIndex);
	}
	else if (WorldAssetExistsAt(gridNo, level, tileIndex))
	{
		// Decorative/resource assets have no inventory, but double-click still
		// gives them a first-class inspector instead of an empty fake container.
		OS0SelectWorldObject(nullptr, gridNo, level, tileIndex);
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}
}


BOOLEAN OS0HandlePendingWorldMove(GridNo destination)
{
	OS0CarryState& carry = CarryState();
	if (!carry.pending()) return FALSE;
	if (carry.repositioning()) return TRUE;
	const OS0CarryMode modeBeforeTarget = carry.mode;
	auto rejectTarget = [&carry, modeBeforeTarget](const char* reason) -> BOOLEAN
	{
		if (!carry.pointerDrag)
		{
			carry.mode = carry.persistentGrab ?
				OS0CarryMode::GRAB : modeBeforeTarget;
			SetRenderFlags(RENDER_FLAG_FULL);
			return TRUE;
		}
		RecordFeedbackEvent(ST::format("WORLD DRAG CANCELLED / {}", reason));
		CancelWorldMoveState();
		CursorState().action = ContextAction::MOVE;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
		return TRUE;
	};
	if (carry.source < 0 || carry.source >= WORLD_MAX ||
		destination < 0 || destination >= WORLD_MAX ||
		destination == carry.source || carry.tileIndex >= NUMBEROFTILES)
	{
		return rejectTarget("INVALID TARGET");
	}
	SOLDIERTYPE* const selected = CarryCarrier();
	if (!selected || !selected->bActive || selected->bLife <= 0)
	{
		CancelWorldMoveState();
		return TRUE;
	}

	STRUCTURE* const structure = CarryStructure();
	constexpr StructureFlags fixed = static_cast<StructureFlags>(
		STRUCTURE_WALLSTUFF | STRUCTURE_ROOF | STRUCTURE_PERSON |
		STRUCTURE_CORPSE | STRUCTURE_TREE | STRUCTURE_ANYFENCE |
		STRUCTURE_SWITCH | STRUCTURE_VEHICLE | STRUCTURE_LIGHTSOURCE);
	if (!structure || !(structure->fFlags & STRUCTURE_BASE_TILE) ||
		structure->fFlags & fixed ||
		!structure->pDBStructureRef ||
		structure->pDBStructureRef->pDBStructure->ubNumberOfTiles != 1)
	{
		CancelWorldMoveState();
		return TRUE;
	}
	if (!CanSoldierMoveWorldStructure(selected, structure))
	{
		RecordFeedbackEvent("CARRY CANCELLED / LOAD CHANGED");
		CancelWorldMoveState();
		CursorState().action = ContextAction::MOVE;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
		return TRUE;
	}
	// Wounds and object condition can change while the grab is active. Recompute
	// the live handling pose before every physical step and throw decision.
	WORLD_PHYSICS_PROFILE const livePhysics = GetWorldPhysicsProfile(structure);
	carry.lifted = livePhysics.massKg <=
		GetSoldierWorldCarryCapacityKg(selected) * 0.55f;

	const INT16 sourceDistance = PythSpacesAway(selected->sGridNo, carry.source);
	const INT16 destinationDistance =
		PythSpacesAway(selected->sGridNo, destination);
	if (carry.persistentGrab || carry.mode == OS0CarryMode::GRAB)
	{
		const UINT8 towardObject = GetDirectionFromGridNo(carry.source, selected);
		const GridNo beyondObject =
			NewGridNo(carry.source, DirectionInc(towardObject));
		if (destination == beyondObject)
			carry.mode = OS0CarryMode::PUSH;
		else if (destination == selected->sGridNo ||
			destinationDistance < sourceDistance)
			carry.mode = OS0CarryMode::PULL;
		else
			carry.mode = OS0CarryMode::CARRY;
	}
	if (carry.mode == OS0CarryMode::PUSH)
	{
		const UINT8 away = GetDirectionFromGridNo(carry.source, selected);
		const GridNo required = NewGridNo(carry.source, DirectionInc(away));
		if (destination != required) return rejectTarget("PUSH DIRECTION");
	}
	else if (carry.mode == OS0CarryMode::PULL)
	{
		if (PythSpacesAway(carry.source, destination) > 1 ||
			destinationDistance >= sourceDistance)
			return rejectTarget("PULL DIRECTION");
	}
	else if (carry.mode == OS0CarryMode::THROW)
	{
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		const INT16 maxRange = static_cast<INT16>(std::clamp<INT32>(
			2 + selected->bStrength / 20 - static_cast<INT32>(physics.massKg / 15.0f),
			2, 8));
		if (!carry.lifted || PythSpacesAway(carry.source, destination) > maxRange)
			return rejectTarget("THROW RANGE");
	}

	// Pulling into the actor's current tile is a two-part atomic step: the actor
	// moves away before FinalizeWorldMove commits the structure. Ignore only the
	// carrier during this preflight; every other collision remains authoritative.
	const UINT16 destinationExclusion =
		carry.mode == OS0CarryMode::PULL && destination == selected->sGridNo ?
		IGNORE_PEOPLE_STRUCTURE_ID : INVALID_STRUCTURE_ID;
	if (!OkayToAddStructureToWorld(destination, carry.sourceLevel,
		structure->pDBStructureRef, destinationExclusion))
		return rejectTarget("BLOCKED TARGET");
	GridNo actionGrid = FindCarryActionGrid(selected, destination);
	if (carry.mode == OS0CarryMode::PUSH)
		actionGrid = selected->sGridNo;
	else if (carry.mode == OS0CarryMode::THROW)
		actionGrid = selected->sGridNo;
	else if (carry.mode == OS0CarryMode::PULL &&
		destination == selected->sGridNo)
	{
		const UINT8 awayFromSource = OppositeDirection(
			GetDirectionFromGridNo(carry.source, selected));
		actionGrid = NewGridNo(selected->sGridNo, DirectionInc(awayFromSource));
		if (actionGrid == selected->sGridNo ||
			!NewOKDestination(selected, actionGrid, TRUE, selected->bLevel))
			return rejectTarget("NO PULL FOOTING");
	}
	else if (carry.persistentGrab && carry.mode == OS0CarryMode::CARRY)
	{
		const UINT8 objectToActor = OppositeDirection(
			GetDirectionFromGridNo(carry.source, selected));
		actionGrid = NewGridNo(destination, DirectionInc(objectToActor));
		if (actionGrid == selected->sGridNo ||
			!NewOKDestination(selected, actionGrid, TRUE, selected->bLevel))
			return rejectTarget("NO CARRY FOOTING");
	}
	if (actionGrid == NOWHERE) return rejectTarget("NO APPROACH");

	if (!carry.beginWalk(destination, carry.sourceLevel, actionGrid,
		selected->uiUniqueSoldierIdValue, structure->usStructureID,
		StructureBaseGridNo(structure)))
	{
		CancelWorldMoveState();
		RecordFeedbackEvent("CARRY CANCELLED / IDENTITY CHANGED");
		return TRUE;
	}
	CursorState().action = ContextAction::MOVE;
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;

	if (selected->sGridNo != actionGrid &&
		!EVENT_InternalGetNewSoldierPath(selected, actionGrid,
			selected->usUIMovementMode, TRUE, TRUE))
	{
		if (carry.pointerDrag) return rejectTarget("NO PATH");
		carry.destination = NOWHERE;
		carry.actionGrid = NOWHERE;
		carry.phase = OS0CarryPhase::TARGETING;
		CursorState().action = CarryModeAction(carry.mode);
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
	if (gWorldZoom == 1 && next > 1)
	{
		// The fast pixel-scroll path moves the previously presented buffer.  A
		// magnified viewport no longer has a 1:1 relation to those increments, so
		// render the world for its new camera position instead of shifting stale
		// zoomed pixels (and fixed windows) around.
		gVideoScrollBeforeZoom = gfDoVideoScroll;
		gfDoVideoScroll = FALSE;
	}
	else if (gWorldZoom > 1 && next == 1)
	{
		gfDoVideoScroll = gVideoScrollBeforeZoom;
	}
	gWorldZoom = next;
	// Zoom is a view operation. It must not cancel a crate/asset placement that
	// is currently attached to the cursor or waiting for its carrier.
	DiscardWorldZoomBuffer();
	SetRenderFlags(RENDER_FLAG_FULL);
	InvalidateScreen();
}


UINT8 OS0WorldZoomFactor()
{
	return std::max<UINT8>(1, gWorldZoom);
}


BOOLEAN OS0WorldZoomKeepsLegacyScrollBoost()
{
	return gWorldZoom > 1 && !gVideoScrollBeforeZoom;
}


void OS0PrepareWorldZoom()
{
	if (gWorldZoom <= 1) return;

	// The previous frame contains the enlarged world and the OS//0 overlay.
	// Restore only the last clean, unzoomed tactical viewport before rendering.
	// The engine can then update its normal dirty world regions; forcing a full
	// world render here made 2x zoom needlessly redraw the entire sector forever.
	SGPBox source;
	SGPBox destination;
	GetWorldZoomRects(source, destination);
	const BOOLEAN compatible = gWorldZoomBuffer &&
		gWorldZoomBuffer->Width() == destination.w &&
		gWorldZoomBuffer->Height() == destination.h &&
		gWorldZoomBufferViewportValid &&
		SameWorldZoomViewport(gWorldZoomBufferViewport, destination);
	if (!compatible)
	{
		// Combat top messages move the viewport (usually Y 0 -> 20) without a
		// resolution change. Dirty rendering onto the old stretched frame would
		// capture and magnify mixed pixels. Establish one native full-render frame
		// before a new zoom cache becomes eligible.
		DiscardWorldZoomBuffer();
		SetRenderFlags(RENDER_FLAG_FULL);
		return;
	}
	if (compatible)
	{
		const SGPBox savedViewport{ 0, 0, destination.w, destination.h };
		BltVideoSurface(FRAME_BUFFER, gWorldZoomBuffer,
			destination.x, destination.y, &savedViewport);
	}
}


void OS0ApplyWorldZoom()
{
	if (gWorldZoom <= 1) return;
	SGPBox source;
	SGPBox destination;
	GetWorldZoomRects(source, destination);
	if (!gWorldZoomBuffer ||
		gWorldZoomBuffer->Width() != destination.w ||
		gWorldZoomBuffer->Height() != destination.h ||
		!gWorldZoomBufferViewportValid ||
		!SameWorldZoomViewport(gWorldZoomBufferViewport, destination))
	{
		DiscardWorldZoomBuffer();
		gWorldZoomBuffer = AddVideoSurface(destination.w, destination.h, PIXEL_DEPTH);
	}
	const SGPBox viewport{ destination.x, destination.y,
		destination.w, destination.h };
	BltVideoSurface(gWorldZoomBuffer, FRAME_BUFFER, 0, 0, &viewport);
	gWorldZoomBufferViewport = destination;
	gWorldZoomBufferViewportValid = TRUE;
	SGPBox localSource = source;
	localSource.x -= destination.x;
	localSource.y -= destination.y;
	BltStretchVideoSurface(FRAME_BUFFER, gWorldZoomBuffer, &localSource,
		&destination);
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


INT16 OS0WorldViewportBottom()
{
	if (gInitialized) return gUILayout.worldBottom();
	return std::min<INT16>(gsVIEWPORT_WINDOW_END_Y, SCREEN_HEIGHT);
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
	SetRenderFlags(RENDER_FLAG_FULL);
}


void OS0TalkingPanelClosed()
{
	if (!gTalkDocked) return;
	gTalkDocked = FALSE;
	gBagVisible = gBagVisibleBeforeTalk;
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}
