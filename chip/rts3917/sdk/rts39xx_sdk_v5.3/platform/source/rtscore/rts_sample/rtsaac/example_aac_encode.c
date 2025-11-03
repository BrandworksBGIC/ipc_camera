/*
 ** Copyright (C) 2022 Realtek Semiconductor Corp.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <vo-aacenc/voAAC.h>
#include <vo-aacenc/cmnMemory.h>
#include "header.h"

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"bitrate", required_argument, NULL, 'r'},
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{0, 0, 0, 0}
};

void print_help_info(const char *name)
{
	fprintf(stdout, "DESCRIPTION:\n\n");
	fprintf(stdout, "--help | -h \tprint this help message\n");
	fprintf(stdout, "--bitrate | -r <output file bitrate>\n");
	fprintf(stdout, "--input | -i <input file path>\n");
	fprintf(stdout, "\t\t16bits width pcm file\n");
	fprintf(stdout, "--output | -o <output file path>\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\t%s -r 64000 -i in.wav -o out.aac\n", name);
}

int main(int argc, char *argv[])
{
	int ret, c;
	int bitrate = 64000;
	char *in_filename = NULL, *out_filename = NULL;
	FILE *infp = NULL, *outfp = NULL;
	struct subchunk_fmt fmt = {0};
	int head_length = 0, file_data_size = 0;
	int sample_rate, channels;
	VO_AUDIO_CODECAPI codec_api = { 0 };
	VO_HANDLE handle = 0;
	VO_MEM_OPERATOR mem_operator = { 0 };
	VO_CODEC_INIT_USERDATA user_data;
	AACENC_PARAM params = { 0 };
	short *input_buf = NULL;
	unsigned char *output_buf = NULL;
	int input_size = 0, output_size = 0;

	/* Command Process */
	while ((c = getopt_long(argc, argv,
				":hr:i:o:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info(argv[0]);
			return 0;
		case 'r':
			bitrate = atoi(optarg);
			break;
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
	infp = fopen(in_filename, "rb");
	if (!infp) {
		printf("fail to open %s\n", in_filename);
		goto exit;
	}
	ret = analyze_audio_header(infp, &fmt, &head_length, &file_data_size);
	if (ret) {
		printf("fail to analyze header of %s\n", in_filename);
		goto exit;
	}
	if (fmt.format_tag != 1) {
		printf("unsupported format tag %d\n", fmt.format_tag);
		goto exit;
	}
	if (fmt.num_bits != 16) {
		printf("unsupported bit format %d\n", fmt.num_bits);
		goto exit;
	}
	channels = fmt.num_channels;
	sample_rate = fmt.sample_rate;

	/* AAC Encoder SetParam */
	voGetAACEncAPI(&codec_api);
	mem_operator.Alloc = cmnMemAlloc;
	mem_operator.Copy = cmnMemCopy;
	mem_operator.Free = cmnMemFree;
	mem_operator.Set = cmnMemSet;
	mem_operator.Check = cmnMemCheck;
	user_data.memflag = VO_IMF_USERMEMOPERATOR;
	user_data.memData = &mem_operator;
	codec_api.Init(&handle, VO_AUDIO_CodingAAC, &user_data);
	params.sampleRate = sample_rate;
	params.bitRate = bitrate;
	params.nChannels = channels;
	params.adtsUsed = 1;
	if (codec_api.SetParam(handle, VO_PID_AAC_ENCPARAM,
				&params) != VO_ERR_NONE) {
		printf("unable to set encoding parameters\n");
		goto exit;
	}

	/* Encode */
	outfp = fopen(out_filename, "wb");
	if (!outfp) {
		printf("fail to open %s\n", out_filename);
		goto exit;
	}
	input_size = channels * sizeof(short) * 1024;
	input_buf = (short *)malloc(input_size);
	if (!input_buf) {
		printf("fail to allocate input_buf\n");
		goto exit;
	}
	output_size = 2048;
	output_buf = (unsigned char *)malloc(output_size);
	if (!output_buf) {
		printf("fail to allocate output_buf\n");
		goto exit;
	}
	while (1) {
		VO_CODECBUFFER input = { 0 }, output = { 0 };
		VO_AUDIO_OUTPUTINFO output_info = { 0 };

		ret = fread(input_buf, sizeof(short),
					input_size / sizeof(short), infp);
		if (ret < (input_size / sizeof(short)))
			break;
		input.Buffer = (unsigned char *)input_buf;
		input.Length = ret * sizeof(short);
		codec_api.SetInputData(handle, &input);

		output.Buffer = output_buf;
		output.Length = output_size;
		if (codec_api.GetOutputData(handle, &output,
					&output_info) != VO_ERR_NONE) {
			printf("unable to encode frame\n");
			goto exit;
		}
		fwrite(output.Buffer, 1, output.Length, outfp);
	}

exit:
	if (input_buf) {
		free(input_buf);
		input_buf = NULL;
	}
	if (output_buf) {
		free(output_buf);
		output_buf = NULL;
	}
	if (outfp) {
		fclose(outfp);
		outfp = NULL;
	}
	if (infp) {
		fclose(infp);
		infp = NULL;
	}
	codec_api.Uninit(handle);
	return 0;
}
