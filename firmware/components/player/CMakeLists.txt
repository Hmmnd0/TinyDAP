# Portable player core: no ESP-IDF or FreeRTOS dependencies, so it builds
# both as an ESP-IDF component and as a plain library for host tests.
set(PLAYER_SRCS "pcm_ring.c" "tone.c")

if(ESP_PLATFORM)
    idf_component_register(SRCS ${PLAYER_SRCS}
                           INCLUDE_DIRS "include")
else()
    add_library(tinydap_player STATIC ${PLAYER_SRCS})
    target_include_directories(tinydap_player PUBLIC include)
    target_link_libraries(tinydap_player PUBLIC m)
endif()
