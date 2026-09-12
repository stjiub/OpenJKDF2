#include "Platform/stdControl.h"

#include "stdPlatform.h"

#include "jk.h"

int stdControl_Startup()
{
    _memset(stdControl_aKeyIdleTimes, 0, sizeof(int) * JK_NUM_KEYS);
    _memset(stdControl_aKeyInfo, 0, sizeof(int) * JK_NUM_KEYS);
    _memset(stdControl_aAxes, 0, sizeof(stdControlJoystickEntry) * JK_NUM_AXES);
    _memset(stdControl_aAxisStates, 0, sizeof(int) * JK_NUM_AXES);

    for (int i = 0; i < JK_NUM_JOYSTICKS; i++) {
        stdControl_aJoystickExists[i] = 0;
        stdControl_aJoystickMaxButtons[i] = 0;
        stdControl_aJoystickEnabled[i] = 0;
    }

    stdControl_Reset();
    stdControl_bStartup = 1;
    return 1;
}

void stdControl_Shutdown()
{
    stdControl_bStartup = 0;
}

int stdControl_Open()
{
    stdControl_bOpen = 1;
    stdControl_bControlsActive = 1;
    return 1;
}

int stdControl_Close()
{
    if ( !stdControl_bOpen )
        return 0;

    stdControl_bControlsActive = 0;
    stdControl_bOpen = 0;
    return 1;
}

void stdControl_Flush()
{
    stdControl_curReadTime = stdPlatform_GetTimeMsec();
}

void stdControl_SetActivation(int a)
{
    if ( stdControl_bOpen )
        stdControl_bControlsActive = !!a;
}

void stdControl_ToggleMouse() {}

// Win32 ShowCursor semantics: callers loop until the display count crosses zero.
static int stdControl_cursorCount = 0;

int stdControl_ShowMouseCursor(int a)
{
    stdControl_cursorCount += a ? 1 : -1;
    return stdControl_cursorCount;
}

void stdControl_ReadControls() {}

void stdControl_ReadMouse() {}

void stdControl_ShowSystemKeyboard() {}
void stdControl_HideSystemKeyboard() {}
BOOL stdControl_IsSystemKeyboardShowing() { return 0; }
