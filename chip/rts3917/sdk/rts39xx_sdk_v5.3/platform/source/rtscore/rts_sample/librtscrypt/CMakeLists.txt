add_executable(crypt_example crypt_example.c)
target_link_libraries(crypt_example rtscrypt)

install(TARGETS crypt_example RUNTIME DESTINATION usr/bin)
