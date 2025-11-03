################################################################################
#
# python3-pydevicetree
#
################################################################################

PYTHON3_PYDEVICETREE_VERSION = 0.0.12
PYTHON3_PYDEVICETREE_SITE_METHOD = local
#PYTHON3_PYDEVICETREE_SITE = $(CURDIR)/package/python3-pydevicetree/pydevicetree
PYTHON3_PYDEVICETREE_SITE = $(BR2_EXTERNAL_platform_PATH)/package/buildroot/python3-pydevicetree/pydevicetree
PYTHON3_PYDEVICETREE_SETUP_TYPE = setuptools
PYTHON3_PYDEVICETREE_LICENSE = Apache-2.0
PYTHON3_PYDEVICETREE_LICENSE_FILES = COPYING.txt LICENSE.txt
HOST_PYTHON3_PYDEVICETREE_DEPENDENCIES += host-python3-pyparsing
HOST_PYTHON3_PYDEVICETREE_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
