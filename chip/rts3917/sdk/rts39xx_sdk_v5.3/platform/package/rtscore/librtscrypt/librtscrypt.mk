################################################################################
#
# librtscrypt
#
################################################################################

LIBRTSCRYPT_SITE_METHOD = local
LIBRTSCRYPT_INSTALL_STAGING = YES
LIBRTSCRYPT_SITE = $(BR2_EXTERNAL)/source/rtscore/librtscrypt

LIBRTSCRYPT_CMS_VERSION = $(shell cd $(LIBRTSCRYPT_SITE); $(BR2_EXTERNAL_platform_PATH)/scripts/getver.sh version)
LIBRTSCRYPT_CMS_NUMBER = $(shell cd $(LIBRTSCRYPT_SITE); $(BR2_EXTERNAL_platform_PATH)/scripts/getver.sh number)

LIBRTSCRYPT_CONF_OPTS = \
	-DRTS_VERSION=$(LIBRTSCRYPT_CMS_VERSION) \
	-DRTS_NUMBER=$(LIBRTSCRYPT_CMS_NUMBER) \
	-DDIR_TMPFS=$(STAGING_DIR)/usr

$(eval $(cmake-package))
