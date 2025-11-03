PROJECT(RTSP CXX)

if (NO_OPENSSL MATCHES "YES")
    add_definitions(-DNO_OPENSSL)
endif()

SET(RTSP_INCLUDE ${DIR_TMPFS}/usr/include)
INCLUDE_DIRECTORIES(${RTSP_INCLUDE}/BasicUsageEnvironment
	${RTSP_INCLUDE}/liveMedia ${RTSP_INCLUDE}/UsageEnvironment
	${RTSP_INCLUDE}/groupsock ../ringbuffer)

ADD_EXECUTABLE(rtsrtspd RTSRtspd.cpp json_func.c)
TARGET_LINK_LIBRARIES(rtsrtspd liveMedia groupsock
		BasicUsageEnvironment UsageEnvironment ringbuffer json-c)

INSTALL(TARGETS rtsrtspd DESTINATION bin
	PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
	GROUP_READ GROUP_EXECUTE
	WORLD_READ WORLD_EXECUTE)
