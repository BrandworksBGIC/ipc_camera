// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <getopt.h>

#define LOG_LV_DBG	3
#define LOG_LV_INFO	2
#define LOG_LV_WARN	1
#define LOG_LV_ERR	0

static int log_level = LOG_LV_INFO;

#define LOG_PRINT(lv, prefix, fmt, ...) \
	do { \
		if (lv <= log_level) \
			fprintf(stdout, "%s[%s:%d]: "fmt,\
			prefix, __func__, __LINE__, ##__VA_ARGS__);\
	} while (0)

#define LOG_DBG(fmt, ...) LOG_PRINT(LOG_LV_DBG, "dbg", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_PRINT(LOG_LV_INFO, "info", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_PRINT(LOG_LV_WARN, "warn", fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG_PRINT(LOG_LV_ERR, "err", fmt, ##__VA_ARGS__)

#define RTS_OTP_SYS	"/sys/bus/nvmem/devices/rts-otp0/nvmem"

//#define TEST_MODE
#ifdef TEST_MODE
#define GROUP0_BYTES		(32)
#else
#define GROUP0_BYTES		(16)
#endif

#define GROUP_NUMS		(113)
#define GROUP_BYTES		(16)
#define ECC_BYTES		(2)
#define DATA_GROUP_BYTES	(GROUP_BYTES + ECC_BYTES)
#define BYTE_MAX		(GROUP0_BYTES + (GROUP_NUMS - 1) \
				* DATA_GROUP_BYTES)

#define READ			1
#define WRITE			2
#define FLAGS_LENDIAN		0x01
#define FLAGS_ECC		0x02
#define FLAGS_PG		0x04
#define FLAGS_BIT		0x10

#define VAL_OPT_ECC		0x01
#define VAL_OPT_LOGLEVEL	0x02
#define VAL_OPT_BIT		0x03
#define VAL_OPT_PG		0x04

extern int optind;
static struct option longopts[] = {
	{"ecc", no_argument, NULL, VAL_OPT_ECC},
	{"pg", no_argument, NULL, VAL_OPT_PG},
	{"bit", no_argument, NULL, VAL_OPT_BIT},
	{"loglv", required_argument, NULL, VAL_OPT_LOGLEVEL},
};

static void print_help(void)
{
	fprintf(stdout, "Usage: otp_mfg <-r|w> [[[-l] [--ecc]] | [--bit]]...\n"
			"-r [offset] [bytes]	read data\n"
			"-w <XX> [offset]	write data\n"
			"			XX:data to be written\n"
			"-l			reverse byte order for otp aes-key\n"
			"--loglv <lv>		0:error 1:warn 2:info 3:debug  default: 2\n"
			"only for write:\n"
			"--ecc			auto-fill ecc\n"
			"only for read:\n"
			"--pg			grouping print\n"
			"can't be used with other flags:\n"
			"--bit			read/write bits, can't be used with flags [-l|--ecc|--pg]\n"
			"			-r <offset> [bits]\n"
			"			-w <bits> <offset>\n"
			"\n"
			"OTP:\n"
			"- support 2032 bytes\n"
			"- support 113 groups\n"
			"- group0 = 16 bytes, no ecc bytes\n"
			"- group[2n-1,2n] = (16+16) bytes data + (2+2) bytes ecc, n=[1,56]\n");
}

unsigned int gc_next(unsigned int din)
{
	unsigned int dout;
	unsigned int din_7 = din/128;

	dout = ((din%128) << 1) ^ (din_7 ? 29 : 0);
	return dout;
}

unsigned int mult(unsigned int din, unsigned int k)
{
	unsigned int expo = 1;
	unsigned int dout = 0;
	unsigned int din_shift = din;
	int i;

	for (i = 0; i < k; i++)
		expo = gc_next(expo);

	for (i = 0; i < 8; i++) {
		dout = dout ^ ((din_shift%2) ? expo : 0);
		expo = gc_next(expo);
		din_shift = din_shift >> 1;
	}

	LOG_DBG("%d\n", dout);
	return dout;
}

