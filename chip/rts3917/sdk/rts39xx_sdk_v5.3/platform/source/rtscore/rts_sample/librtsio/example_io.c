/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "rts_io_i2c.h"
#include "rts_io_gpio.h"
#include "rts_io_adc.h"

static int value, func_num, val_index, io_type, i2c_nr, bit_mask, slave_addr;
static int count = 1, reg_len = 1, addr_type = 1;
static char *bulk_value;

static struct option longopts[] = {
	{"help", 0, NULL, 'h'},
	{"iotype", 1, NULL, 't'},
	{"func", 1, NULL, 'f'},
	{"slave_addr", 1, NULL, 's'},
	{"addr_type", 1, NULL, 'a'},
	{"i2c_nr", 1, NULL, 'd'},
	{"gpio", 1, NULL, 'i'},
	{"adc_channel", 1, NULL, 'i'},
	{"reg", 1, NULL, 'i'},
	{"reg_len", 1, NULL, 'l'},
	{"value", 1, NULL, 'v'},
	{"bit_mask", 1, NULL, 'b'},
};

void printf_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "-h --help\n");
	fprintf(stream, "-t --iotype |1:i2c |2:gpio |3:adc\n");
	fprintf(stream, "-f --func\n");
	fprintf(stream, "   0: rts_io_i2c_write     |rts_io_gpio_request\n");
	fprintf(stream, "   1: rts_io_i2c_read      |rts_io_gpio_requested\n");
	fprintf(stream, "   2: rts_io_i2c_byte_write\n");
	fprintf(stream, "   3: rts_io_i2c_byte_read\n");
	fprintf(stream, "   4: rts_io_i2c_set_bits\n");
	fprintf(stream, "   5: rts_io_i2c_clc_bits\n");
	fprintf(stream, "   6: rts_io_i2c_bulk_write\n");
	fprintf(stream, "   7: rts_io_i2c_bulk_read\n");
	fprintf(stream, "   8: rts_io_i2c_update_bits\n");
	fprintf(stream, "-d --i2c_nr i2c bus number\n");
	fprintf(stream, "-s --slave_addr chip addr 16Hex\n");
	fprintf(stream, "-a --addr_type 1:SEVEN(default) 2:TEN\n");
	fprintf(stream,
		"-i --reg i2c_reg |--gpio gpio_num |--adc_channel|(16Hex)\n");
	fprintf(stream, "-l --reg_len register len(default 1)\n");
	fprintf(stream, "-v --value i2c_value 16Hex\n");
	fprintf(stream, "-c --count i2c value count(default 1)\n");
	fprintf(stream, "-b --bit_mask bit_mask 16Hex\n");

	exit(exit_code);
}

static int parse_arg(int argc, char **argv)
{
	int ret = 0;

	while ((ret =
		getopt_long(argc, argv, "t:f:s:a:d:i:l:v:c:b:h",
			longopts, NULL)) != -1) {
		switch (ret) {
		case 't':
			io_type = strtoul(optarg, NULL, 10);
			break;
		case 'f':
			func_num = strtoul(optarg, NULL, 10);
			break;
		case 's':
			slave_addr = strtol(optarg, NULL, 16);
			break;
		case 'a':
			addr_type = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			i2c_nr = strtol(optarg, NULL, 10);
			break;
		case 'i':
			val_index = strtoul(optarg, NULL, 16);
			break;
		case 'l':
			reg_len = strtol(optarg, NULL, 10);
			break;
		case 'v':
			bulk_value = optarg;
			value = strtoul(optarg, NULL, 16);
			break;
		case 'c':
			count = strtoul(optarg, NULL, 10);
			break;
		case 'b':
			bit_mask = strtol(optarg, NULL, 16);
			break;
		case 'h':
			printf_usage(stdout, 1);
			break;
		case ':':
			printf("%c require argument\n", optopt);
			break;
		case '?':
			fprintf(stderr, "%c invalid argument\n", optopt);
			break;
		default:
			printf_usage(stdout, 1);
			break;
		}
	}

	return 0;
}

