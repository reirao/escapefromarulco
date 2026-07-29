/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-29.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "CharProfile.h"
#include "ContentManager.h"
#include "Cursors.h"
#include "Directories.h"
#include "Font_Control.h"
#include "Font.h"
#include "GameInstance.h"
#include "GamePolicy.h"
#include "HImage.h"
#include "IMP_Begin_Screen.h"
#include "IMP_MainPage.h"
#include "IMPVideoObjects.h"
#include "Laptop.h"
#include "LaptopSave.h"
#include "Line.h"
#include "SaveLoadGame.h"
#include "Soldier_Profile_Type.h"
#include "Soldier_Profile.h"
#include "IMP_Attribute_Selection.h"
#include "IMP_Compile_Character.h"
#include "IMP_Portraits.h"
#include "Text_Input.h"
#include "Text.h"
#include "Timer_Control.h"
#include "Video.h"
#include "VObject.h"
#include "VSurface.h"

#include <string_theory/string>


#define FULL_NAME_INPUT_X LAPTOP_SCREEN_UL_X + 196
#define FULL_NAME_INPUT_Y LAPTOP_SCREEN_UL_Y + 153
#define FULL_NAME_INPUT_WIDTH 229
#define NICK_NAME_INPUT_X FULL_NAME_INPUT_X
#define NICK_NAME_INPUT_Y LAPTOP_SCREEN_UL_Y + 213
#define NICK_NAME_INPUT_WIDTH 110
#define NAMES_INPUT_HEIGHT 23
#define MALE_BOX_X 2 + 192 + LAPTOP_SCREEN_UL_X
#define MALE_BOX_Y 254 + LAPTOP_SCREEN_WEB_UL_Y
#define MALE_BOX_WIDTH 24 - 2
#define MALE_BOX_HEIGHT 24 - 2
#define FEMALE_BOX_X  2 + 302 + LAPTOP_SCREEN_UL_X
constexpr std::size_t MAX_NICKNAME_LENGTH = 8;

// genders
enum {
	IMP_FEMALE = 0,
	IMP_MALE,
};

// TextEnterMode .. whether user is entering full name or nick name, or gender selection
enum {
	OTHER_INPUT,
	MALE_GENDER,
	FEMALE_GENDER
};

// beginning character stats
ST::string pFullNameString;
ST::string pNickNameString;


// non gender
INT8 bGenderFlag = -1;

// IMP begin page buttons
static BUTTON_PICS* giIMPBeginScreenButtonImage[1];
GUIButtonRef giIMPBeginScreenButton[1];

// currently focused input
UINT8 ubFocus = 0;

static MOUSE_REGION gIMPBeginScreenMouseRegions[2];

static void CreateIMPBeginScreenButtons(void);
static void CreateIMPBeginScreenMouseRegions(void);

static void InvalidateCheckboxes() {
	InvalidateRegion(MALE_BOX_X, MALE_BOX_Y,  MALE_BOX_X + MALE_BOX_WIDTH + 1, MALE_BOX_Y + MALE_BOX_HEIGHT + 1);
	InvalidateRegion(FEMALE_BOX_X, MALE_BOX_Y,  FEMALE_BOX_X + MALE_BOX_WIDTH + 1, MALE_BOX_Y + MALE_BOX_HEIGHT + 1);

}

static void MaleCheckboxCallback(UINT8 ubID, BOOLEAN fEntering)
{
	ubFocus = fEntering ? MALE_GENDER : OTHER_INPUT;
	InvalidateCheckboxes();
}

static void FemaleCheckboxCallback(UINT8 ubID, BOOLEAN fEntering)
{
	ubFocus = fEntering ? FEMALE_GENDER : OTHER_INPUT;
	InvalidateCheckboxes();
}

