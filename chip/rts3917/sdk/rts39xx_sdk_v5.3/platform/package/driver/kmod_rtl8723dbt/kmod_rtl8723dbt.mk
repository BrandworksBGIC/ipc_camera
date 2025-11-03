################################################################################
#
# kmod_rtl8723dbt
#
################################################################################

KMOD_RTL8723DBT_SITE = $(BR2_EXTERNAL)/source/drivers/bluetooth/rtl8723dbt
KMOD_RTL8723DBT_SITE_METHOD = local
KMOD_RTL8723DBT_LICENSE = GPL-2.0, proprietary (rtl8723du_fw rtl8723du_config firmware blob)
#KMOD_RTL8723DBT_LICENSE_FILES = COPYING
KMOD_RTL8723DBT_MODULE_MAKE_OPTS = CONFIG_RTL8723DBT=m

KMOD_RTL8723DBT_SCRIPT_NAME = 30_init_rtl8723dbt

define KMOD_RTL8723DBT_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_RTL8723DBT_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_RTL8723DBT_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_RTL8723DBT_SCRIPT_NAME)
        echo "modprobe 8723dbt.ko" >> $(@D)/$(KMOD_RTL8723DBT_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_RTL8723DBT_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_RTL8723DBT_SCRIPT_NAME)

	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723du_fw $(TARGET_DIR)/lib/firmware/rtl8723du_fw
	$(INSTALL) -D -m 644 $(@D)/btd_fw/rtl8723du_config $(TARGET_DIR)/lib/firmware/rtl8723du_config
endef

KMOD_RTL8723DBT_POST_INSTALL_TARGET_HOOKS += KMOD_RTL8723DBT_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
