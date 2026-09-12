// zlib's zconf.h includes <sys/types.h> for off_t, which nxdk lacks.
#ifndef _XBOX_SHIM_SYS_TYPES_H
#define _XBOX_SHIM_SYS_TYPES_H

#include <stddef.h>

#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long off_t;
#endif

#endif // _XBOX_SHIM_SYS_TYPES_H
