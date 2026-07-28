/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "Directories.h"
#include "Font.h"
#include "HImage.h"
#include "IMPVideoObjects.h"
#include "VObject.h"
#include "Laptop.h"
#include "GameRes.h"
#include "IMP_Attribute_Selection.h"
#include "Button_System.h"
#include "Font_Control.h"
#include "Object_Cache.h"
#include "Video.h"
#include "VSurface.h"
#include "UILayout.h"

// video object handles
SGPVObject* guiANALYSE;
SGPVObject* guiATTRIBUTEGRAPH;
SGPVObject* guiSMALLSILHOUETTE;

namespace {
cache_key_t const guiBACKGROUND{ LAPTOPDIR "/metalbackground.sti" };
cache_key_t const guiBEGININDENT{ LAPTOPDIR "/beginscreenindent.sti" };
cache_key_t const guiACTIVATIONINDENT{ LAPTOPDIR "/activationindent.sti" };
cache_key_t const guiFRONTPAGEINDENT{ LAPTOPDIR "/frontpageindent.sti" };
cache_key_t const guiNAMEINDENT{ LAPTOPDIR "/nameindent.sti" };
cache_key_t const guiNICKNAMEINDENT{ LAPTOPDIR "/nickname.sti" };
cache_key_t const guiGENDERINDENT{ LAPTOPDIR "/genderindent.sti" };
cache_key_t const guiLARGESILHOUETTE{ LAPTOPDIR "/largesilhouette.sti" };
cache_key_t const guiPORTRAITFRAME{ LAPTOPDIR "/voice_portraitframe.sti" };
cache_key_t const guiSLIDERBAR{ LAPTOPDIR "/attributeslider.sti" };
cache_key_t const guiATTRIBUTEFRAME{ LAPTOPDIR "/attributeframe.sti" };
cache_key_t const guiBUTTON2IMAGE{ LAPTOPDIR "/button_2.sti" };
cache_key_t const guiBUTTON4IMAGE{ LAPTOPDIR "/button_4.sti" };
cache_key_t const guiMAININDENT{ LAPTOPDIR "/mainprofilepageindent.sti" };
cache_key_t const guiLONGINDENT{ LAPTOPDIR "/longindent.sti" };
cache_key_t const guiLONGHINDENT{ LAPTOPDIR "/longindenthigh.sti" };
cache_key_t const guiSHORTINDENT{ LAPTOPDIR "/shortindent.sti" };
cache_key_t const guiSHORTHINDENT{ LAPTOPDIR "/shortindenthigh.sti" };
cache_key_t const guiSHORT2INDENT{ LAPTOPDIR "/shortindent2.sti"};
cache_key_t const guiSHORT2HINDENT{ LAPTOPDIR "/shortindent2high.sti" };
cache_key_t const guiQINDENT{ LAPTOPDIR "/questionindent.sti" };
cache_key_t const guiA1INDENT{ LAPTOPDIR "/attributescreenindent_1.sti" };
cache_key_t const guiA2INDENT{ LAPTOPDIR "/attributescreenindent_2.sti" };
cache_key_t const guiAVGMERCINDENT{ LAPTOPDIR "/anaveragemercindent.sti" };
cache_key_t const guiABOUTUSINDENT{ LAPTOPDIR "/aboutusindent.sti" };

void DrawOS0Panel(INT16 x, INT16 y, INT16 width, INT16 height, BOOLEAN active = FALSE)
{
	const INT16 left = LAPTOP_SCREEN_UL_X + x;
	const INT16 top = LAPTOP_SCREEN_WEB_UL_Y + y;
	const UINT16 fill = Get16BPPColor(active ? FROMRGB(28, 8, 11) : FROMRGB(12, 14, 16));
	const UINT16 edge = Get16BPPColor(active ? FROMRGB(210, 22, 30) : FROMRGB(72, 8, 12));
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, left + width, top + height, fill);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, left + width, top + 1, edge);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top + height - 1, left + width, top + height, edge);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, left + 1, top + height, edge);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left + width - 1, top, left + width, top + height, edge);
}
}


// position defines
#define CHAR_PROFILE_BACKGROUND_TILE_WIDTH 125
#define CHAR_PROFILE_BACKGROUND_TILE_HEIGHT 100

