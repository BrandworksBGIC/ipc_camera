#ifndef _INC_RTS_WAVE_HEADER_H
#define _INC_RTS_WAVE_HEADER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTS_WAVE_HEADER_LENGTH_MAX			500
#define COMPOSE_ID(a, b, c, d)	((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define WAV_RIFF		COMPOSE_ID('R', 'I', 'F', 'F')
#define WAV_WAVE		COMPOSE_ID('W', 'A', 'V', 'E')
#define WAV_FMT			COMPOSE_ID('f', 'm', 't', ' ')
#define WAV_DATA		COMPOSE_ID('d', 'a', 't', 'a')

enum RTS_WAVE_HEADER_STATUS {
	/** invalid pointer reference */
	RTS_WAVE_HEADER_INVALID_REFERENCE		= -4,
	/** invalid arg */
	RTS_WAVE_HEADER_INVALID_ARG			= -3,
	/** out of memory */
	RTS_WAVE_HEADER_NO_MEMORY			= -2,
	/** general error */
	RTS_WAVE_HEADER_FAILURE				= -1,
	/** success */
	RTS_WAVE_HEADER_SUCCESS				=  0,
};

/** "RIFF" chunk */
struct chunk_riff {
	char chunk_riff_id[4]; /** "RIFF" */
	int chunk_riff_size; /** "RIFF" chunk content size */
	char chunk_riff_value[4]; /** "RIFF" chunk format ID */
	/**
	 * If "RIFF" chunk format ID is "WAVE",
	 * then "fmt" sub-chunk and "data" sub-chunk are necessary
	 */
};

/** header of all kind of sub-chunk */
struct subchunk_header {
	char subchunk_id[4];
	/**
	 * current sub-chunk ID
	 * including "fmt", "data", "fact", "LIST", ...
	 * "data" sub-chunk should be the last sub-chunk
	 */
	int subchunk_size; /** current sub-chunk content size */
};

/** "fmt" sub-chunk content */
struct subchunk_fmt {
	short format_tag; /** "0x1" for PCM, "0x6" for ALAW, "0x7" for MULAW */
	short num_channels;
	int sample_rate;
	int bytes_rate;
	short block_align;
	short num_bits;
};

int init_wav_header(FILE *fp, int head_length)
{
	unsigned char *buffer = NULL;

	if (!fp)
		return RTS_WAVE_HEADER_INVALID_REFERENCE;

	buffer = (unsigned char *)calloc(1, head_length);
	if (!buffer)
		return RTS_WAVE_HEADER_NO_MEMORY;

	fwrite(buffer, head_length, 1, fp);

	free(buffer);

	return RTS_WAVE_HEADER_SUCCESS;
}

int fill_wav_header(FILE *fp, struct subchunk_fmt *fmt, int pcmdata_size)
{
	struct chunk_riff riff = {0};
	struct subchunk_header fmt_header = {0};
	struct subchunk_header data_header = {0};
	unsigned int *pt = NULL;

	if (!fp || !fmt)
		return RTS_WAVE_HEADER_INVALID_REFERENCE;

	fseek(fp, 0, SEEK_SET);

	/** "RIFF" chunk */
	pt = (unsigned int *)(riff.chunk_riff_id);
	*pt = WAV_RIFF;
	riff.chunk_riff_size = pcmdata_size +
				sizeof(struct subchunk_header) +
				sizeof(struct subchunk_fmt) +
				sizeof(struct subchunk_header);
	pt = (unsigned int *)(riff.chunk_riff_value);
	*pt = WAV_WAVE;
	fwrite(&riff, sizeof(struct chunk_riff), 1, fp);

	/** header of "fmt" sub-chunk */
	pt = (unsigned int *)(fmt_header.subchunk_id);
	*pt = WAV_FMT;
	fmt_header.subchunk_size = sizeof(struct subchunk_fmt);
	fwrite(&fmt_header, sizeof(struct subchunk_header), 1, fp);

	/** content of "fmt" sub-chunk */
	fwrite(fmt, sizeof(struct subchunk_fmt), 1, fp);

	/** header of "data" sub-chunk */
	pt = (unsigned int *)(data_header.subchunk_id);
	*pt = WAV_DATA;
	data_header.subchunk_size = pcmdata_size;
	fwrite(&data_header, sizeof(struct subchunk_header), 1, fp);

	return RTS_WAVE_HEADER_SUCCESS;
}

