#include "Win95/stdSound.h"

#include "Gui/jkGUISound.h"
#include "Main/Main.h"
#include "stdPlatform.h"
#include "General/stdMath.h"
#include "xbox_audio.h"

#include "jk.h"

#ifdef STDSOUND_XBOX

#define STDSOUND_XBOX_MAX_VOICES (64)
#define STDSOUND_XBOX_GAIN_ONE (1 << 14)

// PCM shared between a buffer and its duplicates, like DirectSound's
// DuplicateSoundBuffer. Refcounts change only on the engine thread; a sample
// is freed only once no buffer (and so no voice) references it.
typedef struct stdSoundXboxSample
{
    int refs;
    uint8_t* pData;
    uint32_t numFrames;
    uint32_t step;
    int bytesPerSample;
    int channels;
} stdSoundXboxSample;

static stdSound_buffer_t* stdSound_aXboxVoices[STDSOUND_XBOX_MAX_VOICES];
static int stdSound_bXboxReady = 0;

static uint32_t stdSound_XboxStep(uint32_t rate)
{
    return (uint32_t)(((uint64_t)rate << 16) / XBOX_AUDIO_RATE);
}

static stdSoundXboxSample* stdSound_XboxSampleNew(stdSound_buffer_t* pBuffer, int bufferBytes)
{
    int channels;
    stdSoundXboxSample* pSample;

    int bytesPerSample = pBuffer->bitsPerSample / 8;
    if (bytesPerSample < 1 || bytesPerSample > 3)
        bytesPerSample = 2;
    channels = pBuffer->bStereo ? 2 : 1;

    pSample = (stdSoundXboxSample*)STD_ALLOC(sizeof(stdSoundXboxSample));
    if (!pSample)
        return NULL;
    pSample->pData = (uint8_t*)STD_ALLOC(bufferBytes);
    if (!pSample->pData)
    {
        STD_FREE(pSample);
        return NULL;
    }
    _memset(pSample->pData, bytesPerSample == 1 ? 0x80 : 0, bufferBytes);

    pSample->refs = 1;
    pSample->bytesPerSample = bytesPerSample;
    pSample->channels = channels;
    pSample->numFrames = (uint32_t)bufferBytes / (uint32_t)(bytesPerSample * channels);
    pSample->step = stdSound_XboxStep(pBuffer->nSamplesPerSec ? pBuffer->nSamplesPerSec : 22050);
    return pSample;
}

static void stdSound_XboxSampleRelease(void* pData)
{
    stdSoundXboxSample* pSample = (stdSoundXboxSample*)pData;
    if (!pSample || --pSample->refs > 0)
        return;
    STD_FREE(pSample->pData);
    STD_FREE(pSample);
}

// The functions below that touch voices or queues run under the audio lock.

static void stdSound_XboxVoiceAdd(stdSound_buffer_t* pBuffer)
{
    if (pBuffer->voice >= 0)
        return;
    for (int i = 0; i < STDSOUND_XBOX_MAX_VOICES; i++)
    {
        if (!stdSound_aXboxVoices[i])
        {
            stdSound_aXboxVoices[i] = pBuffer;
            pBuffer->voice = i;
            return;
        }
    }
}

static void stdSound_XboxVoiceRemove(stdSound_buffer_t* pBuffer)
{
    if (pBuffer->voice >= 0)
        stdSound_aXboxVoices[pBuffer->voice] = NULL;
    pBuffer->voice = -1;
    pBuffer->bPlaying = 0;
}

static stdSoundXboxSample* stdSound_XboxCurrentSample(stdSound_buffer_t* pBuffer)
{
    if (!pBuffer->bStream)
        return (stdSoundXboxSample*)pBuffer->pSample;
    if (pBuffer->queueDone >= pBuffer->queueLen)
        return NULL;
    return (stdSoundXboxSample*)pBuffer->apQueue[(pBuffer->queueHead + pBuffer->queueDone) % STDSOUND_XBOX_QUEUE_LEN];
}

