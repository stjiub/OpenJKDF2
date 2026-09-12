// PDCLib's scanf skips %e/%f/%g/%a without parsing or storing anything. PDCLib
// defines sscanf and vsscanf in their own objects, so defining them here keeps
// its versions out of the link. Implements the C99 conversions except hex
// floats and wide characters.

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    LEN_DEFAULT,
    LEN_HH,
    LEN_H,
    LEN_L,
    LEN_LL,
    LEN_LONG_DOUBLE,
    LEN_SIZE,
    LEN_INTMAX,
    LEN_PTRDIFF,
};

static const char* xbox_scanf_ParseLength(const char* pFmt, int* pLen)
{
    switch (*pFmt) {
        case 'h':
            if (pFmt[1] == 'h') { *pLen = LEN_HH; return pFmt + 2; }
            *pLen = LEN_H;
            return pFmt + 1;
        case 'l':
            if (pFmt[1] == 'l') { *pLen = LEN_LL; return pFmt + 2; }
            *pLen = LEN_L;
            return pFmt + 1;
        case 'L': *pLen = LEN_LONG_DOUBLE; return pFmt + 1;
        case 'z': *pLen = LEN_SIZE; return pFmt + 1;
        case 'j': *pLen = LEN_INTMAX; return pFmt + 1;
        case 't': *pLen = LEN_PTRDIFF; return pFmt + 1;
        default: *pLen = LEN_DEFAULT; return pFmt;
    }
}

static void xbox_scanf_StoreInteger(va_list* pAp, int len, unsigned long long value)
{
    switch (len) {
        case LEN_HH: *va_arg(*pAp, unsigned char*) = (unsigned char)value; break;
        case LEN_H: *va_arg(*pAp, unsigned short*) = (unsigned short)value; break;
        case LEN_L: *va_arg(*pAp, unsigned long*) = (unsigned long)value; break;
        case LEN_LL: *va_arg(*pAp, unsigned long long*) = value; break;
        case LEN_SIZE: *va_arg(*pAp, size_t*) = (size_t)value; break;
        case LEN_INTMAX: *va_arg(*pAp, uintmax_t*) = (uintmax_t)value; break;
        case LEN_PTRDIFF: *va_arg(*pAp, ptrdiff_t*) = (ptrdiff_t)value; break;
        default: *va_arg(*pAp, unsigned int*) = (unsigned int)value; break;
    }
}

// Copies at most `width` characters (all if 0) into buf for a number parser.
static size_t xbox_scanf_CopyField(const char* pS, size_t width, char* pBuf, size_t bufsz)
{
    size_t n = 0;
    size_t max = (width && width < bufsz) ? width : bufsz - 1;

    while (n < max && pS[n] && !isspace((unsigned char)pS[n])) {
        pBuf[n] = pS[n];
        n++;
    }
    pBuf[n] = 0;
    return n;
}

// Parses a scanset after '['. Returns the character after the closing ']'.
static const char* xbox_scanf_ParseScanset(const char* pFmt, unsigned char pSet[256])
{
    int bInvert = 0;

    memset(pSet, 0, 256);
    if (*pFmt == '^') {
        bInvert = 1;
        pFmt++;
    }
    if (*pFmt == ']')
        pSet[(unsigned char)*pFmt++] = 1;
    while (*pFmt && *pFmt != ']') {
        if (pFmt[1] == '-' && pFmt[2] && pFmt[2] != ']') {
            for (int c = (unsigned char)pFmt[0]; c <= (unsigned char)pFmt[2]; c++)
                pSet[c] = 1;
            pFmt += 3;
        }
        else {
            pSet[(unsigned char)*pFmt++] = 1;
        }
    }
    if (*pFmt == ']')
        pFmt++;
    if (bInvert) {
        for (int c = 0; c < 256; c++)
            pSet[c] = !pSet[c];
    }
    return pFmt;
}

