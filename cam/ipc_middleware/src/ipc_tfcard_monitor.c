#include <fcntl.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ipc_std.h"
#include "ipc_tfcard_monitor.h"

static struct {
    u8 gorun;                          ///< Control thread survival
    u8 alive;                          ///< Thread survival feedback
    u8 format;                         ///< Notification needs formatting flag bit
    u8 remount;                        ///< Notification needs to reload the tf card
    ipc_tfcard_monitor_event_f f_event; ///< Event notification
    v8 filesystem[16];
} _gh_tf[1] = { {
    // coverity[PW.BAD_INITIALIZER_TYPE :SUPPRESS]
    .f_event    = NOT_DO_ANYTHING,
    .filesystem = "auto",
} };

#define FORMAT_CMD "mkfs.vfat"

static pcv8 _get_dev_node(void)
{
    FILE* fp = fopen("/proc/partitions", "r");
    if (fp == NULL)
        return NULL;

    s32 major = 0, minor = 0;
    u64 blocks  = 0;
    v8 name[64] = { 0 }, max_name[64] = { 0 };

    s32 ret = 0;
    pv8 cur = NULL;
    v8 buffer[384];
    // coverity[TAINTED_RETURN_VALUE :SUPPRESS]
    while ((cur = fgets(buffer, sizeof(buffer), fp))) {
        // coverity[SECURE_CODING :SUPPRESS]
        // coverity[DC.STRING_BUFFER :SUPPRESS]
        ret = sscanf(cur, "%d%d%llu%s", &major, &minor, &blocks, name);
        if (ret != 4)
            continue; // Exclude the first few lines of the header
        ipcdebug("major=[%d], minor=[%d], blocks=[%llu], name=[%s]", major, minor, blocks, name);
        if (strncmp(name, "mmcblk", sizeof("mmcblk") - 1))
            continue; // Filter out non-mmcblk beginnings
        if (blocks < 10)
            continue;                     // Memory is too small, filtering
        if (strcmp(max_name, name) < 0) { // Select the largest and longest node
            ipctrace("%s < %s", max_name, name);
            // coverity[TAINTED_DATA_TRANSITIVE :SUPPRESS]
            strncpy(max_name, name, sizeof(max_name));
        }
    }

    fclose(fp);

    if (!max_name[0])
        return NULL;

    static v8 _node_path[128];
    // coverity[TAINTED_DATA_TRANSITIVE :SUPPRESS]
    snprintf(_node_path, sizeof(_node_path), "/dev/%s", max_name);

    // coverity[PATH_MANIPULATION :SUPPRESS]
    if (access(_node_path, F_OK)) {
        ipcfatal("%s not found!", _node_path);
        return NULL;
    }

    ipcdebug("node path=[%s]", _node_path);

    return _node_path;
}

static s32 _format_tfcard(pcv8 dev_node)
{
    ipc_sleep(1);
    ipc_exec("sync");

    s32 ret = ipc_exec("mount | grep %s", TFCARD_PATH);
    if (ret == 0) { // There is a mount, and it must be unmounted successfully to continue
        ret = ipc_exec("umount -fl %s", TFCARD_PATH);
        if (ret != 0)
            return IPC_FAILED;
    }

    ipc_exec("sync");
    ipc_exec("echo 3 > /proc/sys/vm/drop_caches");
    ipc_sleep(1);

    ret = ipc_exec("%s %s", FORMAT_CMD, dev_node);
    if (ret != 0)
        return IPC_FAILED;

    ipc_exec("sync");
    ipc_exec("echo 3 > /proc/sys/vm/drop_caches");
    ipc_sleep(2);

    return ipc_exec("mount -o noexec -t %s %s %s", _gh_tf->filesystem, dev_node, TFCARD_PATH) ? IPC_FAILED : IPC_SUCCESS;
}

static s32 _check_sd_card_is_not_witable(void)
{
    if (access(TFCARD_PATH, W_OK) != 0) {
        return 1;
    }
    // If the SD card directory is writable but the root directory is not writable and the root file system is read-only, then the SD card directory
    // is indeed writable
    if (access("/", W_OK) != 0) {
        return 0;
    }

    // If both the SD card and the root are writable, then the capacity of the TF card needs to be judged to determine if it is really writable
    // In a readable and writable root file system, directories will all indicate writable
    u32 total_k, used_k, free_k = 0;

    ipc_df(TFCARD_PATH, &total_k, &used_k, &free_k);
    // coverity[UNINIT :SUPPRESS]
    return total_k > 32 * 1024 ? 0 : 1;
}

