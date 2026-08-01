/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "Animation_Data.h"
#include "Cursor_Control.h"
#include "Cursors.h"
#include "HImage.h"
#include "Isometric_Utils.h"
#include "TileDef.h"
#include "VObject.h"
#include "RenderWorld.h"
#include "Interface.h"
#include "Sound_Control.h"
#include "WorldDef.h"
#include "Interactive_Tiles.h"
#include "WorldMan.h"
#include "Structure.h"
#include "Animation_Control.h"
#include "Overhead.h"
#include "OS0_DirectControl.h"
#include "OS0_IngameUI.h"
#include "OS0_ViewportInput.h"
#include "Structure_Wrap.h"
#include "Tile_Animation.h"
#include "Tile_Cache.h"
#include "Handle_Doors.h"
#include "StrategicMap.h"
#include "Quests.h"
#include "Dialogue_Control.h"
#include "English.h"
#include "Handle_Items.h"
#include "Handle_UI.h"
#include "NPC.h"
#include "Explosion_Control.h"
#include "Text.h"
#include "GameSettings.h"
#include "Game_Clock.h"
#include "Environment.h"
#include "Debug.h"
#include "UILayout.h"

#include "Soldier.h"
#include "GameInstance.h"
#include "ContentManager.h"
#include "ShippingDestinationModel.h"

#include <cstdlib>
#include <cstdint>
#include <limits>

#define MAX_INTTILE_STACK 10


struct CUR_INTERACTIVE_TILE
{
	INT16            sGridNo;
	INT16            sTileIndex;
	INT16            sHeighestScreenY;
	BOOLEAN          fFound;
	LEVELNODE const* pFoundNode;
	INT16            sFoundGridNo;
	UINT16           usStructureID;
	BOOLEAN          fStructure;
};


struct INTERACTIVE_TILE_STACK_TYPE
{
	INT8                 bNum;
	CUR_INTERACTIVE_TILE bTiles[MAX_INTTILE_STACK];
	INT8                 bCur;
};


static INTERACTIVE_TILE_STACK_TYPE gCurIntTileStack;
static BOOLEAN                     gfCycleIntTile = FALSE;


static CUR_INTERACTIVE_TILE gCurIntTile;
static BOOLEAN              gfOverIntTile = FALSE;

// Values to determine if we should check or not
static INT16  gsINTOldRenderCenterX = 0;
static INT16  gsINTOldRenderCenterY = 0;
static UINT16 gusINTOldMousePosX    = 0;
static UINT16 gusINTOldMousePosY    = 0;


BOOLEAN IsOS0PersistentWorldAssetNode(LEVELNODE const* const node)
{
	if (!node || node->usIndex >= NUMBEROFTILES ||
		node->uiFlags & (LEVELNODE_ITEM | LEVELNODE_HIDDEN |
			LEVELNODE_ROTTINGCORPSE | LEVELNODE_USEABSOLUTEPOS)) return FALSE;
	UINT32 const type = GetTileType(node->usIndex);
	if (FOOTPRINTS <= type && type <= LASTUIELEM) return FALSE;
	TILE_ELEMENT const& tile = gTileDatabase[node->usIndex];
	return tile.hTileSurface || (node->uiFlags & LEVELNODE_CACHEDANITILE);
}


void StartInteractiveObject(GridNo const gridno, STRUCTURE const& structure, SOLDIERTYPE& s, UINT8 const direction)
{
	// ATE: Patch fix: Don't allow if alreay in animation
	if (s.usAnimState == OPEN_STRUCT)               return;
	if (s.usAnimState == OPEN_STRUCT_CROUCHED)      return;
	if (s.usAnimState == BEGIN_OPENSTRUCT)          return;
	if (s.usAnimState == BEGIN_OPENSTRUCT_CROUCHED) return;

	// Add soldier event for opening door/struct
	Soldier{&s}.setPendingAction(structure.fFlags & STRUCTURE_ANYDOOR ? MERC_OPENDOOR : MERC_OPENSTRUCT);
	s.uiPendingActionData1     = structure.usStructureID;
	s.sPendingActionData2      = gridno;
	s.bPendingActionData3      = direction;
}


bool SoldierHandleInteractiveObject(SOLDIERTYPE& s)
{
	GridNo     const gridno       = s.sPendingActionData2;
	UINT16     const structure_id = (UINT16)s.uiPendingActionData1;
	STRUCTURE* const structure    = FindStructureByID(gridno, structure_id);
	if (!structure) return false;
	return HandleOpenableStruct(&s, gridno, structure);
}


