################################################################################
#
# 1.iptables
#
################################################################################

IPTABLES_CONF_OPTS += --disable-ipv6 --disable-shared --enable-static

# define IPTABLES_INSTALL_TARGET_CMDS
# 	$(INSTALL) -D -m 0755 $(@D)/iptables/xtables-multi $(TARGET_DIR)/sbin/iptables
# endef
