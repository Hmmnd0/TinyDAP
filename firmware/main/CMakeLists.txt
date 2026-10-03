idf_component_register(SRCS "main.c" "player.c" "es8311.c" "i2s_sink.c"
                            "display.c" "keyboard.c" "sdcard.c"
                       INCLUDE_DIRS "."
                       REQUIRES player ui esp_driver_i2c esp_driver_i2s esp_driver_spi
                                esp_driver_sdspi esp_driver_gpio esp_driver_ledc esp_lcd fatfs sdmmc esp_timer)
