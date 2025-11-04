#ifndef __IPC_MOTOR_H__
#define __IPC_MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "asm-generic/ioctl.h"

#define IPC_MOTOR_IOC_MAGIC 'M'

/*Initialize the motor. Parameters #struct ipc_motor_attr*/
#define IPC_IOCTL_MOTOR_INIT _IOW(IPC_MOTOR_IOC_MAGIC, 110, unsigned int)

/*Uninitialize the motor, parameter: none*/
#define IPC_IOCTL_MOTOR_UNINIT _IOW(IPC_MOTOR_IOC_MAGIC, 111, unsigned int)

/*Run the motor, parameter: struct ipc_motor_step*/
#define IPC_IOCTL_MOTOR_RUN_STEPS _IOW(IPC_MOTOR_IOC_MAGIC, 112, unsigned int)

/*Obtain the current operating parameters of the motor, parameter: struct ipc_motor_step, set motor_index to obtain the current remaining steps*/
#define IPC_IOCTL_MOTOR_GET_REMAINING_RUNNING_STEPS _IOW(IPC_MOTOR_IOC_MAGIC, 113, unsigned int)

/*The unit is one step per 10us, parameter: struct ipc_motor_speed*/
#define IPC_IOCTL_MOTOR_SET_SPEED _IOW(IPC_MOTOR_IOC_MAGIC, 114, unsigned int)

/*dump the current operating parameters of the motor*/
#define IPC_IOCTL_MOTOR_DUMP_INFO _IOW(IPC_MOTOR_IOC_MAGIC, 115, unsigned int)

/*Obtain the current motor position step count, parameter: struct ipc_motor_step*/
#define IPC_IOCTL_MOTOR_GET_CUR_STEPS _IOW(IPC_MOTOR_IOC_MAGIC, 116, unsigned int)

/*Set the motor to run to a specified motor position, parameter: struct ipc_motor_step*/
#define IPC_IOCTL_MOTOR_GOTO_SPEC_POS _IOW(IPC_MOTOR_IOC_MAGIC, 117, unsigned int)

/*Get the current running status of the motor, parameter: struct ipc_motor_status*/
#define IPC_IOCTL_MOTOR_GET_STATUS _IOW(IPC_MOTOR_IOC_MAGIC, 118, unsigned int)

/*Set the motor state, currently only supports starting the motor automatic cruise state, parameter: struct ipc_motor_status*/
#define IPC_IOCTL_MOTOR_SET_STATUS _IOW(IPC_MOTOR_IOC_MAGIC, 119, unsigned int)

/*Set the current motor position step count, parameter: struct ipc_motor_step*/
#define IPC_IOCTL_MOTOR_SET_CUR_STEPS _IOW(IPC_MOTOR_IOC_MAGIC, 120, unsigned int)

typedef enum {
    IPC_MOTOR_STATUS_STOP,                       // stop
    IPC_MOTOR_STATUS_RUNNING,                    // running
    IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE, // self-inspection full circle state
    IPC_MOTOR_STATUS_MODEL_BUILDING_HALF_CIRCLE, // self-inspection half circle state
    IPC_MOTOR_STATUS_AUTOMATIC_CRUISE,           // automatic cruise
} IPC_MOTOR_STATUS;

typedef enum {
    IPC_MOTOR_INDEX_0,
    IPC_MOTOR_INDEX_1,
    IPC_MOTOR_INDEX_NUM,
} IPC_MOTOR_INDEX;

typedef enum {
    IPC_MOTOR_DIR_CLOCKWISE,        // clockwise
    IPC_MOTOR_DIR_COUNTERCLOCKWISE, // counterclockwise
} IPC_MOTOR_DIR;

#define IPC_MOTOR_INDEX_HORIZONTAL IPC_MOTOR_INDEX_0
#define IPC_MOTOR_INDEX_VERTICAL IPC_MOTOR_INDEX_1

#define IPC_MOTOR_DIR_RIGHT IPC_MOTOR_DIR_CLOCKWISE
#define IPC_MOTOR_DIR_LEFT IPC_MOTOR_DIR_COUNTERCLOCKWISE

#define IPC_MOTOR_DIR_UP IPC_MOTOR_DIR_CLOCKWISE
#define IPC_MOTOR_DIR_DOWN IPC_MOTOR_DIR_COUNTERCLOCKWISE

struct ipc_motor_attr {
    int max_step[IPC_MOTOR_INDEX_NUM]; /* Maximum rotatable step length */
    int model_build; /* Establish a motor position model (self-test), 0, no self-test; 1, self-test; 2, self-test mode, but the motor does not run,
                        and the modeling steps are set externally */
    int ptz_product_type;                   /* Motor io distinction */
    int speed[IPC_MOTOR_INDEX_NUM];          // Default speed
    int init_pos_step[IPC_MOTOR_INDEX_NUM];  // Initial position after completion of self-test, in steps, in the middle of the maximum position when 0
    int limit_min_step[IPC_MOTOR_INDEX_NUM]; // Minimum limit step count
    int limit_max_step[IPC_MOTOR_INDEX_NUM]; // Maximum limit step count
};

struct ipc_motor_step {
    IPC_MOTOR_INDEX motor_index;       /* Motor number */
    IPC_MOTOR_DIR direction;           /* Motor rotation direction */
    int step;                         /* Motor steps */
    unsigned int run_time_step_count; /* For obtaining the step count statistics of the motor during operation */
};

struct ipc_motor_speed {
    IPC_MOTOR_INDEX motor_index; /* Motor number */
    int speed;                  /* int (80~450), the smaller the faster */
};

struct ipc_motor_status {
    IPC_MOTOR_INDEX motor_index; /* Motor number */
    IPC_MOTOR_STATUS status;     /* Motor status */
};

#ifdef __cplusplus
}
#endif

#endif //__IPC_MOTOR_H__
