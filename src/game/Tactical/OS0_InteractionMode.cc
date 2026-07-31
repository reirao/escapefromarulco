#include "OS0_InteractionMode.h"

namespace
{
	constexpr const char* STATE_NAMES[]{ "NORMAL", "INTERACTING", "FIGHT" };
	constexpr const char* SURFACE_NAMES[]{
		"ACTIONS", "BEHAVIOR", "EQUIPMENT", "ENVIRONMENT"
	};
}

const char* OS0InteractionStateName(OS0InteractionState const state) noexcept
{
	auto const index = static_cast<std::uint8_t>(state);
	return index < static_cast<std::uint8_t>(OS0InteractionState::COUNT) ?
		STATE_NAMES[index] : "UNKNOWN";
}

const char* OS0InteractionSurfaceName(
	OS0InteractionSurface const surface) noexcept
{
	auto const index = static_cast<std::uint8_t>(surface);
	return index < static_cast<std::uint8_t>(OS0InteractionSurface::COUNT) ?
		SURFACE_NAMES[index] : "UNKNOWN";
}

bool OS0InteractionMode::valid(OS0InteractionState const state) noexcept
{
	return state >= OS0InteractionState::NORMAL &&
		state < OS0InteractionState::COUNT;
}

bool OS0InteractionMode::valid(OS0InteractionSurface const surface) noexcept
{
	return surface >= OS0InteractionSurface::ACTIONS &&
		surface < OS0InteractionSurface::COUNT;
}

bool OS0InteractionMode::transitionTo(OS0InteractionState const next) noexcept
{
	if (!valid(next)) return false;
	if (next == OS0InteractionState::FIGHT && !isFight())
	{
		surfaceBeforeFight_ = surface_;
		hasSurfaceBeforeFight_ = true;
	}
	else if (next != OS0InteractionState::FIGHT && isFight() &&
		hasSurfaceBeforeFight_)
	{
		surface_ = surfaceBeforeFight_;
		hasSurfaceBeforeFight_ = false;
	}
	state_ = next;
	return true;
}

bool OS0InteractionMode::beginInteraction(
	OS0InteractionSurface const surface) noexcept
{
	if (!valid(surface)) return false;
	transitionTo(OS0InteractionState::INTERACTING);
	surface_ = surface;
	return true;
}

bool OS0InteractionMode::beginFight(OS0InteractionSurface const surface) noexcept
{
	if (!valid(surface)) return false;
	transitionTo(OS0InteractionState::FIGHT);
	surface_ = surface;
	return true;
}

void OS0InteractionMode::returnToNormal() noexcept
{
	transitionTo(OS0InteractionState::NORMAL);
}

bool OS0InteractionMode::returnToNormal(
	OS0InteractionSurface const selectedSurface) noexcept
{
	if (!valid(selectedSurface)) return false;
	transitionTo(OS0InteractionState::NORMAL);
	surface_ = selectedSurface;
	return true;
}

bool OS0InteractionMode::selectSurface(
	OS0InteractionSurface const surface) noexcept
{
	if (!valid(surface)) return false;
	surface_ = surface;
	return true;
}

bool OS0InteractionMode::setNearbyScanEnabled(bool const enabled) noexcept
{
	if (enabled && !canScanNearby()) return false;
	nearbyScanRequested_ = enabled;
	return true;
}

OS0CancellationLayer OS0SelectCancellationLayer(
	OS0CancellationFacts const& facts) noexcept
{
	if (facts.modal) return OS0CancellationLayer::MODAL;
	if (facts.heldItem) return OS0CancellationLayer::HELD_ITEM;
	if (facts.worldManipulation)
		return OS0CancellationLayer::WORLD_MANIPULATION;
	if (facts.approach) return OS0CancellationLayer::APPROACH;
	if (facts.cursorAction) return OS0CancellationLayer::CURSOR_ACTION;
	return OS0CancellationLayer::NONE;
}

bool OS0InteractionMode::toggleNearbyScan() noexcept
{
	return setNearbyScanEnabled(!nearbyScanRequested_);
}

bool OS0InteractionMode::beginPerception() noexcept
{
	if (!setNearbyScanEnabled(true)) return false;
	surface_ = OS0InteractionSurface::ENVIRONMENT;
	return true;
}

void OS0InteractionMode::synchronize(
	OS0InteractionFrameFacts const& facts) noexcept
{
	if (facts.tutorial)
	{
		reset();
		return;
	}
	if (facts.fight)
	{
		if (!isFight()) beginFight(OS0InteractionSurface::ACTIONS);
		return;
	}
	if (isFight()) transitionTo(OS0InteractionState::NORMAL);
	// Context radials and floating windows are overlays. Their visibility must
	// never manufacture a gameplay state or replace the active control intent.
	if (facts.cursorAction && valid(facts.cursorSurface))
	{
		beginInteraction(facts.cursorSurface);
		return;
	}
	if (facts.passiveInteraction)
	{
		transitionTo(OS0InteractionState::INTERACTING);
		return;
	}
	returnToNormal();
}

void OS0InteractionMode::reset() noexcept
{
	state_ = OS0InteractionState::NORMAL;
	surface_ = OS0InteractionSurface::ACTIONS;
	surfaceBeforeFight_ = OS0InteractionSurface::ACTIONS;
	hasSurfaceBeforeFight_ = false;
	nearbyScanRequested_ = false;
}