void InitImpBeginScreeenTextInputBoxes() {
	InitTextInputMode();

	SetTextInputFont(FONT14ARIAL);
	Set16BPPTextFieldColor( Get16BPPColor(FROMRGB( 0, 0, 0) ) );
	SetTextInputRegularColors( FONT_LTGREEN, FONT_BLACK );
	SetTextInputHilitedColors( FONT_BLACK, FONT_LTGREEN, FONT_LTGREEN  );
	SetCursorColor( Get16BPPColor(FROMRGB(0, 255, 0) ) );

	AddTextInputField(
		FULL_NAME_INPUT_X,
		FULL_NAME_INPUT_Y,
		FULL_NAME_INPUT_WIDTH,
		NAMES_INPUT_HEIGHT,
		MSYS_PRIORITY_HIGH + 2,
		pFullName,
		NAME_LENGTH-1,
		INPUTTYPE_FULL_TEXT
	);

	AddTextInputField(
		NICK_NAME_INPUT_X,
		NICK_NAME_INPUT_Y,
		NICK_NAME_INPUT_WIDTH,
		NAMES_INPUT_HEIGHT,
		MSYS_PRIORITY_HIGH + 2,
		pNickName,
		MAX_NICKNAME_LENGTH,
		INPUTTYPE_FULL_TEXT
	);

	AddUserInputField(MaleCheckboxCallback);
	AddUserInputField(FemaleCheckboxCallback);
}

void EnterIMPBeginScreen( void )
{
	InitImpBeginScreeenTextInputBoxes();

	bGenderFlag = iCurrentProfileMode != 0 ? fCharacterIsMale : -1;

	ubFocus = OTHER_INPUT;

	// render the screen on entry
	RenderIMPBeginScreen( );

	fFirstIMPAttribTime = TRUE;

	// create mouse regions
	CreateIMPBeginScreenMouseRegions( );

	// create buttons needed for begin screen
	CreateIMPBeginScreenButtons( );
}


static void Print8CharacterOnlyString(void);
static void RenderGender(void);


void RenderIMPBeginScreen( void )
{
	// OS//0 identity panel.  The legacy IMP artwork is deliberately not drawn.
	RenderProfileBackGround( );
	const INT16 x = LAPTOP_SCREEN_UL_X;
	const INT16 y = LAPTOP_SCREEN_WEB_UL_Y;
	const UINT16 panel = Get16BPPColor(FROMRGB(12, 14, 16));
	const UINT16 red = Get16BPPColor(FROMRGB(168, 16, 22));
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 28, y + 44, x + 474, y + 300, panel);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, x + 28, y + 44, x + 474, y + 45, red);
	SetFont(FONT12ARIAL);
	SetFontForeground(FONT_MCOLOR_RED);
	MPrint(x + 44, y + 62, "OPERATOR IDENTITY");
	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_DKGRAY);
	MPrint(x + 84, y + 126, "LEGAL NAME");
	MPrint(x + 84, y + 186, "CALLSIGN");
	MPrint(x + 84, y + 246, "BODY PROFILE");
	MPrint(x + 222, y + 246, "MALE");
	MPrint(x + 332, y + 246, "FEMALE");

	RenderGender();
	RenderAllTextFields();
}


static void DestroyIMPBeginScreenMouseRegions();
static void RemoveIMPBeginScreenButtons(void);


void ExitIMPBeginScreen( void )
{
	// remove buttons
	RemoveIMPBeginScreenButtons( );

	// remove mouse regions
	DestroyIMPBeginScreenMouseRegions( );

	KillTextInputMode();

	pFullName = pFullNameString;
	pNickName = pNickNameString;

	// set gender
	fCharacterIsMale = bGenderFlag;
}


static void DisplayFemaleCheckboxFocus(void);
static void DisplayMaleCheckboxFocus(void);
static void GetPlayerKeyBoardInputForIMPBeginScreen(void);


void HandleIMPBeginScreen( void )
{

	GetPlayerKeyBoardInputForIMPBeginScreen( );

	RenderGender();

	// render the cursor
	switch (ubFocus)
	{
		case MALE_GENDER:
			DisplayMaleCheckboxFocus();       break;
		case FEMALE_GENDER:
			DisplayFemaleCheckboxFocus();     break;
		default: break;
	}

	RenderAllTextFields();
}


static void BtnIMPBeginScreenDoneCallback(GUI_BUTTON* btn, UINT32 reason);


