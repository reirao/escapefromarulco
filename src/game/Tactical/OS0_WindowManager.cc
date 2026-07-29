#include "OS0_WindowManager.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>

BOOLEAN OS0UIRect::contains(INT16 pointX, INT16 pointY) const noexcept
{
	return pointX >= x && pointX < x + w && pointY >= y && pointY < y + h;
}

OS0WindowManager::OS0WindowManager() noexcept
{
	for (OS0WindowState& state : states_) state = {};
}

size_t OS0WindowManager::index(OS0WindowHandle id) noexcept
{
	return static_cast<size_t>(id);
}

BOOLEAN OS0WindowManager::registerTemplate(
	OS0WindowTemplate const& definition) noexcept
{
	const size_t slot = index(definition.id);
	if (definition.id == OS0_INVALID_WINDOW || slot >= OS0_MAX_WINDOWS ||
		!definition.persistenceKey || !definition.title ||
		definition.defaultBounds.w <= 0 || definition.defaultBounds.h <= 0)
		return FALSE;
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		if (i != slot && registered_[i] &&
			std::strcmp(definitions_[i].persistenceKey,
				definition.persistenceKey) == 0)
			return FALSE;
	}
	definitions_[slot] = definition;
	registered_[slot] = TRUE;
	OS0WindowState& state = states_[slot];
	state.x = definition.defaultBounds.x;
	state.y = definition.defaultBounds.y;
	state.w = std::max(definition.minimumWidth, definition.defaultBounds.w);
	state.h = std::max(definition.minimumHeight, definition.defaultBounds.h);
	state.visible = definition.defaultVisible;
	state.dragging = FALSE;
	state.suspensionMask = 0;
	state.zOrder = definition.defaultLayer;
	nextZ_ = std::max<INT16>(nextZ_, static_cast<INT16>(state.zOrder + 1));
	clamp(definition.id);
	return TRUE;
}

BOOLEAN OS0WindowManager::registered(OS0WindowHandle id) const noexcept
{
	const size_t slot = index(id);
	return slot < registered_.size() && registered_[slot];
}

OS0WindowTemplate const* OS0WindowManager::definition(
	OS0WindowHandle id) const noexcept
{
	return registered(id) ? &definitions_[index(id)] : nullptr;
}

OS0WindowHandle OS0WindowManager::fromPersistenceKey(const char* key) const noexcept
{
	if (!key) return OS0_INVALID_WINDOW;
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		if (registered_[i] &&
			std::strcmp(definitions_[i].persistenceKey, key) == 0)
			return static_cast<OS0WindowHandle>(i);
	}
	return OS0_INVALID_WINDOW;
}

OS0WindowState& OS0WindowManager::state(OS0WindowHandle id) noexcept
{
	const size_t slot = index(id);
	return states_[slot < states_.size() ? slot : 0];
}

OS0WindowState const& OS0WindowManager::state(OS0WindowHandle id) const noexcept
{
	const size_t slot = index(id);
	return states_[slot < states_.size() ? slot : 0];
}

void OS0WindowManager::resetToDefaults() noexcept
{
	dragging_ = OS0_INVALID_WINDOW;
	nextZ_ = 1;
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		if (!registered_[i]) continue;
		OS0WindowTemplate const& definition = definitions_[i];
		OS0WindowState& state = states_[i];
		state = {
			definition.defaultBounds.x, definition.defaultBounds.y,
			std::max(definition.minimumWidth, definition.defaultBounds.w),
			std::max(definition.minimumHeight, definition.defaultBounds.h),
			definition.defaultVisible, FALSE, 0, definition.defaultLayer
		};
		nextZ_ = std::max<INT16>(nextZ_, static_cast<INT16>(state.zOrder + 1));
	}
	clampAll();
}

void OS0WindowManager::setWorkspace(OS0UIRect workspace) noexcept
{
	workspace.w = std::max<INT16>(1, workspace.w);
	workspace.h = std::max<INT16>(1, workspace.h);
	workspace_ = workspace;
	clampAll();
}

