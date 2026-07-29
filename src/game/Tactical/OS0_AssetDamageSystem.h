#pragma once

#include "OS0_WorldTypes.h"

#include <vector>

struct STRUCTURE;

struct OS0AssetDamageRecord
{
	OS0AssetKey key;
	INT16 remaining;
	INT16 maximum;
};

struct OS0AssetDamageResult
{
	BOOLEAN handled;
	BOOLEAN destroyed;
	INT16 remaining;
	INT16 maximum;
};

class OS0AssetDamageSystem
{
public:
	INT16 durability(OS0AssetKey const& key, INT16 maximum) const;
	OS0AssetDamageResult apply(OS0AssetKey const& key, INT16 maximum,
		INT16 damage);
	void remove(OS0AssetKey const& key);
	void move(OS0AssetKey const& source, OS0AssetKey const& destination);
	void clear();

	std::vector<OS0AssetDamageRecord> const& records() const noexcept;
	void replaceRecords(std::vector<OS0AssetDamageRecord> records);

private:
	std::vector<OS0AssetDamageRecord> records_;
};

INT16 OS0AssetDurabilityMaximum(OS0AssetMaterial material);
INT16 OS0CurrentAssetDurability(GridNo gridNo, UINT8 level,
	UINT16 tileIndex, OS0AssetMaterial material);

// Neutral simulation entry point. Weapons and world code depend on this module,
// never on OS0_IngameUI.
BOOLEAN OS0ApplyWorldAssetDamage(GridNo gridNo, UINT8 level,
	STRUCTURE* structure, UINT8 impact);

BOOLEAN OS0ValidateResourceItemDefinitions(ST::string* error = nullptr);
