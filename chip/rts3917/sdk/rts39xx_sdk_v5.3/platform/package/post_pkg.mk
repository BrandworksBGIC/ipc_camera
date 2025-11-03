# target-mini-package -- Install the specified packages and recursive
# dependencies to the target-mini dir
#
#  argument 1 is the lowercase package name
#  argument 2 is the uppercase package name

define target-mini-package

$(2)_TARGET_INSTALL_TARGET += $$($(2)_DIR)/.stamp_target_mini_installed

ifeq ($$($(2)_INSTALL_TARGET),YES)
$$($(2)_TARGET_INSTALL): $$($(2)_TARGET_INSTALL_TARGET)
$(1)-install-target: $$($(2)_TARGET_INSTALL_TARGET)
$$($(2)_TARGET_INSTALL_TARGET):	$$($(2)_TARGET_BUILD)
endif

ifndef $(2)_INSTALL_TARGET_MINI_CMDS
define $(2)_INSTALL_TARGET_MINI_CMDS
$$(call $(2)_INSTALL_TARGET_CMDS)
endef
endif

# Install to target-mini dir
$$($(2)_DIR)/.stamp_target_mini_installed: TARGET_DIR=$$(TARGET_MINI_DIR)
$$($(2)_DIR)/.stamp_target_mini_installed:
	@$$(call step_start,install-target)
	@$$(call MESSAGE,"Installing to target-mini")
	$$(foreach hook,$$($$(PKG)_PRE_INSTALL_TARGET_MINI_HOOKS),$$(call $$(hook))$$(sep))
	+$$($$(PKG)_INSTALL_TARGET_MINI_CMDS)
	$$(if $$(BR2_INIT_SYSTEMD),\
		$$($$(PKG)_INSTALL_INIT_SYSTEMD))
	$$(if $$(BR2_INIT_SYSV)$$(BR2_INIT_BUSYBOX),\
		$$($$(PKG)_INSTALL_INIT_SYSV))
	$$(if $$(BR2_INIT_OPENRC), \
		$$(or $$($$(PKG)_INSTALL_INIT_OPENRC), \
			$$($$(PKG)_INSTALL_INIT_SYSV)))
	$$(foreach hook,$$($$(PKG)_POST_INSTALL_TARGET_MINI_HOOKS),$$(call $$(hook))$$(sep))
	$$(Q)if test -n "$$($$(PKG)_CONFIG_SCRIPTS)" ; then \
		$$(RM) -f $$(addprefix $$(TARGET_DIR)/usr/bin/,$$($$(PKG)_CONFIG_SCRIPTS)) ; \
	fi
	@$$(call step_end,install-target)
	$$(Q)touch $$@

endef

INITRAMFS_MINI_FINAL_INSTALL = $(sort $(INITRAMFS_MINI_INSTALL) \
	$(foreach pkg,$(call UPPERCASE,$(INITRAMFS_MINI_INSTALL)),\
		$(foreach dep,$($(pkg)_FINAL_RECURSIVE_DEPENDENCIES),\
			$(dep)\
		)\
	))

INITRAMFS_MINI_FINAL_INSTALL += $(INITRAMFS_MINI_INSTALL_NDEPEND)

$(foreach pkg,$(INITRAMFS_MINI_FINAL_INSTALL),\
	$(eval\
		$(call target-mini-package,$(pkg),$(call UPPERCASE,$(pkg)))\
	$(sep)\
	)\
)
