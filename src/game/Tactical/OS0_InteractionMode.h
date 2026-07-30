#pragma once

#include <cstdint>

// High-level tactical presentation mode. This controller deliberately owns no
// engine pointers and performs no JA2 mutations; integrations project its value
// into windows, input routing and gameplay commands.
enum class OS0InteractionState : std::uint8_t
{
	NORMAL,
	INTERACTING,
	FIGHT,
	COUNT
};

// The selected interaction surface is independent from the high-level state.
// NORMAL remembers the last selection without presenting it as active.
enum class OS0InteractionSurface : std::uint8_t
{
	ACTIONS,
	BEHAVIOR,
	EQUIPMENT,
	ENVIRONMENT,
	COUNT
};

const char* OS0InteractionStateName(OS0InteractionState state) noexcept;
const char* OS0InteractionSurfaceName(OS0InteractionSurface surface) noexcept;

// Engine/UI facts are reduced in one deterministic order. The tactical layer
// reports facts; it does not reconstruct mode transitions with ad-hoc booleans.
struct OS0InteractionFrameFacts
{
	bool tutorial = false;
	bool fight = false;
	bool context = false;
	bool environment = false;
	bool equipment = false;
	bool cursorAction = false;
	OS0InteractionSurface cursorSurface = OS0InteractionSurface::ACTIONS;
	bool passiveInteraction = false;
};

class OS0InteractionMode
{
public:
	OS0InteractionState state() const noexcept { return state_; }
	OS0InteractionSurface surface() const noexcept { return surface_; }

	bool transitionTo(OS0InteractionState next) noexcept;
	bool beginInteraction(OS0InteractionSurface surface) noexcept;
	bool beginFight(OS0InteractionSurface surface =
		OS0InteractionSurface::ACTIONS) noexcept;
	void returnToNormal() noexcept;
	bool returnToNormal(OS0InteractionSurface selectedSurface) noexcept;

	// Surface selection never changes the state implicitly. In NORMAL it sets
	// the surface that a later interaction/fight transition will open.
	bool selectSurface(OS0InteractionSurface surface) noexcept;

	// Nearby scanning is an explicit user toggle, not a side effect of choosing
	// ENVIRONMENT. FIGHT disables and blocks it; NORMAL and INTERACTING permit it.
	// Mutators return whether the request was accepted. Query
	// nearbyScanEnabled() for the resulting on/off value.
	bool setNearbyScanEnabled(bool enabled) noexcept;
	bool toggleNearbyScan() noexcept;
	// The perception trigger makes ENVIRONMENT the selected surface and enables
	// nearby discovery without inventing a target or entering interaction by
	// itself. The viewport may then open the freshly resolved hovered relation.
	bool beginPerception() noexcept;
	bool nearbyScanEnabled() const noexcept { return nearbyScanEnabled_; }
	bool canScanNearby() const noexcept
	{
		return state_ != OS0InteractionState::FIGHT;
	}

	bool isNormal() const noexcept
	{
		return state_ == OS0InteractionState::NORMAL;
	}
	bool isInteracting() const noexcept
	{
		return state_ == OS0InteractionState::INTERACTING;
	}
	bool isFight() const noexcept
	{
		return state_ == OS0InteractionState::FIGHT;
	}
	bool hasActiveSurface() const noexcept { return !isNormal(); }
	bool isSurfaceSelected(OS0InteractionSurface surface) const noexcept
	{
		return surface_ == surface;
	}
	bool isSurfaceActive(OS0InteractionSurface surface) const noexcept
	{
		return hasActiveSurface() && isSurfaceSelected(surface);
	}
	void synchronize(OS0InteractionFrameFacts const& facts) noexcept;

	void reset() noexcept;

private:
	static bool valid(OS0InteractionState state) noexcept;
	static bool valid(OS0InteractionSurface surface) noexcept;

	OS0InteractionState state_ = OS0InteractionState::NORMAL;
	OS0InteractionSurface surface_ = OS0InteractionSurface::ACTIONS;
	OS0InteractionSurface surfaceBeforeFight_ = OS0InteractionSurface::ACTIONS;
	bool hasSurfaceBeforeFight_ = false;
	bool nearbyScanEnabled_ = false;
};
