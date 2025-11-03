################################################################################
#
# NEURALYZER
#
################################################################################

NEURALYZER_SITE_METHOD = local
NEURALYZER_INSTALL_STAGING = YES
NEURALYZER_SITE = $(BR2_EXTERNAL)/source/system/neuralyzer
NEURALYZER_DEPENDENCIES = mtd

NEURALYZER_CONF_OPTS = \
	-DCMAKE_INSTALL_PREFIX="/"

$(eval $(cmake-package))
