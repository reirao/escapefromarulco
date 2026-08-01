#pragma once

#include "JA2Types.h"
#include "OS0_FixedList.h"
#include "OS0_UIAssetManager.h"

#include <cstdint>

enum class ContextAction : UINT8
{
	MOVE,
	USE,
	INSPECT,
	CONTENTS,
	BUILD,
	CARRY,
	PUSH,
	PULL,
	THROW,
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
	CATALOG,
	TAKE_COVER,
	AUTO_FIRST_AID,
	PREVIOUS_SQUAD,
	NEXT_SQUAD,
	TEAM,
	END_TURN,
	GOD_ASSETS,
	GOD_EDITOR,
	GOD_ICONS,
	GOD_TOOLS,
	GOD_REVIVE,
	COUNT
};

enum class OS0CursorMode : UINT8
{
	NONE,
	MOVE,
	HAND,
	LOOK,
	TALK,
	ATTACK
};

enum class ActionCategory : UINT8
{
	INFO,
	GEAR,
	MOVEMENT,
	STANCE,
	COMBAT,
	MEDICAL,
	SOCIAL,
	WORLD,
	GROUP,
	DEBUG,
	COUNT
};

struct ActionCategoryDescriptor
{
	ActionCategory category;
	const char* name;
	const char* explanation;
	OS0UIIcon icon;
};

struct ContextActionDescriptor
{
	ContextAction action;
	const char* name;
	ActionCategory category;
	const char* explanation;
	OS0UIIcon icon;
	INT16 priority = 100;
	OS0CursorMode cursor = OS0CursorMode::NONE;
};

ContextActionDescriptor const& GetContextActionDescriptor(ContextAction action);
ActionCategoryDescriptor const& GetActionCategoryDescriptor(
	ActionCategory category);
const char* ContextActionName(ContextAction action);
ActionCategory ContextActionCategory(ContextAction action);
const char* ActionCategoryName(ActionCategory category);
const char* ContextActionExplanation(ContextAction action);
OS0UIIcon ContextActionIcon(ContextAction action);
INT16 ContextActionPriority(ContextAction action);
OS0CursorMode ContextActionCursor(ContextAction action);

struct OS0ActionFacts
{
	BOOLEAN hasTarget = FALSE;
	BOOLEAN ownTarget = FALSE;
	BOOLEAN hostileTarget = FALSE;
	BOOLEAN hasItems = FALSE;
	BOOLEAN openable = FALSE;
	BOOLEAN movable = FALSE;
	BOOLEAN hasAsset = FALSE;
	BOOLEAN armed = FALSE;
};

struct OS0EnvironmentActionFacts
{
	BOOLEAN actorAvailable = FALSE;
	BOOLEAN hasAsset = FALSE;
	BOOLEAN hasItems = FALSE;
	BOOLEAN openable = FALSE;
	BOOLEAN terrain = FALSE;
	BOOLEAN near = FALSE;
	// Opening, looting and tool use permit the engine's normal two-tile reach.
	// Physical manipulation starts only while hands are actually adjacent.
	BOOLEAN manipulationNear = FALSE;
	BOOLEAN moveCandidate = FALSE;
	BOOLEAN canMove = FALSE;
	BOOLEAN canThrow = FALSE;
	BOOLEAN salvageable = FALSE;
	BOOLEAN canSalvage = FALSE;
	BOOLEAN diggableSurface = FALSE;
	BOOLEAN canDig = FALSE;
	BOOLEAN buildable = FALSE;
	BOOLEAN debugCatalog = FALSE;
};

enum class OS0InteractionTargetKind : UINT8
{
	NONE,
	ACTOR,
	WORLD_ITEM,
	WORLD_ASSET,
	TERRAIN
};

