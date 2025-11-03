################################################################################
#
# cmos_sensor
#
################################################################################

CMOS_SENSOR_LIST = $(strip $(BR2_PACKAGE_CMOS_SENSOR_LIST))

CMOS_SENSOR_SITE_METHOD = local
CMOS_SENSOR_SITE = $(BR2_EXTERNAL)/source/rtscore/cmos_sensor
CMOS_SENSOR_INSTALL_STAGING = YES
CMOS_SENSOR_DEPENDENCIES = rtstream
CMOS_SENSOR_CONF_OPTS += -DSENSORS_LIST=$(CMOS_SENSOR_LIST) \
			-DCHIP_ID=${LIBRTSISP_CHIP_ID}
CMOS_SENSOR_CONF_ENV = PYTHONNOUSERSITE=1

ifeq ($(CMOS_SENSOR_LIST), "")
CMOS_SENSOR_INSTALL_STAGING = NO
define CMOS_SENSOR_INSTALL_TARGET_CMDS
	$(NOOP)
endef
endif

$(eval $(cmake-package))
