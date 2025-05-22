message(STATUS "Python interpreter: ${PYTHON}")

file(GLOB SRCS "*.c")

set(WEB_DIR "${COMPONENT_DIR}/web")
file(MAKE_DIRECTORY ${GEN_DIR})

# Force CMake to re-run this script on each build to detect new files
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WEB_DIR}")

# enumerate all files in the web directory
file(GLOB_RECURSE WEB_FILES "${WEB_DIR}/*")
message(STATUS "Web files: ${WEB_FILES}")
set(WEB_FILES_RELATIVE "")
foreach(f IN LISTS WEB_FILES)
    file(RELATIVE_PATH asset_rel
         "${COMPONENT_DIR}"
         "${f}")
    list(APPEND WEB_FILES_RELATIVE "${asset_rel}")
endforeach()
message(STATUS "Web files relative: ${WEB_FILES_RELATIVE}")

execute_process(
    COMMAND "${PYTHON}" "${COMPONENT_DIR}/generate.py"
            "${COMPONENT_NAME}"
            "${WEB_DIR}"
            "${COMPONENT_DIR}/app_files.h"
            "${COMPONENT_DIR}/app_files.c"
    RESULT_VARIABLE _gen_rv
    # OUTPUT_QUIET ERROR_QUIET
)
if(NOT _gen_rv EQUAL 0)
    message(FATAL_ERROR "app_files generate.py failed (rv=${_gen_rv})")
endif()

# Clean the build directory to ensure proper regeneration
execute_process(
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/esp-idf/app_files
    RESULT_VARIABLE _clean_rv
)

# Set each web file as a configure dependency to force CMake to re-run when any file changes
foreach(web_file ${WEB_FILES})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${web_file}")
endforeach()

# Create a directory for our custom embedded files
idf_component_register(
    SRCS
        "${SRCS}"
    INCLUDE_DIRS
        "."
    EMBED_FILES
        "${WEB_FILES_RELATIVE}"
    REQUIRES
        esp_http_server
        uthash
)
