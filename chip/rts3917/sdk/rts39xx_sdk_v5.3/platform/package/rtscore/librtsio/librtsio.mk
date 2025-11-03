################################################################################
#
# librtsio
#
################################################################################

LIBRTSIO_SITE_METHOD = local
LIBRTSIO_INSTALL_STAGING = YES
LIBRTSIO_SITE = $(BR2_EXTERNAL)/source/rtscore/librtsio

$(eval $(cmake-package))
