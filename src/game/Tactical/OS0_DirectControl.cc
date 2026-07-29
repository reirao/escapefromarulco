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
#include "Soldier_Macros.h"
#include "UILayout.h"

#include <SDL_keycode.h>
#include <algorithm>
#include <array>
#include <cstdlib>

namespace
{
	struct DirectControlTiming
	{
		UINT32 soldierInstance = 0;
		UINT32 manualFacingUntil = 0;
		UINT32 nextMouseFacingAt = 0;
		UINT32 nextTurnAt = 0;
		UINT32 nextStepAt = 0;
		UINT8 lookDirection = NUM_WORLD_DIRECTIONS;
		UINT8 lastTravelDirection = NUM_WORLD_DIRECTIONS;
		GridNo bufferedFrom = NOWHERE;
		BOOLEAN lookDirectionValid = FALSE;
		BOOLEAN lastReverse = FALSE;
		BOOLEAN lastSprint = FALSE;
		BOOLEAN moveInputActive = FALSE;
		BOOLEAN clearMovementFlagsWhenStationary = FALSE;
		BOOLEAN releaseWhenResumed = FALSE;
	};

	struct TravelIntent
	{
		UINT8 direction = NUM_WORLD_DIRECTIONS;
		BOOLEAN reverse = FALSE;
		BOOLEAN active = FALSE;
	};

	std::array<DirectControlTiming, TOTAL_SOLDIERS> gDirectControlTimings{};

	DirectControlTiming& TimingFor(SOLDIERTYPE const* soldier)
	{
		DirectControlTiming& timing =
			gDirectControlTimings[soldier->ubID < gDirectControlTimings.size() ?
				soldier->ubID : 0];
		if (timing.soldierInstance != soldier->uiUniqueSoldierIdValue)
		{
			timing = {};
			timing.soldierInstance = soldier->uiUniqueSoldierIdValue;
		}
		return timing;
	}

	UINT8 ActualFacing(SOLDIERTYPE const* soldier)
	{
		return soldier->bDirection < NUM_WORLD_DIRECTIONS ?
			soldier->bDirection : NORTH;
	}

	void EnsureLookDirection(SOLDIERTYPE const* soldier,
		DirectControlTiming& timing)
	{
		if (timing.lookDirectionValid) return;
		timing.lookDirection = ActualFacing(soldier);
		timing.lookDirectionValid = TRUE;
	}

	BOOLEAN IsMoving(SOLDIERTYPE const* soldier)
	{
		return (gAnimControl[soldier->usAnimState].uiFlags & ANIM_MOVING) != 0;
	}

	BOOLEAN EngineOwnsMovement(SOLDIERTYPE const* soldier)
	{
		return
			(gAnimControl[soldier->usAnimState].uiFlags & ANIM_SPECIALMOVE) ||
			(soldier->uiStatusFlags & SOLDIER_PCUNDERAICONTROL) ||
			soldier->fInNonintAnim || soldier->fRTInNonintAnim ||
			soldier->fNoAPToFinishMove || soldier->bCollapsed ||
			soldier->bBreathCollapsed || soldier->ubWaitActionToDo != 0 ||
			soldier->ubPendingAction != NO_PENDING_ACTION ||
			soldier->bEndDoorOpenCode != 0 ||
			(gTacticalStatus.uiFlags & ENGAGED_IN_CONV) ||
			gTacticalStatus.fAutoBandageMode;
	}

	BOOLEAN DirectControlTemporarilyPaused(SOLDIERTYPE const* soldier)
	{
		return (soldier->uiStatusFlags & SOLDIER_PAUSEANIMOVE) ||
			soldier->fDelayedMovement || soldier->fPausedMove;
	}

	BOOLEAN DirectControlTransitionPending(SOLDIERTYPE const* soldier,
		DirectControlTiming const& timing)
	{
		return timing.moveInputActive && !IsMoving(soldier) &&
			(soldier->fTurningUntilDone ||
				soldier->usPendingAnimation != NO_PENDING_ANIMATION ||
				soldier->ubPendingStanceChange != NO_PENDING_STANCE);
	}

	void TrimDirectControlRouteTail(SOLDIERTYPE* soldier,
		DirectControlTiming const& timing)
	{
		if (!timing.moveInputActive || !IsMoving(soldier) ||
			EngineOwnsMovement(soldier)) return;
		if (soldier->sDestination < 0 || soldier->sDestination >= WORLD_MAX) return;

		// The active interpolation still owns sDestination. Making it the final
		// destination discards only the queued tail; JA2 reaches the tile centre and
		// performs its normal reservation/path cleanup without EVENT_StopMerc's
		// visible mid-tile position snap.
		soldier->sFinalDestination = soldier->sDestination;
		soldier->sAbsoluteFinalDestination = NOWHERE;
		soldier->bPathStored = FALSE;
	}