extern void DrawBonusPointsRemaining( void );


void RemoveProfileBackGround( void )
{
	// remove background
	RemoveVObject(guiBACKGROUND);
}


void RenderProfileBackGround( void )
{
	// Escape from Arulco: local OS//0 terminal skin for the character editor.
	const INT16 left = LAPTOP_SCREEN_UL_X;
	const INT16 top = LAPTOP_SCREEN_WEB_UL_Y;
	const INT16 right = LAPTOP_SCREEN_LR_X;
	const INT16 bottom = LAPTOP_SCREEN_WEB_LR_Y;
	const UINT16 black = Get16BPPColor(FROMRGB(4, 5, 6));
	const UINT16 graphite = Get16BPPColor(FROMRGB(22, 24, 26));
	const UINT16 red = Get16BPPColor(FROMRGB(168, 16, 22));
	const UINT16 dark_red = Get16BPPColor(FROMRGB(72, 8, 12));

	// Clear the entire legacy laptop, including its grey sidebar and taskbar.
	ColorFillVideoSurfaceArea(FRAME_BUFFER, STD_SCREEN_X, STD_SCREEN_Y,
		STD_SCREEN_X + 639, STD_SCREEN_Y + 479, black);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, STD_SCREEN_X, STD_SCREEN_Y,
		STD_SCREEN_X + 639, STD_SCREEN_Y + 20, graphite);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, STD_SCREEN_X, STD_SCREEN_Y,
		STD_SCREEN_X + 639, STD_SCREEN_Y + 1, red);
	SetFont(FONT12ARIAL);
	SetFontForeground(FONT_MCOLOR_RED);
	MPrint(STD_SCREEN_X + 18, STD_SCREEN_Y + 5, "OS//0");
	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_DKGRAY);
	MPrint(STD_SCREEN_X + 78, STD_SCREEN_Y + 7, "OPERATOR CREATION  /  LOCAL SESSION");

	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, right, bottom, black);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, right, top + 1, red);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, bottom - 1, right, bottom, dark_red);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, left, top, left + 1, bottom, dark_red);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, right - 1, top, right, bottom, dark_red);

	// dirty buttons
	MarkButtonsDirty( );

	// force refresh of screen
	InvalidateRegion(LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
}


void DeleteIMPSymbol( void )
{
	// remove IMP symbol
	RemoveVObject(MLG_IMPSYMBOL);
}


void RenderIMPSymbol(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 96, 44);
}


void DeleteBeginIndent( void )
{
	// remove indent symbol
	RemoveVObject(guiBEGININDENT);
}


void RenderBeginIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 282, 154);
}


void DeleteActivationIndent( void )
{
	// remove activation indent symbol
	RemoveVObject(guiACTIVATIONINDENT);
}


void RenderActivationIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 160, 42);
}


void DeleteFrontPageIndent( void )
{
	// remove activation indent symbol
	RemoveVObject(guiFRONTPAGEINDENT);
}


void RenderFrontPageIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 103, 220);
}


void LoadAnalyse(void)
{
	// this procedure will load the activation indent into memory
	guiANALYSE = AddVideoObjectFromFile(LAPTOPDIR "/analyze.sti");
}


void DeleteAnalyse( void )
{
	// remove activation indent symbol
	DeleteVideoObject(guiANALYSE);
}


void LoadAttributeGraph(void)
{
	// this procedure will load the activation indent into memory
	guiATTRIBUTEGRAPH = AddVideoObjectFromFile(LAPTOPDIR "/attributegraph.sti");
}


void DeleteAttributeGraph( void )
{
	// remove activation indent symbol
	DeleteVideoObject(guiATTRIBUTEGRAPH);
}


void DeleteNickNameIndent( void )
{
	// remove activation indent symbol
	RemoveVObject(guiNICKNAMEINDENT);
}


void RenderNickNameIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 116, 28);
}


void DeleteNameIndent( void )
{
	// remove activation indent symbol
	RemoveVObject(guiNAMEINDENT);
}


void RenderNameIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 235, 28);
}


void DeleteGenderIndent( void )
{
	// remove activation indent symbol
	RemoveVObject(guiGENDERINDENT);
}