int vsscanf(const char* pStr, const char* pFmt, va_list ap_in)
{
    const char* pS = pStr;
    int nAssigned = 0;
    va_list ap;

    va_copy(ap, ap_in);

    while (*pFmt) {
        int bSuppress;
        size_t width;
        int len;
        char conv;

        if (isspace((unsigned char)*pFmt)) {
            while (isspace((unsigned char)*pFmt))
                pFmt++;
            while (isspace((unsigned char)*pS))
                pS++;
            continue;
        }

        if (*pFmt != '%' || pFmt[1] == '%') {
            if (*pFmt == '%') {
                pFmt++;
                while (isspace((unsigned char)*pS))
                    pS++;
            }
            if (*pS != *pFmt)
                goto done_input_or_match;
            pS++;
            pFmt++;
            continue;
        }

        pFmt++;
        bSuppress = 0;
        if (*pFmt == '*') {
            bSuppress = 1;
            pFmt++;
        }
        width = 0;
        while (isdigit((unsigned char)*pFmt))
            width = width * 10 + (*pFmt++ - '0');

        pFmt = xbox_scanf_ParseLength(pFmt, &len);
        conv = *pFmt++;

        if (conv == 'n') {
            if (!bSuppress)
                xbox_scanf_StoreInteger(&ap, len, (unsigned long long)(pS - pStr));
            continue;
        }
        if (conv != 'c' && conv != '[') {
            while (isspace((unsigned char)*pS))
                pS++;
        }
        if (!*pS)
            goto done_input_or_match;

        switch (conv) {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
                unsigned long long value;

                char aBuf[72];
                char* pEnd;
                int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
                xbox_scanf_CopyField(pS, width, aBuf, sizeof(aBuf));
                value = (conv == 'd' || conv == 'i')
                    ? (unsigned long long)strtoll(aBuf, &pEnd, base)
                    : strtoull(aBuf, &pEnd, base);
                if (pEnd == aBuf)
                    goto done;
                pS += pEnd - aBuf;
                if (bSuppress)
                    break;
                if (conv == 'p')
                    *va_arg(ap, void** ) = (void*)(uintptr_t)value;
                else
                    xbox_scanf_StoreInteger(&ap, len, value);
                nAssigned++;
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
                double value;

                char aBuf[72];
                char* pEnd;
                xbox_scanf_CopyField(pS, width, aBuf, sizeof(aBuf));
                value = xbox_strtod(aBuf, &pEnd);
                if (pEnd == aBuf)
                    goto done;
                pS += pEnd - aBuf;
                if (bSuppress)
                    break;
                if (len == LEN_LONG_DOUBLE)
                    *va_arg(ap, long double*) = value;
                else if (len == LEN_L)
                    *va_arg(ap, double*) = value;
                else
                    *va_arg(ap, float*) = (float)value;
                nAssigned++;
                break;
            }
            case 's': {
                char* pOut = bSuppress ? NULL : va_arg(ap, char*);
                size_t n = 0;
                while (*pS && !isspace((unsigned char)*pS) && (!width || n < width)) {
                    if (pOut)
                        pOut[n] = *pS;
                    n++;
                    pS++;
                }
                if (pOut) {
                    pOut[n] = 0;
                    nAssigned++;
                }
                break;
            }
            case 'c': {
                char* pOut = bSuppress ? NULL : va_arg(ap, char*);
                size_t count = width ? width : 1;
                if (strlen(pS) < count) {
                    pS += strlen(pS);
                    goto done_input_or_match;
                }
                if (pOut) {
                    memcpy(pOut, pS, count);
                    nAssigned++;
                }
                pS += count;
                break;
            }
            case '[': {
                char* pOut;
                size_t n;

                unsigned char aSet[256];
                pFmt = xbox_scanf_ParseScanset(pFmt, aSet);
                pOut = bSuppress ? NULL : va_arg(ap, char*);
                n = 0;
                while (*pS && aSet[(unsigned char)*pS] && (!width || n < width)) {
                    if (pOut)
                        pOut[n] = *pS;
                    n++;
                    pS++;
                }
                if (!n)
                    goto done;
                if (pOut) {
                    pOut[n] = 0;
                    nAssigned++;
                }
                break;
            }
            default:
                goto done;
        }
    }
    goto done;

done_input_or_match:
    // Running out of input before anything is assigned is an input failure.
    if (!*pS && !nAssigned) {
        va_end(ap);
        return EOF;
    }
done:
    va_end(ap);
    return nAssigned;
}

int sscanf(const char* pStr, const char* pFmt, ...)
{
    int ret;

    va_list ap;
    va_start(ap, pFmt);
    ret = vsscanf(pStr, pFmt, ap);
    va_end(ap);
    return ret;
}
