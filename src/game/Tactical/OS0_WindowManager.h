#pragma once

#include "JA2Types.h"
#include "OS0_UIAssetManager.h"

#include <array>
#include <cstddef>
#include <string_theory/string>
#include <vector>

struct OS0UIRect
{
	INT16 x = 0;
	INT16 y = 0;
	INT16 w = 0;
	INT16 h = 0;

	BOOLEAN contains(INT16 pointX, INT16 pointY) const noexcept;
};

using OS0WindowHandle = UINT8;
constexpr OS0WindowHandle OS0_INVALID_WINDOW = 0xff;
constexpr size_t OS0_MAX_WINDOWS = 24;

enum class OS0WindowPresentation : UINT8
{
	FLOATING,
	MODAL,
	RADIAL,
	WORLD_ATTACHED
};

enum OS0WindowFeature : UINT16
{
	OS0_WINDOW_MOVABLE = 1U << 0,
	OS0_WINDOW_CLOSABLE = 1U << 1,
	OS0_WINDOW_BLOCKS_WORLD_INPUT = 1U << 2,
	OS0_WINDOW_PERSIST_POSITION = 1U << 3,
	OS0_WINDOW_TRANSIENT = 1U << 4,
	OS0_WINDOW_COLLAPSE_DURING_AIM = 1U << 5,
	OS0_WINDOW_DOCK_ENTRY = 1U << 6
};

enum class OS0WindowSuspendReason : UINT8
{
	AIM = 0,
	MODAL = 1,
	WORLD_SWAP = 2,
	COUNT
};

struct OS0WindowTemplate
{
	OS0WindowHandle id = OS0_INVALID_WINDOW;
	const char* persistenceKey = "";
	const char* title = "";
	OS0UIIcon icon = OS0UIIcon::LOOK;
	OS0WindowPresentation presentation = OS0WindowPresentation::FLOATING;
	OS0UIRect defaultBounds{};
	INT16 minimumWidth = 1;
	INT16 minimumHeight = 1;
	UINT16 features = OS0_WINDOW_MOVABLE | OS0_WINDOW_CLOSABLE |
		OS0_WINDOW_BLOCKS_WORLD_INPUT | OS0_WINDOW_PERSIST_POSITION |
		OS0_WINDOW_COLLAPSE_DURING_AIM;
	BOOLEAN defaultVisible = FALSE;
	INT16 defaultLayer = 0;
	INT16 dockOrder = -1;
};

// One state representation is shared by every OS0 window. visible
// records the user's choice; suspension only affects effective visibility and
// therefore never destroys that choice during aim, a modal dialog or a map swap.
struct OS0WindowState
{
	INT16 x = 0;
	INT16 y = 0;
	INT16 w = 1;
	INT16 h = 1;
	BOOLEAN visible = FALSE;
	BOOLEAN dragging = FALSE;
	UINT8 suspensionMask = 0;
	INT16 zOrder = 0;
};

class OS0WindowManager
{
public:
	OS0WindowManager() noexcept;

	BOOLEAN registerTemplate(OS0WindowTemplate const& definition) noexcept;
	BOOLEAN registered(OS0WindowHandle id) const noexcept;
	OS0WindowTemplate const* definition(OS0WindowHandle id) const noexcept;
	OS0WindowHandle fromPersistenceKey(const char* key) const noexcept;

	OS0WindowState& state(OS0WindowHandle id) noexcept;
	OS0WindowState const& state(OS0WindowHandle id) const noexcept;
	void resetToDefaults() noexcept;

	void setWorkspace(OS0UIRect workspace) noexcept;
	OS0UIRect workspace() const noexcept { return workspace_; }
	void setBounds(OS0WindowHandle id, OS0UIRect bounds) noexcept;
	OS0UIRect bounds(OS0WindowHandle id) const noexcept;
	void clamp(OS0WindowHandle id) noexcept;
	void clampAll() noexcept;

	void show(OS0WindowHandle id) noexcept;
	void hide(OS0WindowHandle id) noexcept;
	void toggle(OS0WindowHandle id) noexcept;
	BOOLEAN requestedVisible(OS0WindowHandle id) const noexcept;
	BOOLEAN visible(OS0WindowHandle id) const noexcept;
	void setSuspended(OS0WindowSuspendReason reason, BOOLEAN suspended) noexcept;
	void setSuspended(OS0WindowHandle id, OS0WindowSuspendReason reason,
		BOOLEAN suspended) noexcept;
	void hideTransient() noexcept;

	BOOLEAN beginDrag(OS0WindowHandle id, INT16 pointerX,
		INT16 pointerY) noexcept;
	BOOLEAN dragTo(INT16 pointerX, INT16 pointerY) noexcept;
	BOOLEAN endDrag() noexcept;
	void cancelDrag() noexcept;
	OS0WindowHandle draggingWindow() const noexcept { return dragging_; }
	void bringToFront(OS0WindowHandle id) noexcept;

	std::vector<OS0WindowHandle> renderOrder() const;
	std::vector<OS0WindowHandle> dockEntries() const;
	OS0WindowHandle hitTest(INT16 pointX, INT16 pointY) const noexcept;
	BOOLEAN blocksWorldInputAt(INT16 pointX, INT16 pointY) const noexcept;

	ST::string serializeLayout(INT16 screenWidth, INT16 screenHeight) const;
	BOOLEAN restoreLayout(ST::string const& text, INT16 screenWidth,
		INT16 screenHeight) noexcept;

private:
	static size_t index(OS0WindowHandle id) noexcept;
	OS0UIRect clampRect(OS0WindowHandle id, OS0UIRect rect) const noexcept;
	static UINT8 suspensionBit(OS0WindowSuspendReason reason) noexcept;

	std::array<OS0WindowTemplate, OS0_MAX_WINDOWS> definitions_{};
	std::array<OS0WindowState, OS0_MAX_WINDOWS> states_{};
	std::array<BOOLEAN, OS0_MAX_WINDOWS> registered_{};
	OS0UIRect workspace_{ 0, 0, 640, 442 };
	OS0WindowHandle dragging_ = OS0_INVALID_WINDOW;
	INT16 dragOffsetX_ = 0;
	INT16 dragOffsetY_ = 0;
	INT16 nextZ_ = 1;
};
