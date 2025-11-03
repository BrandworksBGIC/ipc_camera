################################################################################
#
# OTP_MFG
#
################################################################################

OTP_MFG_SITE_METHOD = local
OTP_MFG_INSTALL_STAGING = YES

OTP_MFG_SITE = $(BR2_EXTERNAL)/source/system/otp_mfg
OTP_MFG_CONF_OPTS = \
	-DCMAKE_INSTALL_PREFIX="/"


$(eval $(cmake-package))
