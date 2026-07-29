#pragma once

#include "JA2Types.h"
#include "OS0_InteractionMode.h"
#include "OS0_UIAssetManager.h"
#include "OS0_WindowManager.h"

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

enum class OS0UICommandIntent : UINT8
{
	RETURN_TO_ACTIONS,
	TOGGLE_EQUIPMENT,
	OPEN_BEHAVIOR,
	TOGGLE_NEARBY_SCAN,
	TOGGLE_ENVIRONMENT,
	OPEN_ASSET_LIBRARY,
	TOGGLE_REALTIME_EDITOR,
	TOGGLE_STRATEGY,
	OPEN_ICON_LIBRARY
};

struct OS0UICommandDescriptor
{
	OS0UICommand command;
	const char* label;
	const char* tooltip;
	OS0UIIcon icon;
	OS0UICommandIntent intent;
};

enum class OS0UIWindow : UINT8
{
	SECTOR,
	INSPECTOR,
	TOOLBOX,
	ENVIRONMENT,
	REALTIME_EDITOR,
	COUNT
};

struct OS0UIWindowDescriptor
{
	OS0UIWindow window;
	const char* persistenceKey;
	const char* title;
	OS0UIIcon icon;
	BOOLEAN defaultVisible;
};

// Stable IDs for the single window manager. Legacy panel/window enums remain as
// semantic call-site types while both resolve to this one canonical state set.
enum class OS0ManagedWindow : OS0WindowHandle
{
	INVENTORY,
	CONTEXT,
	LOOT,
	EQUIPMENT,
	STACK_SPLIT,
	ASSET_LIBRARY,
	ASSET_CATALOG,
	ITEM_DETAILS,
	SECTOR,
	INSPECTOR,
	TOOLBOX,
	ENVIRONMENT,
	REALTIME_EDITOR,
	COUNT
};

using OS0UIWindowState = OS0WindowState;

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
	OS0UIWindowState& panel(OS0UIPanel panel) noexcept;
	OS0UIWindowState const& panel(OS0UIPanel panel) const noexcept;

	OS0UIWindowState& window(OS0UIWindow window) noexcept;
	OS0UIWindowState const& window(OS0UIWindow window) const noexcept;
	OS0WindowManager& windowManager() noexcept { return windowManager_; }
	OS0WindowManager const& windowManager() const noexcept { return windowManager_; }
	OS0InteractionMode& interactionMode() noexcept { return interactionMode_; }
	OS0InteractionMode const& interactionMode() const noexcept
	{
		return interactionMode_;
	}
	OS0WindowHandle managedId(OS0UIPanel panel) const noexcept;
	OS0WindowHandle managedId(OS0UIWindow window) const noexcept;

private:
	static size_t index(OS0UIPanel panel) noexcept;
	static size_t index(OS0UIWindow window) noexcept;

	BOOLEAN creatorActive_ = TRUE;
	UINT8 creatorStage_ = static_cast<UINT8>(OS0CreatorStage::WELCOME);
	OS0WindowManager windowManager_;
	OS0InteractionMode interactionMode_;
};

OS0UICommandDescriptor const& GetOS0UICommandDescriptor(
	OS0UICommand command) noexcept;
OS0UIWindowDescriptor const& GetOS0UIWindowDescriptor(
	OS0UIWindow window) noexcept;
OS0UIWindow OS0UIWindowFromPersistenceKey(const char* key) noexcept;
OS0UICommand OS0CommandForDockSlot(size_t slot) noexcept;
