/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2018 Grant Shen <sherry_cheng@realsil.com.cn>
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <rts_isp_sensor.h>
#include "verify.h"

#define VERIFY_MODE VERIFY_CROP_L_LOCATION
#define VERIFY_IMAGE_PATH "/mnt/raw16.bin"

#define SENSOR_WIDTH 3072
#define SENSOR_HEIGHT 2048
#define SENSOR_HTS 3200
#define SENSOR_VTS 2400

#define ISP_CLK 48000000

#define SENSOR_DUMMY_PIXEL (SENSOR_HTS - SENSOR_WIDTH)
#define SENSOR_DUMMY_LINE (SENSOR_VTS - SENSOR_HEIGHT)

/* #define DEBUG */
#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define SUPPORTED_ISP_NUM 1

struct verify_status {
	uint32_t buffer_addr;
	uint32_t buffer_size;
	uint32_t y_len;
	uint32_t uv_len;
};

static struct verify_status g_status[SUPPORTED_ISP_NUM];

static int verify_get_info(uint32_t isp_id,
			   struct rts_isp_sensor_info *info)
{
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	if (VERIFY_MODE == VERIFY_FUSION_IN_LOCATION)
		info->modes.mode[0].hdr = RTS_ISP_HDR_LINE_2TO1;
	else
		info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = SENSOR_WIDTH;
	info->modes.mode[0].size.h = SENSOR_HEIGHT;
	info->modes.mode[0].fps = (float)ISP_CLK / SENSOR_HTS / SENSOR_VTS;
	info->modes.num = 1;

	info->i2c.i2c_id = 0;
	info->i2c.data_len = 1;
	info->i2c.addr_len = 1;

	up->num = 0;
	down->num = 0;

	return RTS_ISP_OK;
}


static int verify_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("verify get fps %.1f init info\n", mode->fps);

	debug("fps: %u, pclk: %u, clk_div: %u, vts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->vts);

	info->interface.interface = SNR_INTERFACE_NONE;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = SENSOR_WIDTH;
	info->size.h = SENSOR_HEIGHT;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = SENSOR_HTS;
	info->pclk = ISP_CLK;
	info->min_vts = SENSOR_VTS;
	info->max_vts = SENSOR_VTS;

	return RTS_ISP_OK;
}


static int verify_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int verify_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int verify_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	return RTS_ISP_OK;
}

static int __alloc_buffer(struct verify_status *status)
{
	if (status->buffer_addr)
		return -RTS_ISP_EBUSY;
	switch (VERIFY_MODE) {
	case VERIFY_CROP_L_LOCATION:
	case VERIFY_CROP_S_LOCATION:
	case VERIFY_YUYV_LOCATION:
		status->buffer_size = SENSOR_WIDTH * SENSOR_HEIGHT * 2;
		status->y_len = status->buffer_size;
		status->uv_len = 0;
		break;
	case VERIFY_FUSION_IN_LOCATION:
	case VERIFY_FUSION_OUT_LOCATION:
	case VERIFY_RAW_BACKEND_LOCATION:
		status->buffer_size = SENSOR_WIDTH * SENSOR_HEIGHT * 4;
		status->y_len = status->buffer_size;
		status->uv_len = 0;
		break;
	case VERIFY_RGB_LOCATION:
	case VERIFY_YUV_LOCATION:
		status->buffer_size = (SENSOR_WIDTH * SENSOR_HEIGHT +
				       2) / 3 * 16;
		status->y_len = status->buffer_size;
		status->uv_len = 0;
		break;
	case VERIFY_NV16_LOCATION:
		status->buffer_size = SENSOR_WIDTH * SENSOR_HEIGHT * 2;
		status->y_len = SENSOR_WIDTH * SENSOR_HEIGHT;
		status->uv_len = SENSOR_WIDTH * SENSOR_HEIGHT;
		break;
	default:
		return -RTS_ISP_EINVAL;
	}
	return isp_driver_mem_alloc(&status->buffer_addr,
				    status->buffer_size, "verify");
}

static int __load_image(struct verify_status *status)
{
	int fd;
	int ret;
	void *virt = NULL;

	fd = open(VERIFY_IMAGE_PATH, O_RDONLY);
	if (fd < 0) {
		ret = errno;
		goto out;
	}
	virt = isp_driver_mmap(status->buffer_addr, status->buffer_size);
	if (!virt) {
		ret = -errno;
		goto out;
	}
	ret = read(fd, virt, status->buffer_size);
	if (ret < 0 || ret != status->buffer_size) {
		ret = -RTS_ISP_ERANGE;
		goto out;
	}
	isp_driver_mem_sync(status->buffer_addr, status->buffer_size,
			    RTS_ISP_SYNC_FOR_DEVICE, RTS_ISP_DMA_TO_DEVICE);
	ret = 0;
out:
	if (virt)
		munmap(virt, status->buffer_size);
	if (fd >= 0)
		close(fd);
	return ret;
}