unsigned int calculate_ecc(const uint8_t din[], uint8_t ecc[])
{
	unsigned int p0[16], p1[16];
	int i;
	unsigned int par0 = 0;
	unsigned int par1 = 0;

	p0[0] = mult(din[0], 43);
	p0[1] = mult(din[1], 120);
	p0[2] = mult(din[2], 8);
	p0[3] = mult(din[3], 199);
	p0[4] = mult(din[4], 74);
	p0[5] = mult(din[5], 102);
	p0[6] = mult(din[6], 220);
	p0[7] = mult(din[7], 251);
	p0[8] = mult(din[8], 95);
	p0[9] = mult(din[9], 175);
	p0[10] = mult(din[10], 87);
	p0[11] = mult(din[11], 166);
	p0[12] = mult(din[12], 113);
	p0[13] = mult(din[13], 75);
	p0[14] = mult(din[14], 198);
	p0[15] = mult(din[15], 25);

	p1[0] = mult(din[0], 121);
	p1[1] = mult(din[1], 9);
	p1[2] = mult(din[2], 200);
	p1[3] = mult(din[3], 75);
	p1[4] = mult(din[4], 103);
	p1[5] = mult(din[5], 221);
	p1[6] = mult(din[6], 252);
	p1[7] = mult(din[7], 96);
	p1[8] = mult(din[8], 176);
	p1[9] = mult(din[9], 88);
	p1[10] = mult(din[10], 167);
	p1[11] = mult(din[11], 114);
	p1[12] = mult(din[12], 76);
	p1[13] = mult(din[13], 199);
	p1[14] = mult(din[14], 26);
	p1[15] = mult(din[15], 1);

	for (i = 0; i < 16; i++) {
		par0 = par0 ^ p0[i];
		par1 = par1 ^ p1[i];
	}

	LOG_DBG("par0:\n0x%x\npar1:\n0x%x\n", par0, par1);

	ecc[0] = (uint8_t)par0;
	ecc[1] = (uint8_t)par1;

	return 0;
}

static int  _otp_read(int fd, uint8_t *buf, int bytes, int offset)
{
	int ret;

	if (!fd || !buf)
		return -1;

	LOG_DBG("bytes=%d, offset=%d\n", bytes, offset);
	ret = lseek(fd, offset, SEEK_SET);
	if (ret == -1) {
		printf("lseek failed, errno = %d\n", errno);
		return -1;
	}

	ret = read(fd, buf, bytes);
	if (ret == -1) {
		printf("read failed, errno = %d\n", errno);
		return -1;
	}

	return 0;
}

static int  _otp_write(int fd, uint8_t *buf, int bytes, int offset)
{
	int ret;

	LOG_DBG("bytes=%d, offset=%d\n", bytes, offset);
	if (!fd || !buf)
		return -1;

	ret = lseek(fd, offset, SEEK_SET);
	if (ret == -1) {
		printf("lseek failed, errno = %d\n", errno);
		return -1;
	}

	ret = write(fd, buf, bytes);
	if (ret == -1) {
		printf("write failed, errno = %d\n", errno);
		return -1;
	}

	return 0;
}

static int otp_write_group(int fd, uint8_t *buf, int bytes,
			int group, int offset)
{
	int ret;

	LOG_DBG("\n");
	uint8_t g_buf[GROUP0_BYTES] = {0};
	uint8_t e_buf[ECC_BYTES] = {0};

	memcpy(&g_buf[offset], buf, bytes);

	/* group 0 */
	if (!group)
		return _otp_write(fd, g_buf, GROUP0_BYTES, 0);

	/* group 1-112 */
	offset = GROUP0_BYTES + GROUP_BYTES * (group - 1) +
			(group - 1) / 2 * ECC_BYTES * 2;
	ret =  _otp_write(fd, g_buf, GROUP_BYTES, offset);
	if (ret)
		return ret;

	/* ecc */
	calculate_ecc(g_buf, e_buf);
	offset += GROUP_BYTES * (group % 2 + 1) +
				(group + 1) % 2 * ECC_BYTES;
	return _otp_write(fd, e_buf, ECC_BYTES, offset);
}

