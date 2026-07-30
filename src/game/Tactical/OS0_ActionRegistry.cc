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
			"Return to movement and direct-control navigation.", OS0UIIcon::WALK,
			1000, OS0CursorMode::MOVE },
		{ ContextAction::USE, "USE / LOOT", ActionCategory::WORLD,
			"Use the target or expose its physical contents.", OS0UIIcon::HAND,
			10, OS0CursorMode::HAND },
		{ ContextAction::INSPECT, "INSPECT", ActionCategory::INFO,
			"Read identity, condition and material data.", OS0UIIcon::LOOK,
			30, OS0CursorMode::LOOK },
		{ ContextAction::CONTENTS, "CONTENTS", ActionCategory::GEAR,
			"Expose real body, pocket or container slots.", OS0UIIcon::OPEN },
		{ ContextAction::BUILD, "BUILD", ActionCategory::WORLD,
			"Open blueprint, material and placement controls.", OS0UIIcon::TOOLKIT },
		{ ContextAction::CARRY, "CARRY", ActionCategory::MOVEMENT,
			"Lift the asset and choose a valid placement tile.", OS0UIIcon::HAND,
			20, OS0CursorMode::HAND },
		{ ContextAction::PUSH, "PUSH", ActionCategory::MOVEMENT,
			"Push the asset one tile while staying behind it.", OS0UIIcon::PUNCH,
			21, OS0CursorMode::HAND },
		{ ContextAction::PULL, "PULL", ActionCategory::MOVEMENT,
			"Pull the asset while the operator moves backward.", OS0UIIcon::CROWBAR,
			22, OS0CursorMode::HAND },
		{ ContextAction::THROW, "THROW", ActionCategory::MOVEMENT,
			"Throw a light asset within the strength-limited range.", OS0UIIcon::EXPLOSIVE,
			23, OS0CursorMode::HAND },
		{ ContextAction::TALK, "TALK", ActionCategory::SOCIAL,
			"Start a conversation with this contact.", OS0UIIcon::TALK,
			10, OS0CursorMode::TALK },
		{ ContextAction::ATTACK, "ATTACK", ActionCategory::COMBAT,
			"Aim weapon; WASD moves and the mouse controls facing.", OS0UIIcon::TARGET,
			5, OS0CursorMode::ATTACK },
		{ ContextAction::STAND, "STAND", ActionCategory::STANCE,
			"Adopt standing stance for speed and vision.", OS0UIIcon::WALK },
		{ ContextAction::CROUCH, "CROUCH", ActionCategory::STANCE,
			"Lower profile while retaining mobility.", OS0UIIcon::SNEAK },
		{ ContextAction::PRONE, "PRONE", ActionCategory::STANCE,
			"Go prone for the smallest exposed profile.", OS0UIIcon::CRAWL },
		{ ContextAction::STEALTH, "STEALTH", ActionCategory::STANCE,
			"Toggle quiet tactical movement.", OS0UIIcon::SNEAK },
		{ ContextAction::WEAPON_MODE, "WEAPON_MODE", ActionCategory::COMBAT,
			"Cycle the equipped weapon's firing mode.", OS0UIIcon::TARGET },
		{ ContextAction::RELOAD, "RELOAD", ActionCategory::COMBAT,
			"Load compatible ammunition into the weapon.", OS0UIIcon::TARGET },
		{ ContextAction::SWAP_HANDS, "SWAP_HANDS", ActionCategory::GEAR,
			"Exchange primary and secondary hand items.", OS0UIIcon::HAND },
		{ ContextAction::UNLOAD, "UNLOAD", ActionCategory::COMBAT,
			"Remove the current magazine as a real item.", OS0UIIcon::HAND },
		{ ContextAction::DETAILS, "DETAILS", ActionCategory::INFO,
			"Inspect condition, ammunition and attachments.", OS0UIIcon::EXAMINE },
		{ ContextAction::EQUIP_ITEM, "EQUIP_ITEM", ActionCategory::GEAR,
			"Move item to its matching body or hand slot.", OS0UIIcon::HAND },
		{ ContextAction::MOVE_ITEM, "MOVE_ITEM", ActionCategory::GEAR,
			"Take item on the cursor for direct placement.", OS0UIIcon::WALK },
		{ ContextAction::PICK_UP, "PICK_UP", ActionCategory::GEAR,
			"Move item from world or container into carried slots.", OS0UIIcon::HAND },
		{ ContextAction::DIG, "DIG", ActionCategory::WORLD,
			"Use a shovel to remove the current ground layer.", OS0UIIcon::TOOLKIT,
			24, OS0CursorMode::HAND },
		{ ContextAction::SALVAGE, "SALVAGE", ActionCategory::WORLD,
			"Dismantle asset into registered materials.", OS0UIIcon::CROWBAR,
			25, OS0CursorMode::HAND },
		{ ContextAction::CATALOG, "CATALOG", ActionCategory::DEBUG,
			"Classify this asset in the sandbox database.", OS0UIIcon::KEYRING },
		{ ContextAction::TAKE_COVER, "TAKE_COVER", ActionCategory::MOVEMENT,
			"AI runs to nearby cover and lowers stance.", OS0UIIcon::CRAWL },
		{ ContextAction::AUTO_FIRST_AID, "AUTO_FIRST_AID", ActionCategory::MEDICAL,
			"Treat bleeding allies; Escape aborts treatment.", OS0UIIcon::FIRST_AID },
		{ ContextAction::PREVIOUS_SQUAD, "PREVIOUS_SQUAD", ActionCategory::GROUP,
			"Select the previous active squad in the current tactical sector.",
			OS0UIIcon::WALK },
		{ ContextAction::NEXT_SQUAD, "NEXT_SQUAD", ActionCategory::GROUP,
			"Select the next active squad in the current tactical sector.",
			OS0UIIcon::RUN },
		{ ContextAction::TEAM, "TEAM", ActionCategory::GROUP,
			"Open team selection and squad-management actions.", OS0UIIcon::TALK },
		{ ContextAction::END_TURN, "END_TURN", ActionCategory::GROUP,
			"Commit the squad's actions and hand control to the next turn.",
			OS0UIIcon::CANCEL },
		{ ContextAction::GOD_ASSETS, "GOD_ASSETS", ActionCategory::DEBUG,
			"Open the complete world-asset catalogue and classification tools.",
			OS0UIIcon::KEYRING },
		{ ContextAction::GOD_EDITOR, "GOD_EDITOR", ActionCategory::DEBUG,
			"Open live terrain, structure and entity editing tools.",
			OS0UIIcon::TOOLKIT },
		{ ContextAction::GOD_ICONS, "GOD_ICONS", ActionCategory::DEBUG,
			"Open the validated library of reusable in-game interface symbols.",
			OS0UIIcon::EXAMINE },
		{ ContextAction::GOD_TOOLS, "GOD_TOOLS", ActionCategory::DEBUG,
			"Issue the complete debug field-tool set to the selected operator.",
			OS0UIIcon::TOOLKIT },
		{ ContextAction::GOD_REVIVE, "GOD_REVIVE", ActionCategory::DEBUG,
			"Revive and fully restore the selected operator for sandbox testing.",
			OS0UIIcon::FIRST_AID }
	}};

	constexpr std::array<ActionCategoryDescriptor,
		static_cast<size_t>(ActionCategory::COUNT)> CATEGORIES{{
		{ ActionCategory::INFO, "INFO",
			"Identity, condition and descriptive information.", OS0UIIcon::EXAMINE },
		{ ActionCategory::GEAR, "GEAR",
			"Inventory, equipment and carried-item operations.", OS0UIIcon::HAND },
		{ ActionCategory::MOVEMENT, "MOVE",
			"Operator navigation and physical object movement.", OS0UIIcon::WALK },
		{ ActionCategory::STANCE, "STANCE",
			"Posture, stealth and exposure controls.", OS0UIIcon::SNEAK },
		{ ActionCategory::COMBAT, "COMBAT",
			"Weapon handling, aiming and offensive actions.", OS0UIIcon::TARGET },
		{ ActionCategory::MEDICAL, "MEDICAL",
			"Treatment and casualty-management actions.", OS0UIIcon::FIRST_AID },
		{ ActionCategory::SOCIAL, "SOCIAL",
			"Conversation and contact interaction.", OS0UIIcon::TALK },
		{ ActionCategory::WORLD, "WORLD",
			"Environment, construction and salvage operations.", OS0UIIcon::TOOLKIT },
		{ ActionCategory::GROUP, "GROUP",
			"Squad selection, team management and turn control.", OS0UIIcon::WALK },
		{ ActionCategory::DEBUG, "GOD",
			"God-mode asset, editor and interface development tools.",
			OS0UIIcon::KEYRING }
		}};
}

