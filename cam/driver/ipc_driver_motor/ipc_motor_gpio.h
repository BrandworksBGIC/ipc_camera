#ifndef __IPC_MOTOR_GPIO_H__
#define __IPC_MOTOR_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IPC_MOTOR_PIN_GROUP_0,
    IPC_MOTOR_PIN_GROUP_1,
    IPC_MOTOR_PIN_GROUP_NUM,
} IPC_MOTOR_PIN_GROUP;

typedef enum {
    IPC_MOTOR_PIN_D0,
    IPC_MOTOR_PIN_D1,
    IPC_MOTOR_PIN_D2,
    IPC_MOTOR_PIN_D3,
    IPC_MOTOR_PIN_NUM,
} IPC_MOTOR_PIN;

int ipc_motor_gpio_init(int ptz_product_type, char* motor_gpioV_seq, char* motor_gpioH_seq);

int ipc_motor_gpio_set(IPC_MOTOR_PIN_GROUP motor_group, IPC_MOTOR_PIN motor_pin, int val);

int ipc_motor_gpio_uninit(void);

/*
 * For a motor that can rotate 360 degrees without limit, detect whether it has reached the zero point position,
 * and return the IO level state of the detection.
 * Returns 0 for low level, 1 for high level, and less than 0 for invalid or unsupported.
 */
int ipc_motor_gpio_get_zero_point_val(IPC_MOTOR_PIN_GROUP motor_group);

#ifdef __cplusplus
}
#endif

#endif //__IPC_GPIO_H__