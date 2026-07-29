#include "OS0_SectorEconomySystem.h"

#include <algorithm>

namespace
{
	constexpr UINT8 LEGACY_RESOURCE_BITS = 5;
	constexpr UINT32 LEGACY_RESOURCE_MASK = (1u << LEGACY_RESOURCE_BITS) - 1u;
	constexpr std::array<UINT8, static_cast<size_t>(OS0ResourceKind::COUNT)>
		LEGACY_RESOURCE_SHIFTS{{ 8, 13, 18, 23 }};
	constexpr UINT32 LEGACY_UPGRADE_SHELTER = 0x10000000u;
	constexpr UINT32 LEGACY_UPGRADE_WORKSHOP = 0x20000000u;
	constexpr UINT32 LEGACY_UPGRADE_DEPOT = 0x40000000u;
}

OS0SectorEconomyRecord& OS0SectorEconomySystem::ensure(OS0SectorKey sector)
{
	auto const found = std::find_if(records_.begin(), records_.end(),
		[&](OS0SectorEconomyRecord const& value) { return value.sector == sector; });
	if (found != records_.end()) return *found;
	records_.push_back({ sector });
	return records_.back();
}

OS0SectorEconomyRecord const* OS0SectorEconomySystem::find(
	OS0SectorKey sector) const
{
	auto const found = std::find_if(records_.begin(), records_.end(),
		[&](OS0SectorEconomyRecord const& value) { return value.sector == sector; });
	return found == records_.end() ? nullptr : &*found;
}

UINT16 OS0SectorEconomySystem::resource(OS0SectorKey sector,
	OS0ResourceKind kind) const
{
	OS0SectorEconomyRecord const* const record = find(sector);
	size_t const index = static_cast<size_t>(kind);
	return record && index < record->resources.size() ? record->resources[index] : 0;
}

void OS0SectorEconomySystem::setResource(OS0SectorKey sector,
	OS0ResourceKind kind, UINT16 value)
{
	size_t const index = static_cast<size_t>(kind);
	if (index >= static_cast<size_t>(OS0ResourceKind::COUNT)) return;
	ensure(sector).resources[index] = std::min<UINT16>(value,
		OS0_SECTOR_RESOURCE_MAX);
}

void OS0SectorEconomySystem::addResource(OS0SectorKey sector,
	OS0ResourceKind kind, UINT16 amount)
{
	setResource(sector, kind, static_cast<UINT16>(std::min<UINT32>(
		OS0_SECTOR_RESOURCE_MAX, resource(sector, kind) + amount)));
}

BOOLEAN OS0SectorEconomySystem::hasUpgrade(OS0SectorKey sector,
	UINT32 upgrade) const
{
	OS0SectorEconomyRecord const* const record = find(sector);
	return record && (record->upgrades & upgrade) != 0;
}

void OS0SectorEconomySystem::addUpgrade(OS0SectorKey sector, UINT32 upgrade)
{
	ensure(sector).upgrades |= upgrade;
}

void OS0SectorEconomySystem::migrateLegacy(OS0SectorKey sector,
	UINT32 legacyFacilities)
{
	OS0SectorEconomyRecord& record = ensure(sector);
	if (record.legacyMigrated) return;
	for (size_t i = 0; i < record.resources.size(); ++i)
	{
		record.resources[i] = static_cast<UINT16>(
			(legacyFacilities >> LEGACY_RESOURCE_SHIFTS[i]) &
			LEGACY_RESOURCE_MASK);
	}
	if (legacyFacilities & LEGACY_UPGRADE_SHELTER)
		record.upgrades |= OS0_SECTOR_UPGRADE_SHELTER;
	if (legacyFacilities & LEGACY_UPGRADE_WORKSHOP)
		record.upgrades |= OS0_SECTOR_UPGRADE_WORKSHOP;
	if (legacyFacilities & LEGACY_UPGRADE_DEPOT)
		record.upgrades |= OS0_SECTOR_UPGRADE_DEPOT;
	record.legacyMigrated = TRUE;
}

BOOLEAN OS0SectorEconomySystem::legacyMigrated(OS0SectorKey sector) const
{
	OS0SectorEconomyRecord const* const record = find(sector);
	return record && record->legacyMigrated;
}

std::vector<OS0SectorEconomyRecord> const&
OS0SectorEconomySystem::records() const noexcept
{
	return records_;
}

void OS0SectorEconomySystem::replaceRecords(
	std::vector<OS0SectorEconomyRecord> records)
{
	records_ = std::move(records);
}

void OS0SectorEconomySystem::clear()
{
	records_.clear();
}

UINT32 OS0LegacySectorEconomyMask()
{
	UINT32 mask = LEGACY_UPGRADE_SHELTER | LEGACY_UPGRADE_WORKSHOP |
		LEGACY_UPGRADE_DEPOT;
	for (UINT8 shift : LEGACY_RESOURCE_SHIFTS)
		mask |= LEGACY_RESOURCE_MASK << shift;
	return mask;
}
