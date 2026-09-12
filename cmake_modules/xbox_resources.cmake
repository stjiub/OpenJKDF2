# Added: MSYS2 does not convert paths inside the joined resource list.
find_program(CYGPATH_EXECUTABLE cygpath)
set(EMBEDDED_RESOURCES_NATIVE "")
foreach(_res_file ${EMBEDDED_RESOURCES})
    if(CYGPATH_EXECUTABLE)
        execute_process(
            COMMAND ${CYGPATH_EXECUTABLE} -w "${_res_file}"
            RESULT_VARIABLE _res_file_result
            OUTPUT_VARIABLE _res_file_native
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _res_file_result EQUAL 0)
            message(FATAL_ERROR "cygpath failed for ${_res_file}")
        endif()
    else()
        set(_res_file_native "${_res_file}")
    endif()
    list(APPEND EMBEDDED_RESOURCES_NATIVE "${_res_file_native}")
endforeach()
list(JOIN EMBEDDED_RESOURCES_NATIVE "+" EMBEDDED_RESOURCES_SEPARATED)
