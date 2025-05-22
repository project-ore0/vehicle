file(GLOB SRCS "*.c")


set(WEB_DIR "${COMPONENT_DIR}/web")
file(MAKE_DIRECTORY ${GEN_DIR})

message(STATUS "Python interpreter: ${PYTHON}")

get_filename_component(COMPONENT_NAME "${COMPONENT_DIR}" NAME)

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

# 3) Register component: your handler + the generated source + embedded web assets
idf_component_register(
    SRCS
        "${SRCS}"
    INCLUDE_DIRS
        "."
    EMBED_FILES
        web
    REQUIRES
        esp_http_server
)
