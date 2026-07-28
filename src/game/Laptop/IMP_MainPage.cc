/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "Directories.h"
#include "Font.h"
#include "HImage.h"
#include "IMP_MainPage.h"
#include "CharProfile.h"
#include "IMPVideoObjects.h"
#include "MessageBoxScreen.h"
#include "Overhead.h"
#include "Text.h"
#include "Video.h"
#include "VObject.h"
#include "VSurface.h"
#include "Cursors.h"
#include "Laptop.h"
#include "IMP_Attribute_Selection.h"
#include "IMP_Finish.h"
#include "MouseSystem.h"
#include "LaptopSave.h"
#include "IMP_Portraits.h"
#include "Button_System.h"
#include "ScreenIDs.h"
#include "Font_Control.h"
#include "GamePolicy.h"
#include "GameInstance.h"
#include "ContentManager.h"


#define MAIN_PAGE_BUTTON_TEXT_WIDTH 95

// main page buttons
static BUTTON_PICS* giIMPMainPageButtonImage[6];
GUIButtonRef giIMPMainPageButton[6];

extern INT32 iCurrentVoices;

// mouse regions for not entablable warning
static MOUSE_REGION pIMPMainPageMouseRegions[4];

static SGPVObject* guiCHARACTERPORTRAITFORMAINPAGE;

void BtnIMPMainPagePortraitCallback(GUI_BUTTON *btn, UINT32 reason);
void BtnIMPMainPageVoiceCallback(GUI_BUTTON *btn, UINT32 reason);


// this is the current state of profiling the player is in.
	/*
	0 - Beginning
	1 - Personnality
	2 - Attributes and Skills
	3 - Portrait
	4 - Voice
	5 - Done
	*/
INT32 iCurrentProfileMode = 0;


static void CreateIMPMainPageButtons(void);
static void CreateMouseRegionsForIMPMainPageBasedOnCharGenStatus(void);
static void LoadCharacterPortraitForMainPage(void);
static void UpDateIMPMainPageButtons(void);


void EnterIMPMainPage( void )
{

	// turn off review mode
	fReviewStats = FALSE;


	// create buttons
	CreateIMPMainPageButtons( );

	// load portrait for face button, if applicable
	LoadCharacterPortraitForMainPage( );

	// create button masks for this screen
	CreateMouseRegionsForIMPMainPageBasedOnCharGenStatus( );

	// alter states
	UpDateIMPMainPageButtons( );

	// entry into IMP about us page
	RenderIMPMainPage( );
}


static void DeleteIMPMainPageButtons(void);
static void DestoryMouseRegionsForIMPMainPageBasedOnCharGenStatus(void);


void ExitIMPMainPage( void )
{
	// exit from IMP About us page

	// delete Buttons
	DeleteIMPMainPageButtons( );
	DestoryMouseRegionsForIMPMainPageBasedOnCharGenStatus( );
}


void RenderIMPMainPage( void )
{
	// Escape from Arulco: OS//0 operator-creation landing page.
	RenderProfileBackGround( );
	const INT16 x = LAPTOP_SCREEN_UL_X;
	const INT16 y = LAPTOP_SCREEN_WEB_UL_Y;
	const UINT16 panel = Get16BPPColor(FROMRGB(12, 14, 16));
	const UINT16 red = Get16BPPColor(FROMRGB(168, 16, 22));
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 28, y + 48, x + 474, y + 145, panel);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 28, y + 48, x + 474, y + 49, red);
	SetFont(FONT12ARIAL);
	SetFontForeground(FONT_MCOLOR_RED);
	MPrint(x + 44, y + 64, "CREATE OPERATOR");
	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_DKGRAY);
	MPrint(x + 44, y + 88, "Configure identity, abilities, portrait and voice.");
	MPrint(x + 44, y + 104, "No account. No network. No recruitment service.");
}


static BOOLEAN CheckIfFinishedCharacterGeneration(void);


void HandleIMPMainPage( void )
{
	// handles the IMP about main page

	if ( CheckIfFinishedCharacterGeneration( ) )
	{
		iCurrentImpPage = IMP_FINISH;
	}
}


static void MakeButton(UINT idx, const char* /*img_file*/, const ST::string& text, INT16 x, INT16 y, GUI_CALLBACK click)
{
	giIMPMainPageButtonImage[idx] = nullptr;
	GUIButtonRef const btn = CreateTextButton(text, FONT12ARIAL, FONT_MCOLOR_RED, FONT_BLACK,
		x, y, 105, 48, MSYS_PRIORITY_HIGH, click);
	giIMPMainPageButton[idx] = btn;
	btn->SetCursor(CURSOR_WWW);
	btn->SpecifyHilitedTextColors(FONT_WHITE, FONT_BLACK);
}


static void BtnIMPMainPageAttributesCallback(GUI_BUTTON* btn, UINT32 reason);
static void BtnIMPMainPageBackCallback(GUI_BUTTON* btn, UINT32 reason);
static void BtnIMPMainPageBeginCallback(GUI_BUTTON* btn, UINT32 reason);
static void BtnIMPMainPagePersonalityCallback(GUI_BUTTON* btn, UINT32 reason);