OS0UIRect OS0WindowManager::clampRect(OS0WindowHandle id,
	OS0UIRect rect) const noexcept
{
	OS0WindowTemplate const* const spec = definition(id);
	const INT16 minimumWidth = spec ? spec->minimumWidth : 1;
	const INT16 minimumHeight = spec ? spec->minimumHeight : 1;
	rect.w = std::clamp<INT16>(rect.w, minimumWidth, workspace_.w);
	rect.h = std::clamp<INT16>(rect.h, minimumHeight, workspace_.h);
	rect.x = std::clamp<INT16>(rect.x, workspace_.x,
		static_cast<INT16>(workspace_.x + workspace_.w - rect.w));
	rect.y = std::clamp<INT16>(rect.y, workspace_.y,
		static_cast<INT16>(workspace_.y + workspace_.h - rect.h));
	return rect;
}

void OS0WindowManager::setBounds(OS0WindowHandle id, OS0UIRect bounds) noexcept
{
	if (!registered(id)) return;
	OS0UIRect const safe = clampRect(id, bounds);
	OS0WindowState& window = state(id);
	window.x = safe.x;
	window.y = safe.y;
	window.w = safe.w;
	window.h = safe.h;
}

OS0UIRect OS0WindowManager::bounds(OS0WindowHandle id) const noexcept
{
	OS0WindowState const& window = state(id);
	return { window.x, window.y, window.w, window.h };
}

void OS0WindowManager::clamp(OS0WindowHandle id) noexcept
{
	if (!registered(id)) return;
	setBounds(id, bounds(id));
}

void OS0WindowManager::clampAll() noexcept
{
	for (size_t i = 0; i < registered_.size(); ++i)
		if (registered_[i]) clamp(static_cast<OS0WindowHandle>(i));
}

void OS0WindowManager::show(OS0WindowHandle id) noexcept
{
	if (!registered(id)) return;
	state(id).visible = TRUE;
	bringToFront(id);
}

void OS0WindowManager::hide(OS0WindowHandle id) noexcept
{
	if (!registered(id)) return;
	state(id).visible = FALSE;
	if (dragging_ == id) cancelDrag();
}

void OS0WindowManager::toggle(OS0WindowHandle id) noexcept
{
	if (!registered(id)) return;
	if (state(id).visible) hide(id);
	else show(id);
}

BOOLEAN OS0WindowManager::requestedVisible(OS0WindowHandle id) const noexcept
{
	return registered(id) && state(id).visible;
}

BOOLEAN OS0WindowManager::visible(OS0WindowHandle id) const noexcept
{
	return requestedVisible(id) && state(id).suspensionMask == 0;
}

UINT8 OS0WindowManager::suspensionBit(OS0WindowSuspendReason reason) noexcept
{
	const UINT8 value = static_cast<UINT8>(reason);
	return value < static_cast<UINT8>(OS0WindowSuspendReason::COUNT) ?
		static_cast<UINT8>(1U << value) : 0;
}

void OS0WindowManager::setSuspended(OS0WindowSuspendReason reason,
	BOOLEAN suspended) noexcept
{
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		if (!registered_[i]) continue;
		OS0WindowTemplate const& spec = definitions_[i];
		if (reason == OS0WindowSuspendReason::AIM &&
			!(spec.features & OS0_WINDOW_COLLAPSE_DURING_AIM)) continue;
		setSuspended(static_cast<OS0WindowHandle>(i), reason, suspended);
	}
}

void OS0WindowManager::setSuspended(OS0WindowHandle id,
	OS0WindowSuspendReason reason, BOOLEAN suspended) noexcept
{
	if (!registered(id)) return;
	UINT8 const bit = suspensionBit(reason);
	if (suspended) state(id).suspensionMask |= bit;
	else state(id).suspensionMask &= static_cast<UINT8>(~bit);
	if (suspended && dragging_ == id) cancelDrag();
}

void OS0WindowManager::hideTransient() noexcept
{
	for (size_t i = 0; i < registered_.size(); ++i)
		if (registered_[i] &&
			(definitions_[i].features & OS0_WINDOW_TRANSIENT))
			hide(static_cast<OS0WindowHandle>(i));
}

