// PDCLib's printf family skips %e/%f/%g/%a without consuming the argument,
// misreading every argument after it. PDCLib defines each of these in its own
// object, so defining them here keeps its versions out of the link.

#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf/stb_sprintf.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

int vsnprintf(char* pBuf, size_t count, const char* pFmt, va_list va)
{
    // stb writes buf[-1] for a non-null buffer with a zero count.
    if (!count)
        return stbsp_vsnprintf(NULL, 0, pFmt, va);
    return stbsp_vsnprintf(pBuf, count > INT_MAX ? INT_MAX : (int)count, pFmt, va);
}

int snprintf(char* pBuf, size_t count, const char* pFmt, ...)
{
    int ret;

    va_list va;
    va_start(va, pFmt);
    ret = vsnprintf(pBuf, count, pFmt, va);
    va_end(va);
    return ret;
}

int vsprintf(char* pBuf, const char* pFmt, va_list va)
{
    return stbsp_vsprintf(pBuf, pFmt, va);
}

int sprintf(char* pBuf, const char* pFmt, ...)
{
    int ret;

    va_list va;
    va_start(va, pFmt);
    ret = vsprintf(pBuf, pFmt, va);
    va_end(va);
    return ret;
}

typedef struct
{
    FILE* pStream;
    int bError;
    char aBuf[STB_SPRINTF_MIN];
} xbox_printf_sink;

static char* xbox_printf_flush(const char* pBuf, void* pUser, int len)
{
    xbox_printf_sink* pSink = (xbox_printf_sink*)pUser;

    if (fwrite(pBuf, 1, len, pSink->pStream) != (size_t)len)
        pSink->bError = 1;
    return pSink->aBuf;
}

int vfprintf(FILE* pStream, const char* pFmt, va_list va)
{
    int ret;

    xbox_printf_sink sink;

    sink.pStream = pStream;
    sink.bError = 0;
    ret = stbsp_vsprintfcb(xbox_printf_flush, &sink, sink.aBuf, pFmt, va);
    return sink.bError ? -1 : ret;
}

int fprintf(FILE* pStream, const char* pFmt, ...)
{
    int ret;

    va_list va;
    va_start(va, pFmt);
    ret = vfprintf(pStream, pFmt, va);
    va_end(va);
    return ret;
}

int vprintf(const char* pFmt, va_list va)
{
    return vfprintf(stdout, pFmt, va);
}

int printf(const char* pFmt, ...)
{
    int ret;

    va_list va;
    va_start(va, pFmt);
    ret = vprintf(pFmt, va);
    va_end(va);
    return ret;
}
