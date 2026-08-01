#ifndef __HANDLE_ITEMS_H
#define __HANDLE_ITEMS_H

#include "JA2Types.h"
#include "World_Items.h"

#include <vector>


enum ItemHandleResult
{
	ITEM_HANDLE_OK                    =  1,
	ITEM_HANDLE_RELOADING             = -1,
	ITEM_HANDLE_UNCONSCIOUS           = -2,
	ITEM_HANDLE_NOAPS                 = -3,
	ITEM_HANDLE_NOAMMO                = -4,
	ITEM_HANDLE_CANNOT_GETTO_LOCATION = -5,
	ITEM_HANDLE_BROKEN                = -6,
	ITEM_HANDLE_NOROOM                = -7,
	ITEM_HANDLE_REFUSAL               = -8
};

// Define for code to try and pickup all items....
#define ITEM_PICKUP_ACTION_ALL	32000
#define ITEM_PICKUP_SELECTION	31000

#define ITEM_IGNORE_Z_LEVEL	-1

enum Visibility
{
	ANY_VISIBILITY_VALUE = -10,
	HIDDEN_ITEM          =  -4,
	BURIED               =  -3,
	HIDDEN_IN_OBJECT     =  -2,
	INVISIBLE            =  -1,
	VISIBILITY_0         =   0, // XXX investigate
	VISIBLE              =   1
};

// OS//0 metadata stored alongside a container's spatial contents.  Keeping the
// discriminator here lets every destruction/move path apply the same ownership
// rules instead of duplicating a magic ACTION_ITEM check.
constexpr UINT8 OS0_CONTAINER_SEED_MARKER = 0xE0;

#define	ITEM_LOCATOR_LOCKED 0x02


// Check if at least one item in the item pool is visible
bool IsItemPoolVisible(ITEM_POOL const*);


struct ITEM_POOL
{
	ITEM_POOL *pNext;
	INT32     iItemIndex;
	LEVELNODE *pLevelNode;
};


ItemHandleResult HandleItem(SOLDIERTYPE* pSoldier, INT16 usGridNo, INT8 bLevel, UINT16 usHandItem, BOOLEAN fFromUI);

// Legacy pickup request.  OS0 callers with an exact world item must use the
// guarded overload below so an asynchronously recycled pool index cannot pick
// up a replacement object.
void SoldierPickupItem( SOLDIERTYPE *pSoldier, INT32 iItemIndex, INT16 sGridNo, INT8 bZLevel );
BOOLEAN OS0SoldierPickupExactWorldItem(SOLDIERTYPE* pSoldier,
	INT32 iItemIndex, INT16 sGridNo, INT8 bZLevel);
// Gate for OS0 paths which detach a world object without the native pickup
// animation. It validates exact location/visibility and delegates booby-trap
// discovery before ownership may move.
BOOLEAN OS0PrepareWorldItemForDirectDetach(SOLDIERTYPE* pSoldier,
	INT32 iItemIndex, INT16 sGridNo, UINT8 level);

void HandleSoldierPickupItem( SOLDIERTYPE *pSoldier, INT32 iItemIndex, INT16 sGridNo, INT8 bZLevel );
void HandleFlashingItems(void);

void SoldierDropItem(SOLDIERTYPE*, OBJECTTYPE*);

void HandleSoldierThrowItem( SOLDIERTYPE *pSoldier, INT16 sGridNo );
SOLDIERTYPE* VerifyGiveItem(SOLDIERTYPE* pSoldier);
void SoldierGiveItemFromAnimation( SOLDIERTYPE *pSoldier );
void SoldierGiveItem( SOLDIERTYPE *pSoldier, SOLDIERTYPE *pTargetSoldier, OBJECTTYPE *pObject, INT8 bInvPos );


void NotifySoldiersToLookforItems(void);
void AllSoldiersLookforItems(void);


void SoldierGetItemFromWorld(SOLDIERTYPE* pSoldier, INT32 iItemIndex, INT16 sGridNo, INT8 bZLevel, const BOOLEAN* pfSelectionList);

INT32 AddItemToPool(INT16 sGridNo, OBJECTTYPE *pObject, Visibility, UINT8 ubLevel, UINT16 usFlags, INT8 bRenderZHeightAboveLevel);
INT32 InternalAddItemToPool(INT16* psGridNo, OBJECTTYPE* pObject, Visibility, UINT8 ubLevel, UINT16 usFlags, INT8 bRenderZHeightAboveLevel);
// Reconstruct one serialized world item. Container-owned children keep their
// saved grid exactly: map modifications are replayed after the item temp file,
// so snapping them to a not-yet-removed base-map structure would orphan them
// from a container which was moved onto that structure's former footprint.
// Every other item retains AddItemToPool's native placement behaviour.
INT32 OS0RestorePersistedWorldItemToPool(WORLDITEM const& item,
	GridNo requestedGridNo);

