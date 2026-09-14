// PDCLib's memcpy and memset move one byte per loop iteration. PDCLib defines
// each in its own object, so defining them here keeps its versions out of the link.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void* memcpy(void* restrict pDst, const void* restrict pSrc, size_t n)
{
    void* pRet = pDst;
    size_t head = (0u - (uintptr_t)pDst) & 3;
    size_t words;
    size_t tail;

    if (head > n)
        head = n;
    words = (n - head) / 4;
    tail = (n - head) % 4;

    __asm__ volatile("rep movsb" : "+D"(pDst), "+S"(pSrc), "+c"(head) : : "memory");
    __asm__ volatile("rep movsl" : "+D"(pDst), "+S"(pSrc), "+c"(words) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(pDst), "+S"(pSrc), "+c"(tail) : : "memory");
    return pRet;
}

void* memset(void* pDst, int c, size_t n)
{
    void* pRet = pDst;
    uint32_t pattern = (uint8_t)c * 0x01010101u;
    size_t head = (0u - (uintptr_t)pDst) & 3;
    size_t words;
    size_t tail;

    if (head > n)
        head = n;
    words = (n - head) / 4;
    tail = (n - head) % 4;

    __asm__ volatile("rep stosb" : "+D"(pDst), "+c"(head) : "a"(pattern) : "memory");
    __asm__ volatile("rep stosl" : "+D"(pDst), "+c"(words) : "a"(pattern) : "memory");
    __asm__ volatile("rep stosb" : "+D"(pDst), "+c"(tail) : "a"(pattern) : "memory");
    return pRet;
}
