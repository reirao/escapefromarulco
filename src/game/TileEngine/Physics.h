/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#ifndef __PHYSICS_H
#define __PHYSICS_H

#include "Item_Types.h"
#include "JA2Types.h"
#include "WorldDef.h"


extern UINT32 guiNumObjectSlots;

struct vector_3
{
	float x, y, z;
};

// Deterministic physical description of a tile-world structure.  JA2 already
// simulates ballistic REAL_OBJECTs; this profile exposes the same material
// vocabulary to furniture, containers and environmental resources without
// introducing a non-deterministic third-party rigid-body world.
struct WORLD_PHYSICS_PROFILE
{
	const char* materialName;
	float massKg;
	float friction;
	float restitution;
	UINT8 integrity;
	BOOLEAN portableObject;
};

struct REAL_OBJECT
{
	BOOLEAN      fAllocated;
	BOOLEAN      fAlive;
	BOOLEAN      fApplyFriction;
	BOOLEAN      fVisible;
	BOOLEAN      fInWater;
	BOOLEAN      fTestObject;
	BOOLEAN      fTestEndedWithCollision;
	BOOLEAN      fTestPositionNotSet;

	float        TestZTarget;
	float        AppliedMu;

	vector_3     Position;
	vector_3     TestTargetPosition;
	vector_3     OldPosition;
	vector_3     Velocity;
	vector_3     OldVelocity;
	vector_3     InitialForce;
	vector_3     Force;
	vector_3     CollisionNormal;
	vector_3     CollisionVelocity;
	float        CollisionElasticity;

	INT16        sGridNo;
	LEVELNODE    *pNode;
	LEVELNODE    *pShadow;

	INT16        sConsecutiveCollisions;
	INT16        sConsecutiveZeroVelocityCollisions;
	INT32        iOldCollisionCode;

	FLOAT        dLifeLength;
	FLOAT        dLifeSpan;
	OBJECTTYPE   Obj;
	SOLDIERTYPE* owner;
	INT16        sFirstGridNo;
	BOOLEAN      fFirstTimeMoved;
	UINT8        ubActionCode;
	SOLDIERTYPE* target;
	BOOLEAN      fDropItem;
	UINT32       uiNumTilesMoved;
	BOOLEAN      fCatchGood;
	BOOLEAN      fAttemptedCatch;
	BOOLEAN      fCatchAnimOn;
	BOOLEAN      fCatchCheckDone;
	BOOLEAN      fEndedWithCollisionPositionSet;
	vector_3     EndedWithCollisionPosition;
	BOOLEAN      fHaveHitGround;
	BOOLEAN      fPotentialForDebug;
	INT16        sLevelNodeGridNo;
	UINT32       uiSoundID;
	UINT8        ubLastTargetTakenDamage;
};


// OBJECT LIST STUFF
REAL_OBJECT* CreatePhysicalObject(const OBJECTTYPE* pGameObj, float dLifeLength, float xPos, float yPos, float zPos, float xForce, float yForce, float zForce, SOLDIERTYPE* owner, UINT8 ubActionCode, SOLDIERTYPE* target);
void RemoveAllPhysicsObjects(void);

WORLD_PHYSICS_PROFILE GetWorldPhysicsProfile(const STRUCTURE* structure);
BOOLEAN CanSoldierMoveWorldStructure(const SOLDIERTYPE* soldier, const STRUCTURE* structure);


BOOLEAN CalculateLaunchItemChanceToGetThrough(const SOLDIERTYPE* pSoldier, const OBJECTTYPE* pItem, INT16 sGridNo, UINT8 ubLevel, INT16 sEndZ, INT16* psFinalGridNo, BOOLEAN fArmed, INT8* pbLevel, BOOLEAN fFromUI);

void CalculateLaunchItemParamsForThrow(SOLDIERTYPE* pSoldier, INT16 sGridNo, UINT8 ubLevel, INT16 sZPos, OBJECTTYPE* pItem, INT8 bMissBy, UINT8 ubActionCode, SOLDIERTYPE* target);


// SIMULATE WORLD
void SimulateWorld(void);


void SavePhysicsTableToSaveGameFile(HWFILE);
void LoadPhysicsTableFromSavedGameFile(HWFILE);

#endif
