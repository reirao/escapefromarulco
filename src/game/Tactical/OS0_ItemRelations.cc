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

const std::array<ItemTransferIntentSpec, 5> gOS0ItemTransferIntents{{
	{ ItemTransferIntent::PRIMARY_HAND,   "HAND 1 / PRIMARY", 24, -80, -38 },
	{ ItemTransferIntent::SECONDARY_HAND, "HAND 2 / OFF HAND", 33,  48, -38 },
	{ ItemTransferIntent::BODY,           "EQUIP ON BODY",     12, -17,-106 },
	{ ItemTransferIntent::PACK,           "PUT IN PACK",       18,  48, -80 },
	{ ItemTransferIntent::DROP,           "DROP AT FEET",      39, -80, -80 }
}};

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
