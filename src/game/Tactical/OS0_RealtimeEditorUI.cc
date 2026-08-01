#include "OS0_RealtimeEditorUI.h"

#include "Cursors.h"
#include "ContentManager.h"
#include "Font.h"
#include "Font_Control.h"
#include "GameInstance.h"
#include "Game_Clock.h"
#include "HImage.h"
#include "Handle_Items.h"
#include "Interface_Items.h"
#include "ItemModel.h"
#include "Local.h"
#include "MouseSystem.h"
#include "OS0_RealtimeEditor.h"
#include "OS0_UIAssetManager.h"
#include "OS0_WindowManager.h"
#include "Overhead.h"
#include "Render_Dirty.h"
#include "RenderWorld.h"
#include "TileDat.h"
#include "TileDef.h"
#include "VObject.h"
#include "VSurface.h"
#include "Video.h"
#include "WorldMan.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string_theory/format>
#include <vector>

namespace
{
	constexpr INT16 HEADER_H = 20;
	constexpr INT16 TAB_H = 19;
	constexpr INT16 FILTER_H = 19;
	constexpr INT16 TOOL_H = 21;
	constexpr INT16 BRUSH_H = 21;
	constexpr INT16 COMMAND_H = 21;
	constexpr INT16 FOOTER_H = 37;
	constexpr INT16 GAP = 3;
	constexpr INT16 MARGIN = 5;
	constexpr size_t ENTRIES_PER_PAGE = 6;
	constexpr size_t ENTRY_COLUMNS = 2;
	constexpr size_t ENTRY_ROWS = ENTRIES_PER_PAGE / ENTRY_COLUMNS;
	constexpr UINT32 CONFIRM_TIMEOUT_MS = 5000;

	constexpr std::array<const char*,
		static_cast<size_t>(OS0RealtimeEditorPalette::COUNT)> PALETTE_NAMES{
		"TILES", "ITEMS", "NPCS", "SYSTEM"
	};

	constexpr std::array<const char*,
		static_cast<size_t>(OS0RealtimeEditorTool::COUNT)> TOOL_NAMES{
		"SELECT", "PLACE", "ERASE"
	};

	constexpr std::array<OS0UIIcon,
		static_cast<size_t>(OS0RealtimeEditorTool::COUNT)> TOOL_ICONS{
		OS0UIIcon::LOOK, OS0UIIcon::HAND, OS0UIIcon::CANCEL
	};

	constexpr std::array<const char*, 8> LAYER_NAMES{
		"AUTO", "LAND", "OBJECT", "STRUCTURE", "SHADOW", "ROOF",
		"ON ROOF", "TOPMOST"
	};

	enum class RegionRole : UINT8
	{
		PALETTE,
		CATEGORY_PREVIOUS,
		CATEGORY_CLEAR,
		CATEGORY_NEXT,
		TOOL,
		LAYER_PREVIOUS,
		LAYER_RESET,
		LAYER_NEXT,
		VARIANT_PREVIOUS,
		VARIANT_NEXT,
		REPLACE,
		EMPTY_MAP,
		SAVE,
		ENTRY,
		PREVIOUS,
		NEXT
	};

	UINT16 Colour(UINT8 red, UINT8 green, UINT8 blue)
	{
		return Get16BPPColor(FROMRGB(red, green, blue));
	}

	void Fill(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour)
	{
		if (w <= 0 || h <= 0) return;
		ColorFillVideoSurfaceArea(FRAME_BUFFER, x, y, x + w - 1, y + h - 1,
			colour);
	}

	void LineBox(INT16 x, INT16 y, INT16 w, INT16 h, UINT16 colour)
	{
		if (w <= 0 || h <= 0) return;
		Fill(x, y, w, 1, colour);
		Fill(x, y + h - 1, w, 1, colour);
		Fill(x, y, 1, h, colour);
		Fill(x + w - 1, y, 1, h, colour);
	}

	void Print(INT16 x, INT16 y, ST::string const& text, UINT8 colour)
	{
		SetFont(TINYFONT1);
		SetFontBackground(FONT_MCOLOR_BLACK);
		SetFontForeground(colour);
		MPrint(x, y, text);
	}

	OS0UIIcon ActionIcon(OS0EditorAction const action)
	{
		switch (action)
		{
			case OS0EditorAction::NEW_BLANK_MAP: return OS0UIIcon::EXPLOSIVE;
			case OS0EditorAction::LOAD_MAP: return OS0UIIcon::EXAMINE;
			case OS0EditorAction::PLACE_TILE: return OS0UIIcon::HAND;
			case OS0EditorAction::PLACE_ITEM: return OS0UIIcon::HAND;
			case OS0EditorAction::PLACE_NPC: return OS0UIIcon::TALK;
			case OS0EditorAction::REMOVE: return OS0UIIcon::CANCEL;
			case OS0EditorAction::SAVE_MAP: return OS0UIIcon::TOOLKIT;
			case OS0EditorAction::REBUILD_CATALOGS: return OS0UIIcon::EXAMINE;
			case OS0EditorAction::COUNT: break;
		}
		return OS0UIIcon::LOOK;
	}
}

struct OS0RealtimeEditorUI::Impl
{
	struct Binding
	{
		MOUSE_REGION region{};
		Impl* owner = nullptr;
		RegionRole role = RegionRole::PALETTE;
		size_t index = 0;
		ST::string help;
		BOOLEAN defined = FALSE;
	};

	static constexpr size_t PALETTE_COUNT =
		static_cast<size_t>(OS0RealtimeEditorPalette::COUNT);
	static constexpr size_t PALETTE_REGION = 0;
	static constexpr size_t CATEGORY_PREVIOUS_REGION =
		PALETTE_REGION + PALETTE_COUNT;
	static constexpr size_t CATEGORY_CLEAR_REGION =
		CATEGORY_PREVIOUS_REGION + 1;
	static constexpr size_t CATEGORY_NEXT_REGION = CATEGORY_CLEAR_REGION + 1;
	static constexpr size_t TOOL_REGION = CATEGORY_NEXT_REGION + 1;
	static constexpr size_t LAYER_PREVIOUS_REGION =
		TOOL_REGION + static_cast<size_t>(OS0RealtimeEditorTool::COUNT);
	static constexpr size_t LAYER_RESET_REGION = LAYER_PREVIOUS_REGION + 1;
	static constexpr size_t LAYER_NEXT_REGION = LAYER_RESET_REGION + 1;
	static constexpr size_t VARIANT_PREVIOUS_REGION = LAYER_NEXT_REGION + 1;
	static constexpr size_t VARIANT_NEXT_REGION = VARIANT_PREVIOUS_REGION + 1;
	static constexpr size_t REPLACE_REGION = VARIANT_NEXT_REGION + 1;
	static constexpr size_t EMPTY_REGION =
		REPLACE_REGION + 1;
	static constexpr size_t SAVE_REGION = EMPTY_REGION + 1;
	static constexpr size_t ENTRY_REGION = SAVE_REGION + 1;
	static constexpr size_t PREVIOUS_REGION = ENTRY_REGION + ENTRIES_PER_PAGE;
	static constexpr size_t NEXT_REGION = PREVIOUS_REGION + 1;
	static constexpr size_t REGION_COUNT = NEXT_REGION + 1;

	OS0WindowManager* manager = nullptr;
	OS0WindowHandle window = OS0_INVALID_WINDOW;
	std::array<Binding, REGION_COUNT> regions{};
	std::array<MOUSE_REGION*, REGION_COUNT> zOrderedRegions{};
	SGPVSurface* preview = nullptr;
	OS0RealtimeEditorPalette palette = OS0RealtimeEditorPalette::TILES;
	OS0RealtimeEditorTool tool = OS0RealtimeEditorTool::SELECT;
	std::array<size_t, PALETTE_COUNT> selected{};
	std::array<size_t, PALETTE_COUNT> pages{};
	std::array<OS0EditorCategory, PALETTE_COUNT> categoryFilters{
		OS0EditorCategory::COUNT, OS0EditorCategory::COUNT,
		OS0EditorCategory::COUNT, OS0EditorCategory::COUNT
	};
	std::array<std::vector<OS0EditorCategory>, PALETTE_COUNT> categoryOptions;
	std::array<std::vector<size_t>, PALETTE_COUNT> filteredEntries;
	OS0EditorLayer brushLayer = OS0EditorLayer::AUTO;
	UINT8 brushRadius = 0;
	UINT8 roadMacroId = 0;
	BOOLEAN smoothTerrain = FALSE;
	INT8 npcDirection = 0;
	UINT8 itemQuantity = 1;
	BOOLEAN replaceExisting = FALSE;
	Binding const* hovered = nullptr;
	ST::string hoverHelp;
	std::uint64_t catalogGeneration = 0;
	UINT32 blankConfirmStarted = 0;
	BOOLEAN blankConfirmArmed = FALSE;
	BOOLEAN inputEnabled = TRUE;
	BOOLEAN regionsActive = FALSE;
	ST::string localStatus = "SELECT A CATALOG ENTRY";

	BOOLEAN active() const noexcept
	{
		return manager != nullptr && window != OS0_INVALID_WINDOW &&
			manager->visible(window);
	}

	size_t paletteIndex() const noexcept
	{
		return static_cast<size_t>(palette);
	}

	OS0EditorCatalog const& catalog() const noexcept
	{
		return OS0GetRealtimeEditor().catalog();
	}

