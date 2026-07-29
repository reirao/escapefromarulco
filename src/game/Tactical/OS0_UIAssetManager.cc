/*
 * Escape from Arulco: central, semantic access to original JA2 UI artwork.
 */

#include "OS0_UIAssetManager.h"

#include "Directories.h"
#include "VObject.h"

#include <stdexcept>
#include <string_theory/format>

namespace
{
	constexpr std::array<OS0UIAssetSheetDescriptor,
		static_cast<size_t>(OS0UIAssetSheet::COUNT)> SHEETS{{
		{ OS0UIAssetSheet::TACTICAL_ACTIONS,
			INTERFACEDIR "/newicons3.sti", 45 },
		{ OS0UIAssetSheet::DOOR_ACTIONS,
			INTERFACEDIR "/door_op2.sti", 28 },
		{ OS0UIAssetSheet::BUTTON_FRAME,
			INTERFACEDIR "/button_frame.sti", 1 }
	}};

	// Frames match InitializeTacticalInterface: each base frame is the normal
	// state of an original JA2 action icon, not a guessed bitmap index.
	constexpr std::array<OS0UIIconDescriptor,
		static_cast<size_t>(OS0UIIcon::COUNT)> ICONS{{
		{ OS0UIIcon::RUN, OS0UIAssetSheet::TACTICAL_ACTIONS, 0, "RUN" },
		{ OS0UIIcon::WALK, OS0UIAssetSheet::TACTICAL_ACTIONS, 3, "WALK" },
		{ OS0UIIcon::SNEAK, OS0UIAssetSheet::TACTICAL_ACTIONS, 6, "SNEAK" },
		{ OS0UIIcon::CRAWL, OS0UIAssetSheet::TACTICAL_ACTIONS, 9, "CRAWL" },
		{ OS0UIIcon::LOOK, OS0UIAssetSheet::TACTICAL_ACTIONS, 12, "LOOK" },
		{ OS0UIIcon::CANCEL, OS0UIAssetSheet::TACTICAL_ACTIONS, 15, "CANCEL" },
		{ OS0UIIcon::HAND, OS0UIAssetSheet::TACTICAL_ACTIONS, 18, "HAND" },
		{ OS0UIIcon::TALK, OS0UIAssetSheet::TACTICAL_ACTIONS, 21, "TALK" },
		{ OS0UIIcon::TARGET, OS0UIAssetSheet::TACTICAL_ACTIONS, 24, "TARGET" },
		{ OS0UIIcon::KNIFE, OS0UIAssetSheet::TACTICAL_ACTIONS, 27, "KNIFE" },
		{ OS0UIIcon::FIRST_AID, OS0UIAssetSheet::TACTICAL_ACTIONS, 30, "FIRST AID" },
		{ OS0UIIcon::PUNCH, OS0UIAssetSheet::TACTICAL_ACTIONS, 33, "PUNCH" },
		{ OS0UIIcon::EXPLOSIVE, OS0UIAssetSheet::TACTICAL_ACTIONS, 36, "EXPLOSIVE" },
		{ OS0UIIcon::TOOLKIT, OS0UIAssetSheet::TACTICAL_ACTIONS, 39, "TOOLKIT" },
		{ OS0UIIcon::WIRE_CUTTER, OS0UIAssetSheet::TACTICAL_ACTIONS, 42, "WIRE CUTTER" },
		{ OS0UIIcon::CROWBAR, OS0UIAssetSheet::DOOR_ACTIONS, 0, "CROWBAR" },
		{ OS0UIIcon::KEY, OS0UIAssetSheet::DOOR_ACTIONS, 3, "KEY" },
		{ OS0UIIcon::KEYRING, OS0UIAssetSheet::DOOR_ACTIONS, 6, "KEYRING" },
		{ OS0UIIcon::OPEN, OS0UIAssetSheet::DOOR_ACTIONS, 9, "OPEN" },
		{ OS0UIIcon::EXAMINE, OS0UIAssetSheet::DOOR_ACTIONS, 12, "EXAMINE" },
		{ OS0UIIcon::BREACH, OS0UIAssetSheet::DOOR_ACTIONS, 15, "BREACH" },
		{ OS0UIIcon::DISARM, OS0UIAssetSheet::DOOR_ACTIONS, 18, "DISARM" },
		{ OS0UIIcon::LOCKPICK, OS0UIAssetSheet::DOOR_ACTIONS, 21, "LOCKPICK" },
		{ OS0UIIcon::BOOT, OS0UIAssetSheet::DOOR_ACTIONS, 25, "BOOT" }
	}};

