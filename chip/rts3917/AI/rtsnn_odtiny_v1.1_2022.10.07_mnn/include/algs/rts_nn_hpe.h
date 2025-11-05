#ifndef _RTS_NN_HPE_RES_H_
#define _RTS_NN_HPE_RES_H_

#include "rts_nn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTS_NN_HPE_MAX_FACE_NUM (32)

/**
 * @breif nn face direction info
 */
struct rts_nn_head_pose {
	struct rts_nn_bbox box; /**< face bounding box */
	float yaw; /**< angle yaw */
	float pitch; /**< angle pitch */
	float roll; /**< angle roll */
};

struct rts_nn_hpe_res {
	int num; /**< num of faces */
	/** face pose */
	struct rts_nn_head_pose faces[RTS_NN_HPE_MAX_FACE_NUM];
};

RTS_NN_API int rts_nn_hpe_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_od_res *od_res, struct rts_nn_hpe_res **res);

#ifdef __cplusplus
}
#endif

#endif
