#ifndef _RTS_NN_OD_H_
#define _RTS_NN_OD_H_

#include "rts_nn.h"
#include "rts_nn_log.h"

#ifdef _cplusplus
extern "C" {
#endif

/**
 * @file rts_nn_od.h
 * @brief RTSNN Object Detection API Reference
 */

/**
 * @brief realnet od result
 */
struct rts_nn_od_res {
	struct rts_nn_bbox *bboxes; /**< bounding box */
	int cap; /**< capacity of alloced  mem */
	int num; /**< num of detected object bounding box */
};

/**
 * @brief run object detection network
 * @param[in] net nn handle
 * @param[in] image nn image
 * @param[out] res od result object
 * @return 0: success, minus: fail
 */
static inline int rts_nn_od_run(rts_nn_handle net,
		struct rts_nn_image *image, struct rts_nn_od_res **res)
{
	int ret;
	struct rts_nn_objects input = {0};
	struct rts_nn_objects output = {0};

	if (!net || !image || !res)
		return RTS_NN_ERR_INVALID_REFERENCE;

	input.num = 1;
	input.objs[0].type = RTS_NN_OBJ_TYPE_IMAGE;
	input.objs[0].image = image;

	ret = rts_nn_run(net, &input, &output);
	if (ret)
		goto exit;

	if (output.num != 1 && output.objs[0].type != RTS_NN_OBJ_TYPE_RES_OD) {
		ret = RTS_NN_FAILURE;
		RTS_NN_ERR("invalid od result, num: %d, type: %d\n",
				output.num, output.objs[0].type);
		goto exit;
	}

	*res = output.objs[0].data;
exit:
	return ret;
}

#ifdef _cplusplus
}
#endif

#endif /* _RTS_NN_OD_H_ */
