#ifndef _RTS_NN_FR_RES_H_
#define _RTS_NN_FR_RES_H_

#include "../rts_nn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTS_NN_FR_EMB_DIM 512
#define RTS_NN_FR_MAX_FACE_NUM (32)

/**
 * @breif nn fr embedding
 */
struct rts_nn_fr_emb {
	struct rts_nn_bbox box; /**< bounding box */
	float emb[RTS_NN_FR_EMB_DIM]; /**< face embedding */
};

struct rts_nn_fr_res {
	/** face embeddings */
	struct rts_nn_fr_emb faces[RTS_NN_FR_MAX_FACE_NUM];
	int num; /**< num of faces */
};

RTS_NN_API int rts_nn_fr_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_od_res *od_res, struct rts_nn_fr_res **res);

RTS_NN_API int rts_nn_afr_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_lm_res *lm_res, struct rts_nn_fr_res **res);

#ifdef __cplusplus
}
#endif
#endif
