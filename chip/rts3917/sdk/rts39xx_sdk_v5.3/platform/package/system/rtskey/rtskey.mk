################################################################################
#
# rtskey
#
################################################################################
RTSKEY_SITE_METHOD = local
RTSKEY_SITE = $(BR2_EXTERNAL)/scripts/rtskey
RTSKEY_INSTALL_IMAGES = YES
RTSKEY_DEPENDENCIES = host-uboot-tools

RTSKEY_OUT_DIR = $(strip $(patsubst "%",%, $(BR2_RTSKEY_OUT_DIR)))
TARGET_KEYDIR = /etc/keys

CRYPTO_CIPHER = -aes-256-cbc
CRYPTO_KEYSIZE = 32
CRYPTO_KEY = $(strip $(patsubst "%",%, $(BR2_CRYPTO_BOOT_KEY))).bin
CRYPTO_IV = $(strip $(patsubst "%",%, $(BR2_CRYPTO_BOOT_IV))).bin
EFUSE_KEY = 0000000000000000000000000000000000000000000000000000000000000000
CRYPTO_KEYS = $(CRYPTO_KEY)

SECURE_ROOT_KEY = $(strip $(patsubst "%",%, $(BR2_SECURE_ROOT_KEY))).key
SECURE_KERNEL_KEY = $(strip $(patsubst "%",%, $(BR2_SECURE_KERNEL_KEY))).key
SECURE_DATA_KEY = $(strip $(patsubst "%",%, $(BR2_SECURE_DATA_KEY))).key
SECURE_DATA_PUBKEY = $(patsubst %.key,%.der, $(SECURE_DATA_KEY))
SECURE_DATA_CRT = $(patsubst %.key,%.crt, $(SECURE_DATA_KEY))

FBE_KEY = $(strip $(patsubst "%",%, $(BR2_CRYPTO_FBE_KEY))).bin
UBIFS_AUTH_KEY = $(strip $(patsubst "%",%, $(BR2_SECURE_UBIFS_AUTH_KEY))).bin

# build
# generate secure boot key
define SECURE_BOOT_KEY_BUILD_CMDS
	PATH=$(HOST_DIR)/bin:${PATH} $(@D)/rsakey.sh $(RTSKEY_OUT_DIR) \
		$(patsubst %.key,%, $(SECURE_ROOT_KEY)) \
		$(patsubst %.key,%, $(SECURE_KERNEL_KEY)) \
		$(patsubst %.key,%, $(SECURE_DATA_KEY))

	$(Q)echo ">>>   Pre-build dtb for uboot"
	make -C $(O) dtb
	@$(call MESSAGE, "generate public key dtb for u-boot")
	echo > $(@D)/dummy_image
	sed "s%TAG_KERNEL_BIN%$(@D)/dummy_image%g;s%TAG_LOAD_ADDR%0x00000000%g;s%TAG_ENTRY_ADDR%0x00000000%g" $(@D)/linux.its.template > $(@D)/linux.its
	cp $(BASE_DIR)/build/uboot-custom/u-boot.dtb $(@D)/u-boot_pubkey.dtb
	PATH=$(HOST_DIR)/bin:${PATH} $(HOST_DIR)/bin/mkimage -f $(@D)/linux.its -K $(@D)/u-boot_pubkey.dtb -k $(BASE_DIR)/rtskey -r $(@D)/dummy.img;
	$(HOST_DIR)/bin/dtc -I dtb $(@D)/u-boot_pubkey.dtb > $(@D)/u-boot_pubkey.dts

	$(UBIFS_AUTH_KEY_BUILD_CMDS)
endef

# generate crypto boot key
define CRYPTO_BOOT_KEY_BUILD_CMDS
	$(foreach k,$(CRYPTO_KEYS),\
		if [ ! -e $(BASE_DIR)/rtskey/$(k) ];then \
			openssl enc $(CRYPTO_CIPHER) -k secret -P | awk -Fkey= '{printf $$2}' | xxd -r -ps > $(RTSKEY_OUT_DIR)/$(k);\
			$(call MESSAGE, "generate $(k)");\
		else\
			$(call MESSAGE, "use previous generated $(k)");\
		fi\
		$(sep))

	if [ $(CRYPTO_IV) ]; then \
		if [ ! -e $(RTSKEY_OUT_DIR)/$(CRYPTO_IV) ];then \
			openssl enc $(CRYPTO_CIPHER) -k secret -P | awk -F'iv =' '{printf $$2}' | xxd -r -ps > $(RTSKEY_OUT_DIR)/$(CRYPTO_IV);\
			$(call MESSAGE, "generate $(CRYPTO_IV)");\
		else\
			$(call MESSAGE, "use previous generated $(CRYPTO_IV)");\
		fi \
	fi

	$(FBE_KEY_BUILD_CMDS)
endef # CRYPTO_BOOT_KEY_BUILD_CMDS

ifeq ($(BR2_CONFIG_FILE_BASED_ENCRYPTION),y)
define FBE_KEY_BUILD_CMDS
	if [ ! -e $(RTSKEY_OUT_DIR)/$(FBE_KEY) ];then \
		dd if=/dev/urandom of=$(RTSKEY_OUT_DIR)/$(FBE_KEY) count=64 bs=1; \
		$(call MESSAGE, "generate $(FBE_KEY)");\
	else\
		$(call MESSAGE, "use previous generated $(FBE_KEY)");\
	fi
