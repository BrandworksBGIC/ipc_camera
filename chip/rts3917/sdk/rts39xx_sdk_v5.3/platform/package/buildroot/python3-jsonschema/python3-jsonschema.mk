################################################################################
#
# python3-jsonschema
#
################################################################################

PYTHON3_JSONSCHEMA_VERSION = 4.4.0
PYTHON3_JSONSCHEMA_SOURCE = jsonschema-$(PYTHON3_JSONSCHEMA_VERSION).tar.gz
PYTHON3_JSONSCHEMA_SITE = https://files.pythonhosted.org/packages/26/67/36cfd516f7b3560bbf7183d7a0f82bb9514d2a5f4e1d682a8a1d55d8031d
PYTHON3_JSONSCHEMA_SETUP_TYPE = setuptools
PYTHON3_JSONSCHEMA_LICENSE = MIT
PYTHON3_JSONSCHEMA_LICENSE_FILES = COPYING json/LICENSE
PYTHON3_JSONSCHEMA_DEPENDENCIES = host-python-vcversioner

HOST_PYTHON3_JSONSCHEMA_NEEDS_HOST_PYTHON = python3

$(eval $(python-package))
$(eval $(host-python-package))