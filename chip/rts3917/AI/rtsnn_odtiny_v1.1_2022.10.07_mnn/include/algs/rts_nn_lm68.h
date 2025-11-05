#ifndef _RTS_NN_LM68_H_
#define _RTS_NN_LM68_H_

#include "../rts_nn.h"

#ifdef _cplusplus
extern "C" {
#endif

#define RTS_NN_LM68_NUM 68
#define RTS_NN_LM68_MAX_FACE_NUM (32)

/**
 * @breif realnet 68 landmarks info
 */
struct rts_nn_lm68_face {
	struct rts_nn_bbox box; /**< bouding box */
	/**68 landmarks */
	struct rts_nn_point_f lm68s[RTS_NN_LM68_NUM];
	/** masks */
	float masks[RTS_NN_LM68_NUM];

	/** angle yaw */
	float yaw;
	/** angle pitch */
	float pitch;
	/** angle roll*/
	float roll;
};

struct rts_nn_lm68_res {
	/** face 68 landmarks info */
	struct rts_nn_lm68_face
		faces[RTS_NN_LM68_MAX_FACE_NUM];
	int num; /**< num of faces */
};
RTS_NN_API int rts_nn_lm68_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_od_res *od_res, struct rts_nn_lm68_res **res);

#ifdef _cplusplus
}
#endif

#endif /* _RTS_NN_LM68_H_ */
