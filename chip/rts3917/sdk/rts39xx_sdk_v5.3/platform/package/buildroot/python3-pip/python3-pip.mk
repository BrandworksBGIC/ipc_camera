################################################################################
#
# python3-pip
#
################################################################################

PYTHON3_PIP_VERSION = 22.0.4
PYTHON3_PIP_SOURCE = pip-$(PYTHON3_PIP_VERSION).tar.gz
PYTHON3_PIP_SITE = https://files.pythonhosted.org/packages/33/c9/e2164122d365d8f823213a53970fa3005eb16218edcfc56ca24cb6deba2b
PYTHON3_PIP_SETUP_TYPE = setuptools
PYTHON3_PIP_LICENSE = MIT
PYTHON3_PIP_LICENSE_FILES = LICENSE.txt

HOST_PYTHON3_PIP_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
