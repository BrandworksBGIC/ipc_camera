#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rts_errno.h"
#include "rts_io_adc.h"

#include "ipc_gpio_dri.h"
#include "ipc_io.h"
#include "ipc_pwm.h"

#include "ipc_json.h"
#include "m433_ioctl.h"

typedef enum {
    GPIO_5          = 5,
    GPIO_6          = 6,
    GPIO_8          = 8,
    GPIO_12         = 12,
    GPIO_SD1_D0     = 48,
    GPIO_SD1_CLK    = 49,
    GPIO_USB_DEV_DP = 80,
    GPIO_USB_DEV_DM = 81,
    GPIO_USB_VBUS   = 84,
    GPIO_PWM1       = 42,
} IPRT_GPIO_DEFINE_E;

extern IPC_PRODUCT_TYPE __sys_get_product_type(void);

struct {
    union {
        struct ipc_gpio_attr gpio_attr;
        struct {
            char adc_chn;
            int  fd;
        } adc_attr;
        struct {
            s32 chn;
            u32 period;
            u32 duty;
        } pwm_attr;
    };

    int active_level;
    int is_init_at; // 0 ignore, 1 GPIO mode, 2 ADC mode
#define IO_TYPE_NULL 0
#define IO_TYPE_GPIO 1
#define IO_TYPE_ADC 2
#define IO_TYPE_PWM 3
} gpio[IPC_IO_NAME_NUM];

static int ipc_gpio_fd = -1;

