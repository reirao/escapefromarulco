#pragma once

#include "JA2Types.h"

struct SOLDIERTYPE;

enum class OS0DirectControlKey : UINT8
{
	NONE,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	TURN_LEFT,
	TURN_RIGHT,
	SPRINT
};

struct OS0DirectControlInput
{
	BOOLEAN forward = FALSE;
	BOOLEAN backward = FALSE;
	BOOLEAN left = FALSE;
	BOOLEAN right = FALSE;
	BOOLEAN turnLeft = FALSE;
	BOOLEAN turnRight = FALSE;
	BOOLEAN sprint = FALSE;

	constexpr BOOLEAN hasMovement() const noexcept
	{
		return forward || backward || left || right;
	}

	constexpr BOOLEAN hasManualTurn() const noexcept
	{
		return turnLeft != turnRight;
	}
};

struct OS0DirectTravelIntent
{
	UINT8 direction = 0xff;
	BOOLEAN reverse = FALSE;
	BOOLEAN active = FALSE;
};

OS0DirectControlKey OS0ClassifyDirectControlKey(UINT32 key) noexcept;
BOOLEAN OS0IsDirectControlKey(UINT32 key);
BOOLEAN OS0DirectControlOwnsSprintModifier();
// A modal/UI owner consumed this physical key. Polling must ignore it until the
// corresponding release, otherwise dismissing UI also starts movement.
void OS0SuppressDirectControlKeyUntilRelease(UINT32 key) noexcept;
OS0DirectTravelIntent OS0ResolveDirectTravelIntent(UINT8 facing,
	OS0DirectControlInput const& input) noexcept;
BOOLEAN OS0HandleTurnBasedDirectControlKey(SOLDIERTYPE* soldier, UINT32 key,
	UINT16 eventType, BOOLEAN enabled);
void OS0UpdateDirectControl(SOLDIERTYPE* soldier, BOOLEAN enabled);
// Re-applies pointer-facing after ScrollWorld committed the camera used for the
// displayed frame. Movement remains owned by OS0UpdateDirectControl.
void OS0RefreshDirectControlFacing(SOLDIERTYPE* soldier, INT16 worldX,
	INT16 worldY, BOOLEAN cameraChanged);
void OS0ResetDirectControl();
