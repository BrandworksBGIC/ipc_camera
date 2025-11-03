################################################################################
#
# luaunit
#
################################################################################

LUAUNIT_VERSION = 3.3-1
LUAUNIT_LICENSE = BSD
LUAUNIT_LICENSE_FILES = $(LUAUNIT_DIR)/LICENSE.txt
LUAUNIT_SUBDIR = rock-luaunit-3.3

$(eval $(luarocks-package))
