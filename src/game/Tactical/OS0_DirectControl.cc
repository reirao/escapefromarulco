/* Escape from Arulco: one owner for mouse-facing and held-key movement. */

#include "OS0_DirectControl.h"

#include "OS0_IngameUI.h"

#include "Animation_Control.h"
#include "Cursor_Control.h"
#include "Game_Clock.h"
#include "Handle_UI.h"
#include "Input.h"
#include "Isometric_Utils.h"
#include "Overhead.h"
#include "PathAI.h"
#include "Points.h"
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
		UINT32 pendingTurnBasedKey = 0;
		UINT8 lookDirection = NUM_WORLD_DIRECTIONS;
		UINT8 lastTravelDirection = NUM_WORLD_DIRECTIONS;
		GridNo bufferedFrom = NOWHERE;
		BOOLEAN lookDirectionValid = FALSE;
		BOOLEAN lastReverse = FALSE;
		BOOLEAN lastSprint = FALSE;
		BOOLEAN moveInputActive = FALSE;
		BOOLEAN clearMovementFlagsWhenStationary = FALSE;
		BOOLEAN releaseWhenResumed = FALSE;
		BOOLEAN turnBasedCommandActive = FALSE;
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
			soldier->bDirection : static_cast<UINT8>(NORTH);
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
			(gAnimControl[soldier->usAnimState].uiFlags &
				(ANIM_SPECIALMOVE | ANIM_FIRE)) ||
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

	BOOLEAN TurnBasedControlHasForeignOwner(SOLDIERTYPE const* soldier,
		DirectControlTiming const& timing)
	{
		const BOOLEAN ownsTurnLock = timing.turnBasedCommandActive &&
			gCurrentUIMode == LOCKOURTURN_UI_MODE;
		const BOOLEAN ownsMovementTransition = timing.turnBasedCommandActive &&
			(IsMoving(soldier) || DirectControlTemporarilyPaused(soldier) ||
			 soldier->fTurningUntilDone ||
			 soldier->usPendingAnimation != NO_PENDING_ANIMATION ||
			 soldier->ubPendingStanceChange != NO_PENDING_STANCE);
		const BOOLEAN ownsReleaseProjection = timing.turnBasedCommandActive &&
			!ownsMovementTransition &&
			guiPendingOverrideEvent == M_ON_TERRAIN;
		return gTacticalStatus.ubAttackBusyCount > 0 ||
			gfDisableRegionActive ||
			(gfUserTurnRegionActive && !ownsTurnLock) ||
			(guiPendingOverrideEvent != I_DO_NOTHING &&
			 !ownsReleaseProjection) ||
			gCurrentUIMode == LOCKUI_MODE ||
			(gCurrentUIMode == LOCKOURTURN_UI_MODE && !ownsTurnLock) ||
			gCurrentUIMode == ENEMYS_TURN_MODE ||
			(EngineOwnsMovement(soldier) && !ownsMovementTransition);
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

	BOOLEAN GetDirectControlMouseWorldCoords(INT16& worldX, INT16& worldY)
	{
		// GetMouseWorldCoords deliberately refuses coordinates whenever another
		// high-priority mouse region owns the pointer. OS//0 projects lightweight
		// object icons over the world, so that vanilla ownership check made facing
		// stop exactly when looking at an interactive object. Direct control has
		// already rejected real modal windows; project the visible pointer directly.
		auto cursor = GetCursorPos();
		if (cursor.iX < gsVIEWPORT_START_X || cursor.iX >= gsVIEWPORT_END_X ||
			cursor.iY < gsVIEWPORT_WINDOW_START_Y ||
			cursor.iY >= OS0WorldViewportBottom()) return FALSE;
		OS0MapDisplayToWorldScreen(&cursor.iX, &cursor.iY);
		const INT16 offsetX = static_cast<INT16>(
			cursor.iX - g_ui.m_tacticalMapCenterX);
		const INT16 offsetY = static_cast<INT16>(
			cursor.iY - g_ui.m_tacticalMapCenterY + 10);
		INT16 cellX;
		INT16 cellY;
		FromScreenToCellCoordinates(offsetX, offsetY, &cellX, &cellY);
		const INT32 projectedX = static_cast<INT32>(gsRenderCenterX) + cellX;
		const INT32 projectedY = static_cast<INT32>(gsRenderCenterY) + cellY;
		if (projectedX < 0 || projectedX >= WORLD_COORD_COLS ||
			projectedY < 0 || projectedY >= WORLD_COORD_ROWS) return FALSE;
		worldX = static_cast<INT16>(projectedX);
		worldY = static_cast<INT16>(projectedY);
		return TRUE;
	}

	void FollowMouse(SOLDIERTYPE* soldier, DirectControlTiming& timing,
		BOOLEAN manualTurn, BOOLEAN applyFacing)
	{
		EnsureLookDirection(soldier, timing);
		const UINT32 now = GetJA2Clock();
		if (manualTurn || now < timing.manualFacingUntil ||
			now < timing.nextMouseFacingAt)
			return;
		INT16 worldX;
		INT16 worldY;
		if (!GetDirectControlMouseWorldCoords(worldX, worldY)) return;
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
		const UINT16 base = sprint ? static_cast<UINT16>(RUNNING) :
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

BOOLEAN OS0HandleTurnBasedDirectControlKey(SOLDIERTYPE* soldier, UINT32 key,
	UINT16 eventType, BOOLEAN enabled)
{
	if (!(gTacticalStatus.uiFlags & INCOMBAT)) return FALSE;
	if (eventType != KEY_DOWN && eventType != KEY_REPEAT) return FALSE;
	if (!soldier) return FALSE;
	DirectControlTiming& timing = TimingFor(soldier);
	const BOOLEAN actionable = key == SDLK_w || key == SDLK_a ||
		key == SDLK_s || key == SDLK_d || key == SDLK_q || key == SDLK_e ||
		key == 'W' || key == 'A' || key == 'S' || key == 'D' ||
		key == 'Q' || key == 'E';
	if (!actionable) return FALSE;
	if (!enabled || !soldier->bActive ||
		soldier->bTeam != OUR_TEAM || soldier->bLife < OKLIFE ||
		!OK_CONTROLLABLE_MERC(soldier) ||
		gTacticalStatus.ubCurrentTeam != OUR_TEAM)
		return FALSE;
	if (TurnBasedControlHasForeignOwner(soldier, timing))
	{
		// Buffered movement belongs only to the currently completing movement or
		// turn. A shot, interrupt, modal lock or queued native event cancels it so
		// an old tap cannot execute unexpectedly after that owner finishes.
		timing.pendingTurnBasedKey = 0;
		return FALSE;
	}
	if (timing.turnBasedCommandActive &&
		(gCurrentUIMode == LOCKOURTURN_UI_MODE ||
		 gfUserTurnRegionActive ||
		 guiPendingOverrideEvent == M_ON_TERRAIN))
	{
		// SetUIBusy owns this lock until the one-tile command settles. Keep only
		// the newest deliberate tap; never dispatch a second path underneath it.
		if (eventType == KEY_DOWN) timing.pendingTurnBasedKey = key;
		return FALSE;
	}

	if (IsMoving(soldier) || DirectControlTemporarilyPaused(soldier) ||
		soldier->fTurningUntilDone ||
		soldier->usPendingAnimation != NO_PENDING_ANIMATION ||
		soldier->ubPendingStanceChange != NO_PENDING_STANCE)
	{
		// One-tile TB movement can outlast a quick tap. Preserve the most recent
		// fresh direction and replay it once the native animation reaches its tile;
		// repeats remain disposable so a released held key cannot run indefinitely.
		if (eventType == KEY_DOWN) timing.pendingTurnBasedKey = key;
		return FALSE;
	}
	timing.pendingTurnBasedKey = 0;
	EnsureLookDirection(soldier, timing);
	const UINT32 now = GetJA2Clock();
	// A fresh tap always supersedes the repeat throttle. Swallowing KEY_DOWN here
	// made quick direction changes appear to hang even though the key was owned.
	if (eventType == KEY_REPEAT && now < timing.nextStepAt) return FALSE;

	const BOOLEAN turnLeft = key == SDLK_q || key == 'Q';
	const BOOLEAN turnRight = key == SDLK_e || key == 'E';
	if (turnLeft || turnRight)
	{
		// Each key event is exactly one physical facing step. A look intent can
		// survive the RT->TB transition and may point somewhere entirely different;
		// using it here made the first Q/E press rotate several directions at once.
		const UINT8 facing = ActualFacing(soldier);
		const UINT8 direction = turnLeft ? OneCCDirection(facing) :
			OneCDirection(facing);
		// The virtual mouse-facing intent can be one step away from the physical
		// animation. A Q/E step that lands on the current direction is a consumed
		// no-op; setting UI busy here would wait forever for a turn that never starts.
		if (direction == facing)
		{
			timing.lookDirection = direction;
			timing.lookDirectionValid = TRUE;
			timing.nextStepAt = now + 70;
			return TRUE;
		}
		const INT16 apCost = GetAPsToLook(soldier);
		if (!EnoughPoints(soldier, apCost, 0, TRUE)) return TRUE;
		timing.lookDirection = direction;
		timing.lookDirectionValid = TRUE;
		SendSoldierSetDesiredDirectionEvent(soldier, direction);
		soldier->bTurningFromUI = TRUE;
		timing.turnBasedCommandActive = TRUE;
		SetUIBusy(soldier);
		timing.nextStepAt = now + 135;
		return TRUE;
	}
	// Movement is relative to the current zoom-aware mouse-facing intent. Q/E is
	// deliberately handled above so a held manual turn cannot be reset by the
	// pointer on every key repeat.
	FollowMouse(soldier, timing, FALSE, FALSE);

	// Resolve the complete held-key chord, not merely whichever repeat arrived
	// this frame. W+D therefore produces a stable diagonal instead of alternating
	// forward/right steps.
	const BOOLEAN forward = _KeyDown(SDLK_w) || key == SDLK_w || key == 'W';
	const BOOLEAN backward = _KeyDown(SDLK_s) || key == SDLK_s || key == 'S';
	const BOOLEAN left = _KeyDown(SDLK_a) || key == SDLK_a || key == 'A';
	const BOOLEAN right = _KeyDown(SDLK_d) || key == SDLK_d || key == 'D';
	if (!forward && !backward && !left && !right) return FALSE;
	const TravelIntent intent = ResolveTravelDirection(timing.lookDirection,
		forward, backward, left, right);
	if (!intent.active) return FALSE;
	const GridNo destination = NewGridNo(soldier->sGridNo,
		DirectionInc(intent.direction));
	if (destination == soldier->sGridNo ||
		!NewOKDestination(soldier, destination, TRUE, soldier->bLevel))
	{
		timing.nextStepAt = now + 100;
		return TRUE;
	}

	const UINT8 stance = gAnimControl[soldier->usAnimState].ubEndHeight;
	const BOOLEAN sprint =
		(_KeyDown(SDLK_LSHIFT) || _KeyDown(SDLK_RSHIFT)) &&
		stance == ANIM_STAND && !intent.reverse && !MercInWater(soldier);
	const UINT16 previousMode = soldier->usUIMovementMode;
	const BOOLEAN previousFast = soldier->fUIMovementFast;
	const INT8 previousReverse = soldier->bReverse;
	const UINT16 movementMode = MovementModeForIntent(soldier, intent, stance,
		FALSE, sprint);
	soldier->usUIMovementMode = movementMode;
	soldier->fUIMovementFast = sprint;
	soldier->bReverse = intent.reverse;
	const INT16 apCost = PlotPath(soldier, destination, NO_COPYROUTE, FALSE,
		movementMode, soldier->bActionPoints);
	if (apCost <= 0 || !EnoughPoints(soldier, apCost, 0, TRUE))
	{
		soldier->usUIMovementMode = previousMode;
		soldier->fUIMovementFast = previousFast;
		soldier->bReverse = previousReverse;
		timing.nextStepAt = now + 100;
		return TRUE;
	}

	const BOOLEAN accepted = EVENT_InternalGetNewSoldierPath(soldier,
		destination, movementMode, TRUE, FALSE);
	soldier->fUIMovementFast = previousFast;
	if (!accepted)
	{
		soldier->usUIMovementMode = previousMode;
		soldier->bReverse = previousReverse;
		timing.nextStepAt = now + 100;
		return TRUE;
	}
	timing.turnBasedCommandActive = TRUE;
	SetUIBusy(soldier);
	timing.lastTravelDirection = intent.direction;
	timing.lastReverse = intent.reverse;
	timing.lastSprint = sprint;
	timing.nextStepAt = now + 70;
	return TRUE;
}

void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled)
{
	if (!soldier) return;
	DirectControlTiming& timing = TimingFor(soldier);
	if (!soldier->bActive)
	{
		timing.moveInputActive = FALSE;
		timing.lookDirectionValid = FALSE;
		timing.releaseWhenResumed = FALSE;
		timing.pendingTurnBasedKey = 0;
		timing.turnBasedCommandActive = FALSE;
		return;
	}
	ClearReleasedMovementFlags(soldier, timing);
	if (gTacticalStatus.uiFlags & INCOMBAT)
	{
		ReleaseMovementIntent(soldier, timing);
		if (!enabled || soldier->bTeam != OUR_TEAM || soldier->bLife < OKLIFE ||
			!OK_CONTROLLABLE_MERC(soldier) ||
			gTacticalStatus.ubCurrentTeam != OUR_TEAM)
		{
			timing.pendingTurnBasedKey = 0;
			timing.turnBasedCommandActive = FALSE;
			return;
		}
		const BOOLEAN ownCommandSettled = timing.turnBasedCommandActive &&
			!IsMoving(soldier) && !DirectControlTemporarilyPaused(soldier) &&
			!soldier->fTurningUntilDone &&
			soldier->usPendingAnimation == NO_PENDING_ANIMATION &&
			soldier->ubPendingStanceChange == NO_PENDING_STANCE &&
			gCurrentUIMode != LOCKOURTURN_UI_MODE &&
			!gfUserTurnRegionActive &&
			guiPendingOverrideEvent == I_DO_NOTHING;
		if (ownCommandSettled) timing.turnBasedCommandActive = FALSE;
		if (TurnBasedControlHasForeignOwner(soldier, timing))
		{
			timing.pendingTurnBasedKey = 0;
			return;
		}
		if (timing.pendingTurnBasedKey != 0 && !IsMoving(soldier) &&
			!DirectControlTemporarilyPaused(soldier) &&
			!soldier->fTurningUntilDone &&
			soldier->usPendingAnimation == NO_PENDING_ANIMATION &&
			soldier->ubPendingStanceChange == NO_PENDING_STANCE &&
			gCurrentUIMode != LOCKOURTURN_UI_MODE &&
			!gfUserTurnRegionActive &&
			guiPendingOverrideEvent == I_DO_NOTHING)
		{
			OS0HandleTurnBasedDirectControlKey(soldier,
				timing.pendingTurnBasedKey, KEY_DOWN, TRUE);
		}
		return;
	}
	if (!enabled || soldier->bTeam != OUR_TEAM || soldier->bLife < OKLIFE ||
		!OK_CONTROLLABLE_MERC(soldier) || gfDisableRegionActive ||
		gfUserTurnRegionActive || gTacticalStatus.ubAttackBusyCount > 0 ||
		guiPendingOverrideEvent != I_DO_NOTHING ||
		gCurrentUIMode == LOCKUI_MODE ||
		gCurrentUIMode == LOCKOURTURN_UI_MODE ||
		gCurrentUIMode == ENEMYS_TURN_MODE)
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
	// Q/E is an explicit turn only while the key is physically held. As soon as
	// the player presses forward without Q/E, the pointer becomes authoritative
	// again; a previous manual turn must not make W keep walking in an old heading
	// for the remainder of manualFacingUntil.
	if (forward && !manualTurn) timing.manualFacingUntil = 0;
	// In OS//0 the pointer is the operator's look intent in every live-control
	// state. Movement animation still owns the sprite while traversing a tile;
	// FollowMouse retains that virtual direction and applies it as soon as the
	// operator is stationary. This makes normal exploration visibly responsive
	// without fighting JA2's interpolation.
	FollowMouse(soldier, timing, manualTurn, !moving);

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
	std::array<UINT8, MAX_PATH_LIST_SIZE> previousPath;
	const size_t previousPathCount = std::min<size_t>(previousPathSize,
		previousPath.size());
	// Only the live prefix can be observed when a rejected replan is rolled
	// back. Copying all MAX_PATH_LIST_SIZE bytes on every 45 ms movement tick
	// added avoidable work to the direct-control hot path.
	std::copy_n(soldier->ubPathingData, previousPathCount,
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
		std::copy_n(previousPath.begin(), previousPathCount,
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
