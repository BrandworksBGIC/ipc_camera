################################################################################
#
# libkcapi
#
################################################################################

LIBKCAPI1_VERSION = 1.2.0
LIBKCAPI1_SOURCE = libkcapi-$(LIBKCAPI1_VERSION).tar.xz
LIBKCAPI1_SITE = http://www.chronox.de/libkcapi
LIBKCAPI1_AUTORECONF = YES
LIBKCAPI1_INSTALL_STAGING = YES
LIBKCAPI1_LICENSE = BSD-3-Clause (library), BSD-3-Clause or GPL-2.0 (programs)
LIBKCAPI1_LICENSE_FILES = COPYING COPYING.gplv2 COPYING.bsd

ifeq ($(BR2_PACKAGE_LIBKCAPI1_APPS),y)
LIBKCAPI1_CONF_OPTS += \
	--enable-kcapi-test \
	--enable-kcapi-speed \
	--enable-kcapi-hasher \
	--enable-kcapi-rngapp \
	--enable-kcapi-encapp \
	--enable-kcapi-dgstapp
endif

LIBKCAPI1_CONF_OPTS += --enable-lib-asym
$(eval $(autotools-package))
