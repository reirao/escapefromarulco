#include "OS0_UIRuntime.h"

#include <algorithm>
#include <cstring>

namespace
{
	constexpr std::array<OS0UICommandDescriptor,
		static_cast<size_t>(OS0UICommand::COUNT)> COMMANDS{{
		{ OS0UICommand::TACTICAL, "ACTIONS",
			"ACTIONS / NORMAL / RETURN HELD ITEM", OS0UIIcon::TARGET,
			OS0UICommandIntent::RETURN_TO_ACTIONS },
		{ OS0UICommand::CHARACTER, "EQUIPMENT",
			"EQUIPMENT / RPG INVENTORY", OS0UIIcon::HAND,
			OS0UICommandIntent::TOGGLE_EQUIPMENT },
		{ OS0UICommand::STEALTH, "BEHAVIOR",
			"BEHAVIOR / STEALTH / STANCE", OS0UIIcon::SNEAK,
			OS0UICommandIntent::OPEN_BEHAVIOR },
		{ OS0UICommand::OBJECT, "SCAN",
			"NEARBY OBJECT SCAN / INSPECTOR", OS0UIIcon::LOOK,
			OS0UICommandIntent::TOGGLE_NEARBY_SCAN },
		{ OS0UICommand::WORLD, "ENVIRONMENT",
			"ENVIRONMENT / WORLD ABILITIES", OS0UIIcon::TOOLKIT,
			OS0UICommandIntent::TOGGLE_ENVIRONMENT },
		{ OS0UICommand::ASSETS, "ASSETS",
			"ASSET DATABASE", OS0UIIcon::KEYRING,
			OS0UICommandIntent::OPEN_ASSET_LIBRARY },
		{ OS0UICommand::TERRAIN, "TERRAIN",
			"LIVE WORLD EDITOR", OS0UIIcon::WIRE_CUTTER,
			OS0UICommandIntent::TOGGLE_REALTIME_EDITOR },
		{ OS0UICommand::STRATEGY, "STRATEGY",
			"STRATEGY / MAP / TEAM / REPORT", OS0UIIcon::WALK,
			OS0UICommandIntent::TOGGLE_STRATEGY },
		{ OS0UICommand::SANDBOX, "SANDBOX",
			"SANDBOX / GAME ASSET LIBRARY", OS0UIIcon::EXPLOSIVE,
			OS0UICommandIntent::OPEN_ICON_LIBRARY }
	}};

	constexpr std::array<OS0UIWindowDescriptor,
		static_cast<size_t>(OS0UIWindow::COUNT)> WINDOWS{{
		{ OS0UIWindow::SECTOR, "sector", "LIVE STRATEGY",
			OS0UIIcon::WALK, FALSE },
		{ OS0UIWindow::INSPECTOR, "inspector", "OBJECT / INSPECTOR",
			OS0UIIcon::LOOK, TRUE },
		{ OS0UIWindow::TOOLBOX, "toolbox", "OS//0 TOOLBOX",
			OS0UIIcon::KEYRING, FALSE },
		{ OS0UIWindow::ENVIRONMENT, "environment",
			"ENVIRONMENT / ABILITIES", OS0UIIcon::TOOLKIT, FALSE },
		{ OS0UIWindow::REALTIME_EDITOR, "realtime-editor",
			"WORLD EDITOR / LIVE", OS0UIIcon::WIRE_CUTTER, FALSE }
	}};

	constexpr UINT16 FLOATING_FEATURES = OS0_WINDOW_MOVABLE |
		OS0_WINDOW_CLOSABLE | OS0_WINDOW_BLOCKS_WORLD_INPUT |
		OS0_WINDOW_PERSIST_POSITION | OS0_WINDOW_COLLAPSE_DURING_AIM |
		OS0_WINDOW_DOCK_ENTRY;
	constexpr UINT16 PASS_THROUGH_FLOATING_FEATURES =
		FLOATING_FEATURES & ~OS0_WINDOW_BLOCKS_WORLD_INPUT;
	constexpr UINT16 RADIAL_FEATURES = OS0_WINDOW_BLOCKS_WORLD_INPUT |
		OS0_WINDOW_TRANSIENT | OS0_WINDOW_COLLAPSE_DURING_AIM;
	// Spatial item/equipment projections consist of individual sprite hit
	// regions, not an opaque rectangular panel. Their regions own the symbols;
	// the manager must not turn the empty space between them into a click shield.
	constexpr UINT16 WORLD_ATTACHED_FEATURES = OS0_WINDOW_TRANSIENT |
		OS0_WINDOW_COLLAPSE_DURING_AIM;

