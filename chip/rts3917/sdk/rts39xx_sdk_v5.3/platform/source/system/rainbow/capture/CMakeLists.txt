cmake_minimum_required(VERSION 2.8)
project(capture)

if (WAVE521_SUPPORT MATCHES "YES")
    add_definitions(-DRTS_ENABLE_W521)
endif()

if (WAVE420_SUPPORT MATCHES "YES")
    add_definitions(-DRTS_ENABLE_W420)
endif()

if (H1_SUPPORT MATCHES "YES")
    add_definitions(-DRTS_ENABLE_H1)
endif()

if (VER_RTS3903 MATCHES "YES")
    add_definitions(-DRTS_VER_RTS3903)
endif()

if (VER_RTS3913 MATCHES "YES")
    add_definitions(-DRTS_VER_RTS3913)
endif()

if (VER_RTS3915 MATCHES "YES")
    add_definitions(-DRTS_VER_RTS3915)
endif()

if (VER_RTS3917 MATCHES "YES")
    add_definitions(-DRTS_VER_RTS3917)
endif()

INCLUDE_DIRECTORIES(../ringbuffer)
add_library(rfmsg SHARED rf_msg.c)
target_link_libraries(rfmsg ubus ubox pthread rtstream rtsvideo)

install(FILES rf_msg.h DESTINATION include/)
install(FILES rf_error.h DESTINATION include/)
install(TARGETS rfmsg DESTINATION lib/)

add_executable(capture capture.c video.c audio.c ispctrl.c mask.c md.c osd.c rf_msg_svr.c mjpeg.c h26x_parse.c ../ringbuffer/ringbuffer.c)
target_link_libraries(capture rfmsg rtstream rtsvideo rtsaudio asound json-c)

add_executable(test_rfmsg test_rfmsg.c)
target_link_libraries(test_rfmsg rfmsg)

#add_executable(test_rainbow test_rainbow.c)
#target_link_libraries(test_rainbow rfmsg cunit)

install(TARGETS capture RUNTIME DESTINATION bin)
install(TARGETS test_rfmsg RUNTIME DESTINATION bin)
#install(TARGETS test_rainbow RUNTIME DESTINATION bin)
install(FILES rainbow.json DESTINATION /etc/conf)
