idf_component_register(SRCS "main.c" "es8311.c" "i2s_sink.c"
                       INCLUDE_DIRS "."
                       REQUIRES player esp_driver_i2c esp_driver_i2s)