	constexpr OS0WindowHandle Handle(OS0ManagedWindow window)
	{
		return static_cast<OS0WindowHandle>(window);
	}

	constexpr std::array<OS0WindowTemplate,
		static_cast<size_t>(OS0ManagedWindow::COUNT)> WINDOW_TEMPLATES{{
		{ Handle(OS0ManagedWindow::INVENTORY), "character", "CHARACTER",
			OS0UIIcon::HAND, OS0WindowPresentation::FLOATING,
			{ 145, 12, 465, 184 }, 360, 160, FLOATING_FEATURES, FALSE, 30, 0 },
		{ Handle(OS0ManagedWindow::CONTEXT), "context", "CONTEXT",
			OS0UIIcon::TARGET, OS0WindowPresentation::RADIAL,
			{ 0, 0, 168, 200 }, 120, 80, RADIAL_FEATURES, FALSE, 80, -1 },
		{ Handle(OS0ManagedWindow::LOOT), "loot", "OBJECT CONTENTS",
			OS0UIIcon::OPEN, OS0WindowPresentation::WORLD_ATTACHED,
			{ 0, 0, 300, 170 }, 180, 100, WORLD_ATTACHED_FEATURES, FALSE, 45, -1 },
		{ Handle(OS0ManagedWindow::EQUIPMENT), "equipment", "EQUIPMENT",
			OS0UIIcon::HAND, OS0WindowPresentation::WORLD_ATTACHED,
			{ 0, 0, 240, 190 }, 160, 120, WORLD_ATTACHED_FEATURES, FALSE, 50, -1 },
		{ Handle(OS0ManagedWindow::STACK_SPLIT), "stack-split", "MOVE STACK",
			OS0UIIcon::HAND, OS0WindowPresentation::MODAL,
			{ 208, 180, 224, 82 }, 224, 82,
			OS0_WINDOW_BLOCKS_WORLD_INPUT, FALSE, 100, -1 },
		{ Handle(OS0ManagedWindow::ASSET_LIBRARY), "asset-library", "ASSET LIBRARY",
			OS0UIIcon::KEYRING, OS0WindowPresentation::FLOATING,
			{ 110, 76, 420, 250 }, 320, 210, FLOATING_FEATURES, FALSE, 35, 5 },
		{ Handle(OS0ManagedWindow::ASSET_CATALOG), "asset-catalog", "ASSET RECORD",
			OS0UIIcon::LOOK, OS0WindowPresentation::MODAL,
			{ 161, 110, 318, 194 }, 280, 170,
			OS0_WINDOW_BLOCKS_WORLD_INPUT, FALSE, 90, -1 },
		{ Handle(OS0ManagedWindow::ITEM_DETAILS), "item-details", "ITEM DETAILS",
			OS0UIIcon::EXAMINE, OS0WindowPresentation::FLOATING,
			{ 190, 100, 260, 160 }, 220, 120,
			PASS_THROUGH_FLOATING_FEATURES, FALSE, 55, 1 },
		{ Handle(OS0ManagedWindow::SECTOR), "sector", "LIVE STRATEGY",
			OS0UIIcon::WALK, OS0WindowPresentation::FLOATING,
			{ 332, 180, 300, 226 }, 260, 190, FLOATING_FEATURES, FALSE, 20, 6 },
		{ Handle(OS0ManagedWindow::INSPECTOR), "inspector", "OBJECT / INSPECTOR",
			OS0UIIcon::LOOK, OS0WindowPresentation::FLOATING,
			{ 8, 330, 310, 96 }, 240, 84,
			PASS_THROUGH_FLOATING_FEATURES, TRUE, 10, 3 },
		{ Handle(OS0ManagedWindow::TOOLBOX), "toolbox", "OS//0 TOOLBOX",
			OS0UIIcon::KEYRING, OS0WindowPresentation::FLOATING,
			{ 500, 280, 132, 132 }, 120, 116, FLOATING_FEATURES, FALSE, 25, 8 },
		{ Handle(OS0ManagedWindow::ENVIRONMENT), "environment",
			"ENVIRONMENT / ABILITIES", OS0UIIcon::TOOLKIT,
			OS0WindowPresentation::FLOATING, { 386, 160, 246, 104 }, 210, 92,
			FLOATING_FEATURES, FALSE, 22, 4 },
		{ Handle(OS0ManagedWindow::REALTIME_EDITOR), "realtime-editor",
			"WORLD EDITOR / LIVE", OS0UIIcon::WIRE_CUTTER,
			OS0WindowPresentation::FLOATING, { 62, 50, 516, 306 }, 420, 260,
			FLOATING_FEATURES, FALSE, 60, 7 }
	}};
}

