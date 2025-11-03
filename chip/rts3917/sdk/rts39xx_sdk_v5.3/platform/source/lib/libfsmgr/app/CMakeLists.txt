cmake_minimum_required(VERSION 2.8)

add_executable(fs_mgr fs_mgr_main.c)
target_link_libraries(fs_mgr fsmgr)
install(TARGETS fs_mgr RUNTIME DESTINATION bin)
install(FILES S02fsmgr DESTINATION /etc/init.d
	PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE
	GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
