cmake_minimum_required(VERSION 2.8)
PROJECT(libfsmgr)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wall -Werror -ftrapv")
add_definitions(-D_GNU_SOURCE)
INCLUDE_DIRECTORIES(. include)

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()

ADD_DEFINITIONS(-Wall -Werror --std=gnu99 -Wmissing-declarations)

FILE(GLOB SRC_LIST "*.c" "./ext4_utils/*.c" "./squashfs_utils/*.c" "./cutils/*.c")

ADD_LIBRARY(fsmgr SHARED ${SRC_LIST})
target_link_libraries(fsmgr kcapi)

install(FILES include/fs_mgr.h DESTINATION include)
install(TARGETS fsmgr LIBRARY DESTINATION lib)

add_subdirectory(app)
