#ifndef _RTS_NN_H_
#define _RTS_NN_H_
#include "rts_nn_err.h"
#include "rts_nn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rts_nn.h
 * @brief rts nn interface
 */

/**
 * @brief intialize network environment using given configurations
 * @param[out] phandle the pointer of handle which hold the network structure
 * @param[in] cfg the network configuration
 * @return 0 : success, minus : fail
 */
RTS_NN_API int rts_nn_init(rts_nn_handle *phandle, struct rts_nn_cfg *cfg);

/**
 * @brief run nn
 * @param[in] handle the handle which hold the network structure
 * @param[in] input input object array
 * @param[out] output output object array
 * @return 0 : success, minus : fail
 * @note: pointer in output buffer is borrowed from internal, must not free it
 * and rts_nn_run is not multi-thread safely
 */
RTS_NN_API int rts_nn_run(rts_nn_handle handle,
		struct rts_nn_objects *input, struct rts_nn_objects *output);

/**
 * @brief release nn resources
 * @param[in] phandle the pointer of handle which hold the network structure
 */
RTS_NN_API void rts_nn_release(rts_nn_handle *phandle);

/**
 * @brief get network property
 * @param[in] handle nn handle
 * @param[in] key property key
 * @param[in] size size of input data
 * @param[in] data value to be stored
 * @return 0: success, minus: fail
 * @note Data type must be consistent with key.\n
 * The properties of each model are descripted in each model introduction page
 */
RTS_NN_API int rts_nn_get_property(rts_nn_handle handle,
		const char *key, uint32_t size, void *data);

/**
 * @brief set network property
 * @param[in] handle nn handle
 * @param[in] key property key
 * @param[in] size size of input data
 * @param[out] data value to be set
 * @return 0: success, minus: fail
 * @note Data type must be consistent with key.\n
 * The properties of each model are descripted in each model introduction page.
 */
RTS_NN_API int rts_nn_set_property(rts_nn_handle handle,
		const char *key, uint32_t size, void *data);

/**
 * @brief get model version
 * @param[in] handle nn handle
 * @param[out] ver model version info
 * @see rts_nn_version
 * @return 0: success, minus: fail
 */
RTS_NN_API int rts_nn_get_version(rts_nn_handle handle,
		struct rts_nn_version *ver);

/**
 * @brief get model class information
 * @param[in] handle nn handle
 * @param[out] cls model class num and each class name
 * @see rts_nn_classes
 * @return 0: success, minus: fail
 */
RTS_NN_API int rts_nn_get_classes(rts_nn_handle handle,
		struct rts_nn_classes *cls);

/**
 * @brief get model normalization information
 * @param[in] handle nn handle
 * @param[out] ms normalization means and scales
 * @see rts_nn_norm_mean_scale
 * @return 0: success, minus: fail
 */
RTS_NN_API int rts_nn_get_mean_scale(rts_nn_handle handle,
		struct rts_nn_norm_mean_scale *ms);

/**
 * @brief get model quantization information
 * @param[in] handle nn handle
 * @param[out] qfmt quantization format
 * @see rts_nn_qnt_format
 * @return 0: success, minus: fail
 */
RTS_NN_API int rts_nn_get_qnt_format(rts_nn_handle handle,
		struct rts_nn_qnt_format *qfmt);

/**
 * @brief debug functions, save result to bmp
 * @param[in] fileName file name of saved bmp
 * @param[in] inputBuf the pointer of image data
 * @param[in] objs result array of nn algorithm
 * @param[in] obj_num of objects
 * @param[in] width image width
 * @param[in] height image height
 * @param[in] channel image channel
 * @param[in] format image format, currently support GRAY, RGB, BGR, RGBA, NV12
 * @return 0 : success, minus : fail
 */
RTS_NN_API int rts_nn_save_bmp(const char *fileName,
		unsigned char *inputBuf,
		struct rts_nn_object objs[], int obj_num,
		unsigned int width, unsigned int height, int channel,
		int format);

#ifdef __cplusplus
}
#endif

#endif /*_RTS_NN_H_*/
