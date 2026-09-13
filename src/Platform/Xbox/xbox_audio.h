// nxdk's audio HAL includes xboxkrnl.h, which clashes with the engine's
// types.h, so the AC97 output and its mixer thread live behind this plain-C
// boundary.
#ifndef _XBOX_AUDIO_H
#define _XBOX_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The MCPX AC97 controller only plays 16-bit stereo at 48 kHz.
#define XBOX_AUDIO_RATE 48000

// Adds `frames` interleaved stereo frames into `pAccum`. Called on the audio
// thread with the audio lock held; must not block.
typedef void (*xbox_audio_source_fn)(int32_t* pAccum, uint32_t frames);

// Starts the AC97 output and the mixer thread. Safe to call more than once.
int xbox_audio_init(void);

int xbox_audio_add_source(xbox_audio_source_fn fn);
void xbox_audio_remove_source(xbox_audio_source_fn fn);

// Excludes the mixer thread, for changing state that sources read. No-ops if
// the output never started.
void xbox_audio_lock(void);
void xbox_audio_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_AUDIO_H
