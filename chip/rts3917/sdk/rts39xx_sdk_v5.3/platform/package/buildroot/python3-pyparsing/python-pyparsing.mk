################################################################################
#
# python3-pyparsing
#
################################################################################

PYTHON3_PYPARSING_VERSION = 3.0.7
PYTHON3_PYPARSING_SOURCE = pyparsing-$(PYTHON3_PYPARSING_VERSION).tar.gz
PYTHON3_PYPARSING_SITE = https://files.pythonhosted.org/packages/d6/60/9bed18f43275b34198eb9720d4c1238c68b3755620d20df0afd89424d32b
PYTHON3_PYPARSING_LICENSE = MIT
PYTHON3_PYPARSING_LICENSE_FILES = LICENSE
PYTHON3_PYPARSING_SETUP_TYPE = setuptools
HOST_PYTHON3_PYPARSING_NEEDS_HOST_PYTHON = python3

$(eval $(python-package))
$(eval $(host-python-package))