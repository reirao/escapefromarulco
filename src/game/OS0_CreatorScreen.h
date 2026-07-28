/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#ifndef OS0_CREATOR_SCREEN_H
#define OS0_CREATOR_SCREEN_H

#include "ScreenIDs.h"

void OS0CreatorScreenInit();
ScreenID OS0CreatorScreenHandle();
void OS0CreatorScreenShutdown();
void RequestOS0CreatorExit();

#endif
