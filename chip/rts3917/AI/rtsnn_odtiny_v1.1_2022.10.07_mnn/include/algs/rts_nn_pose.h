#ifndef _RTS_NN_POSE_RES_H_
#define _RTS_NN_POSE_RES_H_

#include "rts_nn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @breif nn pose key points
 */
struct rts_nn_pose_points {
	int num;
	struct rts_nn_point *points;
};

/**
 * @breif nn pose skeletons
 */
struct rts_nn_pose_lines {
	int num;
	struct rts_nn_line *lines;
};

/**
 * @breif nn pose info
 */
struct rts_nn_pose {
	struct rts_nn_bbox bbox;

	struct rts_nn_pose_points keypoints;
	struct rts_nn_pose_lines skeletons;
};

/**
 * @breif nn poses info
 */
struct rts_nn_pose_res {
	int num; /**< num of persons */
	int cap; /**< capacity of alloced mem */
	struct rts_nn_pose *poses; /**< pose result */
};

RTS_NN_API int rts_nn_pose_run(rts_nn_handle net, struct rts_nn_image *image,
		struct rts_nn_pose_res **res);

#ifdef __cplusplus
}
#endif

#endif
