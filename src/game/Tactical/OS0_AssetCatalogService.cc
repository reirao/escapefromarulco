#include "OS0_AssetCatalogService.h"

#include "ContentManager.h"
#include "GameInstance.h"
#include "OS0_TacticalSession.h"
#include "SGPFile.h"
#include "TileDef.h"

#include <algorithm>
#include <sstream>
#include <string_theory/format>

namespace
{
	constexpr const char* USER_CATALOG_PATH = "AssetCatalog/os0-assets.tsv";
	constexpr const char* BUILTIN_CATALOG_PATH = "os0-assets.tsv";

	std::vector<OS0AssetCatalogRecord>& Records()
	{
		return OS0GetTacticalSession().state().assetCatalog;
	}
}

OS0AssetCatalogRecord* OS0FindAssetCatalogRecord(INT16 tileset,
	UINT16 tileIndex)
{
	auto& records = Records();
	auto const found = std::find_if(records.begin(), records.end(),
		[&](OS0AssetCatalogRecord const& value)
		{
			return value.tileset == tileset && value.tileIndex == tileIndex;
		});
	return found == records.end() ? nullptr : &*found;
}

OS0AssetCatalogRecord const* OS0FindAssetCatalogRecordConst(INT16 tileset,
	UINT16 tileIndex)
{
	return OS0FindAssetCatalogRecord(tileset, tileIndex);
}

void OS0UpsertAssetCatalogRecord(OS0AssetCatalogRecord record)
{
	if (OS0AssetCatalogRecord* const existing = OS0FindAssetCatalogRecord(
		record.tileset, record.tileIndex)) *existing = std::move(record);
	else Records().push_back(std::move(record));
}

void OS0MergeAssetCatalogText(ST::string const& contents,
	std::vector<OS0AssetCatalogRecord>& records)
{
	std::istringstream stream(contents.c_str());
	std::string line;
	while (std::getline(stream, line))
	{
		if (line.empty() || line[0] == '#') continue;
		std::istringstream row(line);
		int tileset, tile, category, material, role, width, height, buildable;
		if (!(row >> tileset >> tile >> category >> material >> role >>
			width >> height >> buildable)) continue;
		std::string label;
		std::getline(row, label);
		size_t const first = label.find_first_not_of(" \t");
		if (first != std::string::npos) label.erase(0, first);
		else label = "UNNAMED ASSET";
		if (tile < 0 || tile >= NUMBEROFTILES ||
			category < 0 || category >= static_cast<int>(OS0AssetCategory::COUNT) ||
			material < 0 || material >= static_cast<int>(OS0AssetMaterial::COUNT) ||
			role < 0 || role >= static_cast<int>(OS0AssetRole::COUNT)) continue;
		OS0AssetCatalogRecord parsed{ static_cast<INT16>(tileset),
			static_cast<UINT16>(tile), static_cast<OS0AssetCategory>(category),
			static_cast<OS0AssetMaterial>(material), static_cast<OS0AssetRole>(role),
			static_cast<UINT8>(std::clamp(width, 1, 12)),
			static_cast<UINT8>(std::clamp(height, 1, 12)),
			buildable != 0, ST::string(label) };
		auto const existing = std::find_if(records.begin(), records.end(),
			[&](OS0AssetCatalogRecord const& value)
			{
				return value.tileset == parsed.tileset &&
					value.tileIndex == parsed.tileIndex;
			});
		if (existing == records.end()) records.push_back(std::move(parsed));
		else *existing = std::move(parsed);
	}
}

ST::string OS0SerializeAssetCatalog()
{
	ST::string output =
		"# Escape from Arulco asset catalog v1\n"
		"# tileset tile category material role width height buildable label\n";
	for (OS0AssetCatalogRecord const& record : Records())
	{
		output += ST::format("{} {} {} {} {} {} {} {}\t{}\n",
			record.tileset, record.tileIndex,
			static_cast<UINT8>(record.category),
			static_cast<UINT8>(record.material),
			static_cast<UINT8>(record.role), record.width, record.height,
			record.buildable ? 1 : 0, record.label);
	}
	return output;
}

BOOLEAN OS0WriteAssetCatalog()
{
	try
	{
		GCM->userPrivateFiles()->createDir("AssetCatalog");
		ST::string const output = OS0SerializeAssetCatalog();
		AutoSGPFile file{ GCM->userPrivateFiles()->openForWriting(
			USER_CATALOG_PATH, true) };
		file->write(output.c_str(), output.size());
		return TRUE;
	}
	catch (...) { return FALSE; }
}

void OS0LoadAssetCatalog()
{
	auto& records = Records();
	records.clear();
	try
	{
		if (GCM->doesGameResExists(BUILTIN_CATALOG_PATH))
		{
			AutoSGPFile builtIn{ GCM->openGameResForReading(BUILTIN_CATALOG_PATH) };
			OS0MergeAssetCatalogText(builtIn->readStringToEnd(), records);
		}
	}
	catch (...)
	{
		OS0GetTacticalSession().state().pendingDiagnostics.push_back(
			"BUILT-IN ASSET CATALOG LOAD FAILED");
	}
	try
	{
		AutoSGPFile user{ GCM->userPrivateFiles()->openForReading(USER_CATALOG_PATH) };
		OS0MergeAssetCatalogText(user->readStringToEnd(), records);
	}
	catch (...) {}
}
