################################################################################
#
# kmod_rtl8830au
#
################################################################################
KMOD_RTL8830AU_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/rtl8830au
KMOD_RTL8830AU_SITE_METHOD = local
KMOD_RTL8830AU_LICENSE = GPL-2.0, proprietary (firmware blob)
KMOD_RTL8830AU_LICENSE_FILES = COPYING
KMOD_RTL8830AU_MODULE_MAKE_OPTS = CONFIG_RTL8830AU=m

ifeq ($(BR2_CONFIG_RTW_CONCURRENT_MODE), y)
KMOD_RTL8830AU_MODULE_MAKE_OPTS += CONFIG_RTW_CONCURRENT_MODE=y
endif

ifeq ($(BR2_CONFIG_RTS_MESH), y)
KMOD_RTL8830AU_MODULE_MAKE_OPTS += CONFIG_RTS_MESH=y
endif

ifeq ($(BR2_CONFIG_RTW_MINIMAL_MEMORY_USAGE), y)
KMOD_RTL8830AU_MODULE_MAKE_OPTS += CONFIG_RTW_MINIMAL_MEMORY_USAGE=y
endif

ifeq ($(BR2_CONFIG_RTW_COMPILER_OPTIMIZATION), y)
KMOD_RTL8830AU_MODULE_MAKE_OPTS += CONFIG_RTW_COMPILER_OPTIMIZATION=y
endif

KMOD_RTL8830AU_SCRIPT_NAME = 03_init_rtl8830au

define KMOD_RTL8830AU_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8830AU_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8830AU_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8830AU_SCRIPT_NAME)
        echo "modprobe 8830au.ko" >> $(@D)/$(KMOD_RTL8830AU_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8830AU_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8830AU_SCRIPT_NAME)

endef

KMOD_RTL8830AU_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8830AU_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
