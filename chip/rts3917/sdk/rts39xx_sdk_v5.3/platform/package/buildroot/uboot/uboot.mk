################################################################################
#
# uboot custom script
#
################################################################################

ifeq ($(BR2_CONFIG_SECURE_BOOT),y)
UBOOT_MAKE_OPTS += EXT_DTB=$(BINARIES_DIR)/u-boot_pubkey.dtb
endif

# This define build secure boot or normal boot
ifeq ($(BR2_TARGET_ARM_TRUSTED_FIRMWARE),y)
UBOOT_MAKE_OPTS += ATF_BOOT=y
endif

# This define uboot is bl2 or bl33
ifeq ($(BR2_TARGET_ARM_TRUSTED_FIRMWARE_UBOOT_AS_BL33),y)
UBOOT_MAKE_OPTS += UBOOT_AS_BL33=y
endif

UBOOT_RTS_PATCHES := $(BR2_EXTERNAL_platform_PATH)/patches/uboot-$(BR2_TARGET_UBOOT_VERSION) \

define UBOOT_APPLY_LOCAL_RTS_PATCHES
	for p in $(UBOOT_RTS_PATCHES) ; do \
		if test -d $$p ; then \
			$(APPLY_PATCHES) $(@D) $$p \*.patch || exit 1 ; \
		else \
			$(APPLY_PATCHES) $(@D) `dirname $$p` `basename $$p` || exit 1; \
		fi \
        done
endef

UBOOT_POST_RSYNC_HOOKS += UBOOT_APPLY_LOCAL_RTS_PATCHES

define UBOOT_INSTALL_TOOLS
	$(Q)echo ">>>   Install uboot env tool"
	$(INSTALL) -D -m 755 $(@D)/tools/env/fw_printenv $(TARGET_DIR)/usr/bin/fw_printenv
	ln -sf fw_printenv $(TARGET_DIR)/usr/bin/fw_setenv
endef

# trick to avoid uboot build always run
define UBOOT_SYNC_STAMPS
	touch $(@D)/.config
	touch $(@D)/.stamp_*
endef

UBOOT_POST_INSTALL_TARGET_HOOKS += UBOOT_INSTALL_TOOLS
UBOOT_POST_INSTALL_TARGET_HOOKS += UBOOT_SYNC_STAMPS

define UBOOT_MAKE_TOOLS
	$(Q)echo ">>>   Make uboot env tool"
	CROSS_COMPILE="$(TARGET_CROSS)" make -C $(@D) envtools
endef

UBOOT_POST_BUILD_HOOKS += UBOOT_MAKE_TOOLS

define UBOOT_KCONFIG_CUSTOM_FIXUP
	$(if $(filter $(BR2_CONFIG_SECURE_BOOT),y),
		$(call KCONFIG_ENABLE_OPT,CONFIG_FIT,$(@D)/.config)
		$(call KCONFIG_SET_OPT,CONFIG_FIT_EXTERNAL_OFFSET,0x0,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_FIT_SIGNATURE,$(@D)/.config)
		$(call KCONFIG_SET_OPT,CONFIG_FIT_SIGNATURE_MAX_SIZE,0x10000000,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT_RSASSA_PSS,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT_CIPHER,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT_VERBOSE,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT_BEST_MATCH,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_FIT_PRINT,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_BOOTMETH_VBE,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_BOOTMETH_VBE_SIMPLE,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_BOOTMETH_VBE_SIMPLE_OS,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_UPDATE_TFTP,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_CMD_VBE,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_RSA_VERIFY_WITH_PKEY,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_ASYMMETRIC_KEY_TYPE,$(@D)/.config))
	$(if $(filter-out $(BR2_CONFIG_SECURE_BOOT),y),
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_FIT_SIGNATURE,$(@D)/.config))
	$(if $(filter $(BR2_CONFIG_CRYPTO_BOOT),y),
		$(call KCONFIG_ENABLE_OPT,CONFIG_CRYPTO_BOOT,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_CRYPTO_UPDATE,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_CBC_MODE,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_ECB_MODE,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_AES_256,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_AES_128,$(@D)/.config))
endef

define UBOOT_PREBUILD_DTB
	$(Q)echo ">>>   Pre-build dtb for uboot"
	make -C $(O) dtb
endef

UBOOT_PRE_BUILD_HOOKS += UBOOT_KCONFIG_CUSTOM_FIXUP

ifneq ($(BR2_CONFIG_SECURE_BOOT),y)
UBOOT_PRE_BUILD_HOOKS += UBOOT_PREBUILD_DTB
endif

