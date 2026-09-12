file(GLOB TARGET_XBOX_SRCS ${PROJECT_SOURCE_DIR}/src/Platform/Xbox/*.c)
list(APPEND ENGINE_SOURCE_FILES ${TARGET_XBOX_SRCS})

# pbkit and pbgl are built from source; nxdk's toolchain doesn't link them.
# This list matches $NXDK_DIR/lib/pbkit/Makefile.
set(XBOX_PBKIT_DIR $ENV{NXDK_DIR}/lib/pbkit)
list(APPEND ENGINE_SOURCE_FILES
    ${XBOX_PBKIT_DIR}/pbkit.c
    ${XBOX_PBKIT_DIR}/pbkit_gamma.c
    ${XBOX_PBKIT_DIR}/pbkit_dma.c
    ${XBOX_PBKIT_DIR}/pbkit_draw.c
    ${XBOX_PBKIT_DIR}/pbkit_print.c
    ${XBOX_PBKIT_DIR}/pbkit_pushbuffer.c)

file(GLOB XBOX_PBGL_SRCS ${PBGL_DIR}/src/*.c)
list(APPEND ENGINE_SOURCE_FILES ${XBOX_PBGL_SRCS})

# Mounts D: to the XBE's directory at startup. nxdk's Makefiles link this
# in by default; its CMake toolchain doesn't.
list(APPEND ENGINE_SOURCE_FILES $ENV{NXDK_DIR}/lib/nxdk/automount_d.c)

list(REMOVE_ITEM ENGINE_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/Main/jkQuakeConsole.c)
