macro(plat_initialize)
    message(STATUS "Targeting Original Xbox (nxdk)")

    if(NOT DEFINED ENV{NXDK_DIR})
        message(FATAL_ERROR "NXDK_DIR must be set in the environment (see toolchain_xbox.cmake).")
    endif()
    set(NXDK_DIR $ENV{NXDK_DIR})

    set(BIN_NAME "openjkdf2")
    set(XISO_NAME "openjkdf2.iso")

    find_program(CXBE cxbe HINTS ${NXDK_DIR}/tools/cxbe)
    find_program(EXTRACT_XISO extract-xiso HINTS ${NXDK_DIR}/tools/extract-xiso/build)
    if(NOT CXBE OR NOT EXTRACT_XISO)
        message(WARNING "cxbe/extract-xiso not found; run `make tools` in ${NXDK_DIR}. "
                        "The .exe will build but won't be packaged into an XISO.")
    endif()
    find_program(XBOX_LLVM_NM llvm-nm)
    if(NOT XBOX_LLVM_NM)
        message(WARNING "llvm-nm not found; skipping the check for calls to unimplemented nxdk functions.")
    endif()

    add_definitions(-DPLAT_MISSING_WIN32)
    add_definitions(-DTARGET_XBOX)
    add_definitions(-DTARGET_RETRO_HOMEBREW)
    add_definitions(-D_XOPEN_SOURCE=500)
    add_definitions(-D_DEFAULT_SOURCE)
    add_definitions(-DSMK_FAST)

    set(TARGET_USE_PHYSFS FALSE)
    set(TARGET_USE_GAMENETWORKINGSOCKETS FALSE)
    set(TARGET_USE_LIBSMACKER TRUE)
    set(TARGET_USE_LIBSMUSHER TRUE)
    set(TARGET_USE_SDL2 FALSE)
    set(TARGET_USE_OPENGL FALSE)
    set(TARGET_USE_OPENGL11 TRUE)
    set(TARGET_USE_OPENAL FALSE)
    set(TARGET_POSIX TRUE)
    set(TARGET_NO_BLOBS TRUE)
    set(TARGET_CAN_JKGM FALSE)
    set(OPENJKDF2_NO_ASAN TRUE)
    set(TARGET_USE_CURL FALSE)
    set(TARGET_FIND_OPENAL FALSE)
    set(TARGET_NO_MULTIPLAYER_MENUS TRUE)

    set(TARGET_BUILD_TESTS FALSE)
    set(SDL2_COMMON_LIBS "")

    set(TARGET_XBOX TRUE)

    add_compile_options(-Wall -Wno-unused-variable -Wno-parentheses -Wno-missing-braces)
    add_compile_options(-fno-exceptions)

    # nxdk's toolchain sets no optimization level of its own. Frame pointers
    # are kept so GDB can walk the stack in any build type.
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
    add_compile_options(-fno-omit-frame-pointer)

    include_directories(${NXDK_DIR}/lib)
    include_directories(${NXDK_DIR}/lib/pbkit)

    if(DEFINED ENV{PBGL_DIR})
        set(PBGL_DIR $ENV{PBGL_DIR})
    elseif(NOT DEFINED PBGL_DIR)
        set(PBGL_DIR ${PROJECT_SOURCE_DIR}/../pbgl)
    endif()
    if(NOT EXISTS ${PBGL_DIR}/include/pbgl.h)
        message(FATAL_ERROR "pbgl not found at ${PBGL_DIR}; set -DPBGL_DIR or $ENV{PBGL_DIR}.")
    endif()
    include_directories(${PBGL_DIR}/include)

    set(XBOX_SHIMS_DIR ${PROJECT_SOURCE_DIR}/cmake_modules/xbox_shims)
    include_directories(${XBOX_SHIMS_DIR})
    add_compile_options("SHELL:-include ${XBOX_SHIMS_DIR}/xbox_compat.h")
endmacro()

macro(plat_specific_deps)
    set(SDL2_COMMON_LIBS "")
endmacro()

# Copies one game's install into its own directory on the disc: the resource
# and episode files it needs, its cutscenes, and its packaged Ogg soundtrack.
# Does nothing if DATA_DIR is empty.
function(xbox_copy_game_data)
    cmake_parse_arguments(ARG "" "DATA_DIR;XISO_DIR;GAME_DIR;CHECK_FILE;VIDEO_EXT" "RESOURCE;EPISODE" ${ARGN})

    if(NOT ARG_DATA_DIR)
        return()
    endif()
    if(NOT EXISTS "${ARG_DATA_DIR}/${ARG_CHECK_FILE}")
        message(FATAL_ERROR "No ${ARG_GAME_DIR} game data at ${ARG_DATA_DIR}")
    endif()

    set(gameDir ${ARG_XISO_DIR}/${ARG_GAME_DIR})
    foreach(name IN LISTS ARG_RESOURCE)
        file(COPY "${ARG_DATA_DIR}/Resource/${name}" DESTINATION ${gameDir}/Resource)
    endforeach()
    foreach(name IN LISTS ARG_EPISODE)
        file(COPY "${ARG_DATA_DIR}/Episode/${name}" DESTINATION ${gameDir}/Episode)
    endforeach()

    file(GLOB video "${ARG_DATA_DIR}/Resource/VIDEO/*.${ARG_VIDEO_EXT}")
    if(video)
        file(COPY ${video} DESTINATION ${gameDir}/Resource/VIDEO)
    endif()

    file(GLOB music "${ARG_DATA_DIR}/MUSIC/Track*.ogg")
    if(music)
        file(COPY ${music} DESTINATION ${gameDir}/MUSIC)
    endif()
