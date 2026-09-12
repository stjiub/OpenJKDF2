#include "xbox_storage.h"

#include <windows.h>
#include <nxdk/mount.h>
#include <string.h>

#define XBOX_DISC_ROOT "D:\\"
#define XBOX_DATA_ROOT "E:\\UDATA\\OpenJKDF2\\"

static int xbox_storage_bWritable = 0;

int xbox_storage_init(void)
{
    if (!nxIsDriveMounted('E') && !nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\"))
        return 0;

    CreateDirectoryA("E:\\UDATA", NULL);
    if (!CreateDirectoryA(XBOX_DATA_ROOT, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;

    xbox_storage_bWritable = 1;
    return 1;
}

static int xbox_storage_IsSeparator(char c)
{
    return c == '/' || c == '\\';
}

static int xbox_storage_HasDirectoryPrefix(const char* pPath, const char* pDirectory, size_t n)
{
    return !_strnicmp(pPath, pDirectory, n) && (pPath[n] == 0 || xbox_storage_IsSeparator(pPath[n]));
}

// Player data and root-level JSON config require writable storage.
static int xbox_storage_IsWritable(const char* pRelative)
{
    const char* pExt;

    if (xbox_storage_HasDirectoryPrefix(pRelative, "player", 6) || xbox_storage_HasDirectoryPrefix(pRelative, "persist", 7))
        return 1;
    if (strpbrk(pRelative, "/\\"))
        return 0;

    pExt = strrchr(pRelative, '.');
    return pExt && !_stricmp(pExt, ".json");
}

int xbox_resolve_path(const char* pPath, char* pResolved, size_t outsz)
{
    const char* pRelative = pPath ? pPath : "";
    const char* pRoot;
    size_t n;
    char prev;

    if (!outsz)
        return 0;

    if (pRelative[0] && pRelative[1] == ':' && _strnicmp(pRelative, "D:", 2)) {
        pRoot = "";
    }
    else {
        if (pRelative[0] && pRelative[1] == ':')
            pRelative += 2;
        while (xbox_storage_IsSeparator(pRelative[0]) || (pRelative[0] == '.' && xbox_storage_IsSeparator(pRelative[1])))
            pRelative += xbox_storage_IsSeparator(pRelative[0]) ? 1 : 2;
        pRoot = (xbox_storage_bWritable && xbox_storage_IsWritable(pRelative)) ? XBOX_DATA_ROOT : XBOX_DISC_ROOT;
    }

    n = strlen(pRoot);
    if (n >= outsz) {
        pResolved[0] = 0;
        return 0;
    }
    memcpy(pResolved, pRoot, n);

    prev = n ? pResolved[n - 1] : 0;
    for (; *pRelative; pRelative++) {
        char c = xbox_storage_IsSeparator(*pRelative) ? '\\' : *pRelative;
        if (c == '\\' && prev == '\\')
            continue;
        if (n + 1 >= outsz) {
            pResolved[n] = 0;
            return 0;
        }
        pResolved[n++] = c;
        prev = c;
    }
    pResolved[n] = 0;
    return 1;
}
