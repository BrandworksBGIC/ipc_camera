################################################################################
#
# kmod_RTL8723DS_BT
#
################################################################################

KMOD_RTL8723DS_BT_SITE = $(BR2_EXTERNAL)/source/drivers/bluetooth/rtl8723ds_bt
KMOD_RTL8723DS_BT_SITE_METHOD = local
KMOD_RTL8723DS_BT_LICENSE = GPL-2.0, proprietary (  firmware blob)
#KMOD_RTL8723DS_BT_LICENSE_FILES = COPYING
KMOD_RTL8723DS_BT_MODULE_MAKE_OPTS = CONFIG_RTL8723DS_BT=m

KMOD_RTL8723DS_BT_SCRIPT_NAME = 03_init_rtl8723ds_bt

SERIAL_NAME = ttyS0

ifeq ($(BR2_PACKAGE_ATTENDANCE),y)
SERIAL_NAME = ttyS0
endif

define KMOD_RTL8723DS_BT_BUILD_TOOLS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/rtk_hciattach rtk_hciattach
endef

KMOD_RTL8723DS_BT_POST_BUILD_HOOKS = KMOD_RTL8723DS_BT_BUILD_TOOLS

define KMOD_RTL8723DS_BT_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	echo "modprobe 8723dst.ko" >> $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	echo "rtk_hciattach -s 115200 $(SERIAL_NAME) rtk_h5" >> $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)

	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723d_fw $(TARGET_DIR)/lib/firmware/rtl8723d_fw
	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723d_config_115200 $(TARGET_DIR)/lib/firmware/rtl8723d_config
#	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723d_config_1500000 $(TARGET_DIR)/lib/firmware/rtl8723d_config
	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8723DS_BT_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8723DS_BT_SCRIPT_NAME)
	$(INSTALL) -D -m 755 $(@D)/rtk_hciattach/rtk_hciattach $(TARGET_DIR)/usr/sbin/rtk_hciattach

endef

KMOD_RTL8723DS_BT_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8723DS_BT_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