void HandleStructChangeFromGridNo(SOLDIERTYPE* const s, GridNo const grid_no)
{
	STRUCTURE* const structure = FindStructure(grid_no, STRUCTURE_OPENABLE);
	if (!structure)
	{
		SLOGW("Told to handle struct that does not exist at {}.", grid_no);
		return;
	}

	// Do sound...
	bool const closing = structure->fFlags & STRUCTURE_OPEN;
	PlayLocationJA2Sample(grid_no, GetStructureOpenSound(structure, closing), HIGHVOLUME, 1);

	// ATE: Don't handle switches!
	if (!(structure->fFlags & STRUCTURE_SWITCH))
	{
		bool did_missing_quote = false;
		if (s->bTeam == OUR_TEAM)
		{
			auto primaryDest = GCM->getPrimaryShippingDestination();
			if (grid_no        == primaryDest->deliverySectorGridNo  &&
			    gWorldSector == primaryDest->deliverySector &&
					CheckFact(FACT_PABLOS_STOLE_FROM_LATEST_SHIPMENT, 0) &&
					!CheckFact(FACT_PLAYER_FOUND_ITEMS_MISSING, 0))
			{
				SayQuoteFromNearbyMercInSector(grid_no, 3, QUOTE_STUFF_MISSING_DRASSEN);
				did_missing_quote = true;
			}
		}
		else if (s->bTeam == CIV_TEAM)
		{
			if (s->ubProfile != NO_PROFILE)
			{
				TriggerNPCWithGivenApproach(s->ubProfile, APPROACH_DONE_OPEN_STRUCTURE);
			}
		}

		ITEM_POOL* const item_pool = GetItemPool(grid_no, s->bLevel);
		if (item_pool)
		{
			// Update visiblity
			if (!closing)
			{
				bool do_humm     = true;
				bool do_locators = true;

				if (s->bTeam != OUR_TEAM)
				{
					do_humm     = false;
					do_locators = false;
				}

				// Look for ownership here
				if (GetWorldItem(item_pool->iItemIndex).o.usItem == OWNERSHIP)
				{
					do_humm = false;
					MakeCharacterDialogueEventDoBattleSound(*s, BATTLE_SOUND_NOTHING, 500);
				}

				// If now open, set visible
				SetItemsVisibilityOn(grid_no, s->bLevel, ANY_VISIBILITY_VALUE, do_locators);

				// ATE: Check now many things in pool
				if (!did_missing_quote)
				{
					if (item_pool->pNext && item_pool->pNext->pNext)
					{
						MakeCharacterDialogueEventDoBattleSound(*s, BATTLE_SOUND_COOL1, 500);
					}
				else if (do_humm)
				{
					MakeCharacterDialogueEventDoBattleSound(*s, BATTLE_SOUND_HUMM, 500);
				}
				}
			}
			else
			{
				SetItemsVisibilityHidden(grid_no, s->bLevel);
			}
		}
		else
		{
			if (!closing)
			{
				MakeCharacterDialogueEventDoBattleSound(*s, BATTLE_SOUND_NOTHING, 500);
			}
		}
	}

	STRUCTURE* const new_structure = SwapStructureForPartner(structure);
	if (new_structure)
	{
		RecompileLocalMovementCosts(grid_no);
		SetRenderFlags(RENDER_FLAG_FULL);
		if (new_structure->fFlags & STRUCTURE_SWITCH)
		{ // Just turned a switch on!
			ActivateSwitchInGridNo(s, grid_no);
		}
	}
}


UICursorID GetInteractiveTileCursor(UICursorID const old_cursor, BOOLEAN const confirm)
{
	GridNo                 grid_no;
	STRUCTURE*             structure;
	LEVELNODE const* const int_node = GetCurInteractiveTileGridNoAndStructure(&grid_no, &structure);
	if (!int_node || !structure) return old_cursor;

	if (structure->fFlags & STRUCTURE_ANYDOOR)
	{
		SetDoorString(grid_no);
	}
	else if (structure->fFlags & STRUCTURE_SWITCH)
	{
		SetIntTileLocationText(gzLateLocalizedString[STR_LATE_25]);
	}
	return confirm ? OKHANDCURSOR_UICURSOR : NORMALHANDCURSOR_UICURSOR;
}