void OS0UILayout::configure(INT16 screenWidth, INT16 screenHeight,
	INT16 engineWorldBottom) noexcept
{
	screenWidth_ = std::max<INT16>(1, screenWidth);
	screenHeight_ = std::max<INT16>(DOCK_HEIGHT, screenHeight);
	engineWorldBottom_ = std::clamp<INT16>(engineWorldBottom, 0, screenHeight_);
}

INT16 OS0UILayout::workspaceBottom() const noexcept
{
	return std::max<INT16>(0,
		std::min<INT16>(worldBottom(), screenHeight_ - DOCK_HEIGHT - 3));
}

INT16 OS0UILayout::worldBottom() const noexcept
{
	return std::min<INT16>(engineWorldBottom_, screenHeight_ - DOCK_HEIGHT);
}

OS0UIRect OS0UILayout::dock() const noexcept
{
	return { 0, static_cast<INT16>(screenHeight_ - DOCK_HEIGHT),
		screenWidth_, DOCK_HEIGHT };
}

OS0UIRect OS0UILayout::command(OS0UICommand command) const noexcept
{
	const size_t raw = static_cast<size_t>(command);
	if (raw >= COMMAND_COUNT) return {};
	const INT16 left = static_cast<INT16>(
		static_cast<INT32>(screenWidth_) * raw / COMMAND_COUNT);
	const INT16 right = static_cast<INT16>(
		static_cast<INT32>(screenWidth_) * (raw + 1) / COMMAND_COUNT);
	return { left, dock().y, static_cast<INT16>(right - left), DOCK_HEIGHT };
}

OS0UIRect OS0UILayout::clampWindow(OS0UIRect window) const noexcept
{
	window.w = std::clamp<INT16>(window.w, 1, screenWidth_);
	window.h = std::clamp<INT16>(window.h, 1,
		std::max<INT16>(1, workspaceBottom()));
	window.x = std::clamp<INT16>(window.x, 0,
		std::max<INT16>(0, screenWidth_ - window.w));
	window.y = std::clamp<INT16>(window.y, 0,
		std::max<INT16>(0, workspaceBottom() - window.h));
	return window;
}

OS0UIRuntime::OS0UIRuntime()
{
	for (OS0WindowTemplate const& definition : WINDOW_TEMPLATES)
		windowManager_.registerTemplate(definition);
	enterCampaign(FALSE);
}

void OS0UIRuntime::enterCampaign(BOOLEAN creatorCompleted) noexcept
{
	windowManager_.resetToDefaults();
	interactionMode_.reset();
	creatorActive_ = !creatorCompleted;
	creatorStage_ = static_cast<UINT8>(creatorCompleted ?
		OS0CreatorStage::COMPLETE : OS0CreatorStage::WELCOME);
	visibilityRef(OS0UIPanel::INVENTORY) = creatorActive_;
}

OS0CreatorStage OS0UIRuntime::creatorStage() const noexcept
{
	return static_cast<OS0CreatorStage>(creatorStage_);
}

BOOLEAN OS0UIRuntime::advanceCreator() noexcept
{
	if (!creatorActive_) return FALSE;
	switch (creatorStage())
	{
		case OS0CreatorStage::WELCOME:
			creatorStage_ = static_cast<UINT8>(OS0CreatorStage::IDENTITY);
			break;
		case OS0CreatorStage::IDENTITY:
			creatorStage_ = static_cast<UINT8>(OS0CreatorStage::ATTRIBUTES);
			break;
		case OS0CreatorStage::ATTRIBUTES:
			creatorStage_ = static_cast<UINT8>(OS0CreatorStage::TRAITS);
			break;
		case OS0CreatorStage::TRAITS:
			creatorStage_ = static_cast<UINT8>(OS0CreatorStage::CONTROLS);
			break;
		case OS0CreatorStage::CONTROLS:
			completeCreator();
			return TRUE;
		case OS0CreatorStage::COMPLETE:
			return FALSE;
	}
	return FALSE;
}

