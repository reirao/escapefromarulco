#ifndef __INTERACTIVE_TILES_H
#define __INTERACTIVE_TILES_H

#include "Interface_Cursors.h"
#include "JA2Types.h"
#include "WorldDef.h"


#define INTTILE_DOOR_OPENSPEED	70


void StartInteractiveObject(GridNo, STRUCTURE const&, SOLDIERTYPE&, UINT8 direction);
BOOLEAN StartInteractiveObjectFromMouse( SOLDIERTYPE *pSoldier, UINT8 ubDirection );
UICursorID GetInteractiveTileCursor(UICursorID old_cursor, BOOLEAN fConfirm);
bool SoldierHandleInteractiveObject(SOLDIERTYPE&);

void HandleStructChangeFromGridNo(SOLDIERTYPE*, GridNo);


void BeginCurInteractiveTileCheck(void);
void EndCurInteractiveTileCheck(void);
void LogMouseOverInteractiveTile( INT16 sGridNo );
BOOLEAN ShouldCheckForMouseDetections(void);

void CycleIntTileFindStack( UINT16 usMapPos );
void SetActionModeDoorCursorText(void);

LEVELNODE *GetCurInteractiveTile(void);
LEVELNODE *GetCurInteractiveTileGridNo( INT16 *psGridNo );
LEVELNODE *GetCurInteractiveTileGridNoAndStructure( INT16 *psGridNo, STRUCTURE **ppStructure );
LEVELNODE *ConditionalGetCurInteractiveTileGridNoAndStructure( INT16 *psGridNo, STRUCTURE **ppStructure, BOOLEAN fRejectOnTopItems );

BOOLEAN CheckVideoObjectScreenCoordinateInData(HVOBJECT hSrcVObject, UINT16 usIndex, INT32 iTestX, INT32 iTestY);

// Pixel-accurate OS0 scenery picking. Unlike JA2's original interactive-tile
// search this also accepts ordinary object/structure sprites that extend beyond
// the grid cell underneath the pointer.
BOOLEAN FindOS0WorldAssetAtScreen(GridNo* gridNo, UINT8 level,
	UINT16* tileIndex, INT16 screenX, INT16 screenY);

// Pixel-accurate loose-item picking. The returned index identifies the exact
// ITEM_POOL entry whose sprite was hit; callers must still revalidate it before
// detaching because pool order and indices can change after a world mutation.
BOOLEAN FindOS0WorldItemAtScreen(INT32* worldItemIndex, GridNo* gridNo,
	UINT8 level, INT16 screenX, INT16 screenY);

// Shared admission rule for persistent scenery. Path footprints, cover glyphs,
// item nodes and other engine-owned overlays must never become OS//0 assets.
BOOLEAN IsOS0PersistentWorldAssetNode(LEVELNODE const* node);

#endif