void SetActionModeDoorCursorText()
{
	// If we are over a merc, don't
	if (gUIFullTarget) return;

	GridNo     grid_no;
	STRUCTURE* structure;
	LEVELNODE const* const int_node = GetCurInteractiveTileGridNoAndStructure(&grid_no, &structure);
	if (!int_node || !structure)                  return;
	if (!(structure->fFlags & STRUCTURE_ANYDOOR)) return;
	SetDoorString(grid_no);
}


static void GetLevelNodeScreenRect(LEVELNODE const& n, SGPRect& rect, INT16 const x, INT16 const y, GridNo const gridno)
{
	// Get 'TRUE' merc position
	INT16 sTempX_S;
	INT16 sTempY_S;
	INT16 const offset_x = x - gsRenderCenterX;
	INT16 const offset_y = y - gsRenderCenterY;
	FromCellToScreenCoordinates(offset_x, offset_y, &sTempX_S, &sTempY_S);

	ETRLEObject const* pTrav;
	if (n.uiFlags & LEVELNODE_CACHEDANITILE)
	{
		ANITILE const& a = *n.pAniTile;
		pTrav = &gpTileCache[a.sCachedTileID].pImagery->vo->SubregionProperties(a.sCurrentFrame);
	}
	else
	{
		TILE_ELEMENT const* te = &gTileDatabase[n.usIndex];
		// Adjust for current frames and animations
		if (te->uiFlags & ANIMATED_TILE)
		{
			te = &gTileDatabase[te->pusFrames[te->bCurrentFrame]];
		}
		else if (n.uiFlags & LEVELNODE_ANIMATION && n.sCurrentFrame != -1)
		{
			te = &gTileDatabase[te->pusFrames[n.sCurrentFrame]];
		}
		pTrav = &te->hTileSurface->SubregionProperties(te->usRegionIndex);
	}

	INT16 sScreenX = (g_ui.m_tacticalMapCenterX) + (INT16)sTempX_S;
	INT16 sScreenY = (g_ui.m_tacticalMapCenterY) + (INT16)sTempY_S;

	// Adjust for offset position on screen
	sScreenX -= gsRenderWorldOffsetX;
	sScreenY -= gsRenderWorldOffsetY;
	sScreenY -=	gpWorldLevelData[gridno].sHeight;

	// Adjust based on interface level
	if (gsInterfaceLevel > 0)
	{
		sScreenY += ROOF_LEVEL_HEIGHT;
	}

	// Adjust for render height
	sScreenY += gsRenderHeight;

	// Add to start position of dest buffer
	sScreenX += pTrav->sOffsetX - WORLD_TILE_X / 2;
	sScreenY += pTrav->sOffsetY - WORLD_TILE_Y / 2;

	// Adjust y offset!
	sScreenY += WORLD_TILE_Y / 2;

	rect.iLeft   = sScreenX;
	rect.iTop    = sScreenY;
	rect.iRight  = sScreenX + pTrav->usWidth;
	rect.iBottom = sScreenY + pTrav->usHeight;
}


static bool RefineLogicOnStruct(GridNo, LEVELNODE const&);
static BOOLEAN RefinePointCollisionOnStruct(INT16 sTestX, INT16 sTestY, INT16 sSrcX, INT16 sSrcY, LEVELNODE const&);


void LogMouseOverInteractiveTile(INT16 const sGridNo)
{
	// OK, for now, don't allow any interactive tiles on higher interface level!
	if (gsInterfaceLevel > 0) return;

	// Also, don't allow for mercs who are on upper level
	SOLDIERTYPE const* const sel = GetSelectedMan();
	if (sel && sel->bLevel == 1) return;

	// Get World XY From gridno
	INT16 sXMapPos;
	INT16 sYMapPos;
	ConvertGridNoToCellXY(sGridNo, &sXMapPos, &sYMapPos);

	// Set mouse stuff
	auto cursorPosition{ GetCursorPos() };
	OS0MapDisplayToWorldScreen(&cursorPosition.iX, &cursorPosition.iY);

	for (LEVELNODE const* n = gpWorldLevelData[sGridNo].pStructHead; n; n = n->pNext)
	{
		SGPRect aRect;
		GetLevelNodeScreenRect(*n, aRect, sXMapPos, sYMapPos, sGridNo);

		// Make sure we are always on guy if we are on same gridno
		if (!IsPointInScreenRect(cursorPosition.iX, cursorPosition.iY, aRect)) continue;

		if (!RefinePointCollisionOnStruct(cursorPosition.iX, cursorPosition.iY, aRect.iLeft, aRect.iBottom, *n)) continue;

		if (!RefineLogicOnStruct(sGridNo, *n)) continue;

		gCurIntTile.fFound = TRUE;

		if (gfCycleIntTile) continue;

		// Accumulate them!
		gCurIntTileStack.bTiles[gCurIntTileStack.bNum].pFoundNode   = n;
		gCurIntTileStack.bTiles[gCurIntTileStack.bNum].sFoundGridNo = sGridNo;
		gCurIntTileStack.bNum++;

		// Determine if it's the best one
		if (aRect.iBottom <= gCurIntTile.sHeighestScreenY) continue;

		gCurIntTile.sHeighestScreenY = aRect.iBottom;

		gCurIntTile.pFoundNode   = n;
		gCurIntTile.sFoundGridNo = sGridNo;

		// Set stack current one
		gCurIntTileStack.bCur = gCurIntTileStack.bNum - 1;
	}
}