static int otp_write_with_ecc(int fd, uint8_t *buf, int bytes, int offset)
{
	int len, g, f, ret = 0;

	LOG_DBG("bytes = %d, offset = %d\n", bytes, offset);
	if (offset < GROUP0_BYTES) {
		g = 0;
		f = offset;
	} else {
		g = (offset - GROUP0_BYTES) / GROUP_BYTES + 1;
		f = (offset - GROUP0_BYTES) % GROUP_BYTES;
	}

	/* write group */
	while (bytes) {
		if (!g)
			len = GROUP0_BYTES - f;
		else
			len = GROUP_BYTES - f;
		len = bytes < len ? bytes : len;

		LOG_DBG("g = %d, f = %d, len = %d\n", g, f, len);
		ret = otp_write_group(fd, buf, len, g, f);
		if (ret)
			return ret;

		g++;
		f = 0;
		bytes -= len;
		buf += len;
	}

	return ret;
}


static int parse_read_byte_params(int argc, char *argv[],
			int *bytes, int *offset)
{
	LOG_DBG("\n");
	if (!argc) {
		*offset = 0;
		*bytes = BYTE_MAX;
	} else if (argc == 1 || argc == 2) {
		*offset = atoi(argv[0]);
		if (*offset < 0 || *offset > BYTE_MAX - 1) {
			fprintf(stdout, "offset = %d invalid\n", *offset);
			return -1;
		}

		if (argc == 1)
			*bytes = BYTE_MAX - *offset;
		else
			*bytes = atoi(argv[1]);

		if (*bytes < 1 || *bytes > BYTE_MAX - *offset) {
			fprintf(stdout, "bytes = %d invalid\n", *bytes);
			return -1;
		}
	} else {
		print_help();
		return -1;
	}

	return 0;
}

static int parse_write_byte_params(int argc, char *argv[], int flags,
			int *bytes, int *offset, uint8_t *buf)
{
	int i, len;
	char t[3] = {0};

	LOG_DBG("\n");
	if (argc == 1 || argc == 2) {
		len = strlen(argv[0]);
		if (!len || (len % 2) || len / 2 > BYTE_MAX) {
			fprintf(stdout, "data invalid, len = %d\n", len);
			return -1;
		}

		*bytes = len / 2;

		for (i = 0; i < *bytes; i++) {
			if (flags & FLAGS_LENDIAN)
				buf[i] = (uint8_t)strtoul
					(strncpy(t, argv[0] +
					len - (i + 1) * 2, 2), NULL, 16);
			else
				buf[i] = (uint8_t)strtoul
					(strncpy(t, argv[0] +
					i * 2, 2), NULL, 16);
		}

		if (argc == 1)
			*offset = 0;
		else
			*offset = atoi(argv[1]);

		if (*offset < 0 || *offset > BYTE_MAX - *bytes) {
			fprintf(stdout, "offset = %d invalid\n", *offset);
			return -1;
		}
	} else {
		print_help();
		return -1;
	}

	return 0;
}

static int parse_read_bit_params(int argc, char *argv[],
			int *bytes, int *offset, int *bits, int *offset_bit)
{
	LOG_DBG("\n");
	if (argc == 2) {
		*offset_bit = atoi(argv[0]);
		if (*offset_bit < 0 || *offset_bit > (BYTE_MAX << 3) - 1) {
			fprintf(stdout, "offset_bit = %d invalid\n",
						*offset_bit);
			return -1;
		}

		*bits = atoi(argv[1]);
		if (*bits < 1 || *bits > (BYTE_MAX << 3) - *offset_bit) {
			fprintf(stdout, "bits = %d invalid\n", *bits);
			return -1;
		}

		*offset = *offset_bit >> 3;
		*bytes = ((*bits - 1) >> 3) + 1;
	} else {
		print_help();
		return -1;
	}

	return 0;
}

