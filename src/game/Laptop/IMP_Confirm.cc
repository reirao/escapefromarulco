/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "CharProfile.h"
#include "Directories.h"
#include "Font.h"
#include "IMPVideoObjects.h"
#include "Merc_Hiring.h"
#include "Text.h"
#include "Cursors.h"
#include "Laptop.h"
#include "IMP_Compile_Character.h"
#include "IMP_AboutUs.h"
#include "IMP_Text_System.h"
#include "IMP_Confirm.h"
#include "Items.h"
#include "Finances.h"
#include "Soldier_Profile.h"
#include "Soldier_Profile_Type.h"
#include "Soldier_Control.h"
#include "IMP_Portraits.h"
#include "History.h"
#include "Game_Clock.h"
#include "Game_Event_Hook.h"
#include "LaptopSave.h"
#include "SaveLoadGame.h"
#include "Strategic.h"
#include "Random.h"
#include "Button_System.h"
#include "Font_Control.h"
#include "Auto_Resolve.h"
#include "GameLoop.h"
#include "ScreenIDs.h"
#include "StrategicMap.h"
#include "OS0_CreatorScreen.h"
#include "Overhead.h"
#include "Player_Command.h"
#include "Tactical_Save.h"

#include "ContentManager.h"
#include "GameInstance.h"
#include "policy/GamePolicy.h"
#include "policy/IMPPolicy.h"

#include <string_theory/string>


static BUTTON_PICS* giIMPConfirmButtonImage[2];
GUIButtonRef giIMPConfirmButton[2];


struct FacePosInfo
{
	UINT8 eye_x;
	UINT8 eye_y;
	UINT8 mouth_x;
	UINT8 mouth_y;
};

static const FacePosInfo g_face_info[] =
{
	{  8,  5,  8, 21 },
	{  9,  4,  9, 23 },
	{  8,  5,  7, 24 },
	{  6,  6,  7, 25 },
	{ 13,  5, 11, 23 },
	{ 11,  5, 10, 24 },
	{  8,  4,  8, 24 },
	{  8,  4,  8, 24 },
	{  4,  4,  5, 25 },
	{  5,  5,  6, 24 },
	{  7,  5,  7, 24 },
	{  5,  7,  6, 26 },
	{  7,  6,  7, 24 },
	{ 11,  5,  9, 23 },
	{  8,  5,  7, 24 },
	{  5,  6,  5, 26 }
};


static void BtnIMPConfirmNo(GUI_BUTTON *btn, UINT32 reason);
static void BtnIMPConfirmYes(GUI_BUTTON *btn, UINT32 reason);


static void CreateConfirmButtons(void);


void EnterIMPConfirm( void )
{
	// create buttons
	CreateConfirmButtons( );
}

void RenderIMPConfirm( void )
{

	// the background
	RenderProfileBackGround( );

	// indent
	RenderAvgMercIndentFrame(90, 40 );

	// highlight answer
	PrintImpText( );
}


static void DestroyConfirmButtons(void);


void ExitIMPConfirm( void )
{
	// destroy buttons
	DestroyConfirmButtons( );
}

void HandleIMPConfirm( void )
{
}


static void MakeButton(UINT idx, const ST::string& text, INT16 y, GUI_CALLBACK click)
{
	BUTTON_PICS* const img = LoadButtonImage(LAPTOPDIR "/button_2.sti", 0, 1);
	giIMPConfirmButtonImage[idx] = img;
	GUIButtonRef const btn = CreateTextButton(text, FONT12ARIAL, FONT_MCOLOR_RED, FONT_BLACK,
		LAPTOP_SCREEN_UL_X + 136, y, 230, 48, MSYS_PRIORITY_HIGH, click);
	giIMPConfirmButton[idx] = btn;
	btn->SpecifyHilitedTextColors(FONT_WHITE, FONT_BLACK);
	btn->SetCursor(CURSOR_WWW);
}


