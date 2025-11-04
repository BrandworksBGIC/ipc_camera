#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ipc_motion_detect.h"

#define BIG_BLOCK_NUM 8

static struct {
    ipc_motion_sad_e sad_type;   // Type of SAD, macroblock size
    s32 width;                  // Width of the analyzed image
    s32 height;                 // Height of the analyzed image
    pu16 macroblock[2];         // Macroblock buffer, each array unit u16 max 65535, stores the sum of macroblock luminance
    s32 macroblock_buffer_size; // Size of the macroblock buffer
    s32 macroblock_w_num;       // Number of macroblocks in width
    s32 macroblock_h_num;       // Number of macroblocks in height
    s32 bigblock_w;             // Width coordinate count for large blocks
    s32 bigblock_h;             // Height coordinate count for large blocks
    s32 luminance_sad_count[BIG_BLOCK_NUM + 1][BIG_BLOCK_NUM + 1]; // Number of small macroblocks within each large block
    s32 rdx_correction;                                            // Coordinate correction amount for the final output rectangle
    s32 rdy_correction;                                            // Coordinate correction amount for the final output rectangle
    s32 cur_macroblock_index;                                      // Current index of the used macroblock buffer
    f32 sensitivity;                                               // Sensitivity
    u16 alarm_level[BIG_BLOCK_NUM + 1][BIG_BLOCK_NUM + 1];         // Dynamically calculated alarm threshold
    s32 displacement;                                              // Displacement calculation based on macroblock size
    u8 init_flag;
} _g_motion;

static void _point_push_to_rect(s32 w, s32 h, s32 flag, ipc_alarm_result_p result)
{
    s32 i   = 0;
    s32 ret = 0;
    for (i = 0; i < MAX_RECT_NUM; i++) {
        ret = 0;
        if (result->rect[i].rdx < 0) {
            if (flag) {
                result->rect[i].lux = w;
                result->rect[i].luy = h;
                result->rect[i].rdx = w;
                result->rect[i].rdy = h;
                result->rect_num    = (i + 1 > result->rect_num) ? i + 1 : result->rect_num;
            }
            break;
        }

        if ((abs(w - result->rect[i].rdx) <= 1) && (h - result->rect[i].rdy == 0)) {
            ret = 1;
        }

        if ((abs(h - result->rect[i].rdy) <= 1) && (result->rect[i].lux <= w) && (result->rect[i].rdx >= w)) {
            ret = 1;
        }

        if (ret) {
            if (w < result->rect[i].lux) {
                result->rect[i].lux = w;
            }

            if (h < result->rect[i].luy) {
                result->rect[i].luy = h;
            }

            if (w > result->rect[i].rdx) {
                result->rect[i].rdx = w;
            }

            if (h > result->rect[i].rdy) {
                result->rect[i].rdy = h;
            }

            result->rect_num = (i + 1 > result->rect_num) ? i + 1 : result->rect_num;
            break;
        }
    }
}

static u8 is_overlap(rect_p rc1, rect_p rc2)
{
    if (rc1->rdx + 1 >= rc2->lux && rc2->rdx + 1 >= rc1->lux && rc1->rdy + 1 >= rc2->luy && rc2->rdy + 1 >= rc1->luy) {
        return 1;
    } else {
        return 0;
    }
}

static void _merge_rect(ipc_alarm_result_p result)
{
    s32 i = 0;
    s32 j = 0;

    if (result->rect_num >= 2) {
        for (i = 0; i < result->rect_num - 1; i++) {
            for (j = i + 1; j < result->rect_num; j++) {
                if (is_overlap(&result->rect[i], &result->rect[j])) {
                    result->rect[i].lux = (result->rect[j].lux < result->rect[i].lux) ? result->rect[j].lux : result->rect[i].lux;
                    result->rect[i].luy = (result->rect[j].luy < result->rect[i].luy) ? result->rect[j].luy : result->rect[i].luy;
                    result->rect[i].rdx = (result->rect[j].rdx > result->rect[i].rdx) ? result->rect[j].rdx : result->rect[i].rdx;
                    result->rect[i].rdy = (result->rect[j].rdy > result->rect[i].rdy) ? result->rect[j].rdy : result->rect[i].rdy;
                    result->rect[j]     = result->rect[i];
                }
            }
        }
    }
}

