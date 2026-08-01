/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella on 2026-07-28.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#ifndef __RADAR_SCREEN_H
#define __RADAR_SCREEN_H

#include "Types.h"

class SGPVSurface;


void LoadRadarScreenBitmap(const ST::string&);

// RADAR WINDOW DEFINES
#define RADAR_WINDOW_X		(g_ui.get_RADAR_WINDOW_X())
#define RADAR_WINDOW_TM_Y	(g_ui.get_RADAR_WINDOW_TM_Y())
#define RADAR_WINDOW_WIDTH	88
#define RADAR_WINDOW_HEIGHT	44

void InitRadarScreen(void);
void RenderRadarScreen(void);
void MoveRadarScreen(void);

// OS//0 owns radar and squad presentation while the live tactical screen is
// active.  This is an explicit ownership gate rather than a render-time hide:
// suppressed legacy code may neither draw nor recreate mouse regions.
void SetLegacyRadarScreenSuppressed(BOOLEAN suppressed);
BOOLEAN IsLegacyRadarScreenSuppressed();

// Draw only the current sector artwork at an arbitrary screen position.  OS//0
// uses this to host the real JA2 radar asset inside its movable field computer
// without reviving the legacy fixed tactical panel.
BOOLEAN BlitRadarScreenImage(SGPVSurface* destination, INT16 x, INT16 y);

// toggle rendering flag of radar screen
void ToggleRadarScreenRender( void );

// clear out the video object for the radar map
void ClearOutRadarMapImage( void );

// do we render the radar screen?..or the squad list?
extern BOOLEAN   fRenderRadarScreen;

#endif