static void CreateConfirmButtons(void)
{
	// create buttons for confirm screen
	const INT16 dy = LAPTOP_SCREEN_WEB_UL_Y;
	MakeButton(0, pImpButtonText[16], dy + 254, BtnIMPConfirmYes);
	MakeButton(1, pImpButtonText[17], dy + 314, BtnIMPConfirmNo);
}


static void DestroyConfirmButtons(void)
{
	// destroy buttons for confirm screen

	RemoveButton(giIMPConfirmButton[ 0 ] );
	UnloadButtonImage(giIMPConfirmButtonImage[ 0 ] );


	RemoveButton(giIMPConfirmButton[ 1 ] );
	UnloadButtonImage(giIMPConfirmButtonImage[ 1 ] );
}


static void GiveItemsToPC(UINT8 ubProfileId);


static BOOLEAN AddCharacterToPlayersTeam(void)
{
	MERC_HIRE_STRUCT HireMercStruct{};

	// last minute change to make sure merc with right face has not only the right body, but body specific skills...
	// ie. small mercs have martial arts, but big guys and women don't
	if (!fLoadingCharacterForPreviousImpProfile)
	{
		HandleMercStatsForChangesInFace();
	}

	HireMercStruct.ubProfileID = ( UINT8 )( PLAYER_GENERATED_CHARACTER_ID + LaptopSaveInfo.iVoiceId ) ;

	if (!fLoadingCharacterForPreviousImpProfile)
	{
		// give them items
		GiveItemsToPC( 	HireMercStruct.ubProfileID );
	}

	HireMercStruct.bWhatKindOfMerc = MERC_TYPE__PLAYER_CHARACTER;

	HireMercStruct.sSector = g_merc_arrive_sector;
	HireMercStruct.fUseLandingZoneForArrival = TRUE;

	HireMercStruct.fCopyProfileItemsOver = TRUE;

	// indefinite contract length
	HireMercStruct.iTotalContractLength = -1;

	HireMercStruct.ubInsertionCode = INSERTION_CODE_NORTH;
	// OS//0 deploys the created operator immediately.  A zero arrival time is
	// deliberately preserved by HireMerc for this local sandbox path.
	HireMercStruct.uiTimeTillMercArrives = 0;

	const FacePosInfo* const fi = &g_face_info[iPortraitNumber];
	SetProfileFaceData(HireMercStruct.ubProfileID, 200 + iPortraitNumber, fi->eye_x, fi->eye_y, fi->mouth_x, fi->mouth_y);

	//if we succesfully hired the merc
	return HireMerc(HireMercStruct);
}

void FinalizeIMPCharacter()
{
	if (LaptopSaveInfo.fIMPCompletedFlag)
	{
		return;
	}

	LaptopSaveInfo.fIMPCompletedFlag = AddCharacterToPlayersTeam();
	if (!LaptopSaveInfo.fIMPCompletedFlag) return;

	SOLDIERTYPE* const pSoldier = FindSoldierByProfileID(PLAYER_GENERATED_CHARACTER_ID + LaptopSaveInfo.iVoiceId);
	if (!pSoldier) return;

	if (fLoadingCharacterForPreviousImpProfile && gamepolicy(imp_load_keep_inventory))
	{
		IMPSavedProfileLoadInventory(gMercProfiles[PLAYER_GENERATED_CHARACTER_ID + LaptopSaveInfo.iVoiceId].zNickname, pSoldier);
	}

	AddHistoryToPlayersLog(HISTORY_CHARACTER_GENERATED, 0, GetWorldTotalMin(), SGPSector(-1, -1));

	// Turn the vanilla delayed helicopter arrival into a direct, enemy-free
	// sandbox deployment.
	MercArrivesCallback(*pSoldier);
	EliminateAllEnemies(g_merc_arrive_sector);
	SetThisSectorAsPlayerControlled(g_merc_arrive_sector, FALSE);
	SetSectorFlag(g_merc_arrive_sector, SF_ALREADY_VISITED);

	// The sandbox starts as a normal live tactical sector, not as JA2's
	// opening helicopter sequence.
	gTacticalStatus.fDidGameJustStart = FALSE;
	RequestOS0CreatorExit();
	SetLaptopExitScreen(GAME_SCREEN);
	SetCurrentWorldSector(g_merc_arrive_sector);
	SetPendingNewScreen(GAME_SCREEN);

	LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = -1;
	ResetCharacterStats();
}


