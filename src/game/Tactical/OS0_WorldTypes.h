#pragma once

#include "JA2Types.h"

#include <string_theory/string>

enum class OS0ResourceKind : UINT8
{
	TIMBER,
	STONE,
	SCRAP,
	SOIL,
	COUNT
};

enum class OS0AssetCategory : UINT8
{
	UNKNOWN, SANDBAG, DOOR, CONTAINER, STONE, DEBRIS, FURNITURE,
	TREE, WALL, FLOOR, RESOURCE, DECOR, WORKSTATION, COUNT
};

enum class OS0AssetMaterial : UINT8
{
	AUTO, WOOD, STONE, METAL, SAND, EARTH, ORGANIC, FABRIC,
	COMPOSITE, COUNT
};

enum class OS0AssetRole : UINT8
{
	DECOR, SALVAGE, STORAGE, BUILDING_PART, CRAFTING_STATION,
	RESOURCE_NODE, BARRIER, COUNT
};

struct OS0AssetCatalogRecord
{
	INT16 tileset;
	UINT16 tileIndex;
	OS0AssetCategory category;
	OS0AssetMaterial material;
	OS0AssetRole role;
	UINT8 width;
	UINT8 height;
	BOOLEAN buildable;
	ST::string label;
};

struct OS0AssetKey
{
	UINT8 sectorX;
	UINT8 sectorY;
	INT8 sectorZ;
	GridNo gridNo;
	UINT8 level;
	UINT16 tileIndex;

	bool operator==(OS0AssetKey const& rhs) const noexcept
	{
		return sectorX == rhs.sectorX && sectorY == rhs.sectorY &&
			sectorZ == rhs.sectorZ && gridNo == rhs.gridNo &&
			level == rhs.level && tileIndex == rhs.tileIndex;
	}
};
