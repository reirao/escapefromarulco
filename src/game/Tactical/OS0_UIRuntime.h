#pragma once

#include "JA2Types.h"

#include <array>
#include <cstddef>

// The tactical UI has one state owner. Renderers only project this model and
// callbacks only dispatch transitions into it; no panel owns another panel.
enum class OS0UIPanel : UINT8
{
	INVENTORY,
	CONTEXT,
	LOOT,
	EQUIPMENT,
	STACK_SPLIT,
	ASSET_LIBRARY,
	ASSET_CATALOG,
	ITEM_DETAILS,
	COUNT
};

enum class OS0CreatorStage : UINT8
{
	WELCOME = 0,
	IDENTITY = 1,
	ATTRIBUTES = 2,
	TRAITS = 3,
	CONTROLS = 5,
	COMPLETE = 6
};

enum class OS0UICommand : UINT8
{
	TACTICAL,
	CHARACTER,
	STEALTH,
	OBJECT,
	WORLD,
	ASSETS,
	TERRAIN,
	STRATEGY,
	SANDBOX,
	COUNT
};

struct OS0UIRect
{
	INT16 x = 0;
	INT16 y = 0;
	INT16 w = 0;
	INT16 h = 0;

	BOOLEAN contains(INT16 pointX, INT16 pointY) const noexcept;
};

class OS0UILayout
{
public:
	static constexpr INT16 DOCK_HEIGHT = 38;
	static constexpr size_t COMMAND_COUNT =
		static_cast<size_t>(OS0UICommand::COUNT);

	void configure(INT16 screenWidth, INT16 screenHeight,
		INT16 engineWorldBottom) noexcept;
	INT16 workspaceBottom() const noexcept;
	INT16 worldBottom() const noexcept;
	OS0UIRect dock() const noexcept;
	OS0UIRect command(OS0UICommand command) const noexcept;
	OS0UIRect clampWindow(OS0UIRect window) const noexcept;

private:
	INT16 screenWidth_ = 640;
	INT16 screenHeight_ = 480;
	INT16 engineWorldBottom_ = 442;
};

class OS0UIRuntime
{
public:
	OS0UIRuntime();

	void enterCampaign(BOOLEAN creatorCompleted) noexcept;
	BOOLEAN creatorActive() const noexcept { return creatorActive_; }
	BOOLEAN& creatorActiveRef() noexcept { return creatorActive_; }
	OS0CreatorStage creatorStage() const noexcept;
	UINT8& creatorStageValueRef() noexcept { return creatorStage_; }
	BOOLEAN advanceCreator() noexcept;
	void completeCreator() noexcept;

	BOOLEAN visible(OS0UIPanel panel) const noexcept;
	BOOLEAN& visibilityRef(OS0UIPanel panel) noexcept;
	void show(OS0UIPanel panel) noexcept;
	void hide(OS0UIPanel panel) noexcept;
	void toggle(OS0UIPanel panel) noexcept;
	void hideTransientWorldPanels() noexcept;

private:
	static size_t index(OS0UIPanel panel) noexcept;

	BOOLEAN creatorActive_ = TRUE;
	UINT8 creatorStage_ = static_cast<UINT8>(OS0CreatorStage::WELCOME);
	std::array<BOOLEAN, static_cast<size_t>(OS0UIPanel::COUNT)> visible_{};
};

OS0UICommand OS0CommandForDockSlot(size_t slot) noexcept;