	void ReleaseMovementIntent(SOLDIERTYPE* soldier,
		DirectControlTiming& timing)
	{
		if (!timing.moveInputActive) return;
		TrimDirectControlRouteTail(soldier, timing);
		timing.moveInputActive = FALSE;
		timing.bufferedFrom = NOWHERE;
		timing.clearMovementFlagsWhenStationary = TRUE;
		timing.releaseWhenResumed = FALSE;
	}

	void ClearReleasedMovementFlags(SOLDIERTYPE* soldier,
		DirectControlTiming& timing)
	{
		if (!timing.clearMovementFlagsWhenStationary || IsMoving(soldier)) return;
		// Reverse belongs to the direct-control segment. Leaving it behind changes
		// the next ordinary click path into an unintended backward move.
		soldier->bReverse = FALSE;
		timing.clearMovementFlagsWhenStationary = FALSE;
	}

	void FollowMouse(SOLDIERTYPE* soldier, DirectControlTiming& timing,
		BOOLEAN manualTurn, BOOLEAN applyFacing)
	{
		EnsureLookDirection(soldier, timing);
		const UINT32 now = GetJA2Clock();
		if (manualTurn || now < timing.manualFacingUntil ||
			now < timing.nextMouseFacingAt)
			return;
		if (gusMouseXPos < gsVIEWPORT_START_X || gusMouseXPos > gsVIEWPORT_END_X ||
			gusMouseYPos < gsVIEWPORT_WINDOW_START_Y ||
			gusMouseYPos > gsVIEWPORT_WINDOW_END_Y) return;
		INT16 worldX;
		INT16 worldY;
		if (!GetMouseWorldCoords(&worldX, &worldY)) return;
		if (std::abs(static_cast<INT32>(worldX - soldier->dXPos)) <
				CELL_X_SIZE / 2 &&
			std::abs(static_cast<INT32>(worldY - soldier->dYPos)) <
				CELL_Y_SIZE / 2) return;
		const UINT8 direction = atan8(static_cast<INT16>(soldier->dXPos),
			static_cast<INT16>(soldier->dYPos), worldX, worldY);
		timing.lookDirection = direction;
		timing.lookDirectionValid = TRUE;
		// JA2's moving animation owns desiredDirection. Writing a second facing
		// direction every frame fights EVENT_InternalSetSoldierDestination and makes
		// both click paths and WASD visibly twitch. Mouse facing is applied only while
		// stationary; during movement it remains a virtual intent for the next tile.
		if (applyFacing && !IsMoving(soldier) &&
			soldier->bDesiredDirection != direction)
			EVENT_SetSoldierDesiredDirection(soldier, direction);
		// Mouse motion can arrive much faster than animation ticks. Throttling the
		// desired-direction event keeps the avatar responsive without restarting
		// the same turn animation dozens of times per frame.
		timing.nextMouseFacingAt = now + 70;
	}

	TravelIntent ResolveTravelDirection(UINT8 const facing, BOOLEAN const forward,
		BOOLEAN const backward, BOOLEAN const left, BOOLEAN const right)
	{
		const INT8 longitudinal = static_cast<INT8>(forward) -
			static_cast<INT8>(backward);
		const INT8 lateral = static_cast<INT8>(right) -
			static_cast<INT8>(left);
		if (longitudinal == 0 && lateral == 0) return {};

		TravelIntent intent;
		intent.active = TRUE;
		intent.reverse = longitudinal < 0 ||
			(longitudinal == 0 && lateral != 0);
		if (longitudinal > 0)
		{
			intent.direction = lateral < 0 ? OneCCDirection(facing) :
				(lateral > 0 ? OneCDirection(facing) : facing);
			return intent;
		}
		if (longitudinal < 0)
		{
			const UINT8 opposite = OppositeDirection(facing);
			// Around the opposite axis local left/right are mirrored: facing north,
			// back-left is south-west (clockwise from south), not south-east.
			intent.direction = lateral < 0 ? OneCDirection(opposite) :
				(lateral > 0 ? OneCCDirection(opposite) : opposite);
			return intent;
		}
		intent.direction = lateral < 0 ? TwoCCDirection(facing) :
			TwoCDirection(facing);
		return intent;
	}

