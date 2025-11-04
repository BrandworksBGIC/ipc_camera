#ifndef __IPC_INTERPOLATION_H__
#define __IPC_INTERPOLATION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief Interpolation algorithm for 32bit images such as rgba, argb, bgra, etc. (vertical scan interpolation)
 *
 * @param dest_data Interpolation starting position
 * @param dset_width The width of the interpolation target
 * @param dest_height The height of the interpolation target
 * @param src_data Source image
 * @param src_width Source image width
 * @param src_height Source image height
 * @param skip_each_line When scanning vertically, the number of pixels to skip after each line of interpolation is
 * completed. If it is 0, the default is to skip dset_width pixels, which is used when interpolating and stitching
 * multiple images into a larger image (such as osd)
 */
EXAPI void ipc_32bit_interpolation(pu32 dest_data, s32 dset_width, s32 dest_height, pu32 src_data, s32 src_width,
                                  s32 src_height, s32 skip_each_line);

#ifdef __cplusplus
}
#endif

#endif //__IPC_INTERPOLATION_H__