	OS0EditorCategory selectedTileCategory() const noexcept
	{
		if (palette != OS0RealtimeEditorPalette::TILES ||
			selected[paletteIndex()] >= catalog().tiles.size())
		{
			return OS0EditorCategory::COUNT;
		}
		return catalog().tiles[selected[paletteIndex()]].category;
	}

	BOOLEAN selectedTileUsesTerrainBrush() const noexcept
	{
		auto const category = selectedTileCategory();
		return category == OS0EditorCategory::TERRAIN ||
			category == OS0EditorCategory::WATER;
	}

	BOOLEAN selectedTileCanSmoothTerrain() const noexcept
	{
		if (!selectedTileUsesTerrainBrush()) return FALSE;
		UINT16 const type = catalog().tiles[selected[paletteIndex()]].tileType;
		return (FIRSTTEXTURE <= type && type <= SEVENTHTEXTURE) ||
			type == REGWATERTEXTURE;
	}

	BOOLEAN selectedTileUsesRoadMacro() const noexcept
	{
		return selectedTileCategory() == OS0EditorCategory::ROADS;
	}

	BOOLEAN selectedTileUsesNativeRecipe() const noexcept
	{
		return selectedTileUsesTerrainBrush() || selectedTileUsesRoadMacro();
	}

	void normalizeNativeRecipeState() noexcept
	{
		if (smoothTerrain && selectedTileUsesTerrainBrush() &&
			!selectedTileCanSmoothTerrain())
		{
			smoothTerrain = FALSE;
		}
	}

	size_t rawEntryCount(OS0RealtimeEditorPalette const value) const noexcept
	{
		switch (value)
		{
			case OS0RealtimeEditorPalette::TILES: return catalog().tiles.size();
			case OS0RealtimeEditorPalette::ITEMS: return catalog().items.size();
			case OS0RealtimeEditorPalette::NPCS:
				return catalog().npcTemplates.size();
			case OS0RealtimeEditorPalette::SYSTEM:
				return catalog().actions.size();
			case OS0RealtimeEditorPalette::COUNT: break;
		}
		return 0;
	}

	OS0EditorCategory entryCategory(OS0RealtimeEditorPalette const value,
		size_t const absolute) const noexcept
	{
		switch (value)
		{
			case OS0RealtimeEditorPalette::TILES:
				if (absolute < catalog().tiles.size())
					return catalog().tiles[absolute].category;
				break;
			case OS0RealtimeEditorPalette::ITEMS:
				if (absolute < catalog().items.size())
					return catalog().items[absolute].category;
				break;
			case OS0RealtimeEditorPalette::NPCS:
				return OS0EditorCategory::NPCS;
			case OS0RealtimeEditorPalette::SYSTEM:
				if (absolute < catalog().actions.size())
					return catalog().actions[absolute].category;
				break;
			case OS0RealtimeEditorPalette::COUNT: break;
		}
		return OS0EditorCategory::COUNT;
	}

	ST::string categoryLabel(OS0EditorCategory const category) const
	{
		if (category == OS0EditorCategory::COUNT) return "ALL CATEGORIES";
		auto const found = std::find_if(catalog().categories.begin(),
			catalog().categories.end(),
			[category](OS0EditorCategoryRecord const& entry)
			{ return entry.id == category; });
		return found != catalog().categories.end() ? found->label : "CATEGORY";
	}

