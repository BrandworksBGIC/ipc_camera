################################################################################
#
# python-tomli
#
################################################################################

PYTHON3_TOMLI_VERSION = 1.2.0
PYTHON3_TOMLI_SOURCE = tomli-$(PYTHON3_TOMLI_VERSION).tar.gz
PYTHON3_TOMLI_SITE = https://files.pythonhosted.org/packages/ec/38/8eccdc662c61aed187d5f5b168c18b1d2de3827976c3691e4da8be7375aa
PYTHON3_TOMLI_SETUP_TYPE = setuptools
PYTHON3_TOMLI_LICENSE = MIT
PYTHON3_TOMLI_LICENSE_FILES = LICENSE
HOST_PYTHON3_TOMLI_NEEDS_HOST_PYTHON = python3

$(eval $(python-package))
$(eval $(host-python-package))
