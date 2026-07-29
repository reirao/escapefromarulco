#pragma once

#include "OS0_WorldTypes.h"

#include <array>
#include <vector>

constexpr UINT32 OS0_SECTOR_UPGRADE_SHELTER = 0x00000001u;
constexpr UINT32 OS0_SECTOR_UPGRADE_WORKSHOP = 0x00000002u;
constexpr UINT32 OS0_SECTOR_UPGRADE_DEPOT = 0x00000004u;
constexpr UINT16 OS0_SECTOR_RESOURCE_MAX = 9999;

struct OS0SectorKey
{
	UINT8 x;
	UINT8 y;
	INT8 z;

	bool operator==(OS0SectorKey const& rhs) const noexcept
	{
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}
};

struct OS0SectorEconomyRecord
{
	OS0SectorKey sector;
	std::array<UINT16, static_cast<size_t>(OS0ResourceKind::COUNT)> resources{};
	UINT32 upgrades = 0;
	BOOLEAN legacyMigrated = FALSE;
};

class OS0SectorEconomySystem
{
public:
	UINT16 resource(OS0SectorKey sector, OS0ResourceKind kind) const;
	void setResource(OS0SectorKey sector, OS0ResourceKind kind, UINT16 value);
	void addResource(OS0SectorKey sector, OS0ResourceKind kind, UINT16 amount);
	BOOLEAN hasUpgrade(OS0SectorKey sector, UINT32 upgrade) const;
	void addUpgrade(OS0SectorKey sector, UINT32 upgrade);

	// Imports the alpha-build bit layout once. The caller may then clear only
	// those legacy bits from SECTORINFO without touching native facility flags.
	void migrateLegacy(OS0SectorKey sector, UINT32 legacyFacilities);
	BOOLEAN legacyMigrated(OS0SectorKey sector) const;

	std::vector<OS0SectorEconomyRecord> const& records() const noexcept;
	void replaceRecords(std::vector<OS0SectorEconomyRecord> records);
	void clear();

private:
	OS0SectorEconomyRecord& ensure(OS0SectorKey sector);
	OS0SectorEconomyRecord const* find(OS0SectorKey sector) const;
	std::vector<OS0SectorEconomyRecord> records_;
};

UINT32 OS0LegacySectorEconomyMask();
