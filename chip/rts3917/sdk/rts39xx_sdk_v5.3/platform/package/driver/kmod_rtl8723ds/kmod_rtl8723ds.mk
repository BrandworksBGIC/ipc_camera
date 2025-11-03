################################################################################
#
# kmod_rtl8723ds
#
################################################################################

KMOD_RTL8723DS_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/rtl8723ds
KMOD_RTL8723DS_SITE_METHOD = local
KMOD_RTL8723DS_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8723DS_LICENSE_FILES = COPYING
KMOD_RTL8723DS_MODULE_MAKE_OPTS = CONFIG_RTL8723DS=m

ifeq ($(BR2_CONFIG_RTW_CONCURRENT_MODE), y)
KMOD_RTL8723DS_MODULE_MAKE_OPTS += CONFIG_RTW_CONCURRENT_MODE=y
endif

ifeq ($(BR2_CONFIG_RTS_MESH), y)
KMOD_RTL8723DS_MODULE_MAKE_OPTS += CONFIG_RTS_MESH=y
endif

ifeq ($(BR2_CONFIG_RTW_MINIMAL_MEMORY_USAGE), y)
KMOD_RTL8723DS_MODULE_MAKE_OPTS += CONFIG_RTW_MINIMAL_MEMORY_USAGE=y
endif

ifeq ($(BR2_CONFIG_RTW_COMPILER_OPTIMIZATION), y)
KMOD_RTL8723DS_MODULE_MAKE_OPTS += CONFIG_RTW_COMPILER_OPTIMIZATION=y
endif

KMOD_RTL8723DS_SCRIPT_NAME = 03_init_rtl8723ds

define KMOD_RTL8723DS_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8723DS_SCRIPT_NAME)
        echo "#this is astomatically generated" >> $(@D)/$(KMOD_RTL8723DS_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723DS_SCRIPT_NAME)
        echo "modprobe 8723ds.ko" >> $(@D)/$(KMOD_RTL8723DS_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8723DS_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8723DS_SCRIPT_NAME)

endef

KMOD_RTL8723DS_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8723DS_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
