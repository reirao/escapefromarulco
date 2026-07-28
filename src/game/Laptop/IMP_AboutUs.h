/*
 * Escape from Arulco modification notice:
 * Modified from JA2 Stracciatella, 2026-07-24 through 2026-07-28.
 * See /MODIFICATIONS.md and the SFI source-code license agreement.
 */

#ifndef _IMP_ABOUTUS_H
#define _IMP_ABOUTUS_H

struct MERCPROFILESTRUCT;

void RenderIMPAboutUs( void );
void ExitIMPAboutUs( void );
void EnterIMPAboutUs( void );
void HandleIMPAboutUs( void );
void ApplyOS0LoadoutToProfile(MERCPROFILESTRUCT& profile);

#endif