static LEVELNODE* InternalGetCurInteractiveTile(const BOOLEAN fRejectItemsOnTop)
{
	if (_KeyDown(SHIFT) && !OS0DirectControlOwnsSprintModifier()) return NULL;
	if (!gfOverIntTile)  return NULL;

	LEVELNODE* n = gpWorldLevelData[gCurIntTile.sGridNo].pStructHead;
	for (; n != NULL; n = n->pNext)
	{
		if (n->usIndex != gCurIntTile.sTileIndex) continue;
		if (fRejectItemsOnTop && gCurIntTile.fStructure)
		{
			// get strucuture here...
			STRUCTURE* const s = FindStructureByID(gCurIntTile.sGridNo, gCurIntTile.usStructureID);
			if (s == NULL || s->fFlags & STRUCTURE_HASITEMONTOP) return NULL;
		}
		break;
	}
	return n;
}


LEVELNODE* GetCurInteractiveTile(void)
{
	return InternalGetCurInteractiveTile(TRUE);
}


LEVELNODE* GetCurInteractiveTileGridNo(INT16* const psGridNo)
{
	LEVELNODE* const n = GetCurInteractiveTile();
	*psGridNo = (n != NULL ? gCurIntTile.sGridNo : NOWHERE);
	return n;
}


LEVELNODE* ConditionalGetCurInteractiveTileGridNoAndStructure(INT16* const psGridNo, STRUCTURE** const ppStructure, const BOOLEAN fRejectOnTopItems)
{
	GridNo     g = NOWHERE;
	STRUCTURE* s = NULL;
	LEVELNODE* n = InternalGetCurInteractiveTile(fRejectOnTopItems);
	if (n != NULL)
	{
		g = gCurIntTile.sGridNo;
		if (gCurIntTile.fStructure)
		{
			s = FindStructureByID(g, gCurIntTile.usStructureID);
			if (s == NULL) n = NULL;
		}
	}
	*ppStructure = s;
	*psGridNo    = g;
	return n;
}


LEVELNODE* GetCurInteractiveTileGridNoAndStructure(INT16* const psGridNo, STRUCTURE** const ppStructure)
{
	return ConditionalGetCurInteractiveTileGridNoAndStructure(psGridNo, ppStructure, TRUE);
}


void BeginCurInteractiveTileCheck(void)
{
	gfOverIntTile = FALSE;

	// OK, release our stack, stuff could be different!
	gfCycleIntTile = FALSE;

	// Reset some highest values
	gCurIntTile.sHeighestScreenY = 0;
	gCurIntTile.fFound           = FALSE;

	// Reset stack values
	gCurIntTileStack.bNum = 0;

}


void EndCurInteractiveTileCheck()
{
	if (gCurIntTile.fFound)
	{ // We are over this cycled node or levelnode
		CUR_INTERACTIVE_TILE const& cur_int_tile =
			gfCycleIntTile ? gCurIntTileStack.bTiles[gCurIntTileStack.bCur] :
			gCurIntTile;

		gCurIntTile.sGridNo    = cur_int_tile.sFoundGridNo;
		gCurIntTile.sTileIndex = cur_int_tile.pFoundNode->usIndex;

		if (cur_int_tile.pFoundNode->pStructureData)
		{
			gCurIntTile.usStructureID = cur_int_tile.pFoundNode->pStructureData->usStructureID;
			gCurIntTile.fStructure    = TRUE;
		}
		else
		{
			gCurIntTile.fStructure = FALSE;
		}

		gfOverIntTile = TRUE;
	}
	else
	{ // If we are in cycle mode, end it
		gfCycleIntTile = FALSE;
	}
}