// Moves finished queue entries into aOut for release outside the lock.
static int stdSound_XboxReapQueue(stdSound_buffer_t* pBuffer, void** pAOut)
{
    int n = pBuffer->queueDone;
    for (int i = 0; i < n; i++)
        pAOut[i] = pBuffer->apQueue[(pBuffer->queueHead + i) % STDSOUND_XBOX_QUEUE_LEN];
    pBuffer->queueHead = (pBuffer->queueHead + n) % STDSOUND_XBOX_QUEUE_LEN;
    pBuffer->queueLen -= n;
    pBuffer->queueDone = 0;
    return n;
}

static int stdSound_XboxDetachAll(stdSound_buffer_t* pBuffer, void** pAOut)
{
    int n = 0;
    for (int i = 0; i < pBuffer->queueLen; i++)
        pAOut[n++] = pBuffer->apQueue[(pBuffer->queueHead + i) % STDSOUND_XBOX_QUEUE_LEN];
    pBuffer->queueHead = 0;
    pBuffer->queueLen = 0;
    pBuffer->queueDone = 0;
    pBuffer->bStream = 0;
    return n;
}

static void stdSound_XboxReleaseAll(void** pASamples, int n)
{
    for (int i = 0; i < n; i++)
        stdSound_XboxSampleRelease(pASamples[i]);
}

static inline int32_t stdSound_XboxFetch(const stdSoundXboxSample* pSample, uint32_t frame, int ch)
{
    const uint8_t* pP = pSample->pData + (frame * pSample->channels + ch) * pSample->bytesPerSample;
    switch (pSample->bytesPerSample)
    {
        case 1:
            return ((int32_t)pP[0] - 128) << 8;
        case 3:
            return (int16_t)(pP[1] | (pP[2] << 8));
        default:
            return *(const int16_t*)pP;
    }
}

static inline int32_t stdSound_XboxLerp(int32_t a, int32_t b, uint32_t frac)
{
    return a + (((b - a) * (int32_t)(frac >> 1)) >> 15);
}

// Audio thread.
static void stdSound_XboxMixVoice(stdSound_buffer_t* pBuffer, int32_t* pAccum, uint32_t frames)
{
    uint32_t i = 0;
    while (i < frames)
    {
        uint32_t step;
        uint32_t last;
        uint32_t wrap;
        int32_t gainL;
        int32_t gainR;
        uint32_t pos;
        uint32_t frac;

        stdSoundXboxSample* pSample = stdSound_XboxCurrentSample(pBuffer);
        if (!pSample || !pSample->numFrames)
        {
            stdSound_XboxVoiceRemove(pBuffer);
            pBuffer->pos = 0;
            pBuffer->frac = 0;
            return;
        }

        if (pBuffer->pos >= pSample->numFrames)
        {
            if (pBuffer->bStream)
            {
                pBuffer->pos -= pSample->numFrames;
                pBuffer->queueDone++;
                continue;
            }
            if (pBuffer->bLooping)
            {
                pBuffer->pos %= pSample->numFrames;
                continue;
            }
            stdSound_XboxVoiceRemove(pBuffer);
            pBuffer->pos = 0;
            pBuffer->frac = 0;
            return;
        }

        step = pBuffer->freqStep ? pBuffer->freqStep : pSample->step;
        last = pSample->numFrames - 1;
        wrap = (pBuffer->bLooping && !pBuffer->bStream) ? 0 : last;
        gainL = pBuffer->gainL;
        gainR = pBuffer->gainR;
        pos = pBuffer->pos;
        frac = pBuffer->frac;

        if (pSample->channels == 2)
        {
            for (; i < frames && pos < pSample->numFrames; i++)
            {
                uint32_t next = (pos < last) ? pos + 1 : wrap;
                int32_t l = stdSound_XboxLerp(stdSound_XboxFetch(pSample, pos, 0), stdSound_XboxFetch(pSample, next, 0), frac);
                int32_t r = stdSound_XboxLerp(stdSound_XboxFetch(pSample, pos, 1), stdSound_XboxFetch(pSample, next, 1), frac);
                pAccum[i * 2] += (l * gainL) >> 14;
                pAccum[i * 2 + 1] += (r * gainR) >> 14;
                frac += step;
                pos += frac >> 16;
                frac &= 0xFFFF;
            }
        }
        else
        {
            for (; i < frames && pos < pSample->numFrames; i++)
            {
                uint32_t next = (pos < last) ? pos + 1 : wrap;
                int32_t s = stdSound_XboxLerp(stdSound_XboxFetch(pSample, pos, 0), stdSound_XboxFetch(pSample, next, 0), frac);
                pAccum[i * 2] += (s * gainL) >> 14;
                pAccum[i * 2 + 1] += (s * gainR) >> 14;
                frac += step;
                pos += frac >> 16;
                frac &= 0xFFFF;
            }
        }

        pBuffer->pos = pos;
        pBuffer->frac = frac;
    }
}