void RenderGenderIndent(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 25, 25);
}


void LoadSmallSilhouette(void)
{
	// this procedure will load the activation indent into memory
	guiSMALLSILHOUETTE = AddVideoObjectFromFile(LAPTOPDIR "/smallsilhouette.sti");
}


void DeleteSmallSilhouette( void )
{
	// remove activation indent symbol
	DeleteVideoObject(guiSMALLSILHOUETTE);
}


void DeleteLargeSilhouette( void )
{
	// remove activation indent symbol
	RemoveVObject(guiLARGESILHOUETTE);
}


void RenderLargeSilhouette(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 116, 146);
}


void DeleteAttributeFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiATTRIBUTEFRAME);
}


void RenderAttributeFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 400, 220);
	for (INT16 row = 0; row < 10; ++row)
	{
		DrawOS0Panel(sX + 112, sY + 10 + row * 20, 281, 20);
	}
}

void RenderAttributeFrameForIndex( INT16 sX, INT16 sY, INT32 iIndex )
{
	INT16 sCurrentY = 0;

	// valid index?
	if( iIndex == -1 )
	{
		return;
	}

	sCurrentY = ( INT16 )( 10 + ( iIndex * 20 ) );

	DrawOS0Panel(sX + 112, sY + sCurrentY, 281, 20, TRUE);

	RenderAttrib2IndentFrame(350, 42 );

	// amt of bonus pts
	DrawBonusPointsRemaining( );

	// render attribute boxes
	RenderAttributeBoxes( );

	InvalidateRegion( LAPTOP_SCREEN_UL_X + sX + 112, LAPTOP_SCREEN_WEB_UL_Y + sY + sCurrentY, LAPTOP_SCREEN_UL_X + sX + 394, LAPTOP_SCREEN_WEB_UL_Y + sY + sCurrentY + 21 );
}


void DeleteSliderBar( void )
{
	// remove activation indent symbol
	RemoveVObject(guiSLIDERBAR);
}


void RenderSliderBar(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 18, 12, TRUE);
}


void DeleteButton2Image( void )
{
	// remove activation indent symbol
	RemoveVObject(guiBUTTON2IMAGE);
}


void RenderButton2Image(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 230, 48);
}


void DeleteButton4Image( void )
{
	// remove activation indent symbol
	RemoveVObject(guiBUTTON4IMAGE);
}


void RenderButton4Image(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 360, 46);
}


void DeletePortraitFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiPORTRAITFRAME);
}


void RenderPortraitFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 116, 146);
}


void DeleteMainIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiMAININDENT);
}


void RenderMainIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 180, 70);
}


void DeleteQtnLongIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiLONGINDENT);
}


void RenderQtnLongIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42);
}


void DeleteQtnShortIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiSHORTINDENT);
}


void RenderQtnShortIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42);
}


void DeleteQtnLongIndentHighFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiLONGHINDENT);
}


void RenderQtnLongIndentHighFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42, TRUE);
}


void DeleteQtnShortIndentHighFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiSHORTHINDENT);
}


void RenderQtnShortIndentHighFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42, TRUE);
}


void DeleteQtnIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiQINDENT);
}


void RenderQtnIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 470, 62);
}


void DeleteAttrib1IndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiA1INDENT);
}


void RenderAttrib1IndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 245, 248);
}


void DeleteAttrib2IndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiA2INDENT);
}


void RenderAttrib2IndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 115, 72);
}


void DeleteAvgMercIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiAVGMERCINDENT);
}


void RenderAvgMercIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 320, 190);
}


void DeleteAboutUsIndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiABOUTUSINDENT);
}


void RenderAboutUsIndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 235, 140);
}


void DeleteQtnShort2IndentFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiSHORT2INDENT);
}


void RenderQtnShort2IndentFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42);
}


void DeleteQtnShort2IndentHighFrame( void )
{
	// remove activation indent symbol
	RemoveVObject(guiSHORT2HINDENT);
}


void RenderQtnShort2IndentHighFrame(INT16 sX, INT16 sY)
{
	DrawOS0Panel(sX, sY, 220, 42, TRUE);
}
