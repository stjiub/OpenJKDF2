#ifndef _XBOX_STORAGE_H
#define _XBOX_STORAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mounts the HDD user partition and creates the writable data directory.
// Returns 0 if writable storage is unavailable; the game still runs, but
// config and saves will not persist.
int xbox_storage_init(void);

// Selects the game whose data the engine runs from. Each game keeps its
// assets in its own directory on the disc, with matching writable storage,
// since nxdk has no working directory to switch between them.
void xbox_storage_SetGame(int bMots);

// nxdk has no working directory: every path handed to the kernel must be
// absolute. Maps an engine path (relative, or rooted at the fixed "D:\"
// cwd) to either the read-only disc or the writable data directory.
// Both roots use the current game's directory. A path starting with ".."
// resolves against the disc root instead, naming the other game's data.
// Returns 0 if the result was truncated.
int xbox_resolve_path(const char* pPath, char* pResolved, size_t outsz);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_STORAGE_H
