# Portable UI: framebuffer, font, and player screens. No ESP-IDF dependencies.
set(UI_SRCS "fb.c" "font5x7.c" "ui_app.c")

if(ESP_PLATFORM)
    idf_component_register(SRCS ${UI_SRCS}
                           INCLUDE_DIRS "include"
                           REQUIRES player)
else()
    add_library(tinydap_ui STATIC ${UI_SRCS})
    target_include_directories(tinydap_ui PUBLIC include)
    target_link_libraries(tinydap_ui PUBLIC tinydap_player)
endif()
