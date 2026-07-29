#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;

BOOLEAN OS0IsDirectControlKey(UINT32 key);
void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled,
	BOOLEAN attackMode);
void OS0ResetDirectControl();
