################################################################################
#
# dbus
#
################################################################################


define DBUS_INSTALL_MISC
		rm -rf $(TARGET_DIR)/usr/bin/dbus-cleanup-sockets
		rm -rf $(TARGET_DIR)/usr/bin/dbus-launch
		rm -rf $(TARGET_DIR)/usr/bin/dbus-run-session
		rm -rf $(TARGET_DIR)/usr/bin/dbus-test-tool
		rm -rf $(TARGET_DIR)/usr/bin/dbus-update-activation-environment
		rm -rf $(TARGET_DIR)/usr/share/doc
		rm -rf $(TARGET_DIR)/usr/share/xml
		rm -rf $(TARGET_DIR)/usr/lib/pkgconfig
		rm -rf $(TARGET_DIR)/usr/lib/cmake
		rm -rf $(TARGET_DIR)/usr/lib/libdbus-1.la
		rm -rf $(TARGET_DIR)/etc/init.d/S30dbus

		$(INSTALL) -D -m 755 \
			$(BR2_EXTERNAL_platform_PATH)/package/buildroot/dbus/S30dbus \
			$(TARGET_DIR)/etc/init.d/S30dbus

endef

DBUS_POST_INSTALL_TARGET_HOOKS += DBUS_INSTALL_MISC
