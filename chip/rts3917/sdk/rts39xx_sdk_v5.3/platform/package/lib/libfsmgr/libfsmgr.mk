################################################################################
#
# libfsmgr
#
################################################################################
LIBFSMGR_SITE_METHOD = local
LIBFSMGR_SITE = $(BR2_EXTERNAL)/source/lib/libfsmgr
LIBFSMGR_INSTALL_STAGING = YES
LIBFSMGR_DEPENDENCIES = libkcapi1


$(eval $(cmake-package))
