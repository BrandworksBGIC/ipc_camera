################################################################################
#
# kmod_rtl8723cu
#
################################################################################

KMOD_RTL8723CU_BT_SITE = $(BR2_EXTERNAL)/source/drivers/bluetooth/rtl8723cu_bt
KMOD_RTL8723CU_BT_SITE_METHOD = local
KMOD_RTL8723CU_BT_LICENSE = GPL-2.0, proprietary (rtl8723cu_fw rtl8723cu_config firmware blob)
#KMOD_RTL8723CU_BT_LICENSE_FILES = COPYING
KMOD_RTL8723CU_BT_MODULE_MAKE_OPTS = CONFIG_RTL8723CU_BT=m

KMOD_RTL8723CU_BT_SCRIPT_NAME = 03_init_rtl8723cu_bt

define KMOD_RTL8723CU_BT_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8723CU_BT_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8723CU_BT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723CU_BT_SCRIPT_NAME)
        echo "modprobe 8723cut.ko" >> $(@D)/$(KMOD_RTL8723CU_BT_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8723CU_BT_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8723CU_BT_SCRIPT_NAME)

	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723cu_fw $(TARGET_DIR)/lib/firmware/rtl8723cu_fw
	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723cu_config $(TARGET_DIR)/lib/firmware/rtl8723cu_config
endef

KMOD_RTL8723CU_BT_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8723CU_BT_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
