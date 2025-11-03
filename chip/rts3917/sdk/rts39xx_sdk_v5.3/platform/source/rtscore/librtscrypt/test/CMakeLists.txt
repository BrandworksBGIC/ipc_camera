include_directories(.)

set(target test_crypt)
set(src rts_test_crypt.c)

add_executable(${target} ${src})

target_link_libraries(${target} rtscrypt)
