#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;

BOOLEAN OS0IsDirectControlKey(UINT32 key);
BOOLEAN OS0HandleTurnBasedDirectControlKey(SOLDIERTYPE* soldier, UINT32 key,
	UINT16 eventType, BOOLEAN enabled);
void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled,
	BOOLEAN attackMode);
void OS0ResetDirectControl();
