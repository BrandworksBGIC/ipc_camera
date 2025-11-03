ifeq ($(BR2_CONFIG_HW_VER_RTS3903), y)
SOC_HW_VER = rts3903
endif

ifeq ($(BR2_CONFIG_HW_VER_RTS3913), y)
SOC_HW_VER = rts3913
endif

ifeq ($(BR2_CONFIG_HW_VER_RTS3915), y)
SOC_HW_VER = rts3915
endif

ifeq ($(BR2_CONFIG_HW_VER_RTS3917), y)
SOC_HW_VER = rts3917
endif

IPCAM_BIN_TARGET = $(BR2_EXTERNAL)/source/ipcam/ipcam.bin/$(SOC_HW_VER)
NETWORK_BIN_TARGET = $(BR2_EXTERNAL)/source/network/network.bin/$(SOC_HW_VER)
USB_BIN_TARGET = $(BR2_EXTERNAL)/source/usb/usb.bin/$(SOC_HW_VER)
ISPTUNING_BIN_TARGET = $(BR2_EXTERNAL)/source/debug/isp_tuning/isp_tuning.bin
ATF_BIN_TARGET = $(BR2_EXTERNAL)/source/bootloader/atf-2.3/atf.bin

include $(sort $(wildcard $(BR2_EXTERNAL_platform_PATH)/package/*/*/*.mk))
include $(sort $(wildcard $(BR2_EXTERNAL_platform_PATH)/package/*/*.mk))
EXTERNAL_ONLY_PACKAGES = $(notdir $(patsubst %.mk,%,$(wildcard $(BR2_EXTERNAL_platform_PATH)/package/*/*/*.mk)))
EXTERNAL_ONLY_PACKAGES += $(notdir $(patsubst %.mk,%,$(wildcard $(BR2_EXTERNAL_platform_PATH)/package/*/*.mk)))
