/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

#ifndef _RTS_ISP_ERRNO_H_INC_
#define _RTS_ISP_ERRNO_H_INC_

#include <errno.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum rts_isp_boolean {
	RTS_ISP_FALSE = 0,
	RTS_ISP_TRUE,
};

enum rts_isp_errno {
	RTS_ISP_OK = 0,
	RTS_ISP_EPERM = EPERM,
	RTS_ISP_EINTR = EINTR,
	RTS_ISP_EAGAIN = EAGAIN,
	RTS_ISP_ENOMEM = ENOMEM,
	RTS_ISP_EACCES = EACCES,
	RTS_ISP_EFAULT = EFAULT,
	RTS_ISP_EBUSY = EBUSY,
	RTS_ISP_EEXIST = EEXIST,
	RTS_ISP_EINVAL = EINVAL,
	RTS_ISP_ENOSPC = ENOSPC,
	RTS_ISP_ERANGE = ERANGE,
	RTS_ISP_ENAMETOOLONG = ENAMETOOLONG,
	RTS_ISP_ECONNRESET = ECONNRESET,
	RTS_ISP_ETIMEDOUT = ETIMEDOUT,
	/* private ERROR */
	RTS_ISP_ENOOPS = 200,
	RTS_ISP_ENOITEM,
	RTS_ISP_ENOTREADY,
	RTS_ISP_EPLUGIN,
	RTS_ISP_EPOLLERR,
	RTS_ISP_ECTRL,
	RTS_ISP_ECONFLICT,
	RTS_ISP_ECANCELD,
	RTS_ISP_EPROTOBUF,
	_MAX_RTS_ISP_ERRNO,
};

int rts_isp_strerror(int errnum, char *msg_buf, size_t size);
void rts_isp_perror(int errnum, const char *s, ...);

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_ERRNO_H_INC_ */
