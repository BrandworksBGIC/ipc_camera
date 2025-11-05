#ifndef _RTS_NN_REID_H_
#define _RTS_NN_REID_H_

#include "rts_nn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTS_NN_REID_MAX_NUM (32)

/**
 * @breif realnet reid object, assign pid for each people
 */
struct rts_nn_reid_object {
	struct rts_nn_bbox box; /**< bounding box */
	uint64_t pid; /**< people id */
	float match_prob; /**< matched probability */
};


struct rts_nn_reid_res {
	/** reid embeddings */
	struct rts_nn_reid_object objs[RTS_NN_REID_MAX_NUM];
	int num; /**< num of person */
};

RTS_NN_API int rts_nn_reid_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_od_res *od_res, struct rts_nn_reid_res **res);

#ifdef __cplusplus
}
#endif

#endif
