set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -shared-libgcc -std=gnu99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

set(sensors_list "${SENSORS_LIST}")
string(REPLACE "," ";" sensors_list "${sensors_list}")

if(DEFINED ENV{STAGING_DIR})
    set(PROTO_DIR $ENV{STAGING_DIR}/usr/include/isp-proto)
    execute_process(COMMAND protoc -I${PROTO_DIR} --python_out=.
        ${PROTO_DIR}/isp_iq_table.proto
        RESULT_VARIABLE ret_val)

    if(NOT ret_val EQUAL 0)
        message(FATAL_ERROR "==== generate isp_iq_table_pb2.py fail ====")
    endif()
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
    file(GLOB src ${name}.c ${name}_patch.c)
    set(target sensor_${name})
    add_library(${target} SHARED ${src})
    list(FIND sensors_list ${name} index)

    if(index GREATER -1)
        install(TARGETS ${target} LIBRARY DESTINATION /lib/rtsisp/sensors)
    endif()
endmacro()

macro(add_iq name)
    string(TOLOWER ${CHIP_ID} chip_id)
    set(iq_proto ${CMAKE_SOURCE_DIR}/isp_iq_table_pb2.py)
    set(iq_json ${CMAKE_CURRENT_SOURCE_DIR}/${name}_${chip_id}.json)
    set(iq_binary ${CMAKE_CURRENT_BINARY_DIR}/${name}.bin)

    if(EXISTS ${iq_proto} AND EXISTS ${iq_json})
        execute_process(COMMAND ${CMAKE_SOURCE_DIR}/pack_iq.py ${iq_binary}
            ${iq_json} ${iq_json} ${iq_json} ${iq_json}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE ret_val)

        if(NOT ret_val EQUAL 0)
            message(FATAL_ERROR "==== convert iq table fail ====")
        endif()
    endif()

    list(FIND sensors_list ${name} index)

    if(index GREATER -1)
        install(FILES ${name}.bin DESTINATION /lib/rtsisp/iqs
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
endmacro()

add_all_subdirectories()
