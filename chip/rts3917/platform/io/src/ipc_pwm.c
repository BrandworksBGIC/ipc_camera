#include <stdio.h>
#include "ipc_pwm.h"

#define PWM_PATH "/sys/devices/platform/ocp/18820000.pwm/settings/pwm"

s32 ipc_pwm_chn_init(s32 ch, s32 period, s32 duty)
{
    char path[256] = { 0 };
    FILE* fp = NULL;

    snprintf(path, sizeof(path), PWM_PATH"%d/request",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite("1", 1, 1, fp);
        fclose(fp);
    }

    snprintf(path, sizeof(path), PWM_PATH"%d/period_ns",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "%d", period);
        fwrite(buf, strlen(buf), 1, fp);
        fclose(fp);
    }

    snprintf(path, sizeof(path), PWM_PATH"%d/duty_ns",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "%d", duty);
        fwrite(buf, strlen(buf), 1, fp);
        fclose(fp);
    }

    snprintf(path, sizeof(path), PWM_PATH"%d/enable",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite("1", 1, 1, fp);
        fclose(fp);
    }


    return 0;
}

s32 ipc_pwm_chn_uninit(s32 ch)
{
    char path[256] = { 0 };
    FILE* fp = NULL;

    snprintf(path, sizeof(path), PWM_PATH"%d/enable",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite("0", 1, 1, fp);
        fclose(fp);
    }

    return 0;
}

s32 ipc_pwm_modify_chn_duty(s32 ch, s32 duty)
{
    char path[256] = { 0 };
    FILE* fp = NULL;
    
    snprintf(path, sizeof(path), PWM_PATH"%d/duty_ns",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "%d", duty);
        fwrite(buf, strlen(buf), 1, fp);
        fclose(fp);
    }

    snprintf(path, sizeof(path), PWM_PATH"%d/enable",  ch);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite("1", 1, 1, fp);
        fclose(fp);
    }

    return 0;
}