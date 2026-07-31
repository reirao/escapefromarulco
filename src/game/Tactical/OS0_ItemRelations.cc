/* Escape from Arulco: authoritative item-to-actor relation rules. */

#include "OS0_ItemRelations.h"

#include "ArmourModel.h"
#include "ContentManager.h"
#include "GameInstance.h"
#include "Interface_Items.h"
#include "ItemModel.h"
#include "Items.h"
#include "Points.h"
#include "SkillCheck.h"
#include "Soldier_Control.h"
#include "Weapons.h"

#include <algorithm>

const std::array<ItemTransferIntentSpec, 5> gOS0ItemTransferIntents{{
	{ ItemTransferIntent::PRIMARY_HAND,   "HAND 1 / PRIMARY", OS0UIIcon::TARGET, -80, -38 },
	{ ItemTransferIntent::SECONDARY_HAND, "HAND 2 / OFF HAND", OS0UIIcon::PUNCH,  48, -38 },
	{ ItemTransferIntent::BODY,           "EQUIP ON BODY", OS0UIIcon::EXAMINE, -17,-106 },
	{ ItemTransferIntent::PACK,           "PUT IN PACK", OS0UIIcon::HAND, 48, -80 },
	{ ItemTransferIntent::DROP,           "DROP AT FEET", OS0UIIcon::WALK, -80, -80 }
}};

namespace
{
	constexpr size_t IntentIndex(ItemTransferIntent intent)
	{
		return static_cast<size_t>(intent);
	}
}

BOOLEAN ItemTransferPolicyDecision::allows(ItemTransferIntent intent) const
{
	return std::find(actions.begin(), actions.end(), intent) != actions.end();
}

ItemTransferPolicyDecision ResolveItemTransferPolicy(
	ItemTransferPolicyInput const& input)
{
	ItemTransferPolicyDecision decision;
	if (!input.carryingItem || !input.targetAvailable || !input.targetAccessible)
		return decision;

	for (ItemTransferIntentSpec const& spec : gOS0ItemTransferIntents)
	{
		if (input.allowed[IntentIndex(spec.intent)])
			decision.actions.push_back(spec.intent);
	}

	// Prefer a semantic destination over generic storage: armour/face gear goes
	// onto the body, weapons into a usable hand and generic objects into the pack.
	// DROP remains the explicit escape hatch, never the first useful suggestion.
	constexpr std::array<ItemTransferIntent, 5> preference{{
		ItemTransferIntent::BODY,
		ItemTransferIntent::PRIMARY_HAND,
		ItemTransferIntent::SECONDARY_HAND,
		ItemTransferIntent::PACK,
		ItemTransferIntent::DROP
	}};
	for (ItemTransferIntent intent : preference)
	{
		if (!decision.allows(intent)) continue;
		decision.preferred = intent;
		decision.hasPreferred = TRUE;
		break;
	}

	size_t usefulDestinations = 0;
	for (ItemTransferIntent intent : decision.actions)
		if (intent != ItemTransferIntent::DROP) ++usefulDestinations;
	decision.safeToApplyAutomatically = usefulDestinations == 1;
	return decision;
}

const char* OS0InventorySlotName(INT8 slot)
{
	switch (slot)
	{
		case HELMETPOS: return "HELMET";
		case VESTPOS: return "VEST";
		case LEGPOS: return "LEGS";
		case HEAD1POS: return "FACE 1";
		case HEAD2POS: return "FACE 2";
		case HANDPOS: return "HAND 1 / PRIMARY";
		case SECONDHANDPOS: return "HAND 2 / OFF HAND";
		default:
			return slot >= BIGPOCK1POS && slot <= BIGPOCK4POS ?
				"PACK / LARGE" : "PACK / SMALL";
	}
}

UINT32 OS0AddedCarryPercent(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object)
{
	if (!soldier || object.usItem == NOTHING) return 0;
	ItemModel const* const item = GCM->getItem(object.usItem);
	INT32 weightGrams = 100 * item->getWeight();
	if (item->getPerPocket() <= 1)
	{
		for (UINT16 attachment : object.usAttachItem)
			if (attachment != NONE)
				weightGrams += 100 * GCM->getItem(attachment)->getWeight();
		if (item->isGun() && object.ubGunShotsLeft > 0)
			weightGrams += 100 * GCM->getItem(object.usGunAmmoItem)->getWeight();
	}
	else
	{
		weightGrams *= object.ubNumberOfObjects;
	}
	INT32 strength = EffectiveStrength(soldier);
	if (strength > 80) strength += strength - 80;
	return strength > 0 ? static_cast<UINT32>(
		100 * weightGrams / (strength * 500)) : 999;
}

