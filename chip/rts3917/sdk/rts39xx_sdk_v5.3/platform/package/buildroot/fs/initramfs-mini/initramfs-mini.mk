ifeq ($(BR2_TARGET_ROOTFS_INITRAMFS_MINI),y)

# force use of target-mini dir make mini-initramfs
TARGET_MINI_DIR := $(BASE_TARGET_DIR)-mini
$(BINARIES_DIR)/rootfs.cpio: BASE_TARGET_DIR=$(TARGET_MINI_DIR)

# get packages specified in menuconfig that need to be install to mini-initramfs
INITRAMFS_MINI_INSTALL += $(strip $(patsubst "%",%, \
			  $(BR2_TARGET_ROOTFS_INITRAMFS_MINI_INCLUDE)))


ifeq ($(BR2_PACKAGE_BUSYBOX),y)
INITRAMFS_MINI_INSTALL_NDEPEND += busybox
ifneq ($(BR2_CONFIG_INITRAMFS_MINI_BUSYBOX_SOURCE),"")
define BUSYBOX_INSTALL_TARGET_MINI_CMDS
	$(INSTALL) -D -m 0755 $(BR2_CONFIG_INITRAMFS_MINI_BUSYBOX_SOURCE) $(TARGET_DIR)/usr/bin/busybox
	cd $(TARGET_DIR) && ln -sf bin/busybox linuxrc \
		&& ln -sf ../bin/busybox bin/sh
endef
endif
endif

ifneq ($(BR2_CONFIG_INITRAMFS_MINI_INIT_SOURCE),"")
define ROOTFS_CPIO_ADD_INIT
	if [ ! -e $(TARGET_DIR)/init ]; then \
		$(INSTALL) -m 0755 $(BR2_CONFIG_INITRAMFS_MINI_INIT_SOURCE) $(TARGET_DIR)/init; \
	fi
	mkdir -p $(TARGET_DIR)/dev
	mknod -m 0622 $(TARGET_DIR)/dev/console c 5 1
endef
endif

ifeq ($(BR2_PACKAGE_DBUS),y)
INITRAMFS_MINI_INSTALL += dbus

define DBUS_INSTALL_TARGET_MINI_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bus/dbus-daemon-launch-helper \
		$(TARGET_DIR)/usr/libexec/dbus-daemon-launch-helper
endef
endif


# adjust rootfs targets compilation order
TARGETS_ROOTFS := $(filter-out rootfs-cpio rootfs-initramfs,$(TARGETS_ROOTFS))
TARGETS_ROOTFS += rootfs-cpio rootfs-initramfs


.PHONY: target-mini-finalize

target-finalize: target-mini-finalize
target-mini-finalize: $(PACKAGES)
	@$(call MESSAGE,"Finalizing target-mini directory")
	$(foreach hook,$(TARGET_MINI_FINALIZE_HOOKS),$($(hook))$(sep))
	rm -rf $(TARGET_MINI_DIR)/usr/include $(TARGET_MINI_DIR)/usr/share/aclocal \
		$(TARGET_MINI_DIR)/usr/lib/pkgconfig $(TARGET_MINI_DIR)/usr/share/pkgconfig \
		$(TARGET_MINI_DIR)/usr/lib/cmake $(TARGET_MINI_DIR)/usr/share/cmake \
		$(TARGET_MINI_DIR)/usr/doc $(TARGET_MINI_DIR)/usr/share/locale
	find $(TARGET_MINI_DIR)/usr/{lib,share}/ -name '*.cmake' -print0 | xargs -0 rm -f
	find $(TARGET_MINI_DIR)/lib/ $(TARGET_MINI_DIR)/usr/lib/ $(TARGET_MINI_DIR)/usr/libexec/ \
		\( -name '*.a' -o -name '*.la' -o -name '*.prl' \) -print0 | xargs -0 rm -f
ifneq ($(BR2_PACKAGE_GDB),y)
	rm -rf $(TARGET_MINI_DIR)/usr/share/gdb
endif
ifneq ($(BR2_PACKAGE_BASH),y)
	rm -rf $(TARGET_MINI_DIR)/usr/share/bash-completion
	rm -rf $(TARGET_MINI_DIR)/etc/bash_completion.d
endif
ifneq ($(BR2_PACKAGE_ZSH),y)
	rm -rf $(TARGET_MINI_DIR)/usr/share/zsh
endif
	rm -rf $(TARGET_MINI_DIR)/usr/man $(TARGET_MINI_DIR)/usr/share/man
	rm -rf $(TARGET_MINI_DIR)/usr/info $(TARGET_MINI_DIR)/usr/share/info
	rm -rf $(TARGET_MINI_DIR)/usr/doc $(TARGET_MINI_DIR)/usr/share/doc
	rm -rf $(TARGET_MINI_DIR)/usr/share/gtk-doc
	rmdir $(TARGET_MINI_DIR)/usr/share 2>/dev/null || true
ifneq ($(BR2_ENABLE_DEBUG):$(BR2_STRIP_strip),y:)
	rm -rf $(TARGET_MINI_DIR)/lib/debug $(TARGET_MINI_DIR)/usr/lib/debug
endif
	$(STRIP_FIND_CMD) | xargs -0 $(STRIPCMD) 2>/dev/null || true
	$(STRIP_FIND_SPECIAL_LIBS_CMD) | xargs -0 -r $(STRIPCMD) $(STRIP_STRIP_DEBUG) 2>/dev/null || true

	test -f $(TARGET_MINI_DIR)/etc/ld.so.conf && \
		{ echo "ERROR: we shouldn't have a /etc/ld.so.conf file"; exit 1; } || true
	test -d $(TARGET_MINI_DIR)/etc/ld.so.conf.d && \
		{ echo "ERROR: we shouldn't have a /etc/ld.so.conf.d directory"; exit 1; } || true



# HOOKS
define MK_ROOTFS_FSTAB
	$(call MESSAGE,"Generating rootfs fstab")
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/part.py fstab \
		root \
		$(BR2_CONFIG) \
		$(LINUX_DTS_PATH)/partitions.dts \
		$(TARGET_DIR)/etc/fstab.root
endef
ROOTFS_CPIO_PRE_GEN_HOOKS += MK_ROOTFS_FSTAB

endif # BR2_TARGET_ROOTFS_INITRAMFS_MINI