	void rebuildFilterCache(OS0RealtimeEditorPalette const value)
	{
		if (value >= OS0RealtimeEditorPalette::COUNT) return;
		const size_t p = static_cast<size_t>(value);
		auto& options = categoryOptions[p];
		auto& entries = filteredEntries[p];
		options.clear();
		entries.clear();

		for (OS0EditorCategoryRecord const& category : catalog().categories)
		{
			for (size_t i = 0; i < rawEntryCount(value); ++i)
			{
				if (entryCategory(value, i) == category.id)
				{
					options.push_back(category.id);
					break;
				}
			}
		}

		// Catalog extensions may introduce a category before adding its display
		// record. Keep those entries accessible and stable until the next rebuild.
		for (size_t i = 0; i < rawEntryCount(value); ++i)
		{
			OS0EditorCategory const category = entryCategory(value, i);
			if (category != OS0EditorCategory::COUNT &&
				std::find(options.begin(), options.end(), category) == options.end())
			{
				options.push_back(category);
			}
		}

		if (categoryFilters[p] != OS0EditorCategory::COUNT &&
			std::find(options.begin(), options.end(), categoryFilters[p]) ==
				options.end())
		{
			categoryFilters[p] = OS0EditorCategory::COUNT;
		}
		for (size_t i = 0; i < rawEntryCount(value); ++i)
		{
			if (categoryFilters[p] == OS0EditorCategory::COUNT ||
				entryCategory(value, i) == categoryFilters[p])
			{
				entries.push_back(i);
			}
		}

		if (entries.empty())
		{
			selected[p] = 0;
			pages[p] = 0;
			return;
		}
		auto selectedPosition = std::find(entries.begin(), entries.end(),
			selected[p]);
		if (selectedPosition == entries.end())
		{
			selected[p] = entries.front();
			selectedPosition = entries.begin();
		}
		pages[p] = std::min(pages[p],
			(entries.size() + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE - 1);
	}

	void rebuildFilterCaches()
	{
		for (size_t i = 0; i < PALETTE_COUNT; ++i)
		{
			rebuildFilterCache(static_cast<OS0RealtimeEditorPalette>(i));
		}
	}

	size_t entryCount() const noexcept
	{
		return filteredEntries[paletteIndex()].size();
	}

	size_t absoluteEntry(size_t const filteredIndex) const noexcept
	{
		auto const& entries = filteredEntries[paletteIndex()];
		return filteredIndex < entries.size() ? entries[filteredIndex] :
			std::numeric_limits<size_t>::max();
	}

	size_t pageCount() const noexcept
	{
		return std::max<size_t>(1,
			(entryCount() + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE);
	}

	void clampPresentationState() noexcept
	{
		const size_t p = paletteIndex();
		pages[p] = std::min(pages[p], pageCount() - 1);
		auto const& entries = filteredEntries[p];
		if (entries.empty()) selected[p] = 0;
		else if (std::find(entries.begin(), entries.end(), selected[p]) ==
			entries.end()) selected[p] = entries.front();
	}

	void cancelBlankConfirmation() noexcept
	{
		blankConfirmArmed = FALSE;
		blankConfirmStarted = 0;
	}

	void changeCategory(INT16 const direction)
	{
		const size_t p = paletteIndex();
		auto const& options = categoryOptions[p];
		const size_t count = options.size() + 1; // ALL plus concrete categories
		if (count <= 1) return;
		size_t position = 0;
		if (categoryFilters[p] != OS0EditorCategory::COUNT)
		{
			auto const found = std::find(options.begin(), options.end(),
				categoryFilters[p]);
			if (found != options.end())
				position = static_cast<size_t>(std::distance(options.begin(), found)) + 1;
		}
		if (direction < 0) position = position == 0 ? count - 1 : position - 1;
		else position = (position + 1) % count;
		categoryFilters[p] = position == 0 ? OS0EditorCategory::COUNT :
			options[position - 1];
		pages[p] = 0;
		rebuildFilterCache(palette);
		normalizeNativeRecipeState();
		localStatus = ST::format("FILTER {} / {} ENTRIES",
			categoryLabel(categoryFilters[p]), entryCount());
		cancelBlankConfirmation();
	}

	void clearCategory()
	{
		categoryFilters[paletteIndex()] = OS0EditorCategory::COUNT;
		pages[paletteIndex()] = 0;
		rebuildFilterCache(palette);
		auto const selectedPosition = std::find(
			filteredEntries[paletteIndex()].begin(),
			filteredEntries[paletteIndex()].end(), selected[paletteIndex()]);
		if (selectedPosition != filteredEntries[paletteIndex()].end())
		{
			pages[paletteIndex()] = static_cast<size_t>(std::distance(
				filteredEntries[paletteIndex()].begin(), selectedPosition)) /
				ENTRIES_PER_PAGE;
		}
		localStatus = ST::format("ALL CATEGORIES / {} ENTRIES", entryCount());
		cancelBlankConfirmation();
	}

	void changeLayer(INT16 const direction)
	{
		constexpr INT16 count = static_cast<INT16>(LAYER_NAMES.size());
		INT16 layer = static_cast<INT16>(brushLayer);
		layer = direction < 0 ? (layer + count - 1) % count :
			(layer + 1) % count;
		brushLayer = static_cast<OS0EditorLayer>(layer);
		localStatus = ST::format("BRUSH LAYER {}",
			LAYER_NAMES[static_cast<size_t>(brushLayer)]);
		cancelBlankConfirmation();
	}

	void resetLayer()
	{
		brushLayer = OS0EditorLayer::AUTO;
		localStatus = "BRUSH LAYER AUTO / ASSET DEFAULT";
		cancelBlankConfirmation();
	}

	void changeVariant(INT16 const direction)
	{
		const size_t p = paletteIndex();
		if (palette == OS0RealtimeEditorPalette::TILES &&
			selected[p] < catalog().tiles.size())
		{
			if (selectedTileUsesTerrainBrush())
			{
				if (direction < 0 && brushRadius > 0) --brushRadius;
				else if (direction > 0 && brushRadius < 8) ++brushRadius;
				localStatus = ST::format("TERRAIN BRUSH RADIUS {} / {}",
					brushRadius, smoothTerrain ? "SMOOTH" : "PAINT");
				cancelBlankConfirmation();
				return;
			}
			if (selectedTileUsesRoadMacro())
			{
				constexpr UINT8 ROAD_MACRO_COUNT = 32;
				roadMacroId = direction < 0 ?
					static_cast<UINT8>((roadMacroId + ROAD_MACRO_COUNT - 1) %
						ROAD_MACRO_COUNT) :
					static_cast<UINT8>((roadMacroId + 1) % ROAD_MACRO_COUNT);
				localStatus = ST::format("ROAD MACRO {} / 31", roadMacroId);
				cancelBlankConfirmation();
				return;
			}
			UINT16 const tileType = catalog().tiles[selected[p]].tileType;
			std::vector<size_t> variants;
			for (size_t const absolute : filteredEntries[p])
			{
				if (catalog().tiles[absolute].tileType == tileType)
					variants.push_back(absolute);
			}
			if (variants.empty()) return;
			auto found = std::find(variants.begin(), variants.end(), selected[p]);
			size_t position = found == variants.end() ? 0 :
				static_cast<size_t>(std::distance(variants.begin(), found));
			position = direction < 0 ?
				(position == 0 ? variants.size() - 1 : position - 1) :
				(position + 1) % variants.size();
			selected[p] = variants[position];
			auto const visible = std::find(filteredEntries[p].begin(),
				filteredEntries[p].end(), selected[p]);
			if (visible != filteredEntries[p].end())
				pages[p] = static_cast<size_t>(std::distance(
					filteredEntries[p].begin(), visible)) / ENTRIES_PER_PAGE;
			auto const& entry = catalog().tiles[selected[p]];
			localStatus = ST::format("VARIANT {} / ORIENTATION {}",
				entry.regionIndex, entry.wallOrientation);
		}
		else if (palette == OS0RealtimeEditorPalette::NPCS)
		{
			npcDirection = static_cast<INT8>(direction < 0 ?
				(npcDirection + 7) % 8 : (npcDirection + 1) % 8);
			localStatus = ST::format("NPC DIRECTION {} / 8", npcDirection);
		}
		else if (palette == OS0RealtimeEditorPalette::ITEMS)
		{
			if (direction < 0 && itemQuantity > 1) --itemQuantity;
			else if (direction > 0 && itemQuantity < 20) ++itemQuantity;
			localStatus = ST::format("ITEM BRUSH QUANTITY {}", itemQuantity);
		}
		else
		{
			localStatus = "NO VARIANTS FOR THIS PALETTE";
		}
		cancelBlankConfirmation();
	}

	void toggleReplace()
	{
		if (selectedTileUsesTerrainBrush())
		{
			if (!selectedTileCanSmoothTerrain())
			{
				smoothTerrain = FALSE;
				localStatus = "THIS WATER TEXTURE SUPPORTS PAINT ONLY";
				cancelBlankConfirmation();
				return;
			}
			smoothTerrain = !smoothTerrain;
			localStatus = ST::format("TERRAIN {} / RADIUS {}",
				smoothTerrain ? "SMOOTH" : "PAINT", brushRadius);
			cancelBlankConfirmation();
			return;
		}
		if (selectedTileUsesRoadMacro())
		{
			roadMacroId = 0;
			localStatus = "ROAD MACRO RESET TO 0";
			cancelBlankConfirmation();
			return;
		}
		replaceExisting = !replaceExisting;
		localStatus = replaceExisting ?
			"REPLACE MODE / SAME LAYER IS REPLACED" :
			"ADDITIVE MODE / KEEP EXISTING ASSETS";
		cancelBlankConfirmation();
	}

	void setPalette(OS0RealtimeEditorPalette const value)
	{
		if (value >= OS0RealtimeEditorPalette::COUNT) return;
		palette = value;
		cancelBlankConfirmation();
		clampPresentationState();
		normalizeNativeRecipeState();
		localStatus = ST::format("{} CATALOG / {} ENTRIES",
			PALETTE_NAMES[paletteIndex()], entryCount());
	}

	void setTool(OS0RealtimeEditorTool const value)
	{
		if (value >= OS0RealtimeEditorTool::COUNT) return;
		tool = value;
		cancelBlankConfirmation();
		localStatus = ST::format("{} TOOL ACTIVE",
			TOOL_NAMES[static_cast<size_t>(tool)]);
	}

	void define(Binding& binding, RegionRole const role, size_t const index)
	{
		binding.owner = this;
		binding.role = role;
		binding.index = index;
		MSYS_DefineRegion(&binding.region, 0, 0, 1, 1,
			MSYS_PRIORITY_HIGHEST,
			MSYS_NO_CURSOR,
			[](MOUSE_REGION* region, UINT32 reason)
			{
				Binding* const b = region->GetUserPtr<Binding>();
				if (b && b->owner) b->owner->onRegionEvent(*b, reason);
			},
			[](MOUSE_REGION* region, UINT32 reason)
			{
				Binding* const b = region->GetUserPtr<Binding>();
				if (b && b->owner) b->owner->onRegionEvent(*b, reason);
			});
		binding.region.SetUserPtr(&binding);
		binding.region.Disable();
		binding.defined = TRUE;
	}

	void initializeRegions()
	{
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorPalette::COUNT); ++i)
		{
			define(regions[PALETTE_REGION + i], RegionRole::PALETTE, i);
		}
		define(regions[CATEGORY_PREVIOUS_REGION],
			RegionRole::CATEGORY_PREVIOUS, 0);
		define(regions[CATEGORY_CLEAR_REGION], RegionRole::CATEGORY_CLEAR, 0);
		define(regions[CATEGORY_NEXT_REGION], RegionRole::CATEGORY_NEXT, 0);
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorTool::COUNT); ++i)
		{
			define(regions[TOOL_REGION + i], RegionRole::TOOL, i);
		}
		define(regions[LAYER_PREVIOUS_REGION], RegionRole::LAYER_PREVIOUS, 0);
		define(regions[LAYER_RESET_REGION], RegionRole::LAYER_RESET, 0);
		define(regions[LAYER_NEXT_REGION], RegionRole::LAYER_NEXT, 0);
		define(regions[VARIANT_PREVIOUS_REGION],
			RegionRole::VARIANT_PREVIOUS, 0);
		define(regions[VARIANT_NEXT_REGION], RegionRole::VARIANT_NEXT, 0);
		define(regions[REPLACE_REGION], RegionRole::REPLACE, 0);
		define(regions[EMPTY_REGION], RegionRole::EMPTY_MAP, 0);
		define(regions[SAVE_REGION], RegionRole::SAVE, 0);
		for (size_t i = 0; i < ENTRIES_PER_PAGE; ++i)
		{
			define(regions[ENTRY_REGION + i], RegionRole::ENTRY, i);
		}
		define(regions[PREVIOUS_REGION], RegionRole::PREVIOUS, 0);
		define(regions[NEXT_REGION], RegionRole::NEXT, 0);
		for (size_t i = 0; i < regions.size(); ++i)
			zOrderedRegions[i] = &regions[i].region;
	}

	void removeRegions() noexcept
	{
		for (Binding& binding : regions)
		{
			if (binding.defined)
			{
				MSYS_RemoveRegion(&binding.region);
				binding.defined = FALSE;
			}
		}
	}

	void setRegion(Binding& binding, INT16 const x, INT16 const y,
		INT16 const w, INT16 const h, BOOLEAN const enabled,
		ST::string const& help = {})
	{
		if (!binding.defined) return;
		binding.region.RegionTopLeftX = x;
		binding.region.RegionTopLeftY = y;
		binding.region.RegionBottomRightX = x + std::max<INT16>(1, w);
		binding.region.RegionBottomRightY = y + std::max<INT16>(1, h);
		if (binding.help != help)
		{
			binding.help = help;
			binding.region.SetFastHelpText(help);
		}
		if (enabled) binding.region.Enable();
		else binding.region.Disable();
	}

	void syncRegions()
	{
		if (!manager || window == OS0_INVALID_WINDOW) return;
		const OS0UIRect bounds = manager->bounds(window);
		const BOOLEAN enabled = active() && inputEnabled;

		const INT16 innerW = std::max<INT16>(1, bounds.w - MARGIN * 2);
		const INT16 tabW = std::max<INT16>(1,
			innerW / static_cast<INT16>(OS0RealtimeEditorPalette::COUNT));
		INT16 y = bounds.y + HEADER_H + GAP;
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorPalette::COUNT); ++i)
		{
			setRegion(regions[PALETTE_REGION + i],
				bounds.x + MARGIN + static_cast<INT16>(i) * tabW, y,
				i + 1 == static_cast<size_t>(OS0RealtimeEditorPalette::COUNT) ?
					innerW - static_cast<INT16>(i) * tabW : tabW,
				TAB_H, enabled, ST::format("{} CATALOG", PALETTE_NAMES[i]));
		}

		y += TAB_H + GAP;
		constexpr INT16 categoryArrowW = 34;
		setRegion(regions[CATEGORY_PREVIOUS_REGION], bounds.x + MARGIN, y,
			categoryArrowW, FILTER_H, enabled,
			"PREVIOUS CATEGORY FILTER");
		setRegion(regions[CATEGORY_CLEAR_REGION],
			bounds.x + MARGIN + categoryArrowW + GAP, y,
			innerW - categoryArrowW * 2 - GAP * 2, FILTER_H, enabled,
			ST::format("FILTER: {} / CLICK FOR ALL",
				categoryLabel(categoryFilters[paletteIndex()])));
		setRegion(regions[CATEGORY_NEXT_REGION],
			bounds.x + bounds.w - MARGIN - categoryArrowW, y,
			categoryArrowW, FILTER_H, enabled, "NEXT CATEGORY FILTER");

		y += FILTER_H + GAP;
		const INT16 toolW = std::max<INT16>(1,
			(innerW - GAP * 2) /
				static_cast<INT16>(OS0RealtimeEditorTool::COUNT));
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorTool::COUNT); ++i)
		{
			setRegion(regions[TOOL_REGION + i],
				bounds.x + MARGIN + static_cast<INT16>(i) * (toolW + GAP), y,
				toolW, TOOL_H, enabled,
				ST::format("{} WORLD TOOL", TOOL_NAMES[i]));
		}

		y += TOOL_H + GAP;
		constexpr INT16 layerArrowW = 24;
		constexpr INT16 layerLabelW = 104;
		constexpr INT16 variantW = 90;
		const BOOLEAN terrainBrush = selectedTileUsesTerrainBrush();
		const BOOLEAN roadBrush = selectedTileUsesRoadMacro();
		const BOOLEAN primitiveTileBrush =
			palette == OS0RealtimeEditorPalette::TILES &&
			!selectedTileUsesNativeRecipe();
		ST::string previousVariantHelp = "DECREASE ITEM QUANTITY";
		ST::string nextVariantHelp = "INCREASE ITEM QUANTITY";
		if (terrainBrush)
		{
			previousVariantHelp = "DECREASE TERRAIN BRUSH RADIUS";
			nextVariantHelp = "INCREASE TERRAIN BRUSH RADIUS";
		}
		else if (roadBrush)
		{
			previousVariantHelp = "PREVIOUS NATIVE ROAD MACRO";
			nextVariantHelp = "NEXT NATIVE ROAD MACRO";
		}
		else if (palette == OS0RealtimeEditorPalette::TILES)
		{
			previousVariantHelp = "PREVIOUS TILE VARIANT";
			nextVariantHelp = "NEXT TILE VARIANT";
		}
		else if (palette == OS0RealtimeEditorPalette::NPCS)
		{
			previousVariantHelp = "ROTATE NPC COUNTER-CLOCKWISE";
			nextVariantHelp = "ROTATE NPC CLOCKWISE";
		}
		INT16 brushX = bounds.x + MARGIN;
		setRegion(regions[LAYER_PREVIOUS_REGION], brushX, y, layerArrowW,
			BRUSH_H, enabled && primitiveTileBrush,
			"PREVIOUS TARGET LAYER");
		brushX += layerArrowW + GAP;
		setRegion(regions[LAYER_RESET_REGION], brushX, y, layerLabelW,
			BRUSH_H, enabled && primitiveTileBrush,
			"RESET TO THE ASSET'S ENGINE LAYER");
		brushX += layerLabelW + GAP;
		setRegion(regions[LAYER_NEXT_REGION], brushX, y, layerArrowW,
			BRUSH_H, enabled && primitiveTileBrush,
			"NEXT TARGET LAYER");
		brushX += layerArrowW + GAP;
		setRegion(regions[VARIANT_PREVIOUS_REGION], brushX, y, variantW,
			BRUSH_H, enabled && palette != OS0RealtimeEditorPalette::SYSTEM,
			previousVariantHelp);
		brushX += variantW + GAP;
		setRegion(regions[VARIANT_NEXT_REGION], brushX, y, variantW,
			BRUSH_H, enabled && palette != OS0RealtimeEditorPalette::SYSTEM,
			nextVariantHelp);
		brushX += variantW + GAP;
		setRegion(regions[REPLACE_REGION], brushX, y,
			bounds.x + MARGIN + innerW - brushX, BRUSH_H,
			enabled && palette == OS0RealtimeEditorPalette::TILES,
			terrainBrush ?
				(selectedTileCanSmoothTerrain() ?
					"SWITCH BETWEEN TERRAIN PAINT AND SMOOTH" :
					"THIS WATER TEXTURE SUPPORTS PAINT ONLY") :
			roadBrush ? "RESET NATIVE ROAD MACRO TO ZERO" :
			replaceExisting ? "REPLACE SAME-LAYER TILE WHEN PLACING" :
				"KEEP EXISTING TILES WHEN PLACING");

		y += BRUSH_H + GAP;
		const INT16 commandW = (innerW - GAP) / 2;
		setRegion(regions[EMPTY_REGION], bounds.x + MARGIN, y, commandW,
			COMMAND_H, enabled,
			blankConfirmArmed ? "CLICK AGAIN TO REPLACE THE LIVE MAP" :
				"ARM NEW BLANK MAP");
		setRegion(regions[SAVE_REGION], bounds.x + MARGIN + commandW + GAP, y,
			innerW - commandW - GAP, COMMAND_H, enabled,
			"SAVE TO USER OS0/MAPS/LIVE-EDITOR.DAT");

		y += COMMAND_H + GAP;
		const INT16 footerY = bounds.y + bounds.h - FOOTER_H;
		const INT16 contentH = std::max<INT16>(1, footerY - y - GAP);
		const INT16 cellW = std::max<INT16>(1, (innerW - GAP) / 2);
		const INT16 cellH = std::max<INT16>(1,
			(contentH - GAP * (static_cast<INT16>(ENTRY_ROWS) - 1)) /
				static_cast<INT16>(ENTRY_ROWS));
		const size_t first = pages[paletteIndex()] * ENTRIES_PER_PAGE;
		const size_t count = entryCount();
		for (size_t i = 0; i < ENTRIES_PER_PAGE; ++i)
		{
			const INT16 column = static_cast<INT16>(i % ENTRY_COLUMNS);
			const INT16 row = static_cast<INT16>(i / ENTRY_COLUMNS);
			const size_t filtered = first + i;
			const size_t absolute = absoluteEntry(filtered);
			setRegion(regions[ENTRY_REGION + i],
				bounds.x + MARGIN + column * (cellW + GAP),
				y + row * (cellH + GAP),
				column == static_cast<INT16>(ENTRY_COLUMNS - 1) ?
					innerW - cellW - GAP : cellW,
				cellH, enabled && filtered < count,
				filtered < count ? entryHelp(absolute) : ST::string{});
		}

		setRegion(regions[PREVIOUS_REGION], bounds.x + MARGIN, footerY + 2,
			42, FOOTER_H - 4, enabled && pages[paletteIndex()] > 0,
			"PREVIOUS PAGE");
		setRegion(regions[NEXT_REGION], bounds.x + bounds.w - MARGIN - 42,
			footerY + 2, 42, FOOTER_H - 4,
			enabled && pages[paletteIndex()] + 1 < pageCount(), "NEXT PAGE");
	}

	ST::string entryLabel(size_t const absolute) const
	{
		switch (palette)
		{
			case OS0RealtimeEditorPalette::TILES:
				if (absolute < catalog().tiles.size())
					return catalog().tiles[absolute].label;
				break;
			case OS0RealtimeEditorPalette::ITEMS:
				if (absolute < catalog().items.size())
					return catalog().items[absolute].label;
				break;
			case OS0RealtimeEditorPalette::NPCS:
				if (absolute < catalog().npcTemplates.size())
					return catalog().npcTemplates[absolute].label;
				break;
			case OS0RealtimeEditorPalette::SYSTEM:
				if (absolute < catalog().actions.size())
					return catalog().actions[absolute].label;
				break;
			case OS0RealtimeEditorPalette::COUNT: break;
		}
		return {};
	}

	ST::string entryHelp(size_t const absolute) const
	{
		switch (palette)
		{
			case OS0RealtimeEditorPalette::TILES:
				if (absolute < catalog().tiles.size())
				{
					auto const& entry = catalog().tiles[absolute];
					return ST::format(
						"{} / {} / TILE {} / VARIANT {} / LAYER {} / {}x{}{}{}",
						entry.label, categoryLabel(entry.category), entry.tileIndex,
						entry.regionIndex,
						LAYER_NAMES[std::min<size_t>(
							static_cast<size_t>(entry.layer), LAYER_NAMES.size() - 1)],
						entry.footprintWidth, entry.footprintHeight,
						entry.animated ? " / ANIMATED" : "",
						entry.placeable ? " / CLICK TO ARM" : " / READ ONLY");
				}
				break;
			case OS0RealtimeEditorPalette::ITEMS:
				if (absolute < catalog().items.size())
				{
					auto const& entry = catalog().items[absolute];
					return ST::format("{} / {} / ITEM {} / WEIGHT {} / POCKET {}",
						entry.label, categoryLabel(entry.category), entry.itemIndex,
						entry.weight, entry.perPocket);
				}
				break;
			case OS0RealtimeEditorPalette::NPCS:
				if (absolute < catalog().npcTemplates.size())
				{
					auto const& entry = catalog().npcTemplates[absolute];
					return ST::format("{} / TEAM {} / BODY {} / PROFILE {}",
						entry.label, entry.team, entry.bodyType, entry.profileId);
				}
				break;
			case OS0RealtimeEditorPalette::SYSTEM:
				if (absolute < catalog().actions.size())
					return catalog().actions[absolute].explanation;
				break;
			case OS0RealtimeEditorPalette::COUNT: break;
		}
		return {};
	}

	void onRegionEvent(Binding const& binding, UINT32 const reason)
	{
		if (!manager || !active()) return;
		if (reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
		{
			if (hovered == &binding)
			{
				hovered = nullptr;
				hoverHelp.clear();
				SetRenderFlags(RENDER_FLAG_FULL);
			}
		}
		else if (reason & (MSYS_CALLBACK_REASON_GAIN_MOUSE |
			MSYS_CALLBACK_REASON_MOVE))
		{
			if (hovered != &binding || hoverHelp != binding.help)
			{
				hovered = &binding;
				hoverHelp = binding.help;
				SetRenderFlags(RENDER_FLAG_FULL);
			}
		}
		if (reason & (MSYS_CALLBACK_REASON_WHEEL_UP |
			MSYS_CALLBACK_REASON_WHEEL_DOWN))
		{
			const BOOLEAN next = reason & MSYS_CALLBACK_REASON_WHEEL_DOWN;
			changePage(next ? 1 : -1);
			syncRegions();
			SetRenderFlags(RENDER_FLAG_FULL);
			return;
		}

		if (!(reason & MSYS_CALLBACK_REASON_POINTER_UP)) return;

		manager->bringToFront(window);
		switch (binding.role)
		{
			case RegionRole::PALETTE:
				setPalette(static_cast<OS0RealtimeEditorPalette>(binding.index));
				break;
			case RegionRole::CATEGORY_PREVIOUS: changeCategory(-1); break;
			case RegionRole::CATEGORY_CLEAR: clearCategory(); break;
			case RegionRole::CATEGORY_NEXT: changeCategory(1); break;
			case RegionRole::TOOL:
				setTool(static_cast<OS0RealtimeEditorTool>(binding.index));
				break;
			case RegionRole::LAYER_PREVIOUS: changeLayer(-1); break;
			case RegionRole::LAYER_RESET: resetLayer(); break;
			case RegionRole::LAYER_NEXT: changeLayer(1); break;
			case RegionRole::VARIANT_PREVIOUS: changeVariant(-1); break;
			case RegionRole::VARIANT_NEXT: changeVariant(1); break;
			case RegionRole::REPLACE: toggleReplace(); break;
			case RegionRole::EMPTY_MAP: activateBlankMap();
				break;
			case RegionRole::SAVE: saveMap();
				break;
			case RegionRole::ENTRY: activateEntry(binding.index);
				break;
			case RegionRole::PREVIOUS: changePage(-1);
				break;
			case RegionRole::NEXT: changePage(1);
				break;
		}
		syncRegions();
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void changePage(INT16 const direction)
	{
		size_t& page = pages[paletteIndex()];
		if (direction < 0)
		{
			if (page > 0) --page;
		}
		else if (page + 1 < pageCount())
		{
			++page;
		}
		cancelBlankConfirmation();
	}

	void activateEntry(size_t const visibleSlot)
	{
		const size_t filtered =
			pages[paletteIndex()] * ENTRIES_PER_PAGE + visibleSlot;
		if (filtered >= entryCount()) return;
		const size_t absolute = absoluteEntry(filtered);
		selected[paletteIndex()] = absolute;
		normalizeNativeRecipeState();
		cancelBlankConfirmation();

		if (palette != OS0RealtimeEditorPalette::SYSTEM)
		{
			if (palette == OS0RealtimeEditorPalette::TILES &&
				!catalog().tiles[absolute].placeable)
			{
				localStatus = "ASSET IS READ-ONLY / ENGINE REJECTED";
				return;
			}
			tool = OS0RealtimeEditorTool::PLACE;
			localStatus = ST::format("{} READY / CLICK WORLD",
				entryLabel(absolute).left(31));
			return;
		}

		OS0EditorAction const action = catalog().actions[absolute].id;
		localStatus = ST::format("SYSTEM / {} SELECTED",
			catalog().actions[absolute].label.left(27));
		switch (action)
		{
			case OS0EditorAction::NEW_BLANK_MAP: activateBlankMap(); break;
			case OS0EditorAction::LOAD_MAP:
			{
				OS0EditorLoadRequest request;
				request.name = "live-editor";
				OS0GetRealtimeEditor().queueLoad(request);
				localStatus = "LOAD QUEUED / SAFE FRAME BOUNDARY";
				break;
			}
			case OS0EditorAction::PLACE_TILE:
				setPalette(OS0RealtimeEditorPalette::TILES);
				setTool(OS0RealtimeEditorTool::PLACE);
				break;
			case OS0EditorAction::PLACE_ITEM:
				setPalette(OS0RealtimeEditorPalette::ITEMS);
				setTool(OS0RealtimeEditorTool::PLACE);
				break;
			case OS0EditorAction::PLACE_NPC:
				setPalette(OS0RealtimeEditorPalette::NPCS);
				setTool(OS0RealtimeEditorTool::PLACE);
				break;
			case OS0EditorAction::REMOVE:
				setTool(OS0RealtimeEditorTool::ERASE);
				break;
			case OS0EditorAction::SAVE_MAP: saveMap(); break;
			case OS0EditorAction::REBUILD_CATALOGS:
				OS0GetRealtimeEditor().queueRebuildCatalogs();
				localStatus = "CATALOG REFRESH QUEUED";
				break;
			case OS0EditorAction::COUNT: break;
			default:
				// Newly registered engine actions remain browsable immediately. The
				// session owns their eventual queue route; presentation never mutates
				// the world directly.
				break;
		}
	}

	void activateBlankMap()
	{
		const UINT32 now = GetJA2Clock();
		if (!blankConfirmArmed ||
			static_cast<UINT32>(now - blankConfirmStarted) >
				CONFIRM_TIMEOUT_MS)
		{
			blankConfirmArmed = TRUE;
			blankConfirmStarted = now;
			localStatus = "EMPTY MAP ARMED / CLICK AGAIN TO CONFIRM";
			return;
		}

		OS0EditorBlankMapRequest request;
		request.tileset = catalog().tileset;
		OS0GetRealtimeEditor().queueNewBlankMap(request);
		cancelBlankConfirmation();
		localStatus = "BLANK MAP QUEUED / SAFE FRAME BOUNDARY";
	}

	void saveMap()
	{
		OS0EditorSaveRequest request;
		request.name = "live-editor";
		OS0GetRealtimeEditor().queueSave(request);
		cancelBlankConfirmation();
		localStatus = "SAVE QUEUED / OS0/MAPS/LIVE-EDITOR.DAT";
	}

	void revealSelection(OS0RealtimeEditorPalette const value,
		size_t const absolute)
	{
		palette = value;
		const size_t p = paletteIndex();
		categoryFilters[p] = entryCategory(value, absolute);
		rebuildFilterCache(value);
		selected[p] = absolute;
		normalizeNativeRecipeState();
		auto const position = std::find(filteredEntries[p].begin(),
			filteredEntries[p].end(), absolute);
		pages[p] = position == filteredEntries[p].end() ? 0 :
			static_cast<size_t>(std::distance(filteredEntries[p].begin(),
				position)) / ENTRIES_PER_PAGE;
	}

	BOOLEAN handleWorldClick(SOLDIERTYPE* const target, GridNo const gridNo,
		UINT8 const level, UINT16 const tileIndex)
	{
		if (!active()) return FALSE;
		// A modal/context surface currently owns input. Consume any delayed world
		// event instead of leaking it into vanilla gameplay behind that surface.
		if (!inputEnabled) return TRUE;
		cancelBlankConfirmation();
		if (gridNo < 0)
		{
			localStatus = "WORLD CLICK REJECTED / INVALID GRID";
			return TRUE;
		}

		if (tool == OS0RealtimeEditorTool::SELECT)
		{
			if (palette == OS0RealtimeEditorPalette::NPCS && target)
			{
				auto const found = std::find_if(catalog().npcTemplates.begin(),
					catalog().npcTemplates.end(),
					[target](OS0EditorNpcTemplate const& entry)
					{
						if (target->ubProfile != NO_PROFILE)
							return entry.profileId == target->ubProfile;
						return entry.profileId == std::numeric_limits<UINT16>::max() &&
							entry.team == target->bTeam &&
							(entry.bodyType < 0 ||
							 entry.bodyType == static_cast<INT8>(target->ubBodyType));
					});
				if (found != catalog().npcTemplates.end())
				{
					revealSelection(OS0RealtimeEditorPalette::NPCS,
						static_cast<size_t>(std::distance(
							catalog().npcTemplates.begin(), found)));
					localStatus = ST::format("SELECTED NPC {}", found->label.left(28));
					syncRegions();
				}
				else localStatus = "NPC HAS NO CATALOG TEMPLATE";
				return TRUE;
			}
			if (palette == OS0RealtimeEditorPalette::ITEMS)
			{
				ITEM_POOL const* const pool = GetItemPool(gridNo, level);
				if (pool != nullptr)
				{
					WORLDITEM const& worldItem = GetWorldItem(pool->iItemIndex);
					auto const found = std::find_if(catalog().items.begin(),
						catalog().items.end(),
						[&worldItem](OS0EditorItemRecord const& entry)
						{ return entry.itemIndex == worldItem.o.usItem; });
					if (found != catalog().items.end())
					{
						revealSelection(OS0RealtimeEditorPalette::ITEMS,
							static_cast<size_t>(std::distance(
								catalog().items.begin(), found)));
						localStatus = ST::format("SELECTED ITEM {}",
							found->label.left(27));
						syncRegions();
					}
					return TRUE;
				}
			}
			auto const found = std::find_if(catalog().tiles.begin(),
				catalog().tiles.end(),
				[tileIndex](OS0EditorTileRecord const& entry)
				{
					return entry.tileIndex == tileIndex;
				});
			if (found == catalog().tiles.end())
			{
				localStatus = ST::format("TILE {} IS NOT IN ACTIVE TILESET",
					tileIndex);
				return TRUE;
			}
			revealSelection(OS0RealtimeEditorPalette::TILES,
				static_cast<size_t>(std::distance(catalog().tiles.begin(), found)));
			localStatus = ST::format("SELECTED {}", found->label.left(34));
			syncRegions();
			return TRUE;
		}

		if (tool == OS0RealtimeEditorTool::ERASE)
		{
			OS0EditorRemoveRequest request;
			request.expectedWorldRevision = WorldTileMutationRevision();
			if (target && palette == OS0RealtimeEditorPalette::NPCS)
			{
				request.kind = OS0EditorRemoveKind::NPC;
				request.soldierId = Soldier2ID(target);
				request.expectedNpcInstanceId = target->uiUniqueSoldierIdValue;
				request.expectedNpcTeam = target->bTeam;
				request.expectedNpcGridNo = target->sGridNo;
			}
			else if (palette == OS0RealtimeEditorPalette::ITEMS &&
				GetItemPool(gridNo, level) != nullptr)
			{
				ITEM_POOL const* const pool = GetItemPool(gridNo, level);
				WORLDITEM const& worldItem = GetWorldItem(pool->iItemIndex);
				request.kind = OS0EditorRemoveKind::WORLD_ITEM;
				request.worldItemIndex = pool->iItemIndex;
				request.expectedItemIndex = worldItem.o.usItem;
				request.expectedItemGridNo = worldItem.sGridNo;
			}
			else
			{
				request.kind = OS0EditorRemoveKind::TILE;
				request.gridNo = gridNo;
				request.tileIndex = tileIndex;
				request.tileset = catalog().tileset;
				// Native recipes do not expose a layer selector. Resolve the clicked
				// tile through its canonical engine layer instead of leaking a stale
				// primitive-brush override into erase.
				request.layer = selectedTileUsesNativeRecipe() ?
					OS0EditorLayer::AUTO : brushLayer;
			}
			OS0GetRealtimeEditor().queueRemove(request);
			localStatus = ST::format("REMOVE {} QUEUED AT {}",
				palette == OS0RealtimeEditorPalette::NPCS ? "NPC" :
				palette == OS0RealtimeEditorPalette::ITEMS ? "ITEM" : "TILE",
				gridNo);
			return TRUE;
		}

		const size_t chosen = selected[paletteIndex()];
		switch (palette)
		{
			case OS0RealtimeEditorPalette::TILES:
				if (chosen < catalog().tiles.size())
				{
					auto const& entry = catalog().tiles[chosen];
					if (!entry.placeable)
					{
						localStatus = "ASSET IS READ-ONLY";
						return TRUE;
					}
					if (selectedTileUsesTerrainBrush())
					{
						const BOOLEAN useSmooth = smoothTerrain &&
							selectedTileCanSmoothTerrain();
						if (useSmooth)
						{
							OS0EditorTerrainSmoothRecipe request;
							request.gridNo = gridNo;
							request.textureType = entry.tileType;
							request.radius = brushRadius;
							request.force = TRUE;
							OS0GetRealtimeEditor().queueSmoothTerrain(request);
						}
						else
						{
							OS0EditorTerrainPaintRecipe request;
							request.gridNo = gridNo;
							request.textureType = entry.tileType;
							request.radius = brushRadius;
							OS0GetRealtimeEditor().queuePaintTerrain(request);
						}
						localStatus = ST::format("TERRAIN {} R{} QUEUED AT {}",
							useSmooth ? "SMOOTH" : "PAINT", brushRadius,
							gridNo);
					}
					else if (selectedTileUsesRoadMacro())
					{
						OS0EditorRoadRecipe request;
						request.gridNo = gridNo;
						request.macroId = roadMacroId;
						OS0GetRealtimeEditor().queuePlaceRoad(request);
						localStatus = ST::format("ROAD MACRO {} QUEUED AT {}",
							roadMacroId, gridNo);
					}
					else
					{
						OS0EditorTilePlacement request;
						request.gridNo = gridNo;
						request.tileIndex = entry.tileIndex;
						request.tileset = entry.tileset;
						request.layer = brushLayer == OS0EditorLayer::AUTO ?
							entry.layer : brushLayer;
						request.replaceExisting = replaceExisting;
						OS0GetRealtimeEditor().queuePlaceTile(request);
						localStatus = ST::format("PLACE {} QUEUED AT {}",
							entry.label.left(25), gridNo);
					}
				}
				break;
			case OS0RealtimeEditorPalette::ITEMS:
				if (chosen < catalog().items.size())
				{
					auto const& entry = catalog().items[chosen];
					OS0EditorItemPlacement request;
					request.gridNo = gridNo;
					request.itemIndex = entry.itemIndex;
					request.quantity = itemQuantity;
					request.level = level;
					OS0GetRealtimeEditor().queuePlaceItem(request);
					localStatus = ST::format("ITEM {} QUEUED AT {}",
						entry.label.left(26), gridNo);
				}
				break;
			case OS0RealtimeEditorPalette::NPCS:
				if (chosen < catalog().npcTemplates.size())
				{
					auto const& entry = catalog().npcTemplates[chosen];
					OS0EditorNpcPlacement request;
					request.gridNo = gridNo;
					request.templateId = entry.id;
					request.direction = npcDirection;
					request.onRoof = level > 0;
					OS0GetRealtimeEditor().queuePlaceNpc(request);
					localStatus = ST::format("NPC {} QUEUED AT {}",
						entry.label.left(27), gridNo);
				}
				break;
			case OS0RealtimeEditorPalette::SYSTEM:
			case OS0RealtimeEditorPalette::COUNT:
				localStatus = "SELECT TILES, ITEMS OR NPCS TO PLACE";
				break;
		}
		return TRUE;
	}

	void update()
	{
		if (!manager) return;
		const BOOLEAN nowActive = active();
		BOOLEAN regionsDirty = FALSE;
		if (regionsActive != nowActive)
		{
			regionsActive = nowActive;
			if (!nowActive)
			{
				for (Binding& binding : regions) binding.region.Disable();
				return;
			}
			regionsDirty = TRUE;
		}
		if (!nowActive) return;
		OS0RealtimeEditorSession& session = OS0GetRealtimeEditor();
		if (session.catalog().generation == 0)
		{
			session.rebuildCatalogs();
		}
		if (catalogGeneration != session.catalog().generation)
		{
			catalogGeneration = session.catalog().generation;
			rebuildFilterCaches();
			clampPresentationState();
			normalizeNativeRecipeState();
			regionsDirty = TRUE;
		}
		if (blankConfirmArmed &&
			static_cast<UINT32>(GetJA2Clock() - blankConfirmStarted) >
				CONFIRM_TIMEOUT_MS)
		{
			cancelBlankConfirmation();
			localStatus = "EMPTY MAP CONFIRMATION EXPIRED";
			regionsDirty = TRUE;
		}
		if (manager->draggingWindow() == window) regionsDirty = TRUE;
		if (regionsDirty) syncRegions();
	}

	void drawPreview(size_t const absolute, INT16 const x, INT16 const y,
		INT16 const size)
	{
		if (!preview)
			preview = AddVideoSurface(64, 64, PIXEL_DEPTH);
		preview->Fill(Colour(5, 7, 7));

		if (palette == OS0RealtimeEditorPalette::TILES &&
			absolute < catalog().tiles.size())
		{
			auto const& entry = catalog().tiles[absolute];
			if (entry.tileIndex < NUMBEROFTILES)
			{
				TILE_ELEMENT const& tile = gTileDatabase[entry.tileIndex];
				if (tile.hTileSurface)
				{
					ETRLEObject const& frame =
						tile.hTileSurface->SubregionProperties(
							tile.usRegionIndex);
					const INT16 drawX = static_cast<INT16>(
						32 - frame.usWidth / 2 - frame.sOffsetX);
					const INT16 drawY = static_cast<INT16>(
						57 - frame.usHeight - frame.sOffsetY);
					BltVideoObject(preview, tile.hTileSurface,
						tile.usRegionIndex, drawX, drawY);
				}
			}
		}
		else if (palette == OS0RealtimeEditorPalette::ITEMS &&
			absolute < catalog().items.size())
		{
			ItemModel const* const item =
				GCM->getItem(catalog().items[absolute].itemIndex);
			if (item)
			{
				CSubVObject const graphic =
					GetSmallInventoryGraphicForItem(item);
				if (graphic.first)
				{
					ETRLEObject const& frame =
						graphic.first->SubregionProperties(graphic.second);
					const INT16 drawX = static_cast<INT16>(
						32 - frame.usWidth / 2 - frame.sOffsetX);
					const INT16 drawY = static_cast<INT16>(
						32 - frame.usHeight / 2 - frame.sOffsetY);
					BltVideoObjectOutline(preview, graphic.first,
						graphic.second, drawX, drawY, SGP_TRANSPARENT);
				}
			}
		}
		else
		{
			OS0UIIcon icon = OS0UIIcon::TALK;
			if (palette == OS0RealtimeEditorPalette::SYSTEM &&
				absolute < catalog().actions.size())
			{
				icon = ActionIcon(catalog().actions[absolute].id);
			}
			OS0UIAssets().draw(icon, preview, 22, 22);
		}

		const SGPBox source{ 0, 0, 64, 64 };
		const SGPBox destination{
			static_cast<UINT16>(std::max<INT16>(0, x)),
			static_cast<UINT16>(std::max<INT16>(0, y)),
			static_cast<UINT16>(std::max<INT16>(1, size)),
			static_cast<UINT16>(std::max<INT16>(1, size))
		};
		BltStretchVideoSurface(FRAME_BUFFER, preview, &source, &destination);
	}

	void render()
	{
		if (!active()) return;
		const OS0UIRect bounds = manager->bounds(window);
		const UINT16 black = Colour(2, 4, 4);
		const UINT16 black2 = Colour(6, 8, 8);
		const UINT16 red = Colour(205, 12, 12);
		const UINT16 mutedRed = Colour(75, 10, 10);
		const UINT16 selectedRed = Colour(117, 9, 9);
		const UINT16 grey = Colour(47, 50, 50);

		Fill(bounds.x, bounds.y, bounds.w, bounds.h, black);
		LineBox(bounds.x, bounds.y, bounds.w, bounds.h, mutedRed);
		Fill(bounds.x, bounds.y, bounds.w, 2, red);
		Fill(bounds.x + 1, bounds.y + HEADER_H - 1, bounds.w - 2, 1,
			mutedRed);
		OS0UIAssets().draw(OS0UIIcon::TOOLKIT, FRAME_BUFFER,
			bounds.x + 5, bounds.y + 1);
		Print(bounds.x + 27, bounds.y + 5, "WORLD EDITOR / LIVE",
			FONT_MCOLOR_RED);
		ST::string brushStatus = LAYER_NAMES[static_cast<size_t>(brushLayer)];
		if (selectedTileUsesTerrainBrush())
		{
			brushStatus = ST::format("{} R{}",
				smoothTerrain && selectedTileCanSmoothTerrain() ? "SMOOTH" : "PAINT",
				brushRadius);
		}
		else if (selectedTileUsesRoadMacro())
		{
			brushStatus = ST::format("ROAD M{}", roadMacroId);
		}
		Print(bounds.x + 156, bounds.y + 5,
			ST::format("{} / {}", TOOL_NAMES[static_cast<size_t>(tool)],
				brushStatus),
			FONT_MCOLOR_DKGRAY);
		Print(bounds.x + bounds.w - 14, bounds.y + 5, "X",
			FONT_MCOLOR_RED);

		const INT16 innerW = std::max<INT16>(1, bounds.w - MARGIN * 2);
		const INT16 tabW = std::max<INT16>(1,
			innerW / static_cast<INT16>(OS0RealtimeEditorPalette::COUNT));
		INT16 y = bounds.y + HEADER_H + GAP;
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorPalette::COUNT); ++i)
		{
			const INT16 x = bounds.x + MARGIN + static_cast<INT16>(i) * tabW;
			if (i == paletteIndex()) Fill(x, y, tabW, TAB_H, mutedRed);
			Print(x + 5, y + 5, PALETTE_NAMES[i],
				i == paletteIndex() ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		}

		y += TAB_H + GAP;
		constexpr INT16 categoryArrowW = 34;
		const INT16 categoryCenterX = bounds.x + MARGIN + categoryArrowW + GAP;
		const INT16 categoryCenterW =
			innerW - categoryArrowW * 2 - GAP * 2;
		Fill(bounds.x + MARGIN, y, categoryArrowW, FILTER_H, black2);
		Fill(categoryCenterX, y, categoryCenterW, FILTER_H, black2);
		Fill(bounds.x + bounds.w - MARGIN - categoryArrowW, y,
			categoryArrowW, FILTER_H, black2);
		LineBox(categoryCenterX, y, categoryCenterW, FILTER_H, mutedRed);
		Print(bounds.x + MARGIN + 13, y + 5, "<", FONT_MCOLOR_RED);
		ST::string const filter = ST::format("FILTER / {} ({})",
			categoryLabel(categoryFilters[paletteIndex()]), entryCount());
		Print(categoryCenterX + 6, y + 5,
			filter.left(std::max<INT16>(8, (categoryCenterW - 10) / 5)),
			FONT_MCOLOR_LTGRAY);
		Print(bounds.x + bounds.w - MARGIN - 21, y + 5, ">",
			FONT_MCOLOR_RED);

		y += FILTER_H + GAP;
		const INT16 toolW = std::max<INT16>(1,
			(innerW - GAP * 2) /
				static_cast<INT16>(OS0RealtimeEditorTool::COUNT));
		for (size_t i = 0;
			i < static_cast<size_t>(OS0RealtimeEditorTool::COUNT); ++i)
		{
			const INT16 x =
				bounds.x + MARGIN + static_cast<INT16>(i) * (toolW + GAP);
			const BOOLEAN activeTool = i == static_cast<size_t>(tool);
			Fill(x, y, toolW, TOOL_H, activeTool ? selectedRed : black2);
			LineBox(x, y, toolW, TOOL_H, activeTool ? red : grey);
			OS0UIAssets().draw(TOOL_ICONS[i], FRAME_BUFFER, x + 3, y + 2);
			Print(x + 25, y + 6, TOOL_NAMES[i],
				activeTool ? FONT_WHITE : FONT_MCOLOR_DKGRAY);
		}

		y += TOOL_H + GAP;
		constexpr INT16 layerArrowW = 24;
		constexpr INT16 layerLabelW = 104;
		constexpr INT16 variantW = 90;
		const BOOLEAN terrainBrush = selectedTileUsesTerrainBrush();
		const BOOLEAN roadBrush = selectedTileUsesRoadMacro();
		const BOOLEAN primitiveTileBrush =
			palette == OS0RealtimeEditorPalette::TILES &&
			!selectedTileUsesNativeRecipe();
		INT16 brushX = bounds.x + MARGIN;
		Fill(brushX, y, layerArrowW, BRUSH_H, black2);
		Print(brushX + 8, y + 6, "<",
			primitiveTileBrush ? FONT_MCOLOR_RED : FONT_MCOLOR_DKGRAY);
		brushX += layerArrowW + GAP;
		Fill(brushX, y, layerLabelW, BRUSH_H, black2);
		LineBox(brushX, y, layerLabelW, BRUSH_H,
			selectedTileUsesNativeRecipe() ? mutedRed :
			brushLayer == OS0EditorLayer::AUTO ? grey : red);
		ST::string const layerLabel = terrainBrush ? "TERRAIN BRUSH" :
			roadBrush ? "ROAD MACRO" :
			ST::format("LAYER {}", LAYER_NAMES[static_cast<size_t>(brushLayer)]);
		Print(brushX + 5, y + 6,
			layerLabel,
			primitiveTileBrush ? FONT_MCOLOR_LTGRAY :
			selectedTileUsesNativeRecipe() ? FONT_MCOLOR_RED : FONT_MCOLOR_DKGRAY);
		brushX += layerLabelW + GAP;
		Fill(brushX, y, layerArrowW, BRUSH_H, black2);
		Print(brushX + 8, y + 6, ">",
			primitiveTileBrush ? FONT_MCOLOR_RED : FONT_MCOLOR_DKGRAY);
		brushX += layerArrowW + GAP;
		ST::string previousLabel = "VARIANT -";
		ST::string nextLabel = "VARIANT +";
		if (terrainBrush)
		{
			previousLabel = "RADIUS -";
			nextLabel = "RADIUS +";
		}
		else if (roadBrush)
		{
			previousLabel = "MACRO -";
			nextLabel = "MACRO +";
		}
		else if (palette == OS0RealtimeEditorPalette::NPCS)
		{
			previousLabel = "ROTATE -";
			nextLabel = "ROTATE +";
		}
		else if (palette == OS0RealtimeEditorPalette::ITEMS)
		{
			previousLabel = "QUANTITY -";
			nextLabel = "QUANTITY +";
		}
		Fill(brushX, y, variantW, BRUSH_H, black2);
		LineBox(brushX, y, variantW, BRUSH_H, grey);
		Print(brushX + 5, y + 6, previousLabel,
			palette == OS0RealtimeEditorPalette::SYSTEM ? FONT_MCOLOR_DKGRAY :
				FONT_MCOLOR_LTGRAY);
		brushX += variantW + GAP;
		Fill(brushX, y, variantW, BRUSH_H, black2);
		LineBox(brushX, y, variantW, BRUSH_H, grey);
		Print(brushX + 5, y + 6, nextLabel,
			palette == OS0RealtimeEditorPalette::SYSTEM ? FONT_MCOLOR_DKGRAY :
				FONT_MCOLOR_LTGRAY);
		brushX += variantW + GAP;
		const INT16 replaceW = bounds.x + MARGIN + innerW - brushX;
		const BOOLEAN optionActive = terrainBrush ? smoothTerrain :
			roadBrush ? FALSE : replaceExisting;
		ST::string optionLabel = replaceExisting ? "REPLACE ON" : "ADDITIVE";
		if (terrainBrush && !selectedTileCanSmoothTerrain())
			optionLabel = "PAINT ONLY";
		else if (terrainBrush)
			optionLabel = ST::format("{} R{}",
				smoothTerrain ? "SMOOTH" : "PAINT", brushRadius);
		else if (roadBrush)
			optionLabel = ST::format("RESET M{}", roadMacroId);
		Fill(brushX, y, replaceW, BRUSH_H,
			optionActive ? selectedRed : black2);
		LineBox(brushX, y, replaceW, BRUSH_H,
			optionActive ? red : grey);
		Print(brushX + 5, y + 6, optionLabel,
			palette == OS0RealtimeEditorPalette::TILES ? FONT_MCOLOR_LTGRAY :
				FONT_MCOLOR_DKGRAY);

		y += BRUSH_H + GAP;
		const INT16 commandW = (innerW - GAP) / 2;
		Fill(bounds.x + MARGIN, y, commandW, COMMAND_H,
			blankConfirmArmed ? red : black2);
		LineBox(bounds.x + MARGIN, y, commandW, COMMAND_H,
			blankConfirmArmed ? red : mutedRed);
		OS0UIAssets().draw(OS0UIIcon::EXPLOSIVE, FRAME_BUFFER,
			bounds.x + MARGIN + 3, y + 2);
		Print(bounds.x + MARGIN + 25, y + 6,
			blankConfirmArmed ? "CONFIRM EMPTY" : "EMPTY MAP",
			blankConfirmArmed ? FONT_WHITE : FONT_MCOLOR_RED);

		const INT16 saveX = bounds.x + MARGIN + commandW + GAP;
		Fill(saveX, y, innerW - commandW - GAP, COMMAND_H, black2);
		LineBox(saveX, y, innerW - commandW - GAP, COMMAND_H, mutedRed);
		OS0UIAssets().draw(OS0UIIcon::TOOLKIT, FRAME_BUFFER,
			saveX + 3, y + 2);
		Print(saveX + 25, y + 6, "SAVE MAP", FONT_MCOLOR_RED);

		y += COMMAND_H + GAP;
		const INT16 footerY = bounds.y + bounds.h - FOOTER_H;
		const INT16 contentH = std::max<INT16>(1, footerY - y - GAP);
		const INT16 cellW = std::max<INT16>(1, (innerW - GAP) / 2);
		const INT16 cellH = std::max<INT16>(1,
			(contentH - GAP * (static_cast<INT16>(ENTRY_ROWS) - 1)) /
				static_cast<INT16>(ENTRY_ROWS));
		const size_t first = pages[paletteIndex()] * ENTRIES_PER_PAGE;
		for (size_t i = 0; i < ENTRIES_PER_PAGE; ++i)
		{
			const size_t filtered = first + i;
			if (filtered >= entryCount()) continue;
			const size_t absolute = absoluteEntry(filtered);
			const INT16 column = static_cast<INT16>(i % ENTRY_COLUMNS);
			const INT16 row = static_cast<INT16>(i / ENTRY_COLUMNS);
			const INT16 cellX = bounds.x + MARGIN + column * (cellW + GAP);
			const INT16 width =
				column == static_cast<INT16>(ENTRY_COLUMNS - 1) ?
					innerW - cellW - GAP : cellW;
			const INT16 cellY = y + row * (cellH + GAP);
			const BOOLEAN chosen = selected[paletteIndex()] == absolute;
			const BOOLEAN isHovered =
				hovered == &regions[ENTRY_REGION + i];
			Fill(cellX, cellY, width, cellH,
				isHovered ? Colour(18, 20, 20) : black2);
			LineBox(cellX, cellY, width, cellH,
				chosen ? red : grey);
			const INT16 previewSize = std::clamp<INT16>(
				cellH - 6, 18, std::min<INT16>(42, width / 3));
			drawPreview(absolute, cellX + 3,
				cellY + (cellH - previewSize) / 2, previewSize);
			Print(cellX + previewSize + 8, cellY + 6,
				entryLabel(absolute).left(
					std::max<INT16>(4, (width - previewSize - 10) / 5)),
				chosen ? FONT_WHITE : FONT_MCOLOR_LTGRAY);

			if (palette == OS0RealtimeEditorPalette::TILES)
			{
				auto const& entry = catalog().tiles[absolute];
				Print(cellX + previewSize + 8, cellY + 19,
					ST::format("#{} / {}x{}", entry.tileIndex,
						entry.footprintWidth, entry.footprintHeight),
					entry.placeable ? FONT_MCOLOR_DKGRAY :
						FONT_MCOLOR_RED);
			}
			else if (palette == OS0RealtimeEditorPalette::ITEMS)
			{
				auto const& entry = catalog().items[absolute];
				Print(cellX + previewSize + 8, cellY + 19,
					ST::format("#{} / W{}", entry.itemIndex, entry.weight),
					FONT_MCOLOR_DKGRAY);
			}
			else if (palette == OS0RealtimeEditorPalette::NPCS)
			{
				auto const& entry = catalog().npcTemplates[absolute];
				Print(cellX + previewSize + 8, cellY + 19,
					ST::format("TEAM {} / BODY {}", entry.team,
						entry.bodyType), FONT_MCOLOR_DKGRAY);
			}
			else
			{
				Print(cellX + previewSize + 8, cellY + 19,
					catalog().actions[absolute].explanation.left(24),
					FONT_MCOLOR_DKGRAY);
			}
		}

		Fill(bounds.x + 1, footerY, bounds.w - 2, 1, mutedRed);
		Print(bounds.x + MARGIN, footerY + 7, "<", FONT_MCOLOR_RED);
		Print(bounds.x + bounds.w - MARGIN - 8, footerY + 7, ">",
			FONT_MCOLOR_RED);
		const ST::string pageText = ST::format("{}/{}  {}  Q{}",
			pages[paletteIndex()] + 1, pageCount(),
			localStatus.left(25), OS0GetRealtimeEditor().pendingCount());
		Print(bounds.x + 50, footerY + 5, pageText.left(
			std::max<INT16>(8, (bounds.w - 100) / 5)), FONT_MCOLOR_DKGRAY);
		Print(bounds.x + MARGIN, footerY + 20,
			(hoverHelp.empty() ? "HOVER CONTROLS FOR ENGINE DETAILS" : hoverHelp)
				.left(std::max<INT16>(8, (bounds.w - MARGIN * 2) / 5)),
			hoverHelp.empty() ? FONT_MCOLOR_DKGRAY : FONT_MCOLOR_LTGRAY);

		InvalidateRegion(bounds.x, bounds.y, bounds.x + bounds.w,
			bounds.y + bounds.h);
	}
};

