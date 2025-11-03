################################################################################
#
# python3-six
#
################################################################################

PYTHON3_SIX_VERSION = 1.14.0
PYTHON3_SIX_SOURCE = six-$(PYTHON3_SIX_VERSION).tar.gz
PYTHON3_SIX_SITE = https://files.pythonhosted.org/packages/21/9f/b251f7f8a76dec1d6651be194dfba8fb8d7781d10ab3987190de8391d08e
PYTHON3_SIX_SETUP_TYPE = setuptools
PYTHON3_SIX_LICENSE = MIT
PYTHON3_SIX_LICENSE_FILES = LICENSE

HOST_PYTHON3_SIX_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
