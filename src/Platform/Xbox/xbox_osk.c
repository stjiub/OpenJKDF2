#include "xbox_osk.h"

#include "Win95/Window.h"
#include "Win95/stdDisplay.h"
#include "General/stdFont.h"
#include "Main/jkGame.h"

#include "jk.h"

// Layout in 640x480 menu coordinates, below the text boxes of the new player
// and save game dialogs.
#define OSK_COLS    (10)
#define OSK_ROWS    (5)
#define OSK_KEY_W   (60)
#define OSK_KEY_H   (32)
#define OSK_X       (20)
#define OSK_Y       (288)
#define OSK_HINT_H  (22)
#define OSK_MARGIN  (6)
#define OSK_PANEL_W (OSK_COLS * OSK_KEY_W + 2 * OSK_MARGIN)
#define OSK_PANEL_H (OSK_ROWS * OSK_KEY_H + OSK_HINT_H + 2 * OSK_MARGIN)

#define OSK_STICK_THRESH   (0x4000)
#define OSK_TRIGGER_THRESH (0x40)

// Pad state bits past the XBOX_PAD_* buttons, for the triggers.
#define OSK_LTRIGGER (0x10000)
#define OSK_RTRIGGER (0x20000)

enum
{
    OSK_KEY_SHIFT,
    OSK_KEY_SPACE,
    OSK_KEY_DELETE,
    OSK_KEY_DONE,
};

static const char16_t* xbox_osk_aRows[OSK_ROWS - 1] =
{
    u"ABCDEFGHIJ",
    u"KLMNOPQRST",
    u"UVWXYZ-_!'",
    u"1234567890",
};

// The bottom row holds wide keys, listed per column.
static const int xbox_osk_aBottomRow[OSK_COLS] =
{
    OSK_KEY_SHIFT, OSK_KEY_SHIFT,
    OSK_KEY_SPACE, OSK_KEY_SPACE, OSK_KEY_SPACE, OSK_KEY_SPACE,
    OSK_KEY_DELETE, OSK_KEY_DELETE,
    OSK_KEY_DONE, OSK_KEY_DONE,
};

static const char16_t* xbox_osk_aBottomLabels[] = { u"Shift", u"Space", u"Delete", u"Done" };

static const char16_t xbox_osk_waHint[] = u"A Type    X Delete    Y Space    LT/RT Move    B Close    START Done";

static int xbox_osk_bShowing = 0;
static int xbox_osk_bShift = 1;
static int xbox_osk_row = 0;
static int xbox_osk_col = 0;
static uint32_t xbox_osk_lastState = 0;
static int xbox_osk_bFocusDown = 0;
static uint8_t* xbox_osk_pSaved = NULL;
static rdRect xbox_osk_savedRect;

void xbox_osk_Show(void)
{
    // In-game chat asks for the keyboard too, but it's only drawn over GUI menus.
    if (xbox_osk_bShowing || jkGame_isDDraw)
        return;

    xbox_osk_bShowing = 1;
    xbox_osk_bShift = 1;
    xbox_osk_row = 0;
    xbox_osk_col = 0;
}

void xbox_osk_Hide(void)
{
    xbox_osk_bShowing = 0;
}

int xbox_osk_IsShowing(void)
{
    return xbox_osk_bShowing;
}

int xbox_osk_TakeFocusDown(void)
{
    int bFocusDown = xbox_osk_bFocusDown;
    xbox_osk_bFocusDown = 0;
    return bFocusDown;
}

static void xbox_osk_PostChar(uint16_t c)
{
    Window_msg_main_handler(g_hWnd, WM_CHAR, c, 0);
}

static void xbox_osk_PostKey(uint16_t vk)
{
    Window_msg_main_handler(g_hWnd, WM_KEYFIRST, vk, 0);
}

static void xbox_osk_Done(void)
{
    // Hide first: Enter usually closes the dialog, and whatever opens next may
    // focus a text box of its own.
    xbox_osk_bShowing = 0;
    xbox_osk_PostKey(VK_RETURN);
}