static bool RefineLogicOnStruct(INT16 gridno, LEVELNODE const& n)
{
	if (n.uiFlags & LEVELNODE_CACHEDANITILE) return false;

	// See if we are on an interactable tile!
	// Try and get struct data from levelnode pointer
	if (!n.pStructureData) return false; // If no data, quit
	STRUCTURE const& structure = *n.pStructureData;

	if (!(structure.fFlags & (STRUCTURE_OPENABLE | STRUCTURE_HASITEMONTOP))) return false;

	SOLDIERTYPE const* const sel = GetSelectedMan();
	if (sel && sel->ubBodyType == ROBOTNOWEAPON) return false;

	if (structure.fFlags & STRUCTURE_ANYDOOR)
	{ // A door, we need a different definition of being visible than other structs
		if (!IsDoorVisibleAtGridNo(gridno)) return false;

		// For a OPENED door, addition requirements are: need to be in 'HAND CURSOR' mode
		if (structure.fFlags & STRUCTURE_OPEN &&
			gCurrentUIMode != HANDCURSOR_MODE &&
			gCurrentUIMode != ACTION_MODE)
		{
			return false;
		}

		if (!gGameSettings.fOptions[TOPTION_SNAP_CURSOR_TO_DOOR] &&
			gCurrentUIMode != HANDCURSOR_MODE)
		{
			return false;
		}

		return true;
	}
	else if (structure.fFlags & STRUCTURE_SWITCH)
	{ // A switch, reject in another direction
		// Find a new gridno based on switch's orientation
		switch (structure.pDBStructureRef->pDBStructure->ubWallOrientation)
		{
			case OUTSIDE_TOP_LEFT:
			case INSIDE_TOP_LEFT:
				// Move south
				gridno = NewGridNo(gridno, DirectionInc(SOUTH));
				break;

			case OUTSIDE_TOP_RIGHT:
			case INSIDE_TOP_RIGHT:
				// Move east
				gridno = NewGridNo(gridno, DirectionInc(EAST));
				break;

			default: return true; // XXX exception?
		}
	}

	// If we are hidden by a roof, reject it!
	if (!gfBasement && IsRoofVisible(gridno) && !(gTacticalStatus.uiFlags & SHOW_ALL_ITEMS))
	{
		return false;
	}

	return true;
}


static BOOLEAN RefinePointCollisionOnStruct(INT16 const test_x, INT16 const test_y, INT16 const src_x, INT16 const src_y, LEVELNODE const& n)
{
	HVOBJECT vo;
	UINT16   idx;
	if (n.uiFlags & LEVELNODE_CACHEDANITILE)
	{
		ANITILE const& a = *n.pAniTile;
		vo  = gpTileCache[a.sCachedTileID].pImagery->vo;
		idx = a.sCurrentFrame;
	}
	else
	{
		TILE_ELEMENT const* te = &gTileDatabase[n.usIndex];
		// Adjust for current frames and animations
		if (te->uiFlags & ANIMATED_TILE)
		{
			te = &gTileDatabase[te->pusFrames[te->bCurrentFrame]];
		}
		else if (n.uiFlags & LEVELNODE_ANIMATION && n.sCurrentFrame != -1)
		{
			te = &gTileDatabase[te->pusFrames[n.sCurrentFrame]];
		}
		vo  = te->hTileSurface;
		idx = te->usRegionIndex;
	}
	return CheckVideoObjectScreenCoordinateInData(vo, idx, test_x - src_x, -(test_y - src_y));
}