BOOLEAN OS0CanAcceptCarriedObject(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object)
{
	return soldier && CalculateCarriedWeight(soldier) +
		OS0AddedCarryPercent(soldier, object) <= 125;
}

BOOLEAN OS0HasActiveHandUse(OBJECTTYPE const& object)
{
	if (object.usItem == NOTHING) return FALSE;
	UINT32 const itemClass = GCM->getItem(object.usItem)->getItemClass();
	constexpr UINT32 activeClasses = IC_WEAPON | IC_THROWN | IC_PUNCH |
		IC_GRENADE | IC_BOMB | IC_MEDKIT | IC_KIT;
	return (itemClass & activeClasses) != 0 || object.usItem == CROWBAR;
}

BOOLEAN OS0CanPackObject(SOLDIERTYPE const* soldier, OBJECTTYPE const& object)
{
	if (!soldier || object.usItem == NOTHING || object.ubNumberOfObjects == 0)
		return FALSE;
	UINT16 remaining = object.ubNumberOfObjects;
	for (INT8 slot = BIGPOCK1POS; slot <= SMALLPOCK8POS; ++slot)
	{
		const UINT8 limit = ItemSlotLimit(object.usItem, slot);
		if (limit == 0) continue;
		OBJECTTYPE const& stored = soldier->inv[slot];
		UINT8 available = 0;
		if (stored.usItem == NOTHING)
			available = limit;
		else if (stored.usItem == object.usItem &&
			stored.ubNumberOfObjects < limit)
			available = static_cast<UINT8>(limit - stored.ubNumberOfObjects);
		if (available >= remaining) return TRUE;
		remaining = static_cast<UINT16>(remaining - available);
	}
	return FALSE;
}

INT8 OS0PreferredEquipmentSlot(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object)
{
	if (!soldier || object.usItem == NOTHING) return NO_SLOT;
	ItemModel const* const item = GCM->getItem(object.usItem);
	if (item->isWeapon())
	{
		if (item->isTwoHanded() || soldier->inv[HANDPOS].usItem == NOTHING)
			return HANDPOS;
		const BOOLEAN primaryTwoHanded =
			GCM->getItem(soldier->inv[HANDPOS].usItem)->isTwoHanded();
		if (!primaryTwoHanded && soldier->inv[SECONDHANDPOS].usItem == NOTHING)
			return SECONDHANDPOS;
		return HANDPOS;
	}
	if (item->isArmour())
	{
		switch (item->asArmour()->getArmourClass())
		{
			case ARMOURCLASS_HELMET: return HELMETPOS;
			case ARMOURCLASS_VEST: return VESTPOS;
			case ARMOURCLASS_LEGGINGS: return LEGPOS;
			default: return NO_SLOT;
		}
	}
	if (item->isFace())
		return soldier->inv[HEAD1POS].usItem == NOTHING ? HEAD1POS : HEAD2POS;
	return NO_SLOT;
}

BOOLEAN OS0EquipObject(SOLDIERTYPE* soldier, OBJECTTYPE* object, INT8 returnSlot)
{
	if (!soldier || !object) return FALSE;
	const INT8 target = OS0PreferredEquipmentSlot(soldier, *object);
	if (target == NO_SLOT) return FALSE;
	OBJECTTYPE preview = *object;
	if (!CanItemFitInPosition(soldier, &preview, target, FALSE)) return FALSE;
	if (target == returnSlot) return PlaceObject(soldier, returnSlot, object);
	if (!PlaceObject(soldier, target, object)) return FALSE;
	if (object->usItem != NOTHING)
	{
		if (returnSlot == NO_SLOT || !PlaceObject(soldier, returnSlot, object))
			AutoPlaceObject(soldier, object, FALSE);
	}
	return TRUE;
}