GridNo     AdjustGridNoForItemPlacement(SOLDIERTYPE*, GridNo);
ITEM_POOL* GetItemPool(UINT16 usMapPos, UINT8 ubLevel);
void       DrawItemPoolList(const ITEM_POOL* pItemPool, INT8 bZLevel, INT16 sXPos, INT16 sYPos);
void       RemoveItemFromPool(WORLDITEM& wi);
void       MoveItemPools(INT16 sStartPos, INT16 sEndPos);

BOOLEAN SetItemsVisibilityOn(GridNo, UINT8 level, Visibility bAllGreaterThan, BOOLEAN fSetLocator);

void SetItemsVisibilityHidden(GridNo, UINT8 level);

BOOLEAN OS0IsContainerSeedMarker(WORLDITEM const& item);
// Closed native containers use HIDDEN_IN_OBJECT. Open native containers retain
// WORLD_ITEM_DONTRENDER while changing visibility to VISIBLE. Both forms are
// the same container-owned content and must travel/loot/spill together.
BOOLEAN OS0IsContainerContentItem(WORLDITEM const& item);
BOOLEAN OS0IsContainerOwnedItem(WORLDITEM const& item);
// A loose item is a visible physical world object, never a hidden/open
// container child or OS//0 metadata marker. This is the single predicate used
// by native hand pickup and the object-action registry.
BOOLEAN OS0IsActionableLooseWorldItem(WORLDITEM const& item);
INT32 OS0FindActionableLooseWorldItem(GridNo gridNo, UINT8 level);
// A destroyed container no longer owns its items. Reveal its real contents in
// place and remove only OS//0's private seed marker.
BOOLEAN OS0SpillContainerContents(GridNo gridNo, UINT8 level);

void RenderTopmostFlashingItems(void);

void RemoveAllUnburiedItems( INT16 sGridNo, UINT8 ubLevel );


BOOLEAN DoesItemPoolContainAnyHiddenItems(const ITEM_POOL* pItemPool);


void HandleSoldierDropBomb( SOLDIERTYPE *pSoldier, INT16 sGridNo );
void HandleSoldierUseRemote( SOLDIERTYPE *pSoldier, INT16 sGridNo );

BOOLEAN ItemPoolOKForDisplay(const ITEM_POOL* pItemPool, INT8 bZLevel);

void SoldierHandleDropItem( SOLDIERTYPE *pSoldier );

INT8 GetZLevelOfItemPoolGivenStructure(INT16 sGridNo, UINT8 ubLevel, const STRUCTURE* pStructure);

INT8 GetLargestZLevelOfItemPool(const ITEM_POOL* pItemPool);

void PutItemPoolOnGround(GridNo const gridNo, uint8_t const level);

BOOLEAN NearbyGroundSeemsWrong( SOLDIERTYPE * pSoldier, INT16 sGridNo, BOOLEAN fCheckAroundGridno, INT16 * psProblemGridNo );
void MineSpottedDialogueCallBack( void );

extern INT16 gsBoobyTrapGridNo;
extern SOLDIERTYPE * gpBoobyTrapSoldier;
void RemoveBlueFlag( INT16 sGridNo, INT8 bLevel  );

// check if item is booby trapped
BOOLEAN ContinuePastBoobyTrapInMapScreen( OBJECTTYPE *pObject, SOLDIERTYPE *pSoldier );

void RefreshItemPools(const std::vector<WORLDITEM>& pItemList);

BOOLEAN ItemTypeExistsAtLocation( INT16 sGridNo, UINT16 usItem, UINT8 ubLevel, INT32 * piItemIndex );

INT16 FindNearestAvailableGridNoForItem( INT16 sSweetGridNo, INT8 ubRadius );

void MakeNPCGrumpyForMinorOffense(SOLDIERTYPE* pSoldier, const SOLDIERTYPE* pOffendingSoldier);

BOOLEAN AnyItemsVisibleOnLevel(const ITEM_POOL* pItemPool, INT8 bZLevel);

void RemoveFlashItemSlot(ITEM_POOL const*);

void ToggleItemGlow(BOOLEAN fOn);

BOOLEAN HandleCheckForBadChangeToGetThrough(SOLDIERTYPE* pSoldier, const SOLDIERTYPE* pTargetSoldier, INT16 sTargetGridNo, INT8 bLevel);

#endif