struct OS0ActionBinding
{
	OS0InteractionTargetKind kind = OS0InteractionTargetKind::NONE;
	INT32 actorId = -1;
	GridNo gridNo = -1;
	UINT8 level = 0;
	UINT16 tileIndex = 0xffff;
	INT32 worldItemIndex = -1;
	// Ephemeral instance guards. Numeric engine IDs and vector slots are reused;
	// deferred actions must prove that the target is still the same object.
	UINT32 actorInstanceId = 0;
	UINT16 worldItemType = 0xffff;
	INT8 worldItemVisibility = 0;
	UINT16 worldItemFlags = 0;
	INT8 worldItemRenderZHeight = 0;
	std::uint64_t worldItemFingerprint = 0;
	// Structures have a native session identity. Object/terrain LEVELNODEs do
	// not, so their pointer snapshot is valid only within the captured monotonic
	// world revision and is rejected after any OS0/editor world mutation.
	BOOLEAN assetHasStructure = FALSE;
	UINT16 assetStructureId = 0;
	GridNo assetBaseGridNo = -1;
	UINT32 worldRevision = 0;
	UINT16 terrainTileIndex = 0xffff;
	std::uintptr_t assetInstance = 0;

	bool operator==(OS0ActionBinding const& rhs) const noexcept
	{
		return kind == rhs.kind && actorId == rhs.actorId &&
			gridNo == rhs.gridNo && level == rhs.level &&
			tileIndex == rhs.tileIndex &&
			worldItemIndex == rhs.worldItemIndex &&
			actorInstanceId == rhs.actorInstanceId &&
			worldItemType == rhs.worldItemType &&
			worldItemVisibility == rhs.worldItemVisibility &&
			worldItemFlags == rhs.worldItemFlags &&
			worldItemRenderZHeight == rhs.worldItemRenderZHeight &&
			worldItemFingerprint == rhs.worldItemFingerprint &&
			assetHasStructure == rhs.assetHasStructure &&
			assetStructureId == rhs.assetStructureId &&
			assetBaseGridNo == rhs.assetBaseGridNo &&
			worldRevision == rhs.worldRevision &&
			terrainTileIndex == rhs.terrainTileIndex &&
			assetInstance == rhs.assetInstance;
	}
	bool operator!=(OS0ActionBinding const& rhs) const noexcept
	{
		return !(*this == rhs);
	}
};

enum class OS0ActionApproach : UINT8
{
	IMMEDIATE,
	MOVE_TO_RANGE,
	IMPOSSIBLE
};

enum class OS0ActionBlockReason : UINT8
{
	NONE,
	NO_ACTOR,
	MISSING_TOOL,
	TOO_HEAVY,
	INVALID_TARGET,
	UNAVAILABLE
};

struct OS0ResolvedAction
{
	ContextAction action = ContextAction::INSPECT;
	BOOLEAN enabled = FALSE;
	OS0ActionApproach approach = OS0ActionApproach::IMMEDIATE;
	OS0ActionBlockReason blockReason = OS0ActionBlockReason::NONE;
	OS0ActionBinding binding{};
	INT16 score = 0;
};

// Context actions are resolved from the pointer every time its target changes.
// The complete environment relation currently contains at most twelve unique
// actions, so keeping the result inline avoids a heap allocation in this hot
// hover/input path.
using OS0ResolvedActionList = OS0FixedList<OS0ResolvedAction, 12>;

struct OS0InteractionContext
{
	OS0ActionFacts cursor{};
	OS0EnvironmentActionFacts environment{};
	OS0ActionBinding target{};
	BOOLEAN hasEnvironment = FALSE;
};

// Authoritative relation resolver. Hover, F, RMB, MMB, environment panels and
// direct execution all consume this same ordered result. Each action keeps the
// exact target it was resolved for and declares whether it is immediate,
// requires an approach path, or is impossible.
OS0ResolvedActionList ResolveOS0InteractionActions(
	OS0InteractionContext const& context);
OS0ResolvedAction const* FindOS0ResolvedAction(
	OS0ResolvedActionList const& actions, ContextAction action) noexcept;
OS0ResolvedAction const* PrimaryOS0InteractionAction(
	OS0ResolvedActionList const& actions) noexcept;
const char* OS0ActionBlockReasonName(OS0ActionBlockReason reason) noexcept;
BOOLEAN OS0IsManipulationAction(ContextAction action) noexcept;
