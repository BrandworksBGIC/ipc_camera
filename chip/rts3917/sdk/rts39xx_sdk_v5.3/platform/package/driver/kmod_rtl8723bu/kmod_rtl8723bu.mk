################################################################################
#
# kmod_rtl8723bu
#
################################################################################

KMOD_RTL8723BU_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/rtl8723bu
KMOD_RTL8723BU_SITE_METHOD = local
KMOD_RTL8723BU_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8723BU_LICENSE_FILES = COPYING
KMOD_RTL8723BU_MODULE_MAKE_OPTS = CONFIG_RTL8723BU=m

ifeq ($(BR2_CONFIG_RTW_CONCURRENT_MODE), y)
KMOD_RTL8723BU_MODULE_MAKE_OPTS += CONFIG_RTW_CONCURRENT_MODE=y
endif

KMOD_RTL8723BU_SCRIPT_NAME = 03_init_rtl8723bu

define KMOD_RTL8723BU_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8723BU_SCRIPT_NAME)
        echo "#this is astomatically generated" >> $(@D)/$(KMOD_RTL8723BU_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723BU_SCRIPT_NAME)
        echo "modprobe 8723bu.ko" >> $(@D)/$(KMOD_RTL8723BU_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8723BU_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8723BU_SCRIPT_NAME)

endef

KMOD_RTL8723BU_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8723BU_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
