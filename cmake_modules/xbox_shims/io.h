// Declarations only: needed to compile zlib's gz* sources, which the engine
// never links.
#ifndef _XBOX_SHIM_IO_H
#define _XBOX_SHIM_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int _wopen(const void *path, int flags, int mode);
size_t wcstombs(char *dst, const void *src, size_t len);
long long _lseeki64(int fd, long long offset, int whence);
int read(int fd, void *buf, unsigned int count);
int write(int fd, const void *buf, unsigned int count);
int close(int fd);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_SHIM_IO_H