static s32 _io_init(void)
{
    int i   = 0;
    int ret = 0;

    ipc_gpio_fd = open("/dev/ipc-gpio-dri", O_RDWR);
    if (ipc_gpio_fd < 0) {
        printf("open gv-gpio node fail,%s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < IPC_IO_NAME_NUM; i++) {
        if (gpio[i].is_init_at == IO_TYPE_GPIO) {
            if (gpio[i].gpio_attr.gpio_dir) {
                if (i == IPC_IO_NAME_WIRELESS_PWR) {
                    gpio[i].gpio_attr.value = gpio[i].active_level;
                } else {
                    gpio[i].gpio_attr.value = !gpio[i].active_level;
                }
            }
            ret = ioctl(ipc_gpio_fd, IOCTL_IO_INIT, &gpio[i].gpio_attr);
            if (ret < 0) {
                break;
            }
        } else if (gpio[i].is_init_at == IO_TYPE_ADC) {

        } else if (gpio[i].is_init_at == IO_TYPE_PWM) {
            ipc_pwm_chn_init(gpio[i].pwm_attr.chn, gpio[i].pwm_attr.period, gpio[i].pwm_attr.duty);
        }
    }

    return ret;
}

static s32 _io_uninit(void)
{
    int i   = 0;
    int ret = 0;

    if (ipc_gpio_fd < 0) {
        return -1;
    }

    for (i = 0; i < IPC_IO_NAME_NUM; i++) {
        if (gpio[i].is_init_at == IO_TYPE_GPIO) {
            ret = ioctl(ipc_gpio_fd, IOCTL_IO_UNINIT, &gpio[i].gpio_attr);
            if (ret < 0) {
                printf("gpio uninit fail,%s\n", strerror(errno));
            }
        } else if (gpio[i].is_init_at == IO_TYPE_ADC) {

        } else if (gpio[i].is_init_at == IO_TYPE_PWM) {
            ipc_pwm_chn_uninit(gpio[i].pwm_attr.chn);
        }

        gpio[i].is_init_at = 0;
    }

    close(ipc_gpio_fd);
    ipc_gpio_fd = -1;

    return ret;
}

#define STR_FUN_MAP(name)                                                                                              \
    {                                                                                                                  \
#name, IPC_IO_NAME_##name                                                                                       \
    }

typedef struct {
    pv8        node_name;
    IPC_IO_NAME io_name;
} IoNodeMap;

static IoNodeMap node_map[] = {
    STR_FUN_MAP(SPEAKER),         STR_FUN_MAP(RESET_BUTTON),       STR_FUN_MAP(WHITE_LIGTH),
    STR_FUN_MAP(INFRARED_LIGTH),  STR_FUN_MAP(LIGHT_SENSOR),       STR_FUN_MAP(IRCUT_A),
    STR_FUN_MAP(IRCUT_B),         STR_FUN_MAP(STATUS_INDICATOR_A), STR_FUN_MAP(FLOOD_LIGHT),
    STR_FUN_MAP(PIR_ALARM),       STR_FUN_MAP(STATUS_INDICATOR_B), STR_FUN_MAP(RGB_LIGHT_RED),
    STR_FUN_MAP(RGB_LIGHT_GREEN), STR_FUN_MAP(RGB_LIGHT_BLUE),     STR_FUN_MAP(WIRELESS_PWR),
    STR_FUN_MAP(FRONT_BUTTON),    STR_FUN_MAP(ETH0_RST),
};

static void _map_device_node_to_gpio(pv8 json_str, pv8 io_name_s, IPC_IO_NAME io_name)
{
    v8  compatible[16] = { 0 };
    s32 active_level   = 0;

    struct {
        u32 num;
        u32 dir;
        u32 val;
    } gpio_attr;

    struct {
        u32 chn;
    } adc_attr;

    struct {
        u32 chn;
        u32 period;
        u32 duty;
        u32 min_duty;
        u32 max_duty;
    } pwm_attr;

    ipc_json_t _gpio_attr_json[] = {
        json_int("gpio_num", gpio_attr.num),
        json_int("gpio_dir", gpio_attr.dir),
        json_int("value", gpio_attr.val),
    };

    ipc_json_t _adc_attr_json[] = {
        json_int("adc_chn", adc_attr.chn),
    };

    ipc_json_t _pwm_attr_json[] = {
        json_int("pwm_chn", pwm_attr.chn),       json_int("period", pwm_attr.period),
        json_int("duty", pwm_attr.duty),         json_int("min_duty", pwm_attr.min_duty),
        json_int("max_duty", pwm_attr.max_duty),
    };

    ipc_json_t _attr_json[] = {
        json_string("compatible", compatible),           json_int("active_level", active_level),
        json_object("gpio_attr", NULL, _gpio_attr_json), json_object("adc_attr", NULL, _adc_attr_json),
        json_object("pwm_attr", NULL, _pwm_attr_json),
    };

    ipc_json_t _io_json[] = {
        json_object(io_name_s, NULL, _attr_json),
    };

    ipc_json_parse(json_str, _io_json, sizeof(_io_json) / sizeof(ipc_json_t));

    if (strcmp(compatible, "GPIO") == 0) {
        gpio[io_name].gpio_attr.gpio_num = gpio_attr.num;
        gpio[io_name].gpio_attr.gpio_dir = gpio_attr.dir;
        gpio[io_name].gpio_attr.value    = gpio_attr.val;
        gpio[io_name].active_level       = active_level;
        gpio[io_name].is_init_at         = IO_TYPE_GPIO;
    } else if (strcmp(compatible, "ADC") == 0) {
        gpio[io_name].adc_attr.adc_chn = adc_attr.chn;
        gpio[io_name].active_level     = active_level;
        gpio[io_name].is_init_at       = IO_TYPE_ADC;

    } else if (strcmp(compatible, "PWM") == 0) {
        gpio[io_name].pwm_attr.chn    = pwm_attr.chn;
        gpio[io_name].pwm_attr.period = pwm_attr.period;
        gpio[io_name].pwm_attr.duty   = pwm_attr.duty;
        gpio[io_name].active_level    = active_level;
        gpio[io_name].is_init_at      = IO_TYPE_PWM;
    } else {
    }
}

static s32 _io_init_by_json(void)
{
    FILE* fp = fopen("/conf/io.json", "r");
    if (fp == NULL) {
        printf("open file %s failed!\n", "/conf/io.json");
        return -1;
    }

    char json_str[8192] = { 0 };
    size_t read_size = fread(json_str, 1, sizeof(json_str), fp);
    fclose(fp);
    if (read_size == 0) {
        printf("read file %s failed or file is empty!\n", "/conf/io.json");
        return -1;
    }
    int i = 0;

    for (i = 0; i < sizeof(node_map) / sizeof(IoNodeMap); i++) {
        _map_device_node_to_gpio(json_str, node_map[i].node_name, node_map[i].io_name);
    }

    printf("\nParsed IO Configurations:\n");
    for (i = 0; i < IPC_IO_NAME_NUM; i++) {
        if (gpio[i].is_init_at == IO_TYPE_GPIO) {
            printf("%d: GPIO (num: %d, dir: %d, value: %d, active_level: %d)\n",
                   i,
                   gpio[i].gpio_attr.gpio_num,
                   gpio[i].gpio_attr.gpio_dir,
                   gpio[i].gpio_attr.value,
                   gpio[i].active_level);
        } else if (gpio[i].is_init_at == IO_TYPE_ADC) {
            printf("%d: ADC (chn: %d, active_level: %d)\n", i, gpio[i].adc_attr.adc_chn, gpio[i].active_level);
        } else if (gpio[i].is_init_at == IO_TYPE_PWM) {
            printf("%d: PWM (chn: %d, period: %d, duty: %d, active_level: %d)\n",
                   i,
                   gpio[i].pwm_attr.chn,
                   gpio[i].pwm_attr.period,
                   gpio[i].pwm_attr.duty,
                   gpio[i].active_level);
        } else {
            printf("%d: Not configured\n", i);
        }
    }

    return 0;
}

static s32 quirk_io_init(IPC_PRODUCT_TYPE product_type)
{

    return 0;
}

static s32 quirk_io_uninit(void)
{

    return 0;
}

static s32 quirk_io_write(IPC_IO_NAME name, s32 value)
{

    return 0;
}

static s32 _feeder_v_io_init(void)
{

    printf("%s\n", __func__);

    return 0;
}

static s32 _ptz_io_init(void)
{
    // return 0;
    printf("%s\n", __func__);

    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_num = GPIO_USB_DEV_DM; // GPIO_USB_VBUS;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_SPEAKER].active_level       = 0;
    gpio[IPC_IO_NAME_SPEAKER].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_num = GPIO_USB_VBUS; // GPIO_USB_DEV_DM;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_dir = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].active_level       = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_num = GPIO_SD1_CLK;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_A].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_A].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_num = GPIO_SD1_D0;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_B].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_B].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_num = GPIO_8;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WIRELESS_PWR].active_level       = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_num = GPIO_12;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_num = GPIO_USB_DEV_DP;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WHITE_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_num = GPIO_6;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].active_level       = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].is_init_at         = IO_TYPE_GPIO;

    // gpio[IPC_IO_NAME_FRONT_BUTTON].adc_attr.adc_chn = 0;
    // gpio[IPC_IO_NAME_FRONT_BUTTON].is_init_at       = IO_TYPE_ADC;
    // gpio[IPC_IO_NAME_FRONT_BUTTON].active_level     = 0;

    return 0;
}

static s32 _card_io_init(void)
{

    printf("%s\n", __func__);

    return 0;
}

static s32 _38_io_init(void)
{

    printf("%s\n", __func__);

    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_num = GPIO_USB_DEV_DM; // GPIO_USB_VBUS;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_SPEAKER].active_level       = 0;
    gpio[IPC_IO_NAME_SPEAKER].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_num = GPIO_USB_VBUS; // GPIO_USB_DEV_DM;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_dir = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].active_level       = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_num = GPIO_SD1_CLK;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_A].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_A].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_num = GPIO_SD1_D0;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_B].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_B].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_num = GPIO_8;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WIRELESS_PWR].active_level       = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_num = GPIO_12;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_num = GPIO_USB_DEV_DP;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WHITE_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_num = GPIO_6;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].active_level       = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].is_init_at         = IO_TYPE_GPIO;

    // gpio[IPC_IO_NAME_FRONT_BUTTON].adc_attr.adc_chn = 0;
    // gpio[IPC_IO_NAME_FRONT_BUTTON].is_init_at       = IO_TYPE_ADC;
    // gpio[IPC_IO_NAME_FRONT_BUTTON].active_level     = 0;

    return 0;
}

static s32 _doorbell_io_init(void)
{
    // return 0;
    printf("%s\n", __func__);

    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_num = GPIO_USB_DEV_DM; // GPIO_USB_VBUS;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_SPEAKER].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_SPEAKER].active_level       = 0;
    gpio[IPC_IO_NAME_SPEAKER].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_num = GPIO_USB_VBUS; // GPIO_USB_DEV_DM;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.gpio_dir = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].active_level       = 0;
    gpio[IPC_IO_NAME_RESET_BUTTON].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_num = GPIO_SD1_CLK;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_A].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_A].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_num = GPIO_SD1_D0;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_IRCUT_B].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_IRCUT_B].active_level       = 1;
    gpio[IPC_IO_NAME_IRCUT_B].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_num = GPIO_8;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WIRELESS_PWR].active_level       = 1;
    gpio[IPC_IO_NAME_WIRELESS_PWR].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_num = GPIO_12;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_INFRARED_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_num = GPIO_USB_DEV_DP;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_WHITE_LIGTH].active_level       = 1;
    gpio[IPC_IO_NAME_WHITE_LIGTH].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_num = GPIO_6;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].active_level       = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_A].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_STATUS_INDICATOR_B].gpio_attr.gpio_num = GPIO_PWM1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_B].gpio_attr.gpio_dir = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_B].gpio_attr.value    = 0;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_B].active_level       = 1;
    gpio[IPC_IO_NAME_STATUS_INDICATOR_B].is_init_at         = IO_TYPE_GPIO;

    gpio[IPC_IO_NAME_FRONT_BUTTON].adc_attr.adc_chn = 0;
    gpio[IPC_IO_NAME_FRONT_BUTTON].is_init_at       = IO_TYPE_ADC;
    gpio[IPC_IO_NAME_FRONT_BUTTON].active_level     = 0;

    // Initialize M433 driver for 433MHz transmission
    printf("Initializing M433 driver with GPIO 14 for doorbell...\n");
    int m433_fd = open("/dev/m433", O_RDWR);
    if (m433_fd >= 0) {
        if (ioctl(m433_fd, M433_IOCTL_SET_GPIO, 14) >= 0) {
            printf("M433 driver GPIO set to 14 successfully\n");
        } else {
            printf("Failed to set M433 driver GPIO to 14: %s\n", strerror(errno));
            m433_fd = -1;
        }
    } else {
        printf("Failed to open M433 device: %s\n", strerror(errno));
    }
    close(m433_fd);

    return 0;
}