static void CreateIMPMainPageButtons(void)
{
	// this function will create the buttons needed for th IMP about us page
	const INT16 dx = LAPTOP_SCREEN_UL_X;
	const INT16 dy = LAPTOP_SCREEN_WEB_UL_Y;

	// the back button button
	MakeButton(0, "", "RESET", dx + 28, dy + 330, BtnIMPMainPageBackCallback);

	// the begin profiling button
	ST::string profiling_text = (iCurrentProfileMode == 0 || iCurrentProfileMode > 2 ? "BEGIN" : "CONTINUE");
	MakeButton(1, "", profiling_text, dx + 198, dy + 168, BtnIMPMainPageBeginCallback);

	// the personality/specialties button
	MakeButton(2, "", "IDENTITY", dx + 28, dy + 250, BtnIMPMainPagePersonalityCallback);

	// the attribs button
	MakeButton(3, "", "ATTRIBUTES", dx + 140, dy + 250, BtnIMPMainPageAttributesCallback);

	// the portrait button
	MakeButton(4, "", "PORTRAIT", dx + 252, dy + 250, BtnIMPMainPagePortraitCallback);

	// the voice button
	MakeButton(5, "", "VOICE", dx + 364, dy + 250, BtnIMPMainPageVoiceCallback);
}


static void DeleteIMPMainPageButtons(void)
{
	// this function destroys the buttons needed for the IMP about Us Page

	// the back  button
	RemoveButton(giIMPMainPageButton[0] );

	// begin profiling button
	RemoveButton(giIMPMainPageButton[1] );

	// begin personna button
	RemoveButton(giIMPMainPageButton[2] );

	// begin attribs button
	RemoveButton(giIMPMainPageButton[3] );

	// begin portrait button
	RemoveButton(giIMPMainPageButton[4] );

	// begin voice button
	RemoveButton(giIMPMainPageButton[5] );
}


static void BtnIMPMainPageBackCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for IMP Homepage About US button

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		// There is no public IMP home page in Escape from Arulco.
		iCurrentImpPage = IMP_MAIN_PAGE;
		fButtonPendingFlag = TRUE;
		iCurrentProfileMode = 0;
		fFinishedCharGeneration = FALSE;
		ResetCharacterStats();
	}
}


static void BeginMessageBoxCallBack(MessageBoxReturnValue);


static void BtnIMPMainPageBeginCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for Main Page Begin Profiling

	// too far along to change gender

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		// are we going to change name, or do we have to start over from scratch
		if (iCurrentProfileMode > 2)
		{
			// too far along, restart
			DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[1], OS0_CREATOR_SCREEN, MSG_BOX_FLAG_YESNO, BeginMessageBoxCallBack);
		}
		else
		{
			if (LaptopSaveInfo.iCurrentBalance < COST_OF_PROFILE)
			{
				DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[3], OS0_CREATOR_SCREEN, MSG_BOX_FLAG_OK, BeginMessageBoxCallBack);
			}
			else if (NumberOfMercsOnPlayerTeam() >= 18)
			{
				DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[5], OS0_CREATOR_SCREEN, MSG_BOX_FLAG_OK, BeginMessageBoxCallBack);
			}
			else
			{
				// change name
				iCurrentImpPage = IMP_BEGIN;
				fButtonPendingFlag = TRUE;
			}
		}
	}
}


static void BtnIMPMainPagePersonalityCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for Main Page Begin Profiling

	// if not this far in char generation, don't alot ANY action
	if (iCurrentProfileMode != 1)
	{
		btn->uiFlags &= ~BUTTON_CLICKED_ON;
		return;
	}

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		iCurrentImpPage = IMP_PERSONALITY;
		fButtonPendingFlag = TRUE;
	}
}


static void BtnIMPMainPageAttributesCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for Main Page Begin Profiling

	// if not this far in char generation, don't alot ANY action
	if (iCurrentProfileMode < 2)
	{
		btn->uiFlags &= ~BUTTON_CLICKED_ON;
		return;
	}

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		iCurrentImpPage = IMP_ATTRIBUTE_ENTRANCE;
		fButtonPendingFlag = TRUE;
	}
}


void BtnIMPMainPagePortraitCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for Main Page Begin Profiling

	// if not this far in char generation, don't alot ANY action
	if (iCurrentProfileMode != 3 && iCurrentProfileMode != 4 && iCurrentProfileMode > 5)
	{
		btn->uiFlags&= ~BUTTON_CLICKED_ON;
		return;
	}

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		iCurrentImpPage = IMP_PORTRAIT;
		fButtonPendingFlag = TRUE;
	}
}


void BtnIMPMainPageVoiceCallback(GUI_BUTTON *btn, UINT32 reason)
{
	// btn callback for Main Page Begin Profiling

	// if not this far in char generation, don't alot ANY action
	if (iCurrentProfileMode != 4 && iCurrentProfileMode > 5)
	{
		btn->uiFlags&= ~BUTTON_CLICKED_ON;
		return;
	}

	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		iCurrentImpPage = IMP_VOICE;
		fButtonPendingFlag = TRUE;
	}
}


