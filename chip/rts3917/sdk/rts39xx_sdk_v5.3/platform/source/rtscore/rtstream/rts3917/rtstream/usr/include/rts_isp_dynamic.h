#ifndef _RTS_ISP_DYNAMIC_H_INC_
#define _RTS_ISP_DYNAMIC_H_INC_

#include <stdint.h>
#include <rts_isp_define.h>

#ifdef __cplusplus
extern "C" {
#endif

struct isp_notify_dynamic_ae {
	enum rts_isp_hdr_mode mode;
	int num; /* exposure gain channel num */

	float exposure[RTS_ISP_HDR_CHAN_MAX]; /* us */
	float analog_gain[RTS_ISP_HDR_CHAN_MAX];
	float digital_gain[RTS_ISP_HDR_CHAN_MAX];
	float isp_hdr_gain[RTS_ISP_HDR_CHAN_MAX];
	float isp_gain;

	float sensor_gain[RTS_ISP_HDR_CHAN_MAX];
	float total_gain[RTS_ISP_HDR_CHAN_MAX];

	uint32_t iq_total_gain[RTS_ISP_HDR_CHAN_MAX]; /* total_gain * 16 */
	uint32_t iq_exp_gain[RTS_ISP_HDR_CHAN_MAX]; /* exposure * total_gain / 100 */
	uint32_t iq_ratio[RTS_ISP_HDR_CHAN_MAX]; /* exposure * total gain ratio */
	uint32_t iq_sensor_gain[RTS_ISP_HDR_CHAN_MAX]; /* sensor_gain * 16 */

	uint32_t contrast; /* contrast value based histogram*/
	int32_t iq_bv; /* bv * 16 */
};

struct isp_notify_dynamic_awb {
	uint32_t color_temp;
	float r_gain;
	float g_gain;
	float b_gain;
};

struct isp_notify_dynamic_sensor {
	int high_temp_en;
	uint32_t temperature;
};

struct isp_notify_dynamic {
	struct isp_notify_dynamic *pre;

	struct isp_notify_dynamic_ae ae;
	struct isp_notify_dynamic_awb awb;
	struct isp_notify_dynamic_sensor sensor;
};

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_DYNAMIC_H_INC_ */