OS0RealtimeEditorUI::OS0RealtimeEditorUI()
	: impl_(std::make_unique<Impl>())
{
}

OS0RealtimeEditorUI::~OS0RealtimeEditorUI() = default;

void OS0RealtimeEditorUI::initialize(OS0WindowManager& manager,
	OS0WindowHandle const window)
{
	shutdown();
	impl_->manager = &manager;
	impl_->window = window;
	impl_->initializeRegions();
	impl_->update();
}

void OS0RealtimeEditorUI::shutdown() noexcept
{
	if (!impl_) return;
	if (impl_->manager && impl_->manager->draggingWindow() == impl_->window)
		impl_->manager->cancelDrag();
	impl_->removeRegions();
	if (impl_->preview)
	{
		DeleteVideoSurface(impl_->preview);
		impl_->preview = nullptr;
	}
	impl_->manager = nullptr;
	impl_->window = OS0_INVALID_WINDOW;
	impl_->catalogGeneration = 0;
	impl_->inputEnabled = TRUE;
	impl_->regionsActive = FALSE;
	impl_->hovered = nullptr;
	impl_->hoverHelp.clear();
	impl_->cancelBlankConfirmation();
}

void OS0RealtimeEditorUI::update()
{
	impl_->update();
}

void OS0RealtimeEditorUI::render()
{
	impl_->render();
}