BOOLEAN OS0WindowManager::beginDrag(OS0WindowHandle id, INT16 pointerX,
	INT16 pointerY) noexcept
{
	OS0WindowTemplate const* const spec = definition(id);
	if (!spec || !(spec->features & OS0_WINDOW_MOVABLE) || !visible(id))
		return FALSE;
	cancelDrag();
	OS0WindowState& window = state(id);
	window.dragging = TRUE;
	dragging_ = id;
	dragOffsetX_ = pointerX - window.x;
	dragOffsetY_ = pointerY - window.y;
	bringToFront(id);
	return TRUE;
}

BOOLEAN OS0WindowManager::dragTo(INT16 pointerX, INT16 pointerY) noexcept
{
	if (!registered(dragging_)) return FALSE;
	OS0UIRect const old = bounds(dragging_);
	setBounds(dragging_, { static_cast<INT16>(pointerX - dragOffsetX_),
		static_cast<INT16>(pointerY - dragOffsetY_), old.w, old.h });
	OS0UIRect const current = bounds(dragging_);
	return current.x != old.x || current.y != old.y;
}

BOOLEAN OS0WindowManager::endDrag() noexcept
{
	if (!registered(dragging_)) return FALSE;
	state(dragging_).dragging = FALSE;
	dragging_ = OS0_INVALID_WINDOW;
	return TRUE;
}

void OS0WindowManager::cancelDrag() noexcept
{
	if (registered(dragging_)) state(dragging_).dragging = FALSE;
	dragging_ = OS0_INVALID_WINDOW;
}

void OS0WindowManager::bringToFront(OS0WindowHandle id) noexcept
{
	if (!registered(id)) return;
	if (nextZ_ >= 30000)
	{
		std::vector<OS0WindowHandle> const order = renderOrder();
		INT16 z = 1;
		for (OS0WindowHandle window : order) state(window).zOrder = z++;
		nextZ_ = z;
	}
	state(id).zOrder = nextZ_++;
}

std::vector<OS0WindowHandle> OS0WindowManager::renderOrder() const
{
	std::vector<OS0WindowHandle> result;
	for (size_t i = 0; i < registered_.size(); ++i)
		if (registered_[i] && visible(static_cast<OS0WindowHandle>(i)))
			result.push_back(static_cast<OS0WindowHandle>(i));
	std::stable_sort(result.begin(), result.end(), [this](auto left, auto right)
	{
		return state(left).zOrder < state(right).zOrder;
	});
	return result;
}

std::vector<OS0WindowHandle> OS0WindowManager::dockEntries() const
{
	std::vector<OS0WindowHandle> result;
	for (size_t i = 0; i < registered_.size(); ++i)
		if (registered_[i] &&
			(definitions_[i].features & OS0_WINDOW_DOCK_ENTRY))
			result.push_back(static_cast<OS0WindowHandle>(i));
	std::stable_sort(result.begin(), result.end(), [this](auto left, auto right)
	{
		return definition(left)->dockOrder < definition(right)->dockOrder;
	});
	return result;
}

OS0WindowHandle OS0WindowManager::hitTest(INT16 pointX,
	INT16 pointY) const noexcept
{
	OS0WindowHandle best = OS0_INVALID_WINDOW;
	INT16 bestZ = -32768;
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		OS0WindowHandle const id = static_cast<OS0WindowHandle>(i);
		if (!registered_[i] || !visible(id)) continue;
		OS0UIRect interactionBounds = bounds(id);
		// Radials store the world/actor anchor in x/y. Treating that anchor as
		// a conventional top-left rectangle made their input shield jump down
		// and right from the symbols that were actually rendered.
		if (definitions_[i].presentation == OS0WindowPresentation::RADIAL)
		{
			interactionBounds.x = static_cast<INT16>(
				interactionBounds.x - interactionBounds.w / 2);
			interactionBounds.y = static_cast<INT16>(
				interactionBounds.y - interactionBounds.h / 2);
		}
		if (!interactionBounds.contains(pointX, pointY)) continue;
		if (best == OS0_INVALID_WINDOW || state(id).zOrder >= bestZ)
		{
			best = id;
			bestZ = state(id).zOrder;
		}
	}
	return best;
}

