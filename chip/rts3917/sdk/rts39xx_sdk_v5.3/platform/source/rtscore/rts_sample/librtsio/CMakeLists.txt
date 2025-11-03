add_executable(example_io example_io.c)
target_link_libraries(example_io rtsio rtsvideo rtstream)

install(TARGETS example_io RUNTIME DESTINATION usr/bin)
