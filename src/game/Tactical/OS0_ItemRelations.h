#pragma once

#include "JA2Types.h"

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
	UINT16 iconFrame;
	INT16 offsetX;
	INT16 offsetY;
};

extern const std::array<ItemTransferIntentSpec, 5> gOS0ItemTransferIntents;

const char* OS0InventorySlotName(INT8 slot);
UINT32 OS0AddedCarryPercent(SOLDIERTYPE const* soldier, OBJECTTYPE const& object);
BOOLEAN OS0CanAcceptCarriedObject(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object);
INT8 OS0PreferredEquipmentSlot(SOLDIERTYPE const* soldier,
	OBJECTTYPE const& object);
BOOLEAN OS0EquipObject(SOLDIERTYPE* soldier, OBJECTTYPE* object, INT8 returnSlot);
