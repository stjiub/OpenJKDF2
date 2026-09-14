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
# pbkit polls state shared with its interrupt handlers through non-volatile
# variables, so it keeps nxdk's default of no optimization.
set_property(SOURCE
    ${XBOX_PBKIT_DIR}/pbkit.c
    ${XBOX_PBKIT_DIR}/pbkit_gamma.c
    ${XBOX_PBKIT_DIR}/pbkit_dma.c
    ${XBOX_PBKIT_DIR}/pbkit_draw.c
    ${XBOX_PBKIT_DIR}/pbkit_print.c
    ${XBOX_PBKIT_DIR}/pbkit_pushbuffer.c
    APPEND PROPERTY COMPILE_OPTIONS "-O0")

file(GLOB XBOX_PBGL_SRCS ${PBGL_DIR}/src/*.c)
list(APPEND ENGINE_SOURCE_FILES ${XBOX_PBGL_SRCS})

# Mounts D: to the XBE's directory at startup. nxdk's Makefiles link this
# in by default; its CMake toolchain doesn't.
list(APPEND ENGINE_SOURCE_FILES $ENV{NXDK_DIR}/lib/nxdk/automount_d.c)

# Gamepads go through nxdk's USB host stack; its toolchain links nxdk_usb.lib.
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/Platform/Xbox/xbox_input.c PROPERTIES
    INCLUDE_DIRECTORIES "$ENV{NXDK_DIR}/lib/usb/libusbohci/inc;$ENV{NXDK_DIR}/lib/usb/libusbohci_xbox"
    COMPILE_DEFINITIONS "USBH_USE_EXTERNAL_CONFIG=\"usbh_config_xbox.h\"")

# Music is Ogg Vorbis, decoded on the console.
list(APPEND ENGINE_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/external/stb_vorbis/stb_vorbis.c)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/external/stb_vorbis/stb_vorbis.c PROPERTIES
    COMPILE_OPTIONS "-w")

# The mixer and decoder run for every output sample on the audio threads, so
# they stay optimized in Debug builds too.
set_property(SOURCE
    ${PROJECT_SOURCE_DIR}/src/Platform/Xbox/stdSound.c
    ${PROJECT_SOURCE_DIR}/src/Platform/Xbox/xbox_audio.c
    ${PROJECT_SOURCE_DIR}/src/Platform/Xbox/xbox_music.c
    ${PROJECT_SOURCE_DIR}/src/external/stb_vorbis/stb_vorbis.c
    APPEND PROPERTY COMPILE_OPTIONS "-O2")

list(REMOVE_ITEM ENGINE_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/Main/jkQuakeConsole.c)