static int __get_location_fmt(int mode, uint32_t *location, uint32_t *format)
{
	if (!location || !format)
		return -RTS_ISP_EINVAL;
	switch (mode) {
	case VERIFY_CROP_L_LOCATION:
		*location = 0x1;
		*format = 0x1;
		break;
	case VERIFY_CROP_S_LOCATION:
		*location = 0x400;
		*format = 0x1;
		break;
	case VERIFY_FUSION_IN_LOCATION:
		*location = 0x2;
		*format = 0x2;
		break;
	case VERIFY_FUSION_OUT_LOCATION:
		*location = 0x4;
		*format = 0x2;
		break;
	case VERIFY_RAW_BACKEND_LOCATION:
		*location = 0x8;
		*format = 0x2;
		break;
	case VERIFY_RGB_LOCATION:
		*location = 0x20;
		*format = 0x3;
		break;
	case VERIFY_YUV_LOCATION:
		*location = 0x40;
		*format = 0x3;
		break;
	case VERIFY_YUYV_LOCATION:
		*location = 0x80;
		*format = 0x4;
		break;
	case VERIFY_NV16_LOCATION:
		*location = 0x80;
		*format = 0x0;
		break;
	default:
		return -RTS_ISP_EINVAL;
	}
	return RTS_ISP_OK;
}

static int __config_regs(struct verify_status *status)
{
	int ret;
	uint32_t location, format;

	ret = __get_location_fmt(VERIFY_MODE, &location, &format);
	if (ret)
		return ret;
	isp_write_reg(location, VERIFY_SEL);
	isp_write_reg(0x1d, VERIFY_CTRL);
	isp_write_reg(status->buffer_addr, VERIFY_Y_DDR_ADDR0);
	isp_write_reg(status->buffer_addr, VERIFY_Y_DDR_ADDR1);
	isp_write_reg(status->buffer_addr + status->y_len, VERIFY_UV_DDR_ADDR0);
	isp_write_reg(status->buffer_addr + status->y_len, VERIFY_UV_DDR_ADDR1);
	isp_write_reg(status->y_len, VERIFY_Y_DDR_LEN);
	isp_write_reg(status->uv_len, VERIFY_UV_DDR_LEN);
	isp_write_reg(0, VERIFY_FRAME_NUM);
	isp_write_reg(format, VERIFY_FRAME_FORMAT);
	isp_write_reg((SENSOR_WIDTH - 1) | (SENSOR_HEIGHT - 1) << 16,
		      VERIFY_FRAME_SIZE);
	isp_write_reg(0x1010, VERIFY_FRAME_CONFG0);
	isp_write_reg((SENSOR_DUMMY_PIXEL - 1) | (SENSOR_DUMMY_LINE - 1) << 16,
		      VERIFY_FRAME_CONFG1);
	isp_write_reg(0x1, VERIFY_START_FLAG);

	return RTS_ISP_OK;
};

static int verify_start(uint32_t isp_id)
{
	int ret;
	struct verify_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;
	status = &g_status[isp_id];
	ret = __alloc_buffer(status);
	if (ret)
		goto out;
	ret = __load_image(status);
	if (ret)
		goto out;
	ret = __config_regs(status);
	if (ret)
		goto out;
out:
	if (ret && status->buffer_addr) {
		isp_driver_mem_free(status->buffer_addr);
		status->buffer_addr = 0;
	}
	return ret;
}

static int verify_stop(uint32_t isp_id)
{
	int cnt;
	struct verify_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	isp_write_reg(1, VERIFY_STOP_FLAG);
	isp_write_reg(1, VERIFY_INT_FLAG);
	for (cnt = 0; cnt < 20 && isp_read_reg(VERIFY_INT_FLAG) == 0; cnt++)
		usleep(10000);

	if (status->buffer_addr) {
		isp_driver_mem_free(status->buffer_addr);
		status->buffer_addr = 0;
	}
	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_ops verify_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "verify_still",
	.get_info = verify_get_info,
	.get_init_info = verify_get_init_info,
	.get_tuned_again = verify_get_tuned_again,
	.get_tuned_dgain = verify_get_tuned_dgain,
	.get_exposure_gain_info = verify_get_exposure_gain_info,
	.start = verify_start,
	.stop = verify_stop,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(verify_ops)
