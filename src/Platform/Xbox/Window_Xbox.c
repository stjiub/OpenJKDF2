#include "Win95/Window.h"

#include "Win95/stdGdi.h"
#include "Platform/std3D.h"
#include "Main/Main.h"
#include "Main/jkMain.h"
#include "Main/jkGame.h"
#include "Gui/jkGUI.h"
#include "Win95/stdDisplay.h"
#include "World/jkPlayer.h"
#include "Platform/stdControl.h"
#include "stdPlatform.h"
#include "Devices/sithConsole.h"
#include "Platform/wuRegistry.h"
#include "Main/jkQuakeConsole.h"
#include "Gui/jkGUIRend.h"

#include "jk.h"

#ifdef TARGET_XBOX

extern int jkGuiBuildMulti_bRendering;
extern int Window_needsRecreate;

int Window_lastXRel = 0;
int Window_lastYRel = 0;
int Window_lastSampleTime = 0;
int Window_lastSampleMs = 0;
int Window_bMouseLeft = 0;
int Window_bMouseRight = 0;
int Window_resized = 0;
int Window_mouseX = 0;
int Window_mouseY = 0;
int Window_mouseWheelX = 0;
int Window_mouseWheelY = 0;
int Window_lastMouseX = 0;
int Window_lastMouseY = 0;
int Window_xPos = 0;
int Window_yPos = 0;
int Window_lastGameIsDDraw = 0;
int Window_menu_mouseX = 0;
int Window_menu_mouseY = 0;
int Window_bFlipRequested = 0;

void Window_Main_Loop()
{
    jkMain_GuiAdvance();
    Window_msg_main_handler(g_hWnd, WM_PAINT, 0, 0);
}

int Window_Main_Linux(int argc, char** pArgv)
{
    int bFullscreen;
    int bHiDpi;

    char aCmdLine[1024];
    int result;
    size_t cmdLen = 0;

    strcpy(aCmdLine, "");

    g_handler_count = 0;
    g_thing_two_some_dialog_count = 0;
    g_should_exit = 0;
    g_window_not_destroyed = 0;
    g_hInstance = 0;
    g_nShowCmd = 0;

    for (int i = 1; i < argc; i++) {
        size_t argLen = strlen(pArgv[i]);
        if (argLen >= sizeof(aCmdLine) - cmdLen - 1)
            return 0;
        memcpy(aCmdLine + cmdLen, pArgv[i], argLen);
        cmdLen += argLen;
        aCmdLine[cmdLen++] = ' ';
        aCmdLine[cmdLen] = 0;
    }
    stdPlatform_Printf("cmdline: %s\n", aCmdLine);

    result = Main_Startup(aCmdLine);

    bFullscreen = wuRegistry_GetBool("Window_isFullscreen", 0);
    bHiDpi = wuRegistry_GetBool("Window_isHiDpi", 0);
    Window_SetFullscreen(bFullscreen);
    Window_SetHiDpi(bHiDpi);
    Window_resized = 1;
    Window_xSize = 640;
    Window_ySize = 480;

    if (!result) return result;

    std3D_FreeResources();

    g_window_not_destroyed = 1;

    Window_msg_main_handler(g_hWnd, WM_CREATE, 0, 0);
    Window_msg_main_handler(g_hWnd, WM_ACTIVATE, 2, 0);
    Window_msg_main_handler(g_hWnd, WM_ACTIVATEAPP, 1, 0);
    Window_msg_main_handler(g_hWnd, WM_SHOWWINDOW, 0, 0);
    Window_msg_main_handler(g_hWnd, WM_PAINT, 0, 0);

    while (1) {
        Window_Main_Loop();
        if (g_should_exit) break;
    }

    if (jkPlayer_bHasLoadedSettingsOnce) {
        jkPlayer_WriteConf(jkPlayer_playerShortName);
    }

    Main_Shutdown();
    return 1;
}

int Window_DefaultHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, void* pUnused)
{
    return 0;
}

int Window_ShowCursorUnwindowed(int a1)
{
    return stdControl_ShowMouseCursor(a1);
}

int Window_MessageLoop()
{
    jkGuiRend_UpdateController();
    Window_SdlUpdate();

    if (Window_bFlipRequested) {
        Window_msg_main_handler(g_hWnd, WM_PAINT, 0, 0);
        Window_bFlipRequested = 0;
    }
    return 0;
}

void Window_SdlUpdate()
{
    if (Main_bHeadless) return;

    if (Window_resized) {
        jkMain_FixRes();
        if (!jkGui_SetModeMenu(0)) {
            stdDisplay_SetMode(0, 0, 0);
        }
        jkGui_SetModeGame();
        Window_resized = 0;
    }

    Window_lastSampleTime = stdPlatform_GetTimeMsec();

    if (!jkGame_isDDraw) {
        if (!jkGuiBuildMulti_bRendering) {
            std3D_StartScene();
            std3D_DrawMenu();
            std3D_EndScene();
        } else {
            std3D_DrawMenu();
        }

        if (Window_needsRecreate) {
            std3D_PurgeEntireTextureCache();
            Window_resized = 1;
            Window_needsRecreate = 0;
        }
    } else {
        if (jkGame_isDDraw != Window_lastGameIsDDraw) {
            Window_menu_mouseX = Window_mouseX;
            Window_menu_mouseY = Window_mouseY;
            Window_lastXRel = 0;
            Window_lastYRel = 0;
        }
    }

    Window_lastGameIsDDraw = jkGame_isDDraw;
}

void Window_SdlVblank()
{
    if (Main_bHeadless) return;
}

#endif // TARGET_XBOX