static void CreateIMPBeginScreenButtons(void)
{
	// this procedure will create the buttons needed for the IMP BeginScreen

	// ths done button
	giIMPBeginScreenButtonImage[0] = nullptr;
	giIMPBeginScreenButton[0] = CreateTextButton("CONFIRM IDENTITY", FONT12ARIAL,
		FONT_MCOLOR_RED, FONT_BLACK, LAPTOP_SCREEN_UL_X + 198, LAPTOP_SCREEN_WEB_UL_Y + 314,
		150, 48, MSYS_PRIORITY_HIGH, BtnIMPBeginScreenDoneCallback);
	giIMPBeginScreenButton[0]->SpecifyHilitedTextColors(FONT_WHITE, FONT_BLACK);

	giIMPBeginScreenButton[0]->SetCursor(CURSOR_WWW);
}


static void RemoveIMPBeginScreenButtons(void)
{
	// this procedure will destroy the already created buttosn for the IMP BeginScreen

	// the done button
	RemoveButton(giIMPBeginScreenButton[0] );
}


static void BtnIMPBeginScreenDoneCallback(GUI_BUTTON *btn, UINT32 reason)
{
	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		// back to mainpage
		pFullNameString = GetStringFromField(0).trim();
		pNickNameString = GetStringFromField(1).trim();

		// check to see if a name has been selected, if not, do not allow player to proceed with more char generation
		if (!pFullNameString.empty() && bGenderFlag != -1)
		{
			// valid full name, check to see if nick name
			if (pNickNameString.empty())
			{
				// no nick name
				// copy first name to nick name
				pNickNameString = pFullNameString.before_first(' ').left(MAX_NICKNAME_LENGTH);
			}
			// ok, now set back to main page, and set the fact we have completed part 1
			if (iCurrentProfileMode < 1 && bGenderFlag != -1)
			{
				iCurrentProfileMode = 1;
			}
			else if (bGenderFlag == -1)
			{
				iCurrentProfileMode = 0;
			}

			iCurrentImpPage = IMP_MAIN_PAGE;
			fButtonPendingFlag = TRUE;
		}
		else if (GCM->getGamePolicy()->imp_load_saved_merc_by_nickname && IMPSavedProfileDoesFileExist(pNickNameString))
		{
			fLoadingCharacterForPreviousImpProfile = true;
			LaptopSaveInfo.iVoiceId = IMPSavedProfileLoadMercProfile(pNickNameString);
			MERCPROFILESTRUCT& profile_saved = gMercProfiles[PLAYER_GENERATED_CHARACTER_ID + LaptopSaveInfo.iVoiceId];
			iPortraitNumber = profile_saved.ubFaceIndex - 200;
			fCharacterIsMale = ( profile_saved.bSex == MALE );
			iCurrentImpPage = IMP_CONFIRM;
			fButtonPendingFlag = TRUE;
		}
		else
		{
			// invalid name, reset current mode
			DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[2], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
			iCurrentProfileMode = 0;
		}
	}
}

static void GetPlayerKeyBoardInputForIMPBeginScreen(void)
{
	InputAtom InputEvent;

	// handle input events
	while( DequeueSpecificEvent(&InputEvent, KEYBOARD_EVENTS) )
	{
		if(!HandleTextInput( &InputEvent ) && (InputEvent.usEvent == KEY_DOWN || InputEvent.usEvent == KEY_REPEAT) )
		{
			switch( InputEvent.usParam )
			{
				case SDLK_RETURN:
				case SDLK_SPACE:
					if (ubFocus != OTHER_INPUT) {
						bGenderFlag = ubFocus == MALE_GENDER ? IMP_MALE : IMP_FEMALE;
					}
					SetActiveField(0);
					break;

				case SDLK_ESCAPE:
					HandleLapTopESCKey();
					break;

				default:
					break;
			}
		}
	}
}

