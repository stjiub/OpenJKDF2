#include "xbox_music.h"
#include "xbox_audio.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define STB_VORBIS_HEADER_ONLY
#include "external/stb_vorbis/stb_vorbis.c"

#define XBOX_MUSIC_PATH_LEN MAX_PATH
#define XBOX_MUSIC_RING_FRAMES (1 << 16)
#define XBOX_MUSIC_DECODE_FRAMES 4096
#define XBOX_MUSIC_GAIN_ONE (1 << 14)
#define XBOX_MUSIC_STACK_SIZE (128 * 1024)

// Interleaved stereo. The decoder thread only advances ringWrite and the
// mixer only advances ringRead, except when the decoder resets both for a
// new request under the audio lock.
static int16_t xbox_music_aRing[XBOX_MUSIC_RING_FRAMES * 2];
static volatile uint32_t xbox_music_ringWrite = 0;
static volatile uint32_t xbox_music_ringRead = 0;

// Guarded by the audio lock.
static char xbox_music_aPaths[XBOX_MUSIC_MAX_TRACKS][XBOX_MUSIC_PATH_LEN];
static int xbox_music_numPaths = 0;
static volatile uint32_t xbox_music_reqGen = 0; // bumped by every play and stop
static uint32_t xbox_music_ringGen = 0;         // request the ring belongs to
static int xbox_music_bDecoding = 0;            // more audio coming for ringGen
static int xbox_music_bPlaying = 0;
static uint32_t xbox_music_step = 0;
static uint32_t xbox_music_frac = 0;
static int32_t xbox_music_gain = XBOX_MUSIC_GAIN_ONE;

static HANDLE xbox_music_hWake = NULL;
static int xbox_music_bInitted = 0;

// Audio thread.
static void xbox_music_Mix(int32_t* pAccum, uint32_t frames)
{
    uint32_t read;
    uint32_t write;
    uint32_t frac;
    uint32_t step;
    int32_t gain;

    if (!xbox_music_bPlaying || xbox_music_ringGen != xbox_music_reqGen)
        return;

    read = xbox_music_ringRead;
    write = xbox_music_ringWrite;
    frac = xbox_music_frac;
    step = xbox_music_step;
    gain = xbox_music_gain;

    for (uint32_t i = 0; i < frames; i++)
    {
        const int16_t* pA;
        const int16_t* pB;
        int32_t t;
        int32_t l;
        int32_t r;

        if (write - read < 2)
        {
            if (!xbox_music_bDecoding)
                xbox_music_bPlaying = 0;
            break;
        }

        pA = &xbox_music_aRing[(read & (XBOX_MUSIC_RING_FRAMES - 1)) * 2];
        pB = &xbox_music_aRing[((read + 1) & (XBOX_MUSIC_RING_FRAMES - 1)) * 2];
        t = (int32_t)(frac >> 1);
        l = pA[0] + (((pB[0] - pA[0]) * t) >> 15);
        r = pA[1] + (((pB[1] - pA[1]) * t) >> 15);
        pAccum[i * 2] += (l * gain) >> 14;
        pAccum[i * 2 + 1] += (r * gain) >> 14;

        frac += step;
        read += frac >> 16;
        frac &= 0xFFFF;
    }

    xbox_music_ringRead = read;
    xbox_music_frac = frac;
}

static stb_vorbis* xbox_music_OpenNext(char (*aPaths)[XBOX_MUSIC_PATH_LEN], int numPaths, int* pNext, uint32_t* pRate)
{
    while (*pNext < numPaths)
    {
        stb_vorbis* pV;
        stb_vorbis_info info;

        const char* pPath = aPaths[(*pNext)++];
        FILE* pF = fopen(pPath, "rb");
        if (!pF)
            continue;

        // Closes f on failure too.
        pV = stb_vorbis_open_file(pF, 1, NULL, NULL);
        if (!pV)
            continue;

        info = stb_vorbis_get_info(pV);
        if (!info.sample_rate)
        {
            stb_vorbis_close(pV);
            continue;
        }
        *pRate = info.sample_rate;
        return pV;
    }
    return NULL;
}

static uint32_t xbox_music_Step(uint32_t rate)
{
    return (uint32_t)(((uint64_t)rate << 16) / XBOX_AUDIO_RATE);
}

