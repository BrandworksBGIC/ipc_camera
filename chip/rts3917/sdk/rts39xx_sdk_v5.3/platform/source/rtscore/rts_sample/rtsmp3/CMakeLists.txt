INCLUDE_DIRECTORIES(../include)
add_executable(example_mp3_decode example_mp3_decode.c)

install(TARGETS example_mp3_decode RUNTIME DESTINATION usr/bin)
