set(sensors_list "${CONFIG_RTS_ISP_ISPD_SENSORS_LIST}")
string(REPLACE "," ";" sensors_list "${sensors_list}")

file(GLOB libs libsensor/libsensor_*.a)

if(libs)
	foreach(lib_file ${libs})
		get_filename_component(lib ${lib_file} NAME)
		string(REPLACE "libsensor_" "" name ${lib})
		string(REPLACE ".a" "" name ${name})
		list(FIND sensors_list ${name} index)

		if(index GREATER -1)
			message("import library ${lib}")
			zephyr_library_import(${lib} ${lib_file})
		endif()
	endforeach()

	file(GLOB libs libiq/libiq_*.a)

	foreach(lib_file ${libs})
		message("import library ${lib}")
		get_filename_component(lib ${lib_file} NAME)
		zephyr_library_import(${lib} ${lib_file})
	endforeach()
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -ffreestanding -nostdlib -z max-page-size=0x1000")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

	set(sensorsdir ${CMAKE_CURRENT_BINARY_DIR})
	set(isptopdir ${CMAKE_CURRENT_SOURCE_DIR}/../isp)
	set(PROTO_DIR ${isptopdir}/server/proto)

	find_program(PYTHON3 python3)

	if(NOT PYTHON3)
		message(FATAL_ERROR "python3 not found!")
	endif()

	if(DEFINED PROTO_DIR)
		execute_process(COMMAND ${PYTHON3} -m grpc_tools.protoc -I${PROTO_DIR}
			--python_out=${sensorsdir}
			${PROTO_DIR}/isp_iq_table.proto
			RESULT_VARIABLE ret_val)

		if(NOT ret_val EQUAL 0)
			message(FATAL_ERROR "==== generate isp_iq_table_pb2.py fail ====")
		endif()

		file(COPY pack_iq.py DESTINATION ${sensorsdir} USE_SOURCE_PERMISSIONS)
		file(COPY get_iq_table.S.in DESTINATION ${sensorsdir} USE_SOURCE_PERMISSIONS)
	endif()

	macro(add_all_subdirectories)
		set(cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
		file(GLOB children RELATIVE ${cur_dir} ${cur_dir}/*)

		foreach(child ${children})
			if(IS_DIRECTORY ${cur_dir}/${child} AND
				EXISTS ${cur_dir}/${child}/CMakeLists.txt)
				add_subdirectory(${child})
			endif()
		endforeach()
	endmacro()

	macro(add_sensor name)
		file(GLOB source ${name}.c ${name}_patch.c)
		set(target sensor_${name})
		add_library(${target} STATIC ${source})
		target_include_directories(${target} PRIVATE ${isptopdir}/common/api)
		target_include_directories(${target} PRIVATE ${PROTO_DIR})
		target_include_directories(${target} PRIVATE ${PROTO_DIR}/nanopb/)
		list(FIND sensors_list ${name} index)

		if(index GREATER -1)
			target_link_libraries(app PRIVATE ${target})
		endif()
	endmacro()

	macro(add_iq name)
		string(TOLOWER "${CHIP_ID}" chip_id)
		set(iq_proto ${sensorsdir}/isp_iq_table_pb2.py)
		set(iq_json ${CMAKE_CURRENT_SOURCE_DIR}/${name}_${chip_id}.json)
		set(iq_binary ${CMAKE_CURRENT_BINARY_DIR}/${name}.bin)
		set(iq_binary_source ${CMAKE_CURRENT_SOURCE_DIR}/${name}.bin)

		if(EXISTS ${iq_binary_source})
			message(STATUS "==== iq table ${name}.bin exist ====")
			file(COPY ${iq_binary_source} DESTINATION ${sensorsdir} USE_SOURCE_PERMISSIONS)
		elseif(EXISTS ${iq_proto} AND EXISTS ${iq_json})
			execute_process(COMMAND ${PYTHON3} ${sensorsdir}/pack_iq.py
				${iq_binary}
				${iq_json} ${iq_json} ${iq_json} ${iq_json}
				WORKING_DIRECTORY ${sensorsdir}
				RESULT_VARIABLE ret_val)

			if(NOT ret_val EQUAL 0)
				message(FATAL_ERROR "==== convert iq table fail ====")
			endif()

			file(COPY ${iq_binary} DESTINATION ${sensorsdir} USE_SOURCE_PERMISSIONS)
		endif()
	endmacro()

	add_all_subdirectories()

	file(GLOB BIN_FILES "${sensorsdir}/*.bin")

	if(NOT BIN_FILES)
		message(FATAL_ERROR "No .bin files found in directory: ${sensorsdir}")
	endif()

	foreach(BIN_FILE IN LISTS BIN_FILES)
		message(STATUS "Found iq binary file: ${BIN_FILE}")

		get_filename_component(SENSOR_NAME ${BIN_FILE} NAME_WE)
		set(BINARY_FILE_PATH ${sensorsdir}/${SENSOR_NAME}.bin)
		set(SYMBOL_NAME ${SENSOR_NAME})

		configure_file(${sensorsdir}/get_iq_table.S.in
			${sensorsdir}/${SENSOR_NAME}.S @ONLY)

		set(target iq_${SENSOR_NAME})
		add_library(${target} STATIC ${sensorsdir}/${SENSOR_NAME}.S)
		target_link_libraries(app PRIVATE ${target})
	endforeach()
endif()