static BOOLEAN CheckIfFinishedCharacterGeneration(void)
{
	// this function checks to see if character is done character generation

	// are we done character generation
	if( iCurrentProfileMode == 5 )
	{
		// yes
		return ( TRUE );
	}
	else
	{
		// no
		return ( FALSE );
	}
}


static void UpDateIMPMainPageButtons(void)
{
	// update mainpage button states
	INT32 iCount = 0;

	// disable all
	for( iCount = 2; iCount < 6; iCount++)
	{
		DisableButton( giIMPMainPageButton[ iCount ] );
	}

	for( iCount = 0; iCount < 4; iCount++ )
	{
		pIMPMainPageMouseRegions[iCount].Disable();
	}
	// enable
	switch(  iCurrentProfileMode )
	{
		case 0:
			pIMPMainPageMouseRegions[0].Enable();
			pIMPMainPageMouseRegions[1].Enable();
			pIMPMainPageMouseRegions[2].Enable();
			pIMPMainPageMouseRegions[3].Enable();
			break;
		case( 1 ):
			EnableButton( giIMPMainPageButton[2] );
			pIMPMainPageMouseRegions[1].Enable();
			pIMPMainPageMouseRegions[2].Enable();
			pIMPMainPageMouseRegions[3].Enable();
			break;
		case( 2 ):
			EnableButton( giIMPMainPageButton[3] );
			pIMPMainPageMouseRegions[0].Enable();
			pIMPMainPageMouseRegions[2].Enable();
			pIMPMainPageMouseRegions[3].Enable();
			break;
		case( 3 ):
			EnableButton( giIMPMainPageButton[3] );
			EnableButton( giIMPMainPageButton[4] );
			pIMPMainPageMouseRegions[0].Enable();
			//pIMPMainPageMouseRegions[1].Enable();
			pIMPMainPageMouseRegions[3].Enable();
			break;
		case( 4 ):
			//pIMPMainPageMouseRegions[1].Enable();
			pIMPMainPageMouseRegions[0].Enable();
			EnableButton( giIMPMainPageButton[3] );
			EnableButton( giIMPMainPageButton[4] );
			EnableButton( giIMPMainPageButton[5] );
			break;
	}
}


static void BeginMessageBoxCallBack(MessageBoxReturnValue const bExitValue)
{
	// yes, so start over, else stay here and do nothing for now
	if( bExitValue == MSG_BOX_RETURN_YES )
	{
		iCurrentImpPage = IMP_BEGIN;
		iCurrentProfileMode = 0;
	}

	else if( bExitValue == MSG_BOX_RETURN_OK )
	{
		// if ok, then we are coming from financial warning, allow continue
	}
}


static void IMPMainPageNotSelectableBtnCallback(MOUSE_REGION* pRegion, UINT32 iReason);


static void CreateMouseRegionsForIMPMainPageBasedOnCharGenStatus(void)
{
	// this procedure will create masks for the char generation main page
	// create masks for the personality, attrib, portrait and page buttons on the
	// character generation main page
	UINT16       x = LAPTOP_SCREEN_UL_X     +  13;
	UINT16 const y = LAPTOP_SCREEN_WEB_UL_Y + 245;
	UINT16 const w = 115;
	UINT16 const h =  93;
	for (auto & r : pIMPMainPageMouseRegions)
	{
		MSYS_DefineRegion(&r, x, y, x + w, y + h, MSYS_PRIORITY_HIGH + 5, CURSOR_WWW, MSYS_NO_CALLBACK, IMPMainPageNotSelectableBtnCallback);
		x += 120;
	}
}


static void DestoryMouseRegionsForIMPMainPageBasedOnCharGenStatus(void)
{
	// will destroy button masks for the char gen pages
	FOR_EACH(MOUSE_REGION, r, pIMPMainPageMouseRegions)
	{
		MSYS_RemoveRegion(r);
	}
}


static void IMPMainPageNotSelectableBtnCallback(MOUSE_REGION* pRegion, UINT32 iReason)
{
	if(iReason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		DoLapTopMessageBox( MSG_BOX_IMP_STYLE, pImpPopUpStrings[ 4 ], OS0_CREATOR_SCREEN, MSG_BOX_FLAG_OK, BeginMessageBoxCallBack);
	}
}


SGPVObject* LoadIMPPortait()
{
	ST::string filename = ST::format(FACESDIR "/{}.sti", 200 + iPortraitNumber);
	return AddVideoObjectFromFile(filename);
}


static void LoadCharacterPortraitForMainPage(void)
{
	// this function will load the character's portrait, to be used on portrait button
	if( iCurrentProfileMode >= 4 )
	{
		guiCHARACTERPORTRAITFORMAINPAGE = LoadIMPPortait();
		giIMPMainPageButton[4]->SpecifyIcon(guiCHARACTERPORTRAITFORMAINPAGE, 0, 33, 23, FALSE);
	}
}