BOOLEAN OS0WindowManager::blocksWorldInputAt(INT16 pointX,
	INT16 pointY) const noexcept
{
	// A modal owns the complete workspace, including the area outside its
	// visible card. This makes modal behaviour independent from engine mouse
	// region ordering and prevents click-through at odd resolutions.
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		OS0WindowHandle const id = static_cast<OS0WindowHandle>(i);
		if (registered_[i] && visible(id) &&
			definitions_[i].presentation == OS0WindowPresentation::MODAL &&
			(definitions_[i].features & OS0_WINDOW_BLOCKS_WORLD_INPUT))
			return TRUE;
	}
	OS0WindowHandle const id = hitTest(pointX, pointY);
	OS0WindowTemplate const* const spec = definition(id);
	return spec && (spec->features & OS0_WINDOW_BLOCKS_WORLD_INPUT);
}

ST::string OS0WindowManager::serializeLayout(INT16 screenWidth,
	INT16 screenHeight) const
{
	std::ostringstream output;
	output << "# Escape from Arulco window layout v3\n";
	output << "screen " << screenWidth << ' ' << screenHeight << '\n';
	for (OS0WindowHandle id : renderOrder())
	{
		OS0WindowTemplate const* const spec = definition(id);
		if (!spec || !(spec->features & OS0_WINDOW_PERSIST_POSITION)) continue;
		OS0WindowState const& window = state(id);
		output << "window " << spec->persistenceKey << ' ' << window.x << ' '
			<< window.y << ' ' << window.w << ' ' << window.h << ' '
			<< window.zOrder << '\n';
	}
	// Hidden windows are absent from renderOrder but still need their geometry.
	for (size_t i = 0; i < registered_.size(); ++i)
	{
		OS0WindowHandle const id = static_cast<OS0WindowHandle>(i);
		OS0WindowTemplate const& spec = definitions_[i];
		if (!registered_[i] || visible(id) ||
			!(spec.features & OS0_WINDOW_PERSIST_POSITION)) continue;
		OS0WindowState const& window = states_[i];
		output << "window " << spec.persistenceKey << ' ' << window.x << ' '
			<< window.y << ' ' << window.w << ' ' << window.h << ' '
			<< window.zOrder << '\n';
	}
	return output.str();
}

BOOLEAN OS0WindowManager::restoreLayout(ST::string const& text,
	INT16 screenWidth, INT16 screenHeight) noexcept
try
{
	std::istringstream input(text.c_str());
	std::string line;
	INT32 savedWidth = std::max<INT16>(1, screenWidth);
	INT32 savedHeight = std::max<INT16>(1, screenHeight);
	struct Pending
	{
		OS0WindowHandle id;
		INT32 x;
		INT32 y;
		INT32 w;
		INT32 h;
		INT32 z;
	};
	std::vector<Pending> pending;
	while (std::getline(input, line))
	{
		if (line.empty() || line[0] == '#') continue;
		std::istringstream row(line);
		std::string kind;
		row >> kind;
		if (kind == "screen")
		{
			row >> savedWidth >> savedHeight;
			savedWidth = std::max<INT32>(1, savedWidth);
			savedHeight = std::max<INT32>(1, savedHeight);
		}
		else if (kind == "window")
		{
			std::string key;
			Pending value{};
			if (!(row >> key >> value.x >> value.y >> value.w >> value.h >> value.z))
				continue;
			value.id = fromPersistenceKey(key.c_str());
			if (value.id != OS0_INVALID_WINDOW) pending.push_back(value);
		}
	}
	for (Pending const& value : pending)
	{
		OS0UIRect scaled{
			static_cast<INT16>(value.x * screenWidth / savedWidth),
			static_cast<INT16>(value.y * screenHeight / savedHeight),
			static_cast<INT16>(std::max<INT32>(1,
				value.w * screenWidth / savedWidth)),
			static_cast<INT16>(std::max<INT32>(1,
				value.h * screenHeight / savedHeight))
		};
		setBounds(value.id, scaled);
		state(value.id).zOrder = static_cast<INT16>(std::clamp<INT32>(
			value.z, -32767, 32766));
		nextZ_ = std::max<INT16>(nextZ_,
			static_cast<INT16>(state(value.id).zOrder + 1));
	}
	return TRUE;
}
catch (...)
{
	return FALSE;
}
