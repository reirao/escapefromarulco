#include "OS0_PointerSnapshot.h"

#include "Handle_UI.h"
#include "Interactive_Tiles.h"
#include "Isometric_Utils.h"
#include "OS0_IngameUI.h"
#include "Overhead.h"
#include "RenderWorld.h"
#include "Soldier_Find.h"
#include "UILayout.h"

BOOLEAN OS0ProjectTacticalScreenToWorld(INT16 screenX, INT16 screenY,
	INT16& worldX, INT16& worldY, GridNo* const gridNo)
{
	if (screenX < gsVIEWPORT_START_X || screenX >= gsVIEWPORT_END_X ||
		screenY < gsVIEWPORT_WINDOW_START_Y ||
		screenY >= OS0WorldViewportBottom()) return FALSE;
	OS0MapDisplayToWorldScreen(&screenX, &screenY);
	const INT16 offsetX = static_cast<INT16>(
		screenX - g_ui.m_tacticalMapCenterX);
	const INT16 offsetY = static_cast<INT16>(
		screenY - g_ui.m_tacticalMapCenterY + 10);
	INT16 cellX;
	INT16 cellY;
	FromScreenToCellCoordinates(offsetX, offsetY, &cellX, &cellY);
	const INT32 projectedX = static_cast<INT32>(gsRenderCenterX) + cellX;
	const INT32 projectedY = static_cast<INT32>(gsRenderCenterY) + cellY;
	if (projectedX < 0 || projectedX >= WORLD_COORD_COLS ||
		projectedY < 0 || projectedY >= WORLD_COORD_ROWS)
		return FALSE;

	worldX = static_cast<INT16>(projectedX);
	worldY = static_cast<INT16>(projectedY);
	if (gridNo)
	{
		const INT16 column = static_cast<INT16>(projectedX / CELL_X_SIZE);
		const INT16 row = static_cast<INT16>(projectedY / CELL_Y_SIZE);
		*gridNo = MAPROWCOLTOPOS(row, column);
		if (*gridNo < 0 || *gridNo >= WORLD_MAX) return FALSE;
	}
	return TRUE;
}

OS0PointerSnapshot OS0CapturePointerSnapshot(UINT8 const level)
{
	OS0PointerSnapshot result;
	result.screenX = static_cast<INT16>(gusMouseXPos);
	result.screenY = static_cast<INT16>(gusMouseYPos);
	result.level = level;
	result.tileIndex = 0xffff;
	if (!OS0ProjectTacticalScreenToWorld(result.screenX, result.screenY,
		result.worldX, result.worldY, &result.gridNo))
		return result;
	result.hasWorldPoint = TRUE;

	// Soldier rectangles live in the unscaled tactical surface. Resolve every
	// visible actor against that same coordinate space, then choose the rendered
	// front-most one. The vanilla global cursor cache is frame-late while scrolling
	// and uses display pixels, so it cannot be a second source of target truth.
	INT16 worldScreenX = result.screenX;
	INT16 worldScreenY = result.screenY;
	OS0MapDisplayToWorldScreen(&worldScreenX, &worldScreenY);
	INT16 bestBottom = -32768;
	FOR_EACH_MERC(i)
	{
		SOLDIERTYPE* const soldier = *i;
		if (!soldier || !soldier->bActive || soldier->bLevel != result.level ||
			(soldier->uiStatusFlags &
				(SOLDIER_DEAD | SOLDIER_PASSENGER | SOLDIER_DRIVER)) ||
			(soldier->bVisible == -1 &&
				!(gTacticalStatus.uiFlags & SHOW_ALL_MERCS)) ||
			!IsPointInSoldierBoundingBox(soldier, worldScreenX, worldScreenY))
			continue;
		INT16 soldierX;
		INT16 soldierY;
		GetSoldierScreenPos(soldier, &soldierX, &soldierY);
		const INT16 bottom = static_cast<INT16>(
			soldierY + soldier->sBoundingBoxHeight);
		if (!result.actor || bottom >= bestBottom)
		{
			result.actor = soldier;
			bestBottom = bottom;
		}
	}
	if (result.actor)
	{
		result.gridNo = result.actor->sGridNo;
		result.level = result.actor->bLevel;
		return result;
	}

	if (FindOS0WorldItemAtScreen(&result.worldItemIndex, &result.gridNo,
		result.level, gusMouseXPos, gusMouseYPos))
	{
		result.hasWorldPoint = TRUE;
		return result;
	}
	FindOS0WorldAssetAtScreen(&result.gridNo, result.level,
		&result.tileIndex, gusMouseXPos, gusMouseYPos);
	result.hasWorldPoint = result.gridNo >= 0 && result.gridNo < WORLD_MAX;
	return result;
}
