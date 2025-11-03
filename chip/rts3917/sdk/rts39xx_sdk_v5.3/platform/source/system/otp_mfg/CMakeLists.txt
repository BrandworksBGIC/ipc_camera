CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(otp_mfg)

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()

INCLUDE_DIRECTORIES(../include)

set(target otp_mfg)
set(src otp_mfg.c)

add_executable(${target} ${src})

install(TARGETS ${target}
	LIBRARY DESTINATION usr/lib
	ARCHIVE DESTINATION usr/lib
	RUNTIME DESTINATION usr/bin)
