################################################################################
#
# ubus
#
################################################################################


define UBUS_INSTALL_MISC
        $(INSTALL) -D -m 755 \
		$(BR2_EXTERNAL_platform_PATH)/package/buildroot/ubus/S30ubusd	\
		$(TARGET_DIR)/etc/init.d/S30ubusd
endef


UBUS_POST_INSTALL_TARGET_HOOKS += UBUS_INSTALL_MISC