endef
endif

ifeq ($(BR2_CONFIG_UBIFS_FS_AUTHENTICATION),y)
define UBIFS_AUTH_KEY_BUILD_CMDS
	if [ ! -e $(RTSKEY_OUT_DIR)/$(UBIFS_AUTH_KEY) ];then \
		dd if=/dev/urandom of=$(RTSKEY_OUT_DIR)/$(UBIFS_AUTH_KEY) count=32 bs=1; \
		$(call MESSAGE, "generate $(UBIFS_AUTH_KEY)");\
	else\
		$(call MESSAGE, "use previous generated $(UBIFS_AUTH_KEY)");\
	fi
endef
endif

define RTSKEY_BUILD_CMDS
	[ -d ${RTSKEY_OUT_DIR} ] || mkdir ${RTSKEY_OUT_DIR}
	$(if $(BR2_CONFIG_SECURE_BOOT),$(SECURE_BOOT_KEY_BUILD_CMDS))
	$(if $(BR2_CONFIG_CRYPTO_BOOT),$(CRYPTO_BOOT_KEY_BUILD_CMDS))
endef


# install target
# secure boot
ifeq ($(BR2_CONFIG_SECURE_DM_MAPPER),y)
ifeq ($(BR2_CONFIG_SECURE_BOOT_USER_PARTITION)$(BR2_FW_PACK_USERDATA_EXT4),yy)
define SECURE_BOOT_KEY_INSTALL_TARGET_CMDS
	openssl rsa -in $(RTSKEY_OUT_DIR)/$(SECURE_DATA_KEY) -pubout -RSAPublicKey_out -outform DER > $(@D)/$(SECURE_DATA_PUBKEY)
	$(INSTALL) -D -m 644 $(@D)/$(SECURE_DATA_PUBKEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(SECURE_DATA_PUBKEY)
endef
endif

define SECURE_BOOT_KEY_INSTALL_TARGET_MINI_CMDS
	openssl rsa -in $(RTSKEY_OUT_DIR)/$(SECURE_DATA_KEY) -pubout -RSAPublicKey_out -outform DER > $(@D)/$(SECURE_DATA_PUBKEY)
	$(INSTALL) -D -m 644 $(@D)/$(SECURE_DATA_PUBKEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(SECURE_DATA_PUBKEY)
endef
endif

ifeq ($(BR2_CONFIG_UBIFS_FS_AUTHENTICATION),y)
define SECURE_BOOT_KEY_INSTALL_TARGET_CMDS
endef

define SECURE_BOOT_KEY_INSTALL_TARGET_MINI_CMDS
	$(INSTALL) -D -m 644 $(RTSKEY_OUT_DIR)/$(UBIFS_AUTH_KEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(UBIFS_AUTH_KEY)
endef
endif


# crypto boot
ifeq ($(BR2_CONFIG_FULL_DISK_ENCRYPTION),y)
ifeq ($(BR2_CONFIG_CRYPTO_BOOT_USER_PARTITION)$(BR2_FW_PACK_USERDATA_EXT4),yy)
define CRYPTO_BOOT_KEY_INSTALL_TARGET_CMDS
	echo -n $(EFUSE_KEY) | xxd -r -ps > $(@D)/$(CRYPTO_KEY)
	$(INSTALL) -D -m 644 $(@D)/$(CRYPTO_KEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(CRYPTO_KEY)
endef
endif

define CRYPTO_BOOT_KEY_INSTALL_TARGET_MINI_CMDS
	echo -n $(EFUSE_KEY) | xxd -r -ps > $(@D)/$(CRYPTO_KEY)
	$(INSTALL) -D -m 644 $(@D)/$(CRYPTO_KEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(CRYPTO_KEY)
endef
endif

ifeq ($(BR2_CONFIG_FILE_BASED_ENCRYPTION),y)
define CRYPTO_BOOT_KEY_INSTALL_TARGET_CMDS
endef

define CRYPTO_BOOT_KEY_INSTALL_TARGET_MINI_CMDS
	$(INSTALL) -D -m 644 $(RTSKEY_OUT_DIR)/$(FBE_KEY) $(TARGET_DIR)/$(TARGET_KEYDIR)/$(FBE_KEY)
endef
endif

# install image
define SECURE_BOOT_KEY_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 644 $(@D)/u-boot_pubkey.dtb $(BINARIES_DIR)
endef


# install
define RTSKEY_INSTALL_TARGET_CMDS
	$(if $(BR2_CONFIG_SECURE_BOOT),$(SECURE_BOOT_KEY_INSTALL_TARGET_CMDS))
	$(if $(BR2_CONFIG_CRYPTO_BOOT),$(CRYPTO_BOOT_KEY_INSTALL_TARGET_CMDS))
endef

define RTSKEY_INSTALL_TARGET_MINI_CMDS
	$(if $(BR2_CONFIG_SECURE_BOOT),$(SECURE_BOOT_KEY_INSTALL_TARGET_MINI_CMDS))
	$(if $(BR2_CONFIG_CRYPTO_BOOT),$(CRYPTO_BOOT_KEY_INSTALL_TARGET_MINI_CMDS))
endef

define RTSKEY_INSTALL_IMAGES_CMDS
	$(if $(BR2_CONFIG_SECURE_BOOT),$(SECURE_BOOT_KEY_INSTALL_IMAGES_CMDS))
endef

$(eval $(generic-package))