static s32 _safe_light_io_init(void)
{

    printf("%s\n", __func__);

    return 0;
}

static s32 _card_pano_360_io_init(void)
{
    printf("%s\n", __func__);

    return 0;
}

static u32 convert_percent_to_duty(u32 period, s32 percent, s32 active_level)
{
    if (percent > 0) {
        if (active_level) {
            percent = 100 - percent;
        }

        period = period / 100;
        period *= 80;

        period = period / 100;

        return period * percent;
    } else {
        if (active_level) {
            return period;
        }
        return 0;
    }
}

static void _flip_io_active_level(struct ipc_io_active_level_flip* flip_table, int num)
{
    int i = 0;
    if (flip_table == NULL) {
        return;
    }

    for (i = 0; i < num; i++) {
        // coverity[NO_EFFECT :SUPPRESS]
        if (flip_table[i].name >= IPC_IO_NAME_NUM || flip_table[i].name < 0) {
            printf("%s io name error\n", __func__);
            continue;
        }

        gpio[flip_table[i].name].active_level = flip_table[i].is_flip == 0 ? gpio[flip_table[i].name].active_level
                                                                           : !gpio[flip_table[i].name].active_level;
    }
}

s32 ipc_plat_io_init(struct ipc_io_active_level_flip* flip_table, int num)
{
    IPC_PRODUCT_TYPE product_type = __sys_get_product_type();
    int             ret          = 0;

    if (_io_init_by_json() < 0) {
        switch (product_type) {
            case IPC_PRODUCT_TYPE_PTZ:
                ret = _ptz_io_init();
                break;
            case IPC_PRODUCT_TYPE_SAFE_LIGHT:
                ret = _safe_light_io_init();
                break;
            case IPC_PRODUCT_TYPE_FEEDER_V:
                ret = _feeder_v_io_init();
                break;
            case IPC_PRODUCT_TYPE_CARD:
                ret = _card_io_init();
                break;
            case IPC_PRODUCT_TYPE_38:
                ret = _38_io_init();
                break;
            case IPC_PRODUCT_TYPE_PANO_360:
                ret = _card_pano_360_io_init();
                break;
            case IPC_PRODUCT_TYPE_DOORBELL:
                ret = _doorbell_io_init();
                break;
            default:
                ret = -1;
                goto product_type_err;
                break;
        }

        if (ret < 0) {
            return ret;
        }
    }

    _flip_io_active_level(flip_table, num);

    ret = _io_init();

    quirk_io_init(product_type);

product_type_err:

    return ret;
}

