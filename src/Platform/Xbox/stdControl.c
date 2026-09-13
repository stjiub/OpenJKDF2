#include "Platform/stdControl.h"

#include "stdPlatform.h"
#include "xbox_input.h"

#include "jk.h"

// Trigger-as-button threshold, as in the SDL gamepad backend.
#define STDCONTROL_XBOX_TRIGGER_THRESH (0x2666)

static const struct
{
    int keyNum;
    uint16_t mask;
} stdControl_aXboxButtons[] =
{
    { KEY_JOY1_B1,     XBOX_PAD_A },
    { KEY_JOY1_B2,     XBOX_PAD_B },
    { KEY_JOY1_B3,     XBOX_PAD_X },
    { KEY_JOY1_B4,     XBOX_PAD_Y },
    { KEY_JOY1_B5,     XBOX_PAD_BACK },
    { KEY_JOY1_B7,     XBOX_PAD_START },
    { KEY_JOY1_B8,     XBOX_PAD_LTHUMB },
    { KEY_JOY1_B9,     XBOX_PAD_RTHUMB },
    { KEY_JOY1_B10,    XBOX_PAD_WHITE },
    { KEY_JOY1_B11,    XBOX_PAD_BLACK },
    { KEY_JOY1_HLEFT,  XBOX_PAD_DPAD_LEFT },
    { KEY_JOY1_HUP,    XBOX_PAD_DPAD_UP },
    { KEY_JOY1_HRIGHT, XBOX_PAD_DPAD_RIGHT },
    { KEY_JOY1_HDOWN,  XBOX_PAD_DPAD_DOWN },
};

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

    // Registered up front so the default bindings can attach to them.
    for (int i = 0; i < JK_JOYSTICK_AXIS_STRIDE; i++) {
        stdControl_RegisterAxis(AXIS_JOY1_X + i, -0x7FFF, 0x7FFF, 0.2);
    }

    xbox_input_init();

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

// Presents the gamepads as joystick 0, with the SDL gamepad layout: sticks
// on X/Y and Z/R (positive Y is down), triggers on U/V.
void stdControl_ReadControls()
{
    int bConnected;
    int trigL;
    int trigR;

    flex_d_t khz;
    xbox_pad pad;

    if (!stdControl_bControlsActive)
        return;

    stdControl_bControlsIdle = 1;
    stdControl_curReadTime = stdPlatform_GetTimeMsec();
    stdControl_readDeltaTime = stdControl_curReadTime - stdControl_lastReadTime;
    khz = (stdControl_readDeltaTime != 0) ? (1.0 / (flex_d_t)(stdControl_readDeltaTime)) : (flex_d_t)1.0;
    stdControl_updateKHz = khz;
    stdControl_updateHz = khz * 1000.0;

    xbox_input_poll();
    bConnected = xbox_input_read(&pad) != 0;

    stdControl_aJoystickExists[0] = bConnected;
    stdControl_aJoystickEnabled[0] = bConnected;
    // The joystick menu lists buttons B1 through B17 (right trigger).
    stdControl_aJoystickMaxButtons[0] = bConnected ? 17 : 0;

    // Like the SDL gamepad backend, keep the sticks readable for menu
    // navigation before any bindings enable them.
    stdControl_aAxisEnabled[0] = 1;
    for (int i = AXIS_JOY1_X; i <= AXIS_JOY1_R; i++) {
        stdControl_aAxes[i].flags |= 2;
    }

    trigL = pad.lTrigger * 0x7FFF / 0xFF;
    trigR = pad.rTrigger * 0x7FFF / 0xFF;
    stdControl_aAxisStates[AXIS_JOY1_X] = pad.lx;
    stdControl_aAxisStates[AXIS_JOY1_Y] = -1 - pad.ly;
    stdControl_aAxisStates[AXIS_JOY1_Z] = pad.rx;
    stdControl_aAxisStates[AXIS_JOY1_R] = -1 - pad.ry;
    stdControl_aAxisStates[AXIS_JOY1_U] = trigL;
    stdControl_aAxisStates[AXIS_JOY1_V] = trigR;

    for (int i = 0; i < (int)(sizeof(stdControl_aXboxButtons) / sizeof(stdControl_aXboxButtons[0])); i++) {
        stdControl_UpdateKeyState(stdControl_aXboxButtons[i].keyNum, !!(pad.buttons & stdControl_aXboxButtons[i].mask), stdControl_curReadTime);
    }
    stdControl_UpdateKeyState(KEY_JOY1_B16, trigL > STDCONTROL_XBOX_TRIGGER_THRESH, stdControl_curReadTime);
    stdControl_UpdateKeyState(KEY_JOY1_B17, trigR > STDCONTROL_XBOX_TRIGGER_THRESH, stdControl_curReadTime);

    stdControl_lastReadTime = stdControl_curReadTime;
}

void stdControl_ReadMouse() {}

void stdControl_ShowSystemKeyboard() {}
void stdControl_HideSystemKeyboard() {}
BOOL stdControl_IsSystemKeyboardShowing() { return 0; }