static void stdSound_XboxMix(int32_t* pAccum, uint32_t frames)
{
    for (int i = 0; i < STDSOUND_XBOX_MAX_VOICES; i++)
    {
        if (stdSound_aXboxVoices[i])
            stdSound_XboxMixVoice(stdSound_aXboxVoices[i], pAccum, frames);
    }
}

// The engine mixes positional pSound itself (3D is off) and hands us a pan of
// -1..1, the listener-right component of the direction to the source.
static void stdSound_XboxUpdateGain(stdSound_buffer_t* pBuffer)
{
    flex_t vol = stdMath_Clamp(pBuffer->vol, 0.0, 2.0);
    flex_t pan = stdMath_Clamp(pBuffer->pan, -1.0, 1.0);
    int32_t gainL = (int32_t)(vol * (pan > 0.0 ? 1.0 - pan : 1.0) * STDSOUND_XBOX_GAIN_ONE);
    int32_t gainR = (int32_t)(vol * (pan < 0.0 ? 1.0 + pan : 1.0) * STDSOUND_XBOX_GAIN_ONE);

    xbox_audio_lock();
    pBuffer->gainL = gainL;
    pBuffer->gainR = gainR;
    xbox_audio_unlock();
}

int stdSound_Startup()
{
    jkGuiSound_b3DSound = 0;

    if (!stdSound_bXboxReady && !Main_bHeadless && xbox_audio_init())
        stdSound_bXboxReady = xbox_audio_add_source(stdSound_XboxMix);
    return 1;
}

void stdSound_Shutdown()
{
    xbox_audio_lock();
    for (int i = 0; i < STDSOUND_XBOX_MAX_VOICES; i++)
    {
        if (stdSound_aXboxVoices[i])
            stdSound_XboxVoiceRemove(stdSound_aXboxVoices[i]);
    }
    xbox_audio_unlock();
}

void stdSound_SetMenuVolume(flex_t a1)
{
    stdSound_fMenuVolume = a1;
}

stdSound_buffer_t* stdSound_BufferCreate(int bStereo, uint32_t nSamplesPerSec, uint16_t bitsPerSample, int bufferLen)
{
    stdSound_buffer_t* pOut = (stdSound_buffer_t*)STD_ALLOC(sizeof(stdSound_buffer_t));
    if (!pOut)
        return NULL;

    _memset(pOut, 0, sizeof(*pOut));

    pOut->bStereo = bStereo;
    pOut->bufferLen = bufferLen;
    pOut->nSamplesPerSec = nSamplesPerSec;
    pOut->bitsPerSample = bitsPerSample;
    pOut->refcnt = 1;
    pOut->vol = 1.0 * stdSound_fMenuVolume;
    pOut->voice = -1;
    stdSound_XboxUpdateGain(pOut);
    return pOut;
}

