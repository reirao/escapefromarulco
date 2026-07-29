#pragma once

#include "JA2Types.h"

#include <vector>

enum class ContextAction : UINT8
{
	MOVE,
	USE,
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
	CATALOG,
	TAKE_COVER,
	AUTO_FIRST_AID,
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
	DEBUG,
	COUNT
};

struct ContextActionDescriptor
{
	ContextAction action;
	const char* name;
	ActionCategory category;
	const char* explanation;
	UINT16 iconFrame;
	BOOLEAN usesDoorIcons;
	INT16 priority = 100;
	OS0CursorMode cursor = OS0CursorMode::NONE;
};

ContextActionDescriptor const& GetContextActionDescriptor(ContextAction action);
const char* ContextActionName(ContextAction action);
ActionCategory ContextActionCategory(ContextAction action);
const char* ActionCategoryName(ActionCategory category);
const char* ContextActionExplanation(ContextAction action);
UINT16 ContextActionIconFrame(ContextAction action);
BOOLEAN ContextActionUsesDoorIcons(ContextAction action);
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
