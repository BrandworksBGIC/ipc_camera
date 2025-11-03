################################################################################
#
# python-protobuf
#
################################################################################

HOST_PYTHON_PROTOBUF_DEPENDENCIES = host-protobuf host-python3-six
HOST_PYTHON_PROTOBUF_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
