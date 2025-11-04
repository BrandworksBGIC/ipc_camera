#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipc_alarm.h"
#include "ipc_button_monitor.h"
#include "ipc_decrypt.h"
#include "ipc_env_monitor.h"
#include "ipc_factory.h"
#include "ipc_middleware_config.h"
#include "ipc_misc.h"
#include "ipc_mpp.h"
#include "ipc_network.h"
#include "ipc_ping.h"
#include "ipc_ptz.h"
#include "ipc_rtc.h"
#include "ipc_sal_api.h"
#include "ipc_status_led.h"
#include "ipc_time.h"

#include "ed25519.h"
#include "sha512.h"

/********************************************** configure **************************************************/

#define SESSION "factory"

static ipc_factory_parm_t _g_parm = {
    // Static default values, defaults to 0 if not specified
    .version                           = 1,
    .lens_max_angle                    = 102,
    .ptz_v_max_angle                   = 170,
    .ptz_h_max_angle                   = 350,
    .ptz_circle_step                   = 4096,
    .ptz_track_reset_ts                = 60,
    .ptz_ctrl_speed                    = 6,
    .ptz_track_speed                   = 2,
    .ptz_h_gear_ratio                  = 1,
    .ptz_v_gear_ratio                  = 1,
    .ptz_self_check                    = 1,
    .disable_sensor_det_protect_lock_s = 2 * 60,
    .sensor_det_measure_time_s         = 5,
    .mplitude_80db                     = 10000,
    .day_to_night_exp_val              = -1,
    .night_to_day_exp_val              = -1,
    .night_to_day_g_r_diff             = -1,
    .night_to_day_g_b_diff             = -1,
    .ptz_gpioH_seq                     = "0,1,2,3",
    .ptz_gpioV_seq                     = "0,1,2,3",
};

static void _dynamic_factory_param_calculate(void)
{
    // Dynamic default values (require runtime checks and calculations)
    if (_g_parm.ptz_h_init_angle < 0.1)
        _g_parm.ptz_h_init_angle = _g_parm.ptz_h_max_angle / 2;
    if (_g_parm.ptz_v_init_angle < 0.1)
        _g_parm.ptz_v_init_angle = _g_parm.ptz_v_max_angle / 2;
    if (!_g_parm.ptz_h_ctrl_speed)
        _g_parm.ptz_h_ctrl_speed = _g_parm.ptz_ctrl_speed;
    if (!_g_parm.ptz_v_ctrl_speed)
        _g_parm.ptz_v_ctrl_speed = _g_parm.ptz_ctrl_speed;
    if (!_g_parm.ptz_h_track_speed)
        _g_parm.ptz_h_track_speed = _g_parm.ptz_track_speed;
    if (!_g_parm.ptz_v_track_speed)
        _g_parm.ptz_v_track_speed = _g_parm.ptz_track_speed;
}

