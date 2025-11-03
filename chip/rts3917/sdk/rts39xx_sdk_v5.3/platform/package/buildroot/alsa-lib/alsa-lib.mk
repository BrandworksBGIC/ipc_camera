define ALSA_LIB_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/.libs/libasound.so.2.0.0 $(TARGET_DIR)/usr/lib/libasound.so.2.0.0
	ln -sf libasound.so.2.0.0 $(TARGET_DIR)/usr/lib/libasound.so.2
	ln -sf libasound.so.2.0.0 $(TARGET_DIR)/usr/lib/libasound.so
	mkdir -p $(TARGET_DIR)/usr/share/alsa
	mkdir -p $(TARGET_DIR)/usr/share/alsa/cards
	mkdir -p $(TARGET_DIR)/usr/share/alsa/pcm
	mkdir -p $(TARGET_DIR)/usr/share/alsa/ctl
	$(INSTALL) -D -m 0644 $(@D)/src/conf/alsa.conf $(TARGET_DIR)/usr/share/alsa/alsa.conf
	$(INSTALL) -D -m 0644 $(@D)/src/conf/cards/aliases.conf $(TARGET_DIR)/usr/share/alsa/cards/aliases.conf
	$(INSTALL) -D -m 0644 $(@D)/src/conf/pcm/default.conf $(TARGET_DIR)/usr/share/alsa/pcm/default.conf
	$(INSTALL) -D -m 0644 $(@D)/src/conf/pcm/dmix.conf $(TARGET_DIR)/usr/share/alsa/pcm/dmix.conf
	$(INSTALL) -D -m 0644 $(@D)/src/conf/pcm/dmix.conf $(TARGET_DIR)/usr/share/alsa/pcm/dsnoop.conf
	$(INSTALL) -D -m 0644 $(@D)/src/conf/ctl/default.conf $(TARGET_DIR)/usr/share/alsa/ctl/default.conf
endef
