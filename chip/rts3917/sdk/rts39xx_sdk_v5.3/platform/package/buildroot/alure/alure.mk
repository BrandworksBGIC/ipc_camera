define ALURE_INSTALL_STAGING_CMDS
	mkdir -p $(STAGING_DIR)/usr/include/alure
	$(INSTALL) -D -m 0644 $(@D)/src/decoders/minimp3.h \
				$(STAGING_DIR)/usr/include/alure/minimp3.h
endef
