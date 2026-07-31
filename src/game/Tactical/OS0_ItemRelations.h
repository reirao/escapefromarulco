#pragma once

#include "JA2Types.h"
#include "OS0_FixedList.h"
#include "OS0_UIAssetManager.h"

#include <array>

struct OBJECTTYPE;
struct SOLDIERTYPE;

enum class ItemTransferIntent : UINT8
{
	PRIMARY_HAND,
	SECONDARY_HAND,
	BODY,
	PACK,
	DROP
};

struct ItemTransferIntentSpec
{
	ItemTransferIntent intent;
	const char* label;
	OS0UIIcon icon;
	INT16 offsetX;
	INT16 offsetY;
};

extern const std::array<ItemTransferIntentSpec, 5> gOS0ItemTransferIntents;

// Engine-independent transfer policy. The tactical UI supplies facts about the
// held item and the hovered actor; player UI, tutorials and future AI can then
// consume the same ordered decision without duplicating interaction rules.
struct ItemTransferPolicyInput
{
	BOOLEAN carryingItem = FALSE;
	BOOLEAN targetAvailable = FALSE;
	BOOLEAN targetAccessible = FALSE;
	std::array<BOOLEAN, 5> allowed{};
};

struct ItemTransferPolicyDecision
{
	OS0FixedList<ItemTransferIntent, 5> actions;
	ItemTransferIntent preferred = ItemTransferIntent::DROP;
	BOOLEAN hasPreferred = FALSE;
	// A later automatic-behaviour module may safely execute preferred when this
	// is true. The player-facing UI merely marks that decision as recommended.
	BOOLEAN safeToApplyAutomatically = FALSE;

	BOOLEAN allows(ItemTransferIntent intent) const;
	BOOLEAN hasAlternatives() const { return hasPreferred && actions.size() > 1; }
};

ItemTransferPolicyDecision ResolveItemTransferPolicy(
	ItemTransferPolicyInput const& input);

const char* OS0InventorySlotName(INT8 slot);
UINT32 OS0AddedCarryPercent(SOLDIERTYPE const* soldier, OBJECTTYPE const& object);
BOOLEAN OS0CanAcceptCarriedObject(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object);
BOOLEAN OS0HasActiveHandUse(OBJECTTYPE const& object);
BOOLEAN OS0CanPackObject(SOLDIERTYPE const* soldier, OBJECTTYPE const& object);
INT8 OS0PreferredEquipmentSlot(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object);
BOOLEAN OS0EquipObject(SOLDIERTYPE* soldier, OBJECTTYPE* object, INT8 returnSlot);