static DWORD WINAPI xbox_music_Thread(LPVOID pArg)
{
    static char xbox_music_aDecodePaths[XBOX_MUSIC_MAX_TRACKS][XBOX_MUSIC_PATH_LEN];
    int numPaths = 0;
    int next = 0;
    uint32_t gen = 0;
    uint32_t rate = 0;
    stb_vorbis* pV = NULL;

    for (;;)
    {
        int bNewRequest;

        WaitForSingleObject(xbox_music_hWake, 20);

        xbox_audio_lock();
        bNewRequest = (xbox_music_reqGen != gen);
        if (bNewRequest)
        {
            gen = xbox_music_reqGen;
            numPaths = xbox_music_numPaths;
            memcpy(xbox_music_aDecodePaths, xbox_music_aPaths, sizeof(xbox_music_aDecodePaths));
        }
        xbox_audio_unlock();

        if (bNewRequest)
        {
            if (pV)
                stb_vorbis_close(pV);
            next = 0;
            pV = xbox_music_OpenNext(xbox_music_aDecodePaths, numPaths, &next, &rate);

            xbox_audio_lock();
            if (xbox_music_reqGen == gen)
            {
                xbox_music_ringRead = 0;
                xbox_music_ringWrite = 0;
                xbox_music_frac = 0;
                xbox_music_step = pV ? xbox_music_Step(rate) : 0;
                xbox_music_ringGen = gen;
                xbox_music_bDecoding = (pV != NULL);
                if (!pV)
                    xbox_music_bPlaying = 0;
            }
            xbox_audio_unlock();
        }

        while (pV && xbox_music_reqGen == gen)
        {
            uint32_t offset;
            uint32_t n;
            int got;

            uint32_t write = xbox_music_ringWrite;
            uint32_t space = XBOX_MUSIC_RING_FRAMES - (write - xbox_music_ringRead);
            if (space < XBOX_MUSIC_DECODE_FRAMES)
                break;

            offset = write & (XBOX_MUSIC_RING_FRAMES - 1);
            n = XBOX_MUSIC_RING_FRAMES - offset;
            if (n > XBOX_MUSIC_DECODE_FRAMES)
                n = XBOX_MUSIC_DECODE_FRAMES;

            got = stb_vorbis_get_samples_short_interleaved(pV, 2, &xbox_music_aRing[offset * 2], (int)n * 2);
            if (got > 0)
            {
                __asm__ __volatile__("" ::: "memory");
                xbox_music_ringWrite = write + (uint32_t)got;
                continue;
            }

            stb_vorbis_close(pV);
            pV = xbox_music_OpenNext(xbox_music_aDecodePaths, numPaths, &next, &rate);

            xbox_audio_lock();
            if (xbox_music_ringGen == gen)
            {
                if (pV)
                    xbox_music_step = xbox_music_Step(rate);
                else
                    xbox_music_bDecoding = 0;
            }
            xbox_audio_unlock();
        }
    }
    return 0;
}

int xbox_music_init(void)
{
    HANDLE hThread;

    if (xbox_music_bInitted)
        return 1;

    if (!xbox_audio_init())
        return 0;

    xbox_music_hWake = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!xbox_music_hWake)
        return 0;

    if (!xbox_audio_add_source(xbox_music_Mix))
    {
        CloseHandle(xbox_music_hWake);
        xbox_music_hWake = NULL;
        return 0;
    }

    hThread = CreateThread(NULL, XBOX_MUSIC_STACK_SIZE, xbox_music_Thread, NULL, 0, NULL);
    if (!hThread)
    {
        xbox_audio_remove_source(xbox_music_Mix);
        CloseHandle(xbox_music_hWake);
        xbox_music_hWake = NULL;
        return 0;
    }
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
    CloseHandle(hThread);

    xbox_music_bInitted = 1;
    return 1;
}

int xbox_music_play(const char* const* apPaths, int numPaths)
{
    if (!xbox_music_bInitted || numPaths <= 0 || numPaths > XBOX_MUSIC_MAX_TRACKS)
        return 0;

    for (int i = 0; i < numPaths; i++)
    {
        if (strlen(apPaths[i]) >= XBOX_MUSIC_PATH_LEN)
            return 0;
    }

    xbox_audio_lock();
    for (int i = 0; i < numPaths; i++)
        strcpy(xbox_music_aPaths[i], apPaths[i]);
    xbox_music_numPaths = numPaths;
    xbox_music_reqGen++;
    xbox_music_bPlaying = 1;
    xbox_audio_unlock();

    SetEvent(xbox_music_hWake);
    return 1;
}

void xbox_music_stop(void)
{
    if (!xbox_music_bInitted)
        return;

    xbox_audio_lock();
    xbox_music_numPaths = 0;
    xbox_music_reqGen++;
    xbox_music_bPlaying = 0;
    xbox_audio_unlock();

    SetEvent(xbox_music_hWake);
}

void xbox_music_set_volume(float vol)
{
    if (vol < 0.0f)
        vol = 0.0f;
    if (vol > 1.0f)
        vol = 1.0f;

    xbox_audio_lock();
    xbox_music_gain = (int32_t)(vol * XBOX_MUSIC_GAIN_ONE);
    xbox_audio_unlock();
}

int xbox_music_is_playing(void)
{
    int bPlaying;

    xbox_audio_lock();
    bPlaying = xbox_music_bPlaying;
    xbox_audio_unlock();
    return bPlaying;
}
