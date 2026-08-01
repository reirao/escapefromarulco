#ifndef __INTERFACE_CONTROL_H
#define __INTERFACE_CONTROL_H

#include "JA2Types.h"


void SetUpInterface();
void ResetInterface();
// World-attached tactical marks are rendered before OS//0 magnifies the world.
// Native popups, dialogue-adjacent controls and buttons are rendered afterwards
// so their screen coordinates and pixels remain unchanged.
void RenderTacticalWorldSpaceInterface(void);
void RenderTacticalScreenSpaceInterface(void);
void RenderTopmostTacticalInterface(void);
void RenderTacticalInterface(void);

void RenderTacticalInterfaceWhileScrolling(void);

void EraseInterfaceMenus( BOOLEAN fIgnoreUIUnLock );

void ResetInterfaceAndUI(void);

bool AreWeInAUIMenu();

void HandleTacticalPanelSwitch();

bool InterfaceOKForMeanwhilePopup();

extern BOOLEAN gfRerenderInterfaceFromHelpText;

// If given a null pointer, show the team panel. Otherwise show the given merc
// in the single merc panel.
void SetNewPanel(SOLDIERTYPE*);

#endif
