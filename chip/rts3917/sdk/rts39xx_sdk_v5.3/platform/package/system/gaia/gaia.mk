GAIA_SITE_METHOD = local
GAIA_SITE = $(BR2_EXTERNAL)/source/system/gaia
GAIA_INSTALL_STAGING = NO

ifeq ($(BR2_CONFIG_HW_VER_RTS3915), y)
ifeq ($(BR2_PACKAGE_JSON_C), y)
GAIA_DEPENDENCIES += $(if $(BR2_PACKAGE_JSON_C), json-c)
endif

ifeq ($(BR2_PACKAGE_RTSTREAM), y)
GAIA_DEPENDENCIES += $(if $(BR2_PACKAGE_RTSTREAM), rtstream)
endif
endif

ifeq ($(BR2_CONFIG_HW_VER_RTS3917), y)
ifeq ($(BR2_PACKAGE_JSON_C), y)
GAIA_DEPENDENCIES += $(if $(BR2_PACKAGE_JSON_C), json-c)
endif

ifeq ($(BR2_PACKAGE_RTSTREAM), y)
GAIA_DEPENDENCIES += $(if $(BR2_PACKAGE_RTSTREAM), rtstream)
endif
endif

TARGET_PLATFORM = RTS
ifeq ($(TARGET_PLATFORM), RTL)
        CHIP_TYPE ?= RTL
else
        ifeq ($(BR2_CONFIG_HW_VER_RTS3903), y)
                CHIP_TYPE ?= RTS3903
        endif
        ifeq ($(BR2_CONFIG_HW_VER_RTS3913), y)
                CHIP_TYPE ?= RTS3913
        endif
        ifeq ($(BR2_CONFIG_HW_VER_RTS3915), y)
                CHIP_TYPE ?= RTS3915
        endif
        ifeq ($(BR2_CONFIG_HW_VER_RTS3917), y)
                CHIP_TYPE ?= RTS3917
        endif
endif

GAIA_CONF_OPTS = \
	-DRTS_CHIP_TYPE=$(CHIP_TYPE) \
	-DCMAKE_INSTALL_PREFIX="/"  \
	-DGAIA_FAST=$(if $(BR2_PACKAGE_GAIA_FAST),yes,no)

GAIA_CONF_OPTS += \
	-DSENSORS_SUPPORT=$(BR2_PACKAGE_GAIA_FAST_SENSOR)

$(eval $(cmake-package))