	UINT16 MovementModeForIntent(SOLDIERTYPE const* soldier,
		TravelIntent const& intent, UINT8 const stance, BOOLEAN const moving,
		BOOLEAN const sprint)
	{
		const UINT16 base = sprint ? RUNNING :
			GetMoveStateBasedOnStance(soldier, stance);
		if (!moving || !intent.reverse || stance == ANIM_PRONE) return base;

		const BOOLEAN perpendicular = soldier->bDirection ==
			gPurpendicularDirection[soldier->bDirection][intent.direction];
		const UINT16 substituted = perpendicular ? SIDE_STEP :
			(stance == ANIM_CROUCH ? SWAT_BACKWARDS : WALK_BACKWARDS);
		// EVENT_InternalGetNewSoldierPath's realtime append path compares the UI
		// movement mode before JA2 performs reverse-animation substitution. Feeding
		// WALKING for every extension therefore restarted SIDE_STEP/WALK_BACKWARDS
		// at frame zero on every tile. Preserve the already-correct animation.
		// Select the effective reverse/strafe animation immediately, including the
		// first transition from ordinary forward movement. Returning WALKING here
		// merely because the substitute is not active yet makes JA2 see no movement-
		// mode change and the sprite continues to play forward while travelling back.
		return IsAnimationValidForBodyType(*soldier, substituted) ? substituted : base;
	}
}

BOOLEAN OS0IsDirectControlKey(UINT32 key)
{
	return key == SDLK_w || key == SDLK_a || key == SDLK_s || key == SDLK_d ||
		key == SDLK_q || key == SDLK_e || key == 'W' || key == 'A' ||
		key == 'S' || key == 'D' || key == 'Q' || key == 'E' ||
		key == SDLK_LSHIFT || key == SDLK_RSHIFT;
}