void StartOS0SandboxWithDefaultOperator()
{
	// Temporary bootstrap identity.  The next development step edits this
	// operator from panels inside the running tactical simulation.
	ResetCharacterStats();
	pFullName = "OS0 Operator";
	pNickName = "Operator";
	fCharacterIsMale = TRUE;
	LaptopSaveInfo.iVoiceId = 0;
	iPortraitNumber = 0;

	CreateACharacterFromPlayerEnteredStats();
	FinalizeIMPCharacter();
}


static void BtnIMPConfirmYes(GUI_BUTTON *btn, UINT32 reason)
{
	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		FinalizeIMPCharacter();
	}
}


// fixed? by CJC Nov 28 2002
static void BtnIMPConfirmNo(GUI_BUTTON *btn, UINT32 reason)
{
	if (reason & MSYS_CALLBACK_REASON_POINTER_UP)
	{
		iCurrentImpPage = IMP_FINISH;
	}
}

static INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT const& p, UINT16 const usItem)
{
	UINT32 uiPos;
	// if it fits into a small pocket
	if (GCM->getItem(usItem)->getPerPocket() != 0)
	{
		// check small pockets first
		for (uiPos = SMALLPOCK1POS; uiPos <= SMALLPOCK8POS; uiPos++)
		{
			if (p.inv[uiPos] == NONE)
			{
				return(uiPos);
			}
		}
	}

	// check large pockets
	for (uiPos = BIGPOCK1POS; uiPos <= BIGPOCK4POS; uiPos++)
	{
		if (p.inv[uiPos] == NONE)
		{
			return(uiPos);
		}
	}
	return(-1);
}

static void GiveItemsToPC(UINT8 ubProfileId)
{
	// gives starting items to merc
	// NOTE: Any guns should probably be from those available in regular gun set

	MERCPROFILESTRUCT& p = GetProfile(ubProfileId);

	for (const IMPStartingItemSet& set : GCM->getIMPPolicy()->getInventory()) {
		if (!set.Evaluate(p)) continue;
		for (const ItemModel* item : set.items) {
			UINT32 uiPos;
			if (set.slot) {
				uiPos = *set.slot;
			} else {
				INT32 const iSlot = FirstFreeBigEnoughPocket(p, item->getItemIndex());
				if (iSlot == -1) continue;
				uiPos = iSlot;
			}
			p.inv[uiPos] = item->getItemIndex();
			p.bInvStatus[uiPos] = 100;
			p.bInvNumber[uiPos] = 1;
		}
	}

	// Transfer the exact slot layout assembled by the player in the OS//0
	// loadout screen.  From here on it is ordinary JA2 profile inventory.
	ApplyOS0LoadoutToProfile(p);
}

void ResetIMPCharactersEyesAndMouthOffsets(const UINT8 ubMercProfileID)
{
	MERCPROFILESTRUCT& p = GetProfile(ubMercProfileID);
	if (p.ubFaceIndex < 200 || p.ubFaceIndex >= 200 + lengthof(g_face_info) || ubMercProfileID >= PROF_HUMMER) return;

	const FacePosInfo* const fi = &g_face_info[p.ubFaceIndex - 200];
	p.usEyesX  = fi->eye_x;
	p.usEyesY  = fi->eye_y;
	p.usMouthX = fi->mouth_x;
	p.usMouthY = fi->mouth_y;
}
