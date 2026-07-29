/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella on 2026-07-29.
 * See MODIFICATIONS.md and the SFI source-code license agreement.
 */

#include "GameVersion.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FULL_VERSION "Escape from Arulco " TOSTRING(ESCAPE_FROM_ARULCO_VERSION) \
	" / Stracciatella " TOSTRING(GAME_VERSION)

//
// Keeps track of the game version
//

const char g_version_label[] = FULL_VERSION;

// This version is written into the save files.
// It should remain the same otherwise there will be warning on
// loading the game.
char const g_version_number[16] = "Build 04.12.02";


#ifdef WITH_UNITTESTS
#include "gtest/gtest.h"

TEST(GameVersion, asserts)
{
	EXPECT_EQ(lengthof(g_version_number), 16u);
}

#endif
