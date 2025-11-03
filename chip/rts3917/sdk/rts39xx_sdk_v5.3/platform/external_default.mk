include $(BR2_EXTERNAL_platform_PATH)/common.mk

RTSCORE_BIN_TARGET = $(BR2_EXTERNAL)/source/rtscore/rtstream/$(SOC_HW_VER)
REALBOX_BIN_TARGET = $(BR2_EXTERNAL)/source/rtscore/realbox/$(SOC_HW_VER)

include $(sort $(wildcard $(BR2_EXTERNAL_platform_PATH)/package/*/*/bin.mki))

include $(BR2_EXTERNAL_platform_PATH)/package/post_pkg.mk