// This function will check the video object at SrcX and SrcY for the lack of transparency
// will return true if data found, else false
BOOLEAN CheckVideoObjectScreenCoordinateInData(HVOBJECT hSrcVObject, UINT16 usIndex, INT32 iTestX, INT32 iTestY)
{
	BOOLEAN fDataFound = FALSE;
	INT32   iTestPos, iStartPos;

	// Assertions
	Assert( hSrcVObject != NULL );

	// Get Offsets from Index into structure
	ETRLEObject const& pTrav    = hSrcVObject->SubregionProperties(usIndex);
	UINT32             usHeight = pTrav.usHeight;
	UINT32      const  usWidth  = pTrav.usWidth;

	// Calculate test position we are looking for!
	// Calculate from 0, 0 at top left!
	iTestPos	= ( ( usHeight - iTestY ) * usWidth ) + iTestX;
	iStartPos	= 0;

	UINT8 const* SrcPtr = hSrcVObject->PixData(pTrav);

	do
	{
		for (;;)
		{
			UINT8 PxCount = *SrcPtr++;
			if (PxCount == 0) break;
			if (PxCount & 0x80)
			{
				PxCount &= 0x7F;
			}
			else
			{
				if (iStartPos < iTestPos && iTestPos <= iStartPos + PxCount) return TRUE;
				SrcPtr += PxCount;
			}
			iStartPos += PxCount;
		}
		if (iStartPos >= iTestPos) break;
	}
	while (--usHeight > 0);
	return(fDataFound);

}


