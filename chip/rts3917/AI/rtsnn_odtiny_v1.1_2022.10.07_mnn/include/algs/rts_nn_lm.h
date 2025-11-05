#ifndef _RTS_NN_LM_RES_H_
#define _RTS_NN_LM_RES_H_

#include "rts_nn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTS_NN_LM_MAX_FACE_NUM (32)
#define RTS_NN_LM_POINT_NUM 5

/**
 * @brief nn lm info
 */
struct rts_nn_lm_face {
	struct rts_nn_bbox box; /**< bouding box */
	/** landmark points */
	struct rts_nn_point_f points[RTS_NN_LM_POINT_NUM];
	/** masks */
	float masks[RTS_NN_LM_POINT_NUM];

	/** angle yaw */
	float yaw;
	/** angle pitch */
	float pitch;
	/** angle roll*/
	float roll;
};

/**
 * @brief result of face landmark
 */
struct rts_nn_lm_res {
	/** face landmark info */
	struct rts_nn_lm_face
		faces[RTS_NN_LM_MAX_FACE_NUM];
	int num; /**< num of faces */
};

RTS_NN_API int rts_nn_lm_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_od_res *od_res, struct rts_nn_lm_res **res);

#ifdef __cplusplus
}
#endif
#endif