static int parse_write_bit_params(int argc, char *argv[],
			int *bytes, int *offset, uint8_t *buf)
{
	int i, j, k, oft;
	int bits, offset_bit;

	LOG_DBG("\n");
	if (argc == 2) {
		bits = strlen(argv[0]);
		if (bits < 1 || bits > BYTE_MAX << 3) {
			fprintf(stdout, "bits = %d invalid\n", bits);
			return -1;
		}

		offset_bit = atoi(argv[1]);
		if (offset_bit < 0 || offset_bit > (BYTE_MAX << 3) - bits) {
			fprintf(stdout, "offset_bit = %d invalid\n",
						offset_bit);
			return -1;
		}

		*bytes = ((bits - 1) >> 3) + 1;
		*offset = offset_bit >> 3;

		memset(buf, 0xff, *bytes);
		oft = offset_bit % 8;
		for (i = 0; i < bits; i++) {
			j = (i + oft) / 8;
			k = (i + oft) % 8;
			buf[j] &= atoi(argv[0] + i) ? 0xff : ~(1 << k);
		}

	} else {
		print_help();
		return -1;
	}

	return 0;
}

static void otp_printf_byte(int offset, int bytes, uint8_t *buf, int flags)
{
	int i, g;

	LOG_DBG("\n");

	if (offset < GROUP0_BYTES)
		g = 0;
	else
		g = (offset - GROUP0_BYTES) / (2 * DATA_GROUP_BYTES) + 1;

	if (flags & FLAGS_PG) {
		if (offset < GROUP0_BYTES)
			fprintf(stdout, "group000-000(%04d): ", offset);
		else
			fprintf(stdout, "group%03d-%03d(%04d): ",
						2 * g - 1, 2 * g, offset);
		g++;
	}

	for (i = offset; i < offset + bytes; i++) {
		/* grouping print */
		if (flags & FLAGS_PG && i != offset) {
			if (i == GROUP0_BYTES) {
				fprintf(stdout, "\ngroup001-002(%04d): ",
							GROUP0_BYTES);
				g++;
			} else if ((i > GROUP0_BYTES) &&
					!((i - GROUP0_BYTES) %
						(2 * DATA_GROUP_BYTES))) {
				fprintf(stdout, "\ngroup%03d-%03d(%04d): ",
							2 * g - 1, 2 * g, i);
				g++;
			}
		} else if (i != offset) {
			if (!((i - offset) % 16))
				fprintf(stdout, "\n");
		}

		if (flags & FLAGS_LENDIAN)
			fprintf(stdout, "%02x ", buf[bytes - offset - i - 1]);
		else
			fprintf(stdout, "%02x ", buf[i - offset]);
	}

	fprintf(stdout, "\n");
}

static void otp_printf_bit(int bits, int offset_bit,  uint8_t *buf)
{
	int i, j, k;

	LOG_DBG("\n");
	offset_bit %= 8;

	for (i = 0; i < bits; i++) {
		j = (i + offset_bit) / 8;
		k = (i + offset_bit) % 8;
		fprintf(stdout, "%d", buf[j] >> k & 0x01);
	}

	fprintf(stdout, "\n");
}

