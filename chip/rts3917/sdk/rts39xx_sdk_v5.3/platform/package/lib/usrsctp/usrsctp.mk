#################################################################################
#
## usrsctp
#
#################################################################################

USRSCTP_VERSION = f9f95023816b61a2f257d2fb77658dceaea7213f
USRSCTP_SITE = $(call github,sctplab,usrsctp,$(USRSCTP_VERSION))
USRSCTP_INSTALL_STAGING = YES

$(eval $(cmake-package))
