# Native (macOS/Linux) build of the portable player core, for unit tests and
# pipeline experiments without hardware. See ../README.md.
cmake_minimum_required(VERSION 3.16)
project(tinydap_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

add_subdirectory(../components/player player)
add_subdirectory(../components/ui ui)
find_package(Threads REQUIRED)

enable_testing()

add_executable(test_pcm_ring test_pcm_ring.c)
target_link_libraries(test_pcm_ring tinydap_player Threads::Threads)
add_test(NAME pcm_ring COMMAND test_pcm_ring)

add_executable(tone_to_wav tone_to_wav.c wav_sink.c)
target_link_libraries(tone_to_wav tinydap_player Threads::Threads)

add_executable(ui_demo ui_demo.c)
target_link_libraries(ui_demo tinydap_ui)

add_executable(decode_to_wav decode_to_wav.c wav_sink.c)
target_link_libraries(decode_to_wav tinydap_player)

add_executable(stack_check stack_check.c)
target_link_libraries(stack_check tinydap_player Threads::Threads)

# Optional: cmake -DTINYDAP_TEST_FLAC=/path/to/file.flac to run the
# stack check under ctest (no audio files are kept in the repo).
if(TINYDAP_TEST_FLAC)
    add_test(NAME decoder_stack COMMAND stack_check ${TINYDAP_TEST_FLAC})
endif()
