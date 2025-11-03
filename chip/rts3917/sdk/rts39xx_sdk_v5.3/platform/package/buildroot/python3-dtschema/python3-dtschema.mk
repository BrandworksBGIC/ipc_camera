################################################################################
#
# python3-dtschema
#
################################################################################

PYTHON3_DTSCHEMA_VERSION = 2022.3.1
PYTHON3_DTSCHEMA_SOURCE = dtschema-$(PYTHON3_DTSCHEMA_VERSION).tar.gz
PYTHON3_DTSCHEMA_SITE = https://files.pythonhosted.org/packages/c8/fe/d2d0a4fab04e456aedcac0962a0606e6fd649dcc1312d6781531b7cb64b6
PYTHON3_DTSCHEMA_SETUP_TYPE = setuptools
PYTHON3_DTSCHEMA_LICENSE = BSD-2-Clause
PYTHON3_DTSCHEMA_LICENSE_FILES = LICENSE.txt

HOST_PYTHON3_DTSCHEMA_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
