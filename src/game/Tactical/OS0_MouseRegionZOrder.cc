#include "OS0_MouseRegionZOrder.h"

#include "MouseSystem.h"

#include <algorithm>
#include <functional>
#include <vector>

size_t OS0ApplyManagedMouseRegionZOrder(
	OS0WindowManager const& manager,
	std::span<OS0ManagedMouseRegionGroup const> groups,
	INT8 const managedPriority) noexcept
{
	// This adapter runs from the single-threaded tactical UI every frame. Reuse
	// the backing storage so a stable window order does not allocate just to take
	// the no-op path. The sorted copy turns managed-region membership checks from
	// a linear scan into a binary search while walking JA2's linked region list.
	static thread_local std::vector<MOUSE_REGION*> expected;
	static thread_local std::vector<MOUSE_REGION*> membership;
	expected.clear();
	for (OS0WindowHandle const window : manager.renderOrder())
	{
		for (OS0ManagedMouseRegionGroup const& group : groups)
		{
			if (group.window != window) continue;
			for (MOUSE_REGION& region : group.regionsBackToFront)
			{
				if (region.uiFlags & MSYS_REGION_EXISTS) expected.push_back(&region);
			}
			for (MOUSE_REGION* const region : group.indirectRegionsBackToFront)
			{
				if (region != nullptr && (region->uiFlags & MSYS_REGION_EXISTS))
					expected.push_back(region);
			}
		}
	}
	membership.assign(expected.begin(), expected.end());
	std::sort(membership.begin(), membership.end(), std::less<>{});

	// SetBagRegionsEnabled runs every frame. Avoid unlinking/reinserting every
	// child when the manager order has not changed; that used to force a global
	// mouse-region refresh and made window dragging needlessly expensive.
	auto nextManaged = [](MOUSE_REGION* region)
	{
		for (MOUSE_REGION* next = region->next; next != nullptr; next = next->next)
		{
			if (std::binary_search(membership.begin(), membership.end(), next,
				std::less<>{}))
				return next;
		}
		return static_cast<MOUSE_REGION*>(nullptr);
	};
	BOOLEAN alreadyOrdered = TRUE;
	for (size_t i = 0; i < expected.size(); ++i)
	{
		MOUSE_REGION* const expectedNext = i == 0 ? nullptr : expected[i - 1];
		if (expected[i]->PriorityLevel != managedPriority ||
			nextManaged(expected[i]) != expectedNext)
		{
			alreadyOrdered = FALSE;
			break;
		}
	}
	if (alreadyOrdered) return 0;

	size_t reordered = 0;
	// renderOrder() is back-to-front.  Reinsert preserves that relation because
	// JA2 puts the latest equal-priority region at the head of its hit-test list.
	for (MOUSE_REGION* const region : expected)
	{
		if (MSYS_SetRegionPriority(region, managedPriority)) ++reordered;
	}
	return reordered;
}
