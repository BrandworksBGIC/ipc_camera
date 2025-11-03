################################################################################
#
# python3-setuptools_scm
#
################################################################################

PYTHON3_SETUPTOOLS_SCM_VERSION = 6.4.2
PYTHON3_SETUPTOOLS_SCM_SOURCE = setuptools_scm-$(PYTHON3_SETUPTOOLS_SCM_VERSION).tar.gz
PYTHON3_SETUPTOOLS_SCM_SITE = https://files.pythonhosted.org/packages/4a/18/477d3d9eb2f88230ff2a41de9d8ffa3554b706352787d289f57f76bfba0b
PYTHON3_SETUPTOOLS_SCM_SETUP_TYPE = setuptools
PYTHON3_SETUPTOOLS_SCM_LICENSE = MIT
PYTHON3_SETUPTOOLS_SCM_LICENSE_FILES = LICENSE

HOST_PYTHON3_SETUPTOOLS_SCM_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
