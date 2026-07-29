#include "OS0_WorldInteractionSystem.h"

#include "Items.h"

namespace
{
	constexpr std::array<OS0SectorUpgradeDefinition, 3> UPGRADES{{
		{ "FIELD SHELTER", "CLICK TO RECOVER TEAM", { 8, 4, 0, 4 },
			OS0_SECTOR_UPGRADE_SHELTER },
		{ "SALVAGE WORKSHOP", "+1 SALVAGE YIELD", { 4, 4, 8, 0 },
			OS0_SECTOR_UPGRADE_WORKSHOP },
		{ "SECURE DEPOT", "+1 CONTAINER MATERIAL", { 6, 3, 6, 2 },
			OS0_SECTOR_UPGRADE_DEPOT }
	}};
}

std::array<OS0SectorUpgradeDefinition, 3> const& OS0SectorUpgrades()
{
	return UPGRADES;
}

UINT16 OS0ResourceItem(OS0ResourceKind kind)
{
	switch (kind)
	{
		case OS0ResourceKind::TIMBER: return OS0_TIMBER;
		case OS0ResourceKind::STONE:  return OS0_STONE;
		case OS0ResourceKind::SCRAP:  return OS0_SCRAP;
		case OS0ResourceKind::SOIL:   return OS0_SOIL;
		case OS0ResourceKind::COUNT:  return NOTHING;
	}
	return NOTHING;
}

const char* OS0ResourceName(OS0ResourceKind kind)
{
	switch (kind)
	{
		case OS0ResourceKind::TIMBER: return "TIMBER";
		case OS0ResourceKind::STONE:  return "STONE";
		case OS0ResourceKind::SCRAP:  return "SCRAP";
		case OS0ResourceKind::SOIL:   return "SOIL";
		case OS0ResourceKind::COUNT:  return "MATERIAL";
	}
	return "MATERIAL";
}

BOOLEAN OS0IsResourceItem(UINT16 item)
{
	return item == OS0_TIMBER || item == OS0_STONE ||
		item == OS0_SCRAP || item == OS0_SOIL;
}

OS0ResourceKind OS0ResourceFromItem(UINT16 item)
{
	if (item == OS0_TIMBER) return OS0ResourceKind::TIMBER;
	if (item == OS0_STONE) return OS0ResourceKind::STONE;
	if (item == OS0_SCRAP) return OS0ResourceKind::SCRAP;
	if (item == OS0_SOIL) return OS0ResourceKind::SOIL;
	return OS0ResourceKind::COUNT;
}

BOOLEAN OS0DepositResources(OS0SectorEconomySystem& economy,
	OS0SectorKey sector, OS0ResourceKind kind, UINT16 amount)
{
	if (kind == OS0ResourceKind::COUNT || amount == 0) return FALSE;
	UINT16 const current = economy.resource(sector, kind);
	if (amount > OS0_SECTOR_RESOURCE_MAX - current) return FALSE;
	economy.addResource(sector, kind, amount);
	return TRUE;
}
BOOLEAN OS0CanBuildSectorUpgrade(OS0SectorEconomySystem const& economy,
	OS0SectorKey sector, size_t index)
{
	if (index >= UPGRADES.size()) return FALSE;
	OS0SectorUpgradeDefinition const& upgrade = UPGRADES[index];
	if (economy.hasUpgrade(sector, upgrade.flag)) return FALSE;
	for (size_t resource = 0; resource < upgrade.cost.size(); ++resource)
	{
		if (economy.resource(sector, static_cast<OS0ResourceKind>(resource)) <
			upgrade.cost[resource]) return FALSE;
	}
	return TRUE;
}

BOOLEAN OS0BuildSectorUpgrade(OS0SectorEconomySystem& economy,
	OS0SectorKey sector, size_t index)
{
	if (!OS0CanBuildSectorUpgrade(economy, sector, index)) return FALSE;
	OS0SectorUpgradeDefinition const& upgrade = UPGRADES[index];
	for (size_t resource = 0; resource < upgrade.cost.size(); ++resource)
	{
		OS0ResourceKind const kind = static_cast<OS0ResourceKind>(resource);
		economy.setResource(sector, kind,
			economy.resource(sector, kind) - upgrade.cost[resource]);
	}
	economy.addUpgrade(sector, upgrade.flag);
	return TRUE;
}
