CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(libsysconf)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

if (DIR_TMPFS)
	LINK_DIRECTORIES(${DIR_TMPFS}/lib)
endif()
INCLUDE_DIRECTORIES(.)

if (SUPPORT_UNIT_TEST MATCHES y)
	add_subdirectory(libsysconf_unit_test)
endif ()
add_library(sysconf_dynamic SHARED sysconf.c utils.c)
set_target_properties(sysconf_dynamic PROPERTIES OUTPUT_NAME "sysconf")
set_target_properties(sysconf_dynamic PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(sysconf_dynamic json-c m)

add_library(sysconf_static STATIC sysconf.c utils.c)
set_target_properties(sysconf_static PROPERTIES OUTPUT_NAME "sysconf")
set_target_properties(sysconf_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(sysconf_static json-c m)

add_executable(entropy entropy.c utils.c)

add_executable(check_cfg check_cfg.c)
target_link_libraries(check_cfg sysconf_dynamic)

install(FILES sysconf.h DESTINATION usr/include)
install(TARGETS entropy sysconf_dynamic sysconf_static
	RUNTIME DESTINATION usr/bin
	LIBRARY DESTINATION usr/lib
	ARCHIVE DESTINATION usr/lib)
install(TARGETS check_cfg RUNTIME DESTINATION usr/bin)
install(FILES 08_cpconf.sh DESTINATION etc/preinit
	PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE
	GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES S45entropy DESTINATION etc/init.d
	PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE
	GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
