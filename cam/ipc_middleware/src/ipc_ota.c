#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "ipc_ota.h"
#include "ipc_alarm.h"
#include "ipc_button_monitor.h"
#include "ipc_env_monitor.h"
#include "ipc_mpp.h"
#include "ipc_network.h"
#include "ipc_ptz.h"
#include "ipc_status_led.h"
#include "ipc_tfcard_monitor.h"


struct mtd_dev_s {
    v8 mtd_size[32];
    v8 mtd_name[32];
};

static struct {
    s32 swdg_fd;
    s32 pack_size;
    s32 now_size;
    u8 has_backup;
    v8 ota_file[35];
    struct mtd_dev_s mtd_dev;
    ipc_file_t file;
} _g_ota[1] = { {
    .pack_size = -1,
} };

static s32 _init_flash_backup_mtd_dev(void);

static s32 _ota_write_backup_head(s32 pack_size)
{

    s32 ret = 0;

    ret = ipc_file_seek(_g_ota->file, 0, IPC_SEEK_HEAD);
    if (ret < 0)
        return ret;

#pragma pack(1)
    struct {
        u8 flag[4];
        u16 desc_len;
        u32 startaddr;
        u32 update_pack_len;
    } ota_bak_head = {
        .flag            = { 'c', 'p', 'o', 'a' },
        .desc_len        = 32,
        .startaddr       = 32,
        .update_pack_len = pack_size,
    };
#pragma pack()

    ret = ipc_file_write(_g_ota->file, (pv8)&ota_bak_head, sizeof(ota_bak_head));
    if (ret < 0)
        return ret;

    ret = ipc_file_seek(_g_ota->file, 32, IPC_SEEK_HEAD);
    if (ret < 0)
        return ret;

    return ret;
}

static void _check_bak_partition_is_valid(pu8 has_backup, s32 pack_size)
{
    s32 mtd_size = strtol(_g_ota->mtd_dev.mtd_size, NULL, 16);
    ipcwarn("partion size: %d, pack_size: %d, pack_size+32: %d\n", mtd_size, pack_size, pack_size+32);
    if (mtd_size < pack_size + 32) {
        *has_backup = 0;
    }
}

s32 ipc_ota_prepare(pv8 path, s32 pack_size, u8 has_backup)
{
    s32 ret = 0;
    clog_init("ota", "OTA upgrade");

    _g_ota->swdg_fd = ipc_swdg_reg(1);
    if (_g_ota->swdg_fd < 0) {
        ipcfatal("Soft watchdog registration failed! retcode=[%d]", _g_ota->swdg_fd);
        return _g_ota->swdg_fd;
    }

    ipcinfo("ota watchdog fd=[%d]", _g_ota->swdg_fd);

    ipc_swdg_feed(_g_ota->swdg_fd, 30);

    /* step 1: Exit all internal modules to free up resources for OTA */
    ipc_alarm_uninit(0);
    ipc_env_monitor_uninit(0);
    ipc_button_monitor_uninit(0);
    ipc_status_led_uninit(0);

    ipc_alarm_uninit(1);
    ipc_env_monitor_uninit(1);
    ipc_button_monitor_uninit(1);
    ipc_status_led_uninit(1);
    ipc_ptz_uninit();
    ipc_tfcard_monitor_uninit(1);
    ipc_mpp_uninit(1);

    /* step 2: Delete all TF card upgrade packages to prevent rollback upgrade by TF card after OTA */
    ipc_exec("rm %s/*all.cppa", TFCARD_PATH);
    ipc_exec("rm %s/*sd.cppa", TFCARD_PATH);
    ipc_exec("rm %s/*ota.cppa", TFCARD_PATH);
    ipc_exec("umount -f %s", TFCARD_PATH);
    ipc_exec("sync");
    ipc_exec("echo 3 > /proc/sys/vm/drop_caches");

    if (has_backup) {
        _init_flash_backup_mtd_dev();
        _check_bak_partition_is_valid(&has_backup, pack_size);
    }

    /* step 3: Open the temporary OTA file, and if needed, write the corresponding header information */
    if (pack_size >= 0) {
        ret = ipc_file_open(_g_ota->file, path, IPC_FILE_WRONLY, __IPC_LOG__);
        if (ret < 0)
            return ret;

        if (has_backup) {
            _ota_write_backup_head(pack_size);
        }
    }

    _g_ota->has_backup = has_backup;
    snprintf(_g_ota->ota_file, sizeof(_g_ota->ota_file), "%s", path);
    _g_ota->pack_size = pack_size;

    ipc_swdg_feed(_g_ota->swdg_fd, 90); // Reset if no feeding at ipc_ota_writing within 90 seconds

    if (pack_size < 0) {
        ipc_swdg_feed(_g_ota->swdg_fd, 5 * 60); // When the external upgrade package file is written, the watchdog timeout is changed to 5 minutes
    }

    return IPC_SUCCESS;
}

