#ifndef _RTS_NN_OD_H_
#define _RTS_NN_OD_H_

#include "rts_nn.h"

#ifdef _cplusplus
extern "C" {
#endif

/**
 * @file rts_nn_od.h
 * @brief REALNET Object Detection API reference
 */

/**
 * @brief realnet od result
 */
struct rts_nn_od_res {
	struct rts_nn_bbox *bboxes; /**< bounding box */
	int cap; /**< capacity of alloced  mem */
	int num; /**< num of detected object bounding box */
};

RTS_NN_API int rts_nn_od_run(rts_nn_handle net,
		struct rts_nn_image *image, struct rts_nn_od_res **res);

#ifdef _cplusplus
}
#endif

#endif /* _RTS_NN_OD_H_ */
