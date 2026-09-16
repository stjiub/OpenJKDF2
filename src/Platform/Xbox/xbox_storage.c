#include "xbox_storage.h"

#include <windows.h>
#include <nxdk/mount.h>
#include <string.h>

#define XBOX_DISC_ROOT "D:\\"
#define XBOX_DATA_ROOT "E:\\UDATA\\OpenJKDF2\\"

#define XBOX_GAME_DIR_DF2 "jk1\\"
#define XBOX_GAME_DIR_MOTS "mots\\"

static int xbox_storage_bDataRootReady = 0;
static int xbox_storage_bWritable = 0;
static char xbox_storage_aDiscRoot[sizeof(XBOX_DISC_ROOT XBOX_GAME_DIR_MOTS)] = XBOX_DISC_ROOT XBOX_GAME_DIR_DF2;
static char xbox_storage_aDataRoot[sizeof(XBOX_DATA_ROOT XBOX_GAME_DIR_MOTS)] = XBOX_DATA_ROOT XBOX_GAME_DIR_DF2;

int xbox_storage_init(void)
{
    if (!nxIsDriveMounted('E') && !nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\"))
        return 0;

    CreateDirectoryA("E:\\UDATA", NULL);
    if (!CreateDirectoryA(XBOX_DATA_ROOT, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;

    xbox_storage_bDataRootReady = 1;
    xbox_storage_SetGame(0);
    return xbox_storage_bWritable;
}

void xbox_storage_SetGame(int bMots)
{
    const char* pGameDir = bMots ? XBOX_GAME_DIR_MOTS : XBOX_GAME_DIR_DF2;

    // Both roots are sized for the longest game directory.
    strcpy(xbox_storage_aDiscRoot, XBOX_DISC_ROOT);
    strcat(xbox_storage_aDiscRoot, pGameDir);
    strcpy(xbox_storage_aDataRoot, XBOX_DATA_ROOT);
    strcat(xbox_storage_aDataRoot, pGameDir);

    xbox_storage_bWritable = xbox_storage_bDataRootReady
                          && (CreateDirectoryA(xbox_storage_aDataRoot, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
}

static int xbox_storage_IsSeparator(char c)
{
    return c == '/' || c == '\\';
}

static int xbox_storage_HasDirectoryPrefix(const char* pPath, const char* pDirectory, size_t n)
{
    return !_strnicmp(pPath, pDirectory, n) && (pPath[n] == 0 || xbox_storage_IsSeparator(pPath[n]));
}

// The engine hands some paths back through stat() or fopen() after resolving
// them. A path already under a game directory is left alone; a "D:" path that
// is not is the fixed working directory the engine prepends to relative paths.
static int xbox_storage_IsResolved(const char* pPath)
{
    if (_strnicmp(pPath, XBOX_DISC_ROOT, sizeof(XBOX_DISC_ROOT) - 1))
        return 0;

    pPath += sizeof(XBOX_DISC_ROOT) - 1;
    return xbox_storage_HasDirectoryPrefix(pPath, XBOX_GAME_DIR_DF2, sizeof(XBOX_GAME_DIR_DF2) - 2)
        || xbox_storage_HasDirectoryPrefix(pPath, XBOX_GAME_DIR_MOTS, sizeof(XBOX_GAME_DIR_MOTS) - 2);
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

    if (pRelative[0] && pRelative[1] == ':' && (_strnicmp(pRelative, "D:", 2) || xbox_storage_IsResolved(pRelative))) {
        pRoot = "";
    }
    else {
        if (pRelative[0] && pRelative[1] == ':')
            pRelative += 2;
        while (xbox_storage_IsSeparator(pRelative[0]) || (pRelative[0] == '.' && xbox_storage_IsSeparator(pRelative[1])))
            pRelative += xbox_storage_IsSeparator(pRelative[0]) ? 1 : 2;

        if (pRelative[0] == '.' && pRelative[1] == '.' && xbox_storage_IsSeparator(pRelative[2])) {
            // The mods menu looks for the other game's data as "../<game>/...".
            pRelative += 3;
            pRoot = XBOX_DISC_ROOT;
        }
        else {
            pRoot = (xbox_storage_bWritable && xbox_storage_IsWritable(pRelative)) ? xbox_storage_aDataRoot : xbox_storage_aDiscRoot;
        }
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
            pResolved[0] = 0;
            return 0;
        }
        pResolved[n++] = c;
        prev = c;
    }
    pResolved[n] = 0;
    return 1;
}
