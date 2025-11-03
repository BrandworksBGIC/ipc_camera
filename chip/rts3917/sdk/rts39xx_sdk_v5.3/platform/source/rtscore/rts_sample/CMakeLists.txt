CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(sample)

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--export-dynamic -Wl,--copy-dt-needed-entries -Wl,-rpath-link,${DIR_TMPFS}/lib")

if (CONFIG_SAMPLE_RTSSED MATCHES "y")
	add_subdirectory(rtssed)
endif()

if (CONFIG_SAMPLE_RTSAAC MATCHES "y")
	add_subdirectory(rtsaac)
endif()

if (CONFIG_SAMPLE_RTSMP3 MATCHES "y")
	add_subdirectory(rtsmp3)
endif()

if (CONFIG_SAMPLE_LIBRTSIO MATCHES "y")
	add_subdirectory(librtsio)
endif()

if (CONFIG_SAMPLE_LIBRTSCRYPT MATCHES "y")
	add_subdirectory(librtscrypt)
endif()

add_subdirectory(rtstream)
