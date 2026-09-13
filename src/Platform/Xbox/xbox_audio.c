#include "xbox_audio.h"

#include <string.h>
#include <windows.h>
#include <hal/audio.h>
#include <xboxkrnl/xboxkrnl.h>

#define XBOX_AUDIO_FRAMES 1024
#define XBOX_AUDIO_NUM_BUFS 4
#define XBOX_AUDIO_BUF_BYTES (XBOX_AUDIO_FRAMES * 2 * sizeof(int16_t))
#define XBOX_AUDIO_MAX_SOURCES 4

// PCM-out Current Index Value: the AC97 bus master's position in the
// 32-entry descriptor ring (nxdk's HAL maps the controller at 0xFEC00000).
#define XBOX_AUDIO_PO_CIV (*(volatile uint8_t*)0xFEC00114)

static int16_t* xbox_audio_apBufs[XBOX_AUDIO_NUM_BUFS];
static int32_t xbox_audio_aAccum[XBOX_AUDIO_FRAMES * 2];
static xbox_audio_source_fn xbox_audio_aSources[XBOX_AUDIO_MAX_SOURCES];
static int xbox_audio_numSources = 0;
static uint32_t xbox_audio_numSubmitted = 0;
static CRITICAL_SECTION xbox_audio_cs;
static KEVENT xbox_audio_event;
static int xbox_audio_bInitted = 0;

// Runs in the HAL's DPC, where the FPU state isn't saved.
static void xbox_audio_Callback(void* pPac97Device, void* pData)
{
    KeSetEvent(&xbox_audio_event, IO_SOUND_INCREMENT, FALSE);
}

// The HAL raises one DPC per interrupt and a DPC can stand for several
// finished buffers, so count what's still queued from the controller itself.
static uint32_t xbox_audio_NumQueued(void)
{
    return (xbox_audio_numSubmitted - XBOX_AUDIO_PO_CIV) & 31;
}

static void xbox_audio_Submit(void)
{
    int16_t* pOut = xbox_audio_apBufs[xbox_audio_numSubmitted % XBOX_AUDIO_NUM_BUFS];

    memset(xbox_audio_aAccum, 0, sizeof(xbox_audio_aAccum));
    EnterCriticalSection(&xbox_audio_cs);
    for (int i = 0; i < xbox_audio_numSources; i++)
        xbox_audio_aSources[i](xbox_audio_aAccum, XBOX_AUDIO_FRAMES);
    LeaveCriticalSection(&xbox_audio_cs);

    for (int i = 0; i < XBOX_AUDIO_FRAMES * 2; i++)
    {
        int32_t s = xbox_audio_aAccum[i];
        pOut[i] = (s > 32767) ? 32767 : (s < -32768) ? -32768 : (int16_t)s;
    }

    XAudioProvideSamples((unsigned char*)pOut, XBOX_AUDIO_BUF_BYTES, FALSE);
    xbox_audio_numSubmitted++;
}

static DWORD WINAPI xbox_audio_Thread(LPVOID pArg)
{
    // Refill even if a completion interrupt goes missing.
    LARGE_INTEGER timeout;
    timeout.QuadPart = -10 * 10000LL;

    for (;;)
    {
        while (xbox_audio_NumQueued() < XBOX_AUDIO_NUM_BUFS)
            xbox_audio_Submit();
        KeWaitForSingleObject(&xbox_audio_event, Executive, KernelMode, FALSE, &timeout);
    }
    return 0;
}

int xbox_audio_init(void)
{
    HANDLE hThread;

    if (xbox_audio_bInitted)
        return 1;

    for (int i = 0; i < XBOX_AUDIO_NUM_BUFS; i++)
    {
        // DMA needs physically contiguous memory.
        xbox_audio_apBufs[i] = MmAllocateContiguousMemoryEx(XBOX_AUDIO_BUF_BYTES, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
        if (!xbox_audio_apBufs[i])
        {
            for (int j = 0; j < i; j++)
            {
                MmFreeContiguousMemory(xbox_audio_apBufs[j]);
                xbox_audio_apBufs[j] = NULL;
            }
            return 0;
        }
        memset(xbox_audio_apBufs[i], 0, XBOX_AUDIO_BUF_BYTES);
    }

    hThread = CreateThread(NULL, 0, xbox_audio_Thread, NULL, CREATE_SUSPENDED, NULL);
    if (!hThread)
    {
        for (int i = 0; i < XBOX_AUDIO_NUM_BUFS; i++)
        {
            MmFreeContiguousMemory(xbox_audio_apBufs[i]);
            xbox_audio_apBufs[i] = NULL;
        }
        return 0;
    }

    InitializeCriticalSection(&xbox_audio_cs);
    KeInitializeEvent(&xbox_audio_event, SynchronizationEvent, FALSE);
    XAudioInit(16, 2, xbox_audio_Callback, NULL);

    // The controller must have descriptors queued before it's started.
    for (int i = 0; i < XBOX_AUDIO_NUM_BUFS; i++)
        xbox_audio_Submit();
    XAudioPlay();

    SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
    ResumeThread(hThread);
    CloseHandle(hThread);

    xbox_audio_bInitted = 1;
    return 1;
}

int xbox_audio_add_source(xbox_audio_source_fn fn)
{
    int bOk;

    if (!xbox_audio_bInitted)
        return 0;

    bOk = 0;
    EnterCriticalSection(&xbox_audio_cs);
    if (xbox_audio_numSources < XBOX_AUDIO_MAX_SOURCES)
    {
        xbox_audio_aSources[xbox_audio_numSources++] = fn;
        bOk = 1;
    }
    LeaveCriticalSection(&xbox_audio_cs);
    return bOk;
}

void xbox_audio_remove_source(xbox_audio_source_fn fn)
{
    if (!xbox_audio_bInitted)
        return;

    EnterCriticalSection(&xbox_audio_cs);
    for (int i = 0; i < xbox_audio_numSources; i++)
    {
        if (xbox_audio_aSources[i] == fn)
        {
            for (int j = i + 1; j < xbox_audio_numSources; j++)
                xbox_audio_aSources[j - 1] = xbox_audio_aSources[j];
            xbox_audio_aSources[--xbox_audio_numSources] = NULL;
            break;
        }
    }
    LeaveCriticalSection(&xbox_audio_cs);
}

void xbox_audio_lock(void)
{
    if (xbox_audio_bInitted)
        EnterCriticalSection(&xbox_audio_cs);
}

void xbox_audio_unlock(void)
{
    if (xbox_audio_bInitted)
        LeaveCriticalSection(&xbox_audio_cs);
}
