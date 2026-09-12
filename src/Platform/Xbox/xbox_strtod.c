// nxdk's strtod/strtof/strtold are assert(0) stubs and PDCLib has no atof;
// xbox_compat.h renames the standard names to these.

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double xbox_strtod_ScaleByPow10(double x, int exp10)
{
    static const double xbox_strtod_aPowers[] = { 1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256 };
    unsigned int e = exp10 < 0 ? -exp10 : exp10;
    double factor = 1.0;

    for (int i = 0; e && i < (int)(sizeof(xbox_strtod_aPowers) / sizeof(xbox_strtod_aPowers[0])); i++, e >>= 1) {
        if (e & 1)
            factor *= xbox_strtod_aPowers[i];
    }
    if (e)
        factor = HUGE_VAL;
    return exp10 < 0 ? x / factor : x * factor;
}

// Decimal only (no hex floats). Accurate to within an ulp or two, not
// always correctly rounded.
double xbox_strtod(const char* pStr, char** pEndptr)
{
    const unsigned long long maxMantissa = 100000000000000000ULL;
    const char* pP = pStr;
    unsigned long long mantissa = 0;
    int exp10 = 0;
    int nDigits = 0;
    int bNegative = 0;
    double result;

    while (isspace((unsigned char)*pP))
        pP++;
    if (*pP == '+' || *pP == '-')
        bNegative = (*pP++ == '-');

    if (!_strnicmp(pP, "inf", 3)) {
        pP += _strnicmp(pP, "infinity", 8) ? 3 : 8;
        result = HUGE_VAL;
    }
    else if (!_strnicmp(pP, "nan", 3)) {
        pP += 3;
        result = NAN;
    }
    else {
        for (; *pP >= '0' && *pP <= '9'; pP++, nDigits++) {
            if (mantissa < maxMantissa)
                mantissa = mantissa * 10 + (*pP - '0');
            else
                exp10++;
        }
        if (*pP == '.') {
            for (pP++; *pP >= '0' && *pP <= '9'; pP++, nDigits++) {
                if (mantissa < maxMantissa) {
                    mantissa = mantissa * 10 + (*pP - '0');
                    exp10--;
                }
            }
        }
        if (!nDigits) {
            if (pEndptr)
                *pEndptr = (char*)pStr;
            return 0.0;
        }

        if (*pP == 'e' || *pP == 'E') {
            const char* pQ = pP + 1;
            int bExpNegative = 0;
            int e = 0;

            if (*pQ == '+' || *pQ == '-')
                bExpNegative = (*pQ++ == '-');
            if (*pQ >= '0' && *pQ <= '9') {
                for (; *pQ >= '0' && *pQ <= '9'; pQ++) {
                    if (e < 100000)
                        e = e * 10 + (*pQ - '0');
                }
                exp10 += bExpNegative ? -e : e;
                pP = pQ;
            }
        }

        result = mantissa ? xbox_strtod_ScaleByPow10((double)mantissa, exp10) : 0.0;
        if (result == HUGE_VAL || (mantissa && result == 0.0))
            errno = ERANGE;
    }

    if (pEndptr)
        *pEndptr = (char*)pP;
    return bNegative ? -result : result;
}

float xbox_strtof(const char* pStr, char** pEndptr)
{
    return (float)xbox_strtod(pStr, pEndptr);
}

long double xbox_strtold(const char* pStr, char** pEndptr)
{
    return xbox_strtod(pStr, pEndptr);
}

// PDCLib declares atof() but does not implement it.
double atof(const char* pStr)
{
    return xbox_strtod(pStr, NULL);
}