static void xbox_osk_PressKey(void)
{
    if (xbox_osk_row < OSK_ROWS - 1)
    {
        char16_t c = xbox_osk_aRows[xbox_osk_row][xbox_osk_col];
        if (!xbox_osk_bShift && c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        xbox_osk_PostChar(c);
        return;
    }

    switch (xbox_osk_aBottomRow[xbox_osk_col])
    {
        case OSK_KEY_SHIFT:
            xbox_osk_bShift = !xbox_osk_bShift;
            break;
        case OSK_KEY_SPACE:
            xbox_osk_PostChar(' ');
            break;
        case OSK_KEY_DELETE:
            xbox_osk_PostChar(VK_BACK);
            break;
        case OSK_KEY_DONE:
            xbox_osk_Done();
            break;
    }
}

static void xbox_osk_MoveHorizontal(int dir)
{
    int key;

    xbox_osk_col = (xbox_osk_col + dir + OSK_COLS) % OSK_COLS;
    if (xbox_osk_row < OSK_ROWS - 1)
        return;

    // Skip the rest of a wide key, then settle on its first column.
    key = xbox_osk_aBottomRow[(xbox_osk_col - dir + OSK_COLS) % OSK_COLS];
    while (xbox_osk_aBottomRow[xbox_osk_col] == key)
        xbox_osk_col = (xbox_osk_col + dir + OSK_COLS) % OSK_COLS;
    while (xbox_osk_col > 0 && xbox_osk_aBottomRow[xbox_osk_col - 1] == xbox_osk_aBottomRow[xbox_osk_col])
        xbox_osk_col--;
}

void xbox_osk_Update(const xbox_pad* pPad)
{
    uint32_t pressed;

    uint32_t state = pPad->buttons;
    if (pPad->lx < -OSK_STICK_THRESH) state |= XBOX_PAD_DPAD_LEFT;
    if (pPad->lx > OSK_STICK_THRESH)  state |= XBOX_PAD_DPAD_RIGHT;
    if (pPad->ly > OSK_STICK_THRESH)  state |= XBOX_PAD_DPAD_UP;
    if (pPad->ly < -OSK_STICK_THRESH) state |= XBOX_PAD_DPAD_DOWN;
    if (pPad->lTrigger > OSK_TRIGGER_THRESH) state |= OSK_LTRIGGER;
    if (pPad->rTrigger > OSK_TRIGGER_THRESH) state |= OSK_RTRIGGER;

    pressed = state & ~xbox_osk_lastState;

    // Update before dispatching: a posted key can run a nested menu loop that
    // calls back in here, and must not see the same press.
    xbox_osk_lastState = state;

    if (!xbox_osk_bShowing || !pressed)
        return;

    if (pressed & XBOX_PAD_DPAD_UP)
        xbox_osk_row = (xbox_osk_row + OSK_ROWS - 1) % OSK_ROWS;
    if (pressed & XBOX_PAD_DPAD_DOWN)
        xbox_osk_row = (xbox_osk_row + 1) % OSK_ROWS;
    if (pressed & XBOX_PAD_DPAD_LEFT)
        xbox_osk_MoveHorizontal(-1);
    if (pressed & XBOX_PAD_DPAD_RIGHT)
        xbox_osk_MoveHorizontal(1);

    if (pressed & XBOX_PAD_A)
    {
        xbox_osk_PressKey();
    }
    else if (pressed & XBOX_PAD_X)
    {
        xbox_osk_PostChar(VK_BACK);
    }
    else if (pressed & XBOX_PAD_Y)
    {
        xbox_osk_PostChar(' ');
    }
    else if (pressed & OSK_LTRIGGER)
    {
        xbox_osk_PostKey(VK_LEFT);
    }
    else if (pressed & OSK_RTRIGGER)
    {
        xbox_osk_PostKey(VK_RIGHT);
    }
    else if (pressed & XBOX_PAD_START)
    {
        xbox_osk_Done();
    }
    else if (pressed & XBOX_PAD_B)
    {
        xbox_osk_bShowing = 0;
        xbox_osk_bFocusDown = 1;
    }
    else if (pressed & XBOX_PAD_BACK)
    {
        xbox_osk_bShowing = 0;
        xbox_osk_PostKey(VK_ESCAPE);
        xbox_osk_PostChar(VK_ESCAPE);
    }
}

static uint8_t xbox_osk_FindColor(int r, int g, int b)
{
    // Skip index 0, which the menu overlay treats as transparent.
    int best = 1;
    int bestDist = 0x7FFFFFFF;
    for (int i = 1; i < 256; i++)
    {
        int dr = stdDisplay_masterPalette[i].r - r;
        int dg = stdDisplay_masterPalette[i].g - g;
        int db = stdDisplay_masterPalette[i].b - b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    return (uint8_t)best;
}

static void xbox_osk_DrawKey(tVBuffer* pVbuf, int x, int y, int width, const char16_t* pLabel, int bHighlighted, uint8_t keyColor, uint8_t highlightColor)
{
    stdFont* pFont;

    rdRect rect = { x + 1, y + 1, width - 2, OSK_KEY_H - 2 };
    if (bHighlighted)
    {
        rdRect inner;

        stdDisplay_VBufferFill(pVbuf, highlightColor, &rect);
        inner = (rdRect){ rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4 };
        stdDisplay_VBufferFill(pVbuf, keyColor, &inner);
    }
    else
    {
        stdDisplay_VBufferFill(pVbuf, keyColor, &rect);
    }

    pFont = jkGui_stdFonts[bHighlighted ? 3 : 2];
    if (pFont)
        stdFont_Draw3(pVbuf, pFont, rect.y, &rect, 3, pLabel, 1);
}

int xbox_osk_BeginDraw(void)
{
    rdRect panel;
    uint8_t* pPixels;
    uint8_t panelColor;
    uint8_t keyColor;
    uint8_t highlightColor;
    char16_t aLabel[2];
    int bottomY;

    tVBuffer* pVbuf = &Video_menuBuffer;
    if (!xbox_osk_bShowing || !pVbuf->surface_lock_alloc)
        return 0;

    panel = (rdRect){ OSK_X - OSK_MARGIN, OSK_Y - OSK_MARGIN, OSK_PANEL_W, OSK_PANEL_H };
    if (panel.x + panel.width > (int)pVbuf->format.width || panel.y + panel.height > (int)pVbuf->format.height)
        return 0;

    if (!xbox_osk_pSaved)
    {
        xbox_osk_pSaved = (uint8_t*)malloc(OSK_PANEL_W * OSK_PANEL_H);
        if (!xbox_osk_pSaved)
            return 0;
    }

    pPixels = (uint8_t*)pVbuf->surface_lock_alloc;
    for (int y = 0; y < panel.height; y++)
        _memcpy(xbox_osk_pSaved + y * panel.width, pPixels + (panel.y + y) * pVbuf->format.rowSize + panel.x, panel.width);
    xbox_osk_savedRect = panel;

    panelColor = xbox_osk_FindColor(8, 8, 16);
    keyColor = xbox_osk_FindColor(40, 40, 56);
    highlightColor = xbox_osk_FindColor(255, 200, 64);
    stdDisplay_VBufferFill(pVbuf, panelColor, &panel);

    aLabel[0] = 0;
    aLabel[1] = 0;
    for (int row = 0; row < OSK_ROWS - 1; row++)
    {
        for (int col = 0; col < OSK_COLS; col++)
        {
            int bHighlighted;

            aLabel[0] = xbox_osk_aRows[row][col];
            if (!xbox_osk_bShift && aLabel[0] >= 'A' && aLabel[0] <= 'Z')
                aLabel[0] += 'a' - 'A';
            bHighlighted = (row == xbox_osk_row && col == xbox_osk_col);
            xbox_osk_DrawKey(pVbuf, OSK_X + col * OSK_KEY_W, OSK_Y + row * OSK_KEY_H, OSK_KEY_W, aLabel, bHighlighted, keyColor, highlightColor);
        }
    }

    bottomY = OSK_Y + (OSK_ROWS - 1) * OSK_KEY_H;
    for (int col = 0; col < OSK_COLS; )
    {
        int bHighlighted;

        int key = xbox_osk_aBottomRow[col];
        int span = 1;
        while (col + span < OSK_COLS && xbox_osk_aBottomRow[col + span] == key)
            span++;
        bHighlighted = (xbox_osk_row == OSK_ROWS - 1 && xbox_osk_aBottomRow[xbox_osk_col] == key);
        xbox_osk_DrawKey(pVbuf, OSK_X + col * OSK_KEY_W, bottomY, span * OSK_KEY_W, xbox_osk_aBottomLabels[key], bHighlighted, keyColor, highlightColor);
        col += span;
    }

    if (jkGui_stdFonts[0])
    {
        rdRect hintRect = { OSK_X, OSK_Y + OSK_ROWS * OSK_KEY_H, OSK_COLS * OSK_KEY_W, OSK_HINT_H };
        stdFont_Draw3(pVbuf, jkGui_stdFonts[0], hintRect.y, &hintRect, 3, xbox_osk_waHint, 1);
    }
    return 1;
}

void xbox_osk_EndDraw(void)
{
    const rdRect* pR;
    uint8_t* pPixels;

    tVBuffer* pVbuf = &Video_menuBuffer;
    if (!xbox_osk_pSaved || !pVbuf->surface_lock_alloc)
        return;

    pR = &xbox_osk_savedRect;
    pPixels = (uint8_t*)pVbuf->surface_lock_alloc;
    for (int y = 0; y < pR->height; y++)
        _memcpy(pPixels + (pR->y + y) * pVbuf->format.rowSize + pR->x, xbox_osk_pSaved + y * pR->width, pR->width);
}
