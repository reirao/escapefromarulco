/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "OS0_CreatorScreen.h"

#include "Button_System.h"
#include "CharProfile.h"
#include "Cursor_Control.h"
#include "Cursors.h"
#include "Event_Pump.h"
#include "HImage.h"
#include "Input.h"
#include "Laptop.h"
#include "Render_Dirty.h"
#include "Video.h"
#include "VObject.h"
#include "VSurface.h"


namespace
{
	BOOLEAN gEntered = FALSE;
	BOOLEAN gExitRequested = FALSE;
}


void OS0CreatorScreenInit()
{
	gEntered = FALSE;
	gExitRequested = FALSE;
}


void RequestOS0CreatorExit()
{
	gExitRequested = TRUE;
}


ScreenID OS0CreatorScreenHandle()
{
	if (!gEntered)
	{
		FRAME_BUFFER->Fill(Get16BPPColor(FROMRGB(0, 0, 0)));
		InitIMPSubPageList();
		iCurrentImpPage = IMP_MAIN_PAGE;
		fButtonPendingFlag = FALSE;
		fReDrawCharProfile = TRUE;
		fLoadPendingFlag = FALSE;
		fDoneLoadPending = FALSE;
		fConnectingToSubPage = FALSE;
		fFastLoadFlag = FALSE;
		EnterCharProfile();
		SetCurrentCursorFromDatabase(CURSOR_WWW);
		InvalidateScreen();
		gEntered = TRUE;
	}

	RestoreBackgroundRects();
	DequeAllGameEvents();

	// The browser used a fake download delay between IMP pages.  Native
	// creator pages switch immediately.
	if (fLoadPendingFlag)
	{
		fLoadPendingFlag = FALSE;
		fDoneLoadPending = TRUE;
		fConnectingToSubPage = FALSE;
		fFastLoadFlag = FALSE;
	}

	HandleCharProfile();
	MarkButtonsDirty();
	RenderButtons();
	SaveBackgroundRects();

	if (gExitRequested && gEntered)
	{
		ExitCharProfile();
		gEntered = FALSE;
		gExitRequested = FALSE;
		FRAME_BUFFER->Fill(Get16BPPColor(FROMRGB(0, 0, 0)));
		InvalidateScreen();
	}

	return OS0_CREATOR_SCREEN;
}


void OS0CreatorScreenShutdown()
{
	if (gEntered)
	{
		ExitCharProfile();
		gEntered = FALSE;
	}
}
