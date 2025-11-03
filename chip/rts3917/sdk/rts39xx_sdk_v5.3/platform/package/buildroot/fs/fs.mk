CRYPTED = $(if $(BR2_CONFIG_CRYPTO_BOOT),.crypted,)
SIGNED = $(if $(BR2_CONFIG_SECURE_BOOT),.signed,)
MTD_MAPPING_CONFIG="$(LINUX_ARCH_PATH)/boot/dts/$(DTS_NAME).dts"

ifeq ($(BR2_FW_PACK_USERDATA_UBIFS),y)
ROOTFS_FS_NAME = $(ROOTFS_UBI_FINAL_IMAGE_NAME)
endif

# squashfs
ifeq ($(BR2_TARGET_ROOTFS_SQUASHFS),y)
ROOTFS_FS_NAME = $(ROOTFS_SQUASHFS_FINAL_IMAGE_NAME)
endif

# ext2
ifeq ($(BR2_TARGET_ROOTFS_EXT2),y)
ROOTFS_FS_NAME = $(ROOTFS_EXT2_FINAL_IMAGE_NAME)
endif

ifeq ($(BR2_FW_PACK_USERDATA_JFFS2),y)
USER_DATA_FS = jffs2
endif

ifeq ($(BR2_FW_PACK_USERDATA_UBIFS),y)
USER_DATA_FS = ubi
endif

ifeq ($(BR2_FW_PACK_USERDATA_EXT4),y)
USER_DATA_FS = ext4
endif

ifeq ($(BR2_FW_PACK_USERDATA_SQUASHFS),y)
USER_DATA_FS = squashfs
endif

# lstrip
ifeq ($(BR2_CONFIG_LSTRIP),y)

ifeq ($(BR2_CONFIG_LSTRIP_LIB_IN_STAGING),y)
LSTRIP_SDK_LIB = --sdk_lib $(STAGING_DIR)/lib
endif

define ROOTFS_LSTRIP
	$(call MESSAGE,"lstrip rootfs $(ROOTFS) target dir")
	cd $O
	cp -avf $(BR2_EXTERNAL)/scripts/lstrip/libs_keep .libs_keep
	cp -avf $(BR2_EXTERNAL)/scripts/lstrip/lib_deps .lib_deps
	cp -avf $(BR2_EXTERNAL)/scripts/lstrip/lstrip_keep .lstrip_keep

	sed -i 's;TARGET_DIR;$(TARGET_DIR);' .libs_keep

	if [ -n "$(CRYPTED)$(SIGNED)" ] && [ "$(ROOTFS)" = "CPIO" ]; then
		rm -vf .libs_keep
	fi

	$(HOST_DIR)/bin/$(TOOLCHAIN_EXTERNAL_PREFIX)-lstrip $(TARGET_DIR) \
		--lib_dirs lib:usr/lib $(LSTRIP_SDK_LIB)
	cd -
endef
ROOTFS_PRE_CMD_HOOKS += ROOTFS_LSTRIP

endif

define MK_GEN_TARGET_MINI
	@$(call MESSAGE,"Generating target-mini")
	$(RM) -r $(BASE_TARGET_DIR)-mini
	cp -rf $(BASE_TARGET_DIR) $(BASE_TARGET_DIR)-mini
endef
SKELETON_POST_INSTALL_TARGET_HOOKS += MK_GEN_TARGET_MINI

define MK_USERDATA_FSTAB
	$(call MESSAGE,"Generating userdata fstab")
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/part.py fstab \
		user \
		$(BR2_CONFIG) \
		$(LINUX_DTS_PATH)/partitions.dts \
		$(TARGET_DIR)/etc/fstab.user
endef

TARGET_FINALIZE_HOOKS += MK_USERDATA_FSTAB