void OS0UIRuntime::completeCreator() noexcept
{
	creatorStage_ = static_cast<UINT8>(OS0CreatorStage::COMPLETE);
	creatorActive_ = FALSE;
	for (UINT8 value = 0; value < static_cast<UINT8>(OS0UIPanel::COUNT); ++value)
		windowManager_.hide(managedId(static_cast<OS0UIPanel>(value)));
}

size_t OS0UIRuntime::index(OS0UIPanel panel) noexcept
{
	const size_t value = static_cast<size_t>(panel);
	return value < static_cast<size_t>(OS0UIPanel::COUNT) ? value : 0;
}

BOOLEAN OS0UIRuntime::visible(OS0UIPanel panel) const noexcept
{
	return windowManager_.visible(managedId(panel));
}

BOOLEAN& OS0UIRuntime::visibilityRef(OS0UIPanel panel) noexcept
{
	return windowManager_.state(managedId(panel)).visible;
}

void OS0UIRuntime::show(OS0UIPanel panel) noexcept
{
	windowManager_.show(managedId(panel));
}

void OS0UIRuntime::hide(OS0UIPanel panel) noexcept
{
	windowManager_.hide(managedId(panel));
}

void OS0UIRuntime::toggle(OS0UIPanel panel) noexcept
{
	windowManager_.toggle(managedId(panel));
}

void OS0UIRuntime::hideTransientWorldPanels() noexcept
{
	hide(OS0UIPanel::CONTEXT);
	hide(OS0UIPanel::LOOT);
	hide(OS0UIPanel::STACK_SPLIT);
	hide(OS0UIPanel::ITEM_DETAILS);
}

OS0UIWindowState& OS0UIRuntime::panel(OS0UIPanel panel) noexcept
{
	return windowManager_.state(managedId(panel));
}

OS0UIWindowState const& OS0UIRuntime::panel(OS0UIPanel panel) const noexcept
{
	return windowManager_.state(managedId(panel));
}

size_t OS0UIRuntime::index(OS0UIWindow window) noexcept
{
	const size_t value = static_cast<size_t>(window);
	return value < static_cast<size_t>(OS0UIWindow::COUNT) ? value : 0;
}

OS0UIWindowState& OS0UIRuntime::window(OS0UIWindow window) noexcept
{
	return windowManager_.state(managedId(window));
}

OS0UIWindowState const& OS0UIRuntime::window(OS0UIWindow window) const noexcept
{
	return windowManager_.state(managedId(window));
}

OS0WindowHandle OS0UIRuntime::managedId(OS0UIPanel panel) const noexcept
{
	return static_cast<OS0WindowHandle>(index(panel));
}

OS0WindowHandle OS0UIRuntime::managedId(OS0UIWindow window) const noexcept
{
	return static_cast<OS0WindowHandle>(
		static_cast<size_t>(OS0ManagedWindow::SECTOR) + index(window));
}

OS0UICommandDescriptor const& GetOS0UICommandDescriptor(
	OS0UICommand command) noexcept
{
	const size_t index = static_cast<size_t>(command);
	return COMMANDS[index < COMMANDS.size() ? index : 0];
}

OS0UIWindowDescriptor const& GetOS0UIWindowDescriptor(
	OS0UIWindow window) noexcept
{
	const size_t index = static_cast<size_t>(window);
	return WINDOWS[index < WINDOWS.size() ? index : 0];
}

OS0UIWindow OS0UIWindowFromPersistenceKey(const char* key) noexcept
{
	if (!key) return OS0UIWindow::COUNT;
	for (OS0UIWindowDescriptor const& descriptor : WINDOWS)
	{
		if (std::strcmp(key, descriptor.persistenceKey) == 0)
			return descriptor.window;
	}
	return OS0UIWindow::COUNT;
}

OS0UICommand OS0CommandForDockSlot(size_t slot) noexcept
{
	// TACTICAL is the physical OS//0 orb. Dock slot zero starts with CHARACTER.
	const size_t command = slot + 1;
	return command < static_cast<size_t>(OS0UICommand::COUNT) ?
		static_cast<OS0UICommand>(command) : OS0UICommand::COUNT;
}
