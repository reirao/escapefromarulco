/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#ifndef __IMP_CONFIRM_H
#define __IMP_CONFIRM_H

#include "Types.h"

void EnterIMPConfirm( void );
void RenderIMPConfirm( void );
void ExitIMPConfirm( void );
void HandleIMPConfirm( void );
void FinalizeIMPCharacter();
void StartOS0SandboxWithDefaultOperator();

void ResetIMPCharactersEyesAndMouthOffsets( UINT8 ubMercProfileID );

#endif
