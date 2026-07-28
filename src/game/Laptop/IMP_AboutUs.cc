/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "IMP_AboutUs.h"

#include "Button_System.h"
#include "CharProfile.h"
#include "ContentManager.h"
#include "Cursor_Control.h"
#include "Cursors.h"
#include "Font.h"
#include "Font_Control.h"
#include "GameInstance.h"
#include "HImage.h"
#include "IMPVideoObjects.h"
#include "Input.h"
#include "Interface_Items.h"
#include "ItemModel.h"
#include "Items.h"
#include "Laptop.h"
#include "MouseSystem.h"
#include "ScreenIDs.h"
#include "Soldier_Profile_Type.h"
#include "Video.h"
#include "VObject.h"
#include "VSurface.h"

#include <array>
#include <string_theory/format>


namespace
{
	constexpr INT32 LOADOUT_BUDGET = 1000;

	struct ShopEntry
	{
		UINT16 item;
		INT16 cost;
	};

	struct SlotLayout
	{
		INT8 slot;
		INT16 x;
		INT16 y;
		INT16 w;
		INT16 h;
		const char* label;
	};

	constexpr std::array<ShopEntry, 10> gShop{{
		{ GLOCK_17,       260 },
		{ CLIP9_15,        45 },
		{ COMBAT_KNIFE,    90 },
		{ FIRSTAIDKIT,     80 },
		{ MEDICKIT,       210 },
		{ CANTEEN,         35 },
		{ SILENCER,       140 },
		{ STEEL_HELMET,   180 },
		{ FLAK_JACKET,    280 },
		{ KEVLAR_LEGGINGS, 240 }
	}};

	// Coordinates are relative to the laptop web area.  These are the real
	// JA2 inventory positions, arranged compactly for the creator screen.
	constexpr std::array<SlotLayout, NUM_INV_SLOTS> gSlots{{
		{ HELMETPOS,       340,  54, 43, 24, "HEAD" },
		{ VESTPOS,         340,  83, 43, 24, "VEST" },
		{ LEGPOS,          340, 112, 43, 24, "LEGS" },
		{ HANDPOS,         244, 150, 61, 23, "MAIN" },
		{ SECONDHANDPOS,   314, 150, 61, 23, "OFF" },
		{ BIGPOCK1POS,     244, 182, 61, 23, "B1" },
		{ BIGPOCK2POS,     314, 182, 61, 23, "B2" },
		{ BIGPOCK3POS,     244, 210, 61, 23, "B3" },
		{ BIGPOCK4POS,     314, 210, 61, 23, "B4" },
		{ SMALLPOCK1POS,   244, 246, 30, 23, "1" },
		{ SMALLPOCK2POS,   278, 246, 30, 23, "2" },
		{ SMALLPOCK3POS,   312, 246, 30, 23, "3" },
		{ SMALLPOCK4POS,   346, 246, 30, 23, "4" },
		{ SMALLPOCK5POS,   244, 274, 30, 23, "5" },
		{ SMALLPOCK6POS,   278, 274, 30, 23, "6" },
		{ SMALLPOCK7POS,   312, 274, 30, 23, "7" },
		{ SMALLPOCK8POS,   346, 274, 30, 23, "8" },
		{ NO_SLOT,           0,   0,  0,  0, "" },
		{ NO_SLOT,           0,   0,  0,  0, "" }
	}};

	std::array<OBJECTTYPE, NUM_INV_SLOTS> gLoadoutInventory{};
	std::array<MOUSE_REGION, gShop.size()> gShopRegions{};
	std::array<MOUSE_REGION, NUM_INV_SLOTS> gSlotRegions{};
	GUIButtonRef gBackButton;
	GUIButtonRef gClearButton;
	OBJECTTYPE gDraggedItem{};
	INT8 gDragSourceSlot = NO_SLOT;
	BOOLEAN gDragging = FALSE;
	BOOLEAN gLoadoutInitialized = FALSE;
	BOOLEAN gLoadoutDirty = FALSE;

