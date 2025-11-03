INCLUDE_DIRECTORIES(../include)
add_executable(example_aac_encode example_aac_encode.c)
target_link_libraries(example_aac_encode vo-aacenc)

install(TARGETS example_aac_encode RUNTIME DESTINATION usr/bin)