static void DisplayCheckboxFocus(INT32 x)
{
	UINT16 currentColor = Get16BPPColor(GetJA2Clock() % 1000 < TEXT_CURSOR_BLINK_INTERVAL ? FROMRGB(0, 255, 0) : FROMRGB(0, 0, 0));
	SGPVSurface::Lock l(FRAME_BUFFER);
	SetClippingRegionAndImageWidth(l.Pitch(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	RectangleDraw(TRUE, x, MALE_BOX_Y, x + MALE_BOX_WIDTH, MALE_BOX_Y + MALE_BOX_HEIGHT, currentColor, l.Buffer<UINT16>());
	InvalidateRegion(x, MALE_BOX_Y,  x + MALE_BOX_WIDTH + 1, MALE_BOX_Y + MALE_BOX_HEIGHT + 1);
}

static void DisplayMaleCheckboxFocus(void)
{
	DisplayCheckboxFocus(MALE_BOX_X);
}


static void DisplayFemaleCheckboxFocus(void)
{
	DisplayCheckboxFocus(FEMALE_BOX_X);
}


static void SelectFemaleRegionCallBack(MOUSE_REGION* pRegion, UINT32 iReason);
static void SelectMaleRegionCallBack(MOUSE_REGION* pRegion, UINT32 iReason);

static void CreateIMPBeginScreenMouseRegions(void)
{
	// IMP_MALE gender area
	MSYS_DefineRegion(
		&gIMPBeginScreenMouseRegions[0],
		MALE_BOX_X,
		MALE_BOX_Y,
		MALE_BOX_X + MALE_BOX_WIDTH,
		MALE_BOX_Y + MALE_BOX_HEIGHT,
		MSYS_PRIORITY_HIGH,
		CURSOR_WWW,
		MSYS_NO_CALLBACK,
		SelectMaleRegionCallBack
	);

	// IMP_FEMALE gender region
	MSYS_DefineRegion(
		&gIMPBeginScreenMouseRegions[1],
		FEMALE_BOX_X,
		MALE_BOX_Y,
		FEMALE_BOX_X + MALE_BOX_WIDTH,
		MALE_BOX_Y + MALE_BOX_HEIGHT,
		MSYS_PRIORITY_HIGH,
		CURSOR_WWW,
		MSYS_NO_CALLBACK,
		SelectFemaleRegionCallBack
	);
}


static void DestroyIMPBeginScreenMouseRegions()
{
	FOR_EACH(MOUSE_REGION, i, gIMPBeginScreenMouseRegions) MSYS_RemoveRegion(&*i);
}

static void SelectMaleRegionCallBack(MOUSE_REGION* pRegion, UINT32 iReason)
{
	if (iReason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		// set mode to nick name type in
		bGenderFlag = IMP_MALE;
		InvalidateCheckboxes();
	}
}


static void SelectFemaleRegionCallBack(MOUSE_REGION* pRegion, UINT32 iReason)
{
	if (iReason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		// set mode to nick name type in
		bGenderFlag = IMP_FEMALE;
		InvalidateCheckboxes();
	}
}

static void RenderGender(void)
{
	// Flat OS//0 selectors replace the old embossed gender widgets.
	const UINT16 outline = Get16BPPColor(FROMRGB(72, 8, 12));
	ColorFillVideoSurfaceArea(FRAME_BUFFER, MALE_BOX_X, MALE_BOX_Y,
		MALE_BOX_X + MALE_BOX_WIDTH, MALE_BOX_Y + MALE_BOX_HEIGHT, outline);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, FEMALE_BOX_X, MALE_BOX_Y,
		FEMALE_BOX_X + MALE_BOX_WIDTH, MALE_BOX_Y + MALE_BOX_HEIGHT, outline);

	INT32 x;
	switch (bGenderFlag)
	{
		case IMP_FEMALE: x = FEMALE_BOX_X + 9; break;
		case IMP_MALE:   x = MALE_BOX_X   + 9; break;
		default:         return; // none selected yet
	}
	SetFontBackground(FONT_BLACK);
	SetFontAttributes(FONT14ARIAL, 184);
	MPrint(x, MALE_BOX_Y + 6, "X");
}


static void Print8CharacterOnlyString(void)
{
	SetFontAttributes(FONT12ARIAL, FONT_BLACK, NO_SHADOW);
	MPrint(STD_SCREEN_X + 430, STD_SCREEN_Y + LAPTOP_SCREEN_WEB_DELTA_Y + 228, pIMPBeginScreenStrings);
	SetFontShadow(DEFAULT_SHADOW);
}