static void _get_image_macroblock_sum(pu8 y_data)
{
    s32 w                    = 0;
    s32 h                    = 0;
    s32 displacement         = _g_motion.displacement;
    s32 cur_macroblock_index = _g_motion.cur_macroblock_index;
    s32 image_width          = _g_motion.width;
    s32 image_height         = _g_motion.height;
    s32 macroblock_w_num     = _g_motion.macroblock_w_num;

    memset(_g_motion.macroblock[cur_macroblock_index], 0, _g_motion.macroblock_buffer_size);

    for (h = 0; h < image_height; h++) {
        s32 width_offset      = image_width * h;
        s32 macroblock_offset = (h >> displacement) * macroblock_w_num;
        for (w = 0; w < image_width; w++) {
            _g_motion.macroblock[cur_macroblock_index][(w >> displacement) + macroblock_offset] += y_data[width_offset + w];
        }
    }

    _g_motion.cur_macroblock_index = !cur_macroblock_index;
}

s32 ipc_motion_detect_process(pu8 y_data, ipc_alarm_result_p result)
{
    if (!y_data || !result)
        return IPC_INVALID_ARGS;
    if (!_g_motion.init_flag)
        return IPC_NOT_INIT;

    _get_image_macroblock_sum(y_data);

    s32 i                = 0;
    s32 j                = 0;
    s32 macroblock_w_num = _g_motion.macroblock_w_num;
    s32 macroblock_h_num = _g_motion.macroblock_h_num;
    s32 index            = !_g_motion.cur_macroblock_index;
    s32 sad_type         = _g_motion.sad_type;
    s32 rdx_correction   = _g_motion.rdx_correction;
    s32 rdy_correction   = _g_motion.rdy_correction;
    s32 luminance_sum[BIG_BLOCK_NUM + 1][BIG_BLOCK_NUM + 1]; // Sum of luminance for each large block
    s32 alarm_macroblock_count = 0;                          // Number of macroblocks in alarm

    memset(luminance_sum, 0, sizeof(luminance_sum));

    memset(result, -2, sizeof(*result));
    result->rect_num = 0;

    for (i = 0; i < macroblock_h_num; i++) {
        s32 woffset    = i * macroblock_w_num;
        s32 bighoffset = i / _g_motion.bigblock_h;
        for (j = 0; j < macroblock_w_num; j++) {
            s32 bigwoffset = j / _g_motion.bigblock_w;
            s32 offset     = woffset + j;
            s32 diff       = abs(_g_motion.macroblock[0][offset] - _g_motion.macroblock[1][offset]);
            luminance_sum[bigwoffset][bighoffset] += _g_motion.macroblock[index][offset];

            if (diff > _g_motion.alarm_level[bigwoffset][bighoffset]) {
                alarm_macroblock_count++;
                _point_push_to_rect(j, i, 1, result);
            }
        }
    }

    _merge_rect(result);

    s32 bigblock_w_count = macroblock_w_num / _g_motion.bigblock_w;
    s32 bigblock_h_count = macroblock_h_num / _g_motion.bigblock_h;

    if (macroblock_w_num % _g_motion.bigblock_w > 0) {
        bigblock_w_count++;
    }
    if (macroblock_h_num % _g_motion.bigblock_h > 0) {
        bigblock_h_count++;
    }

    for (i = 0; i < bigblock_w_count; i++) {
        for (j = 0; j < bigblock_h_count; j++) {
            s32 avg_sad = luminance_sum[i][j] / _g_motion.luminance_sad_count[i][j];
            s32 avg_luminance = avg_sad / (sad_type * sad_type);

            f32 limit = avg_luminance / 10000.f;

            _g_motion.alarm_level[i][j] = (avg_sad * (limit + _g_motion.sensitivity));
        }
    }

    for (i = 0; i < result->rect_num; i++) {
        result->rect[i].lux *= sad_type;
        result->rect[i].luy *= sad_type;
        result->rect[i].rdx *= sad_type;
        result->rect[i].rdy *= sad_type;
        result->rect[i].rdx += rdx_correction;
        result->rect[i].rdy += rdy_correction;
    }

    result->alarm_image_percent = alarm_macroblock_count / (f32)(_g_motion.macroblock_w_num * _g_motion.macroblock_h_num);

    return IPC_SUCCESS;
}

