/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-30.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;
struct STRUCTURE;
struct LEVELNODE;

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
// Reproject world-attached regions after ScrollWorld() has committed the
// camera for this frame. This is intentionally separate from simulation.
void OS0PrepareScreenProjection(BOOLEAN updateDynamicState);
void RenderOS0IngameUI();
void OS0RenderAutoFirstAidStatus(BOOLEAN complete, UINT32 elapsedSeconds);
void OS0OpenCharacterPanel(SOLDIERTYPE* soldier);
void OS0OpenWorldContainer(GridNo gridNo, UINT8 level, UINT16 tileIndex,
	SOLDIERTYPE* actor = nullptr);
void OS0ActivateWorldObject(GridNo gridNo, UINT8 level, UINT16 tileIndex,
	INT32 worldItemIndex = -1);
BOOLEAN OS0SelectWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level, UINT16 tileIndex);
// F-key perception entry point: enable nearby awareness and open the safe
// relation menu for the exact target resolved under the pointer.
BOOLEAN OS0ActivateHoveredInteraction(SOLDIERTYPE* target, GridNo gridNo,
	UINT8 level, UINT16 tileIndex, INT16 screenX, INT16 screenY,
	INT32 worldItemIndex = -1);
BOOLEAN OS0ActivateCurrentHoverInteraction(INT16 screenX, INT16 screenY);
void OS0OpenContextMenu(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY,
	INT32 worldItemIndex = -1);
void OS0HoverWorldObject(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT16 screenX, INT16 screenY,
	INT32 worldItemIndex = -1);
void OS0ClearWorldHover();
// True only for the world-attached quick-action glyph. The viewport uses this
// during LOST_MOUSE so entering that child region does not erase its own target.
BOOLEAN OS0HoverQuickActionOwnsPointer(INT16 screenX, INT16 screenY);
BOOLEAN OS0NearbyHintOwnsPointer(INT16 screenX, INT16 screenY);
BOOLEAN OS0RefreshCurrentNearbyHintHover(INT16 screenX, INT16 screenY);
BOOLEAN OS0ActivateCurrentNearbyHintInteraction(INT16 screenX, INT16 screenY,
	BOOLEAN cycleAction);
BOOLEAN OS0BlocksWorldInputAt(INT16 screenX, INT16 screenY);
BOOLEAN OS0BlocksKeyboardWorldInputAt(INT16 screenX, INT16 screenY);
// Mouse-edge camera scrolling is pointer-owned UI input while a blocking OS0
// surface is under the pointer or any OS0 window/orb drag owns the press.
BOOLEAN OS0BlocksMouseEdgeScroll();
BOOLEAN OS0OwnsViewportPrimaryButton();
void OS0CycleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex = -1);
void OS0CancelCursorAction();
BOOLEAN OS0HandleRealtimeControlKey(UINT32 key, UINT32 keyState,
	UINT16 eventType);
BOOLEAN OS0HandleCursorAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex = -1);
BOOLEAN OS0HandleHeldItemAction(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
	UINT16 tileIndex);
BOOLEAN OS0HandlePendingWorldMove(GridNo destination);
// Direct world-object gestures use the same carry/item transactions as radial
// actions.  The viewport only owns press/threshold/release; gameplay policy and
// identity validation stay here.
BOOLEAN OS0CanBeginWorldPointerDrag(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex = -1);
BOOLEAN OS0BeginWorldPointerDrag(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, INT32 worldItemIndex = -1);
void OS0CancelWorldPointerDrag();
// RenderWorld asks whether the exact source node is represented by the active
// carry projection.  This avoids palette mutation and duplicate source ghosts.
BOOLEAN OS0SuppressCarriedWorldNode(GridNo gridNo, UINT8 level,
	LEVELNODE const* node);
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
