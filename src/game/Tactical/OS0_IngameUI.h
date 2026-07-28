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
void RenderOS0IngameUI();
void OS0OpenCharacterPanel(SOLDIERTYPE* soldier);
void OS0OpenWorldContainer(GridNo gridNo, UINT8 level, UINT16 tileIndex);
void OS0ActivateWorldObject(GridNo gridNo, UINT8 level, UINT16 tileIndex);
BOOLEAN OS0SelectWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex);
void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY);
void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY);
void OS0ClearWorldHover();
void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex);
void OS0CancelCursorAction();
BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex);
BOOLEAN OS0HandlePendingWorldMove(GridNo destination);
void OS0AdjustWorldZoom(INT8 direction);
void OS0PrepareWorldZoom();
void OS0ApplyWorldZoom();
void OS0MapDisplayToWorldScreen(INT16* x, INT16* y);
void OS0MapDisplayToWorldScreen(UINT16* x, UINT16* y);
void OS0MapWorldToDisplayScreen(INT16* x, INT16* y);
void OS0PlaceTalkingPanel(INT16 panelWidth, INT16 panelHeight, INT16* x, INT16* y);
void OS0TalkingPanelClosed();
void OS0ExecuteCharacterQuickAction(SOLDIERTYPE* soldier,
	OS0CharacterQuickAction action);
UINT8 OS0GetGodMenuIcon();
BOOLEAN OS0NotifyWorldAssetHit(GridNo gridNo, STRUCTURE* structure, UINT8 impact);
