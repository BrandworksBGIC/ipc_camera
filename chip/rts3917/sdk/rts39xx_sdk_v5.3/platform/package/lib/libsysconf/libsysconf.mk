################################################################################
#
# libsysconf
#
################################################################################

LIBSYSCONF_SITE_METHOD = local
LIBSYSCONF_DEPENDENCIES = json-c
LIBSYSCONF_INSTALL_STAGING = YES
LIBSYSCONF_SITE = $(BR2_EXTERNAL)/source/lib/libsysconf

LIBSYSCONF_CONF_OPTS = \
	-DCMAKE_INSTALL_PREFIX="/"

ifeq ($(BR2_PACKAGE_LIBSYSCONF_UNIT_TEST),y)
	LIBSYSCONF_CONF_OPTS += -DSUPPORT_UNIT_TEST=y
	LIBSYSCONF_DEPENDENCIES += cunit
endif


$(eval $(cmake-package))
