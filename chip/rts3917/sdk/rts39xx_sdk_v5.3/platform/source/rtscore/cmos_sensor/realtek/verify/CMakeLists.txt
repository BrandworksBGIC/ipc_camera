if(NOT(${CONFIG_CMOS_SENSOR_TARGET_OS} STREQUAL Zephyr))
    add_sensor(verify_seq)
    add_sensor(verify_still)
endif()
