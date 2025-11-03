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
	return open("/dev/rts_motor", O_RDWR);
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
	printf("\texample for ptz_gpio test\n");
	printf("USAGE:\n");
	printf("1.ptz_test degree <x-position> <speed> <degree>\n");
	printf("\texample: ptz_test degree x-right high 30\n");
	printf("\tdescribe: ptz rotate to x-right direction 30 degree\n");
	printf("2. ptz_test rotate <x-dir> <speed> <time>\n");
	printf("\texample: ptz_test rotate x-right high 10\n");
	printf("\tdescribe: ptz will go to x-right 10 with high speed\n");
	printf("3. ptz_test pos\n");
	printf("\texample: ptz_test pos\n");
	printf("\tdescribe: get current position x-position and y-position\n");
	printf("4. ptz_test stop\n");
	printf("\texample: ptz_test stop\n");
	printf("\tdescribe: ptz stop run\n");
	printf("5. ptz_test status\n");
	printf("\texample: ptz_test status\n");
	printf("\tdescribe: get ptz_pgio status\n");
}

int main(int argc, char *argv[])
{
	int fd, c, ret;
	struct motorctrl_info m_info;

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
		unsigned int run_time = 0;

		if (argc != 5)
			goto err;
		if (strcmp(argv[2], "x-right") == 0)
			m_info.dir = DIR_RIGHT;
		else if (strcmp(argv[2], "x-left") == 0)
			m_info.dir = DIR_LEFT;
		else if (strcmp(argv[2], "y-up") == 0)
			m_info.dir = DIR_UP;
		else if (strcmp(argv[2], "y-down") == 0)
			m_info.dir = DIR_DOWN;
		if (strcmp(argv[3], "low") == 0)
			m_info.speed = SPEED_LOW;
		else if (strcmp(argv[3], "high") == 0)
			m_info.speed = SPEED_HIGH;
		else
			m_info.speed = SPEED_NORMAL;
		run_time = strtol(argv[4], NULL, 10);
		m_info.run_time = run_time;
		ioctl(fd, RTS_RUN_MOTOR_TIME_CTL, &m_info);
	} else if (strcmp(argv[1], "pos") == 0) {
		ioctl(fd, RTS_GET_MOTOR_POS_CTL, &m_info);
		printf(" x is %d, y is %d\n", m_info.x, m_info.y);
	} else if (strcmp(argv[1], "stop") == 0) {
		ioctl(fd, RTS_STOP_MOTOR_CTL, &m_info);
	} else if (strcmp(argv[1], "degree") == 0) {
		if (argc != 5)
			goto err;
		if (strcmp(argv[2], "x-right") == 0)
			m_info.dir = DIR_RIGHT;
		else if (strcmp(argv[2], "x-left") == 0)
			m_info.dir = DIR_LEFT;
		else if (strcmp(argv[2], "y-up") == 0)
			m_info.dir = DIR_UP;
		else if (strcmp(argv[2], "y-down") == 0)
			m_info.dir = DIR_DOWN;
		if (strcmp(argv[3], "low") == 0)
			m_info.speed = SPEED_LOW;
		else if (strcmp(argv[3], "high") == 0)
			m_info.speed = SPEED_HIGH;
		else
			m_info.speed = SPEED_NORMAL;
		m_info.degree = strtol(argv[4], NULL, 10);
		ioctl(fd, RTS_RUN_MOTOR_DEGREE_CTL, &m_info);
	} else if (strcmp(argv[1], "status") == 0) {
		ioctl(fd, RTS_IS_RUNNING_MOTOR_CTL_IOR, &m_info);
		printf(m_info.is_running ? "motor is running\n" :
				"motor is not running\n");
	}

	motor_release(fd);

	return 0;

err:
	motor_release(fd);
	print_help_info(argv[0]);
	return ret;
}
