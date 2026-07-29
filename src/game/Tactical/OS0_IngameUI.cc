/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "OS0_IngameUI.h"
#include "OS0_ActionRegistry.h"
#include "OS0_AssetCatalogService.h"
#include "OS0_AssetDamageSystem.h"
#include "OS0_CreatorModel.h"
#include "OS0_DirectControl.h"
#include "OS0_ItemRelations.h"
#include "OS0_MouseRegionZOrder.h"
#include "OS0_RealtimeEditor.h"
#include "OS0_RealtimeEditorUI.h"
#include "OS0_TacticalSession.h"
#include "OS0_UIAssetManager.h"
#include "OS0_UIRuntime.h"
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
#include "Interface_Dialogue.h"
#include "Interface_Items.h"
#include "Interface_Panels.h"
#include "Isometric_Utils.h"
#include "Items.h"
#include "Lighting.h"
#include "Local.h"
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
#include <utility>
#include <vector>


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
	// The field-computer orb owns TACTICAL; the remaining eight toolbox modules
	// are always available as fixed, screen-space dock icons.
	constexpr size_t PANEL_DOCK_COUNT =
		static_cast<size_t>(OS0UICommand::COUNT) - 1;
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

	struct ContextEntry
	{
		ContextAction action;
		ST::string label;
		BOOLEAN enabled;
	};

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
	BOOLEAN gInventoryVisible = FALSE;
	BOOLEAN& gLootVisible = gUIRuntime.visibilityRef(OS0UIPanel::LOOT);
	BOOLEAN& gContextVisible = gUIRuntime.visibilityRef(OS0UIPanel::CONTEXT);
	BOOLEAN gObjectActionFanVisible = FALSE;
	BOOLEAN gCharacterActionFanVisible = FALSE;
	BOOLEAN& gEquipmentExplodedVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::EQUIPMENT);
	BOOLEAN& gStackSplitVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::STACK_SPLIT);
	SOLDIERTYPE* gEquipmentSoldier = nullptr;
	SOLDIERTYPE* gStackSplitSoldier = nullptr;
	INT8 gStackSplitSlot = NO_SLOT;
	UINT8 gStackSplitAmount = 1;
	ContentsMode gContentsMode = ContentsMode::SOLDIER;
	BOOLEAN gHoverVisible = FALSE;
	BOOLEAN gInspectorPinned = TRUE;
	BOOLEAN gAimAutoCollapsed = FALSE;
	BOOLEAN gWindowMovedThisFrame = FALSE;
	OBJECTTYPE const* gLastItemCursorPointer = nullptr;
	UINT16 gLastItemCursorItem = NOTHING;
	UINT32 gNextItemCursorRefreshAt = 0;
	INT8 gLootDragCandidate = -1;
	INT16 gLootDragStartX = 0;
	INT16 gLootDragStartY = 0;
	UINT32 gLootIgnoreInputUntil = 0;
	UINT32 gPanelInteractionGuardUntil = 0;
	BOOLEAN gBagVisibleBeforeTalk = TRUE;
	BOOLEAN gTalkDocked = FALSE;
	BOOLEAN gFieldToolIssued = FALSE;
	INT16& gBagX = gUIRuntime.panel(OS0UIPanel::INVENTORY).x;
	INT16& gBagY = gUIRuntime.panel(OS0UIPanel::INVENTORY).y;
	INT16& gInventoryX = gBagX;
	INT16& gInventoryY = gBagY;
	INT16 gOrbY = 316;
	INT16& gContextX = gUIRuntime.panel(OS0UIPanel::CONTEXT).x;
	INT16& gContextY = gUIRuntime.panel(OS0UIPanel::CONTEXT).y;
	INT16 gEquipmentCentreX = 0;
	INT16 gEquipmentCentreY = 0;
	INT16& gStackSplitX = gUIRuntime.panel(OS0UIPanel::STACK_SPLIT).x;
	INT16& gStackSplitY = gUIRuntime.panel(OS0UIPanel::STACK_SPLIT).y;
	INT16 gHoverX = 0;
	INT16 gHoverY = 0;
	ComputerMode gMode = ComputerMode::INFO;
	SOLDIERTYPE* gInspectedSoldier = nullptr;
	// The equipment window is an independent live view. World inspection and
	// container selection must never silently retarget or close it.
	SOLDIERTYPE* gInventorySoldier = nullptr;
	GridNo gInspectedGridNo = NOWHERE;
	UINT8 gInspectedLevel = 0;
	UINT16 gInspectedTileIndex = NO_TILE;
	BOOLEAN& gItemDetailsVisible =
		gUIRuntime.visibilityRef(OS0UIPanel::ITEM_DETAILS);
	OBJECTTYPE gItemDetailsObject{};
	ST::string gItemDetailsName;
	SectorPanelMode gSectorPanelMode = SectorPanelMode::BASE;
	SGPSector gStrategicSelectedSector{ 9, 1, 0 };
	UINT32 gExplodedViewStarted = 0;
	GridNo gExplodedViewGridNo = NOWHERE;
	UINT16 gExplodedViewTileIndex = NO_TILE;
	BOOLEAN& gTutorialActive = gUIRuntime.creatorActiveRef();
	UINT8& gTutorialStep = gUIRuntime.creatorStageValueRef();
	OS0CreatorModel gCreatorModel;
	BOOLEAN gVideoScrollBeforeCreator = TRUE;
	UINT8 gWorldZoom = 1;
	BOOLEAN gVideoScrollBeforeZoom = TRUE;
	SOLDIERTYPE* gHoverCursorSoldier = nullptr;
	GridNo gHoverCursorGridNo = NOWHERE;
	UINT8 gHoverCursorLevel = 0;
	UINT16 gHoverCursorTileIndex = NO_TILE;
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

	OS0CarryState& CarryState()
	{
		return OS0GetTacticalSession().state().carry;
	}

	OS0CursorState& CursorState()
	{
		return OS0GetTacticalSession().state().cursor;
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
			case ActionCategory::COUNT:
				return OS0InteractionSurface::ACTIONS;
		}
		return OS0InteractionSurface::ACTIONS;
	}

	void ResetNearbyScanCache()
	{
		gNearbyHintActorGridNo = NOWHERE;
		gNearbyHintCursorGridNo = NOWHERE;
		gNextNearbyHintScanAt = 0;
		// Scan mode changes hover semantics. Force the current under-cursor object
		// through the resolver even when the pointer itself did not move.
		gHoverCursorSoldier = nullptr;
		gHoverCursorGridNo = NOWHERE;
		gHoverCursorLevel = 0;
		gHoverCursorTileIndex = NO_TILE;
		gHoverCursorHeldItem = NOTHING;
		gHoverSuggestedAction = ContextAction::COUNT;
	}

	void SetInteractionForAction(ContextAction const action)
	{
		if (action == ContextAction::ATTACK)
		{
			InteractionMode().beginFight(OS0InteractionSurface::ACTIONS);
		}
		else if (action == ContextAction::MOVE)
		{
			if (InteractionMode().nearbyScanEnabled())
				InteractionMode().returnToNormal();
			else
			{
				InteractionMode().selectSurface(OS0InteractionSurface::ACTIONS);
				InteractionMode().returnToNormal();
			}
		}
		else
		{
			InteractionMode().beginInteraction(SurfaceForAction(action));
		}
	}

	SOLDIERTYPE* CarryCarrier()
	{
		return ID2Soldier(CarryState().carrier);
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
	void DrawFloatingPanelShell(FloatingPanel const& panel, FloatingPanelId id,
		const ST::string& title);
	void ContextActionCallback(MOUSE_REGION* region, UINT32 reason);
	void EquipmentPackCallback(MOUSE_REGION* region, UINT32 reason);
	void ItemTransferIntentCallback(MOUSE_REGION* region, UINT32 reason);
	void StackSplitCallback(MOUSE_REGION* region, UINT32 reason);
	void OperationsActionCallback(MOUSE_REGION* region, UINT32 reason);
	void GodIconCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryGrabberCallback(MOUSE_REGION* region, UINT32 reason);
	void DebugLibraryCloseCallback(MOUSE_REGION* region, UINT32 reason);
	void AssetCatalogCallback(MOUSE_REGION* region, UINT32 reason);
	void ToolboxModuleCallback(MOUSE_REGION* region, UINT32 reason);
	void EnvironmentSkillCallback(MOUSE_REGION* region, UINT32 reason);
	void NearbyHintCallback(MOUSE_REGION* region, UINT32 reason);
	void NearbyHintMoveCallback(MOUSE_REGION* region, UINT32 reason);
	void ActivateToolboxModule(ToolboxModule module);
	void ApplyCursorTool(ContextAction action);
	void SetBagRegionsEnabled(BOOLEAN enabled);
	size_t RefreshLootWorldItems();
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
	SOLDIERTYPE* gItemTransferTarget = nullptr;
	MOUSE_REGION gStackSplitBlock;
	std::array<MOUSE_REGION, 5> gStackSplitRegions;
	MOUSE_REGION gOrbRegion;
	MOUSE_REGION gTutorialContinue;
	std::array<MOUSE_REGION, 20> gTutorialStats;
	std::array<MOUSE_REGION, 15> gTutorialTraitRegions;
	std::array<MOUSE_REGION, 5> gOpsActionRegions;
	std::array<MOUSE_REGION, 12> gContextRegions;
	std::array<MOUSE_REGION, 12> gLootRegions;
	std::array<INT32, 12> gLootWorldItems;
	std::array<ST::string, NUM_INV_SLOTS> gSlotHelp;
	std::array<ST::string, 7> gEquipmentHelp;
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
	ST::string gHoverDebugDetail;
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

	constexpr UINT8 OS0_CONTAINER_MARKER = 0xE0;
	auto const& gSectorUpgrades = OS0SectorUpgrades();

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

	INT16 WorkspaceBottom()
	{
		return gUILayout.workspaceBottom();
	}

	BOOLEAN CompactArtworkWorkspace()
	{
		return gsVIEWPORT_END_X < PANE_W + SECTOR_PANEL_W + 32 ||
			WorkspaceBottom() < BAG_H + TOOLBOX_H + 28;
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

	void ClampWindowPositions()
	{
		gUIRuntime.windowManager().clampAll();
		gInventoryX = gBagX;
		gInventoryY = gBagY;
	}

	BOOLEAN SaveUILayout()
	{
		try
		{
			GCM->userPrivateFiles()->createDir("OS0");
			ST::string const output = gUIRuntime.windowManager().serializeLayout(
				SCREEN_WIDTH, SCREEN_HEIGHT);
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
			if (layoutText.find("window ") != std::string::npos)
			{
				gUIRuntime.windowManager().restoreLayout(savedLayout,
					SCREEN_WIDTH, SCREEN_HEIGHT);
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

	BOOLEAN CanAccessSoldierContents(SOLDIERTYPE const* soldier)
	{
		if (!soldier) return FALSE;
		if (soldier->bTeam == OUR_TEAM) return TRUE;
		SOLDIERTYPE const* const selected = GetSelectedMan();
		return selected &&
			PythSpacesAway(selected->sGridNo, soldier->sGridNo) <= 2 &&
			(soldier->bLife < OKLIFE || soldier->uiStatusFlags & SOLDIER_DEAD);
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
		SetUIKeyboardHook(gTutorialActive && gTutorialStep == 1 ?
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

	void RestoreWorldMoveShade()
	{
		OS0CarryState& carry = CarryState();
		if (!carry.sourceShaded) return;
		if (LEVELNODE* const node = WorldLevelNodeAt(carry.source,
			carry.sourceLevel, carry.tileIndex))
		{
			node->ubShadeLevel = carry.oldShade;
		}
		carry.sourceShaded = FALSE;
	}

	void ShadeWorldMoveSource()
	{
		RestoreWorldMoveShade();
		// Never encode carry state by changing the map node's palette. That old
		// shortcut left moved crates permanently grey when another hover/reset won
		// the shade race. The coloured carried sprite is now the only preview.
	}

	void ClearWorldMoveState()
	{
		RestoreWorldMoveShade();
		CarryState().reset();
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

	ContextAction CarryModeAction(OS0CarryMode mode)
	{
		switch (mode)
		{
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
			case ContextAction::PUSH: return OS0CarryMode::PUSH;
			case ContextAction::PULL: return OS0CarryMode::PULL;
			case ContextAction::THROW: return OS0CarryMode::THROW;
			default: return OS0CarryMode::CARRY;
		}
	}

	const char* CarryModeName(OS0CarryMode mode)
	{
		return ContextActionName(CarryModeAction(mode));
	}

	BOOLEAN BeginInspectedWorldMove(OS0CarryMode mode = OS0CarryMode::CARRY)
	{
		if (!IsInspectedWorldAssetNear() || !IsInspectedWorldAssetMovable() ||
			gInspectedTileIndex >= NUMBEROFTILES) return FALSE;
		ClearWorldMoveState();
		CursorState().action = CarryModeAction(mode);
		OS0CarryState& carry = CarryState();
		SOLDIERTYPE* const carrier = GetSelectedMan();
		if (!carrier || !carry.begin(gInspectedGridNo, gInspectedLevel,
			gInspectedTileIndex, Soldier2ID(carrier), mode)) return FALSE;
		if (STRUCTURE const* const structure = WorldStructureAt(carry.source,
			carry.sourceLevel, carry.tileIndex))
		{
			WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
			carry.lifted = physics.massKg <=
				GetSoldierWorldCarryCapacityKg(carrier) * 0.55f;
		}
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
		if ((gTutorialActive && gBagVisible && overlaps(gBagX, gBagY, PANE_W, BAG_H)) ||
			overlaps(0, gOrbY, COLLAPSED_OS0_W, COMMAND_BAR_H) ||
			(gContextVisible && overlaps(gContextX, gContextY, 168,
				static_cast<INT16>(20 + gContextEntryCount * 18)))) return;
		BltVideoSurface(gInspectorPreview, FRAME_BUFFER, 0, 0, &source);
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

	void StartExplodedView(GridNo gridNo, UINT16 tileIndex, BOOLEAN force)
	{
		if (gridNo < 0 || tileIndex >= NUMBEROFTILES) return;
		if (force || gridNo != gExplodedViewGridNo ||
			tileIndex != gExplodedViewTileIndex)
		{
			gExplodedViewGridNo = gridNo;
			gExplodedViewTileIndex = tileIndex;
			gExplodedViewStarted = GetJA2Clock();
		}
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
		gUIRuntime.hide(OS0UIPanel::CONTEXT);
		gObjectActionFanVisible = FALSE;
		gCharacterActionFanVisible = FALSE;
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
		const ContextEntry entry{ action, label, enabled };
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

	void FilterContextEntriesForSurface(OS0InteractionSurface const surface)
	{
		size_t output = 0;
		for (size_t input = 0; input < gContextEntryCount; ++input)
		{
			if (SurfaceForAction(gContextEntries[input].action) != surface) continue;
			if (output != input) gContextEntries[output] = gContextEntries[input];
			++output;
		}
		gContextEntryCount = output;
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
			case ActionCategory::DEBUG: return Get16BPPColor(FROMRGB(156, 38, 156));
			case ActionCategory::COUNT: break;
		}
		return Get16BPPColor(FROMRGB(118, 0, 0));
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
		if (!soldier) return;
		constexpr std::array<UINT16, 4> tools{{
			CROWBAR, WIRECUTTERS, TOOLKIT, COMBAT_KNIFE
		}};
		for (UINT16 item : tools)
		{
			if (FindUsableObj(soldier, item) != NO_SLOT) continue;
			OBJECTTYPE object{};
			CreateItem(item, 100, &object);
			AutoPlaceObject(soldier, &object, TRUE);
		}
	}

	void AddResourceItemToPool(GridNo gridNo, UINT8 level, ResourceKind kind,
		UINT8 amount)
	{
		if (amount == 0 || kind == ResourceKind::COUNT) return;
		OBJECTTYPE resource{};
		CreateItems(OS0ResourceItem(kind), 100,
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
		if (CurrentSectorEconomy().hasUpgrade(CurrentSectorKey(),
			OS0_SECTOR_UPGRADE_DEPOT)) ++primary;
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
		if (!worldItem.fExists || !OS0IsResourceItem(worldItem.o.usItem)) return FALSE;
		ResourceKind const kind = OS0ResourceFromItem(worldItem.o.usItem);
		const UINT8 amount = worldItem.o.ubNumberOfObjects;
		if (!OS0DepositResources(CurrentSectorEconomy(), CurrentSectorKey(),
			kind, amount)) return FALSE;
		RemoveItemFromPool(worldItem);
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
			tileIndex >= NUMBEROFTILES) return FALSE;
		return DescribeWorldAsset(gridNo, level, tileIndex).salvageable &&
			HasFieldTool(soldier, RequiredFieldTool(gridNo, level, tileIndex));
	}

	OS0EnvironmentActionFacts BuildEnvironmentFacts(GridNo gridNo, UINT8 level,
		UINT16 tileIndex, SOLDIERTYPE const* actor)
	{
		OS0EnvironmentActionFacts facts;
		if (gridNo < 0 || gridNo >= WORLD_MAX) return facts;
		STRUCTURE const* const structure = tileIndex < NUMBEROFTILES ?
			WorldStructureAt(gridNo, level, tileIndex) : nullptr;
		facts.hasAsset = tileIndex < NUMBEROFTILES;
		facts.hasItems = GetItemPool(gridNo, level) != nullptr;
		facts.terrain = level == 0 && gpWorldLevelData[gridNo].pLandHead;
		facts.near = actor && actor->bLevel == level &&
			PythSpacesAway(actor->sGridNo, gridNo) <= 2;
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
		facts.canMove = facts.near && facts.moveCandidate && actor &&
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
		facts.canSalvage = CanSalvageWorldAsset(actor, gridNo, level, tileIndex);
		facts.diggableSurface = facts.terrain && !structure;
		facts.canDig = facts.diggableSurface && CanDigTerrainAt(actor, gridNo);
		AssetCatalogRecord const* const catalog = facts.hasAsset ?
			OS0FindAssetCatalogRecordConst(static_cast<INT16>(giCurrentTilesetID),
				CanonicalAssetTileIndex(gridNo, level, tileIndex)) : nullptr;
		facts.buildable = catalog && catalog->buildable;
		facts.debugCatalog = TRUE;
		return facts;
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
				return HasDiggingTool(GetSelectedMan()) ?
					"DIG / REMOVE SURFACE" : "DIG / NEED FIELD SHOVEL";
			case ContextAction::SALVAGE:
			{
				FieldToolKind const tool = RequiredFieldTool(gridNo, level, tileIndex);
				return HasFieldTool(GetSelectedMan(), tool) ?
					"DISMANTLE / SALVAGE" :
					ST::format("DISMANTLE / NEED {}", FieldToolName(tool));
			}
			case ContextAction::CARRY: return "CARRY / LIFT + PLACE";
			case ContextAction::PUSH: return "PUSH / ONE-TILE STEPS";
			case ContextAction::PULL: return "PULL / WALK BACKWARD";
			case ContextAction::THROW:
				return facts.canThrow ? "THROW / CHOOSE LANDING" :
					"THROW / TOO HEAVY";
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
			ResolveOS0EnvironmentActions(facts))
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
		std::vector<OS0ResolvedAction> const& actions)
	{
		for (OS0ResolvedAction const& entry : actions)
		{
			if (entry.action == ContextAction::CONTENTS ||
				entry.action == ContextAction::PICK_UP ||
				entry.action == ContextAction::DIG ||
				entry.action == ContextAction::SALVAGE ||
				OS0IsManipulationAction(entry.action)) return entry.action;
		}
		return ContextAction::COUNT;
	}

	void AddNearbyInteractionHint(GridNo gridNo, UINT8 level, UINT16 tileIndex)
	{
		if (gNearbyHintCount >= gNearbyHints.size()) return;
		for (size_t i = 0; i < gNearbyHintCount; ++i)
		{
			if (gNearbyHints[i].gridNo == gridNo &&
				gNearbyHints[i].tileIndex == tileIndex) return;
		}
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			level, tileIndex, GetSelectedMan());
		std::vector<OS0ResolvedAction> const actions =
			ResolveOS0EnvironmentActions(facts);
		ContextAction const primary = PrimaryProximityAction(actions);
		if (primary == ContextAction::COUNT) return;
		auto const match = std::find_if(actions.begin(), actions.end(),
			[primary](OS0ResolvedAction const& entry)
			{ return entry.action == primary; });
		gNearbyHints[gNearbyHintCount++] = { gridNo, level, tileIndex, primary,
			match != actions.end() && match->enabled };
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
					previous[i].tileIndex != gNearbyHints[i].tileIndex ||
					previous[i].action != gNearbyHints[i].action ||
					previous[i].enabled != gNearbyHints[i].enabled;
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
		auto addCursorSoilHint = [&]()
		{
			if (gNearbyHintCount < gNearbyHints.size() &&
				guiCurrentCursorGridNo >= 0 && guiCurrentCursorGridNo < WORLD_MAX &&
				PythSpacesAway(selected->sGridNo, guiCurrentCursorGridNo) <= 2 &&
				ResolveWorldTileIndex(guiCurrentCursorGridNo, 0, NO_TILE) >= NUMBEROFTILES)
				AddNearbyInteractionHint(guiCurrentCursorGridNo, 0, NO_TILE);
		};
		if (selected->sGridNo == gNearbyHintActorGridNo &&
			now < gNextNearbyHintScanAt)
		{
			gNearbyHints = previous;
			gNearbyHintCount = previousCount;
			if (guiCurrentCursorGridNo != gNearbyHintCursorGridNo)
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
				gNearbyHintCursorGridNo = guiCurrentCursorGridNo;
				addCursorSoilHint();
				invalidateIfChanged();
			}
			return;
		}
		gNearbyHintActorGridNo = selected->sGridNo;
		gNearbyHintCursorGridNo = guiCurrentCursorGridNo;
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
		if (CurrentSectorEconomy().hasUpgrade(CurrentSectorKey(),
			OS0_SECTOR_UPGRADE_WORKSHOP))
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
		gContextTitle = ST::format("SALVAGE / {}", OS0ResourceName(profile.resource));
		gContentsMode = ContentsMode::WORLD;
		gLootVisible = TRUE;
		RecordFeedbackEvent(ST::format("SALVAGE {} +{} {} grid {}",
			profile.displayName, amount, OS0ResourceName(profile.resource), dropGrid));
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
		return TRUE;
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
		std::vector<OS0CoverOrder> const orders = system.orders();
		for (OS0CoverOrder const& order : orders)
		{
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
					ActionCategoryName(ContextActionCategory(gContextEntries[i].action)),
					gContextEntries[i].label,
					ContextActionExplanation(gContextEntries[i].action)));
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
		// JA2 stops its render camera when the unzoomed viewport reaches a map
		// edge. At 2x zoom the old centre crop consequently hid half of that edge.
		// Bias the crop to the boundary that the engine has actually reached.
		if (gfScrolledToLeft) source.x = destination.x;
		else if (gfScrolledToRight)
			source.x = destination.x + destination.w - source.w;
		if (gfScrolledToTop) source.y = destination.y;
		else if (gfScrolledToBottom)
			source.y = destination.y + destination.h - source.h;
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
		if (!gpItemPointer ||
			!GetActorDisplayAnchor(gItemTransferTarget, anchorX, anchorY)) return;
		for (size_t i = 0; i < gOS0ItemTransferIntents.size(); ++i)
		{
			const INT16 x = std::clamp<INT16>(
				anchorX + gOS0ItemTransferIntents[i].offsetX,
				gsVIEWPORT_START_X + 2, gsVIEWPORT_END_X - 30);
			const INT16 y = std::clamp<INT16>(
				anchorY + gOS0ItemTransferIntents[i].offsetY,
				gsVIEWPORT_WINDOW_START_Y + 2, gsVIEWPORT_WINDOW_END_Y - 30);
			MoveRegion(gItemTransferIntentRegions[i], x, y);
		}
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
			addArray(inventory, gOpsActionRegions);
			addOne(inventory, gTutorialContinue);
			addArray(inventory, gTutorialStats);
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
			if (!worldItem.fExists || worldItem.o.usItem == NOTHING ||
				worldItem.o.usItem == OWNERSHIP ||
				worldItem.o.usItem == ACTION_ITEM) continue;
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
		PositionEquipmentRegions();
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
			setVisible(gPanelDockRegions[i], !gTutorialActive && !gAimAutoCollapsed);
		}
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
				(!gTutorialActive || gTutorialStep == 4) &&
				CanAccessSoldierContents(gInventorySoldier ?
					gInventorySoldier : GetSelectedMan())) r.Enable();
			else r.Disable();
		}
		SetLootRegionsEnabled(enabled && showContentLoot);
		for (MOUSE_REGION& r : gOpsActionRegions)
		{
			r.Disable();
		}
		for (MOUSE_REGION& r : gEquipmentRegions)
			setVisible(r, gEquipmentExplodedVisible && gEquipmentSoldier &&
				CanAccessSoldierContents(gEquipmentSoldier) && !contextVisible &&
				!gAimAutoCollapsed);
		setVisible(gEquipmentPackRegion, gEquipmentExplodedVisible &&
			gEquipmentSoldier && gEquipmentSoldier->bTeam == OUR_TEAM &&
			!contextVisible && !gAimAutoCollapsed);
		for (MOUSE_REGION& r : gItemTransferIntentRegions)
			setVisible(r, gpItemPointer && gItemTransferTarget &&
				CanAccessSoldierContents(gItemTransferTarget) && !contextVisible);
		if (enabled && gTutorialActive && !contextVisible && !catalogVisible)
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
		gOrbRegion.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
		gStackSplitBlock.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
		for (MOUSE_REGION& r : gStackSplitRegions)
			r.ChangeCursor(gpItemPointer ? EXTERN_CURSOR : CURSOR_NORMAL);
		RefreshHeldItemCursor();
		if (OS0GetRealtimeEditorUI().initialized())
		{
			OS0GetRealtimeEditorUI().setInputEnabled(enabled &&
				!contextVisible && !gStackSplitVisible && !catalogVisible &&
				!gTutorialActive && !gAimAutoCollapsed && !gpItemPointer);
			OS0GetRealtimeEditorUI().update();
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
		PositionLootRegions();
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
		for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
		{
			OS0UIRect const dock =
				gUILayout.command(OS0CommandForDockSlot(i));
			MoveRegion(gPanelDockRegions[i], dock.x, dock.y);
			gPanelDockRegions[i].RegionBottomRightX = dock.x + dock.w;
			gPanelDockRegions[i].RegionBottomRightY = dock.y + dock.h;
		}
		OS0UIRect const tacticalDock =
			gUILayout.command(OS0UICommand::TACTICAL);
		MoveRegion(gOrbRegion, tacticalDock.x, tacticalDock.y);
		gOrbRegion.RegionBottomRightX = tacticalDock.x + tacticalDock.w;
		gOrbRegion.RegionBottomRightY = gOrbY + COMMAND_BAR_H;
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
		if (gContextVisible) PositionContextRegions();
	}

	BOOLEAN UpdateWindowDragging()
	{
		BOOLEAN const moved = gUIRuntime.windowManager().dragTo(
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

	void BagBlockCallback(MOUSE_REGION*, UINT32)
	{
		// Intentionally consumes clicks so they do not reach the tactical world.
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
			gUIRuntime.windowManager().endDrag();
			SaveUILayout();
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
			gUIRuntime.windowManager().endDrag();
			SaveUILayout();
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
		ActivateToolboxModule(OS0CommandForDockSlot(index));
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
		if (requestedRow < 0) return;
		INT16 visibleRow = 0;
		for (INT32 id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
			id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
		{
			SOLDIERTYPE& soldier = GetMan(id);
			if (!soldier.bActive || soldier.bLife <= 0) continue;
			if (visibleRow++ != requestedRow) continue;
			if (soldier.sSector == gWorldSector && !soldier.fBetweenSectors)
			{
				SelectSoldier(&soldier, SELSOLDIER_FORCE_RESELECT);
				LocateSoldier(&soldier, DONTSETLOCATOR);
				gInspectedSoldier = &soldier;
				gInventorySoldier = &soldier;
			}
			break;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ApplyCursorTool(ContextAction action)
	{
		CursorState().action = action;
		CursorState().attackMode = action == ContextAction::ATTACK;
		SetInteractionForAction(action);
		switch (ContextActionCursor(action))
		{
			case OS0CursorMode::MOVE:   guiPendingOverrideEvent = A_CHANGE_TO_MOVE;     break;
			case OS0CursorMode::HAND:   guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE; break;
			case OS0CursorMode::LOOK:   guiPendingOverrideEvent = LC_CHANGE_TO_LOOK;    break;
			case OS0CursorMode::TALK:   guiPendingOverrideEvent = T_CHANGE_TO_TALKING;  break;
			case OS0CursorMode::ATTACK: guiPendingOverrideEvent = M_CHANGE_TO_ACTION;   break;
			case OS0CursorMode::NONE:   break;
		}
		if (!OS0IsManipulationAction(action)) ClearWorldMoveState();
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
		InteractionMode().beginInteraction(SurfaceForAction(action));
		RecordFeedbackEvent(ST::format("ACTION {} grid {} tile {}",
			ContextActionName(action), gContextGridNo, gContextTileIndex));
		SOLDIERTYPE* const selected = GetSelectedMan();
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
						const BOOLEAN opening = gInventoryVisible &&
							(!gEquipmentExplodedVisible ||
							 gEquipmentSoldier != gContextSoldier);
						gEquipmentSoldier = gContextSoldier;
						gEquipmentExplodedVisible = opening;
						// Equipment lives around the actor. PACK is the explicit
						// gateway to the pocket/container window.
						gBagVisible = FALSE;
					}
					else
					{
						// Bodies use the same actor-centred equipment projection as
						// player characters; there is no second loot-window model.
						gEquipmentSoldier = gContextSoldier;
						gEquipmentExplodedVisible = gInventoryVisible;
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
				gMode = ComputerMode::BUILD;
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
					WORLDITEM& worldItem = GetWorldItem(gContextWorldItemIndex);
					if (worldItem.fExists && worldItem.o.usItem != NOTHING)
					{
						OBJECTTYPE object = worldItem.o;
						if (!OS0CanAcceptCarriedObject(selected, object))
						{
							RecordFeedbackEvent("LOAD LIMIT 125% / ITEM LEFT IN WORLD");
							break;
						}
						RemoveItemFromPool(worldItem);
						OS0EquipObject(selected, &object, NO_SLOT);
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
							OS0EquipObject(subject, &object, gContextInventorySlot);
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
				gItemDetailsObject = *detailObject;
				gItemDetailsName = GCM->getItem(detailObject->usItem)->getName();
				// Details are inspector content, not a second inventory window.  The
				// old path made an invisible ITEM_DETAILS window modal and opened the
				// loot panel as a side effect, which could strand direct control.
				gItemDetailsVisible = FALSE;
				gHoverTitle = gItemDetailsName;
				gHoverDetail = ST::format("ITEM {} / CONDITION {}% / STACK {}",
					detailObject->usItem, detailObject->bStatus[0],
					detailObject->ubNumberOfObjects);
				gHoverDebugDetail = "RMB ACTIONS / DRAG TO CHARACTER OR CONTAINER";
				gHoverVisible = TRUE;
				gInspectorPinned = TRUE;
				gUIRuntime.windowManager().show(gUIRuntime.managedId(
					FloatingPanelId::INSPECTOR));
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
						if (!OS0CanAcceptCarriedObject(selected, object))
						{
							RecordFeedbackEvent("LOAD LIMIT 125% / ITEM LEFT IN WORLD");
							break;
						}
						RemoveItemFromPool(worldItem);
						OS0EquipObject(selected, &object, NO_SLOT);
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
						OS0EquipObject(subject, &object, gContextInventorySlot);
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
							if (OS0IsResourceItem(worldItem.o.usItem))
							{
								StoreResourceWorldItem(itemIndex);
								break;
							}
							// Let JA2 own approach path, animation, AP cost, traps and
							// inventory overflow. OS//0 only selects the exact object.
							const BOOLEAN accepted =
								OS0CanAcceptCarriedObject(selected, worldItem.o);
							if (accepted)
								SoldierPickupItem(selected, itemIndex, gContextGridNo,
									ITEM_IGNORE_Z_LEVEL);
							else
								RecordFeedbackEvent(
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
		CloseContextMenu();
		gStackSplitSoldier = soldier;
		gStackSplitSlot = slot;
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
		gStackSplitAmount = 1;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void ConfirmStackSplit()
	{
		if (!gStackSplitSoldier || gStackSplitSlot < 0 ||
			gStackSplitSlot >= NUM_INV_SLOTS || gpItemPointer) return;
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
		InternalBeginItemPointer(soldier, &moving, slot);
		RecordFeedbackEvent(ST::format("STACK MOVE {} OBJECTS", amount));
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void StackSplitCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) ||
			!gStackSplitVisible || !gStackSplitSoldier ||
			gStackSplitSlot < 0) return;
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
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) ||
			!gEquipmentExplodedVisible || !gEquipmentSoldier) return;
		gInventorySoldier = gEquipmentSoldier;
		gInventoryVisible = TRUE;
		gUIRuntime.toggle(OS0UIPanel::INVENTORY);
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void DropPointerAtActor(SOLDIERTYPE* actor)
	{
		if (!gpItemPointer || !actor) return;
		AddItemToPool(actor->sGridNo, gpItemPointer, VISIBLE, actor->bLevel, 0, -1);
		NotifySoldiersToLookforItems();
		EndItemPointer();
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

	BOOLEAN ItemTransferIntentAllowed(SOLDIERTYPE* actor, ItemTransferIntent intent)
	{
		if (!actor || !gpItemPointer || !CanAccessSoldierContents(actor)) return FALSE;
		if (intent == ItemTransferIntent::DROP) return TRUE;
		if (!OS0CanAcceptCarriedObject(actor, *gpItemPointer)) return FALSE;
		if (intent == ItemTransferIntent::PACK) return TRUE;
		const INT8 slot = ItemTransferIntentSlot(actor, intent);
		if (slot == NO_SLOT) return FALSE;
		OBJECTTYPE preview = *gpItemPointer;
		return CanItemFitInPosition(actor, &preview, slot, FALSE);
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
				return ST::format("{} / SWAP {}", hand,
					GCM->getItem(actor->inv[slot].usItem)->getName());
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
				actor->inv[slot].usItem == NOTHING ? "FREE" : "SWAP");
		}
		return "INCOMPATIBLE";
	}

	BOOLEAN PlacePointerInActorSlot(SOLDIERTYPE* actor, INT8 slot)
	{
		if (!actor || !gpItemPointer || slot == NO_SLOT) return FALSE;
		OBJECTTYPE preview = *gpItemPointer;
		if (!CanItemFitInPosition(actor, &preview, slot, FALSE) ||
			!PlaceObject(actor, slot, gpItemPointer)) return FALSE;

		// A swap leaves the displaced item on the cursor. Return it to the source
		// slot or pack it; only a genuine no-space result becomes a world object.
		if (gpItemPointer->usItem != NOTHING)
		{
			const BOOLEAN sourceSlotUsable = actor == gpItemPointerSoldier &&
				gbItemPointerSrcSlot >= 0 && gbItemPointerSrcSlot < NUM_INV_SLOTS &&
				gbItemPointerSrcSlot != slot &&
				actor->inv[gbItemPointerSrcSlot].usItem == NOTHING;
			if (sourceSlotUsable)
				PlaceObject(actor, gbItemPointerSrcSlot, gpItemPointer);
			if (gpItemPointer->usItem != NOTHING)
				AutoPlaceObject(actor, gpItemPointer, FALSE);
			if (gpItemPointer->usItem != NOTHING) DropPointerAtActor(actor);
		}
		if (gpItemPointer && gpItemPointer->ubNumberOfObjects == 0) EndItemPointer();
		return TRUE;
	}

	void ItemTransferIntentCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || !gpItemPointer ||
			!gItemTransferTarget || !CanAccessSoldierContents(gItemTransferTarget)) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gOS0ItemTransferIntents.size()) return;
		SOLDIERTYPE* const actor = gItemTransferTarget;
		const ItemTransferIntent intent = gOS0ItemTransferIntents[index].intent;
		if (!ItemTransferIntentAllowed(actor, intent))
		{
			RecordFeedbackEvent(ST::format("TRANSFER BLOCKED / {}",
				ItemTransferIntentLabel(actor, intent)));
			return;
		}
		switch (intent)
		{
			case ItemTransferIntent::PRIMARY_HAND:
				PlacePointerInActorSlot(actor, HANDPOS);
				break;
			case ItemTransferIntent::SECONDARY_HAND:
				PlacePointerInActorSlot(actor, SECONDHANDPOS);
				break;
			case ItemTransferIntent::BODY:
				PlacePointerInActorSlot(actor, ItemTransferIntentSlot(actor, intent));
				break;
			case ItemTransferIntent::PACK:
				if (!AutoPlaceObject(actor, gpItemPointer, TRUE) ||
					(gpItemPointer && gpItemPointer->ubNumberOfObjects > 0))
					DropPointerAtActor(actor);
				break;
			case ItemTransferIntent::DROP:
				DropPointerAtActor(actor);
				break;
		}
		if (gpItemPointer && gpItemPointer->ubNumberOfObjects == 0) EndItemPointer();
		if (!gpItemPointer) gItemTransferTarget = nullptr;
		PositionBagRegions();
		SetBagRegionsEnabled(TRUE);
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
			if (soldier == selected && soldier->inv[slot].ubNumberOfObjects > 1)
			{
				BeginStackSplit(soldier, slot);
				return;
			}
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
			if (!OS0CanAcceptCarriedObject(soldier, *gpItemPointer))
				RecordFeedbackEvent("LOAD LIMIT 125% / SLOT REJECTED");
			else if (!PlacePointerInActorSlot(soldier, slot))
				RecordFeedbackEvent("ITEM / SLOT RELATION NOT COMPATIBLE");
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

			if (OS0IsResourceItem(worldItem.o.usItem))
				StoreResourceWorldItem(itemIndex);
			else if (OS0CanAcceptCarriedObject(selected, worldItem.o))
				SoldierPickupItem(selected, itemIndex, gLootGridNo,
					ITEM_IGNORE_Z_LEVEL);
			else
				RecordFeedbackEvent("LOAD LIMIT 125% / PICKUP REJECTED");
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
		if (!soldier || gCreatorModel.callsign().empty()) return;
		const ST::string name = gCreatorModel.callsign().left(16);
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
	}

	BOOLEAN TutorialKeyboardHook(InputAtom* event)
	{
		if (!gTutorialActive || gTutorialStep != 1) return FALSE;
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
			gUIRuntime.advanceCreator();
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
				gUIRuntime.advanceCreator();
				SetUIKeyboardHook(TutorialKeyboardHook);
				break;
			case 1:
				if (gCreatorModel.callsign().empty()) return;
				ApplyTutorialName();
				gUIRuntime.advanceCreator();
				gInventoryVisible = TRUE;
				gInventorySoldier = GetSelectedMan();
				for (MOUSE_REGION& r : gTutorialStats) r.Enable();
				SetUIKeyboardHook(nullptr);
				break;
			case 2:
				ApplyTutorialStats();
				gUIRuntime.advanceCreator();
				for (MOUSE_REGION& r : gTutorialStats) r.Disable();
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Enable();
				break;
			case 3:
				ApplyTutorialTraits();
				EnsureDebugFieldTools(GetSelectedMan());
				gFieldToolIssued = TRUE;
				// Inventory is a gameplay tool, not a compulsory creator page.
				// Continue directly to the control briefing and leave it closed on entry.
				gUIRuntime.advanceCreator();
				gMode = ComputerMode::INFO;
				gContentsMode = ContentsMode::SOLDIER;
				gInventoryVisible = FALSE;
				gInventorySoldier = GetSelectedMan();
				for (MOUSE_REGION& r : gTutorialTraitRegions) r.Disable();
				break;
			case 4:
				// Compatibility with a creator state left at the old equipment page.
				gTutorialStep = static_cast<UINT8>(OS0CreatorStage::CONTROLS);
				gMode = ComputerMode::INFO;
				gInventoryVisible = FALSE;
				break;
			case 5:
				gUIRuntime.advanceCreator();
				OS0GetTacticalSession().state().creatorCompleted = TRUE;
				gfDoVideoScroll = gVideoScrollBeforeCreator;
				gMode = ComputerMode::CONTENTS;
				gContentsMode = ContentsMode::SOLDIER;
				gInspectedSoldier = GetSelectedMan();
				gInventorySoldier = gInspectedSoldier;
				gInventoryVisible = gInventorySoldier != nullptr;
				ClampWindowPositions();
				SaveUILayout();
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
		gCreatorModel.adjustStat(stat, increase ? 1 : -1);
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
				gSectorPanelMode = SectorPanelMode::MAP;
				gStrategicSelectedSector = gWorldSector;
				gUIRuntime.windowManager().show(
					gUIRuntime.managedId(FloatingPanelId::SECTOR));
				break;
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
		if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
		{
			gUIRuntime.windowManager().toggle(
				gUIRuntime.managedId(FloatingPanelId::SECTOR));
		}
		else if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			gUIRuntime.windowManager().toggle(
				gUIRuntime.managedId(FloatingPanelId::TOOLBOX));
		}
		else return;
		PositionBagRegions();
		SaveUILayout();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
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
		OS0OpenContextMenu(nullptr, gEnvironmentGridNo, gEnvironmentLevel,
			gEnvironmentTileIndex, gusMouseXPos, gusMouseYPos);
		for (size_t i = 0; i < gContextEntryCount; ++i)
		{
			if (gContextEntries[i].action != action) continue;
			ContextActionCallback(&gContextRegions[i],
				MSYS_CALLBACK_REASON_POINTER_UP);
			break;
		}
	}

	void NearbyHintCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP) || gContextVisible ||
			gTutorialActive || gAimAutoCollapsed ||
			!InteractionMode().nearbyScanEnabled()) return;
		const size_t index = static_cast<size_t>(region->GetUserData<0>());
		if (index >= gNearbyHintCount) return;
		NearbyInteractionHint const hint = gNearbyHints[index];
		RefreshEnvironmentTarget(hint.gridNo, hint.level, hint.tileIndex);
		gUIRuntime.windowManager().show(
			gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
		OS0OpenContextMenu(nullptr, hint.gridNo, hint.level, hint.tileIndex,
			region->RegionTopLeftX, region->RegionTopLeftY);
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
			region->RegionTopLeftX + 12, region->RegionTopLeftY + 12);
	}

	void ActivateToolboxModule(ToolboxModule module)
	{
		if (module == ToolboxModule::COUNT) return;
		SOLDIERTYPE* const selected = GetSelectedMan();
		switch (GetOS0UICommandDescriptor(module).intent)
		{
			case OS0UICommandIntent::RETURN_TO_ACTIONS:
				InteractionMode().setNearbyScanEnabled(false);
				ResetNearbyScanCache();
				InteractionMode().selectSurface(OS0InteractionSurface::ACTIONS);
				InteractionMode().returnToNormal();
				OS0CancelCursorAction();
				CloseContextMenu();
				break;
			case OS0UICommandIntent::TOGGLE_EQUIPMENT:
				InteractionMode().beginInteraction(
					OS0InteractionSurface::EQUIPMENT);
				if (selected)
				{
					gInventorySoldier = selected;
					gInventoryVisible = TRUE;
					gUIRuntime.toggle(OS0UIPanel::INVENTORY);
					if (gBagVisible && CompactArtworkWorkspace())
					{
						gUIRuntime.windowManager().hide(
							gUIRuntime.managedId(FloatingPanelId::SECTOR));
						gUIRuntime.windowManager().hide(
							gUIRuntime.managedId(FloatingPanelId::TOOLBOX));
					}
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
					FilterContextEntriesForSurface(
						OS0InteractionSurface::BEHAVIOR);
					gContextTitle = ST::format("{} / BEHAVIOR", selected->name);
					PositionContextRegions();
					SetBagRegionsEnabled(TRUE);
				}
				InteractionMode().beginInteraction(
					OS0InteractionSurface::BEHAVIOR);
				break;
			case OS0UICommandIntent::TOGGLE_NEARBY_SCAN:
			{
				if (!InteractionMode().canScanNearby())
				{
					RecordFeedbackEvent("NEARBY SCAN / UNAVAILABLE IN FIGHT");
					break;
				}
				InteractionMode().toggleNearbyScan();
				const bool scanEnabled = InteractionMode().nearbyScanEnabled();
				ResetNearbyScanCache();
				if (scanEnabled)
				{
					if (InteractionMode().isNormal())
						InteractionMode().selectSurface(
							OS0InteractionSurface::ENVIRONMENT);
					gUIRuntime.windowManager().show(
						gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
				}
				else
				{
					gUIRuntime.windowManager().hide(
						gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
					if (!gpItemPointer && !CarryState().active())
						ApplyCursorTool(ContextAction::MOVE);
				}
				RecordFeedbackEvent(scanEnabled ?
					"NEARBY SCAN / ON" : "NEARBY SCAN / OFF");
				break;
			}
			case OS0UICommandIntent::TOGGLE_ENVIRONMENT:
				InteractionMode().beginInteraction(
					OS0InteractionSurface::ENVIRONMENT);
				ApplyCursorTool(ContextAction::INSPECT);
				gUIRuntime.windowManager().show(
					gUIRuntime.managedId(FloatingPanelId::INSPECTOR));
				if (gInspectedGridNo >= 0 && gInspectedGridNo < WORLD_MAX)
					RefreshEnvironmentTarget(gInspectedGridNo, gInspectedLevel,
						gInspectedTileIndex);
				gUIRuntime.windowManager().toggle(
					gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
				break;
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
				if (selected) EnsureDebugFieldTools(selected);
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
				return InteractionMode().isNormal() &&
					InteractionMode().isSurfaceSelected(
						OS0InteractionSurface::ACTIONS);
			case ToolboxModule::CHARACTER:
				return gEquipmentExplodedVisible ||
					InteractionMode().isSurfaceActive(
						OS0InteractionSurface::EQUIPMENT);
			case ToolboxModule::STEALTH:
				return (GetSelectedMan() && GetSelectedMan()->bStealthMode) ||
					InteractionMode().isSurfaceActive(
						OS0InteractionSurface::BEHAVIOR);
			case ToolboxModule::OBJECT:
				return InteractionMode().nearbyScanEnabled();
			case ToolboxModule::WORLD:
				return InteractionMode().isSurfaceActive(
					OS0InteractionSurface::ENVIRONMENT);
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

	void DrawCommandIcon(size_t index, INT16 x, INT16 y)
	{
		if (index >= COMMAND_MODULE_COUNT) return;
		OS0UICommandDescriptor const& descriptor =
			GetOS0UICommandDescriptor(static_cast<OS0UICommand>(index));
		OS0UIAssets().draw(descriptor.icon, FRAME_BUFFER, x, y);
	}

	void DrawOrb()
	{
		const UINT16 black = Get16BPPColor(FROMRGB(2, 3, 3));
		const UINT16 dark = Get16BPPColor(FROMRGB(8, 10, 9));
		const UINT16 red = Get16BPPColor(FROMRGB(118, 0, 0));
		const UINT16 bright = Get16BPPColor(FROMRGB(205, 12, 12));
		// Paint the complete dock every frame. It lives below the world viewport,
		// so camera scrolling can never borrow stale pixels from it.
		ColorFillVideoSurfaceArea(FRAME_BUFFER, 0, gOrbY,
			gsVIEWPORT_END_X, SCREEN_HEIGHT - 1, black);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, 0, gOrbY,
			gsVIEWPORT_END_X, gOrbY, red);
		// The dock has become a single physical field-computer object.  All
		// non-world systems live in its movable OS window; gameplay actions stay on
		// characters and world assets.
		const BOOLEAN open = gUIRuntime.windowManager().visible(
			gUIRuntime.managedId(FloatingPanelId::TOOLBOX));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, 3, gOrbY + 3,
			COLLAPSED_OS0_W - 3, gOrbY + COMMAND_BAR_H - 3, black);
		OutlineBox(3, gOrbY + 3, COLLAPSED_OS0_W - 5,
			COMMAND_BAR_H - 5, open ? bright : red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, 10, gOrbY + 8,
			34, gOrbY + 27, dark);
		OutlineBox(9, gOrbY + 7, 27, 22, red);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, 7, gOrbY + 30,
			39, gOrbY + 32, open ? bright : red);
		OS0UIAssets().draw(OS0UIIcon::KEYRING, FRAME_BUFFER, 13, gOrbY + 9);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(open ? FONT_WHITE : FONT_MCOLOR_RED);
		MPrint(38, gOrbY + 14, "0");
		gOrbRegion.SetFastHelpText(
			"OS//0 TOOLBOX / LEFT: MODULES / RIGHT: MAP, TEAM, BASE, REPORT");
		if (!gTutorialActive && !gAimAutoCollapsed)
		{
			for (size_t i = 0; i < gPanelDockRegions.size(); ++i)
			{
				MOUSE_REGION& region = gPanelDockRegions[i];
				ToolboxModule const module = OS0CommandForDockSlot(i);
				const INT16 x = region.RegionTopLeftX;
				const INT16 w = region.W();
				const BOOLEAN hot = gusMouseXPos >= x &&
					gusMouseXPos <= region.RegionBottomRightX &&
					gusMouseYPos >= gOrbY;
				DrawCommandIcon(i + 1, x + std::max<INT16>(2, (w - 20) / 2),
					gOrbY + 8);
				if (hot || IsToolboxModuleActive(module))
					DrawIconCorners(x + 2, gOrbY + 3,
					std::max<INT16>(8, w - 4), COMMAND_BAR_H - 6, bright);
				else ColorFillVideoSurfaceArea(FRAME_BUFFER,
					x, gOrbY + 5, x, gOrbY + COMMAND_BAR_H - 6, dark);
				region.SetFastHelpText(GetOS0UICommandDescriptor(module).tooltip);
			}
		}
		InvalidateRegion(0, gOrbY, gsVIEWPORT_END_X, SCREEN_HEIGHT);
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
				TutorialText(15, 51,
					ST::format("> {}_", gCreatorModel.callsign()), FONT_WHITE);
				TutorialText(15, 69, "Type a name. ENTER or CONFIRM continues.");
				TutorialText(CONTINUE_X + 8, BAG_H - 17, "CONFIRM", FONT_MCOLOR_RED);
				break;
			case 2:
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
			case 3:
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
			case 4:
			case 5:
				TutorialText(15, 25, "LIVE CONTROL", FONT_MCOLOR_RED);
				TutorialText(15, 42, "LEFT: move / select       DOUBLE: inspect");
				TutorialText(15, 55, "RIGHT: context actions    MIDDLE: cycle action");
				TutorialText(15, 68, "SHIFT+MIDDLE: cancel / CTRL+MIDDLE: center");
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
			PositionLootRegions();
			const size_t slotCount = RefreshLootWorldItems();
			std::array<ST::string, 12> nextHelp;
			const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
			INT16 anchorX;
			INT16 anchorY;
			GetGridNoScreenPos(gLootGridNo, gLootLevel, &anchorX, &anchorY);
			OS0MapWorldToDisplayScreen(&anchorX, &anchorY);
			// The real container is already visible in the world. Do not paint a
			// scaled dark-backed duplicate over it.

			for (size_t slot = 0; slot < slotCount; ++slot)
			{
				const INT32 itemIndex = gLootWorldItems[slot];
				if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= gWorldItems.size())
					continue;
				WORLDITEM& worldItem = GetWorldItem(itemIndex);
				MOUSE_REGION const& region = gLootRegions[slot];
				const INT16 x = region.RegionTopLeftX;
				const INT16 y = region.RegionTopLeftY;
				const INT16 cx = x + 29;
				const INT16 cy = y + 14;
				const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 58 &&
					gusMouseYPos >= y && gusMouseYPos <= y + 27;
				if (hot) DrawIconCorners(x, y, 59, 28, red);
				DrawWorldItemSprite(worldItem.o, cx, cy);
				ItemModel const* const model = GCM->getItem(worldItem.o.usItem);
				nextHelp[slot] = ST::format("{} / {}% / x{}\n"
					"DOUBLE: PACK  RIGHT: OPTIONS  DRAG: MOVE",
					model->getName(), worldItem.o.bStatus[0],
					worldItem.o.ubNumberOfObjects);
			}
			SetLootRegionsEnabled(TRUE);
			for (size_t i = 0; i < gLootRegions.size(); ++i)
			{
				if (nextHelp[i] != gLootHelp[i])
				{
					gLootHelp[i] = nextHelp[i];
					gLootRegions[i].SetFastHelpText(nextHelp[i]);
				}
			}
			if (slotCount == 0)
			{
				// Empty containers collapse back into their world sprite. Never leave a
				// black pseudo-window or an EMPTY label floating over the map.
				gLootVisible = FALSE;
				SetBagRegionsEnabled(TRUE);
				SetRenderFlags(RENDER_FLAG_FULL);
				return;
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
		gLootWorldItems.fill(-1);
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
		PositionEquipmentRegions();
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
		if (gContextVisible || !gpItemPointer || !gItemTransferTarget ||
			!CanAccessSoldierContents(gItemTransferTarget)) return;
		PositionItemTransferIntentRegions();
		INT16 anchorX;
		INT16 anchorY;
		if (!GetActorDisplayAnchor(gItemTransferTarget, anchorX, anchorY)) return;
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
			const BOOLEAN allowed = ItemTransferIntentAllowed(gItemTransferTarget,
				gOS0ItemTransferIntents[i].intent);
			DrawIconCorners(x, y, 28, 28,
				!allowed ? Get16BPPColor(FROMRGB(42, 42, 42)) :
				(hot ? red : muted));
			OS0UIAssets().draw(gOS0ItemTransferIntents[i].icon,
				FRAME_BUFFER, x + 4, y + 4);
			const ST::string relation = ItemTransferIntentLabel(gItemTransferTarget,
				gOS0ItemTransferIntents[i].intent);
			const ST::string help = ST::format("{}\n{}", relation,
				allowed ? "CLICK TO APPLY" : "NOT COMPATIBLE");
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
		InvalidateRegion(dirtyLeft - 2, dirtyTop - 2,
			dirtyRight + 2, dirtyBottom + 2);
	}

	void DrawStackSplitDialog()
	{
		if (!gStackSplitVisible || !gStackSplitSoldier ||
			gStackSplitSlot < 0 || gStackSplitSlot >= NUM_INV_SLOTS) return;
		OBJECTTYPE const& object = gStackSplitSoldier->inv[gStackSplitSlot];
		if (object.usItem == NOTHING || object.ubNumberOfObjects <= 1)
		{
			CloseStackSplit();
			return;
		}
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
		const UINT16 category = ActionCategoryColour(
			ContextActionCategory(entry.action));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + width - 1,
			y + height - 1, dark);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + width - 1, y + 1,
			category);
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(x + 5, y + 5, ST::format("{} / {}",
			ActionCategoryName(ContextActionCategory(entry.action)),
			entry.label).left(47));
		SetFontForeground(entry.enabled ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		MPrint(x + 5, y + 18,
			ST::string(ContextActionExplanation(entry.action)).left(50));
		InvalidateRegion(x, y, x + width, y + height);
	}

	void DrawCharacterActionFan()
	{
		PositionContextRegions();
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
			PositionContextRegions();
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
		// Every OS//0 action uses one of the two spatial fan renderers. Keeping a
		// third rectangular fallback was the source of overlapping, unclickable
		// menus whenever a caller forgot to declare its interaction owner.
		gObjectActionFanVisible = TRUE;
		PositionContextRegions();
	}

	void DrawNearbyInteractionHints()
	{
		const BOOLEAN showHints = InteractionMode().nearbyScanEnabled() &&
			!gTutorialActive && !gContextVisible && !gAimAutoCollapsed &&
			!gAssetCatalogVisible && !gStackSplitVisible &&
			!gpItemPointer && !CarryState().active();
		const UINT16 red = Get16BPPColor(FROMRGB(205, 12, 12));
		const UINT16 muted = Get16BPPColor(FROMRGB(72, 58, 34));
		const UINT16 disabled = Get16BPPColor(FROMRGB(45, 45, 38));
		for (size_t i = 0; i < gNearbyHintRegions.size(); ++i)
		{
			MOUSE_REGION& region = gNearbyHintRegions[i];
			if (!showHints || i >= gNearbyHintCount)
			{
				if (region.uiFlags & MSYS_REGION_ENABLED) region.Disable();
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
				if (region.uiFlags & MSYS_REGION_ENABLED) region.Disable();
				continue;
			}
			MoveRegion(region, x, y);
			if (!(region.uiFlags & MSYS_REGION_ENABLED)) region.Enable();
			const BOOLEAN hot = gusMouseXPos >= x && gusMouseXPos <= x + 24 &&
				gusMouseYPos >= y && gusMouseYPos <= y + 24;
			if (hot || !hint.enabled)
				DrawIconCorners(x, y, 24, 24, hint.enabled ? red : disabled);
			else
				ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 2, y + 22,
					x + 21, y + 23, muted);
			DrawContextActionIcon(hint.action, x + 2, y + 1);
			const ST::string help = ST::format(
				"{} / {}\nCLICK: OBJECT ACTIONS / MMB: CYCLE\n{}",
				ContextActionName(hint.action), hint.enabled ? "READY" : "REQUIREMENT",
				ContextActionExplanation(hint.action));
			if (gNearbyHintHelp[i] != help)
			{
				gNearbyHintHelp[i] = help;
				region.SetFastHelpText(help);
			}
			InvalidateRegion(x - 2, y - 2, x + 26, y + 26);
		}
	}

	void DrawHoverInspector()
	{
		FloatingPanel const& panel =
			gFloatingPanels[static_cast<size_t>(FloatingPanelId::INSPECTOR)];
		if (!gUIRuntime.windowManager().visible(gUIRuntime.managedId(
			FloatingPanelId::INSPECTOR)) || !gHoverVisible || gTutorialActive ||
			gAimAutoCollapsed)
			return;
		gHoverX = panel.x;
		gHoverY = panel.y;
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
				"RMB OPTIONS / MMB CYCLE");
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

	BOOLEAN FinalizeWorldMove()
	{
		OS0CarryState& carry = CarryState();
		if (carry.source < 0 || carry.source >= WORLD_MAX ||
			carry.destination < 0 || carry.destination >= WORLD_MAX ||
			carry.destination == carry.source ||
			carry.tileIndex >= NUMBEROFTILES) return FALSE;

		STRUCTURE* const structure = WorldStructureAt(carry.source,
			carry.sourceLevel, carry.tileIndex);
		LEVELNODE* const sourceNode = WorldLevelNodeAt(carry.source,
			carry.sourceLevel, carry.tileIndex);
		if (!structure || !sourceNode || !structure->pDBStructureRef ||
			!OkayToAddStructureToWorld(carry.destination, carry.destinationLevel,
				structure->pDBStructureRef, INVALID_STRUCTURE_ID)) return FALSE;
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);

		try
		{
			// The destination is created before the source is touched. A rejected
			// placement therefore cannot eat the original crate or its contents.
			RestoreWorldMoveShade();
			LEVELNODE* const destinationNode =
				AddStructToTail(carry.destination, carry.tileIndex);
			if (!destinationNode) return FALSE;
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
			RemoveStructFromLevelNode(carry.source, sourceNode);
			MoveItemPools(carry.source, carry.destination);
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
				StartExplodedView(carry.destination, gInspectedTileIndex, TRUE);
			}
			if (gEnvironmentGridNo == carry.source)
				RefreshEnvironmentTarget(carry.destination, carry.destinationLevel,
					carry.tileIndex);
			// Heavy handling grows the attribute that was actually used. JA2's
			// StatChange stores sub-points in the merc profile, giving us the same
			// learn-by-doing loop as the rest of the campaign rather than a new XP bar.
			if (SOLDIERTYPE* const carrier = CarryCarrier())
			{
				const UINT16 practice = static_cast<UINT16>(std::clamp<INT32>(
					static_cast<INT32>(physics.massKg / 10.0f) +
					(carry.lifted ? 1 : 3), 2, 12));
				StatChange(*carrier, STRAMT, practice, FROM_SUCCESS);
				RecordFeedbackEvent(ST::format("{} {} KG / STR PRACTICE {}",
					CarryModeName(carry.mode),
					static_cast<INT32>(physics.massKg + 0.5f), practice));
				if (carry.mode == OS0CarryMode::THROW)
					OS0GetTacticalSession().state().pendingVisualEvents.push_back({
						carry.destination, OS0AssetMaterial::COMPOSITE, 2 });
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
		OS0CarryState& carry = CarryState();
		// JA2's interactive-tile hover restores its own shade when the cursor
		// leaves the source. Reassert the carry shade until placement/cancel.
		if (carry.active() && carry.sourceShaded)
		{
			if (LEVELNODE* const node = WorldLevelNodeAt(carry.source,
				carry.sourceLevel, carry.tileIndex)) node->ubShadeLevel = SHADE_MIN;
		}
		SOLDIERTYPE* const carrier = CarryCarrier();
		OS0CarryContinuationFacts const facts{
			carrier && carrier->bActive,
			carrier && carrier->bLife > 0,
			carrier && carrier->sSector == gWorldSector && !carrier->fBetweenSectors,
			carrier && carrier->bLevel == carry.sourceLevel,
			carrier && (!carry.walking() || carrier->sGridNo == carry.actionGrid ||
				carrier->sFinalDestination == carry.actionGrid)
		};
		OS0CarryCancelReason const cancellation =
			OS0ValidateCarryContinuation(carry, facts);
		if (cancellation != OS0CarryCancelReason::NONE)
		{
			RecordFeedbackEvent(ST::format("CARRY CANCELLED / REASON {}",
				static_cast<UINT8>(cancellation)));
			ClearWorldMoveState();
			return;
		}
		if (!carry.walking()) return;
		if (carrier->sGridNo != carry.actionGrid)
		{
			return;
		}

		if (!FinalizeWorldMove())
		{
			carry.phase = OS0CarryPhase::TARGETING;
			carry.destination = NOWHERE;
			carry.actionGrid = NOWHERE;
			CursorState().action = CarryModeAction(carry.mode);
			ShadeWorldMoveSource();
			guiPendingOverrideEvent = M_CHANGE_TO_HANDMODE;
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}
		ClearWorldMoveState();
		CursorState().action = ContextAction::MOVE;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
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
			DrawCarryGhost(gusMouseXPos, gusMouseYPos, FALSE);
			SetFont(TINYFONT1);
			SetFontBackground(FONT_MCOLOR_BLACK);
			SetFontForeground(FONT_MCOLOR_RED);
			MPrint(gusMouseXPos + 14, gusMouseYPos - 6, CarryModeName(carry.mode));
		}
		else if (carry.walking())
		{
			SOLDIERTYPE* const carrier = CarryCarrier();
			if (!carrier) return;
			INT16 carrierX;
			INT16 carrierY;
			if (!GetActorDisplayAnchor(carrier, carrierX, carrierY)) return;
			INT16 objectX = carrierX;
			INT16 objectY = carrierY;
			if (!carry.lifted)
			{
				// Heavy objects stay at the feet and slightly behind the actor. This is
				// stable across tile changes and reads as a continuous drag, not a copy
				// snapping back toward the original grid.
				objectX += 22;
				objectY += 8;
			}
			DrawCarryGhost(objectX, objectY, carry.lifted);
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

	void DrawImpactParticles()
	{
		const UINT32 now = GetJA2Clock();
		BOOLEAN active = FALSE;
		for (ImpactParticle& particle : gImpactParticles)
		{
			if (particle.born == 0) continue;
			const UINT32 age = now - particle.born;
			if (age > 720)
			{
				particle.born = 0;
				continue;
			}
			active = TRUE;
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
		if (active) SetRenderFlags(RENDER_FLAG_FULL);
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
		if (!gAssetLibrarySymbolSurface)
			gAssetLibrarySymbolSurface = AddVideoSurface(64, 64, PIXEL_DEPTH);
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
			gUIRuntime.windowManager().endDrag();
			SaveUILayout();
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
		if (gDebugAssetLibrarySector != gWorldSector.AsByte() ||
			gDebugAssetLibraryTileset != static_cast<INT16>(giCurrentTilesetID))
			RebuildDebugAssetLibrary();

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

	size_t BuildContextCursorActions(SOLDIERTYPE* target, GridNo gridNo,
		UINT8 level, UINT16 tileIndex,
		std::array<ContextAction, 12>& available)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
		const BOOLEAN hasItems = validGrid && GetItemPool(gridNo, level) != nullptr;
		const BOOLEAN hasAsset = validGrid && tileIndex < NUMBEROFTILES;
		STRUCTURE const* const structure = hasAsset ?
			WorldStructureAt(gridNo, level, tileIndex) : nullptr;
		const BOOLEAN openable = structure &&
			(structure->fFlags & STRUCTURE_OPENABLE) &&
			!(structure->fFlags & STRUCTURE_ANYDOOR);
		const BOOLEAN movable = hasAsset && IsWorldAssetMovableAt(gridNo,
			level, tileIndex, selected);
		const BOOLEAN armed = selected && selected->inv[HANDPOS].usItem != NOTHING &&
			GCM->getItem(selected->inv[HANDPOS].usItem)->isWeapon();
		OS0ActionFacts const facts{
			target != nullptr,
			target && target->bTeam == OUR_TEAM,
			target && (target->bTeam == ENEMY_TEAM || target->bTeam == CREATURE_TEAM),
			hasItems, openable, movable, hasAsset, armed
		};
		std::vector<ContextAction> resolved = ResolveOS0CursorActions(facts);
		if (!target && validGrid)
		{
			OS0EnvironmentActionFacts const environment = BuildEnvironmentFacts(
				gridNo, level, tileIndex, selected);
			auto const move = std::find(resolved.begin(), resolved.end(),
				ContextAction::MOVE);
			if (move != resolved.end()) resolved.erase(move);
			for (OS0ResolvedAction const& entry :
				ResolveOS0EnvironmentActions(environment))
			{
				if (!entry.enabled ||
					!(entry.action == ContextAction::DIG ||
					  entry.action == ContextAction::SALVAGE ||
					  OS0IsManipulationAction(entry.action))) continue;
				if (std::find(resolved.begin(), resolved.end(), entry.action) ==
					resolved.end()) resolved.push_back(entry.action);
			}
			resolved.push_back(ContextAction::MOVE);
		}
		size_t const count = std::min(available.size(), resolved.size());
		std::copy_n(resolved.begin(), count, available.begin());
		return count;
	}

	BOOLEAN DirectControlBlocked()
	{
		return gTutorialActive || gContextVisible || gStackSplitVisible ||
			gAssetCatalogVisible || gItemDetailsVisible || gFeedbackEditing ||
			OS0GetRealtimeEditorUI().active() ||
			gTacticalStatus.fAutoBandageMode || gpItemPointer ||
			CarryState().active() || gUIRuntime.windowManager().draggingWindow() !=
				OS0_INVALID_WINDOW;
	}

	void SynchronizeInteractionMode()
	{
		const BOOLEAN environmentVisible =
			gUIRuntime.windowManager().requestedVisible(gUIRuntime.managedId(
				FloatingPanelId::ENVIRONMENT));
		OS0InteractionFrameFacts facts;
		facts.tutorial = gTutorialActive;
		facts.fight = CursorState().attackMode || gCurrentUIMode == ACTION_MODE ||
			gCurrentUIMode == CONFIRM_ACTION_MODE;
		facts.context = gContextVisible;
		facts.environment = gLootVisible || gAssetCatalogVisible ||
			gGodLibraryVisible || environmentVisible ||
			OS0GetRealtimeEditorUI().active();
		facts.equipment = gEquipmentExplodedVisible || gBagVisible ||
			gStackSplitVisible || gItemDetailsVisible;
		facts.cursorAction = CursorState().action != ContextAction::MOVE;
		facts.cursorSurface = SurfaceForAction(CursorState().action);
		facts.passiveInteraction = gpItemPointer || CarryState().active() ||
			gTacticalStatus.fAutoBandageMode;
		InteractionMode().synchronize(facts);
	}

	void PrepareRealtimeEditorWorldSwap()
	{
		OS0WindowManager& windows = gUIRuntime.windowManager();
		windows.setSuspended(OS0WindowSuspendReason::WORLD_SWAP, TRUE);
		windows.cancelDrag();
		StopFeedbackEditing();
		if (gpItemPointer) CancelItemPointer();
		gItemTransferTarget = nullptr;
		ClearWorldMoveState();
		CloseContextMenu();
		OS0TacticalState& tactical = OS0GetTacticalSession().state();
		tactical.coverOrders.clear();
		tactical.pendingVisualEvents.clear();
		tactical.cursor = {};
		OS0ResetDirectControl();
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;

		gUIRuntime.hideTransientWorldPanels();
		gUIRuntime.hide(OS0UIPanel::ASSET_LIBRARY);
		gUIRuntime.hide(OS0UIPanel::ASSET_CATALOG);
		gLootVisible = FALSE;
		gEquipmentExplodedVisible = FALSE;
		gStackSplitVisible = FALSE;
		gItemDetailsVisible = FALSE;
		gHoverVisible = FALSE;
		gEquipmentSoldier = nullptr;
		gStackSplitSoldier = nullptr;
		gStackSplitSlot = NO_SLOT;
		gInventorySoldier = nullptr;
		gInspectedSoldier = nullptr;
		gContextSoldier = nullptr;
		gInspectedGridNo = NOWHERE;
		gInspectedTileIndex = NO_TILE;
		gLootGridNo = NOWHERE;
		gLootTileIndex = NO_TILE;
		gLootWorldItems.fill(-1);
		gEnvironmentGridNo = NOWHERE;
		gEnvironmentLevel = 0;
		gEnvironmentTileIndex = NO_TILE;
		gEnvironmentActorGridNo = NOWHERE;
		gNextEnvironmentRefreshAt = 0;
		gEnvironmentEntryCount = 0;
		gNearbyHintCount = 0;
		gNearbyScanWasEnabled = FALSE;
		gHoverCursorSoldier = nullptr;
		gHoverCursorGridNo = NOWHERE;
		gHoverCursorTileIndex = NO_TILE;
		gDebugAssetLibrary.clear();
		gDebugAssetLibrarySector = 0xff;
		gDebugAssetLibraryTileset = -1;
		for (ImpactParticle& particle : gImpactParticles) particle.born = 0;
		SetBagRegionsEnabled(TRUE);
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
		gInventorySoldier = GetSelectedMan();
		if (gInventorySoldier && gBagVisible)
			gInspectedSoldier = gInventorySoldier;
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
		if (worldSwap) PrepareRealtimeEditorWorldSwap();
		editor.update();
		BOOLEAN worldSwapSucceeded = FALSE;
		for (OS0EditorCommandResult const& result : editor.drainResults())
		{
			RecordFeedbackEvent(ST::format("EDITOR {} / {}",
				result.success ? "OK" : "ERROR", result.message));
			if ((result.type == OS0EditorCommandType::NEW_BLANK_MAP ||
				result.type == OS0EditorCommandType::LOAD_MAP) &&
				result.success)
				worldSwapSucceeded = TRUE;
		}
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
				if (gContentsMode == ContentsMode::WORLD) DrawLootMode();
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
			case OS0ManagedWindow::ITEM_DETAILS:
				// ITEM_DETAILS is retained as a persistence-compatible ID. Details
				// are projected through INSPECTOR; there is no duplicate window.
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
}


void InitializeOS0IngameUI()
{
	if (gInitialized) return;
	gUILayout.configure(SCREEN_WIDTH, SCREEN_HEIGHT, gsVIEWPORT_WINDOW_END_Y);
	gUIRuntime.windowManager().setWorkspace(
		{ 0, 0, static_cast<INT16>(SCREEN_WIDTH),
			gUILayout.workspaceBottom() });
	gUIRuntime.enterCampaign(
		OS0GetTacticalSession().state().creatorCompleted);
	if (gTutorialActive)
	{
		gCreatorModel.reset();
		gVideoScrollBeforeCreator = gfDoVideoScroll;
		gfDoVideoScroll = FALSE;
	}
	gInventoryVisible = FALSE;

	gOrbY = gUILayout.dock().y;
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
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, SlotCallback);
		gSlotRegions[i].SetUserData<0>(slot.slot);
	}
	for (size_t i = 0; i < gEquipmentRegions.size(); ++i)
	{
		MSYS_DefineRegion(&gEquipmentRegions[i], 0, 0, 34, 25,
			MSYS_PRIORITY_HIGHEST, CURSOR_NORMAL, MSYS_NO_CALLBACK, SlotCallback);
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
		MSYS_NO_CALLBACK, OrbCallback);
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

	OS0GetRealtimeEditorUI().initialize(gUIRuntime.windowManager(),
		gUIRuntime.managedId(OS0UIWindow::REALTIME_EDITOR));
	PositionBagRegions();
	// Input ownership is global to the OS//0 shell, not conditional on the
	// optional character window.  Tying all regions to gBagVisible left the dock
	// dead whenever a completed campaign correctly started with inventory closed.
	SetBagRegionsEnabled(TRUE);
	gInitialized = TRUE;
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
	SetRenderFlags(RENDER_FLAG_FULL);
}