BOOLEAN FindOS0WorldAssetAtScreen(GridNo* gridNo, UINT8 level,
	UINT16* tileIndex, INT16 screenX, INT16 screenY)
{
	if (!gridNo || !tileIndex || *gridNo < 0 || *gridNo >= WORLD_MAX) return FALSE;
	struct RecentAssetHit
	{
		GridNo hintGrid = NOWHERE;
		UINT8 level = 0;
		INT16 screenX = 0;
		INT16 screenY = 0;
		INT16 renderX = 0;
		INT16 renderY = 0;
		UINT8 zoom = 0;
		UINT32 worldRevision = 0;
		GridNo resultGrid = NOWHERE;
		UINT16 resultTile = NO_TILE;
		GridNo hitGrid = NOWHERE;
		UINT16 hitTile = NO_TILE;
		BOOLEAN hitWasObject = FALSE;
		UINT32 checkedAt = 0;
		BOOLEAN valid = FALSE;
		BOOLEAN found = FALSE;
	};
	static RecentAssetHit recent;
	const GridNo hintGrid = *gridNo;
	OS0MapDisplayToWorldScreen(&screenX, &screenY);
	const UINT32 now = GetJA2Clock();
	const UINT8 zoom = OS0WorldZoomFactor();
	const UINT32 worldRevision = OS0WorldMutationRevision();
	const BOOLEAN sameProjection = recent.valid && recent.hintGrid == hintGrid &&
		recent.level == level && recent.renderX == gsRenderCenterX &&
		recent.renderY == gsRenderCenterY && recent.zoom == zoom &&
		recent.worldRevision == worldRevision;
	// A positive hit may be reused only for a stationary pointer.  Moving within
	// the opaque mask of a large cached sprite can cross a smaller/front-most
	// asset, so any coordinate change must rerun candidate ordering.
	if (sameProjection && recent.screenX == screenX &&
		recent.screenY == screenY && recent.found && recent.hitGrid >= 0 &&
		recent.hitGrid < WORLD_MAX)
	{
		INT16 cellX;
		INT16 cellY;
		ConvertGridNoToCellXY(recent.hitGrid, &cellX, &cellY);
		auto currentHit = [&](LEVELNODE const* node) -> LEVELNODE const*
		{
			for (; node; node = node->pNext)
			{
				if (node->usIndex != recent.hitTile ||
					!IsOS0PersistentWorldAssetNode(node)) continue;
				SGPRect rect;
				GetLevelNodeScreenRect(*node, rect, cellX, cellY,
					recent.hitGrid);
				if (IsPointInScreenRect(screenX, screenY, rect) &&
					RefinePointCollisionOnStruct(screenX, screenY,
						rect.iLeft, rect.iBottom, *node)) return node;
			}
			return nullptr;
		};
		MAP_ELEMENT const& map = gpWorldLevelData[recent.hitGrid];
		LEVELNODE const* const hit = recent.hitWasObject ?
			currentHit(map.pObjectHead) :
			currentHit(level == 0 ? map.pStructHead : map.pOnRoofHead);
		if (hit)
		{
			// Re-resolve the canonical base from the node that exists now. Tile
			// indices are reusable after carry/editor/world swaps; returning the
			// cached old base would bind interaction to a different asset.
			recent.resultGrid = recent.hitGrid;
			recent.resultTile = hit->usIndex;
			if (hit->pStructureData)
			{
				STRUCTURE* const base = FindBaseStructure(hit->pStructureData);
				if (base && base->sGridNo >= 0 && base->sGridNo < WORLD_MAX)
				{
					recent.resultGrid = base->sGridNo;
					if (LEVELNODE* const baseNode = FindLevelNodeBasedOnStructure(base))
						recent.resultTile = baseNode->usIndex;
				}
			}
			recent.screenX = screenX;
			recent.screenY = screenY;
			recent.checkedAt = now;
			*gridNo = recent.resultGrid;
			*tileIndex = recent.resultTile;
			return TRUE;
		}
	}
	// Empty ground is by far the common case. Coalesce sub-pixel motion events,
	// but never hold a positive hit without re-testing its actual sprite mask.
	if (sameProjection && !recent.found &&
		std::abs(static_cast<INT32>(screenX - recent.screenX)) <= 3 &&
		std::abs(static_cast<INT32>(screenY - recent.screenY)) <= 3 &&
		static_cast<UINT32>(now - recent.checkedAt) <= 45)
	{
		return FALSE;
	}
	const INT16 hintX = static_cast<INT16>(*gridNo % WORLD_COLS);
	const INT16 hintY = static_cast<INT16>(*gridNo / WORLD_COLS);
	GridNo bestGrid = NOWHERE;
	UINT16 bestTile = NO_TILE;
	GridNo bestHitGrid = NOWHERE;
	UINT16 bestHitTile = NO_TILE;
	BOOLEAN bestHitWasObject = FALSE;
	INT32 bestScore = INT32_MIN;

	auto scanLayer = [&](LEVELNODE const* node, GridNo candidateGrid,
		INT16 cellX, INT16 cellY, INT32 layerScore)
	{
		for (; node; node = node->pNext)
		{
			if (!IsOS0PersistentWorldAssetNode(node)) continue;
			SGPRect rect;
			GetLevelNodeScreenRect(*node, rect, cellX, cellY, candidateGrid);
			if (!IsPointInScreenRect(screenX, screenY, rect)) continue;
			if (!RefinePointCollisionOnStruct(screenX, screenY,
				rect.iLeft, rect.iBottom, *node)) continue;

			const INT32 area = std::max<INT32>(1,
				(rect.iRight - rect.iLeft) * (rect.iBottom - rect.iTop));
			const INT32 score = layerScore * 1000000 - area;
			if (score <= bestScore) continue;
			bestScore = score;
			bestGrid = candidateGrid;
			bestTile = node->usIndex;
			bestHitGrid = candidateGrid;
			bestHitTile = node->usIndex;
			bestHitWasObject = layerScore == 1;
			if (node->pStructureData)
			{
				STRUCTURE* const base = FindBaseStructure(node->pStructureData);
				if (base && base->sGridNo >= 0 && base->sGridNo < WORLD_MAX)
				{
					bestGrid = base->sGridNo;
					if (LEVELNODE* const baseNode = FindLevelNodeBasedOnStructure(base))
						bestTile = baseNode->usIndex;
				}
			}
		}
	};

	// Large props can cover several neighbouring cells. Six cells reaches the
	// widest vanilla wreck/tree sprites while keeping per-mouse-move work bounded.
	constexpr INT16 radius = 6;
	for (INT16 y = std::max<INT16>(0, hintY - radius);
		y <= std::min<INT16>(WORLD_ROWS - 1, hintY + radius); ++y)
	{
		for (INT16 x = std::max<INT16>(0, hintX - radius);
			x <= std::min<INT16>(WORLD_COLS - 1, hintX + radius); ++x)
		{
			const GridNo candidate = y * WORLD_COLS + x;
			INT16 cellX;
			INT16 cellY;
			ConvertGridNoToCellXY(candidate, &cellX, &cellY);
			MAP_ELEMENT const& map = gpWorldLevelData[candidate];
			scanLayer(map.pObjectHead, candidate, cellX, cellY, 1);
			if (level == 0)
				scanLayer(map.pStructHead, candidate, cellX, cellY, 2);
			else
				scanLayer(map.pOnRoofHead, candidate, cellX, cellY, 2);
		}
	}
	recent = { hintGrid, level, screenX, screenY, gsRenderCenterX,
		gsRenderCenterY, zoom, worldRevision, bestGrid, bestTile, bestHitGrid,
		bestHitTile, bestHitWasObject, now, TRUE,
		bestGrid != NOWHERE && bestTile < NUMBEROFTILES };
	if (!recent.found) return FALSE;
	*gridNo = bestGrid;
	*tileIndex = bestTile;
	return TRUE;
}


