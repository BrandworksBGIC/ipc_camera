UPDATE_SRC_DIR = ipc_updater/

IPC_CFLAGS = -DED25519_NO_SEED -DIPC_IS_UBOOT_BUILD -DIPC_ARCH_PLATFORM_$(shell echo $(CONFIG_IPC_ARCH_PLATFORM) | tr a-z A-Z)


IPC_CFLAGS += -I$(UPDATE_SRC_DIR)port_include -I$(UPDATE_SRC_DIR)uboot
IPC_CFLAGS += -Icommon/$(UPDATE_SRC_DIR)port_include -Icommon/$(UPDATE_SRC_DIR)uboot


IPC_OBJS = $(UPDATE_SRC_DIR)uboot/uboot_sd_update.o
IPC_OBJS += $(UPDATE_SRC_DIR)uboot/uboot_auth.o
IPC_OBJS += $(UPDATE_SRC_DIR)uboot/ipc_uboot_port.o
IPC_OBJS += $(UPDATE_SRC_DIR)update_pack_decode.o

IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/add_scalar.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/fe.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/ge.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/key_exchange.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/keypair.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/sc.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/seed.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/sha512.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/sign.o
IPC_OBJS += $(UPDATE_SRC_DIR)/ed25519/src/verify.o


IPC_OBJS += $(UPDATE_SRC_DIR)ff_fat/source/diskio.o
IPC_OBJS += $(UPDATE_SRC_DIR)ff_fat/source/ff.o
IPC_OBJS += $(UPDATE_SRC_DIR)ff_fat/source/ffsystem.o
IPC_OBJS += $(UPDATE_SRC_DIR)ff_fat/source/ffunicode.o