void ShutdownOS0IngameUI()
{
	if (!gInitialized) return;
	SaveUILayout();
	OS0GetRealtimeEditorUI().shutdown();
	ClearWorldMoveState();
	CloseContextMenu();
	gHoverVisible = FALSE;
	if (gpItemPointer) CancelItemPointer();
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
	MSYS_RemoveRegion(&gStackSplitBlock);
	for (MOUSE_REGION& r : gStackSplitRegions) MSYS_RemoveRegion(&r);
	MSYS_RemoveRegion(&gOrbRegion);
	MSYS_RemoveRegion(&gTutorialContinue);
	for (MOUSE_REGION& r : gTutorialStats) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gTutorialTraitRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gOpsActionRegions) MSYS_RemoveRegion(&r);
	for (MOUSE_REGION& r : gLootRegions) MSYS_RemoveRegion(&r);
	OS0UIAssets().shutdown();
	if (gWorldZoomBuffer)
	{
		DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = nullptr;
	}
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
	gInspectedSoldier = nullptr;
	gInventorySoldier = nullptr;
	gInspectedGridNo = NOWHERE;
	gLootGridNo = NOWHERE;
	gInspectedTileIndex = NO_TILE;
	gLootTileIndex = NO_TILE;
	gInventoryVisible = FALSE;
	gLootVisible = FALSE;
	gEquipmentExplodedVisible = FALSE;
	gEquipmentSoldier = nullptr;
	gStackSplitVisible = FALSE;
	gStackSplitSoldier = nullptr;
	gStackSplitSlot = NO_SLOT;
	gLootDragCandidate = -1;
	gPanelInteractionGuardUntil = 0;
	gLootIgnoreInputUntil = 0;
	gAimAutoCollapsed = FALSE;
	gUIRuntime.windowManager().setSuspended(OS0WindowSuspendReason::AIM, FALSE);
	gFieldToolIssued = FALSE;
	gContentsMode = ContentsMode::SOLDIER;
	gGodLibraryVisible = FALSE;
	gUIRuntime.windowManager().cancelDrag();
	gAssetCatalogVisible = FALSE;
	gAssetCatalogReturnToLibrary = FALSE;
	gAssetCatalogNameEditing = FALSE;
	gHoverCursorSoldier = nullptr;
	gHoverCursorGridNo = NOWHERE;
	OS0ResetDirectControl();
	OS0GetTacticalSession().endTacticalSector();
	gDebugAssetLibrary.clear();
	gDebugAssetLibrarySector = 0xff;
	gDebugAssetLibraryTileset = -1;
	for (ImpactParticle& particle : gImpactParticles) particle.born = 0;
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
	// Editor callbacks only enqueue stable ids. This is the single tactical
	// frame boundary where those commands may mutate canonical JA2 world state.
	UpdateRealtimeEditorSession();
	OS0TacticalState& state = OS0GetTacticalSession().state();
	BOOLEAN inputRegionsDirty = FALSE;
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
	if (!gpItemPointer && gItemTransferTarget)
	{
		gItemTransferTarget = nullptr;
		inputRegionsDirty = TRUE;
	}
	fRenderRadarScreen = FALSE;
	if (inputRegionsDirty) SetBagRegionsEnabled(TRUE);
	gWindowMovedThisFrame = UpdateWindowDragging();
	if (!gTutorialActive && !gFieldToolIssued)
	{
		if (SOLDIERTYPE* const selected = GetSelectedMan())
		{
			EnsureDebugFieldTools(selected);
			gFieldToolIssued = TRUE;
		}
	}
	SynchronizeInteractionMode();
	if (!gTutorialActive)
	{
		OS0UpdateDirectControl(GetSelectedMan(), !DirectControlBlocked(),
			CursorState().attackMode);
		if (CursorState().attackMode && !gpItemPointer)
		{
			SOLDIERTYPE* const selected = GetSelectedMan();
			const BOOLEAN moving = selected &&
				(gAnimControl[selected->usAnimState].uiFlags & ANIM_MOVING);
			if (selected && !moving && gCurrentUIMode != ACTION_MODE &&
				gCurrentUIMode != CONFIRM_ACTION_MODE)
				guiPendingOverrideEvent = M_CHANGE_TO_ACTION;
		}
		const BOOLEAN aiming = !gContextVisible && !gpItemPointer &&
			(gCurrentUIMode == ACTION_MODE ||
			 gCurrentUIMode == CONFIRM_ACTION_MODE);
		if (aiming && !gAimAutoCollapsed)
		{
			StopFeedbackEditing();
			gUIRuntime.windowManager().setSuspended(
				OS0WindowSuspendReason::AIM, TRUE);
			gAimAutoCollapsed = TRUE;
			gStackSplitVisible = FALSE;
			gStackSplitSoldier = nullptr;
			gStackSplitSlot = NO_SLOT;
			gAssetCatalogNameEditing = FALSE;
			SetUIKeyboardHook(nullptr);
			CloseContextMenu();
			SetBagRegionsEnabled(TRUE);
			SetRenderFlags(RENDER_FLAG_FULL);
		}
		else if (!aiming && gAimAutoCollapsed)
		{
			gAimAutoCollapsed = FALSE;
			gUIRuntime.windowManager().setSuspended(
				OS0WindowSuspendReason::AIM, FALSE);
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
	UpdateNearbyInteractionHints();
	if (gLootVisible && !IsInspectedWorldAssetNear())
	{
		gLootVisible = FALSE;
		SetBagRegionsEnabled(TRUE);
	}
	if (!gTutorialActive && gInspectedGridNo == NOWHERE)
	{
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
		EmptyDialogueQueue();
		StopAnyCurrentlyTalkingSpeech();
	}

	// Simulation progresses once per tactical frame and is independent of panel
	// visibility. RenderOS0IngameUI only projects the resulting state.
	UpdateWorldMove();
	UpdateCoverCommands();
}


void RenderOS0IngameUI()
{
	if (!gInitialized) InitializeOS0IngameUI();
	if (!gTutorialActive && gBagVisible)
		CaptureAnimatedMercPreview(gInventorySoldier ?
			gInventorySoldier : GetSelectedMan());
	if (!gTutorialActive) DrawArtworkBrand();
	DrawWorldSelection();
	DrawNearbyInteractionHints();
	DrawImpactParticles();
	if (!gAimAutoCollapsed)
	{
		DrawActionMenu();
		// All registered windows now share one z-order. World-attached effects
		// stay below them; newly focused/modal windows therefore cannot be painted
		// underneath an older hard-coded draw call.
		DrawManagedWindows();
		// The held-item relation is a cursor projection rather than a window and
		// remains immediately visible above the selected destination. Modals keep
		// exclusive ownership of the frame.
		if (!gStackSplitVisible && !gAssetCatalogVisible)
			DrawItemTransferIntents();
		DrawOrb();
	}
	RefreshHeldItemCursor();

	// The tactical renderer uses dirty rectangles. A full refresh while moving
	// prevents the "hall of mirrors" trails visible in the previous prototype.
	const BOOLEAN managedWindowDragging = gWindowMovedThisFrame;
	const BOOLEAN equipmentMoving = gEquipmentExplodedVisible &&
		gEquipmentSoldier &&
		((gAnimControl[gEquipmentSoldier->usAnimState].uiFlags & ANIM_MOVING) ||
		 gfScrollPending || g_scroll_inertia);
	if (managedWindowDragging || CarryState().active() || equipmentMoving)
		SetRenderFlags(RENDER_FLAG_FULL);
}


BOOLEAN OS0CreatorIsActive()
{
	return gInitialized && gTutorialActive;
}

BOOLEAN OS0BlocksWorldInputAt(INT16 const screenX, INT16 const screenY)
{
	return gInitialized &&
		gUIRuntime.windowManager().blocksWorldInputAt(screenX, screenY);
}


void OS0OpenCharacterPanel(SOLDIERTYPE* soldier)
{
	if (!soldier || GetJA2Clock() < gPanelInteractionGuardUntil) return;
	InteractionMode().beginInteraction(OS0InteractionSurface::EQUIPMENT);
	CloseContextMenu();
	gInspectedSoldier = soldier;
	gContentsMode = ContentsMode::SOLDIER;
	gInspectedGridNo = NOWHERE;
	const BOOLEAN contentsAvailable = CanAccessSoldierContents(soldier);
	gMode = contentsAvailable ? ComputerMode::CONTENTS : ComputerMode::INFO;
	if (soldier->bTeam == OUR_TEAM)
	{
		gInventorySoldier = soldier;
		gUIRuntime.show(OS0UIPanel::INVENTORY);
	}
	gInventoryVisible = contentsAvailable;
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
			gInspectedSoldier = soldier;
			gInventorySoldier = soldier;
			gInspectedGridNo = NOWHERE;
			gContentsMode = ContentsMode::SOLDIER;
			gMode = ComputerMode::INFO;
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
		gItemDetailsVisible = FALSE;
		gInspectedSoldier = target;
		gInspectedGridNo = NOWHERE;
		gContentsMode = ContentsMode::SOLDIER;
		if (target->bTeam == OUR_TEAM) gInventorySoldier = target;
		gInventoryVisible = CanAccessSoldierContents(target);
		gMode = target->bTeam == OUR_TEAM ?
			ComputerMode::CONTENTS : ComputerMode::INFO;
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
	InteractionMode().beginInteraction(OS0InteractionSurface::ENVIRONMENT);
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
	RefreshEnvironmentTarget(gridNo, level, tileIndex);
	StartExplodedView(gridNo, tileIndex, TRUE);
	gLootGridNo = gridNo;
	gLootLevel = level;
	gLootTileIndex = tileIndex;
	gMode = ComputerMode::CONTENTS;
	gContentsMode = ContentsMode::WORLD;
	// Selection must not spawn a window underneath the first click. Otherwise
	// that new region swallows the second half of a double-click. Crucially, a
	// trailing LBUTTON_UP after a double-click is the *same* selection and must
	// not close the loot window that the double-click has just opened.
	if (!sameWorldSelection)
	{
		gItemDetailsVisible = FALSE;
		gLootVisible = FALSE;
	}
	CaptureInspectorPreview(gridNo, level);
	PositionBagRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
	return TRUE;
}

void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	const UINT16 heldItem = gpItemPointer ? gpItemPointer->usItem : NOTHING;
	const BOOLEAN cursorContextChanged = target != gHoverCursorSoldier ||
		gridNo != gHoverCursorGridNo || level != gHoverCursorLevel ||
		tileIndex != gHoverCursorTileIndex;
	const BOOLEAN heldItemChanged = heldItem != gHoverCursorHeldItem;
	const BOOLEAN wasHoverVisible = gHoverVisible;
	if (cursorContextChanged)
	{
		gHoverCursorSoldier = target;
		gHoverCursorGridNo = gridNo;
		gHoverCursorLevel = level;
		gHoverCursorTileIndex = tileIndex;
		gHoverSuggestedAction = ContextAction::COUNT;
		if (InteractionMode().nearbyScanEnabled() && !CarryState().active())
		{
			std::array<ContextAction, 12> actions{};
			if (BuildContextCursorActions(target, gridNo, level, tileIndex,
				actions) > 0)
				gHoverSuggestedAction = actions[0];
		}
	}
	// This relation is evaluated every frame, not only when the world hover key
	// changes. Starting an item drag while already over the actor used to miss the
	// transition completely and no transfer destinations appeared.
	if (gpItemPointer && target && CanAccessSoldierContents(target))
	{
		const BOOLEAN transferTargetChanged = gItemTransferTarget != target ||
			gEquipmentSoldier != target || !gEquipmentExplodedVisible;
		gItemTransferTarget = target;
		gEquipmentSoldier = target;
		gInventorySoldier = target;
		gEquipmentExplodedVisible = TRUE;
		if (transferTargetChanged)
		{
			PositionEquipmentRegions();
			PositionBagRegions();
			SetBagRegionsEnabled(TRUE);
		}
	}
	else if (gpItemPointer && gItemTransferTarget)
	{
		gItemTransferTarget = nullptr;
		SetBagRegionsEnabled(TRUE);
	}
	const BOOLEAN validGrid = gridNo >= 0 && gridNo < WORLD_MAX;
	ITEM_POOL* const pool = validGrid ? GetItemPool(gridNo, level) : nullptr;
	const BOOLEAN hasAsset = validGrid && tileIndex < NUMBEROFTILES;
	if (!target && !pool && !hasAsset)
	{
		if (!gInspectorPinned) gHoverVisible = FALSE;
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
	else if (pool && pool->iItemIndex >= 0 &&
		static_cast<size_t>(pool->iItemIndex) < gWorldItems.size())
	{
		const ContextAction displayAction =
			gHoverSuggestedAction != ContextAction::COUNT ?
			gHoverSuggestedAction : CursorState().action;
		gHoverDebugDetail.clear();
		WORLDITEM const& worldItem = GetWorldItem(pool->iItemIndex);
		gHoverTitle = worldItem.o.usItem != NOTHING ?
			GCM->getItem(worldItem.o.usItem)->getName() : "GROUND ITEMS";
		gHoverDetail = ST::format("{}  CLICK ICON / MMB CYCLE",
			ContextActionName(displayAction));
	}
	else
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

	// Content follows the hovered world object; the window does not. Keeping the
	// inspector at its user-chosen, persisted position prevents it from covering
	// the merc and eliminates the cursor-chasing jitter of the prototype.
	(void)screenX;
	(void)screenY;
	gHoverVisible = TRUE;
}

void OS0ClearWorldHover()
{
	if (gHoverVisible) SetRenderFlags(RENDER_FLAG_FULL);
	if (!gInspectorPinned) gHoverVisible = FALSE;
	gHoverCursorSoldier = nullptr;
	gHoverCursorGridNo = NOWHERE;
	gHoverCursorLevel = 0;
	gHoverCursorTileIndex = NO_TILE;
	gHoverCursorHeldItem = NOTHING;
	gHoverSuggestedAction = ContextAction::COUNT;
}

void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY)
{
	if (GetJA2Clock() < gPanelInteractionGuardUntil) return;
	InteractionMode().beginInteraction(OS0InteractionSurface::ACTIONS);
	CloseContextMenu();
	// RMB owns a transient interaction state. Suspend persistent aiming first;
	// otherwise the aim auto-collapse closes the newly created fan in this frame.
	if (CursorState().attackMode || gCurrentUIMode == ACTION_MODE ||
		gCurrentUIMode == CONFIRM_ACTION_MODE)
	{
		CursorState().attackMode = FALSE;
		CursorState().action = ContextAction::MOVE;
		guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
	}
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
		// Build the one radial interaction state directly. Opening the character
		// sheet first created a second, smaller menu for one frame underneath it.
		gInspectedSoldier = target;
		gInspectedGridNo = NOWHERE;
		gContentsMode = ContentsMode::SOLDIER;
		gInventoryVisible = CanAccessSoldierContents(target);
		if (target->bTeam == OUR_TEAM) gInventorySoldier = target;
		gLootVisible = FALSE;
		gCharacterActionFanVisible = TRUE;
		// A context fan is transient and must not destroy the independent RPG
		// inventory window. Its real slots remain valid drag targets behind/after it.
		gContextSoldier = target;
		gContextTitle = target->name;
		const BOOLEAN own = target->bTeam == OUR_TEAM;
		AddContextEntry(ContextAction::INSPECT, "INSPECT / INFO");
		AddContextEntry(ContextAction::CONTENTS,
			own ? "INVENTORY / EQUIPMENT" : "LOOT / CONTENTS",
			CanAccessSoldierContents(target));
		if (own)
		{
			AddContextEntry(ContextAction::TAKE_COVER,
				"AI / RUN TO COVER + STANCE");
			AddContextEntry(ContextAction::STAND, "STANCE / STAND");
			AddContextEntry(ContextAction::CROUCH, "STANCE / CROUCH");
			AddContextEntry(ContextAction::PRONE, "STANCE / PRONE");
			AddContextEntry(ContextAction::STEALTH,
				target->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON");
			AddContextEntry(ContextAction::AUTO_FIRST_AID, "AUTO FIRST AID",
				CanAutoBandage(FALSE));
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
		gObjectActionFanVisible = TRUE;
		gContextSoldier = nullptr;
		gContextGridNo = gridNo;
		gContextLevel = level;
		gContextTileIndex = tileIndex;
		ITEM_POOL* const pool = hasItems ? GetItemPool(gridNo, level) : nullptr;
		gContextWorldItemIndex = pool ? pool->iItemIndex : -1;
		gContextTitle = hasAsset ? DescribeWorldAsset(gridNo, level, tileIndex).displayName :
			(hasItems ? "GROUND ITEMS" : "WORLD ASSET");
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			level, tileIndex, selected);
		for (OS0ResolvedAction const& resolved :
			ResolveOS0EnvironmentActions(facts))
		{
			AddContextEntry(resolved.action,
				EnvironmentActionLabel(resolved.action, facts, gridNo, level,
					tileIndex), resolved.enabled);
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
		RefreshEnvironmentTarget(gridNo, level, tileIndex);
		gUIRuntime.windowManager().show(
			gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
	}
	else if (hasTerrain)
	{
		gObjectActionFanVisible = TRUE;
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
		CaptureInspectorPreview(gridNo, 0);
		OS0EnvironmentActionFacts const facts = BuildEnvironmentFacts(gridNo,
			0, NO_TILE, selected);
		for (OS0ResolvedAction const& resolved :
			ResolveOS0EnvironmentActions(facts))
			AddContextEntry(resolved.action,
				EnvironmentActionLabel(resolved.action, facts, gridNo, 0, NO_TILE),
				resolved.enabled);
		RefreshEnvironmentTarget(gridNo, 0, NO_TILE);
		gUIRuntime.windowManager().show(
			gUIRuntime.managedId(FloatingPanelId::ENVIRONMENT));
	}
	else if (selected)
	{
		gCharacterActionFanVisible = TRUE;
		gContextSoldier = selected;
		gContextGridNo = selected->sGridNo;
		gInspectedSoldier = selected;
		gInspectedGridNo = NOWHERE;
		gContextTitle = selected->name;
		AddContextEntry(ContextAction::INSPECT, "INSPECT / INFO");
		AddContextEntry(ContextAction::CONTENTS, "INVENTORY");
		AddContextEntry(ContextAction::TAKE_COVER,
			"AI / RUN TO COVER + STANCE");
		AddContextEntry(ContextAction::STAND, "STANCE / STAND");
		AddContextEntry(ContextAction::CROUCH, "STANCE / CROUCH");
		AddContextEntry(ContextAction::PRONE, "STANCE / PRONE");
		AddContextEntry(ContextAction::STEALTH,
			selected->bStealthMode ? "STEALTH / OFF" : "STEALTH / ON");
		AddContextEntry(ContextAction::AUTO_FIRST_AID, "AUTO FIRST AID",
			CanAutoBandage(FALSE));
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
	// CloseContextMenu clears stale hover state. Rebuild it explicitly from the
	// same resolved relation so RMB always opens the radial *and* its preview,
	// even when the mouse did not move between button-down and button-up.
	if (target || hasItems || hasAsset)
	{
		OS0HoverWorldObject(target, gridNo, level, tileIndex, screenX, screenY);
	}
	else if (hasTerrain)
	{
		CaptureInspectorPreview(gridNo, level);
		gHoverTitle = TerrainPhysicsName(GetTerrainType(gridNo));
		gHoverDetail = "GROUND / RMB ACTIONS / MMB CYCLE";
		gHoverDebugDetail = HasDiggingTool(selected) ?
			"FIELD SHOVEL READY" : "FIELD SHOVEL MISSING";
		gHoverVisible = TRUE;
	}
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
	PositionContextRegions();
	SetBagRegionsEnabled(TRUE);
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex)
{
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	std::array<ContextAction, 12> available{};
	const size_t count = BuildContextCursorActions(target, gridNo, level,
		tileIndex, available);
	if (count == 0) return;

	size_t current = count;
	for (size_t i = 0; i < count; ++i)
	{
		if (available[i] == CursorState().action)
		{
			current = i;
			break;
		}
	}
	ApplyCursorTool(available[current == count ? 0 : (current + 1) % count]);
	RecordFeedbackEvent(ST::format("CURSOR {}",
		ContextActionName(CursorState().action)));
}

void OS0CancelCursorAction()
{
	if (gpItemPointer)
	{
		CancelItemPointer();
		gItemTransferTarget = nullptr;
		RecordFeedbackEvent("HELD ITEM RETURNED TO INVENTORY");
	}
	CursorState().action = ContextAction::MOVE;
	CursorState().attackMode = FALSE;
	InteractionMode().selectSurface(OS0InteractionSurface::ACTIONS);
	if (!InteractionMode().nearbyScanEnabled()) InteractionMode().returnToNormal();
	ClearWorldMoveState();
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
	SetRenderFlags(RENDER_FLAG_FULL);
}

BOOLEAN OS0HandleRealtimeControlKey(UINT32 key, UINT32 keyState)
{
	if (key == SDLK_ESCAPE)
	{
		if (!CursorState().attackMode) return FALSE;
		OS0CancelCursorAction();
		return TRUE;
	}
	if (!OS0IsDirectControlKey(key)) return FALSE;
	// Alt/Ctrl combinations remain engine shortcuts. Shift is intentionally part
	// of direct control and promotes a standing movement segment to RUNNING. Its
	// own key-down is consumed as a control state, not routed as a legacy modifier.
	if (keyState & (ALT_DOWN | CTRL_DOWN)) return FALSE;
	const BOOLEAN enabled = !DirectControlBlocked() && GetSelectedMan();
	if (enabled)
		OS0UpdateDirectControl(GetSelectedMan(), TRUE,
			CursorState().attackMode);
	// OS0 owns these keys for the whole tactical session. A blocked context/modal
	// pauses movement but must not leak A/S/D/E back into unrelated JA2 shortcuts.
	return TRUE;
}

BOOLEAN OS0HandleHeldItemAction(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex)
{
	if (!gpItemPointer || !gpItemPointerSoldier) return FALSE;
	tileIndex = ResolveWorldTileIndex(gridNo, level, tileIndex);
	SOLDIERTYPE* const actor = gpItemPointerSoldier;

	// The actor is a relation target, not an implicit inventory bucket. Clicking
	// the actor keeps the item held and exposes valid body/hand/pack/drop actions;
	// the chosen symbol performs exactly one registered transfer.
	if (target && CanAccessSoldierContents(target))
	{
		gItemTransferTarget = target;
		gEquipmentSoldier = target;
		gInventorySoldier = target;
		gEquipmentExplodedVisible = TRUE;
		PositionEquipmentRegions();
		PositionItemTransferIntentRegions();
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
		RecordFeedbackEvent(ST::format("ITEM TARGET {} / CHOOSE RELATION",
			target->name));
		return TRUE;
	}

	if (gridNo < 0 || gridNo >= WORLD_MAX) return FALSE;
	const BOOLEAN hasAsset = tileIndex < NUMBEROFTILES;
	if (hasAsset)
	{
		const FieldToolKind required = RequiredFieldTool(gridNo, level, tileIndex);
		const BOOLEAN matchingTool = required != FieldToolKind::NONE &&
			gpItemPointer->usItem == FieldToolItem(required);
		if (matchingTool)
		{
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
		}

		STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
		if (structure && structure->fFlags & STRUCTURE_OPENABLE &&
			!(structure->fFlags & STRUCTURE_ANYDOOR))
		{
			AddItemToPool(gridNo, gpItemPointer, HIDDEN_IN_OBJECT, level, 0, -1);
			EndItemPointer();
			gItemTransferTarget = nullptr;
			OS0OpenWorldContainer(gridNo, level, tileIndex);
			return TRUE;
		}
	}

	// Bare soil accepts the field shovel even when no object-layer asset exists.
	if (gpItemPointer->usItem == CROWBAR && DigTerrainAt(actor, gridNo, tileIndex))
	{
		RecordFeedbackEvent(ST::format("HELD FIELD SHOVEL APPLIED AT {}", gridNo));
		SetRenderFlags(RENDER_FLAG_FULL);
		return TRUE;
	}
	return FALSE;
}

BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex)
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
	if (CursorState().action == ContextAction::MOVE)
	{
		// Deliberately yield to JA2's mature click-path owner. Hover no longer
		// replaces MOVE unless the explicit nearby-scan mode is active, so LMB can
		// once again select a full distant path instead of a one-tile OS0 action.
		if (!InteractionMode().nearbyScanEnabled()) InteractionMode().returnToNormal();
		return FALSE;
	}
	if (CursorState().action == ContextAction::ATTACK)
	{
		SOLDIERTYPE* const selected = GetSelectedMan();
		if (!selected) return TRUE;
		if (gCurrentUIMode == CONFIRM_ACTION_MODE)
		{
			guiPendingOverrideEvent = CA_MERC_SHOOT;
			return TRUE;
		}
		if (UIMouseOnValidAttackLocation(selected))
		{
			guiPendingOverrideEvent = A_CHANGE_TO_CONFIM_ACTION;
			return TRUE;
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
			(GetItemPool(gridNo, level) || tileIndex < NUMBEROFTILES))
		{
			OS0ActivateWorldObject(gridNo, level, tileIndex);
			return TRUE;
		}
		return FALSE;
	}
	if (CursorState().action == ContextAction::INSPECT)
	{
		const BOOLEAN hasInspectable = target ||
			(gridNo >= 0 && gridNo < WORLD_MAX &&
				(GetItemPool(gridNo, level) || tileIndex < NUMBEROFTILES));
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
	if (!selected || PythSpacesAway(selected->sGridNo, gridNo) > 2)
		return TRUE;

	if (!IsWorldAssetMovableAt(gridNo, level, tileIndex, selected))
	{
		return TRUE;
	}

	ClearWorldMoveState();
	OS0CarryState& carry = CarryState();
	OS0CarryMode const mode = CarryModeForAction(CursorState().action);
	if (!carry.begin(gridNo, level, tileIndex, Soldier2ID(selected), mode))
		return TRUE;
	if (STRUCTURE const* const moving = WorldStructureAt(gridNo, 0, tileIndex))
	{
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(moving);
		carry.lifted = physics.massKg <=
			GetSoldierWorldCarryCapacityKg(selected) * 0.55f;
	}
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
	STRUCTURE const* const structure = WorldStructureAt(gridNo, level, tileIndex);
	if (!structure || !(structure->fFlags & STRUCTURE_OPENABLE) ||
		structure->fFlags & STRUCTURE_ANYDOOR)
	{
		// Loose ground items are already physical world objects. They never open
		// the container projection; double-click picks the exact sprite up.
		return;
	}
	InteractionMode().beginInteraction(OS0InteractionSurface::ENVIRONMENT);
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
	gItemDetailsVisible = FALSE;
	StartExplodedView(gridNo, tileIndex, TRUE);
	gMode = ComputerMode::CONTENTS;
	gContentsMode = ContentsMode::WORLD;
	const BOOLEAN hasContents = GetItemPool(gridNo, level) != nullptr;
	gLootVisible = hasContents && IsInspectedWorldAssetNear();
	gInventoryVisible = TRUE;
	if (SOLDIERTYPE* const selected = GetSelectedMan())
	{
		// Loot remains a set of world sprites. The receiving destinations are the
		// real body equipment and pocket slots attached to the active operator.
		gInventorySoldier = selected;
		gEquipmentSoldier = selected;
		gEquipmentExplodedVisible = gLootVisible;
	}
	gPanelInteractionGuardUntil = GetJA2Clock() + 140;
	if (gLootVisible) gLootIgnoreInputUntil = GetJA2Clock() + 300;

	CaptureInspectorPreview(gridNo, level);
	RefreshLootWorldItems();
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
			{
				const BOOLEAN accepted = OS0CanAcceptCarriedObject(selected, worldItem.o);
				if (accepted)
					SoldierPickupItem(selected, item->iItemIndex, gridNo,
						ITEM_IGNORE_Z_LEVEL);
				else
					RecordFeedbackEvent("LOAD LIMIT 125% / PICKUP REJECTED");
				if (accepted)
				{
					gInspectedGridNo = NOWHERE;
					gInspectedTileIndex = NO_TILE;
					gLootVisible = FALSE;
				}
			}
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
		gMode = ComputerMode::INFO;
		SetBagRegionsEnabled(TRUE);
		SetRenderFlags(RENDER_FLAG_FULL);
	}
}


BOOLEAN OS0HandlePendingWorldMove(GridNo destination)
{
	OS0CarryState& carry = CarryState();
	if (!carry.pending()) return FALSE;
	if (carry.source < 0 || carry.source >= WORLD_MAX ||
		destination < 0 || destination >= WORLD_MAX ||
		destination == carry.source || carry.tileIndex >= NUMBEROFTILES)
	{
		return TRUE;
	}
	SOLDIERTYPE* const selected = CarryCarrier();
	if (!selected || !selected->bActive || selected->bLife <= 0)
	{
		ClearWorldMoveState();
		return TRUE;
	}

	STRUCTURE* const structure = WorldStructureAt(carry.source,
		carry.sourceLevel, carry.tileIndex);
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

	const INT16 sourceDistance = PythSpacesAway(selected->sGridNo, carry.source);
	const INT16 destinationDistance =
		PythSpacesAway(selected->sGridNo, destination);
	if (carry.mode == OS0CarryMode::PUSH)
	{
		const UINT8 away = GetDirectionFromGridNo(carry.source, selected);
		const GridNo required = NewGridNo(carry.source, DirectionInc(away));
		if (destination != required) return TRUE;
	}
	else if (carry.mode == OS0CarryMode::PULL)
	{
		if (PythSpacesAway(carry.source, destination) > 1 ||
			destinationDistance >= sourceDistance) return TRUE;
	}
	else if (carry.mode == OS0CarryMode::THROW)
	{
		WORLD_PHYSICS_PROFILE const physics = GetWorldPhysicsProfile(structure);
		const INT16 maxRange = static_cast<INT16>(std::clamp<INT32>(
			2 + selected->bStrength / 20 - static_cast<INT32>(physics.massKg / 15.0f),
			2, 8));
		if (!carry.lifted || PythSpacesAway(carry.source, destination) > maxRange)
			return TRUE;
	}

	// Validate before starting the walk. Invalid tiles leave the crate attached
	// to the cursor, so the player can simply choose another destination.
	if (!OkayToAddStructureToWorld(destination, carry.sourceLevel,
		structure->pDBStructureRef, INVALID_STRUCTURE_ID)) return TRUE;
	GridNo actionGrid = FindCarryActionGrid(selected, destination);
	if (carry.mode == OS0CarryMode::PUSH || carry.mode == OS0CarryMode::THROW)
		actionGrid = selected->sGridNo;
	else if (carry.mode == OS0CarryMode::PULL &&
		destination == selected->sGridNo)
	{
		const UINT8 awayFromSource = OppositeDirection(
			GetDirectionFromGridNo(carry.source, selected));
		actionGrid = NewGridNo(selected->sGridNo, DirectionInc(awayFromSource));
		if (actionGrid == selected->sGridNo ||
			!NewOKDestination(selected, actionGrid, TRUE, selected->bLevel))
			return TRUE;
	}
	if (actionGrid == NOWHERE) return TRUE;

	if (!carry.beginWalk(destination, carry.sourceLevel, actionGrid)) return TRUE;
	CursorState().action = ContextAction::MOVE;
	guiPendingOverrideEvent = A_CHANGE_TO_MOVE;

	if (selected->sGridNo != actionGrid &&
		!EVENT_InternalGetNewSoldierPath(selected, actionGrid,
			selected->usUIMovementMode, TRUE, TRUE))
	{
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
	if (gWorldZoomBuffer)
	{
		DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = nullptr;
	}
	SetRenderFlags(RENDER_FLAG_FULL);
	InvalidateScreen();
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
	if (gWorldZoomBuffer &&
		gWorldZoomBuffer->Width() == destination.w &&
		gWorldZoomBuffer->Height() == destination.h)
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
		gWorldZoomBuffer->Height() != destination.h)
	{
		if (gWorldZoomBuffer) DeleteVideoSurface(gWorldZoomBuffer);
		gWorldZoomBuffer = AddVideoSurface(destination.w, destination.h, PIXEL_DEPTH);
	}
	const SGPBox viewport{ destination.x, destination.y,
		destination.w, destination.h };
	BltVideoSurface(gWorldZoomBuffer, FRAME_BUFFER, 0, 0, &viewport);
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
	return std::min<INT16>(gsVIEWPORT_WINDOW_END_Y,
		std::max<INT16>(0, SCREEN_HEIGHT - OS0UILayout::DOCK_HEIGHT));
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