void* stdSound_BufferSetData(stdSound_buffer_t* pSound, int bufferBytes, int32_t* pBufferMaxSize)
{
    stdSoundXboxSample* pSample;
    void* pOld;

    if (pBufferMaxSize)
        *pBufferMaxSize = bufferBytes;

    // Like refilling a DirectSound buffer: duplicates and queues that hold the
    // old sample keep it.
    pSample = (bufferBytes > 0) ? stdSound_XboxSampleNew(pSound, bufferBytes) : NULL;

    xbox_audio_lock();
    pOld = pSound->pSample;
    pSound->pSample = pSample;
    pSound->data = pSample ? pSample->pData : NULL;
    pSound->bufferBytes = pSample ? bufferBytes : 0;
    pSound->pos = 0;
    pSound->frac = 0;
    xbox_audio_unlock();

    stdSound_XboxSampleRelease(pOld);
    return pSound->data;
}

int stdSound_BufferUnlock(stdSound_buffer_t* pSound, void* pBuffer, int bufferRead)
{
    return 1;
}

int stdSound_BufferPlay(stdSound_buffer_t* pBuffer, int loop)
{
    stdSoundXboxSample* pSample;
    int bPlaying;

    if (!pBuffer)
        return 0;

    xbox_audio_lock();
    pBuffer->bLooping = loop;
    pSample = stdSound_XboxCurrentSample(pBuffer);
    if (pSample)
    {
        if (pBuffer->pos >= pSample->numFrames)
        {
            pBuffer->pos = 0;
            pBuffer->frac = 0;
        }
        stdSound_XboxVoiceAdd(pBuffer);
        pBuffer->bPlaying = (pBuffer->voice >= 0);
    }
    bPlaying = pBuffer->bPlaying;
    xbox_audio_unlock();
    return bPlaying;
}

// Queues pBufferNext's sample to play after everything already queued on pBufferPrev,
// which then plays as a gapless stream (OpenAL source queue semantics).
int stdSound_BufferQueueAfterAnother(stdSound_buffer_t* pBufferPrev, stdSound_buffer_t* pBufferNext)
{
    void* aDone[STDSOUND_XBOX_QUEUE_LEN];
    int numDone;
    int bQueued = 0;

    if (!pBufferPrev || !pBufferNext || !pBufferNext->pSample)
        return 0;

    xbox_audio_lock();
    numDone = stdSound_XboxReapQueue(pBufferPrev, aDone);
    if (!pBufferPrev->bStream)
    {
        stdSound_XboxVoiceRemove(pBufferPrev);
        pBufferPrev->bStream = 1;
        pBufferPrev->pos = 0;
        pBufferPrev->frac = 0;
    }
    if (pBufferPrev->queueLen < STDSOUND_XBOX_QUEUE_LEN)
    {
        stdSoundXboxSample* pSample = (stdSoundXboxSample*)pBufferNext->pSample;
        pSample->refs++;
        pBufferPrev->apQueue[(pBufferPrev->queueHead + pBufferPrev->queueLen) % STDSOUND_XBOX_QUEUE_LEN] = pSample;
        pBufferPrev->queueLen++;
        bQueued = 1;
    }
    if (!pBufferPrev->bPlaying)
    {
        stdSound_XboxVoiceAdd(pBufferPrev);
        pBufferPrev->bPlaying = (pBufferPrev->voice >= 0);
    }
    xbox_audio_unlock();

    stdSound_XboxReleaseAll(aDone, numDone);
    return bQueued;
}

void stdSound_BufferUnqueueProcessed(stdSound_buffer_t* pBuffer)
{
    void* aDone[STDSOUND_XBOX_QUEUE_LEN];
    int numDone;

    if (!pBuffer)
        return;

    xbox_audio_lock();
    numDone = stdSound_XboxReapQueue(pBuffer, aDone);
    xbox_audio_unlock();

    stdSound_XboxReleaseAll(aDone, numDone);
}

void stdSound_BufferRelease(stdSound_buffer_t* pSound)
{
    void* aSamples[STDSOUND_XBOX_QUEUE_LEN + 1];
    int n;

    if (!pSound)
        return;

    xbox_audio_lock();
    stdSound_XboxVoiceRemove(pSound);
    n = stdSound_XboxDetachAll(pSound, aSamples);
    aSamples[n++] = pSound->pSample;
    pSound->pSample = NULL;
    pSound->data = NULL;
    xbox_audio_unlock();

    stdSound_XboxReleaseAll(aSamples, n);
    STD_FREE(pSound);
}

