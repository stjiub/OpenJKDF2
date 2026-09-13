// On-screen keyboard for GUI text boxes, driven by the gamepad. It shows while
// jkGuiRend focuses a text box (stdControl_ShowSystemKeyboard) and types by
// posting the same window messages a keyboard would.
#ifndef _XBOX_OSK_H
#define _XBOX_OSK_H

#include "xbox_input.h"

void xbox_osk_Show(void);
void xbox_osk_Hide(void);
int xbox_osk_IsShowing(void);

// Runs the keyboard from one pPad sample. Call it every GUI update, shown or not,
// so a button held when it opens doesn't count as a press.
void xbox_osk_Update(const xbox_pad* pPad);

// Returns 1 once after the keyboard is closed with B, so the caller can move
// menu focus off the text box.
int xbox_osk_TakeFocusDown(void);

// Draws the keyboard over the menu buffer, saving what it covers. Returns 0 if
// it isn't showing. Call xbox_osk_EndDraw() after presenting to restore it.
int xbox_osk_BeginDraw(void);
void xbox_osk_EndDraw(void);

#endif // _XBOX_OSK_H
