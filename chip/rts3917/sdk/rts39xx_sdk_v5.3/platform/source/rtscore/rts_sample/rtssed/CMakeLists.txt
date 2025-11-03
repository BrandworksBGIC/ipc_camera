set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -O3")

add_executable(example_capture_sed example_capture_sed.c)

target_link_libraries(example_capture_sed rtsnn rtsaudio rtstream asound)
install(TARGETS example_capture_sed RUNTIME DESTINATION usr/bin)