/* Factory configuration */
static ipc_json_t _g_factory_json[] = {
    json_uint("spk_vol", _g_parm.spk_vol),
    json_uint("spk_gain", _g_parm.spk_gain),
    json_uint("mic_vol", _g_parm.mic_vol),
    json_uint("mic_gain", _g_parm.mic_gain),
    json_uint("image_flip", _g_parm.image_flip),
    json_uint("ircut_flip", _g_parm.ircut_flip),
    json_uint("irled_flip", _g_parm.irled_flip),
    json_uint("light_sensor_flip", _g_parm.light_sensor_flip),
    json_uint("white_light_flip", _g_parm.white_light_flip),
    json_uint("flood_light_flip", _g_parm.flood_light_flip),
    json_uint("indicator_lighta_flip", _g_parm.indicator_lighta_flip),
    json_uint("indicator_lightb_flip", _g_parm.indicator_lightb_flip),
    json_uint("spk_flip", _g_parm.spk_flip),
    json_uint("light_ctrl_mode", _g_parm.light_ctrl_mode),
    json_double("lens_max_angle", _g_parm.lens_max_angle),
    json_uint("ptz_circle_step", _g_parm.ptz_circle_step),
    json_uint("ptz_track_reset_ts", _g_parm.ptz_track_reset_ts),
    json_double("ptz_v_max_angle", _g_parm.ptz_v_max_angle),
    json_double("ptz_h_max_angle", _g_parm.ptz_h_max_angle),
    json_double("ptz_v_init_angle", _g_parm.ptz_v_init_angle),
    json_double("ptz_h_init_angle", _g_parm.ptz_h_init_angle),
    json_double("ptz_v_limit_min_angle", _g_parm.ptz_v_limit_min_angle),
    json_double("ptz_h_limit_min_angle", _g_parm.ptz_h_limit_min_angle),
    json_double("ptz_v_limit_max_angle", _g_parm.ptz_v_limit_max_angle),
    json_double("ptz_h_limit_max_angle", _g_parm.ptz_h_limit_max_angle),
    json_uint("ptz_v_flip", _g_parm.ptz_v_flip),
    json_uint("ptz_h_flip", _g_parm.ptz_h_flip),
    json_uint("ptz_ctrl_speed", _g_parm.ptz_ctrl_speed),
    json_uint("ptz_track_speed", _g_parm.ptz_track_speed),
    json_uint("ptz_h_ctrl_speed", _g_parm.ptz_h_ctrl_speed),
    json_uint("ptz_h_track_speed", _g_parm.ptz_h_track_speed),
    json_uint("ptz_v_ctrl_speed", _g_parm.ptz_v_ctrl_speed),
    json_uint("ptz_v_track_speed", _g_parm.ptz_v_track_speed),
    json_uint("ptz_v_track_enable", _g_parm.ptz_v_track_enable),
    json_double("ptz_h_gear_ratio", _g_parm.ptz_h_gear_ratio),
    json_double("ptz_v_gear_ratio", _g_parm.ptz_v_gear_ratio),
    json_uint("ptz_self_check", _g_parm.ptz_self_check),
    json_uint("ptz_position_in_privacy_mode", _g_parm.ptz_position_in_privacy_mode),
    json_uint("disable_light_sensor", _g_parm.disable_light_sensor),

    json_uint("disable_sensor_det_protect_lock_s", _g_parm.disable_sensor_det_protect_lock_s),
    json_uint("sensor_det_measure_time_s", _g_parm.sensor_det_measure_time_s),
    json_uint("mplitude_80db", _g_parm.mplitude_80db),
    json_int("day_to_night_exp_val", _g_parm.day_to_night_exp_val),
    json_int("night_to_day_exp_val", _g_parm.night_to_day_exp_val),
    json_int("night_to_day_g_r_diff", _g_parm.night_to_day_g_r_diff),
    json_int("night_to_day_g_b_diff", _g_parm.night_to_day_g_b_diff),

    json_string("ptz_gpioH_seq", _g_parm.ptz_gpioH_seq),
    json_string("ptz_gpioV_seq", _g_parm.ptz_gpioV_seq),

    json_string("language", _g_parm.language),
    json_string("pid", _g_parm.pid),
    json_uint("version", _g_parm.version),
    /* ... */
};

ipc_factory_parm_p ipc_factory_parm_get(void)
{
    static u8 is_first = 1;
    if (is_first) {
        is_first = 0;
        ipc_json_rdconf(SESSION, _g_factory_json, ARRSIZE(_g_factory_json));
        _dynamic_factory_param_calculate();
    }
    return &_g_parm;
}

s32 ipc_factory_param_update(pv8 src_file)
{

    ipc_json_rdconf(src_file, _g_factory_json, ARRSIZE(_g_factory_json));

    _dynamic_factory_param_calculate();

    ipc_json_wrconf(SESSION, _g_factory_json, ARRSIZE(_g_factory_json));

    return 0;
}

#define IPC_FACTORY_TEST_BIN_PATH "/mnt/sdcard/ipc_factory_test.bin"

