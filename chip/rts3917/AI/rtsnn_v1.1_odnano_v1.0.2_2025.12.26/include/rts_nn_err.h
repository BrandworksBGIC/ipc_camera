#ifndef __RTS_NN_ERR_H_
#define __RTS_NN_ERR_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rts_nn_err.h
 * @brief rts nn defined error types
 */

/**
 * @brief rts nn defined status
 */
enum RTS_NN_STATUS {
	/** exceed limit run time*/
	RTS_NN_ERR_EXCEED_LIMIT_TIMES		= -14,
	/** pointer is not 64Bytes algined*/
	RTS_NN_ERR_NOT_64B_ALIGNED		= -13,
	/** invalid parameter type*/
	RTS_NN_ERR_INVALID_TYPE			= -12,
	/** invalid dimension*/
	RTS_NN_ERR_INVALID_DIMENSION		= -11,
	/** invalid pointer reference*/
	RTS_NN_ERR_INVALID_REFERENCE		= -10,
	/** invalid parameter value */
	RTS_NN_ERR_INVALID_PARAMETERS		= -9,
	/** out of memory */
	RTS_NN_ERR_NO_MEMORY			= -8,
	/** invalid resources */
	RTS_NN_ERR_NO_RESOURCES			= -7,
	/** not compatible*/
	RTS_NN_ERR_NOT_COMPATIBLE		= -6,
	/** resource is not allocated*/
	RTS_NN_ERR_NOT_ALLOCATED		= -5,
	/** condition is not sufficient*/
	RTS_NN_ERR_NOT_SUFFICIENT		= -4,
	/** not supported */
	RTS_NN_ERR_NOT_SUPPORTED		= -3,
	/** function is not implemented */
	RTS_NN_ERR_NOT_IMPLEMENTED		= -2,
	/** general error */
	RTS_NN_FAILURE				= -1,
	/** success */
	RTS_NN_SUCCESS				=  0,
};

#ifdef __cplusplus
}
#endif
#endif
