################################################################################
#
# pfigure
#
################################################################################

PFIGURE_SITE_METHOD = local
PFIGURE_SITE = $(BR2_EXTERNAL)/source/usb/pfigure
PFIGURE_INSTALL_STAGING = YES

PFIGURE_DEPENDENCIES += rtstream libjpeg realnet

ifeq ($(BR2_PACKAGE_PFIGURE_PLUGIN_OD), y)
	PFIGURE_DEPENDENCIES += realnet
endif

PFIGURE_CONF_OPTS += \
	-DCMAKE_INSTALL_PREFIX="/" \
	-DPLUGIN_OD=$(if $(BR2_PACKAGE_PFIGURE_PLUGIN_OD),ON,OFF) \

$(eval $(cmake-package))
