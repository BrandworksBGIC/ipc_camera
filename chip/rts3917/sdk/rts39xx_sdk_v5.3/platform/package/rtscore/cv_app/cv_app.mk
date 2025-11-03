################################################################################
#
# cv_app
#
################################################################################

CV_APP_SITE_METHOD = local
CV_APP_INSTALL_STAGING = YES

DEPEND_RTSTREAM = $(filter y, $(BR2_PACKAGE_RTSMD) $(BR2_PACKAGE_RTSAVD))
CV_APP_DEPENDENCIES += $(if $(DEPEND_RTSTREAM), rtstream)

CV_APP_SITE = $(BR2_EXTERNAL)/source/rtscore/cv_app
CV_APP_CMS_VERSION = $(shell cd $(CV_APP_SITE); $(BR2_EXTERNAL_platform_PATH)/scripts/getver.sh version)
CV_APP_CMS_NUMBER = $(shell cd $(CV_APP_SITE); $(BR2_EXTERNAL_platform_PATH)/scripts/getver.sh number)

CV_APP_CONF_OPTS += \
	-DRTS_VERSION=$(CV_APP_CMS_VERSION) \
	-DRTS_NUMBER=$(CV_APP_CMS_NUMBER) \
	-DDIR_TMPFS=$(STAGING_DIR)/usr \
	-DSDK_VERSION=3 \
	-DCONFIG_CIPHER_ENABLE=$(if $(BR2_CV_APP_CIPHER_EN),y,n)

CV_APP_CONF_ENV = \
	CONFIG_PANORAMA=$(if $(BR2_PACKAGE_CV_PANORAMA),y,n) \
	CONFIG_MOTION_TRACKING=$(if $(BR2_PACKAGE_CV_MOTION_TRACKING),y,n) \
	CONFIG_RTSMD=$(if $(BR2_PACKAGE_CV_RTSMD),y,n) \
	CONFIG_RTSAVD=$(if $(BR2_PACKAGE_CV_RTSAVD),y,n)


$(eval $(cmake-package))

