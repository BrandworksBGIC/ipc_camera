################################################################################
#
# isp_algo
#
################################################################################

ISP_ALGO_SITE_METHOD = local
ISP_ALGO_INSTALL_STAGING = YES
ISP_ALGO_DEPENDENCIES = rtstream
ISP_ALGO_SITE = $(BR2_EXTERNAL)/source/rtscore/isp_algo
ISP_ALGO_CONF_OPTS = \
	-DENABLE_AE_ALGO=$(BR2_PACKAGE_ISP_ALGO_AE) \
	-DENABLE_AWB_ALGO=$(BR2_PACKAGE_ISP_ALGO_AWB) \
	-DENABLE_AF_ALGO=$(BR2_PACKAGE_ISP_ALGO_AF)

$(eval $(cmake-package))
