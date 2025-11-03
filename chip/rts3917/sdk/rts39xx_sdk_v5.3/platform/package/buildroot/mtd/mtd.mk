MTD_CONF_ENV += CFLAGS="$(TARGET_CFLAGS) -fPIC"

ifeq ($(findstring y, $(BR2_CONFIG_FILE_BASED_ENCRYPTION)$(BR2_CONFIG_UBIFS_FS_AUTHENTICATION)),y)
HOST_MTD_CONF_OPTS = \
	--with-jffs \
	--with-ubifs \
	--disable-tests
endif

