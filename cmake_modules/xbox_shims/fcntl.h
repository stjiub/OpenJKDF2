// Declarations only: needed to compile zlib's gz* sources, which the engine
// never links.
#ifndef _XBOX_SHIM_FCNTL_H
#define _XBOX_SHIM_FCNTL_H

#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_APPEND  0x0008
#define O_CREAT   0x0100
#define O_TRUNC   0x0200

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *path, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_SHIM_FCNTL_H
