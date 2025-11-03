################################################################################
#
# kmod_rtl8822cs_bt
#
################################################################################

KMOD_RTL8822CS_BT_SITE = $(BR2_EXTERNAL)/source/drivers/bluetooth/rtl8822cs_bt
KMOD_RTL8822CS_BT_SITE_METHOD = local
KMOD_RTL8822CS_BT_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8822CS_BT_LICENSE_FILES = COPYING
KMOD_RTL8822CS_BT_MODULE_MAKE_OPTS = CONFIG_RTL8822CS_BT=m

KMOD_RTL8822CS_BT_SCRIPT_NAME = 03_init_rtl8822cs_bt

SERIAL_NAME = ttyS0

define KMOD_RTL8822CS_BT_BUILD_TOOLS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/rtk_hciattach rtk_hciattach
endef

KMOD_RTL8822CS_BT_POST_BUILD_HOOKS = KMOD_RTL8822CS_BT_BUILD_TOOLS

define KMOD_RTL8822CS_BT_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	echo "modprobe 8822cst.ko" >> $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	echo "rtk_hciattach -s 115200 $(SERIAL_NAME) rtk_h5" >> $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)

	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8822cs_fw $(TARGET_DIR)/lib/firmware/rtl8822cs_fw
	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8822cs_config $(TARGET_DIR)/lib/firmware/rtl8822cs_config
	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8822CS_BT_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8822CS_BT_SCRIPT_NAME)
	$(INSTALL) -D -m 755 $(@D)/rtk_hciattach/rtk_hciattach $(TARGET_DIR)/usr/sbin/rtk_hciattach

endef

KMOD_RTL8822CS_BT_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8822CS_BT_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
