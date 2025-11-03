#ifndef __IMAGE_WRITE_H__
#define __IMAGE_WRITE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "update_pack_decode.h"

int partition_wirte_image(struct update_pack_image_desc* part, int fd);

#ifdef __cplusplus
}
#endif

#endif //__IMAGE_WRITE_H__