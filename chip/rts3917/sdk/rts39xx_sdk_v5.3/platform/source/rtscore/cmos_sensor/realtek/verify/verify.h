#ifndef _VERIFY_H_INC_
#define _VERIFY_H_INC_

#include <stdint.h>
#include <rts_camera_isp_info.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define VERIFY_REG_BASE 0x6800
#define VERIFY_SEL (VERIFY_REG_BASE + 0x0000)
#define VERIFY_CTRL (VERIFY_REG_BASE + 0x0004)
#define VERIFY_Y_DDR_ADDR0 (VERIFY_REG_BASE + 0x0008)
#define VERIFY_Y_DDR_ADDR1 (VERIFY_REG_BASE + 0x000C)
#define VERIFY_UV_DDR_ADDR0 (VERIFY_REG_BASE + 0x0010)
#define VERIFY_UV_DDR_ADDR1 (VERIFY_REG_BASE + 0x0014)
#define VERIFY_DDR_ADDR_SEL (VERIFY_REG_BASE + 0x0018)
#define VERIFY_Y_DDR_LEN (VERIFY_REG_BASE + 0x001C)
#define VERIFY_UV_DDR_LEN (VERIFY_REG_BASE + 0x0020)
#define VERIFY_FRAME_NUM (VERIFY_REG_BASE + 0x0024)
#define VERIFY_FRAME_FORMAT (VERIFY_REG_BASE + 0x0028)
#define VERIFY_FRAME_SIZE (VERIFY_REG_BASE + 0x002C)
#define VERIFY_FRAME_CONFG0 (VERIFY_REG_BASE + 0x0030)
#define VERIFY_FRAME_CONFG1 (VERIFY_REG_BASE + 0x0034)
#define VERIFY_INT_EN (VERIFY_REG_BASE + 0x0038)
#define VERIFY_INT_FLAG (VERIFY_REG_BASE + 0x003C)
#define VERIFY_START_FLAG (VERIFY_REG_BASE + 0x0040)
#define VERIFY_STOP_FLAG (VERIFY_REG_BASE + 0x0044)

enum verify_mode {
	VERIFY_CROP_L_LOCATION,
	VERIFY_CROP_S_LOCATION,
	VERIFY_FUSION_IN_LOCATION,
	VERIFY_FUSION_OUT_LOCATION,
	VERIFY_RAW_BACKEND_LOCATION,
	VERIFY_RGB_LOCATION,
	VERIFY_YUV_LOCATION,
	VERIFY_YUYV_LOCATION,

	VERIFY_NV16_LOCATION,
};

int isp_driver_mem_alloc(uint32_t *phy_addr, uint32_t length, const char *info);
int isp_driver_mem_free(uint32_t phy_addr);
void *isp_driver_mmap(uint32_t start, uint32_t size);
int isp_driver_mem_sync(uint32_t phy_addr, uint32_t size,
			enum rts_isp_mem_sync_type sync_type,
			enum rts_isp_mem_dma_dir dma_dir);

extern void *isp_io_base;

static inline uint32_t isp_read_reg(uint32_t offset)
{
	uint32_t value;

	value = *(volatile uint32_t *)(isp_io_base + offset);
	__sync_synchronize();
	return value;
}

static inline void isp_write_reg(uint32_t value, uint32_t offset)
{
	__sync_synchronize();
	*(volatile uint32_t *)(isp_io_base + offset) = value;
}

#endif /* _VERIFY_H_INC_ */