int stdSound_BufferReset(stdSound_buffer_t* pSound)
{
    void* aSamples[STDSOUND_XBOX_QUEUE_LEN];
    int n;

    if (!pSound)
        return 0;

    xbox_audio_lock();
    stdSound_XboxVoiceRemove(pSound);
    n = stdSound_XboxDetachAll(pSound, aSamples);
    pSound->bLooping = 0;
    pSound->pos = 0;
    pSound->frac = 0;
    xbox_audio_unlock();

    stdSound_XboxReleaseAll(aSamples, n);
    return 1;
}

void stdSound_BufferSetPan(stdSound_buffer_t* pA1, flex_t a2)
{
    if (!pA1)
        return;
    pA1->pan = a2;
    stdSound_XboxUpdateGain(pA1);
}

void stdSound_BufferSetFrequency(stdSound_buffer_t* pSound, int freq)
{
    uint32_t step;

    if (!pSound)
        return;

    step = (freq > 0) ? stdSound_XboxStep((uint32_t)freq) : 0;
    xbox_audio_lock();
    pSound->freqStep = step;
    xbox_audio_unlock();
}

stdSound_buffer_t* stdSound_BufferDuplicate(stdSound_buffer_t* pSound)
{
    stdSound_buffer_t* pOut = (stdSound_buffer_t*)STD_ALLOC(sizeof(stdSound_buffer_t));
    if (!pOut)
        return NULL;

    _memset(pOut, 0, sizeof(*pOut));

    pOut->data = pSound->data;
    pOut->bStereo = pSound->bStereo;
    pOut->bufferLen = pSound->bufferLen;
    pOut->nSamplesPerSec = pSound->nSamplesPerSec;
    pOut->bitsPerSample = pSound->bitsPerSample;
    pOut->refcnt = 1;
    pOut->vol = pSound->vol;
    pOut->pan = pSound->pan;
    pOut->format = pSound->format;
    pOut->bufferBytes = pSound->bufferBytes;
    pOut->bIsCopy = 1;
    pOut->freqStep = pSound->freqStep;
    pOut->voice = -1;

    pOut->pSample = pSound->pSample;
    if (pOut->pSample)
        ((stdSoundXboxSample*)pOut->pSample)->refs++;

    stdSound_XboxUpdateGain(pOut);
    return pOut;
}

void stdSound_IA3D_idk(flex_t a)
{
}

// Pauses; Play resumes from the same position, as with DirectSound.
int stdSound_BufferStop(stdSound_buffer_t* pBuffer)
{
    if (!pBuffer)
        return 1;

    xbox_audio_lock();
    stdSound_XboxVoiceRemove(pBuffer);
    xbox_audio_unlock();
    return 1;
}

void stdSound_BufferSetVolume(stdSound_buffer_t* pSound, flex_t vol)
{
    if (!pSound)
        return;
    pSound->vol = vol * stdSound_fMenuVolume;
    stdSound_XboxUpdateGain(pSound);
}

int stdSound_3DSetMode(stdSound_buffer_t* pA1, int a2)
{
    return 1;
}

stdSound_3dBuffer_t* stdSound_BufferQueryInterface(stdSound_buffer_t* pSoundBuffer)
{
    return pSoundBuffer;
}

void stdSound_CommitDeferredSettings()
{
}

void stdSound_SetPositionOrientation(rdVector3* pPos, rdVector3* pLvec, rdVector3* pUvec)
{
}

void stdSound_SetPosition(stdSound_buffer_t* pSound, rdVector3* pPos)
{
}

void stdSound_SetVelocity(stdSound_buffer_t* pSound, rdVector3* pVel)
{
}

int stdSound_IsPlaying(stdSound_buffer_t* pSound, rdVector3* pPos)
{
    return pSound && pSound->bPlaying;
}

void stdSound_3DBufferRelease(stdSound_3dBuffer_t* pP3DBuffer)
{
}

#endif // STDSOUND_XBOX
