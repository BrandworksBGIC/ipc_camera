#ifndef _RTS_NN_PUB_H_
#define _RTS_NN_PUB_H_

#include <stdint.h>

#ifndef RTS_NN_API
#if defined(_WIN32)
#define RTS_NN_API __declspec(dllexport)
#else
#define RTS_NN_API __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rts_nn_types.h
 * @brief rts nn defined types
 */

#define RTS_NN_MAX_OBJECTS 16
#define RTS_NN_MAX_CLASSES 128
#define RTS_NN_MAX_LABEL_LEN 16

/**
 * @brief Quantization type of network
 */
enum RTS_NN_DTYPE {
	RTS_NN_INT8 =		0, /**< int8 type */
	RTS_NN_UINT8 =		1, /**< uint8 type */
	RTS_NN_INT16 =		2, /**< int16 type */
	RTS_NN_UINT16 =		3, /**< uint16 type */
	RTS_NN_INT32 =		4, /**< int32 type */
	RTS_NN_UINT32 =		5, /**< uint32 type */
	RTS_NN_INT64 =		6, /**< int64 type */
	RTS_NN_UINT64 =		7, /**< uint64 type */
	RTS_NN_FLOAT16 =	8, /**< float16 type */
	RTS_NN_FLOAT32 =	9,  /**< float32 type */
	RTS_NN_FLOAT64 =	10,  /**< float64(double) type */
};

/**
 * @brief rts nn defined level
 */
enum RTS_NN_LEVEL {
	RTS_NN_CUSTOMIZE = -1, /**< customize mode */
	RTS_NN_LOW = 0, /**< low level */
	RTS_NN_MEDIUM = 1, /**< medium level */
	RTS_NN_HIGH = 2, /**< high level */
};

/**
 * @brief rotation type of image
 */
enum RTS_NN_ROTATE {
	RTS_NN_ROTATE_0 =	0, /**< do not rotate */
	RTS_NN_ROTATE_CW_90 =	1, /**< rotate 90 degrees clockwise */
	RTS_NN_ROTATE_CW_180 =	2, /**< rotate 180 degrees clockwise */
	RTS_NN_ROTATE_CW_270 =	3 /**< rotate 270 degrees clockwise */
};

/**
 * @brief rtsnn image format
 */
enum RTS_NN_IMG_FMT {
	/* RGB formats family */
	RTS_NN_RGB_PACKED =	1000,		/**< RGB888 packed format */
	RTS_NN_BGR_PACKED =	1001,		/**< BGR888 packed format */
	RTS_NN_RGB_PLANAR =	1002,		/**< RGB888 planar format */
	RTS_NN_BGR_PLANAR =	1003,		/**< BGR888 planar format */
	RTS_NN_RGBA_PACKED =	1004,		/**< RGBA packed format */
	RTS_NN_BGRA_PACKED =	1005,		/**< BGRAA packed format */
	RTS_NN_RGBA_PLANAR =	1006,		/**< RGBA planar format */
	RTS_NN_BGRA_PLANAR =	1007,		/**< BGRA planar format */

	/* YUV 4:4:4 formats family */
	RTS_NN_YUV444_PACKED =	2000,	/**< YUVYUVYUVYUVYUVYUVYUVYUV */
	RTS_NN_YUV444_PLANAR =	2001,	/**< YYYYYYYY UUUUUUUU VVVVVVVV*/
	RTS_NN_YUV444_SP =	2002,	/**< YYYYYYYY UVUVUVUVUVUV */

	/* YUV 4:2:2 formats family */
	RTS_NN_YUV422_PACKED =	3000,		/**< YUYVYUYVYUYVYUYV */
	RTS_NN_YUYV = RTS_NN_YUV422_PACKED,	/**< RTS_NN_YUV422_PACKED */
	RTS_NN_YVYU =		3001,		/**< YVYUYVYUYVYUYVYU */
	RTS_NN_UYVY =		3002,		/**< UYVYUYVYUYVYUYVY */
	RTS_NN_VYUY =		3003,		/**< VYUYVYUYVYUYVYUY */
	RTS_NN_YUV422_PLANAR =	3004,		/**< YYYYYYYY UUUU VVVV */
	RTS_NN_YVU422_PLANAR =	3005,		/**< YYYYYYYY VVVV UUUU */
	RTS_NN_YUV422_SP =	3006,		/**< YYYYYYYY UVUVUVUV */
	RTS_NN_YVU422_SP =	3007,		/**< YYYYYYYY VUVUVUVU */

