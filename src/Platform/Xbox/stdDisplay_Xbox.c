#include "Win95/stdDisplay.h"

#include "Win95/Video.h"
#include "Win95/Window.h"
#include "stdPlatform.h"
#include "jk.h"

#include <stdlib.h>
#include <string.h>

extern int Window_bFlipRequested;

rdColor24 stdDisplay_masterPalette[256];

int stdDisplay_Startup()
{
    stdDisplay_bStartup = 1;
    return 1;
}

int stdDisplay_FindClosestDevice(void* pA)
{
    Video_dword_866D78 = 0;
    return 0;
}

int stdDisplay_Open(int a)
{
    stdDisplay_pCurDevice = &stdDisplay_aDisplayDevices[0];
    stdDisplay_bOpen = 1;
    return 1;
}

void stdDisplay_Close()
{
    stdDisplay_bOpen = 0;
}

int stdDisplay_FindClosestMode(render_pair* pA1, struct StdVideoMode* pRenderSurface, unsigned int numModes)
{
    Video_curMode = 0;
    stdDisplay_bPaged = 1;
    stdDisplay_bModeSet = 1;
    return 0;
}

static int stdDisplay_AllocModeBuffer(tVBuffer* pBuf, const tRasterInfo* pFmt)
{
    uint32_t size = pFmt->rowSize * pFmt->height;

    if (pBuf->surface_lock_alloc && pBuf->format.size != size) {
        STD_FREE(pBuf->surface_lock_alloc);
        pBuf->surface_lock_alloc = NULL;
    }
    _memcpy(&pBuf->format, pFmt, sizeof(pBuf->format));
    pBuf->format.size = size;

    if (!pBuf->surface_lock_alloc)
        pBuf->surface_lock_alloc = (char*)STD_ALLOC(size);
    return pBuf->surface_lock_alloc != NULL;
}

MATH_FUNC int stdDisplay_SetMode(unsigned int modeIdx, const void* pPalette, int paged)
{
    tRasterInfo* pFmt;

    uint32_t width = Window_xSize < 640 ? 640 : Window_xSize;
    uint32_t height = Window_ySize < 480 ? 480 : Window_ySize;

    stdDisplay_pCurVideoMode = &Video_renderSurface[modeIdx];
    pFmt = &stdDisplay_pCurVideoMode->format;
    pFmt->format.bpp = 8;
    pFmt->width = width;
    pFmt->height = height;
    pFmt->rowWidth = width;
    pFmt->rowSize = width;

    if (pPalette)
        _memcpy(stdDisplay_gammaPalette, pPalette, 256 * sizeof(rdColor24));

    return stdDisplay_AllocModeBuffer(&Video_menuBuffer, pFmt)
        && stdDisplay_AllocModeBuffer(&Video_otherBuf, pFmt);
}

void stdDisplay_RestoreDisplayMode() {}

int stdDisplay_SetMasterPalette(uint8_t* pPal)
{
    if (!pPal)
        return 0;

    _memcpy(stdDisplay_masterPalette, pPal, sizeof(stdDisplay_masterPalette));
    return 1;
}

int stdDisplay_DDrawGdiSurfaceFlip()
{
    if (!jkCutscene_isRendering && !sithWorld_g_pLastLoadedWorld && !jkGame_isDDraw)
        Window_bFlipRequested = 1;
    else
        Window_SdlUpdate();
    return 1;
}

int stdDisplay_ddraw_waitforvblank()
{
    Window_SdlVblank();
    return 1;
}

void stdDisplay_ddraw_surface_flip2() {}

tVBuffer* stdDisplay_VBufferNew(tRasterInfo* pFmt, int create_ddraw_surface, int gpu_mem, const void* pPalette)
{
    uint32_t bytesPerPixel;

    tVBuffer* pOut = (tVBuffer*)STD_ALLOC(sizeof(tVBuffer));
    if (!pOut)
        return NULL;
    _memset(pOut, 0, sizeof(*pOut));
    _memcpy(&pOut->format, pFmt, sizeof(pOut->format));

    bytesPerPixel = pFmt->format.bpp > 8 ? (pFmt->format.bpp + 7) / 8 : 1;
    pOut->format.rowSize = pFmt->width * bytesPerPixel;
    pOut->format.rowWidth = pFmt->width;
    pOut->format.size = pOut->format.rowSize * pFmt->height;

    if (pOut->format.size) {
        pOut->surface_lock_alloc = (char*)STD_ALLOC(pOut->format.size);
        if (!pOut->surface_lock_alloc) {
            STD_FREE(pOut);
            return NULL;
        }
    }
    return pOut;
}

void stdDisplay_VBufferFree(tVBuffer* pVbuf)
{
    if (!pVbuf)
        return;

    if (pVbuf->surface_lock_alloc)
        STD_FREE(pVbuf->surface_lock_alloc);
    STD_FREE(pVbuf);
}

int stdDisplay_VBufferLock(tVBuffer* pVbuf)
{
    return pVbuf && pVbuf->surface_lock_alloc;
}

void stdDisplay_VBufferUnlock(tVBuffer* pVbuf) {}

