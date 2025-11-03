################################################################################
#
# python3-packaging
#
################################################################################

PYTHON3_PACKAGING_VERSION = 21.3
PYTHON3_PACKAGING_SOURCE = packaging-$(PYTHON3_PACKAGING_VERSION).tar.gz
PYTHON3_PACKAGING_SITE = https://files.pythonhosted.org/packages/df/9e/d1a7217f69310c1db8fdf8ab396229f55a699ce34a203691794c5d1cad0c
PYTHON3_PACKAGING_SETUP_TYPE = setuptools
PYTHON3_PACKAGING_LICENSE = Apache-2.0 or BSD-2-Clause
PYTHON3_PACKAGING_LICENSE_FILES = LICENSE LICENSE.APACHE LICENSE.BSD
HOST_PYTHON3_PACKAGING_NEEDS_HOST_PYTHON = python3

$(eval $(python-package))
$(eval $(host-python-package))