int analyze_audio_header(FILE *fp, struct subchunk_fmt *fmt,
			 int *head_length, int *pcmdata_size)
{
	int tmp = 0, id = 0, head_size = 0;
	struct chunk_riff riff = {0};
	struct subchunk_header header = {0};

	if (!fmt || !fp || !head_length || !pcmdata_size)
		return RTS_WAVE_HEADER_INVALID_REFERENCE;

	/** "RIFF" chunk */
	fread(&riff, 1, sizeof(struct chunk_riff), fp);
	id = (unsigned int)(riff.chunk_riff_id[0] & 0xff) |
		((unsigned int)(riff.chunk_riff_id[1] & 0xff) << 8) |
		((unsigned int)(riff.chunk_riff_id[2] & 0xff) << 16) |
		((unsigned int)(riff.chunk_riff_id[3] & 0xff) << 24);
	if (id != WAV_RIFF) {
		printf("riff chunk ID wrong\n");
		return RTS_WAVE_HEADER_INVALID_ARG;
	}
	id = (unsigned int)(riff.chunk_riff_value[0] & 0xff) |
		((unsigned int)(riff.chunk_riff_value[1] & 0xff) << 8) |
		((unsigned int)(riff.chunk_riff_value[2] & 0xff) << 16) |
		((unsigned int)(riff.chunk_riff_value[3] & 0xff) << 24);
	if (id != WAV_WAVE) {
		printf("riff chunk format ID wrong\n");
		return RTS_WAVE_HEADER_INVALID_ARG;
	}
	head_size += sizeof(struct chunk_riff);

	/** "fmt" sub-chunk */
	fread(&header, 1, sizeof(struct subchunk_header), fp);
	id = (unsigned int)(header.subchunk_id[0] & 0xff) |
			((unsigned int)(header.subchunk_id[1] & 0xff) << 8) |
			((unsigned int)(header.subchunk_id[2] & 0xff) << 16) |
			((unsigned int)(header.subchunk_id[3] & 0xff) << 24);
	if (id != WAV_FMT) {
		printf("fmt sub-chunk ID wrong\n");
		return RTS_WAVE_HEADER_INVALID_ARG;
	}
	fread(fmt, 1, sizeof(struct subchunk_fmt), fp);
	tmp = header.subchunk_size - sizeof(struct subchunk_fmt);
	if (tmp)
		fseek(fp, tmp, SEEK_CUR);
	head_size += (sizeof(struct subchunk_header) + header.subchunk_size);

	/** skip all other kind of sub-chunk except "data" sub-chunk */
	while (head_size < RTS_WAVE_HEADER_LENGTH_MAX) {
		fread(&header, 1, sizeof(struct subchunk_header), fp);
		id = (unsigned int)(header.subchunk_id[0] & 0xff) |
			((unsigned int)(header.subchunk_id[1] & 0xff) << 8) |
			((unsigned int)(header.subchunk_id[2] & 0xff) << 16) |
			((unsigned int)(header.subchunk_id[3] & 0xff) << 24);
		if (id == WAV_DATA) {
			head_size += sizeof(struct subchunk_header);
			break;
		}
		fseek(fp, header.subchunk_size, SEEK_CUR);
		head_size += (sizeof(struct subchunk_header) +
				header.subchunk_size);
	}
	if (head_size >= RTS_WAVE_HEADER_LENGTH_MAX) {
		printf("can't find correct subchuck: data\n");
		return RTS_WAVE_HEADER_INVALID_ARG;
	}

	*head_length = head_size;
	*pcmdata_size = header.subchunk_size;

	return RTS_WAVE_HEADER_SUCCESS;
}

#endif /* _INC_RTS_WAVE_HEADER_H */
