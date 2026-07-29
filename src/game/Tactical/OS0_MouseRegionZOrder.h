#pragma once

#include "JA2Types.h"
#include "MouseSystem.h"
#include "OS0_WindowManager.h"

#include <cstddef>
#include <span>

// One managed window may contribute multiple groups.  Groups and their
// regions must be supplied back-to-front (backdrop, content, header, close,
// etc.).  The adapter then applies the window manager's z-order above that
// local ordering.
struct OS0ManagedMouseRegionGroup
{
	OS0WindowHandle window = OS0_INVALID_WINDOW;
	std::span<MOUSE_REGION> regionsBackToFront{};
	// Used by renderers whose region metadata wraps MOUSE_REGION rather than
	// storing a contiguous native array (for example the realtime editor).
	std::span<MOUSE_REGION* const> indirectRegionsBackToFront{};
};

// Keep native JA2 modal regions at HIGHEST above the OS0 desktop.
constexpr INT8 OS0_MANAGED_MOUSE_PRIORITY = MSYS_PRIORITY_HIGHEST - 1;

// JA2 resolves overlapping equal-priority mouse regions by insertion order.
// This adapter projects the canonical OS0WindowManager order onto that native
// list.  All managed regions deliberately share one priority, otherwise a
// child of a rear window could remain above the backdrop of a front window.
//
// Disabled regions are reordered too: their next Enable() then has the right
// order without rebuilding them. Removed/null regions are ignored safely.
// Returns the number of live regions that were reordered, or zero when the
// manager projection already matches the native list.
size_t OS0ApplyManagedMouseRegionZOrder(
	OS0WindowManager const& manager,
	std::span<OS0ManagedMouseRegionGroup const> groups,
	INT8 managedPriority = OS0_MANAGED_MOUSE_PRIORITY) noexcept;