s32 ipc_motion_detect_init(ipc_motion_detect_attr_p attr)
{
    if (!attr)
        return IPC_INVALID_ARGS;
    if (_g_motion.init_flag)
        return IPC_EXIST;

    s32 div_left = 0;
    memset(&_g_motion, 0, sizeof(_g_motion));

    _g_motion.sad_type         = attr->sad_type;
    _g_motion.width            = attr->width;
    _g_motion.height           = attr->height;
    _g_motion.macroblock_w_num = _g_motion.width / attr->sad_type;
    _g_motion.macroblock_h_num = _g_motion.height / attr->sad_type;
    _g_motion.sensitivity      = attr->sensitivity;

    div_left = _g_motion.width % attr->sad_type;
    if (div_left > 0) {
        _g_motion.macroblock_w_num += 1;
        _g_motion.rdx_correction = div_left;
    } else {
        _g_motion.rdx_correction = attr->sad_type;
    }

    div_left = _g_motion.height % attr->sad_type;
    if (div_left > 0) {
        _g_motion.macroblock_h_num += 1;
        _g_motion.rdy_correction = div_left;
    } else {
        _g_motion.rdy_correction = attr->sad_type;
    }

    if (_g_motion.sad_type == IPC_MOTION_SAD_4x4) {
        _g_motion.displacement = 2;
    } else if (_g_motion.sad_type == IPC_MOTION_SAD_8x8) {
        _g_motion.displacement = 3;
    } else {
        _g_motion.displacement = 4;
    }

    _g_motion.macroblock_buffer_size = _g_motion.macroblock_w_num * _g_motion.macroblock_h_num * 2;

    _g_motion.macroblock[0] = ipc_malloc(_g_motion.macroblock_buffer_size, _g_motion.macroblock_buffer_size);
    if (!_g_motion.macroblock[0])
        return IPC_NOMEM;

    _g_motion.macroblock[1] = ipc_malloc(_g_motion.macroblock_buffer_size, _g_motion.macroblock_buffer_size);
    if (!_g_motion.macroblock[1]) {
        ipc_free(_g_motion.macroblock[0]);
        return IPC_NOMEM;
    }

    _g_motion.bigblock_w = _g_motion.macroblock_w_num / BIG_BLOCK_NUM;
    if (_g_motion.macroblock_w_num % BIG_BLOCK_NUM > 0) {
        _g_motion.bigblock_w++;
    }

    _g_motion.bigblock_h = _g_motion.macroblock_h_num / BIG_BLOCK_NUM;
    if (_g_motion.macroblock_h_num % BIG_BLOCK_NUM > 0) {
        _g_motion.bigblock_h++;
    }

    memset(_g_motion.alarm_level, 0xff, sizeof(_g_motion.alarm_level));

    for (s32 i = 0; i < _g_motion.macroblock_h_num; i++) {
        s32 bighoffset = i / _g_motion.bigblock_h;
        for (s32 j = 0; j < _g_motion.macroblock_w_num; j++) {
            s32 bigwoffset = j / _g_motion.bigblock_w;
            _g_motion.luminance_sad_count[bigwoffset][bighoffset]++;
        }
    }

    _g_motion.init_flag = 1;

    return IPC_SUCCESS;
}

void ipc_motion_detect_set_sensitivity(f32 sensitivity)
{
    _g_motion.sensitivity = sensitivity;
}

void ipc_motion_detect_uninit(void)
{
    if (_g_motion.init_flag) {
        ipc_free(_g_motion.macroblock[0]);
        ipc_free(_g_motion.macroblock[1]);
        memset(&_g_motion, 0, sizeof(_g_motion));
    }
}