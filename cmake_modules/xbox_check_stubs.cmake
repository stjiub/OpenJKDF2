# Fails if any object we compiled references a libc function that nxdk
# implements as a bare assert(0), which halts the console when called.
# Run from the build directory:
#   cmake -DNXDK_DIR=... -DNM=llvm-nm -P xbox_check_stubs.cmake

file(GLOB_RECURSE srcs
    "${NXDK_DIR}/lib/pdclib/platform/xbox/functions/*.c"
    "${NXDK_DIR}/lib/xboxrt/*.c")

set(ws "[ \t\r\n]*")
set(stubs "")
foreach(src ${srcs})
    file(READ "${src}" text)
    string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*${ws}\\([^)]*\\)${ws}{${ws}assert${ws}\\(${ws}0[ \t]*[\\)&]" defs "${text}")
    foreach(def ${defs})
        string(REGEX REPLACE "^([A-Za-z_][A-Za-z0-9_]*).*" "_\\1" name "${def}")
        list(APPEND stubs ${name})
    endforeach()
endforeach()

# Relative paths: NM is a native Windows tool and can't open MSYS-style /d/... paths.
file(GLOB_RECURSE objs RELATIVE "${CMAKE_CURRENT_BINARY_DIR}" "CMakeFiles/*.obj")
string(REPLACE ";" "\n" rsp "${objs}")
file(WRITE "xbox_check_stubs.rsp" "${rsp}\n")
execute_process(
    COMMAND "${NM}" -u -A --format=posix @xbox_check_stubs.rsp
    OUTPUT_VARIABLE undefined
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${NM} failed (${rc})")
endif()

set(found "")
foreach(name ${stubs})
    string(REGEX MATCHALL "[^\n]*: ${name} U" refs "${undefined}")
    foreach(ref ${refs})
        list(APPEND found "${ref}")
    endforeach()
endforeach()

if(found)
    list(REMOVE_DUPLICATES found)
    string(REPLACE ";" "\n  " found "${found}")
    message(FATAL_ERROR "Calls to nxdk functions that are unimplemented (assert and halt):\n  ${found}")
endif()