	INT16 ItemCost(UINT16 item)
	{
		for (const ShopEntry& entry : gShop)
		{
			if (entry.item == item) return entry.cost;
		}
		return 0;
	}

	INT32 CurrentSpend()
	{
		INT32 result = 0;
		for (const OBJECTTYPE& object : gLoadoutInventory)
		{
			if (object.usItem != NOTHING) result += ItemCost(object.usItem);
		}
		return result;
	}

	void DrawSlot(const SlotLayout& layout)
	{
		if (layout.slot == NO_SLOT) return;
		const INT16 x = LAPTOP_SCREEN_UL_X + layout.x;
		const INT16 y = LAPTOP_SCREEN_WEB_UL_Y + layout.y;
		const UINT16 border = Get16BPPColor(FROMRGB(160, 0, 0));
		const UINT16 fill = Get16BPPColor(FROMRGB(8, 12, 14));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + layout.w, y + layout.h, border);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 1, y + 1, x + layout.w - 1, y + layout.h - 1, fill);

		const OBJECTTYPE& object = gLoadoutInventory[layout.slot];
		if (object.usItem != NOTHING)
		{
			INVRenderItem(FRAME_BUFFER, nullptr, object, x + 1, y + 1,
				layout.w - 2, layout.h - 2, DIRTYLEVEL2, 0, SGP_TRANSPARENT);
		}
		else
		{
			SetFont(TINYFONT1);
			SetFontForeground(FONT_MCOLOR_DKGRAY);
			MPrint(x + 3, y + 7, layout.label);
		}
	}

	void DrawShopEntry(UINT32 index)
	{
		const INT16 column = index % 2;
		const INT16 row = index / 2;
		const INT16 x = LAPTOP_SCREEN_UL_X + 24 + column * 99;
		const INT16 y = LAPTOP_SCREEN_WEB_UL_Y + 70 + row * 46;
		OBJECTTYPE object{};
		CreateItem(gShop[index].item, 100, &object);

		const UINT16 border = Get16BPPColor(FROMRGB(120, 0, 0));
		const UINT16 fill = Get16BPPColor(FROMRGB(8, 12, 14));
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + 91, y + 39, border);
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 1, y + 1, x + 90, y + 38, fill);
		INVRenderItem(FRAME_BUFFER, nullptr, object, x + 2, y + 2, 87, 20,
			DIRTYLEVEL2, 0, SGP_TRANSPARENT);

		SetFont(TINYFONT1);
		SetFontForeground(FONT_WHITE);
		MPrint(x + 3, y + 24, GCM->getItem(gShop[index].item)->getShortName());
		SetFontForeground(FONT_MCOLOR_RED);
		MPrint(x + 68, y + 24, ST::format("{}", gShop[index].cost));
	}

	void CancelDrag()
	{
		if (!gDragging) return;
		if (gDragSourceSlot != NO_SLOT)
		{
			gLoadoutInventory[gDragSourceSlot] = gDraggedItem;
		}
		gDraggedItem = OBJECTTYPE{};
		gDragSourceSlot = NO_SLOT;
		gDragging = FALSE;
		SetCurrentCursorFromDatabase(CURSOR_WWW);
		gLoadoutDirty = TRUE;
	}

	const SlotLayout* FindSlotAt(INT16 x, INT16 y)
	{
		for (const SlotLayout& slot : gSlots)
		{
			if (slot.slot == NO_SLOT) continue;
			const INT16 left = LAPTOP_SCREEN_UL_X + slot.x;
			const INT16 top = LAPTOP_SCREEN_WEB_UL_Y + slot.y;
			if (x >= left && x <= left + slot.w && y >= top && y <= top + slot.h) return &slot;
		}
		return nullptr;
	}

	void FinishDragAtMouse()
	{
		const SlotLayout* target = FindSlotAt(gusMouseXPos, gusMouseYPos);
		if (target == nullptr || ItemSlotLimit(gDraggedItem.usItem, target->slot) == 0)
		{
			CancelDrag();
			return;
		}

		OBJECTTYPE displaced = gLoadoutInventory[target->slot];
		gLoadoutInventory[target->slot] = gDraggedItem;
		if (CurrentSpend() > LOADOUT_BUDGET)
		{
			gLoadoutInventory[target->slot] = displaced;
			CancelDrag();
			return;
		}

		if (gDragSourceSlot != NO_SLOT && gDragSourceSlot != target->slot)
		{
			gLoadoutInventory[gDragSourceSlot] = displaced;
		}
		gDraggedItem = OBJECTTYPE{};
		gDragSourceSlot = NO_SLOT;
		gDragging = FALSE;
		SetCurrentCursorFromDatabase(CURSOR_WWW);
		gLoadoutDirty = TRUE;
	}

	void ShopRegionCallback(MOUSE_REGION* region, UINT32 reason)
	{
		if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && !gDragging)
		{
			const UINT32 index = MSYS_GetRegionUserData(region, 0);
			CreateItem(gShop[index].item, 100, &gDraggedItem);
			gDragSourceSlot = NO_SLOT;
			gDragging = TRUE;
			SetMouseCursorFromItem(gDraggedItem.usItem);
		}
		if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gDragging) FinishDragAtMouse();
	}

	void SlotRegionCallback(MOUSE_REGION* region, UINT32 reason)
	{
		const INT8 slot = static_cast<INT8>(MSYS_GetRegionUserData(region, 0));
		if ((reason & MSYS_CALLBACK_REASON_POINTER_DWN) && !gDragging &&
			gLoadoutInventory[slot].usItem != NOTHING)
		{
			gDraggedItem = gLoadoutInventory[slot];
			gLoadoutInventory[slot] = OBJECTTYPE{};
			gDragSourceSlot = slot;
			gDragging = TRUE;
			SetMouseCursorFromItem(gDraggedItem.usItem);
			gLoadoutDirty = TRUE;
		}
		if ((reason & MSYS_CALLBACK_REASON_POINTER_UP) && gDragging) FinishDragAtMouse();
	}

	void BackCallback(GUI_BUTTON*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			CancelDrag();
			iCurrentImpPage = IMP_FINISH;
			fButtonPendingFlag = TRUE;
		}
	}

	void ClearCallback(GUI_BUTTON*, UINT32 reason)
	{
		if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
		{
			CancelDrag();
			gLoadoutInventory.fill(OBJECTTYPE{});
			gLoadoutDirty = TRUE;
		}
	}

	void CreateRegions()
	{
		for (UINT32 i = 0; i < gShop.size(); ++i)
		{
			const INT16 x = LAPTOP_SCREEN_UL_X + 24 + (i % 2) * 99;
			const INT16 y = LAPTOP_SCREEN_WEB_UL_Y + 70 + (i / 2) * 46;
			MSYS_DefineRegion(&gShopRegions[i], x, y, x + 91, y + 39,
				MSYS_PRIORITY_HIGH + 2, CURSOR_WWW, MSYS_NO_CALLBACK, ShopRegionCallback);
			MSYS_SetRegionUserData(&gShopRegions[i], 0, i);
		}

		for (const SlotLayout& slot : gSlots)
		{
			if (slot.slot == NO_SLOT) continue;
			const INT16 x = LAPTOP_SCREEN_UL_X + slot.x;
			const INT16 y = LAPTOP_SCREEN_WEB_UL_Y + slot.y;
			MSYS_DefineRegion(&gSlotRegions[slot.slot], x, y, x + slot.w, y + slot.h,
				MSYS_PRIORITY_HIGH + 2, CURSOR_WWW, MSYS_NO_CALLBACK, SlotRegionCallback);
			MSYS_SetRegionUserData(&gSlotRegions[slot.slot], 0, slot.slot);
		}
	}
}


