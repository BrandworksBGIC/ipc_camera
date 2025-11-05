#ifndef _RTS_NN_BLINK_H_
#define _RTS_NN_BLINK_H_

#ifdef _cplusplus
extern "C" {
#endif

#define RTS_NN_BLINK_MAX_FACE_NUM (32)

struct rts_nn_blink {
	float conf_l;
	float conf_r;
	int is_open;
	int is_blink;
	char list_res[10];
};

struct rts_nn_blink_res {
	struct rts_nn_blink
		confs[RTS_NN_BLINK_MAX_FACE_NUM];

	int num; /**< num of faces */
};

RTS_NN_API int rts_nn_blink_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_lm68_res *lm68_res,
		struct rts_nn_blink_res **res);
#ifdef _cplusplus
}
#endif

#endif /* _RTS_NN_BLINK_H_ */
