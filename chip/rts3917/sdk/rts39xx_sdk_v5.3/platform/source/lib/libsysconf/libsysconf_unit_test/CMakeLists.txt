CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(sysconf_unit_test)


INCLUDE_DIRECTORIES(..)

SET(DEPENDENT_LIBRARIES cunit json-c)

#add_executable(libsysconf_cunit ../utils.c suite.c)
#target_link_libraries(libsysconf_cunit ${DEPENDENT_LIBRARIES})

#install(TARGETS libsysconf_cunit RUNTIME DESTINATION usr/bin)
