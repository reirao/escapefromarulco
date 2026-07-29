#include "gtest/gtest.h"

#include "OS0_InteractionMode.h"

TEST(OS0InteractionModeTest, StartsNormalWithActionsSelected)
{
	OS0InteractionMode mode;

	EXPECT_TRUE(mode.isNormal());
	EXPECT_FALSE(mode.hasActiveSurface());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::ACTIONS));
	EXPECT_FALSE(mode.isSurfaceActive(OS0InteractionSurface::ACTIONS));
	EXPECT_FALSE(mode.nearbyScanEnabled());
	EXPECT_TRUE(mode.canScanNearby());
	EXPECT_STREQ(OS0InteractionStateName(mode.state()), "NORMAL");
	EXPECT_STREQ(OS0InteractionSurfaceName(mode.surface()), "ACTIONS");
}

TEST(OS0InteractionModeTest, ExplicitTransitionsControlTheActiveSurface)
{
	OS0InteractionMode mode;

	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::ENVIRONMENT));
	EXPECT_TRUE(mode.isInteracting());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ENVIRONMENT));

	ASSERT_TRUE(mode.selectSurface(OS0InteractionSurface::EQUIPMENT));
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));
	EXPECT_TRUE(mode.transitionTo(OS0InteractionState::FIGHT));
	EXPECT_TRUE(mode.isFight());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));

	mode.returnToNormal();
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::EQUIPMENT));
	EXPECT_FALSE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));
}

TEST(OS0InteractionModeTest, NearbyScanIsExplicitAndFightAlwaysDisablesIt)
{
	OS0InteractionMode mode;

	EXPECT_TRUE(mode.toggleNearbyScan());
	EXPECT_TRUE(mode.nearbyScanEnabled());
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::ACTIONS));
	EXPECT_TRUE(mode.nearbyScanEnabled());

	ASSERT_TRUE(mode.beginFight(OS0InteractionSurface::BEHAVIOR));
	EXPECT_FALSE(mode.nearbyScanEnabled());
	EXPECT_FALSE(mode.canScanNearby());
	EXPECT_FALSE(mode.setNearbyScanEnabled(true));
	EXPECT_FALSE(mode.nearbyScanEnabled());

	mode.returnToNormal();
	EXPECT_TRUE(mode.setNearbyScanEnabled(true));
	EXPECT_TRUE(mode.nearbyScanEnabled());
	EXPECT_TRUE(mode.toggleNearbyScan());
	EXPECT_FALSE(mode.nearbyScanEnabled());
}

TEST(OS0InteractionModeTest, InvalidValuesAreRejectedWithoutMutation)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::BEHAVIOR));
	ASSERT_TRUE(mode.setNearbyScanEnabled(true));

	auto const invalidState = static_cast<OS0InteractionState>(255);
	auto const invalidSurface = static_cast<OS0InteractionSurface>(255);
	EXPECT_FALSE(mode.transitionTo(invalidState));
	EXPECT_FALSE(mode.selectSurface(invalidSurface));
	EXPECT_FALSE(mode.beginInteraction(invalidSurface));
	EXPECT_FALSE(mode.beginFight(invalidSurface));

	EXPECT_TRUE(mode.isInteracting());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::BEHAVIOR));
	EXPECT_TRUE(mode.nearbyScanEnabled());

	mode.reset();
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::ACTIONS));
	EXPECT_FALSE(mode.nearbyScanEnabled());
}

TEST(OS0InteractionModeTest, FrameReducerPreservesContextAndRestoresVisibleOwner)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::BEHAVIOR));

	OS0InteractionFrameFacts facts;
	facts.context = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::BEHAVIOR));

	facts = {};
	facts.fight = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isFight());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ACTIONS));

	facts = {};
	facts.equipment = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isInteracting());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));
}

TEST(OS0InteractionModeTest, FrameReducerKeepsExplicitCursorAndEnvironmentModes)
{
	OS0InteractionMode mode;
	OS0InteractionFrameFacts facts;
	facts.cursorAction = true;
	facts.cursorSurface = OS0InteractionSurface::ENVIRONMENT;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ENVIRONMENT));

	facts = {};
	facts.environment = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ENVIRONMENT));

	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::EQUIPMENT));
	facts.equipment = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));

	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::ACTIONS));
	facts.cursorAction = true;
	facts.cursorSurface = OS0InteractionSurface::ACTIONS;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ACTIONS));

	facts = {};
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isNormal());
}

TEST(OS0InteractionModeTest, FightRestoresLastPersistentOwnerWhenWindowsCoexist)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::EQUIPMENT));

	OS0InteractionFrameFacts facts;
	facts.environment = true;
	facts.equipment = true;
	facts.fight = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isFight());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::ACTIONS));

	facts.fight = false;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isInteracting());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::EQUIPMENT));
}

TEST(OS0InteractionModeTest, ExplicitCancelRestoresSurfaceSelectedBeforeFight)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::EQUIPMENT));
	ASSERT_TRUE(mode.beginFight(OS0InteractionSurface::ACTIONS));

	// This mirrors the production cancel path: the generic action is selected
	// before returning to NORMAL. Leaving FIGHT must still restore its owner.
	ASSERT_TRUE(mode.selectSurface(OS0InteractionSurface::ACTIONS));
	mode.returnToNormal();
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::EQUIPMENT));
}

TEST(OS0InteractionModeTest, ExplicitInteractionOwnsSurfaceWhenLeavingFight)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::EQUIPMENT));
	ASSERT_TRUE(mode.beginFight(OS0InteractionSurface::ACTIONS));

	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::BEHAVIOR));
	EXPECT_TRUE(mode.isInteracting());
	EXPECT_TRUE(mode.isSurfaceActive(OS0InteractionSurface::BEHAVIOR));

	// A later fight starts from the explicit owner, never a stale backup.
	ASSERT_TRUE(mode.beginFight(OS0InteractionSurface::ACTIONS));
	mode.returnToNormal();
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::BEHAVIOR));
}

TEST(OS0InteractionModeTest, ScanDoesNotCreateAnInteractionWithoutFacts)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.setNearbyScanEnabled(true));

	mode.synchronize({});
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.nearbyScanEnabled());
}

TEST(OS0InteractionModeTest, TutorialResetClearsScanAndFightHistory)
{
	OS0InteractionMode mode;
	ASSERT_TRUE(mode.beginInteraction(OS0InteractionSurface::EQUIPMENT));
	ASSERT_TRUE(mode.setNearbyScanEnabled(true));
	ASSERT_TRUE(mode.beginFight(OS0InteractionSurface::ACTIONS));

	OS0InteractionFrameFacts facts;
	facts.tutorial = true;
	mode.synchronize(facts);
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::ACTIONS));
	EXPECT_FALSE(mode.nearbyScanEnabled());

	mode.synchronize({});
	EXPECT_TRUE(mode.isNormal());
	EXPECT_TRUE(mode.isSurfaceSelected(OS0InteractionSurface::ACTIONS));
}
