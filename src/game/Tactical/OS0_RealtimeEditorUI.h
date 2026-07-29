#pragma once

#include "JA2Types.h"

#include <cstddef>
#include <memory>
#include <span>

class OS0WindowManager;
struct MOUSE_REGION;
struct SOLDIERTYPE;

using OS0WindowHandle = UINT8;

enum class OS0RealtimeEditorPalette : UINT8
{
	TILES,
	ITEMS,
	NPCS,
	SYSTEM,
	COUNT
};

enum class OS0RealtimeEditorTool : UINT8
{
	SELECT,
	PLACE,
	ERASE,
	COUNT
};

// This is presentation state only.  Catalog entries remain owned by
// OS0RealtimeEditorSession and world objects remain owned by JA2.
struct OS0RealtimeEditorToolState
{
	OS0RealtimeEditorPalette palette = OS0RealtimeEditorPalette::TILES;
	OS0RealtimeEditorTool tool = OS0RealtimeEditorTool::SELECT;
	std::size_t selectedIndex = 0;
	std::size_t page = 0;
	BOOLEAN blankMapConfirmationArmed = FALSE;
};

// Mobile tactical presentation for the canonical realtime-editor session.
// It owns mouse regions and a small preview surface, but no map, item, soldier,
// structure or command queue.
class OS0RealtimeEditorUI
{
public:
	OS0RealtimeEditorUI();
	~OS0RealtimeEditorUI();

	OS0RealtimeEditorUI(OS0RealtimeEditorUI const&) = delete;
	OS0RealtimeEditorUI& operator=(OS0RealtimeEditorUI const&) = delete;

	void initialize(OS0WindowManager& manager, OS0WindowHandle window);
	void shutdown() noexcept;

	// Call once per tactical frame.  This synchronizes regions with the shared
	// window manager; it never executes queued engine commands.
	void update();
	void render();

	// Called by tactical world input after screen-to-grid resolution.  The
	// supplied tile index is used for SELECT/ERASE and is never cached as a
	// LEVELNODE/STRUCTURE pointer.
	BOOLEAN handleWorldClick(SOLDIERTYPE* target, GridNo gridNo, UINT8 level,
		UINT16 tileIndex);

	BOOLEAN initialized() const noexcept;
	BOOLEAN active() const noexcept;
	BOOLEAN inputEnabled() const noexcept;
	OS0RealtimeEditorToolState toolState() const noexcept;
	// Native child regions in local back-to-front order. The tactical shell uses
	// this projection to keep manager drawing and JA2 hit testing in the same Z.
	std::span<MOUSE_REGION* const> mouseRegionsBackToFront() noexcept;

	// Rendering/visibility remains owned by the window manager.  The tactical
	// shell can independently yield input to a modal/context surface without
	// destroying the editor's requested visibility or selection.
	void setInputEnabled(BOOLEAN enabled);
	void setPalette(OS0RealtimeEditorPalette palette);
	void setTool(OS0RealtimeEditorTool tool);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

OS0RealtimeEditorUI& OS0GetRealtimeEditorUI() noexcept;
