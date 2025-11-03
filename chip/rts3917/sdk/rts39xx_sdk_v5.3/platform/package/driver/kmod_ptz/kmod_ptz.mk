################################################################################
#
# kmod_ptz
#
################################################################################

KMOD_PTZ_SITE = $(BR2_EXTERNAL)/source/drivers/ptz
KMOD_PTZ_SITE_METHOD = local
KMOD_PTZ_LICENSE = GPL-2.0
KMOD_PTZ_MODULE_MAKE_OPTS = CONFIG_PTZ=m

KMOD_PTZ_SCRIPT_NAME = 08_init_ptz

ifeq ($(BR2_PACKAGE_KMOD_PTZ_GPIO), y)
#PTZ_TYPE=GPIO
KMOD_PTZ_SITE = $(BR2_EXTERNAL)/source/drivers/ptz/ptz_gpio
endif
ifeq ($(BR2_PACKAGE_KMOD_PTZ_595), y)
#PTZ_TYPE=595
KMOD_PTZ_SITE = $(BR2_EXTERNAL)/source/drivers/ptz/ptz_595
endif
ifeq ($(BR2_PACKAGE_KMOD_PTZ_TUYA), y)
#PTZ_TYPE=tuya
KMOD_PTZ_SITE = $(BR2_EXTERNAL)/source/drivers/ptz/ptz_tuya
endif

define KMOD_PTZ_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_PTZ_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_PTZ_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_PTZ_SCRIPT_NAME)
        echo "modprobe rts_ptz.ko" >> $(@D)/$(KMOD_PTZ_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_PTZ_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_PTZ_SCRIPT_NAME)
	$(INSTALL) -D -m 644 $(@D)/ptz_usr.h $(TARGET_DIR)/../host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/include/

endef

KMOD_PTZ_POST_INSTALL_TARGET_HOOKS += KMOD_PTZ_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
