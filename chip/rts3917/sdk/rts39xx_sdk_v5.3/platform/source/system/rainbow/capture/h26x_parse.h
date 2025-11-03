#ifndef __H26X_PARSE_H__
#define __H26X_PARSE_H__

int save_sps_pps_info(int video_index, unsigned int h264_chnno);
int save_vps_sps_pps_info(int video_index, unsigned int h265_chnno);
void write_h26x_info_to_ringbuf(int max_video_num);

#endif
