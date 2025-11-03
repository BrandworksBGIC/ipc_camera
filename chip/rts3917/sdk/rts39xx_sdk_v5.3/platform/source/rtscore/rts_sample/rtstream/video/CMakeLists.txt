
set(depend_share_lib rtsvideo rtstream rtsio pthread m)
set(depend_static_lib librtstream.a rtsio soxr asound)

set(ENABLE_H264 1)
set(ENABLE_H265 1)
set(ENABLE_H26x 1)
set(ENABLE_OSD2_PPU 1)

if (NOT (CONFIG_RTSTREAM_OSD2_PPU MATCHES "y"))
	set(ENABLE_OSD2_PPU 0)
endif()

if (NOT (CONFIG_PACKAGE_h1_encoder MATCHES "y"))
	set(ENABLE_H264 0)
endif()

if (NOT (CONFIG_PACKAGE_H265 MATCHES "y"))
	set(ENABLE_H265 0)
endif()

if (NOT (CONFIG_PACKAGE_H26x MATCHES "y"))
	set(ENABLE_H26x 0)
endif()

if (NOT (CONFIG_PACKAGE_MCU MATCHES "y"))
	FILE(REMOVE example_md.c)
	FILE(REMOVE example_set_sensor_fps.c)
endif()

if (NOT (CONFIG_RTSTREAM_MTD2 MATCHES "y"))
	FILE(REMOVE example_md2.c)
endif()

if (NOT (CONFIG_RTSTREAM_VOUT MATCHES "y"))
	FILE(REMOVE example_video_out.c)
endif()

if (NOT (CONFIG_PACKAGE_ISP_ALGO MATCHES "y"))
	FILE(REMOVE example_isp.c)
endif()

if (ENABLE_H26x)
	FILE(REMOVE h264/example_roi.c)
endif()

FILE(GLOB SRC_LIST "*.c")
set(src ${SRC_LIST})



if (ENABLE_OSD2_PPU)
	set(depend_static_lib ${depend_static_lib} OpenVX)
endif()

if (ENABLE_H264)
	FILE(GLOB_RECURSE SRC_LIST "h264/*.c")
	set(src ${src} ${SRC_LIST})
endif()

if (ENABLE_H265)
	FILE(GLOB_RECURSE SRC_LIST "h265/*.c")
	set(src ${src} ${SRC_LIST})
endif()

if (ENABLE_H26x)
	FILE(GLOB_RECURSE SRC_LIST "h265/*.c" "h264/*.c")
	set(src ${src} ${SRC_LIST})
endif()

FOREACH(app ${src})
	get_filename_component(target ${app} NAME_WE)
	add_executable(${target} ${app})
	if (CONFIG_SHARE_SAMPLE MATCHES "y")
		target_link_libraries(${target} ${depend_share_lib})
	endif()
	if (NOT (CONFIG_SHARE_SAMPLE MATCHES "y"))
		target_link_libraries(${target} ${depend_static_lib})
	endif()
	install(TARGETS ${target} RUNTIME DESTINATION usr/bin)
ENDFOREACH(app)

install(FILES logo_pic.bin DESTINATION etc)