static s32 _check_factory_test_bin_sign(void)
{
    sha512_context sha = { 0 };
    sha512_init(&sha);

    u8 buff[502];
    u8 sign[64];
    s64 file_size = 0;
    s64 offset    = 0;
    s32 ret       = 0;
    FILE* fp      = fopen(IPC_FACTORY_TEST_BIN_PATH, "rb");
    if (!fp) {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    s32 run = 1;
    do {
        size_t read_size = sizeof(buff);

        
        if (offset + read_size > file_size - 64) {
            if (file_size - 64 - offset <= 0) {
                break; 
            }
            read_size = (size_t)(file_size - 64 - offset);
            run       = 0;
        }

        if (read_size > sizeof(buff)) {
            read_size = sizeof(buff);
        }

        size_t bytes_read = fread(buff, 1, read_size, fp);
        if (bytes_read != read_size) {
            if (feof(fp) || ferror(fp)) {
                fclose(fp);
                return -1;
            }
        }

        offset += bytes_read;
        sha512_update(&sha, buff, bytes_read);
    } while (run);

    ret = fread(sign, 1, 64, fp);

    fclose(fp);

    if (ret != 64) {
        return -2;
    }

    u8 sha512sum[64] = { 0 };
    sha512_final(&sha, sha512sum);

    u8 ed25519_pub[] = { 84,  172, 69,  158, 47, 171, 163, 1,  131, 37,  134, 208, 173, 144, 49, 88,
                         208, 204, 202, 152, 62, 235, 53,  77, 55,  147, 177, 117, 218, 39,  34, 152 };

    if (!ed25519_verify(sign, sha512sum, sizeof(sha512sum), ed25519_pub)) {
        printf("factory_test_bin sign error");
        return -1;
    }

    return 0;
}

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void _run_factory_test_library(pv8 args, ipc_factory_misc_cb_f f_misc_cb)
{
    typedef struct ipc_api_s* (*ipc_plat_api_t)(s32 arg);

    typedef s32 (*factory_test_api_t)(ipc_plat_api_t api, pv8 args, ipc_factory_misc_cb_f f_misc_cb);

    void* handle = dlopen("/tmp/factoryapp/" __IPC_ARCH__ "/lib/libfactory_test.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[%s](%d) dlopen get error: %s\n", __FILE__, __LINE__, dlerror());
        return;
    }

    // coverity[RETURNED_NULL :SUPPRESS]
    // coverity[VAR_ASSIGNED :SUPPRESS]
    factory_test_api_t factory_test_api = (factory_test_api_t)dlsym(handle, "factory_test_api");
    
    // coverity[NULL_RETURNS :SUPPRESS]
    factory_test_api(ipc_plat_api, args, f_misc_cb);

    dlclose(handle);
}

static void _factory_test_run_bin(pv8 args, ipc_factory_misc_cb_f f_misc_cb)
{
    s32 ret = 0;
    if ((access(IPC_FACTORY_TEST_BIN_PATH, F_OK) == 0) && (_check_factory_test_bin_sign() == 0)) {
        ret = mkdir("/tmp/factoryapp", 666);
        if (ret < 0) {
            return;
        }
        ipc_exec("mount " IPC_FACTORY_TEST_BIN_PATH " /tmp/factoryapp");
        ipc_exec("/tmp/factoryapp/block_init.sh %s", __IPC_ARCH__);
        ipc_exec("/tmp/factoryapp/init.sh %s &", __IPC_ARCH__);
        _run_factory_test_library(args, f_misc_cb);
    }
}

s32 ipc_factory_try_run(pv8 cloud, pv8 version, ipc_factory_misc_cb_f f_misc_cb)
{
    // coverity[UNUSED_VALUE :SUPPRESS]
    s32 ret         = 0;
    v8 buffer[1024] = { 0 };

    snprintf(buffer, sizeof(buffer), "{\"cloud\":\"%s\",\"version\":\"%s\"}", cloud, version);

    _factory_test_run_bin(buffer, f_misc_cb);

    return ret;
}