static int rts_i2c_func_example(int func_num)
{
	int ret = 0;
	int i;
	uint8_t *bulk;
	char tmp[3];

	printf("reg = 0x%x, value = 0x%x, bit_mask = 0x%x\n",
			val_index, value, bit_mask);

	if (count > 256)
		return -EINVAL;
	else if (count == 0)
		count = 1;

	bulk = calloc(1, count);
	if (!bulk)
		return -ENOMEM;

	if (!(func_num % 2) && bulk_value != NULL) {
		for (i = 0; i < count; i++) {
			tmp[0] = bulk_value[2 * i];
			tmp[1] = bulk_value[2 * i + 1];
			tmp[2] = '\0';
			bulk[i] = strtoul(tmp, NULL, 16);
		}
	}

	for (i = 0; i < count; i++)
		printf(" bulk[%d] = 0x%x;", i, bulk[i]);
	printf("\n");

	switch (func_num) {
	case 0:
		ret = rts_io_i2c_write(
			i2c_nr, slave_addr, addr_type, count, bulk);
		break;
	case 1:
		ret = rts_io_i2c_read(
			i2c_nr, slave_addr, addr_type, count, bulk);
		value = bulk[0];
		break;
	case 2:
		ret = rts_io_i2c_byte_write(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, (uint8_t)value);
		break;
	case 3:
		ret = rts_io_i2c_byte_read(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, (uint8_t *)&value);
		break;
	case 4:
		ret = rts_io_i2c_set_bits(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, (uint8_t)bit_mask);
		break;
	case 5:
		ret = rts_io_i2c_clr_bits(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, (uint8_t)bit_mask);
		break;
	case 6:
		ret = rts_io_i2c_bulk_write(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, count, bulk);
		break;
	case 7:
		ret = rts_io_i2c_bulk_read(i2c_nr, slave_addr, addr_type,
				val_index, reg_len, count, bulk);
		break;
	case 8:
		ret = rts_io_i2c_update_bits(i2c_nr, slave_addr, addr_type,
				val_index, reg_len,
				(uint8_t)value, (uint8_t)bit_mask);
		break;
	default:
		break;
	}
	printf("reg = 0x%x, value = 0x%x, bit_mask=0x%x,ret = %d\n",
			val_index, value, bit_mask, ret);
	for (i = 0; i < count; i++)
		printf(" bulk[%d] = 0x%x;", i, bulk[i]);
	printf("\n");
	free(bulk);

	return ret;
}

static int rts_gpio_func_example(int func_num)
{
	int ret;
	int flag = 0;
	struct rts_gpio *rts_gpio;

	if (func_num == 1) {
		ret = rts_io_gpio_requested(0, val_index);
		if (ret == GPIO_REQUESTED)
			printf("gpio-%d has be requested\n", val_index);
		else if (ret == GPIO_NOT_REQUESTED)
			printf("gpio-%d is not requested\n", val_index);
		return ret;
	}

	rts_gpio = rts_io_gpio_request(0, val_index);
	if (!rts_gpio)
		return -EIO;

	while (!flag) {
		printf("please enter the function number\n");
		printf("       1 rts_io_gpio_free\n");
		printf("       2 rts_io_gpio_set_value\n");
		printf("       3 rts_io_gpio_get_value\n");
		printf("       4 rts_io_gpio_set_direction\n");
		printf("       5 rts_io_gpio_get_direction\n");
		printf("       6 rts_io_gpio_set_pull\n");
		printf("       7 rts_io_gpio_get_pull\n");
		printf("       8 exit\n");

		ret = scanf("%d", &func_num);
		if (ret != 1)
			printf("scanf erro %d\n", ret);
		if (getchar() < 0)
			printf("getchar erro\n");

		if (2 == func_num || 4 == func_num || 6 == func_num) {
			printf("please enter the value :\n");
			ret = scanf("%d", &value);
			if (ret != 1)
				printf("scanf erro %d\n", ret);
			if (getchar() < 0)
				printf("getchar erro\n");
		}

		switch (func_num) {
		case 0:
			break;
		case 1:
			ret = rts_io_gpio_free(rts_gpio);
			flag = 1;
			break;
		case 2:
			ret = rts_io_gpio_set_value(rts_gpio, value);
			if (ret)
				printf("rts_io_gpio_set_value err ret = %d\n",
							ret);
			break;
		case 3:
			ret = rts_io_gpio_get_value(rts_gpio);
			if (ret < 0)
				printf(
					"rts_io_gpio_get_value failed ret = %d\n",
							ret);
			else
				printf("gpio%d's value =%d\n",
							rts_gpio->gpio, ret);
			break;
		case 4:
			ret = rts_io_gpio_set_direction(rts_gpio, value);
			if (ret)
				printf("rts_io_gpio_set_direction err %d\n",
							ret);
			break;
		case 5:
			ret = rts_io_gpio_get_direction(rts_gpio);
			if (ret < 0)
				printf(
					"rts_io_gpio_get_direction failed ret = %d\n",
							ret);
			else
				printf("gpio%d's direction =%d\n",
							rts_gpio->gpio, ret);
			break;
		case 6:
			ret = rts_io_gpio_set_pull(rts_gpio, value);
			if (ret)
				printf("rts_io_gpio_set_pull err ret = %d\n",
							ret);
			break;
		case 7:
			ret = rts_io_gpio_get_pull(rts_gpio);
			if (ret < 0)
				printf("rts_io_gpio_get_pull failed ret = %d\n",
							ret);
			else
				printf("gpio%d's pull value =%d\n",
							rts_gpio->gpio, ret);
			break;
		case 8:
			rts_io_gpio_free(rts_gpio);
			flag = 1;
			break;
		default:
			break;
		}
	}
	return ret;
}

int main(int argc, char **argv)
{
	int ret;

	parse_arg(argc, argv);

	switch (io_type) {
	case 1:
		ret = rts_i2c_func_example(func_num);
		if (ret < 0)
			printf("i2c transfer failed ret = %d\n", ret);
		break;
	case 2:
		ret = rts_gpio_func_example(func_num);
		if (ret < 0)
			printf("gpio failed\n");
		break;
	case 3:
		ret = rts_io_adc_get_value(val_index);
		if (ret < 0)
			printf("rts_io_adc_get_value failed ret = %d\n", ret);
		else
			printf("the value of adc channel%d is %d\n",
						val_index, ret);
		break;
	default:
		break;
	}

	return 0;
}
