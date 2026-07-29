#include "OS0_UIRuntime.h"

#include <algorithm>

BOOLEAN OS0UIRect::contains(INT16 pointX, INT16 pointY) const noexcept
{
	return pointX >= x && pointX < x + w && pointY >= y && pointY < y + h;
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
	enterCampaign(FALSE);
}

void OS0UIRuntime::enterCampaign(BOOLEAN creatorCompleted) noexcept
{
	visible_.fill(FALSE);
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
	visible_.fill(FALSE);
}

size_t OS0UIRuntime::index(OS0UIPanel panel) noexcept
{
	const size_t value = static_cast<size_t>(panel);
	return value < static_cast<size_t>(OS0UIPanel::COUNT) ? value : 0;
}

BOOLEAN OS0UIRuntime::visible(OS0UIPanel panel) const noexcept
{
	return visible_[index(panel)];
}

BOOLEAN& OS0UIRuntime::visibilityRef(OS0UIPanel panel) noexcept
{
	return visible_[index(panel)];
}

void OS0UIRuntime::show(OS0UIPanel panel) noexcept
{
	visible_[index(panel)] = TRUE;
}

void OS0UIRuntime::hide(OS0UIPanel panel) noexcept
{
	visible_[index(panel)] = FALSE;
}

void OS0UIRuntime::toggle(OS0UIPanel panel) noexcept
{
	BOOLEAN& value = visible_[index(panel)];
	value = !value;
}

void OS0UIRuntime::hideTransientWorldPanels() noexcept
{
	hide(OS0UIPanel::CONTEXT);
	hide(OS0UIPanel::LOOT);
	hide(OS0UIPanel::STACK_SPLIT);
	hide(OS0UIPanel::ITEM_DETAILS);
}

OS0UICommand OS0CommandForDockSlot(size_t slot) noexcept
{
	// TACTICAL is the physical OS//0 orb. Dock slot zero starts with CHARACTER.
	const size_t command = slot + 1;
	return command < static_cast<size_t>(OS0UICommand::COUNT) ?
		static_cast<OS0UICommand>(command) : OS0UICommand::COUNT;
}