ContextActionDescriptor const& GetContextActionDescriptor(ContextAction action)
{
	const size_t index = static_cast<size_t>(action);
	return ACTIONS[index < ACTIONS.size() ? index : 0];
}

ActionCategoryDescriptor const& GetActionCategoryDescriptor(
	ActionCategory category)
{
	const size_t index = static_cast<size_t>(category);
	return CATEGORIES[index < CATEGORIES.size() ? index : 0];
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
	return GetActionCategoryDescriptor(category).name;
}

const char* ContextActionExplanation(ContextAction action)
{
	return GetContextActionDescriptor(action).explanation;
}

OS0UIIcon ContextActionIcon(ContextAction action)
{
	return GetContextActionDescriptor(action).icon;
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

std::vector<OS0ResolvedAction> ResolveOS0EnvironmentActions(
	OS0EnvironmentActionFacts const& facts)
{
	std::vector<OS0ResolvedAction> actions;
	auto add = [&](ContextAction action, BOOLEAN enabled)
	{
		if (std::none_of(actions.begin(), actions.end(), [action](auto const& entry)
			{ return entry.action == action; }))
			actions.push_back({ action, enabled });
	};

	if (facts.openable) add(ContextAction::CONTENTS, facts.near);
	if (facts.hasItems) add(ContextAction::PICK_UP, facts.near);
	if (facts.diggableSurface) add(ContextAction::DIG, facts.canDig);
	if (facts.salvageable) add(ContextAction::SALVAGE, facts.canSalvage);
	if (facts.moveCandidate)
	{
		add(ContextAction::CARRY, facts.canMove);
		add(ContextAction::PUSH, facts.canMove);
		add(ContextAction::PULL, facts.canMove);
		add(ContextAction::THROW, facts.canThrow);
	}
	if (facts.hasAsset) add(ContextAction::BUILD, facts.buildable);
	if (facts.hasAsset || facts.hasItems || facts.terrain)
		add(ContextAction::INSPECT, TRUE);
	if (facts.hasAsset && facts.debugCatalog)
		add(ContextAction::CATALOG, TRUE);
	return actions;
}

BOOLEAN OS0IsManipulationAction(ContextAction action) noexcept
{
	return action == ContextAction::CARRY || action == ContextAction::PUSH ||
		action == ContextAction::PULL || action == ContextAction::THROW;
}
