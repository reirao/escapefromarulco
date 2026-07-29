#pragma once

#include "OS0_SectorEconomySystem.h"

#include <array>

struct OS0SectorUpgradeDefinition
{
	const char* name;
	const char* benefit;
	std::array<UINT16, static_cast<size_t>(OS0ResourceKind::COUNT)> cost;
	UINT32 flag;
};

std::array<OS0SectorUpgradeDefinition, 3> const& OS0SectorUpgrades();

UINT16 OS0ResourceItem(OS0ResourceKind kind);
const char* OS0ResourceName(OS0ResourceKind kind);
BOOLEAN OS0IsResourceItem(UINT16 item);
OS0ResourceKind OS0ResourceFromItem(UINT16 item);

// Authoritative resource transaction. It never deletes or moves an engine
// object; callers mutate the real JA2 item only after this transaction commits.
BOOLEAN OS0DepositResources(OS0SectorEconomySystem& economy,
	OS0SectorKey sector, OS0ResourceKind kind, UINT16 amount);

BOOLEAN OS0CanBuildSectorUpgrade(OS0SectorEconomySystem const& economy,
	OS0SectorKey sector, size_t index);
BOOLEAN OS0BuildSectorUpgrade(OS0SectorEconomySystem& economy,
	OS0SectorKey sector, size_t index);
