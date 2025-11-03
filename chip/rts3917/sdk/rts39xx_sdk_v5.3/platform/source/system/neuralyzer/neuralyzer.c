/*
 * neuralyzer.c
 *
 * Copyright(C) 2015 Micky Ching, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <getopt.h>
#include <neuralyzer.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"check", no_argument, NULL, 'c'},
	{0, 0, 0, 0},
};

#define VERSION "0.1"

static int check;

static void print_usage(void)
{
	fprintf(stdout, "neuralyzer usage:\n");
	fprintf(stdout, "\t-c | --check check image only\n");
	fprintf(stdout, "\t-h | --help  show this help and exit\n");
}

static int parse_argv(int argc, char **argv)
{
	int ret = 0;
	int c;

	while ((c = getopt_long(argc, argv, "-:c:h", longopts, NULL)) != -1) {
		switch (c) {
		case 'c':
			check = 1;
			break;
		case 'h':
			print_usage();
			break;
		case '?':
			fprintf(stdout, "%c invalid argument\n", optopt);
			ret = -1;
			break;
		default:
			break;
		}
	}

	return ret;
}

int main(int argc, char **argv)
{
	struct neu_data *data;
	int ret;

	if (argc != 2 && argc != 3) {
		printf("Usage: neuralyzer <filename> or neuralyzer -c <filename>\n");
		return -1;
	}

	ret = parse_argv(argc, argv);
	if (ret)
		return -1;

	data = neu_data_alloc(argv[argc-1]);
	if (!data)
		return -1;

	ret = global_check(data->mtd_desc);
	if (ret < 0) {
		printf("please set global partition as mtd0!\n");
		goto failed;
	}

	if (check)
		ret = neu_check(data);
	else
		ret = neu_burn(data);
	if (ret)
		printf("neuralyzer %s %s\n", argv[1],
			neu_burn_pass(data) ? "pass" : "failed");

failed:
	neu_data_free(data);
	return ret;
}
