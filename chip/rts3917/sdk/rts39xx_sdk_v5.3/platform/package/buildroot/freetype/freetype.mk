################################################################################
#
# freetype
#
################################################################################


define FREETYPE_INSTALL_MISC
		rm -rf $(TARGET_DIR)/etc/simsun.ttc

		$(INSTALL) -D -m 664 \
			$(BR2_EXTERNAL_platform_PATH)/package/buildroot/freetype/simsun.ttc \
			$(TARGET_DIR)/etc/simsun.ttc

endef

FREETYPE_POST_INSTALL_TARGET_HOOKS += FREETYPE_INSTALL_MISC
