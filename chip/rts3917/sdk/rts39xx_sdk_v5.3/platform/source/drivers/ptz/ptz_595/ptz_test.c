#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <getopt.h>
#include "ptz_usr.h"

int motor_init(void)
{
	return open("/dev/rts-ptz", O_RDWR);
}

void motor_release(int fd)
{
	close(fd);
}

struct option longopts[] = {

	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static void print_help_info(char *name)
{
	printf("DESCRIPTION:\n");
	printf("\texample for ptz test\n");
	printf("USAGE:\n");
	printf("1. ptz_test rotate <x-dir> <y-dir> <speed> <x-steps> <y-steps>\n");
	printf("\texample: ptz_test rotate x-right y-up high 100 10\n");
	printf("\tdescribe: from current position, ptz going 100 steps to x-right and 10 steps y-up\n");
	printf("2. ptz_test pos <x-steps-position> <y-steps-position>\n");
	printf("\texample: ptz_test pos 500 50\n");
	printf("\tdescribe: change current position to x-position (500 step) and y-position (50 step)\n");
	printf("3. ptz_test reset\n");
	printf("\tdescribe: ptz adjust to fov center\n");
	printf("4. ptz info: ptz_test info\n");
	printf("\tdescribe: show ptz current info\n");
	printf("5. ptz_test run <x-dir> <y-dir> <speed>\n");
	printf("\texample: run x-right y-up low\n");
	printf("\tdescribe: ptz x-dir and y-dir runs to end at a low speed\n");
}

int main(int argc, char *argv[])
{
	int fd, c, ret;
	struct ptzctrl_info ptz_info;
	unsigned int xrun_steps = 0, yrun_steps = 0;
	unsigned int xpos = 0, ypos = 0;

	if (argc < 2) {
		print_help_info(argv[0]);
		return 0;
	}

	while ((c = getopt_long(argc, argv,
				":h", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info(argv[0]);
			return 0;
		case '?':
			printf("invalid param: -%c\n", optopt);
			break;
		}
	}

	fd = motor_init();

	if (strcmp(argv[1], "rotate") == 0) {
		if (argc != 7)
			goto err;
		if (strcmp(argv[2], "x-right") == 0)
			ptz_info.xmotor_info.dir = DIR_RIGHT;
		else if (strcmp(argv[2], "x-left") == 0)
			ptz_info.xmotor_info.dir = DIR_LEFT;
		if (strcmp(argv[3], "y-up") == 0)
			ptz_info.ymotor_info.dir = DIR_UP;
		else if (strcmp(argv[3], "y-down") == 0)
			ptz_info.ymotor_info.dir = DIR_DOWN;
		if (strcmp(argv[4], "low") == 0) {
			ptz_info.xmotor_info.speed = SPEED_LOW;
			ptz_info.ymotor_info.speed = SPEED_LOW;
		} else if (strcmp(argv[4], "high") == 0) {
			ptz_info.xmotor_info.speed = SPEED_HIGH;
			ptz_info.ymotor_info.speed = SPEED_HIGH;
		} else {
			ptz_info.xmotor_info.speed = SPEED_NORMAL;
			ptz_info.ymotor_info.speed = SPEED_NORMAL;
		}
		xrun_steps = strtol(argv[5], NULL, 10);
		yrun_steps = strtol(argv[6], NULL, 10);
		ptz_info.xmotor_info.steps = xrun_steps;
		ptz_info.ymotor_info.steps = yrun_steps;
		ptz_info.block = true;
		printf("rotate test\n");
		ioctl(fd, RTS_PTZ_IOC_DRIVE, &ptz_info);
	} else if (strcmp(argv[1], "pos") == 0) {
		if (argc != 4)
			goto err;
		xpos = strtol(argv[2], NULL, 10);
		ypos = strtol(argv[3], NULL, 10);
		ptz_info.xmotor_info.pos = xpos;
		ptz_info.ymotor_info.pos = ypos;
		ptz_info.xmotor_info.speed = SPEED_HIGH;
		ptz_info.ymotor_info.speed = SPEED_HIGH;
		printf("set position test\n");
		printf("x is %d, y is %d\n", xpos, ypos);
		ptz_info.block = true;
		ioctl(fd, RTS_PTZ_IOC_S_POS, &ptz_info);
	} else if (strcmp(argv[1], "reset") == 0) {
		printf("reset test\n ");
		ioctl(fd, RTS_PTZ_IOC_RESET, NULL);
	} else if (strcmp(argv[1], "info") == 0) {
		ioctl(fd, RTS_PTZ_IOC_G_INFO, &ptz_info);
		printf("get ptz information test\n");
		printf("ptz info: \n");
		printf("xmotor:dir[%d], speed[%d], pos[%d], ",
				ptz_info.xmotor_info.dir,
				ptz_info.xmotor_info.speed,
				ptz_info.xmotor_info.pos);
		printf("is_running[%d], max_steps[%d], ",
				ptz_info.xmotor_info.is_running,
				ptz_info.xmotor_info.max_steps);
		printf("max_degrees[%d]\n",
				ptz_info.xmotor_info.max_degrees);
		printf("ymotor:dir[%d], speed[%d], pos[%d], ",
				ptz_info.ymotor_info.dir,
				ptz_info.ymotor_info.speed,
				ptz_info.ymotor_info.pos);
		printf("is_running[%d], max_steps[%d], ",
				ptz_info.ymotor_info.is_running,
				ptz_info.ymotor_info.max_steps);
		printf("max_degrees[%d]\n",
				ptz_info.ymotor_info.max_degrees);
	} else if (strcmp(argv[1], "run") == 0) {
		if (argc != 5)
			goto err;
		if (strcmp(argv[2], "x-right") == 0)
			ptz_info.xmotor_info.dir = DIR_RIGHT;
		else if (strcmp(argv[2], "x-left") == 0)
			ptz_info.xmotor_info.dir = DIR_LEFT;
		if (strcmp(argv[3], "y-up") == 0)
			ptz_info.ymotor_info.dir = DIR_UP;
		else if (strcmp(argv[3], "y-down") == 0)
			ptz_info.ymotor_info.dir = DIR_DOWN;
		if (strcmp(argv[4], "low") == 0) {
			ptz_info.xmotor_info.speed = SPEED_LOW;
			ptz_info.ymotor_info.speed = SPEED_LOW;
		} else if (strcmp(argv[4], "high") == 0) {
			ptz_info.xmotor_info.speed = SPEED_HIGH;
			ptz_info.ymotor_info.speed = SPEED_HIGH;
		} else {
			ptz_info.xmotor_info.speed = SPEED_NORMAL;
			ptz_info.ymotor_info.speed = SPEED_NORMAL;
		}
		printf("run test\n");
		ptz_info.block = false;
		ioctl(fd, RTS_PTZ_IOC_RUN, &ptz_info);
		sleep(5);
		printf("stop run\n");
		ioctl(fd, RTS_PTZ_IOC_STOP, NULL);
	}

	printf("wait to quit\n");
	motor_release(fd);

	return 0;

err:
	motor_release(fd);
	print_help_info(argv[0]);
	return ret;
}
