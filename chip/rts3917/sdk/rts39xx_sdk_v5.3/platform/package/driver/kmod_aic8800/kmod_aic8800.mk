################################################################################
#
# kmod_aic8800
#
################################################################################

KMOD_AIC8800_SITE = $(BR2_EXTERNAL)/source/drivers/wifi/aic8800
KMOD_AIC8800_SITE_METHOD = local
KMOD_AIC8800_LICENSE = GPL-2.0
KMOD_AIC8800_MODULE_MAKE_OPTS = CONFIG_AIC8800=m

KMOD_AIC8800_SCRIPT_NAME = 03_init_aic8800

ifeq ($(BR2_PACKAGE_AIC8800D80_FDRV), y)
KMOD_AIC8800_MODULE_MAKE_OPTS += CONFIG_AIC8800D80_FDRV=y
endif

define KMOD_AIC8800_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_AIC8800_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_AIC8800_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_AIC8800_SCRIPT_NAME)

    echo "modprobe aic_load_fw.ko" >> $(@D)/$(KMOD_AIC8800_SCRIPT_NAME)
	$(INSTALL) -D -m 755  $(KMOD_AIC8800_SITE)/firmware/aic_userconfig_8800d80.txt $(TARGET_DIR)/lib/firmware/aic_userconfig_8800d80.txt
	$(INSTALL) -D -m 755  $(KMOD_AIC8800_SITE)/firmware/* $(TARGET_DIR)/lib/firmware/

    echo "modprobe aic8800_fdrv.ko" >> $(@D)/$(KMOD_AIC8800_SCRIPT_NAME)
	$(INSTALL) -D -m 755 $(@D)/$(KMOD_AIC8800_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_AIC8800_SCRIPT_NAME)
endef

KMOD_AIC8800_POST_INSTALL_TARGET_HOOKS += KMOD_AIC8800_INSTALL_MISC


$(eval $(kernel-module))
$(eval $(generic-package))