	/* YUV 4:2:0 formats family */
	RTS_NN_I420 =		4000,		/**< planar, YYYYYYYY UU VV */
	RTS_NN_IYUV = RTS_NN_I420,		/**< RTS_NN_I420 */
	RTS_NN_YV12 =		4001,		/**< planar, YYYYYYYY VV UU */
	RTS_NN_NV12 =		4002,		/**< yuv420sp, YYYYYYYY UVUV */
	RTS_NN_NV21 =		4003,		/**< yuv420sp, YYYYYYYY VUVU */

	/* GRAY format */
	RTS_NN_GRAY =		5000,		/**<gray image: single channel*/
};

/**
 * @brief type of rtsnn shape
 */
enum RTS_NN_SHAPE_TYPE {
	RTS_NN_SHAPE_UNDEF = 0, /**< undefined shape */
	RTS_NN_SHAPE_POINT = 1, /**< line shape type */
	RTS_NN_SHAPE_LINE = 2, /**< line shape type */
	RTS_NN_SHAPE_RECT = 3, /**< rectagnle shape type */
	RTS_NN_SHAPE_CIRCLE = 4, /**< circle shape type */
};

/**
 * @brief Quantization params
 */
union rts_nn_qnt_params {
	struct rts_nn_int8_param {
		int fl; /**< fixed point position of int8 quantization */
	} ip; /**< int8 quantization params */

	struct rts_nn_uint8_param {
		int zp; /**< zero point of uint8 quantization */
		float scale; /**< scale of uint8 quantization */
	} up; /**< uint8 quantization params */
};

/**
 * @brief Quantization format
 */
struct rts_nn_qnt_format {
	enum RTS_NN_DTYPE qtype; /**< quantization type */
	union rts_nn_qnt_params p; /**< quantization params */
};


/**
 * @brief Normalization mean scale params
 */
struct rts_nn_norm_mean_scale {
	float means[3]; /**< normalization mean of each channel */
	float scales[3]; /**< normalization scale */
};

/**
 * @brief Bounding box of objects
 */
struct rts_nn_bbox {
	int x1; /**< left of bounding box */
	int x2;/**< right of bounding box */
	int y1;/**< top of bounding box */
	int y2;/**< bottom of bounding box */

	int id; /**< class id */
	float score;/**< object confidence */

	float yaw;
	float pitch;
	float roll;

	uint32_t reserved[8];
};

/**
 * @brief Bounding box with float coords of objects
 */
struct rts_nn_bbox_f {
	float x1; /**< left of bounding box */
	float x2;/**< right of bounding box */
	float y1;/**< top of bounding box */
	float y2;/**< bottom of bounding box */

	int id; /**< class id */
	float score;/**< object confidence */

	float yaw;
	float pitch;
	float roll;

	uint32_t reserved[8];
};

/**
 * @brief rtsnn image color space
 */
enum RTS_NN_IMG_CS {
	RTS_NN_CS_BT709 = 0, /**< BT709 */
	RTS_NN_CS_BT601 = 1/**< BT601 */
};

/**
 * @brief rtsnn point
 */
struct rts_nn_point {
	int x; /**< coord x of point */
	int y; /**< coord y of point */
};

/**
 * @brief rtsnn float point
 */
struct rts_nn_point_f {
	float x; /**< coord x of point */
	float y; /**< coord y of point */
};

/**
 * @brief rtsnn line
 */
struct rts_nn_line {
	struct rts_nn_point p1; /**< start point of the line */
	struct rts_nn_point p2; /**< end point of the line */
};

/**
 * @brief rtsnn rectangle
 */
struct rts_nn_rect {
	int x1; /**< coord x of rect */
	int y1; /**< coord y of rect */
	int x2; /**< width of rect */
	int y2; /**< height of rect */
};

/**
 * @brief rtsnn float rectangle
 */
struct rts_nn_rect_f {
	float x1; /**< coord x of rect */
	float y1; /**< coord y of rect */
	float x2; /**< width of rect */
	float y2; /**< height of rect */
};

/**
 * @brief rtsnn clicle
 */
struct rts_nn_circle {
	struct rts_nn_point center; /**< center point of the circle */
	int radius; /**< radius of the circle */
};

/**
 * @brief image attribute
 */
struct rts_nn_img_attr {
	int w; /**< width of image */
	int h; /**< height of image */
	int c; /**< channel of image */
	enum RTS_NN_IMG_FMT fmt; /**< format of image */
	enum RTS_NN_IMG_CS cs; /**< color space of image */
};