s32 ipc_plat_io_uninit(void)
{
    int ret = 0;

    quirk_io_uninit();

    ret = _io_uninit();

    return ret;
}

s32 ipc_plat_io_read(IPC_IO_NAME name, IPC_IO_VALUE_TYPE* type)
{
    int ret = 0;
    if (gpio[name].is_init_at == IO_TYPE_GPIO) {
        ret = ioctl(ipc_gpio_fd, IOCTL_IO_GET, &gpio[name].gpio_attr);
        if (ret < 0) {
            return ret;
        }
        if (gpio[name].gpio_attr.value == gpio[name].active_level) {
            *type = IPC_IO_VALUE_IS_ACTIVE;
        } else {
            *type = IPC_IO_VALUE_IS_INACTIVE;
        }
    } else if (gpio[name].is_init_at == IO_TYPE_ADC) {
        ret = rts_io_adc_get_value(gpio[name].adc_attr.adc_chn);
        // printf("[%s][%d]: rts_io_adc_get_value: %d\n", __func__, __LINE__, ret);
        s32 value = 0;
        if (ret > 1500) {
            value = 1;
        }
        *type = gpio[name].active_level == value ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE;
    } else {
        ret = -2;
    }
    // printf("%s:%d\n", __func__, ret);
    // perror(__func__);
    return ret;
}

s32 ipc_plat_io_write(IPC_IO_NAME name, IPC_IO_VALUE_TYPE type)
{
    int ret = 0;

    if (gpio[name].is_init_at == IO_TYPE_GPIO) {
        int active_level = gpio[name].active_level;

        gpio[name].gpio_attr.value = (type == IPC_IO_VALUE_IS_ACTIVE) ? active_level : (!active_level);

        ret = ioctl(ipc_gpio_fd, IOCTL_IO_SET, &gpio[name].gpio_attr);

        quirk_io_write(name, gpio[name].gpio_attr.value);

    } else if (gpio[name].is_init_at == IO_TYPE_PWM) {
        s32 percent = type;
        u32 duty    = convert_percent_to_duty(gpio[name].pwm_attr.period, percent, gpio[name].active_level);
        ipc_pwm_modify_chn_duty(gpio[name].pwm_attr.chn, duty);
    }

    // printf("%s:%d:%d:%d\n", __func__, name, ret, gpio[name].gpio_attr.value);
    return ret;
}
