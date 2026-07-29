#pragma once

#include "JA2Types.h"

#include <array>
#include <cstddef>

class SGPVObject;
class SGPVSurface;

// Semantic names are the public UI contract. Renderers never need to know
// which legacy STI sheet or frame contains a symbol.
enum class OS0UIIcon : UINT8
{
	RUN,
	WALK,
	SNEAK,
	CRAWL,
	LOOK,
	CANCEL,
	HAND,
	TALK,
	TARGET,
	KNIFE,
	FIRST_AID,
	PUNCH,
	EXPLOSIVE,
	TOOLKIT,
	WIRE_CUTTER,
	CROWBAR,
	KEY,
	KEYRING,
	OPEN,
	EXAMINE,
	BREACH,
	DISARM,
	LOCKPICK,
	BOOT,
	COUNT
};

enum class OS0UIAssetSheet : UINT8
{
	TACTICAL_ACTIONS,
	DOOR_ACTIONS,
	BUTTON_FRAME,
	COUNT
};

struct OS0UIAssetSheetDescriptor
{
	OS0UIAssetSheet sheet;
	const char* path;
	UINT16 minimumFrames;
};

struct OS0UIIconDescriptor
{
	OS0UIIcon icon;
	OS0UIAssetSheet sheet;
	UINT16 frame;
	const char* label;
};

OS0UIAssetSheetDescriptor const& GetOS0UIAssetSheetDescriptor(
	OS0UIAssetSheet sheet) noexcept;
OS0UIIconDescriptor const& GetOS0UIIconDescriptor(OS0UIIcon icon) noexcept;

// One owner for UI art used by Escape from Arulco. It validates every known
// sheet at load time and supplies stable semantic drawing to every UI surface.
class OS0UIAssetManager
{
public:
	void initialize();
	void shutdown() noexcept;
	BOOLEAN initialized() const noexcept { return initialized_; }
	BOOLEAN has(OS0UIIcon icon) const noexcept;
	BOOLEAN draw(OS0UIIcon icon, SGPVSurface* destination,
		INT16 x, INT16 y) const;
	BOOLEAN drawFrame(OS0UIAssetSheet sheet, UINT16 frame,
		SGPVSurface* destination, INT16 x, INT16 y) const;
	SGPVObject* sheet(OS0UIAssetSheet sheet) const noexcept;

private:
	static size_t index(OS0UIAssetSheet sheet) noexcept;

	std::array<SGPVObject*, static_cast<size_t>(OS0UIAssetSheet::COUNT)>
		sheets_{};
	BOOLEAN initialized_ = FALSE;
};

OS0UIAssetManager& OS0UIAssets() noexcept;
