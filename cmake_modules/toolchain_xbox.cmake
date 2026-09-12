# Original Xbox (nxdk). Requires NXDK_DIR in the environment and nxdk's host
# tools built (`make tools` in $NXDK_DIR).

if(NOT DEFINED ENV{NXDK_DIR})
    message(FATAL_ERROR "NXDK_DIR is not set.")
endif()

include($ENV{NXDK_DIR}/share/toolchain-nxdk.cmake)

# FORCE so CMakeLists.txt's `set(PLAT_XBOX FALSE CACHE ...)` default can't win.
set(PLAT_XBOX TRUE CACHE BOOL "Original Xbox (nxdk)" FORCE)
