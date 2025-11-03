#define _CRT_SECURE_NO_WARNINGS
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include "ptz_usr.h"

int flag		= 0;
void signal_handle(int sig)
{
    printf("################## get signal: %d\n", sig);

    switch(sig) {
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
        flag = 1;
        break;

    default:
        break;
    }

    return;
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
	printf("%s 0\n", name);
	printf("\tdescribe: go up\n");
	printf("%s 1\n", name);
	printf("\tdescribe: go down\n");
	printf("%s 2\n", name);
	printf("\tdescribe: y self check\n");
	printf("%s 3\n", name);
	printf("\tdescribe: y stop\n");
	printf("%s 4\n", name);
	printf("\tdescribe: go left\n");
	printf("%s 5\n", name);
	printf("\tdescribe: go right\n");
	printf("%s 6\n", name);
	printf("\tdescribe: x self check\n");
	printf("%s 7\n", name);
	printf("\tdescribe: x stop\n");
	printf("%s 8\n", name);
	printf("\tdescribe: go left_up\n");
	printf("%s 9\n", name);
	printf("\tdescribe: go left_down\n");
	printf("%s 10\n", name);
	printf("\tdescribe: go right_up\n");
	printf("%s 11\n", name);
	printf("\tdescribe: go right_down\n");
	printf("%s 12 x y\n", name);
	printf("\tdescribe: go specific pos x y\n");
	printf("%s 13\n", name);
	printf("\tdescribe: stop both\n");
	printf("%s 14 x_speed y_speed\n", name);
	printf("\tdescribe: set x&y_speed(range: 0-1)\n");
	printf("%s 15\n", name);
	printf("\tdescribe: get x&y pos info\n");
}

void ptz_func_example(int argc, char** argv)
{
	int ret			= 0;
	int fd 			= -1;
	char func_num 	= 0;
	int c 			= 0;
	struct ty_motor_param_s param;
	struct ty_motor_param_g gparam;
	char para[100];

	memset(&gparam, 0x00, sizeof(struct ty_motor_param_g));
	memset(&param, 0x00, sizeof(struct ty_motor_param_s));
	
	if (argc < 2) {
		print_help_info(argv[0]);
		return;
	}

	while ((c = getopt_long(argc, argv,
				":h", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info(argv[0]);
			return;
		case '?':
			printf("invalid param: -%c\n", optopt);
			break;
		}
	}

	fd = open("/dev/tymotor", O_RDWR);
	if (fd < 0) {
		printf("open error.\n");
		return;
	}

	func_num = atoi(argv[1]);
	param.motor_func = func_num;
	switch(func_num) {
		case 0:
			param.motor_func = GO_UP;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 1:
			param.motor_func = GO_DOWN;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 2:
			param.motor_func = TILT_SELF_CHECK;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 3:
			param.motor_func = TILT_STOP;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 4:
			param.motor_func = GO_LEFT;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 5:
			param.motor_func = GO_RIGHT;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 6:
			param.motor_func = PAN_SELF_CHECK;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 7:
			param.motor_func = PAN_STOP;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 8:
			param.motor_func = LEFT_UP;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 9:
			param.motor_func = LEFT_DOWN;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 10:
			param.motor_func = RIGHT_UP;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 11:
			param.motor_func = RIGHT_DOWN;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 12:
			if(argc != 4) {
				print_help_info(argv[0]);
				goto fail;
			}
			param.goal_pan_pos = atoi(argv[2]);
			param.goal_tilt_pos = atoi(argv[3]);
			param.motor_func = GO_POS;
			ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
			break;
		case 13:
			ioctl(fd, TY_MOTOR_IOCTL_STOP, NULL);
			break;
		case 14:
			if(argc != 4) {
				print_help_info(argv[0]);
				goto fail;
			}
			param.goal_pan_speed = atoi(argv[2]);
			param.goal_tilt_speed = atoi(argv[3]);
			break;
		case 15:
			gparam.state_cmd= GET_PAN_POS;
			ioctl(fd, TY_MOTOR_IOCTL_GET, &gparam);
			printf("x pos is %d \n",gparam.state_value);

			gparam.state_cmd= GET_TILT_POS;
			ioctl(fd, TY_MOTOR_IOCTL_GET, &gparam);
			printf("y pos is %d \n",gparam.state_value);
			break;
		default: {
			print_help_info(argv[0]);
			break;
		}
	}

fail:
	close(fd);
	return;
}


int main(int argc, char** argv)
{
	int *p = NULL;
	
	signal(SIGINT, signal_handle);

    printf("begin___\n");

	ptz_func_example(argc, argv);

    printf("end___\n");

	return 0;
}