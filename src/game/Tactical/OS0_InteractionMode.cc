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
	if (state_ == OS0InteractionState::FIGHT) nearbyScanEnabled_ = false;
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
	nearbyScanEnabled_ = enabled;
	return true;
}

bool OS0InteractionMode::toggleNearbyScan() noexcept
{
	return setNearbyScanEnabled(!nearbyScanEnabled_);
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
	if (facts.context)
	{
		transitionTo(OS0InteractionState::INTERACTING);
		return;
	}
	const bool selectedOwnerStillPresent =
		(surface_ == OS0InteractionSurface::ENVIRONMENT && facts.environment) ||
		(surface_ == OS0InteractionSurface::EQUIPMENT && facts.equipment) ||
		(facts.cursorAction && surface_ == facts.cursorSurface);
	if (selectedOwnerStillPresent)
	{
		// Independent windows can coexist. Their callbacks select the most recent
		// owner; do not let an older, still-visible window steal it next frame.
		transitionTo(OS0InteractionState::INTERACTING);
		return;
	}
	if (facts.cursorAction && valid(facts.cursorSurface))
	{
		beginInteraction(facts.cursorSurface);
		return;
	}
	if (facts.environment)
	{
		beginInteraction(OS0InteractionSurface::ENVIRONMENT);
		return;
	}
	if (facts.equipment)
	{
		beginInteraction(OS0InteractionSurface::EQUIPMENT);
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
	nearbyScanEnabled_ = false;
}
