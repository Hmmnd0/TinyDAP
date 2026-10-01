# Native (macOS/Linux) build of the portable player core, for unit tests and
# pipeline experiments without hardware. See ../README.md.
cmake_minimum_required(VERSION 3.16)
project(tinydap_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

add_subdirectory(../components/player player)
find_package(Threads REQUIRED)

enable_testing()

add_executable(test_pcm_ring test_pcm_ring.c)
target_link_libraries(test_pcm_ring tinydap_player Threads::Threads)
add_test(NAME pcm_ring COMMAND test_pcm_ring)

add_executable(tone_to_wav tone_to_wav.c wav_sink.c)
target_link_libraries(tone_to_wav tinydap_player Threads::Threads)