BOOLEAN FindOS0WorldItemAtScreen(INT32* const worldItemIndex,
	GridNo* const gridNo, UINT8 const level, INT16 screenX, INT16 screenY)
{
	if (!worldItemIndex || !gridNo || *gridNo < 0 || *gridNo >= WORLD_MAX ||
		level > 1) return FALSE;
	*worldItemIndex = -1;
	const GridNo hintGrid = *gridNo;
	OS0MapDisplayToWorldScreen(&screenX, &screenY);
	const INT16 hintX = static_cast<INT16>(hintGrid % WORLD_COLS);
	const INT16 hintY = static_cast<INT16>(hintGrid / WORLD_COLS);
	INT32 bestIndex = -1;
	GridNo bestGrid = NOWHERE;
	std::int64_t bestScore = std::numeric_limits<std::int64_t>::min();

	// Item graphics are small, but rifles and raised/on-structure objects can
	// extend beyond the projected cell beneath the pointer. Two cells covers the
	// vanilla item atlas without turning every hover refresh into a world scan.
	constexpr INT16 radius = 2;
	for (INT16 y = std::max<INT16>(0, hintY - radius);
		y <= std::min<INT16>(WORLD_ROWS - 1, hintY + radius); ++y)
	{
		for (INT16 x = std::max<INT16>(0, hintX - radius);
			x <= std::min<INT16>(WORLD_COLS - 1, hintX + radius); ++x)
		{
			const GridNo candidate = y * WORLD_COLS + x;
			INT16 cellX;
			INT16 cellY;
			ConvertGridNoToCellXY(candidate, &cellX, &cellY);
			INT32 poolOrder = 0;
			for (ITEM_POOL const* item = GetItemPool(candidate, level); item;
				item = item->pNext, ++poolOrder)
			{
				if (item->iItemIndex < 0 ||
					static_cast<size_t>(item->iItemIndex) >= gWorldItems.size() ||
					!item->pLevelNode ||
					!(item->pLevelNode->uiFlags & LEVELNODE_ITEM)) continue;
				WORLDITEM const& worldItem = GetWorldItem(item->iItemIndex);
				if (!OS0IsActionableLooseWorldItem(worldItem) ||
					worldItem.sGridNo != candidate || worldItem.ubLevel != level)
					continue;
				SGPRect rect;
				GetLevelNodeScreenRect(*item->pLevelNode, rect, cellX, cellY,
					candidate);
				if (!IsPointInScreenRect(screenX, screenY, rect) ||
					!RefinePointCollisionOnStruct(screenX, screenY,
						rect.iLeft, rect.iBottom, *item->pLevelNode)) continue;

				// Screen-bottom is the useful depth proxy across isometric cells; pool
				// order breaks ties exactly as AddStructToTail renders stacked items.
				const std::int64_t score =
					static_cast<std::int64_t>(rect.iBottom) * 1000000 +
					static_cast<std::int64_t>(y + x) * 1000 + poolOrder;
				if (score < bestScore) continue;
				bestScore = score;
				bestIndex = item->iItemIndex;
				bestGrid = candidate;
			}
		}
	}
	if (bestIndex < 0 || bestGrid == NOWHERE) return FALSE;
	*worldItemIndex = bestIndex;
	*gridNo = bestGrid;
	return TRUE;
}


BOOLEAN ShouldCheckForMouseDetections( )
{
	BOOLEAN fOK = FALSE;

	if (gsINTOldRenderCenterX != gsRenderCenterX || gsINTOldRenderCenterY != gsRenderCenterY ||
		gusINTOldMousePosX != gusMouseXPos || gusINTOldMousePosY != gusMouseYPos)
	{
		fOK = TRUE;
	}

	// Set old values
	gsINTOldRenderCenterX = gsRenderCenterX;
	gsINTOldRenderCenterY = gsRenderCenterY;

	gusINTOldMousePosX		= gusMouseXPos;
	gusINTOldMousePosY		= gusMouseYPos;

	return( fOK );
}


void CycleIntTileFindStack( UINT16 usMapPos )
{
	gfCycleIntTile = TRUE;

	// Cycle around!
	gCurIntTileStack.bCur++;

	//PLot new movement
	gfPlotNewMovement = TRUE;

	if ( gCurIntTileStack.bCur == gCurIntTileStack.bNum )
	{
		gCurIntTileStack.bCur = 0;
	}
}
