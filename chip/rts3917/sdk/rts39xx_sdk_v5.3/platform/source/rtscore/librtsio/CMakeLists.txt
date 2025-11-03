CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(io)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wall -Werror")

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()

include_directories(include)

set(STATIC_SWITCH 1)

set(SRC_LIST rts_io_i2c.c rts_io_gpio.c rts_io_adc.c)
add_library(rtsio SHARED ${SRC_LIST})
set_target_properties(rtsio PROPERTIES SOVERSION 0 VERSION 1.2.1)

if (STATIC_SWITCH)
	add_library(rtsio_static STATIC ${SRC_LIST})
	set_target_properties(rtsio_static PROPERTIES OUTPUT_NAME rtsio)

	install(TARGETS rtsio_static ARCHIVE DESTINATION lib)
endif()

install(DIRECTORY include/ DESTINATION include)
install(TARGETS rtsio LIBRARY DESTINATION lib)
