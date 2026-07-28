/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;

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