void EnterIMPAboutUs()
{
	if (!gLoadoutInitialized)
	{
		gLoadoutInventory.fill(OBJECTTYPE{});
		gLoadoutInitialized = TRUE;
	}

	CreateRegions();
	gClearButton = CreateTextButton("CLEAR", FONT12ARIAL, FONT_MCOLOR_RED, FONT_BLACK,
		LAPTOP_SCREEN_UL_X + 24, LAPTOP_SCREEN_WEB_UL_Y + 318, 90, 42,
		MSYS_PRIORITY_HIGH, ClearCallback);
	gBackButton = CreateTextButton("SAVE LOADOUT", FONT12ARIAL, FONT_MCOLOR_RED, FONT_BLACK,
		LAPTOP_SCREEN_UL_X + 330, LAPTOP_SCREEN_WEB_UL_Y + 318, 145, 42,
		MSYS_PRIORITY_HIGH, BackCallback);
	gClearButton->SetCursor(CURSOR_WWW);
	gBackButton->SetCursor(CURSOR_WWW);
	RenderIMPAboutUs();
}


void ExitIMPAboutUs()
{
	CancelDrag();
	for (MOUSE_REGION& region : gShopRegions) MSYS_RemoveRegion(&region);
	for (const SlotLayout& slot : gSlots)
	{
		if (slot.slot != NO_SLOT) MSYS_RemoveRegion(&gSlotRegions[slot.slot]);
	}
	RemoveButton(gClearButton);
	RemoveButton(gBackButton);
}


