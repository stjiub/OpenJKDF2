// nxdk's own _mkdir/_rmdir/_chdir assert (halting the console) on errors they
// don't recognize, so these call the WinAPI directly.

#include <sys/stat.h>
#include <dirent.h>

#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "xbox_storage.h"

static int xbox_posix_FailWithWin32Error(void)
{
    switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            errno = ENOENT;
            break;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            errno = EEXIST;
            break;
        case ERROR_ACCESS_DENIED:
        case ERROR_WRITE_PROTECT:
            errno = EACCES;
            break;
        case ERROR_DIR_NOT_EMPTY:
            errno = ENOTEMPTY;
            break;
        default:
            errno = EIO;
            break;
    }
    return -1;
}

static int xbox_posix_ResolvePath(const char* pPath, char* pOut, size_t outsz)
{
    if (!xbox_resolve_path(pPath, pOut, outsz)) {
        errno = ENAMETOOLONG;
        return 0;
    }
    return 1;
}

int stat(const char* pPath, struct stat* pBuf)
{
    ULARGE_INTEGER mtime;

    char aResolved[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA data;

    if (!xbox_posix_ResolvePath(pPath, aResolved, sizeof(aResolved)))
        return -1;
    if (!GetFileAttributesExA(aResolved, GetFileExInfoStandard, &data))
        return xbox_posix_FailWithWin32Error();

    mtime.LowPart = data.ftLastWriteTime.dwLowDateTime;
    mtime.HighPart = data.ftLastWriteTime.dwHighDateTime;

    pBuf->st_mode = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
    pBuf->st_size = (long)data.nFileSizeLow;
    pBuf->st_mtime = (long)((mtime.QuadPart - 116444736000000000ULL) / 10000000ULL);
    return 0;
}

int mkdir(const char* pPath, int mode)
{
    char aResolved[MAX_PATH];
    (void)mode;

    if (!xbox_posix_ResolvePath(pPath, aResolved, sizeof(aResolved)))
        return -1;
    return CreateDirectoryA(aResolved, NULL) ? 0 : xbox_posix_FailWithWin32Error();
}

int rmdir(const char* pPath)
{
    char aResolved[MAX_PATH];

    if (!xbox_posix_ResolvePath(pPath, aResolved, sizeof(aResolved)))
        return -1;
    return RemoveDirectoryA(aResolved) ? 0 : xbox_posix_FailWithWin32Error();
}

int unlink(const char* pPath)
{
    char aResolved[MAX_PATH];

    if (!xbox_posix_ResolvePath(pPath, aResolved, sizeof(aResolved)))
        return -1;
    return DeleteFileA(aResolved) ? 0 : xbox_posix_FailWithWin32Error();
}

// The working directory is fixed at the disc root; xbox_resolve_path()
// redirects writable data from there.
char* getcwd(char* pBuf, size_t size)
{
    static const char xbox_posix_aCwd[] = "D:\\";

    if (!pBuf) {
        size = size > sizeof(xbox_posix_aCwd) ? size : sizeof(xbox_posix_aCwd);
        pBuf = (char*)malloc(size);
        if (!pBuf) {
            errno = ENOMEM;
            return NULL;
        }
    }
    else if (size < sizeof(xbox_posix_aCwd)) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(pBuf, xbox_posix_aCwd, sizeof(xbox_posix_aCwd));
    return pBuf;
}

int chdir(const char* pPath)
{
    char aResolved[MAX_PATH];

    if (!xbox_posix_ResolvePath(pPath, aResolved, sizeof(aResolved)))
        return -1;
    if (_stricmp(aResolved, "D:\\")) {
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

char* strtok_r(char* pStr, const char* pDelim, char** pSaveptr)
{
    char* pStart;

    if (!pStr)
        pStr = *pSaveptr;

    pStr += strspn(pStr, pDelim);
    if (*pStr == '\0') {
        *pSaveptr = pStr;
        return NULL;
    }

    pStart = pStr;
    pStr = strpbrk(pStart, pDelim);
    if (pStr) {
        *pStr = '\0';
        *pSaveptr = pStr + 1;
    }
    else {
        *pSaveptr = pStart + strlen(pStart);
    }
    return pStart;
}

char* strsep(char** pStringp, const char* pDelim)
{
    char* pStart = *pStringp;
    char* pP;

    if (!pStart)
        return NULL;

    pP = strpbrk(pStart, pDelim);
    if (pP) {
        *pP = '\0';
        *pStringp = pP + 1;
    }
    else {
        *pStringp = NULL;
    }
    return pStart;
}

int strcasecmp(const char* pA, const char* pB)
{
    return _stricmp(pA, pB);
}

struct DIR {
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    int bHavePending;
    struct dirent entry;
};

static int xbox_posix_MakeFindPattern(const char* pDir, char* pOut, size_t outsz)
{
    size_t n;

    if (!xbox_posix_ResolvePath(pDir, pOut, outsz - 2))
        return 0;

    n = strlen(pOut);
    if (n && pOut[n - 1] != '\\')
        pOut[n++] = '\\';
    pOut[n++] = '*';
    pOut[n] = 0;
    return 1;
}

static int xbox_posix_IsDotEntry(const char* pName)
{
    return !strcmp(pName, ".") || !strcmp(pName, "..");
}

static void xbox_posix_FillDirent(struct dirent* pEntry, const char* pName, unsigned char type)
{
    strncpy(pEntry->d_name, pName, sizeof(pEntry->d_name) - 1);
    pEntry->d_name[sizeof(pEntry->d_name) - 1] = 0;
    pEntry->d_type = type;
}

static unsigned char xbox_posix_FindDataType(const WIN32_FIND_DATAA* pFindData)
{
    return (pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG;
}

DIR* opendir(const char* pPath)
{
    char aPattern[MAX_PATH];
    DIR* pD;

    if (!xbox_posix_MakeFindPattern(pPath, aPattern, sizeof(aPattern)))
        return NULL;

    pD = (DIR*)malloc(sizeof(DIR));
    if (!pD) {
        errno = ENOMEM;
        return NULL;
    }

    pD->hFind = FindFirstFileA(aPattern, &pD->findData);
    if (pD->hFind == INVALID_HANDLE_VALUE) {
        free(pD);
        xbox_posix_FailWithWin32Error();
        return NULL;
    }
    pD->bHavePending = 1;
    return pD;
}

struct dirent* readdir(DIR* pDirp)
{
    if (!pDirp)
        return NULL;

    if (!pDirp->bHavePending && !FindNextFileA(pDirp->hFind, &pDirp->findData))
        return NULL;
    pDirp->bHavePending = 0;

    xbox_posix_FillDirent(&pDirp->entry, pDirp->findData.cFileName, xbox_posix_FindDataType(&pDirp->findData));
    return &pDirp->entry;
}

int closedir(DIR* pDirp)
{
    if (!pDirp)
        return -1;

    FindClose(pDirp->hFind);
    free(pDirp);
    return 0;
}

int alphasort(const struct dirent** pA, const struct dirent** pB)
{
    return strcmp((*pA)->d_name, (*pB)->d_name);
}

static void xbox_posix_FreeDirentList(struct dirent** pList, int count)
{
    for (int i = 0; i < count; i++)
        free(pList[i]);
    free(pList);
}

static int xbox_posix_ScandirAdd(struct dirent*** pList, int* pCount, int* pCapacity,
                       const char* pName, unsigned char type,
                       int (*fnFilter)(const struct dirent*))
{
    struct dirent* pCopy;

    struct dirent entry;

    xbox_posix_FillDirent(&entry, pName, type);
    if (fnFilter && !fnFilter(&entry))
        return 1;

    if (*pCount == *pCapacity) {
        int newCapacity = *pCapacity ? *pCapacity * 2 : 16;
        struct dirent** pGrown = (struct dirent**)realloc(*pList, newCapacity * sizeof(**pList));
        if (!pGrown)
            return 0;
        *pList = pGrown;
        *pCapacity = newCapacity;
    }

    pCopy = (struct dirent*)malloc(sizeof(entry));
    if (!pCopy)
        return 0;
    *pCopy = entry;
    (*pList)[(*pCount)++] = pCopy;
    return 1;
}

// XDVDFS and FATX have no "." / ".." entries, but callers such as
// stdFileUtil_FindNext rely on scandir() returning them first.
int scandir(const char* pDirp, struct dirent*** pNamelist,
            int (*fnFilter)(const struct dirent*),
            int (*fnCompare)(const struct dirent**, const struct dirent**))
{
    char aPattern[MAX_PATH];
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    struct dirent** pList = NULL;
    int count = 0;
    int capacity = 0;
    int bOk;

    *pNamelist = NULL;

    if (!xbox_posix_MakeFindPattern(pDirp, aPattern, sizeof(aPattern)))
        return -1;

    hFind = FindFirstFileA(aPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return xbox_posix_FailWithWin32Error();

    bOk = xbox_posix_ScandirAdd(&pList, &count, &capacity, ".", DT_DIR, fnFilter)
       && xbox_posix_ScandirAdd(&pList, &count, &capacity, "..", DT_DIR, fnFilter);
    do {
        if (bOk && !xbox_posix_IsDotEntry(findData.cFileName))
            bOk = xbox_posix_ScandirAdd(&pList, &count, &capacity, findData.cFileName, xbox_posix_FindDataType(&findData), fnFilter);
    } while (bOk && FindNextFileA(hFind, &findData));
    FindClose(hFind);

    if (!bOk) {
        xbox_posix_FreeDirentList(pList, count);
        errno = ENOMEM;
        return -1;
    }

    if (fnCompare && count > 1)
        qsort(pList, count, sizeof(*pList), (int (*)(const void* , const void*))fnCompare);

    *pNamelist = pList;
    return count;
}
