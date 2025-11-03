#ifndef PTZ_USR_H_INCLUDED
#define PTZ_USR_H_INCLUDED

#define DIR_NONE	0
#define DIR_UP		1
#define DIR_DOWN	2
#define DIR_LEFT	3
#define DIR_RIGHT	4

#define SPEED_NORMAL	2
#define SPEED_LOW	1
#define SPEED_HIGH	4

#define RTS_GET_MOTOR_POS_CTL _IOR('m', 1, int)
#define RTS_RUN_MOTOR_TIME_CTL _IOW('m', 2, int)
#define RTS_RUN_MOTOR_DEGREE_CTL _IOW('m', 3, int)
#define RTS_IS_RUNNING_MOTOR_CTL _IOR('m', 4, int)
#define RTS_STOP_MOTOR_CTL _IOW('m', 5, int)

struct motorctrl_info {

	int x;
	int y;
	unsigned int dir;
	unsigned int speed;
	unsigned int degree;
	unsigned int run_time;
	unsigned int is_running;
	unsigned int motor_run_time[2];
};

#endif
