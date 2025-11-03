cmake_minimum_required(VERSION 2.8)

project(neuralyzer)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()
include_directories(include)

add_library(neuralyzer SHARED libneuralyzer.c)
target_link_libraries(neuralyzer libmtd.a)

add_executable(neuralyzer-bin neuralyzer.c libneuralyzer.c)
target_link_libraries(neuralyzer-bin libmtd.a libc.a)
set_target_properties(neuralyzer-bin
	  PROPERTIES OUTPUT_NAME neuralyzer)

add_executable(neucleanup neucleanup.c)
add_dependencies(neucleanup neuralyzer)
target_link_libraries(neucleanup neuralyzer pthread)

install(TARGETS neuralyzer
	LIBRARY DESTINATION usr/lib)
install(TARGETS neuralyzer-bin
	RUNTIME DESTINATION usr/bin)
install(TARGETS neucleanup
	RUNTIME DESTINATION usr/bin)
install(FILES include/neuralyzer.h DESTINATION usr/include)
install(FILES include/mtd_swab.h DESTINATION usr/include)

install(FILES switchroot.sh DESTINATION usr/bin PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
