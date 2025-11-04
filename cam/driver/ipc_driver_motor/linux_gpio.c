#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#include <soc/gpio.h>
#endif
#include "ipc_motor_gpio.h"

typedef struct {
    int motor_gpioV_seq[IPC_MOTOR_PIN_NUM];
    int motor_gpioH_seq[IPC_MOTOR_PIN_NUM];
} IPC_MOTOR_GPIO_SEQ_S, *P_IPC_MOTOR_GPIO_SEQ_S;

struct motor_gpio_s {
    int gpio_num[IPC_MOTOR_PIN_NUM];
} motor_gpio[IPC_MOTOR_PIN_GROUP_NUM];

static unsigned char init_flag = 0;

#if defined(IPC_ARCH_RTS3917)
static void gpio_set_QJ_V1_0_rts3917(void)
{
    motor_gpio[IPC_MOTOR_PIN_GROUP_0].gpio_num[IPC_MOTOR_PIN_D0] = 5;
    motor_gpio[IPC_MOTOR_PIN_GROUP_0].gpio_num[IPC_MOTOR_PIN_D1] = 14;
    motor_gpio[IPC_MOTOR_PIN_GROUP_0].gpio_num[IPC_MOTOR_PIN_D2] = 4;
    motor_gpio[IPC_MOTOR_PIN_GROUP_0].gpio_num[IPC_MOTOR_PIN_D3] = 15;

    motor_gpio[IPC_MOTOR_PIN_GROUP_1].gpio_num[IPC_MOTOR_PIN_D3] = 16;
    motor_gpio[IPC_MOTOR_PIN_GROUP_1].gpio_num[IPC_MOTOR_PIN_D2] = 17;
    motor_gpio[IPC_MOTOR_PIN_GROUP_1].gpio_num[IPC_MOTOR_PIN_D1] = 18;
    motor_gpio[IPC_MOTOR_PIN_GROUP_1].gpio_num[IPC_MOTOR_PIN_D0] = 19;
}

static void _gpio_init(int ptz_product_type)
{
    gpio_set_QJ_V1_0_rts3917();
}

#else
#error A platform must be defined
#endif

static void _gpio_io_change(P_IPC_MOTOR_GPIO_SEQ_S motor_gpio_seq)
{
    struct motor_gpio_s tmp_motor_gpio[IPC_MOTOR_PIN_GROUP_NUM];

    int i = 0;
    int j = 0;
    for (i = 0; i < IPC_MOTOR_PIN_GROUP_NUM; i++) {
        tmp_motor_gpio[i] = motor_gpio[i];
    }

    for (j = 0; j < IPC_MOTOR_PIN_GROUP_NUM; j++) {
        for (i = 0; i < IPC_MOTOR_PIN_NUM; i++) {
            motor_gpio[j].gpio_num[i] = tmp_motor_gpio[j].gpio_num[motor_gpio_seq->motor_gpioH_seq[i]];
        }
    }
}

/* Determine whether array elements are unequal and less than a certain integer */
static int _not_same_and_less(int arr[], int size, int max)
{
    int i              = 0;
    int j              = 0;
    int distinct_pairs = 0;

    for (i = 0; i < size; i++) {
        if (arr[i] >= max) {
            return 0;
        }

        for (j = i + 1; j < size; j++) {
            if (arr[i] != arr[j]) {
                distinct_pairs++;
            }
        }
    }

    printk("distinct_pairs %d, max: %d\n", distinct_pairs, max);

    return distinct_pairs == size * (size - 1) / 2;
}

