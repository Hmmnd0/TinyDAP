idf_component_register(SRCS "main.c" "oled.c" "buttons.c" "../../main/i2s_sink.c"
                       INCLUDE_DIRS "." "../../main"
                       REQUIRES player ui esp_driver_i2s esp_driver_spi esp_driver_gpio esp_timer)