static int otp_read(int fd, int argc, char *argv[], int flags)
{
	int ret;
	int bytes = 0, offset = 0, bits = 0, offset_bit = 0;
	uint8_t buf[BYTE_MAX + 1] = {0};

	LOG_DBG("\n");
	/* parse */
	if (flags & FLAGS_BIT)
		ret = parse_read_bit_params(argc, argv,
				&bytes, &offset, &bits, &offset_bit);
	else
		ret = parse_read_byte_params(argc, argv, &bytes, &offset);
	if (ret)
		return ret;

	/* read */
	memset(buf, 0, sizeof(buf));
	ret = _otp_read(fd, buf, bytes, offset);
	if (ret)
		return ret;

	/* printf */
	if (flags & FLAGS_BIT)
		otp_printf_bit(bits, offset_bit, buf);
	else
		otp_printf_byte(offset, bytes, buf, flags);

	return 0;
}

static int otp_write(int fd, int argc, char *argv[], int flags)
{
	int ret;
	int bytes, offset;
	uint8_t buf[BYTE_MAX + 1] = {0};

	LOG_DBG("\n");
	/* parse */
	if (flags & FLAGS_BIT)
		ret = parse_write_bit_params(argc, argv,
					&bytes, &offset, buf);
	else
		ret = parse_write_byte_params(argc, argv, flags,
					&bytes, &offset, buf);
	if (ret)
		return ret;

	/* write */
	if (flags & FLAGS_BIT) {
		ret = _otp_write(fd, buf, bytes, offset);
	} else {
		if (flags & FLAGS_ECC)
			ret = otp_write_with_ecc(fd, buf, bytes, offset);
		else
			ret = _otp_write(fd, buf, bytes, offset);
	}

	LOG_DBG("\n");
	return ret;
}

static int check_flags(int mode, int flags, int flag)
{
	if (flag != FLAGS_BIT && flags & FLAGS_BIT)
		return 0;

	switch (flag) {
	case FLAGS_LENDIAN:
		break;
	case FLAGS_ECC:
		if (mode != WRITE)
			return 0;
		break;
	case FLAGS_PG:
		if (mode != READ)
			return 0;
		break;
	case FLAGS_BIT:
		if (flags & ~FLAGS_BIT)
			return 0;
		break;
	default:
		return 0;
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int fd;
	int c, retval;
	int (*funcptr)(int, int, char **, int) = NULL;
	int cmd_cnt = 0;
	int flags = 0, mode = READ;

	optind = 0;
	while ((c = getopt_long(argc, argv, "rwl", longopts,
						NULL)) != -1) {
		switch (c) {
		case 'r':
			cmd_cnt++;
			funcptr = otp_read;
			mode = READ;
			break;
		case 'w':
			cmd_cnt++;
			funcptr = otp_write;
			mode = WRITE;
			break;
		case 'l':
			if (!check_flags(mode, flags, FLAGS_LENDIAN))
				goto def;

			flags |= FLAGS_LENDIAN;
			break;
		case VAL_OPT_ECC:
			if (!check_flags(mode, flags, FLAGS_ECC))
				goto def;

			flags |= FLAGS_ECC;
			break;
		case VAL_OPT_PG:
			if (!check_flags(mode, flags, FLAGS_PG))
				goto def;

			flags |= FLAGS_PG;
			break;
		case VAL_OPT_BIT:
			if (!check_flags(mode, flags, FLAGS_BIT))
				goto def;

			flags |= FLAGS_BIT;
			break;
		case VAL_OPT_LOGLEVEL:
			log_level = atoi(optarg);
			break;
		case '?':
		default:
def:
			print_help();
			exit(EXIT_FAILURE);
			break;
		}
	}

	if (cmd_cnt != 1 || !funcptr) {
		print_help();
		exit(EXIT_FAILURE);
	}

	fd = open(RTS_OTP_SYS, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Open device fail!\n");
		exit(EXIT_FAILURE);
	}

	retval = funcptr(fd, argc - optind, argv + optind, flags);
	if (retval < 0) {
		fprintf(stderr, "otp_mfg fail\n");
		(void)close(fd);
		exit(EXIT_FAILURE);
	}

	(void)close(fd);

	return 0;


}
