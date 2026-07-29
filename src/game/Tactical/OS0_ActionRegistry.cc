/*
 * Escape from Arulco: authoritative contextual-action registry.
 * UI code renders these descriptors but does not redefine their semantics.
 */

#include "OS0_ActionRegistry.h"

#include <array>
#include <algorithm>
#include <cstddef>

namespace
{
	constexpr std::array<ContextActionDescriptor,
		static_cast<size_t>(ContextAction::COUNT)> ACTIONS{{
		{ ContextAction::MOVE, "MOVE", ActionCategory::MOVEMENT,
			"Return to movement and direct-control navigation.", 3, FALSE,
			1000, OS0CursorMode::MOVE },
		{ ContextAction::USE, "USE / LOOT", ActionCategory::WORLD,
			"Use the target or expose its physical contents.", 18, FALSE,
			10, OS0CursorMode::HAND },
		{ ContextAction::INSPECT, "INSPECT", ActionCategory::INFO,
			"Read identity, condition and material data.", 12, FALSE,
			30, OS0CursorMode::LOOK },
		{ ContextAction::CONTENTS, "CONTENTS", ActionCategory::GEAR,
			"Expose real body, pocket or container slots.", 18, FALSE },
		{ ContextAction::BUILD, "BUILD", ActionCategory::WORLD,
			"Open blueprint, material and placement controls.", 42, FALSE },
		{ ContextAction::CARRY, "CARRY", ActionCategory::MOVEMENT,
			"Lift or drag the asset using strength and mass.", 3, FALSE,
			20, OS0CursorMode::HAND },
		{ ContextAction::TALK, "TALK", ActionCategory::SOCIAL,
			"Start a conversation with this contact.", 21, FALSE,
			10, OS0CursorMode::TALK },
		{ ContextAction::ATTACK, "ATTACK", ActionCategory::COMBAT,
			"Aim weapon; WASD moves and the mouse controls facing.", 24, FALSE,
			5, OS0CursorMode::ATTACK },
		{ ContextAction::STAND, "STAND", ActionCategory::STANCE,
			"Adopt standing stance for speed and vision.", 3, FALSE },
		{ ContextAction::CROUCH, "CROUCH", ActionCategory::STANCE,
			"Lower profile while retaining mobility.", 6, FALSE },
		{ ContextAction::PRONE, "PRONE", ActionCategory::STANCE,
			"Go prone for the smallest exposed profile.", 9, FALSE },
		{ ContextAction::STEALTH, "STEALTH", ActionCategory::STANCE,
			"Toggle quiet tactical movement.", 0, FALSE },
		{ ContextAction::WEAPON_MODE, "WEAPON_MODE", ActionCategory::COMBAT,
			"Cycle the equipped weapon's firing mode.", 27, FALSE },
		{ ContextAction::RELOAD, "RELOAD", ActionCategory::COMBAT,
			"Load compatible ammunition into the weapon.", 30, FALSE },
		{ ContextAction::SWAP_HANDS, "SWAP_HANDS", ActionCategory::GEAR,
			"Exchange primary and secondary hand items.", 33, FALSE },
		{ ContextAction::UNLOAD, "UNLOAD", ActionCategory::COMBAT,
			"Remove the current magazine as a real item.", 36, FALSE },
		{ ContextAction::DETAILS, "DETAILS", ActionCategory::INFO,
			"Inspect condition, ammunition and attachments.", 15, FALSE },
		{ ContextAction::EQUIP_ITEM, "EQUIP_ITEM", ActionCategory::GEAR,
			"Move item to its matching body or hand slot.", 18, FALSE },
		{ ContextAction::MOVE_ITEM, "MOVE_ITEM", ActionCategory::GEAR,
			"Take item on the cursor for direct placement.", 21, FALSE },
		{ ContextAction::PICK_UP, "PICK_UP", ActionCategory::GEAR,
			"Move item from world or container into carried slots.", 39, FALSE },
		{ ContextAction::DIG, "DIG", ActionCategory::WORLD,
			"Use a shovel to remove the current ground layer.", 42, FALSE },
		{ ContextAction::SALVAGE, "SALVAGE", ActionCategory::WORLD,
			"Dismantle asset into registered materials.", 0, TRUE },
		{ ContextAction::CATALOG, "CATALOG", ActionCategory::DEBUG,
			"Classify this asset in the sandbox database.", 12, FALSE },
		{ ContextAction::TAKE_COVER, "TAKE_COVER", ActionCategory::MOVEMENT,
			"AI runs to nearby cover and lowers stance.", 6, FALSE },
		{ ContextAction::AUTO_FIRST_AID, "AUTO_FIRST_AID", ActionCategory::MEDICAL,
			"Treat bleeding allies; Escape aborts treatment.", 36, FALSE }
	}};

	constexpr std::array<const char*, static_cast<size_t>(ActionCategory::COUNT)>
		CATEGORY_NAMES{{
			"INFO", "GEAR", "MOVE", "STANCE", "COMBAT",
			"MEDICAL", "SOCIAL", "WORLD", "DEBUG"
		}};
}

ContextActionDescriptor const& GetContextActionDescriptor(ContextAction action)
{
	const size_t index = static_cast<size_t>(action);
	return ACTIONS[index < ACTIONS.size() ? index : 0];
}

const char* ContextActionName(ContextAction action)
{
	return GetContextActionDescriptor(action).name;
}

ActionCategory ContextActionCategory(ContextAction action)
{
	return GetContextActionDescriptor(action).category;
}

const char* ActionCategoryName(ActionCategory category)
{
	const size_t index = static_cast<size_t>(category);
	return index < CATEGORY_NAMES.size() ? CATEGORY_NAMES[index] : "ACTION";
}

const char* ContextActionExplanation(ContextAction action)
{
	return GetContextActionDescriptor(action).explanation;
}

UINT16 ContextActionIconFrame(ContextAction action)
{
	return GetContextActionDescriptor(action).iconFrame;
}

BOOLEAN ContextActionUsesDoorIcons(ContextAction action)
{
	return GetContextActionDescriptor(action).usesDoorIcons;
}

INT16 ContextActionPriority(ContextAction action)
{
	return GetContextActionDescriptor(action).priority;
}

OS0CursorMode ContextActionCursor(ContextAction action)
{
	return GetContextActionDescriptor(action).cursor;
}

std::vector<ContextAction> ResolveOS0CursorActions(OS0ActionFacts const& facts)
{
	std::vector<ContextAction> actions;
	auto add = [&](ContextAction action)
	{
		if (std::find(actions.begin(), actions.end(), action) == actions.end())
			actions.push_back(action);
	};

	if (facts.hasTarget)
	{
		if (facts.ownTarget)
		{
			add(ContextAction::USE);
			add(ContextAction::INSPECT);
		}
		else
		{
			if (facts.hostileTarget && facts.armed) add(ContextAction::ATTACK);
			if (!facts.hostileTarget) add(ContextAction::TALK);
			add(ContextAction::INSPECT);
			if (!facts.hostileTarget && facts.armed) add(ContextAction::ATTACK);
		}
	}
	else if (facts.hasItems || facts.openable)
	{
		add(ContextAction::USE);
		add(ContextAction::INSPECT);
		if (facts.movable) add(ContextAction::CARRY);
	}
	else if (facts.movable)
	{
		add(ContextAction::CARRY);
		add(ContextAction::INSPECT);
		add(ContextAction::USE);
	}
	else if (facts.hasAsset)
	{
		add(ContextAction::INSPECT);
		if (facts.armed) add(ContextAction::ATTACK);
	}
	add(ContextAction::MOVE);
	return actions;
}
