#include "gtest/gtest.h"

#include "OS0_DirectControl.h"

#include "Overhead_Types.h"

#include <SDL_keycode.h>

TEST(OS0DirectControlPolicyTest, OwnsOnlyTheUnmodifiedControlKeyFamily)
{
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_w),
		OS0DirectControlKey::FORWARD);
	EXPECT_EQ(OS0ClassifyDirectControlKey('W'),
		OS0DirectControlKey::FORWARD);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_s),
		OS0DirectControlKey::BACKWARD);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_a),
		OS0DirectControlKey::LEFT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_d),
		OS0DirectControlKey::RIGHT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_q),
		OS0DirectControlKey::TURN_LEFT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_e),
		OS0DirectControlKey::TURN_RIGHT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_LSHIFT),
		OS0DirectControlKey::SPRINT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_RSHIFT),
		OS0DirectControlKey::SPRINT);
	EXPECT_EQ(OS0ClassifyDirectControlKey(SDLK_f),
		OS0DirectControlKey::NONE);
	EXPECT_TRUE(OS0IsDirectControlKey(SDLK_w));
	EXPECT_FALSE(OS0IsDirectControlKey(SDLK_f));
}

TEST(OS0DirectControlPolicyTest, ResolvesMouseRelativeMovementAsOneStableChord)
{
	OS0DirectControlInput input;
	input.forward = TRUE;
	OS0DirectTravelIntent intent =
		OS0ResolveDirectTravelIntent(NORTH, input);
	ASSERT_TRUE(intent.active);
	EXPECT_EQ(intent.direction, NORTH);
	EXPECT_FALSE(intent.reverse);

	input.right = TRUE;
	intent = OS0ResolveDirectTravelIntent(NORTH, input);
	ASSERT_TRUE(intent.active);
	EXPECT_EQ(intent.direction, NORTHEAST);
	EXPECT_FALSE(intent.reverse);

	input = {};
	input.backward = TRUE;
	input.left = TRUE;
	intent = OS0ResolveDirectTravelIntent(NORTH, input);
	ASSERT_TRUE(intent.active);
	EXPECT_EQ(intent.direction, SOUTHWEST);
	EXPECT_TRUE(intent.reverse);

	input = {};
	input.left = TRUE;
	intent = OS0ResolveDirectTravelIntent(NORTH, input);
	ASSERT_TRUE(intent.active);
	EXPECT_EQ(intent.direction, WEST);
	EXPECT_TRUE(intent.reverse);
}

TEST(OS0DirectControlPolicyTest, OpposingKeysCancelPerAxisWithoutZigzag)
{
	OS0DirectControlInput input;
	input.forward = TRUE;
	input.backward = TRUE;
	OS0DirectTravelIntent intent =
		OS0ResolveDirectTravelIntent(NORTH, input);
	EXPECT_FALSE(intent.active);

	input.right = TRUE;
	intent = OS0ResolveDirectTravelIntent(NORTH, input);
	ASSERT_TRUE(intent.active);
	EXPECT_EQ(intent.direction, EAST);
	EXPECT_TRUE(intent.reverse);

	input = {};
	input.left = TRUE;
	input.right = TRUE;
	EXPECT_FALSE(OS0ResolveDirectTravelIntent(NORTH, input).active);
	EXPECT_FALSE(OS0ResolveDirectTravelIntent(0xff, input).active);
}

TEST(OS0DirectControlPolicyTest, ShiftAndManualTurnAreStateNotTravelAxes)
{
	OS0DirectControlInput input;
	input.forward = TRUE;
	input.sprint = TRUE;
	OS0DirectTravelIntent const sprint =
		OS0ResolveDirectTravelIntent(EAST, input);
	ASSERT_TRUE(sprint.active);
	EXPECT_EQ(sprint.direction, EAST);
	EXPECT_FALSE(sprint.reverse);

	input.turnLeft = TRUE;
	EXPECT_TRUE(input.hasManualTurn());
	input.turnRight = TRUE;
	EXPECT_FALSE(input.hasManualTurn());
	EXPECT_TRUE(input.hasMovement());
}
