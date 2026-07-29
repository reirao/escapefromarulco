#include "gtest/gtest.h"

#include "MouseSystem.h"
#include "OS0_MouseRegionZOrder.h"
#include "OS0_WindowManager.h"

#include <array>

namespace
{
	void DefineTestRegion(MOUSE_REGION& region, INT8 const priority)
	{
		MSYS_DefineRegion(&region, 0, 0, 20, 20, priority, MSYS_NO_CURSOR,
			MSYS_NO_CALLBACK, MSYS_NO_CALLBACK);
	}

	void RemoveTestRegion(MOUSE_REGION& region)
	{
		if (region.uiFlags & MSYS_REGION_EXISTS) MSYS_RemoveRegion(&region);
	}
}

TEST(MouseSystemRegionOrderTest, ReinsertChangesOnlyEqualPriorityTieBreak)
{
	MOUSE_REGION first{};
	MOUSE_REGION second{};
	DefineTestRegion(first, MSYS_PRIORITY_HIGH);
	DefineTestRegion(second, MSYS_PRIORITY_HIGH);

	EXPECT_EQ(second.next, &first);
	EXPECT_TRUE(MSYS_ReinsertRegion(&first));
	EXPECT_EQ(first.next, &second);
	EXPECT_EQ(first.PriorityLevel, MSYS_PRIORITY_HIGH);

	EXPECT_TRUE(MSYS_SetRegionPriority(&second,
		MSYS_PRIORITY_HIGHEST));
	EXPECT_EQ(second.PriorityLevel, MSYS_PRIORITY_HIGHEST);
	EXPECT_EQ(second.next, &first);
	EXPECT_FALSE(MSYS_ReinsertRegion(nullptr));

	RemoveTestRegion(second);
	RemoveTestRegion(first);
	EXPECT_FALSE(MSYS_ReinsertRegion(&first));
}

TEST(OS0MouseRegionZOrderTest, WindowOrderDominatesEveryChildRegion)
{
	OS0WindowManager manager;
	ASSERT_TRUE(manager.registerTemplate({ 1, "rear", "Rear",
		OS0UIIcon::HAND, OS0WindowPresentation::FLOATING,
		{ 0, 0, 100, 100 }, 20, 20, OS0_WINDOW_MOVABLE,
		TRUE, 1, 0 }));
	ASSERT_TRUE(manager.registerTemplate({ 2, "front", "Front",
		OS0UIIcon::LOOK, OS0WindowPresentation::FLOATING,
		{ 0, 0, 100, 100 }, 20, 20, OS0_WINDOW_MOVABLE,
		TRUE, 2, 1 }));

	std::array<MOUSE_REGION, 2> rear{};
	std::array<MOUSE_REGION, 2> front{};
	// Deliberately model the legacy split where children outrank backdrops.
	DefineTestRegion(rear[0], MSYS_PRIORITY_HIGH);
	DefineTestRegion(rear[1], MSYS_PRIORITY_HIGHEST);
	DefineTestRegion(front[0], MSYS_PRIORITY_HIGH);
	DefineTestRegion(front[1], MSYS_PRIORITY_HIGHEST);

	std::array<OS0ManagedMouseRegionGroup, 2> groups{{
		{ 1, rear }, { 2, front }
	}};

	EXPECT_EQ(OS0ApplyManagedMouseRegionZOrder(manager, groups), 4u);
	EXPECT_EQ(front[1].next, &front[0]);
	EXPECT_EQ(front[0].next, &rear[1]);
	EXPECT_EQ(rear[1].next, &rear[0]);
	EXPECT_EQ(rear[0].PriorityLevel, OS0_MANAGED_MOUSE_PRIORITY);
	EXPECT_EQ(OS0ApplyManagedMouseRegionZOrder(manager, groups), 0u);

	manager.bringToFront(1);
	EXPECT_EQ(OS0ApplyManagedMouseRegionZOrder(manager, groups), 4u);
	EXPECT_EQ(rear[1].next, &rear[0]);
	EXPECT_EQ(rear[0].next, &front[1]);
	EXPECT_EQ(front[1].next, &front[0]);

	RemoveTestRegion(rear[1]);
	RemoveTestRegion(rear[0]);
	RemoveTestRegion(front[1]);
	RemoveTestRegion(front[0]);
}
