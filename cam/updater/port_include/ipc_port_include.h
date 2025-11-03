
#ifndef __CP_PORT_INCLUDE_H__
#define __CP_PORT_INCLUDE_H__
#endif //__CP_PORT_INCLUDE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef IPC_IS_UBOOT_BUILD

#define SEEK_SET 0 /* Seek from beginning of file.  */
#define SEEK_CUR 1 /* Seek from current position.  */
#define SEEK_END 2 /* Seek from end of file.  */

#include "linux/string.h"
#include <malloc.h>

extern int printf(const char *fmt, ...);

#else

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#endif

#ifdef __cplusplus
}
#endif

