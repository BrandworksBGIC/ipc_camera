#include "ipc_interpolation.h"

typedef struct {
    u8 bit1, bit2, bit3, bit4;
} bit32_t, *bit32_p;

void ipc_32bit_interpolation(pu32 dest_data, s32 dset_width, s32 dest_height, pu32 src_data, s32 src_width,
                            s32 src_height, s32 skip_each_line)
{
    if (skip_each_line <= 0)
        skip_each_line = dset_width;

    for (s32 row = 0; row < dest_height; row++) {

        f32 h_point = (row + 0.5) * (src_height * 1.0 / dest_height) - 0.5; /* Calculate virtual mapping points */
        s32 src_h   = floor(h_point);                                       /* top of point (start) */
        h_point -= src_h;                                                   /* Relative starting offset */

        if (src_h < 0) {
            h_point = 0;
            src_h   = 0;
        } else if (src_h >= src_height - 1) {
            h_point = 1; /* Different from online, I am thinking that after geometrical alignment, the value of the
                            pixel point in the lower right corner should be calculated for the exceeded half pixel
                            point. 0 tends to be the pixel point in the upper right corner, and 1 tends to be the pixel
                            point in the lower right corner */
            src_h = src_height - 2; /* + 1 below does not exceed the scope, because two diagonal pixel points are needed
                                       to calculate the interpolation */
        }

        s32 ih_point = h_point * 2048; /* Optimize to integer operation */

        for (s32 col = 0; col < dset_width; col++) {

            f32 w_point = (col + 0.5) * (src_width * 1.0 / dset_width) - 0.5; /* Calculate virtual mapping points */
            s32 src_w   = floor(w_point);                                     /* top of point (start) */
            w_point -= src_w;                                                 /* Relative starting offset */

            if (src_w < 0) {
                w_point = 0;
                src_w   = 0;
            } else if (src_w >= src_width - 1) {
                w_point = 1;
                src_w   = src_width - 2; /* + 1 below does not exceed the scope */
            }

            s32 iw_point   = w_point * 2048; /* Optimize to integer operation */
            bit32_p w_pix0 = (bit32_p)&src_data[src_h * src_width + src_w];
            bit32_p w_pix1 = (bit32_p)&src_data[src_h * src_width + src_w + 1];
            bit32_p w_pix2 = (bit32_p)&src_data[(src_h + 1) * src_width + src_w];
            bit32_p w_pix3 = (bit32_p)&src_data[(src_h + 1) * src_width + src_w + 1];

            bit32_t h_pix0 = {
                .bit1 = ((w_pix1->bit1 - w_pix0->bit1) * iw_point >> 11) + w_pix0->bit1,
                .bit2 = ((w_pix1->bit2 - w_pix0->bit2) * iw_point >> 11) + w_pix0->bit2,
                .bit3 = ((w_pix1->bit3 - w_pix0->bit3) * iw_point >> 11) + w_pix0->bit3,
                .bit4 = ((w_pix1->bit4 - w_pix0->bit4) * iw_point >> 11) + w_pix0->bit4,
            };

            bit32_t h_pix1 = {
                .bit1 = ((w_pix3->bit1 - w_pix2->bit1) * iw_point >> 11) + w_pix2->bit1,
                .bit2 = ((w_pix3->bit2 - w_pix2->bit2) * iw_point >> 11) + w_pix2->bit2,
                .bit3 = ((w_pix3->bit3 - w_pix2->bit3) * iw_point >> 11) + w_pix2->bit3,
                .bit4 = ((w_pix3->bit4 - w_pix2->bit4) * iw_point >> 11) + w_pix2->bit4,
            };

            bit32_t pix = {
                .bit1 = ((h_pix1.bit1 - h_pix0.bit1) * ih_point >> 11) + h_pix0.bit1,
                .bit2 = ((h_pix1.bit2 - h_pix0.bit2) * ih_point >> 11) + h_pix0.bit2,
                .bit3 = ((h_pix1.bit3 - h_pix0.bit3) * ih_point >> 11) + h_pix0.bit3,
                .bit4 = ((h_pix1.bit4 - h_pix0.bit4) * ih_point >> 11) + h_pix0.bit4,
            };

            (dest_data + row * skip_each_line)[col] = *(pu32)&pix;
        }
    }
}
