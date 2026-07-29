#pragma once

#include "OS0_WorldTypes.h"

#include <vector>

OS0AssetCatalogRecord* OS0FindAssetCatalogRecord(INT16 tileset,
	UINT16 tileIndex);
OS0AssetCatalogRecord const* OS0FindAssetCatalogRecordConst(INT16 tileset,
	UINT16 tileIndex);
void OS0UpsertAssetCatalogRecord(OS0AssetCatalogRecord record);
ST::string OS0SerializeAssetCatalog();
BOOLEAN OS0WriteAssetCatalog();
void OS0LoadAssetCatalog();

// Exposed for deterministic parser/override tests. Later rows replace an
// existing (tileset, tile) record instead of creating a parallel truth.
void OS0MergeAssetCatalogText(ST::string const& contents,
	std::vector<OS0AssetCatalogRecord>& records);
