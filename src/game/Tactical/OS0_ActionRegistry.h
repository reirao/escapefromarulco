#pragma once

#include "JA2Types.h"
#include "OS0_UIAssetManager.h"

#include <vector>

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

// One ordered resolution is projected by hover, MMB and the cursor/tool UI.
std::vector<ContextAction> ResolveOS0CursorActions(OS0ActionFacts const& facts);

struct OS0EnvironmentActionFacts
{
	BOOLEAN hasAsset = FALSE;
	BOOLEAN hasItems = FALSE;
	BOOLEAN openable = FALSE;
	BOOLEAN terrain = FALSE;
	BOOLEAN near = FALSE;
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

struct OS0ResolvedAction
{
	ContextAction action = ContextAction::INSPECT;
	BOOLEAN enabled = FALSE;
};

// Full object relation consumed by the object fan, proximity hints and the
// environment-skills window. UI surfaces must not invent their own capability
// lists.
std::vector<OS0ResolvedAction> ResolveOS0EnvironmentActions(
	OS0EnvironmentActionFacts const& facts);
BOOLEAN OS0IsManipulationAction(ContextAction action) noexcept;
