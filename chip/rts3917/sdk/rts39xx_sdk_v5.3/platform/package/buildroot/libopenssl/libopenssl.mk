################################################################################
#
# LIBOPENSSL
#
################################################################################

LIBOPENSSL_INSTALL_STAGING = YES
LIBOPENSSL_CFLAGS = $(TARGET_CFLAGS)
LIBOPENSSL_TARGET_ARCH = generic32

define HOST_LIBOPENSSL_CONFIGURE_CMDS
	(cd $(@D); \
		$(HOST_CONFIGURE_OPTS) \
		./config \
		--prefix=$(HOST_DIR) \
		--openssldir=$(HOST_DIR)/etc/ssl \
		no-tests \
		no-fuzz-libfuzzer \
		no-fuzz-afl \
		shared \
		zlib-dynamic \
		-DOPENSSL_NO_HW_RTS \
	)
	$(SED) "s#-O[0-9]#$(HOST_CFLAGS)#" $(@D)/Makefile
endef

define LIBOPENSSL_CONFIGURE_CMDS
	(cd $(@D); \
		$(TARGET_CONFIGURE_ARGS) \
		$(TARGET_CONFIGURE_OPTS) \
		./Configure \
			linux-$(LIBOPENSSL_TARGET_ARCH) \
			--prefix=/usr \
			--openssldir=/etc/ssl \
			--libdir=lib \
			$(if $(BR2_TOOLCHAIN_HAS_THREADS),threads,no-threads) \
			$(if $(BR2_STATIC_LIBS),no-shared,shared) \
			no-rc5 \
			enable-camellia \
			enable-mdc2 \
			$(if $(BR2_STATIC_LIBS),no-dso) \
			$(if $(BR2_PACKAGE_LIBOPENSSL_RTSENGINE),,-DOPENSSL_NO_HW_RTS) \
	)
	$(SED) "s#-march=[-a-z0-9] ##" -e "s#-mcpu=[-a-z0-9] ##g" $(@D)/Makefile
	$(SED) "s#-O[0-9]#$(LIBOPENSSL_CFLAGS)#" $(@D)/Makefile
	$(SED) "s# build_tests##" $(@D)/Makefile
endef

define LIBOPENSSL_BUILD_CMDS
	rm -rf $(@D)/crypto/engine/eng_rts.o
	$(TARGET_MAKE_ENV) $(MAKE1) -C $(@D)
endef

define LIBOPENSSL_INSTALL_STAGING_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install_sw
endef

define LIBOPENSSL_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) DESTDIR=$(TARGET_DIR) install_sw
	rm -rf $(TARGET_DIR)/usr/lib/ssl
	rm -f $(TARGET_DIR)/usr/bin/c_rehash
endef

LIBOPENSSL_POST_INSTALL_TARGET_HOOKS = 

ifneq ($(BR2_STATIC_LIBS),y)
# libraries gets installed read only, so strip fails
define LIBOPENSSL_INSTALL_FIXUPS_SHARED
	chmod +w $(TARGET_DIR)/usr/lib/engines-3/*.so
	for i in $(addprefix $(TARGET_DIR)/usr/lib/,libcrypto.so.* libssl.so.*); \
	do chmod +w $$i; done
endef
LIBOPENSSL_POST_INSTALL_TARGET_HOOKS += LIBOPENSSL_INSTALL_FIXUPS_SHARED
endif

define LIBOPENSSL_REMOVE_STATIC_LIBS
	for i in $(addprefix $(TARGET_DIR)/usr/lib/,libcrypto.a libssl.a); \
	do rm -f $$i; done
endef
LIBOPENSSL_POST_INSTALL_TARGET_HOOKS += LIBOPENSSL_REMOVE_STATIC_LIBS

ifeq ($(BR2_PACKAGE_LIBOPENSSL_BIN),)
define LIBOPENSSL_REMOVE_BIN
	$(RM) -f $(TARGET_DIR)/usr/bin/openssl
endef
LIBOPENSSL_POST_INSTALL_TARGET_HOOKS += LIBOPENSSL_REMOVE_BIN
endif

# $(eval $(generic-package))