	OS0UIAssetManager gAssetManager;
}

OS0UIAssetSheetDescriptor const& GetOS0UIAssetSheetDescriptor(
	OS0UIAssetSheet sheet) noexcept
{
	const size_t index = static_cast<size_t>(sheet);
	return SHEETS[index < SHEETS.size() ? index : 0];
}

OS0UIIconDescriptor const& GetOS0UIIconDescriptor(OS0UIIcon icon) noexcept
{
	const size_t index = static_cast<size_t>(icon);
	return ICONS[index < ICONS.size() ? index : 0];
}

size_t OS0UIAssetManager::index(OS0UIAssetSheet sheet) noexcept
{
	const size_t value = static_cast<size_t>(sheet);
	return value < static_cast<size_t>(OS0UIAssetSheet::COUNT) ? value : 0;
}

void OS0UIAssetManager::initialize()
{
	if (initialized_) return;
	try
	{
		for (OS0UIAssetSheetDescriptor const& descriptor : SHEETS)
		{
			SGPVObject* const object = AddVideoObjectFromFile(descriptor.path);
			if (!object || object->SubregionCount() < descriptor.minimumFrames)
			{
				const UINT16 actualFrames = object ? object->SubregionCount() : 0;
				if (object) DeleteVideoObject(object);
				throw std::runtime_error(ST::format(
					"OS//0 asset '{}' has {} frames; expected at least {}",
					descriptor.path, actualFrames,
					descriptor.minimumFrames).c_str());
			}
			sheets_[index(descriptor.sheet)] = object;
		}
		initialized_ = TRUE;
	}
	catch (...)
	{
		shutdown();
		throw;
	}
}

void OS0UIAssetManager::shutdown() noexcept
{
	for (SGPVObject*& sheet : sheets_)
	{
		if (sheet) DeleteVideoObject(sheet);
		sheet = nullptr;
	}
	initialized_ = FALSE;
}

SGPVObject* OS0UIAssetManager::sheet(OS0UIAssetSheet sheetId) const noexcept
{
	return sheets_[index(sheetId)];
}

BOOLEAN OS0UIAssetManager::has(OS0UIIcon icon) const noexcept
{
	if (icon == OS0UIIcon::COUNT) return FALSE;
	OS0UIIconDescriptor const& descriptor = GetOS0UIIconDescriptor(icon);
	SGPVObject const* const object = sheet(descriptor.sheet);
	return object && descriptor.frame < object->SubregionCount();
}

BOOLEAN OS0UIAssetManager::draw(OS0UIIcon icon, SGPVSurface* destination,
	INT16 x, INT16 y) const
{
	if (!destination || !has(icon)) return FALSE;
	OS0UIIconDescriptor const& descriptor = GetOS0UIIconDescriptor(icon);
	BltVideoObject(destination, sheet(descriptor.sheet), descriptor.frame, x, y);
	return TRUE;
}

BOOLEAN OS0UIAssetManager::drawFrame(OS0UIAssetSheet sheetId, UINT16 frame,
	SGPVSurface* destination, INT16 x, INT16 y) const
{
	SGPVObject const* const object = sheet(sheetId);
	if (!destination || !object || frame >= object->SubregionCount()) return FALSE;
	BltVideoObject(destination, object, frame, x, y);
	return TRUE;
}

OS0UIAssetManager& OS0UIAssets() noexcept
{
	return gAssetManager;
}
