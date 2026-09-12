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

#endif // _XBOX_COMPAT_H
