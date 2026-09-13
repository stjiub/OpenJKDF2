#include "xbox_input.h"

#include <stdlib.h>
#include <string.h>
#include <xboxkrnl/xboxkrnl.h>
#include <usbh_lib.h>
#include <xid_driver.h>

// Same analog-to-digital threshold as nxdk's SDL joystick driver.
#define XBOX_INPUT_BUTTON_THRESHOLD 0x20

typedef struct xbox_input_slot
{
    xid_dev_t* pDev;
    xid_gamepad_in report;
} xbox_input_slot;

static xbox_input_slot xbox_input_aSlots[CONFIG_XID_MAX_DEV];
static int xbox_input_bInitted = 0;

// Runs in the OHCI DPC, so the report and the device's transfer queue are
// only touched from the main thread at DISPATCH_LEVEL.
static void xbox_input_ReadComplete(UTR_T* pUtr)
{
    xbox_input_slot* pSlot;
    uint32_t len;

    xid_dev_t* pDev = (xid_dev_t*)pUtr->context;
    if (pUtr->status < 0 || !pDev || !pDev->user_data)
        return;

    pSlot = (xbox_input_slot*)pDev->user_data;
    len = pUtr->xfer_len;
    if (len > sizeof(pSlot->report))
        len = sizeof(pSlot->report);
    memcpy(&pSlot->report, pUtr->buff, len);

    pUtr->xfer_len = 0;
    pUtr->bIsTransferDone = 0;
    if (usbh_int_xfer(pUtr) != USBH_OK)
        pUtr->bIsTransferDone = 1; // Lets xbox_input_poll() queue a new read.
}

static void xbox_input_Connected(xid_dev_t* pDev, int status)
{
    if (pDev->xid_desc.bType != XID_TYPE_GAMECONTROLLER)
        return;

    for (int i = 0; i < CONFIG_XID_MAX_DEV; i++)
    {
        if (!xbox_input_aSlots[i].pDev)
        {
            memset(&xbox_input_aSlots[i], 0, sizeof(xbox_input_aSlots[i]));
            xbox_input_aSlots[i].pDev = pDev;
            pDev->user_data = &xbox_input_aSlots[i];
            return;
        }
    }
}

static void xbox_input_Disconnected(xid_dev_t* pDev, int status)
{
    xbox_input_slot* pSlot = (xbox_input_slot*)pDev->user_data;
    if (pSlot)
        pSlot->pDev = NULL;
    pDev->user_data = NULL;
}

void xbox_input_init(void)
{
    if (xbox_input_bInitted)
        return;

    usbh_core_init();
    usbh_xid_init();
    usbh_install_xid_conn_callback(xbox_input_Connected, xbox_input_Disconnected);
    xbox_input_bInitted = 1;
}

void xbox_input_poll(void)
{
    KIRQL oldIrql;

    if (!xbox_input_bInitted)
        return;

    // Enumerates and removes devices; this can block while a new one resets.
    usbh_pooling_hubs();

    // Normally a no-op: the completion callback requeues the read itself.
    oldIrql = KeRaiseIrqlToDpcLevel();
    for (xid_dev_t* pDev = usbh_xid_get_device_list(); pDev; pDev = pDev->next)
    {
        if (pDev->user_data)
            usbh_xid_read(pDev, 0, xbox_input_ReadComplete);
    }
    KfLowerIrql(oldIrql);
}

static int16_t xbox_input_Stronger(int16_t a, int16_t b)
{
    return (abs(b) > abs(a)) ? b : a;
}

int xbox_input_read(xbox_pad* pOut)
{
    xid_gamepad_in aReports[CONFIG_XID_MAX_DEV];
    int numPads = 0;

    KIRQL oldIrql = KeRaiseIrqlToDpcLevel();
    for (xid_dev_t* pDev = usbh_xid_get_device_list(); pDev && numPads < CONFIG_XID_MAX_DEV; pDev = pDev->next)
    {
        if (pDev->user_data)
            memcpy(&aReports[numPads++], &((xbox_input_slot*)pDev->user_data)->report, sizeof(xid_gamepad_in));
    }
    KfLowerIrql(oldIrql);

    memset(pOut, 0, sizeof(*pOut));
    for (int i = 0; i < numPads; i++)
    {
        const xid_gamepad_in* pR = &aReports[i];

        // The low byte of dButtons already uses the XBOX_PAD_* layout.
        pOut->buttons |= pR->dButtons & 0xFF;
        if (pR->a > XBOX_INPUT_BUTTON_THRESHOLD)     pOut->buttons |= XBOX_PAD_A;
        if (pR->b > XBOX_INPUT_BUTTON_THRESHOLD)     pOut->buttons |= XBOX_PAD_B;
        if (pR->x > XBOX_INPUT_BUTTON_THRESHOLD)     pOut->buttons |= XBOX_PAD_X;
        if (pR->y > XBOX_INPUT_BUTTON_THRESHOLD)     pOut->buttons |= XBOX_PAD_Y;
        if (pR->black > XBOX_INPUT_BUTTON_THRESHOLD) pOut->buttons |= XBOX_PAD_BLACK;
        if (pR->white > XBOX_INPUT_BUTTON_THRESHOLD) pOut->buttons |= XBOX_PAD_WHITE;

        if (pR->l > pOut->lTrigger) pOut->lTrigger = pR->l;
        if (pR->r > pOut->rTrigger) pOut->rTrigger = pR->r;
        pOut->lx = xbox_input_Stronger(pOut->lx, pR->leftStickX);
        pOut->ly = xbox_input_Stronger(pOut->ly, pR->leftStickY);
        pOut->rx = xbox_input_Stronger(pOut->rx, pR->rightStickX);
        pOut->ry = xbox_input_Stronger(pOut->ry, pR->rightStickY);
    }
    return numPads;
}
