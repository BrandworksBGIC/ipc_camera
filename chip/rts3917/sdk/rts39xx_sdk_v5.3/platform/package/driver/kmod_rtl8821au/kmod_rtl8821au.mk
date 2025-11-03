################################################################################
#
# kmod_rtl8821au
#
################################################################################

KMOD_RTL8821AU_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/rtl8821au
KMOD_RTL8821AU_SITE_METHOD = local
KMOD_RTL8821AU_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8821AU_LICENSE_FILES = COPYING
KMOD_RTL8821AU_MODULE_MAKE_OPTS = CONFIG_RTL8821AU=m

ifeq ($(BR2_CONFIG_RTW_CONCURRENT_MODE), y)
KMOD_RTL8821AU_MODULE_MAKE_OPTS += CONFIG_RTW_CONCURRENT_MODE=y
endif

KMOD_RTL8821AU_SCRIPT_NAME = 03_init_rtl8821au

define KMOD_RTL8821AU_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8821AU_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8821AU_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8821AU_SCRIPT_NAME)
        echo "modprobe 8821au.ko" >> $(@D)/$(KMOD_RTL8821AU_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8821AU_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8821AU_SCRIPT_NAME)

endef

KMOD_RTL8821AU_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8821AU_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
