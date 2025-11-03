################################################################################
#
# python3-ruamel.yaml
#
################################################################################

PYTHON3_RUAMEL_YAML_VERSION = 0.17.21
PYTHON3_RUAMEL_YAML_SOURCE = ruamel.yaml-$(PYTHON3_RUAMEL_YAML_VERSION).tar.gz
PYTHON3_RUAMEL_YAML_SITE = https://files.pythonhosted.org/packages/46/a9/6ed24832095b692a8cecc323230ce2ec3480015fbfa4b79941bd41b23a3c
PYTHON3_RUAMEL_YAML_SETUP_TYPE = setuptools
PYTHON3_RUAMEL_YAML_LICENSE = OSI
PYTHON3_RUAMEL_YAML_LICENSE_FILES = LICENSE

HOST_PYTHON3_RUAMEL_YAML_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