int ipc_motor_gpio_init(int ptz_product_type, char* motor_gpioV_seq, char* motor_gpioH_seq)
{
    int ret = 0;
    int i   = 0;
    int j   = 0;
    IPC_MOTOR_GPIO_SEQ_S motor_gpio_seq;
    IPC_MOTOR_GPIO_SEQ_S motor_gpio_seq_default = { .motor_gpioV_seq = { 0, 1, 2, 3 }, .motor_gpioH_seq = { 0, 1, 2, 3 } };

    if (init_flag == 1) {
        return -1;
    }
    init_flag = 1;

    memset(&motor_gpio_seq, 0, sizeof(IPC_MOTOR_GPIO_SEQ_S));

    ret = sscanf(motor_gpioV_seq, "%d,%d,%d,%d", &motor_gpio_seq.motor_gpioV_seq[IPC_MOTOR_PIN_D0], &motor_gpio_seq.motor_gpioV_seq[IPC_MOTOR_PIN_D1],
                 &motor_gpio_seq.motor_gpioV_seq[IPC_MOTOR_PIN_D2], &motor_gpio_seq.motor_gpioV_seq[IPC_MOTOR_PIN_D3]);
    if (ret != 4 || !_not_same_and_less(motor_gpio_seq.motor_gpioV_seq, IPC_MOTOR_PIN_NUM, IPC_MOTOR_PIN_NUM)) {
        /* If the parsed sequence does not conform to the rules, use the default GPIO sequence */
        printk("Motor GPIO V argument error, using default !!!\n");
        memcpy(motor_gpio_seq.motor_gpioV_seq, motor_gpio_seq_default.motor_gpioV_seq, sizeof(motor_gpio_seq_default.motor_gpioV_seq));
    }

    ret = sscanf(motor_gpioH_seq, "%d,%d,%d,%d", &motor_gpio_seq.motor_gpioH_seq[IPC_MOTOR_PIN_D0], &motor_gpio_seq.motor_gpioH_seq[IPC_MOTOR_PIN_D1],
                 &motor_gpio_seq.motor_gpioH_seq[IPC_MOTOR_PIN_D2], &motor_gpio_seq.motor_gpioH_seq[IPC_MOTOR_PIN_D3]);
    if (ret != 4 || !_not_same_and_less(motor_gpio_seq.motor_gpioH_seq, IPC_MOTOR_PIN_NUM, IPC_MOTOR_PIN_NUM)) {
        /* If the parsed sequence does not conform to the rules, use the default GPIO sequence */
        printk("Motor GPIO H argument error, using default !!!\n");
        memcpy(motor_gpio_seq.motor_gpioH_seq, motor_gpio_seq_default.motor_gpioH_seq, sizeof(motor_gpio_seq_default.motor_gpioH_seq));
    }

    _gpio_init(ptz_product_type);

    _gpio_io_change(&motor_gpio_seq);

    for (i = 0; i < IPC_MOTOR_PIN_GROUP_NUM; i++) {
        for (j = 0; j < IPC_MOTOR_PIN_NUM; j++) {
            if (motor_gpio[i].gpio_num[j] >= 0) {
                char pin_name[30];
                sprintf(pin_name, "motor_%d%d", i, j);
                printk("%s:%d\n", pin_name, motor_gpio[i].gpio_num[j]);
                gpio_request(motor_gpio[i].gpio_num[j], pin_name);
                gpio_direction_output(motor_gpio[i].gpio_num[j], 0);
                gpio_set_value(motor_gpio[i].gpio_num[j], 0);
            }
        }
    }

    return 0;
}

int ipc_motor_gpio_set(IPC_MOTOR_PIN_GROUP motor_group, IPC_MOTOR_PIN motor_pin, int val)
{
    if (motor_gpio[motor_group].gpio_num[motor_pin] >= 0) {
        gpio_set_value(motor_gpio[motor_group].gpio_num[motor_pin], val);
    }

    return 0;
}

int ipc_motor_gpio_uninit(void)
{
    int i = 0;
    int j = 0;

    if (init_flag == 0) {
        return -1;
    }
    init_flag = 0;

    for (i = 0; i < IPC_MOTOR_PIN_GROUP_NUM; i++) {
        for (j = 0; j < IPC_MOTOR_PIN_NUM; j++) {
            if (motor_gpio[i].gpio_num[j] >= 0) {
                printk("%s:[%d:%d:%d]\n", __func__, i, j, motor_gpio[i].gpio_num[j]);
                // gpio_free(motor_gpio[i].gpio_num[j]); // Do not free, otherwise some pins will remain high, potentially damaging the motor
                gpio_set_value(motor_gpio[i].gpio_num[j], 0);
            }
        }
    }
    return 0;
}

int ipc_motor_gpio_get_zero_point_val(IPC_MOTOR_PIN_GROUP motor_group)
{
    return -1;
}