endfunction()

macro(plat_link_and_package)
    target_link_libraries(sith_engine PRIVATE nlohmann_json::nlohmann_json)

    # Symbol map for resolving addresses from the debugger or a crash.
    target_link_options(${BIN_NAME} PRIVATE -map:${BIN_NAME}.map)

    if(CXBE AND EXTRACT_XISO)
        set(XBOX_XISO_DIR ${CMAKE_CURRENT_BINARY_DIR}/xiso)

        # Game data to put on the disc. Under an MSYS2-hosted CMake, install paths
        # must be POSIX-style (/d/...), not D:/...
        set(XBOX_DF2_DATA_DIR "" CACHE PATH "Jedi Knight install to copy onto the XISO")
        set(XBOX_MOTS_DATA_DIR "" CACHE PATH "Mysteries of the Sith install to copy onto the XISO")

        # Each game gets its own directory on the disc, the layout the engine
        # switches between when it restarts into the other game.
        xbox_copy_game_data(DATA_DIR "${XBOX_DF2_DATA_DIR}" XISO_DIR ${XBOX_XISO_DIR} GAME_DIR jk1
            CHECK_FILE Resource/Res1hi.gob VIDEO_EXT SMK
            RESOURCE Res1hi.gob Res2.gob JK_.CD
            EPISODE JK1.GOB)
        xbox_copy_game_data(DATA_DIR "${XBOX_MOTS_DATA_DIR}" XISO_DIR ${XBOX_XISO_DIR} GAME_DIR mots
            CHECK_FILE Resource/JKMRES.GOO VIDEO_EXT SAN
            RESOURCE JKMRES.GOO JKMsndLO.goo JK_.CD
            EPISODE JKM.GOO)

        set(XBOX_CHECK_STUBS_CMD "")
        if(XBOX_LLVM_NM)
            set(XBOX_CHECK_STUBS_CMD COMMAND ${CMAKE_COMMAND} -DNXDK_DIR=${NXDK_DIR} -DNM=${XBOX_LLVM_NM}
                -P ${PROJECT_SOURCE_DIR}/cmake_modules/xbox_check_stubs.cmake)
        endif()

        set(XBOX_TITLE "OpenJKDF2" CACHE STRING "Title the dashboard shows for the XBE (1 to 40 characters)")
        string(LENGTH "${XBOX_TITLE}" XBOX_TITLE_LEN)
        if(XBOX_TITLE_LEN EQUAL 0 OR XBOX_TITLE_LEN GREATER 40)
            message(FATAL_ERROR "XBOX_TITLE must be 1 to 40 characters: \"${XBOX_TITLE}\"")
        endif()
        # Only rewritten when the title changes, so a new title re-runs cxbe.
        set(XBOX_TITLE_STAMP ${CMAKE_CURRENT_BINARY_DIR}/xbox_title.txt)
        file(CONFIGURE OUTPUT ${XBOX_TITLE_STAMP} CONTENT "${XBOX_TITLE}")

        set(XBOX_XBE_OUT ${XBOX_XISO_DIR}/default.xbe)
        add_custom_command(
            OUTPUT ${XBOX_XBE_OUT}
            ${XBOX_CHECK_STUBS_CMD}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${XBOX_XISO_DIR}
            COMMAND ${CXBE} -OUT:${XBOX_XBE_OUT} -TITLE:${XBOX_TITLE} $<TARGET_FILE:${BIN_NAME}>
            DEPENDS ${BIN_NAME} ${XBOX_TITLE_STAMP}
            COMMENT "cxbe: $<TARGET_FILE_NAME:${BIN_NAME}> -> default.xbe"
            VERBATIM)
        add_custom_target(${BIN_NAME}_xbe ALL DEPENDS ${XBOX_XBE_OUT})

        # extract-xiso needs a relative output name: MSYS2 mangles the drive
        # letter out of an absolute one.
        add_custom_target(${XISO_NAME} ALL
            DEPENDS ${BIN_NAME}_xbe
            BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${XISO_NAME}
            COMMAND ${EXTRACT_XISO} -c ${XBOX_XISO_DIR} ${XISO_NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "extract-xiso: packaging ${XISO_NAME}"
            VERBATIM)
    endif()
endmacro()

macro(plat_extra_deps)
endmacro()
