file(GLOB SRCS "*.c")


idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        nvs_flash
        spiffs_utils
        # esp_google_protobuf
        # proto_files
        app_files
        httpx_server
        captive_portal
    PRIV_REQUIRES spi_flash
)


if(EXISTS "../data/cert")
    file(GLOB CERT_FILES "../data/cert/*")
    message(STATUS "Cert files: ${CERT_FILES}")
    spiffs_create_partition_image(cert "../data/cert" FLASH_IN_PROJECT)
else()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/empty_cert")
    message(WARNING "No cert files found, creating empty cert directory")
    spiffs_create_partition_image(cert "${CMAKE_BINARY_DIR}/empty_cert" FLASH_IN_PROJECT)
endif()
