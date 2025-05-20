file(GLOB SRCS "*.c")

idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_wifi
                        nvs_flash
                        spiffs_utils
                        # esp_google_protobuf
                        # proto_files
                        httpx_server
                        captive_portal
                    PRIV_REQUIRES spi_flash)

spiffs_create_partition_image(spiffs ${CMAKE_SOURCE_DIR}/data FLASH_IN_PROJECT)