/**
 * @brief rtsnn image
 */
struct rts_nn_image {
	struct rts_nn_img_attr attr; /**< attribute of image */

	int quantized; /**< image data is already quantized or not */

	int flip; /**< image data need flip or not */
	int mirror; /**< image data need mirror or not */
	int rotate;  /**< image data need rotate or not */

	void *virt[4]; /**< virtual address of image */
	uint32_t phy[4]; /**< physical address of image */
};


/**
 * @brief rtsnn shape object
 */
struct rts_nn_shapes {
	int type;	/**< shape type: enum RTS_NN_SHAPE_TYPE */
	struct {
		/* shape data */
		union {
			struct rts_nn_point *points; /**< points data */
			struct rts_nn_line *lines; /**< lines data */
			struct rts_nn_rect *rects; /**< rects data */
			struct rts_nn_circle *circles; /**< circles data */
			void *data; /**< undefine data */
		};
		int num; /* num of shape */
	};
};

/**
 * @brief rtsnn tensor object
 */
struct rts_nn_tensor {
	int32_t dtype;	/**< data type: enum RTS_NN_DTYPE */
	void *data;         /**< tensor data */
	int dim_num;	/**< tensor dimension number */
	int dims[8];	/**< dimension size, support max 8 dims */
};

/**
 * @brief rtsnn label object for classification task
 */
struct rts_nn_label {
	int id;
	float score;
};

/**
 * @brief rtsnn labels object
 */
struct rts_nn_labels {
	struct rts_nn_label *labels;
	int num;
};

/**
 * @brief classes of network
 */
struct rts_nn_classes {
	int num; /**< class num */
	/** names of classes */
	char names[RTS_NN_MAX_CLASSES][RTS_NN_MAX_LABEL_LEN];
};

/**
 * @brief network version
 */
struct rts_nn_version {
	char name[16]; /**< od model name */
	char version[16]; /**< od model version */
	char date[16]; /**< date of model */
	char framework[16]; /**< based nn framework: MNN, unify, vipltte */
	char commit_id[16]; /**< commit id of library */
};

/**
 * @brief rtsnn result type
 */
enum RTS_NN_OBJ_TYPE {
	RTS_NN_OBJ_TYPE_IMAGE		= 1,

	RTS_NN_OBJ_TYPE_RES_OD		= 10000,
	RTS_NN_OBJ_TYPE_RES_POSE	= 11000,
	RTS_NN_OBJ_TYPE_RES_FR		= 12000,
	RTS_NN_OBJ_TYPE_RES_HPE		= 13009,
	RTS_NN_OBJ_TYPE_RES_LM68	= 14000,
	RTS_NN_OBJ_TYPE_RES_BLINK	= 15000,
	RTS_NN_OBJ_TYPE_RES_REID	= 16000,
	RTS_NN_OBJ_TYPE_RES_LM5		= 17000,
	RTS_NN_OBJ_TYPE_RES_QA		= 18000,
	RTS_NN_OBJ_TYPE_RES_FASL	= 19000,
	RTS_NN_OBJ_TYPE_RES_FASN	= 20000,
	RTS_NN_OBJ_TYPE_RES_FASG	= 21000,
	RTS_NN_OBJ_TYPE_RES_GIF		= 22000,
	RTS_NN_OBJ_TYPE_RES_FRS		= 23000,

	RTS_NN_OBJ_TYPE_CUSTOM		= 1000000,
};

/**
 * @brief rtsnn object, general nn object
 */
struct rts_nn_object {
	int type;
	union {
		/* image */
		struct rts_nn_image *image;

		/* res */
		struct rts_nn_od_res *od;
		struct rts_nn_lm_res *lm;
		struct rts_nn_lm68_res *lm68;
		struct rts_nn_blink_res *blink;
		struct rts_nn_fr_res *fr;
		struct rts_nn_pose_res *pose;
		struct rts_nn_hpe_res *hpe;
		struct rts_nn_reid_res *reid;

		/* general data */
		void *data;
	};
};

/**
 * @brief rtsnn objects
 */
struct rts_nn_objects {
	int num;
	struct rts_nn_object objs[RTS_NN_MAX_OBJECTS];
};

/**
 * @brief rts nn handle
 */
typedef void *rts_nn_handle;

/**
 * @brief rts nn model base configuration
 */
struct rts_nn_cfg {
	/** nn name, current support is: od_tiny */
	char model_name[32];
	char model_path[128]; /**< model path */
};

#ifdef __cplusplus
}
#endif

#endif
