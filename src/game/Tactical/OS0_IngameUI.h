/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-30.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;
struct STRUCTURE;

enum class OS0CharacterQuickAction : UINT8
{
	CHARACTER,
	INVENTORY,
	ICON_LIBRARY,
	STEALTH,
	WEAPON_MODE,
	RELOAD
};

void InitializeOS0IngameUI();
void ShutdownOS0IngameUI();
void UpdateOS0TacticalSession();
void RenderOS0IngameUI();
void OS0RenderAutoFirstAidStatus(BOOLEAN complete, UINT32 elapsedSeconds);
void OS0OpenCharacterPanel(SOLDIERTYPE* soldier);
void OS0OpenWorldContainer(GridNo gridNo, UINT8 level, UINT16 tileIndex);
void OS0ActivateWorldObject(GridNo gridNo, UINT8 level, UINT16 tileIndex);
BOOLEAN OS0SelectWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex);
// F-key perception entry point: enable nearby awareness and open the safe
// relation menu for the exact target resolved under the pointer.
BOOLEAN OS0ActivateHoveredInteraction(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex, INT16 screenX, INT16 screenY);
BOOLEAN OS0ActivateCurrentHoverInteraction(INT16 screenX, INT16 screenY);
void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY);
void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY);
void OS0ClearWorldHover();
BOOLEAN OS0BlocksWorldInputAt(INT16 screenX, INT16 screenY);
BOOLEAN OS0BlocksKeyboardWorldInputAt(INT16 screenX, INT16 screenY);
BOOLEAN OS0OwnsViewportPrimaryButton();
void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex);
void OS0CancelCursorAction();
BOOLEAN OS0HandleRealtimeControlKey(UINT32 key, UINT32 keyState,
	UINT16 eventType);
BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex);
BOOLEAN OS0HandleHeldItemAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex);
BOOLEAN OS0HandlePendingWorldMove(GridNo destination);
void OS0AdjustWorldZoom(INT8 direction);
void OS0PrepareWorldZoom();
void OS0ApplyWorldZoom();
UINT8 OS0WorldZoomFactor();
BOOLEAN OS0WorldZoomKeepsLegacyScrollBoost();
void OS0MapDisplayToWorldScreen(INT16* x, INT16* y);
void OS0MapDisplayToWorldScreen(UINT16* x, UINT16* y);
void OS0MapWorldToDisplayScreen(INT16* x, INT16* y);
INT16 OS0WorldViewportBottom();
void OS0PlaceTalkingPanel(INT16 panelWidth, INT16 panelHeight, INT16* x, INT16* y);
void OS0TalkingPanelClosed();
BOOLEAN OS0CreatorIsActive();
void OS0ExecuteCharacterQuickAction(SOLDIERTYPE* soldier,
	OS0CharacterQuickAction action);
UINT8 OS0GetGodMenuIcon();
