################################################################################
#
# python3-libfdt
#
################################################################################

PYTHON3_LIBFDT_VERSION = 1.6.1
PYTHON3_LIBFDT_SOURCE = pylibfdt-$(PYTHON3_LIBFDT_VERSION).tar.gz
PYTHON3_LIBFDT_SITE = https://files.pythonhosted.org/packages/15/3c/40b1d6a1df9dbc9d9ba5700a47ad95ca1e984f18daf25ede0da5f67d0cf7
PYTHON3_LIBFDT_SETUP_TYPE = setuptools
PYTHON3_LIBFDT_LICENSE = GPL

HOST_PYTHON3_LIBFDT_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