s32 ipc_ota_writing(vptr data, s32 len)
{
    ipc_swdg_feed(_g_ota->swdg_fd, 90); // If there is no next one within 90 seconds, commit suicide

    if (_g_ota->pack_size < 0) {
        return 0;
    }

    s32 ret = ipc_file_write(_g_ota->file, data, len);
    if (ret < 0)
        return ret;

    return _g_ota->now_size += ret;
}

static s32 _init_flash_backup_mtd_dev(void)
{
    FILE* fp = fopen("/proc/mtd", "r");
    if (fp == NULL) {
        ipcfatal("Open /proc/mtd failed!");
        return IPC_OPEN_ERROR;
    }

    
    v8 buff[64] = { 0 };
    pv8 cur     = NULL;

    while ((cur = fgets(buff, sizeof(buff), fp))) {
        if (!strstr(cur, "otabak"))
            continue;
        // coverity[DC.STRING_BUFFER :SUPPRESS]
        // coverity[SECURE_CODING :SUPPRESS]
        sscanf(cur, "%[^:] ", _g_ota->mtd_dev.mtd_name);
        // coverity[DC.STRING_BUFFER :SUPPRESS]
        // coverity[SECURE_CODING :SUPPRESS]
        sscanf(cur, "%*s %s %*s", _g_ota->mtd_dev.mtd_size);
        if (!_g_ota->mtd_dev.mtd_name[0])
            continue;
        ipcinfo("%s:%s:%s\n", "otabak", _g_ota->mtd_dev.mtd_name, _g_ota->mtd_dev.mtd_size);
        break;
    }
    fclose(fp);
    return IPC_SUCCESS;
}

static s32 _flash_backup(void)
{

    ipc_exec("flashcp %s /dev/%s -v", _g_ota->ota_file, _g_ota->mtd_dev.mtd_name);

    return IPC_SUCCESS;
}

void ipc_ota_upgrade(void (*f_before_exit)(s32 ret))
{
    if (!f_before_exit)
        f_before_exit = NOT_DO_ANYTHING;

    if ((_g_ota->has_backup) && (_g_ota->pack_size == 0)) {
        _ota_write_backup_head(_g_ota->now_size);
    }

    if (_g_ota->pack_size >= 0) {
        ipc_file_close(_g_ota->file);
    }

    ipc_swdg_feed(_g_ota->swdg_fd, 10 * 60); // In any case, a forced restart is required after 10 minutes to prevent possible physical reasons for
                                            // getting stuck during the flash process

    ipc_net_uninit(1);
    ipc_wifi_sta_disconnect();

    s32 ret = IPC_SUCCESS;
    if (_g_ota->has_backup) {

        ret = _flash_backup(); // The backup partition is completed and the system can be restarted directly
        f_before_exit(ret);

        ipc_exec("reboot");

        exit(-1);
    } else { // Normal upgrade
        ipc_file_copy("/app/bin/updater", "/tmp/updater", __IPC_LOG__);
        ipc_exec("chmod 777 /tmp/updater");
        f_before_exit(ret);
        ipc_exec("/tmp/updater %s &", _g_ota->ota_file); // Here, the updater program is rebooted, so the following exit(0) exits normally
        _exit(0);
    }
}
