/*
 * Copyright (C) 2022 Realtek Semiconductor Corp.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "header.h"

#define MINIMP3_IMPLEMENTATION
#include "alure/minimp3.h"

#define BIT_FORMAT				16

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{0, 0, 0, 0}
};

static int get_audio_parameters(unsigned char *frame_header,
			int *channels,
			int *samplerate)
{
	unsigned char MPEG_version_ID, sample_rate_index, channel_num_index;
	int sample_rate = 0, channel_num = 0;

	/* frame_header bit[20:19] */
	MPEG_version_ID = (frame_header[1] >> 3) & 0x3;
	/* frame_header bit[11:10] */
	sample_rate_index = (frame_header[2] >> 2) & 0x3;
	/* frame_header bit[7:6] */
	channel_num_index = (frame_header[3] >> 6) & 0x3;

	switch (MPEG_version_ID) {
	case 0x0: // MPEG 2.5
		if (sample_rate_index == 0x0)
			sample_rate = 11025;
		else if (sample_rate_index == 0x1)
			sample_rate = 12000;
		else if (sample_rate_index == 0x2)
			sample_rate = 8000;
		break;
	case 0x2: // MPEG 2
		if (sample_rate_index == 0x0)
			sample_rate = 22050;
		else if (sample_rate_index == 0x1)
			sample_rate = 24000;
		else if (sample_rate_index == 0x2)
			sample_rate = 16000;
		break;
	case 0x3: // MPEG 1
		if (sample_rate_index == 0x0)
			sample_rate = 44100;
		else if (sample_rate_index == 0x1)
			sample_rate = 48000;
		else if (sample_rate_index == 0x2)
			sample_rate = 32000;
		break;
	default:
		break;
	}
	if (sample_rate == 0) {
		printf("fail to get sample rate\n");
		return -11;
	}
	*samplerate = sample_rate;
	channel_num = (channel_num_index == 0x3 ? 1 : 2);
	*channels = channel_num;

	return 0;
}

void print_help_info(const char *name)
{
	fprintf(stdout, "DESCRIPTION:\n\n");
	fprintf(stdout, "--help | -h \tprint this help message\n");
	fprintf(stdout, "--input | -i <input file path>\n");
	fprintf(stdout, "--output | -o <output file path>\n");
	fprintf(stdout, "\t\t16bits width pcm .wav file\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\t%s -i in.mp3 -o out.wav\n", name);
}

int main(int argc, char *argv[])
{
	int c, i, ret;
	char *in_filename = NULL, *out_filename = NULL;
	FILE *fpout = NULL;
	int fd = -1;
	struct stat st;
	unsigned char *fdm = NULL, *input_ptr = NULL, *p_header = NULL;
	int input_remaining_length = 0;
	int ID3V2_label_size = 0;
	int channels, samplerate;
	int wav_header_length = 0;
	struct subchunk_fmt fmt = {0};
	mp3dec_t mp3_obj = {0};
	mp3dec_frame_info_t decode_info = {0};
	mp3d_sample_t outpcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	int decoded_samples = 0, decoded_length = 0;
	int pcmdata_size = 0;

	/* Command Process */
	while ((c = getopt(argc, argv, "hi:o:")) != -1) {
		switch (c) {
		case 'h':
			print_help_info(argv[0]);
			return 0;
		case 'i':
			in_filename = optarg;
			break;
		case 'o':
			out_filename = optarg;
			break;
		default:
			break;
		}
	}

	/* Param Check */
	fd = open(in_filename, O_RDWR);
	if (fstat(fd, &st) == -1 || st.st_size == 0) {
		printf("fail to get input file status\n");
		goto exit;
	}
	fdm = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!fdm) {
		printf("fail to mmap data\n");
		goto exit;
	}
	/* ID3V2 Label Header Bytes 6-9 */
	ID3V2_label_size = ((fdm[6] & 0x7f) << 21) | ((fdm[7] & 0x7f) << 14) |
			((fdm[8] & 0x7f) << 7) | (fdm[9] & 0x7f);
	ID3V2_label_size += 10;
	ret = get_audio_parameters(fdm + ID3V2_label_size,
					&channels, &samplerate);
	if (ret) {
		printf("fail to get input audio parameters\n");
		goto exit;
	}

	/* Output WAV Header Set */
	fpout = fopen(out_filename, "wb+");
	if (!fpout) {
		printf("fail to open output file\n");
		goto exit;
	}
	wav_header_length = sizeof(struct chunk_riff) +
				sizeof(struct subchunk_header) +
				sizeof(struct subchunk_fmt) +
				sizeof(struct subchunk_header);
	/**
	 * "RIFF" chunk
	 * "fmt" sub-chunk header
	 * "fmt" sub-chunk content
	 * "data" sub-chunk header
	 */
	ret = init_wav_header(fpout, wav_header_length);
	if (ret < 0) {
		printf("fail to init output wav header\n");
		goto exit;
	}

	/* Decode Object Init & Input Configuration */
	mp3dec_init(&mp3_obj);
	input_ptr = (unsigned char *)fdm + ID3V2_label_size;
	input_remaining_length = st.st_size - ID3V2_label_size;

	/* Start Decoding */
	do {
		decoded_length += decode_info.frame_bytes;
		decoded_samples = mp3dec_decode_frame(&mp3_obj,
				input_ptr + decoded_length,
				input_remaining_length - decoded_length,
				outpcm,
				&decode_info);
		fwrite(outpcm, sizeof(mp3d_sample_t),
					decoded_samples * channels, fpout);
		pcmdata_size += decoded_samples * channels *
					sizeof(mp3d_sample_t);
	} while (decoded_samples);

	/* Output WAV Header Modify */
	fmt.format_tag = 0x01;
	fmt.num_channels = channels;
	fmt.sample_rate = samplerate;
	fmt.bytes_rate = samplerate * channels * BIT_FORMAT / 8;
	fmt.block_align = BIT_FORMAT / 8;
	fmt.num_bits = BIT_FORMAT;
	fseek(fpout, 0, SEEK_SET);
	fill_wav_header(fpout, &fmt, pcmdata_size);

exit:
	ret = munmap(fdm, st.st_size);
	if (ret == -1) {
		printf("fail to munmap data\n");
		return -4;
	}
	if (fpout) {
		fclose(fpout);
		fpout = NULL;
	}
	close(fd);
	return 0;
}
