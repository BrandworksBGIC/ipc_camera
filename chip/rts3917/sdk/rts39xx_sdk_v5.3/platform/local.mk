UBOOT_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/bootloader/uboot-$(BR2_TARGET_UBOOT_VERSION)
BR2_TARGET_ARM_TRUSTED_FIRMWARE_VERSION := $(subst ",,$(BR2_TARGET_ARM_TRUSTED_FIRMWARE_VERSION))

define SET_ATF_PATH
	ifeq ($(notdir $(shell readlink $(BR2_EXTERNAL)/external.mk)),external_default.mk)
		$(1) := $(BR2_EXTERNAL)/source/bootloader/atf-$(BR2_TARGET_ARM_TRUSTED_FIRMWARE_VERSION)/atf.bin
	else
		$(1) := $(BR2_EXTERNAL)/source/bootloader/atf-$(BR2_TARGET_ARM_TRUSTED_FIRMWARE_VERSION)
	endif
endef

$(eval $(call SET_ATF_PATH, ARM_TRUSTED_FIRMWARE_OVERRIDE_SRCDIR))

OPTEE_OS_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/bootloader/optee_os
OPTEE_CLIENT_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/security/optee_client
OPTEE_TSET_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/security/optee_test
LINUX_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/kernel/linux-$(BR2_LINUX_KERNEL_VERSION)
LIBOPENSSL_OVERRIDE_SRCDIR = $(BR2_EXTERNAL)/source/lib/rts_openssl

ifeq ($(BR2_PACKAGE_HOST_PYTHON3_PYDEVICETREE),y)
LINUX_DEPENDENCIES += host-python3-pydevicetree host-python3-pycryptodomex
endif

ifneq ($(BR2_MAX_JLEVEL), 0)
ifeq ($(shell test $(PARALLEL_JOBS) -gt $(BR2_MAX_JLEVEL); echo $$?),0)
PARALLEL_JOBS = $(BR2_MAX_JLEVEL)
endif
endif

ifeq ($(BR2_CONFIG_SECURE_BOOT),y)
ifeq ($(BR2_CONFIG_CRYPTO_BOOT),y)
TMP_BOOT_MODE = "secure crypto"
else
TMP_BOOT_MODE = "secure"
endif
else
ifeq ($(BR2_CONFIG_CRYPTO_BOOT),y)
TMP_BOOT_MODE = "crypto"
else
TMP_BOOT_MODE = "normal"
endif
endif

define UPDATE_VERSION
	cat $(BR2_EXTERNAL_platform_PATH)/VERSION > $(TARGET_DIR)/etc/version; \
	echo "$(TMP_BOOT_MODE) boot" >> $(TARGET_DIR)/etc/version; \
	echo `whoami`"@"`hostname` >> $(TARGET_DIR)/etc/version; \
	echo `date "+%Y-%m-%d %H:%M:%S"` >> $(TARGET_DIR)/etc/version
endef
TARGET_FINALIZE_HOOKS += UPDATE_VERSION

export BR2_FW_PACK_MERGE_FILE := $(call qstrip,$(BR2_FW_PACK_MERGE_FILE))

# secure boot
ifeq ($(BR2_CONFIG_SECURE_BOOT),y)
UBOOT_DEPENDENCIES += rtskey
ARM_TRUSTED_FIRMWARE_DEPENDENCIES += uboot
ARM_TRUSTED_FIRMWARE_MAKE_OPTS += ROT_KEY=$(RTSKEY_OUT_DIR)/verity_key0.key
#ARM_TRUSTED_FIRMWARE_MAKE_OPTS += OPENSSL_DIR=$(HOST_DIR)
ARM_TRUSTED_FIRMWARE_MAKE_OPTS += ARM_CORTEX_A7=yes
ARM_TRUSTED_FIRMWARE_MAKE_OPTS += V=1
endif


pack:
	$(RM) $(BINARIES_DIR)/linux.bin
	$(RM) $(BINARIES_DIR)/raw_*.bin
ifneq ($(BR2_CONFIG_SECURE_BOOT),y)
	cp $(BINARIES_DIR)/u-boot.bin $(BINARIES_DIR)/boot.bin
	cp $(BINARIES_DIR)/uImage.dtb$(CRYPTED) $(BINARIES_DIR)/kernel.img
else
	cp $(BINARIES_DIR)/fip.bin $(BINARIES_DIR)/boot.bin
	cp $(BINARIES_DIR)/linux$(CRYPTED).itb $(BINARIES_DIR)/kernel.img
endif
ifeq ($(BR2_TARGET_ROOTFS_UBI),y)
	cp $(BINARIES_DIR)/$(ROOTFS_UBI_FINAL_IMAGE_NAME) $(BINARIES_DIR)/rootfs.fs
else
	cp $(BINARIES_DIR)/$(ROOTFS_FS_NAME)$(SIGNED)$(CRYPTED) $(BINARIES_DIR)/rootfs.fs
endif
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/merge.py $(O) $(USER_DATA_FS) $(BR2_CONFIG)

ifeq ($(BR2_CONFIG_BUNDLE_RAUCB),y)
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/mkraucb.py $(O) $(USER_DATA_FS) $(BR2_CONFIG)
endif

ifeq ($(BR2_CONFIG_BUNDLE_SWU),y)
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/mkswu.py $(O) $(USER_DATA_FS) $(BR2_CONFIG)
endif

	$(RM) $(BINARIES_DIR)/rootfs.fs
	$(RM) $(BINARIES_DIR)/kernel.img*
	$(RM) $(BINARIES_DIR)/boot.bin

rr:
	$(Q)$(call MESSAGE, "Reconstructing rootfs.")
	$(RM) -r $(BINARIES_DIR)/*
	$(RM) -r $(TARGET_DIR)
	$(RM) -r $(TARGET_DIR)-*
	$(RM) -r $(STAGING_DIR)
	$(RM) -r $(BASE_DIR)/staging
	find $(BUILD_DIR) -type f \( -name ".stamp_staging_installed" \
				-o -name ".stamp_target_installed" \
				-o -name ".stamp_target_mini_installed" \
				-o -name ".stamp_images_installed" \
				-o -name "packages-file-list.txt" \
				-o -name "packages-file-list-staging.txt" \) -exec rm -f {} \;
	$(MAKE1) -C $(BASE_DIR)

define PREPARE_FOR_REBUILD
	[ -f '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)'/.stamp_patched ] \
		&& rm -fr '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)'; \
	if [ -d '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)' ]; then \
		find '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)' -name '.stamp*' | grep -v .stamp_dotconfig | xargs  rm -f; \
	fi
endef

define PREPARE_FOR_DIRCLEAN
	[ -f '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)'/.stamp_patched ] \
		&& rm -fr '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)'; \
	if [ -d '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)' ]; then \
		if [ "$(1)" != "linux" ]  && [ "$(1)" != "uboot" ] ; then \
			$(MAKE1) -C $(BASE_DIR) $(1)-dirclean; \
		else \
			find '$(BUILD_DIR)/$($(call UPPERCASE,$(1))_BASENAME)' -name '.stamp*' | grep -v .stamp_dotconfig | xargs  rm -f; \
		fi \
	fi
endef

rd:
	$(Q)$(call MESSAGE, "Rebuiding $(BR2_EXTERNAL_NAMES)")
	@:$(foreach p,$(EXTERNAL_ONLY_PACKAGES),$(if $(filter $p,$(PACKAGES)),\
		$(call MESSAGE, "$p is targeted for dirclean and build");\
		$(call PREPARE_FOR_DIRCLEAN,$p))$(sep))
	$(MAKE1) -C $(BASE_DIR) rr

rp:
	$(Q)$(call MESSAGE, "Rebuiding $(BR2_EXTERNAL_NAMES)")
	@:$(foreach p,$(EXTERNAL_ONLY_PACKAGES),$(if $(filter $p,$(PACKAGES)),\
		$(call MESSAGE, "$p is targeted for rebuild");\
		$(call PREPARE_FOR_REBUILD,$p))$(sep))
	$(MAKE1) -C $(BASE_DIR) rr

binary:
	$(Q)$(call MESSAGE, "update binary")
	mkdir -p $(BASE_DIR)/build
	rm -rfv $(RTSCORE_BIN_TARGET) $(REALBOX_BIN_TARGET) $(IPCAM_BIN_TARGET) $(NETWORK_BIN_TARGET) $(USB_BIN_TARGET) $(ISPTUNING_BIN_TARGET)
	### only reserve Makefile in atf.bin
	find ${ATF_BIN_TARGET} -type f ! -name 'Makefile' -exec rm -f {} +
	echo "BR2_CONFIG_GEN_BINARY=y" >> $(BASE_DIR)/.config
	$(MAKE1) -C $(BASE_DIR) rp
	sed -i "/BR2_CONFIG_GEN_BINARY=y/d" $(BASE_DIR)/.config

SECURE_NAME = Secure and Crypto Boot
HELP_PACKAGES += SECURE

define SECURE_HELP_CMDS
	@echo '  secure_boot [M=0|1|2]  - make world, and enable crypto or secure boot'
	@echo '                             0: enable crypto boot'
	@echo '                             1: enable secure boot'
	@echo '                             2: enable crypto and secure boot. default value'
	@echo '  secure_clean           - Disable crypto and secure boot'
endef
M = 2

.PHONY: secure_dependency secure_boot secure_uboot secure_clean

secure_dependency:
	$(SECURE_HELP_CMDS)
	@if [[ $(M) =~ ^[0-2]$$  ]]; then \
		$(call MESSAGE, "Enable crypto or secure boot");\
		$(call MESSAGE, "Switching mode need to execute the launch.sh again"); \
	else \
		$(call MESSAGE, "Invalid secure boot parameter");\
		exit 1;\
	fi

	$(call KCONFIG_DISABLE_OPT,BR2_CONFIG_CRYPTO_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_CONFIG_SECURE_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_PACKAGE_FSTOOLS,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_TARGET_ROOTFS_INITRAMFS,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_TARGET_ROOTFS_CPIO,$(BR2_CONFIG))
ifeq ($(M),0)
	$(Q)$(call MESSAGE, "Crypto boot mode")
	$(call KCONFIG_ENABLE_OPT,BR2_CONFIG_CRYPTO_BOOT,$(BR2_CONFIG))
endif
ifeq ($(M),1)
	$(Q)$(call MESSAGE, "Secure boot mode")
	$(call KCONFIG_ENABLE_OPT,BR2_CONFIG_SECURE_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_FIP,$(BR2_CONFIG))
	$(call KCONFIG_SET_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION_VALUE,"2.3",$(BR2_CONFIG))
	$(call KCONFIG_SET_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_PLATFORM,"sheipa",$(BR2_CONFIG))
endif
ifeq ($(M),2)
	$(Q)$(call MESSAGE, "Crypto and Secure boot mode")
	$(call KCONFIG_ENABLE_OPT,BR2_CONFIG_CRYPTO_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_CONFIG_SECURE_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION,$(BR2_CONFIG))
	$(call KCONFIG_ENABLE_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_FIP,$(BR2_CONFIG))
	$(call KCONFIG_SET_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION_VALUE,"2.3",$(BR2_CONFIG))
	$(call KCONFIG_SET_OPT,BR2_TARGET_ARM_TRUSTED_FIRMWARE_PLATFORM,"sheipa",$(BR2_CONFIG))
endif
	$(call KCONFIG_DISABLE_OPT,BR2_PACKAGE_LVM2_STANDARD_INSTALL,$(BR2_CONFIG))

	$(MAKE1) -C $(BASE_DIR) olddefconfig
	if [ -e $(BUILD_DIR)/kmod_* ]; then \
		find $(BUILD_DIR)/kmod_* -type f -name ".stamp_built" -exec rm -f {} \;; \
	fi

secure_uboot:
	$(MAKE1) -C $(BASE_DIR) rtskey-rebuild uboot-rebuild arm-trusted-firmware-dirclean

ifneq ($(M),0)
	$(MAKE1) -C $(BASE_DIR) arm-trusted-firmware
endif

secure_boot: secure_dependency secure_uboot
	$(MAKE1) -C $(BASE_DIR) linux-reconfigure host-mtd-dirclean rr

secure_clean:
	$(call KCONFIG_DISABLE_OPT,BR2_CONFIG_CRYPTO_BOOT,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_CONFIG_SECURE_BOOT,$(BR2_CONFIG))

	$(call KCONFIG_DISABLE_OPT,BR2_LINUX_KERNEL_INSTALL_TARGET,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_TARGET_ROOTFS_INITRAMFS,$(BR2_CONFIG))
	$(call KCONFIG_DISABLE_OPT,BR2_TARGET_ROOTFS_CPIO,$(BR2_CONFIG))

	$(MAKE1) -C $(BASE_DIR) olddefconfig
	$(MAKE1) -C $(BASE_DIR) uboot-rebuild arm-trusted-firmware-dirclean linux-rebuild host-mtd-dirclean
	if [ -e $(BUILD_DIR)/kmod_* ]; then \
		find $(BUILD_DIR)/kmod_* -type f -name ".stamp_built" -exec rm -f {} \;; \
	fi

.PHONY: dtb

dtb:
	if [ -e $(LINUX_DIR)/.config ]; then \
		rsync -au --chmod=u=rwX,go=rX  $(RSYNC_VCS_EXCLUSIONS) $(BR2_EXTERNAL)/source/kernel/linux-$(call qstrip,$(BR2_LINUX_KERNEL_VERSION))/ $(LINUX_DIR);\
	else \
		mkdir -p $(LINUX_DIR) ;\
		rsync -au --chmod=u=rwX,go=rX  $(RSYNC_VCS_EXCLUSIONS) $(BR2_EXTERNAL)/source/kernel/linux-$(call qstrip,$(BR2_LINUX_KERNEL_VERSION))/ $(LINUX_DIR);\
		$(INSTALL) -m 0644 -D $(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE)) $(LINUX_DIR)/.config;\
	fi

	$(LINUX_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) $(LINUX_DTBS)

	mkdir -p $(BASE_DIR)/build/uboot-custom
	cp $(LINUX_ARCH_PATH)/boot/dts/$(call qstrip,$(BR2_LINUX_KERNEL_INTREE_DTS_NAME)).dtb $(BASE_DIR)/build/uboot-custom/u-boot.dtb;

.PHONY: dtbs_check

pip3 = $(HOST_DIR)/bin/pip3

dtbs_check: host-python3-packaging host-libyaml host-python-vcversioner host-python3-pip \
	host-python3-tomli host-python3-setuptools_scm host-python3-pyparsing \
	host-python3-six host-python3-ruamel-yaml host-swig host-python3-libfdt host-python3-dtschema
	cd $(BASE_DIR) && make host-python3-jsonschema-configure;
	cd $(BUILD_DIR)/host-python3-jsonschema-4.4.0/ && $(pip3) install . ;
	rsync -au --chmod=u=rwX,go=rX  $(RSYNC_VCS_EXCLUSIONS) $(BR2_EXTERNAL)/source/kernel/linux-$(call qstrip,$(BR2_LINUX_KERNEL_VERSION))/ $(LINUX_DIR) ;
	$(foreach dts,$(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_DTS_PATH)), \
		cp -rf $(dts) $(LINUX_ARCH_PATH)/boot/dts/;)
	$(LINUX_DTBS_CHECK)