static vptr _pth_fty_aging_test(vptr arg)
{
    pv8 aging_log_dir = arg;
    v8 log_file[256];
    // coverity[UNUSED_VALUE :SUPPRESS]
    s32 log_out_fd = -1;

    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();
    if (decrypt == NULL) {
        ipcfatal("Decrypt verify failed");
        return NULL;
    }

    ipc_decrypt_exinfo_p extdecypt = ipc_decrypt_exinfo();
    if (extdecypt == NULL) {
        ipcfatal("Decrypt verify failed");
        return NULL;
    }

    ipc_log_ctrl(0, NULL, IPC_LOG_DEBUG);

    snprintf(log_file, sizeof(log_file), "%s/uptime_%llu_%hu_%hu.txt", aging_log_dir, decrypt->device_id, decrypt->product_type,
             extdecypt->cloud_type);

    FILE* up_fd = fopen(log_file, "a+");
    if (up_fd == NULL) {
        return NULL;
    }

    snprintf(log_file, sizeof(log_file), "%s/runtime_%llu_%hu_%hu.txt", aging_log_dir, decrypt->device_id, decrypt->product_type,
             extdecypt->cloud_type);

    log_out_fd = open(log_file, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (-1 == log_out_fd) {
        ipcerror("opening %s", log_file);
        goto error;
    }

    ipc_exec("cat /proc/kmsg >> %s/kernel_%llu_%hu_%hu.txt &", aging_log_dir, decrypt->device_id, decrypt->product_type, extdecypt->cloud_type);

    if (-1 == dup2(log_out_fd, fileno(stdout))) {
        ipcerror("cannot redirect stdout");
        goto error;
    }

    if (-1 == dup2(log_out_fd, fileno(stderr))) {
        ipcerror("cannot redirect stderr");
        goto error;
    }
    u64 next_time = ipc_mono_ts();
    u64 now       = 0;
    while (1) {
        ipc_sleep(5);

        fsync(log_out_fd);

        ipc_log_ctrl(0, NULL, IPC_LOG_DEBUG);

        now = ipc_mono_ts();
        if (now < next_time) {
            continue;
        }

        next_time = now + 60;

        struct sysinfo info = { 0 };
        if (sysinfo(&info) == 0) {
            fprintf(up_fd, "uptime             : %ld\n", info.uptime);
            fprintf(up_fd, "1 min load average : %lu\n", info.loads[0]);
            fprintf(up_fd, "5 min load average : %lu\n", info.loads[1]);
            fprintf(up_fd, "15 min load average: %lu\n", info.loads[2]);
            fprintf(up_fd, "totalram           : %lu\n", info.totalram);
            fprintf(up_fd, "freeram            : %lu\n", info.freeram);
            fprintf(up_fd, "bufferram          : %lu\n", info.bufferram);
            fprintf(up_fd, "procs              : %u\n--------------------\n", info.procs);
            fflush(up_fd);
        }
    }

error:
    if (up_fd) {
        fclose(up_fd);
        // coverity[UNUSED_VALUE :SUPPRESS]
        up_fd = NULL;
    }

    if (log_out_fd >= 0) {
        close(log_out_fd);
        // coverity[UNUSED_VALUE :SUPPRESS]
        log_out_fd = 0;
    }

    return NULL;
}

void ipc_factory_aging_test(pv8 aging_log_dir)
{
    static u8 inited = 0;
    s32 ret          = 0;
    if (inited) {
        return;
    }
    inited = 1;

    ret = ipc_create_thread("fty_aging_test", _pth_fty_aging_test, aging_log_dir, 16 * 1024, 0);
    if (ret < 0) {
        ipcerror("Create fty_aging_test thread failed! retcode=[%d]", ret);
        return;
    }
}

#define SILENT_REBOOT_FLAG "/conf/ipc_silent_reboot_flag"

EXAPI s32 ipc_factory_create_silent_reboot_flag(void)
{
    s32 fd = creat(SILENT_REBOOT_FLAG, 0777);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }

    return 0;
}

EXAPI s32 ipc_factory_test_and_rm_silent_reboot_flag(void)
{
    s32 ret = 0;

    // CID 21711: Remove access check and directly attempt to remove the file
    // coverity[TOCTOU : SUPPRESS]
    ret = remove(SILENT_REBOOT_FLAG);

    if (ret == 0) {
        // File was successfully removed
        ret = 1;
    } else if (errno == ENOENT) {
        // File didn't exist, which is not an error in this context
        ret = 0;
    } else {
        // Other error occurred, but we still return 0 as per original behavior
        ret = 0;
    }

    return ret;
}