ifeq ($(BR2_PACKAGE_RAUC_UPFW),y)
define MK_RAUCONF
        $(call MESSAGE,"Generating rauc conf")
        $(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/part.py rauconf \
                $(LINUX_ARCH_PATH)/boot/dts/realtek/partitions.dts \
                $(BASE_TARGET_DIR)/etc/rauc/system.conf
endef
TARGET_FINALIZE_HOOKS += MK_RAUCONF
endif

# ext4
define MK_RESIZE_EXT4_ROOTFS
	$(call MESSAGE,"Resize ext4 root filesystem image $(ROOTFS_EXT2_FINAL_IMAGE_NAME)")
	echo '#!/bin/sh' > $(FAKEROOT_SCRIPT)
	echo "set -e" >> $(FAKEROOT_SCRIPT)
	echo "$(HOST_DIR)/sbin/resize2fs rootfs.ext2 -M" >> $(FAKEROOT_SCRIPT)
	cd $(BINARIES_DIR); PATH=$(BR_PATH) $(HOST_DIR)/bin/fakeroot -- $(FAKEROOT_SCRIPT);
endef
ROOTFS_EXT2_POST_GEN_HOOKS += MK_RESIZE_EXT4_ROOTFS


# secure boot: dm_verity
ifeq ($(BR2_CONFIG_SECURE_DM_MAPPER),y)

define MK_SIGNED_ROOTFS
	$(call MESSAGE,"Generating signed root filesystem image $(ROOTFS_FS_NAME)$(SIGNED)")

	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/build_verity_img.py build \
		$(HOST_DIR)/sbin/ \
		$(BINARIES_DIR)/$(ROOTFS_FS_NAME) \
		"/newroot" \
		$(if $(CRYPTED),"dev/mapper/dm-crypt-newroot", \
			`cat ${MTD_MAPPING_CONFIG} | grep root= | awk -Froot= '{print $$2}'| awk '{print $$1}'`) \
		$(RTSKEY_OUT_DIR)/$(SECURE_DATA_KEY) \
		$(BINARIES_DIR)/$(ROOTFS_FS_NAME)$(SIGNED)
endef

ROOTFS_SQUASHFS_POST_GEN_HOOKS += MK_SIGNED_ROOTFS
ROOTFS_EXT2_POST_GEN_HOOKS += MK_SIGNED_ROOTFS
endif # BR2_CONFIG_SECURE_DM_MAPPER

# crypto boot: FDE
ifeq ($(BR2_CONFIG_FULL_DISK_ENCRYPTION),y)

define MK_CRYPTED_ROOTFS
	$(call MESSAGE,"Generating crypted root filesystem image $(ROOTFS_FS_NAME)$(SIGNED)$(CRYPTED)")
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/mkcryptfs.py \
		$(BINARIES_DIR)/$(ROOTFS_FS_NAME)$(SIGNED) \
		$(BINARIES_DIR)/$(ROOTFS_FS_NAME)$(SIGNED)$(CRYPTED) \
		$(RTSKEY_OUT_DIR)/$(CRYPTO_KEY) \
		4096
endef

ROOTFS_SQUASHFS_POST_GEN_HOOKS += MK_CRYPTED_ROOTFS
ROOTFS_EXT2_POST_GEN_HOOKS += MK_CRYPTED_ROOTFS
endif # BR2_CONFIG_FULL_DISK_ENCRYPTION

# dm_init
ifeq ($(BR2_CONFIG_DM_INIT),y)

define MK_ROOTFS_DM_INIT
	$(call MESSAGE,"Generating $(BINARIES_DIR)/rootfs_dm_init.txt")

	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/part.py dminit \
		$(BR2_CONFIG) \
		$(LINUX_DTS_PATH)/partitions.dts \
		$(BINARIES_DIR)/$(ROOTFS_FS_NAME).verity.info \
		$(BINARIES_DIR)/rootfs_dm_init.txt
endef

ROOTFS_SQUASHFS_POST_GEN_HOOKS += MK_ROOTFS_DM_INIT
ROOTFS_EXT2_POST_GEN_HOOKS += MK_ROOTFS_DM_INIT
endif # BR2_CONFIG_DM_INIT

# secure boot: ubifs authentication
ifeq ($(BR2_CONFIG_UBIFS_FS_AUTHENTICATION),y)
UBIFS_OPTS += 	--hash-algo sha256 \
		--auth-key $(RTSKEY_OUT_DIR)/$(SECURE_DATA_KEY) \
		--auth-cert $(RTSKEY_OUT_DIR)/$(SECURE_DATA_CRT)
endif

# crypto boot: FBE
ifeq ($(BR2_CONFIG_FILE_BASED_ENCRYPTION),y)
ifeq ($(BR2_CONFIG_FBE_AES_256_XTS),y)
UBIFS_OPTS += 	--cipher AES-256-XTS
else
UBIFS_OPTS += 	--cipher AES-128-CBC
endif
UBIFS_OPTS +=	--key $(RTSKEY_OUT_DIR)/$(FBE_KEY)
endif


# ubi over squashfs
ifeq ($(BR2_TARGET_ROOTFS_SQUASHFS),y)
ROOTFS_UBI_DEPENDENCIES = rootfs-squashfs

define ROOTFS_UBI_CMD
	sed 's;BR2_ROOTFS_UBIFS_PATH;$(BINARIES_DIR)/$(ROOTFS_FS_NAME)$(SIGNED)$(CRYPTED);' \
		$(UBI_UBINIZE_CONFIG_FILE_PATH) > $(BUILD_DIR)/ubinize.cfg
	$(HOST_DIR)/sbin/ubinize -o $@ $(UBI_UBINIZE_OPTS) $(BUILD_DIR)/ubinize.cfg
	rm $(BUILD_DIR)/ubinize.cfg
endef
endif

# crypto or secure boot: configure the initramfs-mini installation
ifeq ($(findstring y, $(BR2_CONFIG_SECURE_BOOT)$(BR2_CONFIG_CRYPTO_BOOT)),y)
# initramfs-mini
ifeq ($(BR2_TARGET_ROOTFS_INITRAMFS_MINI),y)

define MK_TOOLCHAIN_DELETE
	@$(call MESSAGE,"Delete unnecessary toolchain libs")
	$(RM) -r $(TARGET_DIR)/lib/libgcc_s*
endef
TOOLCHAIN_POST_INSTALL_TARGET_MINI_HOOKS += MK_TOOLCHAIN_DELETE

ifeq ($(BR2_PACKAGE_RTSKEY),y)
INITRAMFS_MINI_INSTALL += rtskey
endif

ifeq ($(BR2_PACKAGE_LVM2),y)
INITRAMFS_MINI_INSTALL += lvm2
LVM2_CONF_OPTS += --disable-dmeventd --disable-blkdeactivate
endif

ifeq ($(BR2_PACKAGE_KEYUTILS),y)
INITRAMFS_MINI_INSTALL += keyutils
endif

ifeq ($(BR2_PACKAGE_FSCRYPTCTL),y)
INITRAMFS_MINI_INSTALL += fscryptctl
endif


ifeq ($(BR2_TARGET_ROOTFS_UBI)$(BR2_PACKAGE_MTD_UBIATTACH),yy)
INITRAMFS_MINI_INSTALL += mtd

define MTD_INSTALL_TARGET_MINI_CMDS
	$(INSTALL) -D -m 0755 $(@D)/ubiattach $(TARGET_DIR)/usr/sbin/ubiattach
endef
endif

endif # BR2_TARGET_ROOTFS_INITRAMFS_MINI
endif # BR2_CONFIG_CRYPTO_BOOT || BR2_CONFIG_SECURE_BOOT

include $(sort $(wildcard $(BR2_EXTERNAL_platform_PATH)/package/buildroot/fs/*/*.mk))