void RenderIMPAboutUs()
{
	RenderProfileBackGround();
	const INT16 x = LAPTOP_SCREEN_UL_X;
	const INT16 y = LAPTOP_SCREEN_WEB_UL_Y;
	const UINT16 panel = Get16BPPColor(FROMRGB(4, 8, 9));
	const UINT16 red = Get16BPPColor(FROMRGB(120, 0, 0));
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 14, y + 38, x + 218, y + 306, panel);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 224, y + 38, x + 486, y + 306, panel);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 218, y + 38, x + 219, y + 306, red);

	SetFont(FONT12ARIAL);
	SetFontForeground(FONT_MCOLOR_RED);
	MPrint(x + 24, y + 47, "STARTER MARKET");
	MPrint(x + 244, y + 47, "FIELD INVENTORY");
	SetFont(TINYFONT1);
	SetFontForeground(FONT_MCOLOR_DKGRAY);
	MPrint(x + 394, y + 56, "ARMOR");
	MPrint(x + 386, y + 154, "HANDS");
	MPrint(x + 386, y + 190, "PACK");
	MPrint(x + 386, y + 253, "POCKETS");

	for (UINT32 i = 0; i < gShop.size(); ++i) DrawShopEntry(i);
	for (const SlotLayout& slot : gSlots) DrawSlot(slot);

	SetFont(FONT12ARIAL);
	SetFontForeground(CurrentSpend() <= LOADOUT_BUDGET ? FONT_WHITE : FONT_MCOLOR_RED);
	MPrint(x + 132, y + 327, ST::format("CREDITS  {} / {}", LOADOUT_BUDGET - CurrentSpend(), LOADOUT_BUDGET));
	InvalidateRegion(x, y, x + 500, y + 370);
}


void HandleIMPAboutUs()
{
	if (gLoadoutDirty)
	{
		RenderIMPAboutUs();
		gLoadoutDirty = FALSE;
	}
}


void ApplyOS0LoadoutToProfile(MERCPROFILESTRUCT& profile)
{
	for (INT8 slot = 0; slot < NUM_INV_SLOTS; ++slot)
	{
		profile.inv[slot] = NOTHING;
		profile.bInvStatus[slot] = 0;
		profile.bInvNumber[slot] = 0;
	}

	for (const SlotLayout& slot : gSlots)
	{
		if (slot.slot == NO_SLOT) continue;
		const OBJECTTYPE& object = gLoadoutInventory[slot.slot];
		if (object.usItem == NOTHING) continue;
		profile.inv[slot.slot] = object.usItem;
		profile.bInvStatus[slot.slot] = object.bStatus[0];
		profile.bInvNumber[slot.slot] = object.ubNumberOfObjects;
	}
}
