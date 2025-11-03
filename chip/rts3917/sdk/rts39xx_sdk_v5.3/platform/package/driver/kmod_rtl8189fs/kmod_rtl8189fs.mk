################################################################################
#
# kmod_rtl8189fs
#
#################################################################################

KMOD_RTL8189FS_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/rtl8189fs
KMOD_RTL8189FS_SITE_METHOD = local
KMOD_RTL8189FS_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8189FS_LICENSE_FILES = COPYING
KMOD_RTL8189FS_MODULE_MAKE_OPTS = CONFIG_RTL8189FS=m

ifeq ($(BR2_CONFIG_RTW_CONCURRENT_MODE), y)
KMOD_RTL8189FS_MODULE_MAKE_OPTS += CONFIG_RTW_CONCURRENT_MODE=y
endif

KMOD_RTL8189FS_SCRIPT_NAME = 03_init_rtl8189fs

define KMOD_RTL8189FS_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8189FS_SCRIPT_NAME)
		echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8189FS_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8189FS_SCRIPT_NAME)
		echo "modprobe 8189fs.ko" >> $(@D)/$(KMOD_RTL8189FS_SCRIPT_NAME)
	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8189FS_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8189FS_SCRIPT_NAME)

endef

KMOD_RTL8189FS_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8189FS_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))



