// nxdk's USB host headers and xboxkrnl.h clash with the engine's types.h, so
// gamepad access lives behind this plain-C boundary.
#ifndef _XBOX_INPUT_H
#define _XBOX_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_PAD_DPAD_UP    0x0001
#define XBOX_PAD_DPAD_DOWN  0x0002
#define XBOX_PAD_DPAD_LEFT  0x0004
#define XBOX_PAD_DPAD_RIGHT 0x0008
#define XBOX_PAD_START      0x0010
#define XBOX_PAD_BACK       0x0020
#define XBOX_PAD_LTHUMB     0x0040
#define XBOX_PAD_RTHUMB     0x0080
#define XBOX_PAD_A          0x0100
#define XBOX_PAD_B          0x0200
#define XBOX_PAD_X          0x0400
#define XBOX_PAD_Y          0x0800
#define XBOX_PAD_BLACK      0x1000
#define XBOX_PAD_WHITE      0x2000

typedef struct xbox_pad
{
    uint16_t buttons;
    uint8_t lTrigger;
    uint8_t rTrigger;
    // Stick Y is positive up, as the hardware reports it.
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
} xbox_pad;

void xbox_input_init(void);

// Handles hotplug and keeps a report read queued on every gamepad.
void xbox_input_poll(void);

// Combines every connected gamepad into `pOut`, so any of them can drive the
// game. Returns the number of gamepads; with none, `pOut` is all released.
int xbox_input_read(xbox_pad* pOut);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_INPUT_H
