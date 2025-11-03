ARM_TRUSTED_FIRMWARE_MAKE_OPTS := $(strip \
	$(filter-out HOSTCC=% \
	HOSTCC="$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS)", \
	$(ARM_TRUSTED_FIRMWARE_MAKE_OPTS)))

define ATF_UPDATE_BINARY
	# update binary for atf.bin
	$(Q)$(call MESSAGE, "cp binary to atf.bin")
	if [ ! -d $(ATF_BIN_TARGET)/ram_init ]; then \
		mkdir -p $(ATF_BIN_TARGET)/ram_init; \
	fi
	if [ ! -d $(ATF_BIN_TARGET)/tools/cert_create ]; then \
		mkdir -p $(ATF_BIN_TARGET)/tools/cert_create; \
	fi
	if [ ! -d $(ATF_BIN_TARGET)/tools/fiptool ]; then \
		mkdir -p $(ATF_BIN_TARGET)/tools/fiptool; \
	fi
	cp -afrv $(@D)/ram_init/generate.sh $(ATF_BIN_TARGET)/ram_init/.
	cp -afrv $(@D)/tools/cert_create/cert_create $(ATF_BIN_TARGET)/tools/cert_create/.
	cp -afrv $(@D)/tools/fiptool/fiptool $(ATF_BIN_TARGET)/tools/fiptool/.
endef

ARM_TRUSTED_FIRMWARE_POST_INSTALL_TARGET_HOOKS += ATF_UPDATE_BINARY
