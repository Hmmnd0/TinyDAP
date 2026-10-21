idf_component_register(SRCS "main.c" "oled.c" "buttons.c" "../../main/i2s_sink.c"
                       INCLUDE_DIRS "." "../../main"
                       REQUIRES player ui esp_driver_i2s esp_driver_spi esp_driver_gpio esp_timer
                                fatfs vfs)

# Test audio stays out of git (see .gitignore); build an image only if present.
set(MUSIC_DIR ${CMAKE_CURRENT_LIST_DIR}/../flash_data)
if(EXISTS ${MUSIC_DIR})
    fatfs_create_rawflash_image(music ${MUSIC_DIR} FLASH_IN_PROJECT PRESERVE_TIME)
endif()
