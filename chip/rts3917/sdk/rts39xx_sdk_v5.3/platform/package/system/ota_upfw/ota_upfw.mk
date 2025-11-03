################################################################################
#
# OTA_UPFW
#
################################################################################

OTA_UPFW_SITE_METHOD = local
OTA_UPFW_INSTALL_STAGING = NO
OTA_UPFW_SITE = $(BR2_EXTERNAL)/source/system/rauc-scripts

RAUC_ENABLE = no
ifeq ($(BR2_PACKAGE_RAUC_UPFW), y)
	OTA_UPFW_DEPENDENCIES = rauc
	RAUC_ENABLE = yes
endif

SWUPDATE_ENABLE = no
ifeq ($(BR2_PACKAGE_SWU_UPFW), y)
	OTA_UPFW_DEPENDENCIES = swupdate
	SWUPDATE_ENABLE = yes
endif

OTA_UPFW_CONF_OPTS = \
	-DCMAKE_INSTALL_PREFIX="/"  \
	-DRAUC_ENABLE=$(RAUC_ENABLE)  \
	-DSWUPDATE_ENABLE=$(SWUPDATE_ENABLE)

$(eval $(cmake-package))
