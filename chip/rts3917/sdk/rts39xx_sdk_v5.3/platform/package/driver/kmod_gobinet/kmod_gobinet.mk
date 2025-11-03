################################################################################
#
# kmod_gobinet
#
################################################################################

KMOD_GOBINET_SITE = $(BR2_EXTERNAL)/source/drivers/iot/gobinet
KMOD_GOBINET_SITE_METHOD = local
KMOD_GOBINET_LICENSE = GPL-2.0, proprietary (  firmware blob)

KMOD_GOBINET_SCRIPT_NAME = 30_init_gobinet

define KMOD_GOBINET_INSTALL_MISC
	echo "#!/bin/sh" > $(@D)/$(KMOD_GOBINET_SCRIPT_NAME)
        echo "#this is automatically generated" >> $(@D)/$(KMOD_GOBINET_SCRIPT_NAME)
	echo -e "\n" >> $(@D)/$(KMOD_GOBINET_SCRIPT_NAME)
        echo "modprobe GobiNet.ko" >> $(@D)/$(KMOD_GOBINET_SCRIPT_NAME)

	$(INSTALL) -D -m 755 $(@D)/$(KMOD_GOBINET_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_GOBINET_SCRIPT_NAME)
endef

KMOD_GOBINET_POST_INSTALL_TARGET_HOOKS += KMOD_GOBINET_INSTALL_MISC

$(eval $(kernel-module))
$(eval $(generic-package))