void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled,
	BOOLEAN attackMode)
{
	if (!soldier) return;
	DirectControlTiming& timing = TimingFor(soldier);
	if (!soldier->bActive)
	{
		timing.moveInputActive = FALSE;
		timing.lookDirectionValid = FALSE;
		timing.releaseWhenResumed = FALSE;
		return;
	}
	ClearReleasedMovementFlags(soldier, timing);
	if (!enabled || soldier->bTeam != OUR_TEAM || soldier->bLife < OKLIFE ||
		!OK_CONTROLLABLE_MERC(soldier) ||
		(gTacticalStatus.uiFlags & INCOMBAT))
	{
		ReleaseMovementIntent(soldier, timing);
		return;
	}

	EnsureLookDirection(soldier, timing);
	const BOOLEAN forward = _KeyDown(SDLK_w);
	const BOOLEAN backward = _KeyDown(SDLK_s);
	const BOOLEAN left = _KeyDown(SDLK_a);
	const BOOLEAN right = _KeyDown(SDLK_d);
	const BOOLEAN turnLeft = _KeyDown(SDLK_q);
	const BOOLEAN turnRight = _KeyDown(SDLK_e);
	const BOOLEAN manualTurn = turnLeft != turnRight;
	const BOOLEAN directMoveInput = forward || backward || left || right;
	if (EngineOwnsMovement(soldier))
	{
		ReleaseMovementIntent(soldier, timing);
		return;
	}
	if (DirectControlTemporarilyPaused(soldier) ||
		DirectControlTransitionPending(soldier, timing))
	{
		// A collision wait or JA2's turn-then-walk bridge is not a hand-off. Keep
		// the route owner and remember a released key until the engine can safely
		// trim the buffered tail; otherwise the merc resumes on its own afterwards.
		timing.releaseWhenResumed = timing.moveInputActive && !directMoveInput;
		return;
	}
	if (timing.releaseWhenResumed)
	{
		timing.releaseWhenResumed = FALSE;
		if (!directMoveInput)
		{
			ReleaseMovementIntent(soldier, timing);
			return;
		}
	}
	const BOOLEAN moving = IsMoving(soldier);
	// Merely selecting a merc must not rotate it throughout an ordinary click
	// path. The pointer owns facing in FIGHT, or while direct movement is engaged.
	FollowMouse(soldier, timing, manualTurn,
		(attackMode || directMoveInput) && !moving);

	const UINT32 now = GetJA2Clock();
	if (manualTurn && now >= timing.nextTurnAt)
	{
		const UINT8 direction = turnLeft ?
			OneCCDirection(timing.lookDirection) :
			OneCDirection(timing.lookDirection);
		timing.lookDirection = direction;
		timing.lookDirectionValid = TRUE;
		if (!moving) EVENT_SetSoldierDesiredDirection(soldier, direction);
		timing.manualFacingUntil = now + 420;
		timing.nextTurnAt = now + 135;
	}
	if (!forward && !backward && !left && !right)
	{
		ReleaseMovementIntent(soldier, timing);
		return;
	}

	const TravelIntent intent = ResolveTravelDirection(timing.lookDirection,
		forward, backward, left, right);
	if (!intent.active)
	{
		ReleaseMovementIntent(soldier, timing);
		return;
	}
	const UINT8 stance = gAnimControl[soldier->usAnimState].ubEndHeight;
	const BOOLEAN sprint =
		(_KeyDown(SDLK_LSHIFT) || _KeyDown(SDLK_RSHIFT)) && stance == ANIM_STAND &&
		!intent.reverse && !MercInWater(soldier);
	const BOOLEAN inputChanged = !timing.moveInputActive ||
		timing.lastTravelDirection != intent.direction ||
		timing.lastReverse != intent.reverse || timing.lastSprint != sprint;
	const UINT8 remainingPath = soldier->ubPathDataSize > soldier->ubPathIndex ?
		static_cast<UINT8>(soldier->ubPathDataSize - soldier->ubPathIndex) : 0;
	// JA2 can retarget an already-moving real-time merc without restarting the
	// animation. Keep at most one unconsumed tile buffered: held keys flow across
	// tile boundaries. Release trims that buffered tail to the active destination.
	if (now < timing.nextStepAt) return;
	if (!inputChanged && moving && remainingPath > 1) return;
	const GridNo pathBase = moving && soldier->sDestination >= 0 &&
		soldier->sDestination < WORLD_MAX ? soldier->sDestination : soldier->sGridNo;
	// Mouse-facing changes are pending intent while interpolation is in progress.
	// Replanning more than once from the same active tile resets PathIndex and
	// visibly bends/restarts the current segment. Consume the latest intent only
	// when JA2 advances sDestination to the next tile anchor.
	if (moving && timing.moveInputActive && timing.bufferedFrom == pathBase) return;
	const GridNo destination = NewGridNo(pathBase, DirectionInc(intent.direction));
	if (destination == pathBase ||
		!NewOKDestination(soldier, destination, TRUE, soldier->bLevel))
	{
		TrimDirectControlRouteTail(soldier, timing);
		timing.nextStepAt = now + 120;
		return;
	}

	const INT16 previousMovementMode = soldier->usUIMovementMode;
	const BOOLEAN previousMovementFast = soldier->fUIMovementFast;
	const INT8 previousReverse = soldier->bReverse;
	const INT8 previousGoodContPath = soldier->bGoodContPath;
	const GridNo previousFinalDestination = soldier->sFinalDestination;
	const GridNo previousAbsoluteFinalDestination =
		soldier->sAbsoluteFinalDestination;
	const UINT8 previousPathIndex = soldier->ubPathIndex;
	const UINT8 previousPathSize = soldier->ubPathDataSize;
	const BOOLEAN previousPathStored = soldier->bPathStored;
	std::array<UINT8, MAX_PATH_LIST_SIZE> previousPath{};
	std::copy_n(soldier->ubPathingData, previousPath.size(),
		previousPath.begin());
	soldier->fUIMovementFast = sprint;
	soldier->usUIMovementMode = MovementModeForIntent(soldier, intent, stance,
		moving, sprint);
	soldier->bReverse = intent.reverse;
	const BOOLEAN pathAccepted = EVENT_InternalGetNewSoldierPath(soldier,
		destination, soldier->usUIMovementMode, TRUE, FALSE);
	// Match vanilla UI ownership: fUIMovementFast influences mode selection but
	// is not persistent movement state.
	soldier->fUIMovementFast = previousMovementFast;
	if (!pathAccepted)
	{
		soldier->usUIMovementMode = previousMovementMode;
		soldier->bReverse = previousReverse;
		soldier->bGoodContPath = previousGoodContPath;
		soldier->sFinalDestination = previousFinalDestination;
		soldier->sAbsoluteFinalDestination = previousAbsoluteFinalDestination;
		soldier->ubPathIndex = previousPathIndex;
		soldier->ubPathDataSize = previousPathSize;
		soldier->bPathStored = previousPathStored;
		std::copy(previousPath.begin(), previousPath.end(),
			soldier->ubPathingData);
		TrimDirectControlRouteTail(soldier, timing);
		timing.nextStepAt = now + 120;
		return;
	}
	soldier->sAbsoluteFinalDestination = NOWHERE;
	soldier->bPathStored = FALSE;
	timing.moveInputActive = TRUE;
	timing.bufferedFrom = pathBase;
	timing.clearMovementFlagsWhenStationary = FALSE;
	timing.lastTravelDirection = intent.direction;
	timing.lastReverse = intent.reverse;
	timing.lastSprint = sprint;
	timing.nextStepAt = now + 45;
}

void OS0ResetDirectControl()
{
	gDirectControlTimings = {};
}
