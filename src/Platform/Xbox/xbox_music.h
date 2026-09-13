// Streams Ogg Vorbis music into the audio mixer. Decoding and disc reads run
// on a thread of their own so a slow read can't stall the mixer.
#ifndef _XBOX_MUSIC_H
#define _XBOX_MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_MUSIC_MAX_TRACKS 8

int xbox_music_init(void);

// Plays the files once, in order; paths must be absolute. Returns 0 if there
// is nothing to play or the music system isn't running.
int xbox_music_play(const char* const* apPaths, int numPaths);
void xbox_music_stop(void);

// 0..1
void xbox_music_set_volume(float vol);

// Stays true from xbox_music_play() until the last file has been heard.
int xbox_music_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_MUSIC_H