BOOLEAN OS0RealtimeEditorUI::handleWorldClick(SOLDIERTYPE* const target,
	GridNo const gridNo, UINT8 const level, UINT16 const tileIndex)
{
	return impl_->handleWorldClick(target, gridNo, level, tileIndex);
}

BOOLEAN OS0RealtimeEditorUI::initialized() const noexcept
{
	return impl_->manager != nullptr &&
		impl_->window != OS0_INVALID_WINDOW;
}

BOOLEAN OS0RealtimeEditorUI::active() const noexcept
{
	return impl_->active();
}

BOOLEAN OS0RealtimeEditorUI::inputEnabled() const noexcept
{
	return impl_->inputEnabled;
}

OS0RealtimeEditorToolState OS0RealtimeEditorUI::toolState() const noexcept
{
	OS0RealtimeEditorToolState result;
	result.palette = impl_->palette;
	result.tool = impl_->tool;
	result.selectedIndex = impl_->selected[impl_->paletteIndex()];
	result.page = impl_->pages[impl_->paletteIndex()];
	result.blankMapConfirmationArmed = impl_->blankConfirmArmed;
	return result;
}

std::span<MOUSE_REGION* const>
OS0RealtimeEditorUI::mouseRegionsBackToFront() noexcept
{
	return impl_->zOrderedRegions;
}

void OS0RealtimeEditorUI::setPalette(
	OS0RealtimeEditorPalette const palette)
{
	impl_->setPalette(palette);
	impl_->syncRegions();
}

void OS0RealtimeEditorUI::setInputEnabled(BOOLEAN const enabled)
{
	if (impl_->inputEnabled == enabled) return;
	impl_->inputEnabled = enabled;
	if (impl_->active()) impl_->syncRegions();
}

void OS0RealtimeEditorUI::setTool(OS0RealtimeEditorTool const tool)
{
	impl_->setTool(tool);
	impl_->syncRegions();
}

OS0RealtimeEditorUI& OS0GetRealtimeEditorUI() noexcept
{
	static OS0RealtimeEditorUI ui;
	return ui;
}
