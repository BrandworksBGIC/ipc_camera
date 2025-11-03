CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(librtscrypt)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wall -Werror")
add_definitions(-D_GNU_SOURCE)

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()

include_directories(.)


set(SOVERSION 1)
if (!RTS_VERSION)
	set(RTS_VERSION 1)
endif()
if (!RTS_NUMBER)
	set(RTS_NUMBER 1)
endif()
set(VERSION ${SOVERSION}.${RTS_VERSION}.${RTS_NUMBER})

set(STATIC_SWITCH 1)

set(SRC_LIST rts_crypto.c)
add_library(rtscrypt SHARED ${SRC_LIST})
set_target_properties(rtscrypt PROPERTIES OUTPUT_NAME "rtscrypt")
set_target_properties(rtscrypt PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(rtscrypt PROPERTIES SOVERSION ${SOVERSION} VERSION ${VERSION})

if (STATIC_SWITCH)
	add_library(rtscrypt_static STATIC ${SRC_LIST})
	set_target_properties(rtscrypt_static PROPERTIES OUTPUT_NAME "rtscrypt")
	set_target_properties(rtscrypt_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
	install(TARGETS rtscrypt_static ARCHIVE DESTINATION lib)
endif()

add_subdirectory(test)

install(FILES rts_crypto.h DESTINATION include)
install(TARGETS rtscrypt LIBRARY DESTINATION lib)
