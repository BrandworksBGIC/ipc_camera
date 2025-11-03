cmake_minimum_required(VERSION 2.8)
project(rainbow)

add_subdirectory(ringbuffer)
add_subdirectory(capture)
add_subdirectory(rtsp)

install(FILES S90rainbow DESTINATION /etc/init.d PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE
	GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