int stdDisplay_VBufferSetColorKey(tVBuffer* pVbuf, int color)
{
    if (pVbuf)
        pVbuf->transparent_color = color;
    return 1;
}

tVBuffer* stdDisplay_VBufferConvertColorFormat(void* pA, tVBuffer* pB)
{
    return pB;
}

int stdDisplay_VBufferCopy(tVBuffer* pVbuf, tVBuffer* pVbuf2, unsigned int blit_x, int blit_y, rdRect* pRect, int alpha_maybe)
{
    rdRect full;
    int srcX, srcY;
    int dstX, dstY;
    int w, h;
    const uint8_t* pSrc;
    uint32_t srcStride;
    uint8_t* pDst;
    uint32_t dstStride;
    uint8_t* pStaging;
    int bKeyed;

    if (!pVbuf || !pVbuf2 || !pVbuf->surface_lock_alloc || !pVbuf2->surface_lock_alloc)
        return 1;

    full = (rdRect){0, 0, (int)pVbuf2->format.width, (int)pVbuf2->format.height};
    if (!pRect)
        pRect = &full;

    srcX = pRect->x;
    srcY = pRect->y;
    dstX = (int)blit_x;
    dstY = blit_y;
    w = pRect->width;
    h = pRect->height;

    if (srcX < 0) { dstX -= srcX; w += srcX; srcX = 0; }
    if (srcY < 0) { dstY -= srcY; h += srcY; srcY = 0; }
    if (dstX < 0) { srcX -= dstX; w += dstX; dstX = 0; }
    if (dstY < 0) { srcY -= dstY; h += dstY; dstY = 0; }
    if (srcX + w > (int)pVbuf2->format.width) w = (int)pVbuf2->format.width - srcX;
    if (srcY + h > (int)pVbuf2->format.height) h = (int)pVbuf2->format.height - srcY;
    if (dstX + w > (int)pVbuf->format.width) w = (int)pVbuf->format.width - dstX;
    if (dstY + h > (int)pVbuf->format.height) h = (int)pVbuf->format.height - dstY;
    if (w <= 0 || h <= 0)
        return 1;

    pSrc = (const uint8_t*)pVbuf2->surface_lock_alloc + srcY * pVbuf2->format.rowSize + srcX;
    srcStride = pVbuf2->format.rowSize;
    pDst = (uint8_t*)pVbuf->surface_lock_alloc + dstY * pVbuf->format.rowSize + dstX;
    dstStride = pVbuf->format.rowSize;

    // Stage same-buffer blits so overlapping rectangles copy correctly.
    pStaging = NULL;
    if (pVbuf == pVbuf2) {
        pStaging = (uint8_t*)malloc((size_t)w * h);
        if (!pStaging)
            return 0;
        for (int j = 0; j < h; j++)
            memcpy(pStaging + j * w, pSrc + j * srcStride, w);
        pSrc = pStaging;
        srcStride = w;
    }

    // Index 0 is transparent for keyed blits, except full-width ones.
    bKeyed = (alpha_maybe & 1) && pRect->width != 640;
    for (int j = 0; j < h; j++) {
        const uint8_t* pSrcRow = pSrc + j * srcStride;
        uint8_t* pDstRow = pDst + j * dstStride;
        if (!bKeyed) {
            memcpy(pDstRow, pSrcRow, w);
            continue;
        }
        for (int i = 0; i < w; i++) {
            if (pSrcRow[i])
                pDstRow[i] = pSrcRow[i];
        }
    }

    free(pStaging);
    return 1;
}

int stdDisplay_VBufferFill(tVBuffer* pVbuf, int fillColor, rdRect* pRect)
{
    rdRect full;
    int x, y;
    int w, h;
    uint8_t* pDst;

    if (!pVbuf || !pVbuf->surface_lock_alloc)
        return 1;

    full = (rdRect){0, 0, (int)pVbuf->format.width, (int)pVbuf->format.height};
    if (!pRect)
        pRect = &full;

    x = pRect->x;
    y = pRect->y;
    w = pRect->width;
    h = pRect->height;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)pVbuf->format.width) w = (int)pVbuf->format.width - x;
    if (y + h > (int)pVbuf->format.height) h = (int)pVbuf->format.height - y;
    if (w <= 0 || h <= 0)
        return 1;

    pDst = (uint8_t*)pVbuf->surface_lock_alloc + y * pVbuf->format.rowSize + x;
    for (int j = 0; j < h; j++)
        memset(pDst + j * pVbuf->format.rowSize, (uint8_t)fillColor, w);
    return 1;
}

int stdDisplay_ClearRect(tVBuffer* pBuf, int fillColor, rdRect* pRect)
{
    return stdDisplay_VBufferFill(pBuf, fillColor, pRect);
}

int stdDisplay_GammaCorrect3(int a1)
{
    _memcpy(stdDisplay_gammaPalette, stdDisplay_masterPalette, 256 * sizeof(rdColor24));
    return 1;
}

int stdDisplay_SetCooperativeLevel(uint32_t a) { return 0; }
int stdDisplay_DrawAndFlipGdi(uint32_t a) { return 0; }
void stdDisplay_422A50() {}