static vptr _pth_tfcard_listen(vptr arg)
{
    s32 ret       = 0;
    pcv8 dev_node = NULL;
    u8 has_card   = 0;
    u8 has_mount  = 0;
    u8 read_only  = 0;

    _gh_tf->alive = 1;

    while (_gh_tf->gorun) {
        dev_node = _get_dev_node();
        if (!dev_node) {
            if (has_card) {
                has_card = 0;
                _gh_tf->f_event(IPC_TFCARD_EVENT_PULL_OUT);
                ipcinfo("Pull out!");
            }
            if (has_mount) {
                has_mount = 0;
                ipcinfo("Umount %s", TFCARD_PATH);
                _gh_tf->f_event(IPC_TFCARD_EVENT_UMOUNT);
                umount2(TFCARD_PATH, MNT_FORCE | MNT_DETACH);
            }
            ipc_sleep(1);
            continue;
        }

        /* There is a node */
        if (!has_card) {
            has_card = 1;
            _gh_tf->f_event(IPC_TFCARD_EVENT_PLUG_IN);
            ipcinfo("Plug in!");
        }

        if (_gh_tf->format) {
            _gh_tf->format = 0;
            ipcinfo("Format start!");
            _gh_tf->f_event(IPC_TFCARD_EVENT_FORMAT_START);
            ret = _format_tfcard(dev_node);
            if (ret != 0) {
                ipcerror("Format failed!");
                _gh_tf->f_event(IPC_TFCARD_EVENT_FORMAT_FAILED);
                has_mount = 0; // Assume no card, remount
                continue;
            } else {
                ipcinfo("Format finish!");
                _gh_tf->f_event(IPC_TFCARD_EVENT_FORMAT_FINISH);
            }
        }

        if (!has_mount) {
            ret = ipc_exec("mount | grep %s", TFCARD_PATH);
            if (ret != 0) { // Not mounted
                ret = ipc_exec("mount -o noexec -t %s %s %s", _gh_tf->filesystem, dev_node, TFCARD_PATH);
                if (ret != 0) { // Mounting failed
                    ipcerror("Mount %s -> %s %s!", dev_node, TFCARD_PATH, "failed");
                    _gh_tf->f_event(IPC_TFCARD_EVENT_UNRECOGNIZED);
                    ipc_sleep(3);
                    continue;
                }
                ipcinfo("Mount %s -> %s %s!", dev_node, TFCARD_PATH, "success");
                _gh_tf->remount = 0;
            } else {
                ipcinfo("Already mount %s -> %s!", dev_node, TFCARD_PATH);
            }
            _gh_tf->f_event(IPC_TFCARD_EVENT_MOUNT);
            has_mount = 1;
        }

        /* The card has been mounted */
        if (_check_sd_card_is_not_witable()) {
            read_only++;
            if (read_only > 2) {
                read_only = 0;
                ipcerror("Read Only!!!");
                _gh_tf->remount = 1;
                _gh_tf->f_event(IPC_TFCARD_EVENT_READONLY);
            }
        } else {
            read_only = 0;
        }

        if (_gh_tf->remount) {
            _gh_tf->remount = 0;
            ipcinfo("Try remount!");
            ret = ipc_exec("mount -o remount,rw %s", TFCARD_PATH);
            ipc_sleep(1);
            if (ret != 0 && _check_sd_card_is_not_witable()) { // Remount failed or still read-only, directly umount
                _gh_tf->f_event(IPC_TFCARD_EVENT_UMOUNT);
                umount2(TFCARD_PATH, MNT_FORCE | MNT_DETACH);
                has_mount = 0;
                continue;
            }
            _gh_tf->f_event(IPC_TFCARD_EVENT_REMOUNT);
        }

        ipc_sleep(1);
    }

    _gh_tf->alive = 0;

    return NULL;
}

void ipc_tfcard_format(void)
{
    _gh_tf->format = 1;
}

void ipc_tfcard_remount(void)
{
    _gh_tf->remount = 1;
}

pv8 ipc_tfcard_monitor_get_current_filesystem(void)
{
    return _gh_tf->filesystem;
}

s32 ipc_tfcard_monitor_init(ipc_tfcard_monitor_event_f f_event)
{
    clog_init("tf", "TFcard monitor");

    if (f_event)
        _gh_tf->f_event = f_event;
    _gh_tf->gorun = 1;
    s32 ret       = ipc_create_thread("ipc_tfcard", _pth_tfcard_listen, NULL, 128 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        return ret;
    }
    ipcinfo("Init complete!");
    return ret;
}

void ipc_tfcard_monitor_uninit(u8 is_wait)
{
    _gh_tf->gorun = 0;
    if (!is_wait)
        return;
    while (_gh_tf->alive)
        ipc_msleep(100);
    ipcinfo("Exit complete!");
}

#ifdef TFCARD_TEST

s32 main()
{
    ipc_tfcard_monitor_init(NULL);

    v8 ch = 0;
    while ((ch = getchar()) != 'q') {

        if (ch == 'r') {
            ipc_tfcard_remount();
        } else if (ch == 'f') {
            ipc_tfcard_format();
        }
    }
    ipc_tfcard_monitor_uninit(1);

    return 0;
}

#endif
