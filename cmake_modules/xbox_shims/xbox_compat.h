// nxdk ships unistd.h and strings.h but they are effectively empty, and its
// include paths take precedence over ours, so these can't be shadowed.
#ifndef _XBOX_COMPAT_H
#define _XBOX_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

// nxdk implements these as assert(0), which halts the console. Rename them
// before any libc header is seen so callers use the Xbox implementations.
#define strtod xbox_strtod
#define strtof xbox_strtof
#define strtold xbox_strtold
double xbox_strtod(const char* pStr, char** ppEnd);
float xbox_strtof(const char* pStr, char** ppEnd);
long double xbox_strtold(const char* pStr, char** ppEnd);

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <malloc.h> // alloca

#ifdef __cplusplus
extern "C" {
#endif

char* getcwd(char* pBuf, size_t size);
int chdir(const char* pPath);
int unlink(const char* pPath);

char* strtok_r(char* pStr, const char* pDelim, char** ppSave);
char* strsep(char** ppString, const char* pDelim);
int strcasecmp(const char* a, const char* b);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_COMPAT_H
