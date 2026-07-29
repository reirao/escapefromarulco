/* Escape from Arulco: one owner for mouse-facing and held-key movement. */

#include "OS0_DirectControl.h"

#include "Animation_Control.h"
#include "Game_Clock.h"
#include "Input.h"
#include "Isometric_Utils.h"
#include "Overhead.h"
#include "PathAI.h"
#include "RenderWorld.h"
#include "Soldier_Control.h"
#include "UILayout.h"

#include <SDL_keycode.h>
#include <array>

namespace
{
	struct DirectControlTiming
	{
		UINT32 manualFacingUntil = 0;
		UINT32 nextTurnAt = 0;
		UINT32 nextStepAt = 0;
	};

	std::array<DirectControlTiming, TOTAL_SOLDIERS> gDirectControlTimings{};

	DirectControlTiming& TimingFor(SOLDIERTYPE const* soldier)
	{
		return gDirectControlTimings[soldier->ubID < gDirectControlTimings.size() ?
			soldier->ubID : 0];
	}

	UINT8 CurrentFacing(SOLDIERTYPE const* soldier)
	{
		return soldier->bDesiredDirection >= 0 &&
			soldier->bDesiredDirection < NUM_WORLD_DIRECTIONS ?
			static_cast<UINT8>(soldier->bDesiredDirection) : soldier->bDirection;
	}

	void FollowMouse(SOLDIERTYPE* soldier, BOOLEAN attackMode)
	{
		if (!attackMode && GetJA2Clock() < TimingFor(soldier).manualFacingUntil)
			return;
		if (gusMouseXPos < gsVIEWPORT_START_X || gusMouseXPos > gsVIEWPORT_END_X ||
			gusMouseYPos < gsVIEWPORT_WINDOW_START_Y ||
			gusMouseYPos > gsVIEWPORT_WINDOW_END_Y) return;
		INT16 worldX;
		INT16 worldY;
		if (!GetMouseXY(&worldX, &worldY)) return;
		const GridNo gridNo = MAPROWCOLTOPOS(worldY, worldX);
		if (gridNo < 0 || gridNo >= WORLD_MAX || gridNo == soldier->sGridNo) return;
		const UINT8 direction = GetDirectionFromGridNo(gridNo, soldier);
		if (soldier->bDesiredDirection != direction)
			EVENT_SetSoldierDesiredDirection(soldier, direction);
	}
}

BOOLEAN OS0IsDirectControlKey(UINT32 key)
{
	return key == SDLK_w || key == SDLK_a || key == SDLK_s || key == SDLK_d ||
		key == SDLK_q || key == SDLK_e || key == 'W' || key == 'A' ||
		key == 'S' || key == 'D' || key == 'Q' || key == 'E';
}

void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled,
	BOOLEAN attackMode)
{
	if (!enabled || !soldier || soldier->bTeam != OUR_TEAM ||
		soldier->bLife < OKLIFE) return;
	FollowMouse(soldier, attackMode);

	const BOOLEAN forward = _KeyDown(SDLK_w);
	const BOOLEAN backward = _KeyDown(SDLK_s);
	const BOOLEAN left = _KeyDown(SDLK_a);
	const BOOLEAN right = _KeyDown(SDLK_d);
	const BOOLEAN turnLeft = _KeyDown(SDLK_q);
	const BOOLEAN turnRight = _KeyDown(SDLK_e);
	if (!forward && !backward && !left && !right && !turnLeft && !turnRight)
		return;

	const UINT32 now = GetJA2Clock();
	DirectControlTiming& timing = TimingFor(soldier);
	const BOOLEAN moving =
		(gAnimControl[soldier->usAnimState].uiFlags & ANIM_MOVING) != 0;
	if ((turnLeft || turnRight) && now >= timing.nextTurnAt)
	{
		const UINT8 direction = turnLeft ?
			OneCCDirection(CurrentFacing(soldier)) :
			OneCDirection(CurrentFacing(soldier));
		EVENT_SetSoldierDesiredDirection(soldier, direction);
		timing.manualFacingUntil = now + 420;
		timing.nextTurnAt = now + 135;
		SetRenderFlags(RENDER_FLAG_FULL);
	}
	if (!forward && !backward && !left && !right) return;
	if (moving || now < timing.nextStepAt) return;

	const UINT8 facing = CurrentFacing(soldier);
	UINT8 direction = facing;
	BOOLEAN reverse = FALSE;
	if (backward)
	{
		direction = OppositeDirection(facing);
		reverse = TRUE;
	}
	else if (left)
	{
		direction = TwoCCDirection(facing);
		reverse = TRUE;
	}
	else if (right)
	{
		direction = TwoCDirection(facing);
		reverse = TRUE;
	}
	const GridNo destination = NewGridNo(soldier->sGridNo, DirectionInc(direction));
	if (destination == soldier->sGridNo ||
		!NewOKDestination(soldier, destination, TRUE, soldier->bLevel))
	{
		timing.nextStepAt = now + 120;
		return;
	}

	const UINT8 stance = gAnimControl[soldier->usAnimState].ubEndHeight;
	const BOOLEAN sprint =
		(_KeyDown(SDLK_LSHIFT) || _KeyDown(SDLK_RSHIFT)) && stance == ANIM_STAND;
	soldier->usUIMovementMode = sprint ? RUNNING :
		GetMoveStateBasedOnStance(soldier, stance);
	soldier->fUIMovementFast = sprint;
	soldier->bReverse = reverse;
	if (!EVENT_InternalGetNewSoldierPath(soldier, destination,
		soldier->usUIMovementMode, TRUE, FALSE))
	{
		soldier->bReverse = FALSE;
		timing.nextStepAt = now + 120;
		return;
	}
	timing.nextStepAt = now + 24;
	SetRenderFlags(RENDER_FLAG_FULL);
}

void OS0ResetDirectControl()
{
	gDirectControlTimings = {};
}
