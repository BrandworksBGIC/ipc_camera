#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

#include "cJSON.h"
#include "ipc_handler.h"

#define IPC_IV_CIPHER_BLOCK_SIZE 16
#define IPC_IV_MAX_PLAIN_SIZE    179999
#define IPC_IV_STORAGE_FALLBACK_SYNC_MS 10000
#define IPC_IV_STORAGE_DEBOUNCE_MS      500
#define IPC_IV_HEADER_MAGIC_SIZE 8
#define IPC_IV_MAX_WATCH_NUM     64

typedef struct __PACKED {
    u8 magic[IPC_IV_HEADER_MAGIC_SIZE];
    u32 plain_len;
    u32 checksum;
} ipc_iv_encrypted_header_t;

typedef struct {
    pcv8 legacy_path;
    pcv8 file_name;
} ipc_iv_legacy_file_t;

typedef struct {
    s32 wd;
    v8 path[384];
} ipc_iv_watch_t;

static const u8 _g_encrypted_magic[IPC_IV_HEADER_MAGIC_SIZE] = {
    'I', 'P', 'C', 'C', 'F', 'G', '0', '1'
};

static const ipc_iv_legacy_file_t _g_legacy_files[] = {
    { "/conf/iv.cfg", "iv.cfg" },
    { "/conf/iv_key.pem", "iv_key.pem" },
    { "/conf/iv_cert.pem", "iv_cert.pem" },
    { "/conf/svr_addr.cfg", "svr_addr.cfg" },
    { "/conf/cert.pem", "cert.pem" },
    { "/conf/setting.cfg", "setting.cfg" },
    { "/conf/set.cfg", "set.cfg" },
};

#define IPC_IV_LEGACY_MBEDTLS_PATH "/conf/iv_mbedtls_conf"
#define IPC_IV_MBEDTLS_NAME        "iv_mbedtls_conf"

static pthread_mutex_t _g_storage_mutex = PTHREAD_MUTEX_INITIALIZER;
static u8 _g_storage_stopped = 0;
static pthread_t _g_monitor_thread;
static s32 _g_inotify_fd = -1;
static s32 _g_monitor_stop_pipe[2] = { -1, -1 };
static u8 _g_monitor_started = 0;
static ipc_iv_watch_t _g_watches[IPC_IV_MAX_WATCH_NUM];
static s32 _g_watch_num = 0;

static u32 _storage_checksum(pcu8 data, size_t len)
{
    u32 checksum = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
        checksum *= 16777619U;
    }
    return checksum;
}

static s32 _join_path(pcv8 parent, pcv8 name, pv8 path, size_t path_size)
{
    s32 len = snprintf(path, path_size, "%s/%s", parent, name);
    return len > 0 && len < (s32)path_size ? IPC_SUCCESS : IPC_INVALID_ARGS;
}

static s32 _remove_if_exists(pcv8 path)
{
    s32 ret = ipc_rm((pv8)path);
    return ret == IPC_NOT_FOUND ? IPC_SUCCESS : ret;
}

static s32 _read_file(pcv8 path, pu8* data, size_t* data_len, mode_t* file_mode)
{
    if (!path || !data || !data_len) {
        return IPC_INVALID_ARGS;
    }

    s32 fd = open(path, O_RDONLY);
    if (fd < 0) {
        return errno == ENOENT ? IPC_NOT_FOUND : IPC_OPEN_ERROR;
    }

    struct stat stat_info;
    if (fstat(fd, &stat_info) != 0 || !S_ISREG(stat_info.st_mode) || stat_info.st_size < 0
        || stat_info.st_size > (off_t)(sizeof(ipc_iv_encrypted_header_t) + IPC_IV_CIPHER_BLOCK_SIZE
                                       + IPC_IV_MAX_PLAIN_SIZE)) {
        close(fd);
        return IPC_READ_ERROR;
    }

    size_t len = (size_t)stat_info.st_size;
    pu8 buff = malloc(len + 1);
    if (!buff) {
        close(fd);
        return IPC_NOMEM;
    }

    size_t offset = 0;
    while (offset < len) {
        ssize_t read_len = read(fd, buff + offset, len - offset);
        if (read_len < 0 && errno == EINTR) {
            continue;
        }
        if (read_len <= 0) {
            free(buff);
            close(fd);
            return IPC_READ_ERROR;
        }
        offset += (size_t)read_len;
    }

    close(fd);
    buff[len] = '\0';
    *data = buff;
    *data_len = len;
    if (file_mode) {
        *file_mode = stat_info.st_mode & 0777;
    }
    return IPC_SUCCESS;
}

static s32 _write_file_atomic(pcv8 path, pcu8 data, size_t data_len, mode_t file_mode)
{
    v8 tmp_path[384];
    s32 path_len = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (path_len <= 0 || path_len >= (s32)sizeof(tmp_path)) {
        return IPC_INVALID_ARGS;
    }

    mode_t mode = file_mode ? file_mode : 0600;
    s32 fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        return IPC_OPEN_ERROR;
    }

    size_t offset = 0;
    while (offset < data_len) {
        ssize_t write_len = write(fd, data + offset, data_len - offset);
        if (write_len < 0 && errno == EINTR) {
            continue;
        }
        if (write_len <= 0) {
            close(fd);
            unlink(tmp_path);
            return IPC_WRITE_ERROR;
        }
        offset += (size_t)write_len;
    }

    if (fsync(fd) != 0 || fchmod(fd, mode) != 0) {
        close(fd);
        unlink(tmp_path);
        return IPC_WRITE_ERROR;
    }
    if (close(fd) != 0) {
        unlink(tmp_path);
        return IPC_WRITE_ERROR;
    }
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return IPC_WRITE_ERROR;
    }
    return IPC_SUCCESS;
}

static s32 _encrypt_file(pcv8 persistent_path, pcu8 plain, size_t plain_len, mode_t file_mode)
{
    if (plain_len > IPC_IV_MAX_PLAIN_SIZE) {
        return IPC_OUT_OF_RANGE;
    }

    size_t cipher_len = ((plain_len / IPC_IV_CIPHER_BLOCK_SIZE) + 1) * IPC_IV_CIPHER_BLOCK_SIZE;
    size_t storage_len = sizeof(ipc_iv_encrypted_header_t) + cipher_len;
    pu8 storage = calloc(1, storage_len);
    if (!storage) {
        return IPC_NOMEM;
    }

    ipc_iv_encrypted_header_t* header = (ipc_iv_encrypted_header_t*)storage;
    memcpy(header->magic, _g_encrypted_magic, sizeof(header->magic));
    header->plain_len = (u32)plain_len;
    header->checksum = _storage_checksum(plain, plain_len);

    pu8 cipher = storage + sizeof(*header);
    memcpy(cipher, plain, plain_len);
    u8 padding = (u8)(cipher_len - plain_len);
    memset(cipher + plain_len, padding, padding);
    key_manage_encrypt_with_conf_key_1((pv8)persistent_path, (pv8)cipher, (s32)cipher_len);

    s32 ret = _write_file_atomic(persistent_path, storage, storage_len, file_mode);
    memset(storage, 0, storage_len);
    free(storage);
    return ret;
}

static s32 _decrypt_file(pcv8 persistent_path, pu8* plain, size_t* plain_len, mode_t* file_mode)
{
    pu8 storage = NULL;
    size_t storage_len = 0;
    mode_t mode = 0;
    s32 ret = _read_file(persistent_path, &storage, &storage_len, &mode);
    if (ret < 0) {
        return ret;
    }

    if (storage_len <= sizeof(ipc_iv_encrypted_header_t)) {
        free(storage);
        return IPC_NOT_MATCH;
    }

    ipc_iv_encrypted_header_t* header = (ipc_iv_encrypted_header_t*)storage;
    if (memcmp(header->magic, _g_encrypted_magic, sizeof(header->magic)) != 0) {
        free(storage);
        return IPC_NOT_MATCH;
    }

    size_t cipher_len = storage_len - sizeof(*header);
    if (cipher_len == 0 || cipher_len % IPC_IV_CIPHER_BLOCK_SIZE != 0 || header->plain_len >= cipher_len) {
        free(storage);
        return IPC_VERIFY_FAILED;
    }

    pu8 cipher = storage + sizeof(*header);
    key_manage_decrypt_with_conf_key_1((pv8)persistent_path, (pv8)cipher, (s32)cipher_len);

    u8 padding = cipher[cipher_len - 1];
    if (padding == 0 || padding > IPC_IV_CIPHER_BLOCK_SIZE
        || (size_t)header->plain_len + padding != cipher_len) {
        memset(storage, 0, storage_len);
        free(storage);
        return IPC_VERIFY_FAILED;
    }
    for (size_t i = cipher_len - padding; i < cipher_len; i++) {
        if (cipher[i] != padding) {
            memset(storage, 0, storage_len);
            free(storage);
            return IPC_VERIFY_FAILED;
        }
    }
    if (_storage_checksum(cipher, header->plain_len) != header->checksum) {
        memset(storage, 0, storage_len);
        free(storage);
        return IPC_VERIFY_FAILED;
    }

    pu8 output = malloc((size_t)header->plain_len + 1);
    if (!output) {
        memset(storage, 0, storage_len);
        free(storage);
        return IPC_NOMEM;
    }
    memcpy(output, cipher, header->plain_len);
    output[header->plain_len] = '\0';
    *plain = output;
    *plain_len = header->plain_len;
    if (file_mode) {
        *file_mode = mode;
    }

    memset(storage, 0, storage_len);
    free(storage);
    return IPC_SUCCESS;
}

static s32 _validate_json(pu8 data, size_t data_len)
{
    if (!data || data_len == 0) {
        return IPC_PARSE_FAILED;
    }
    data[data_len] = '\0';
    cJSON* root = cJSON_Parse((pv8)data);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return IPC_PARSE_FAILED;
    }
    cJSON_Delete(root);
    return IPC_SUCCESS;
}

static s32 _validate_business_config(pcu8 expected, size_t expected_len)
{
    pu8 encrypted = NULL;
    size_t encrypted_len = 0;
    s32 ret = _read_file(IPC_IV_CONFIG_FILE, &encrypted, &encrypted_len, NULL);
    if (ret < 0) {
        return ret;
    }

    key_manage_decrypt_with_conf_key_1(IPC_IV_CONFIG_FILE, (pv8)encrypted, (s32)encrypted_len);
    ret = _validate_json(encrypted, encrypted_len);
    if (ret == IPC_SUCCESS && expected
        && (encrypted_len != expected_len || memcmp(encrypted, expected, expected_len) != 0)) {
        ret = IPC_NOT_MATCH;
    }
    memset(encrypted, 0, encrypted_len);
    free(encrypted);
    return ret;
}

static s32 _encrypt_business_config(pcu8 plain, size_t plain_len, mode_t file_mode)
{
    pu8 encrypted = malloc(plain_len + 1);
    if (!encrypted) {
        return IPC_NOMEM;
    }
    memcpy(encrypted, plain, plain_len);
    encrypted[plain_len] = '\0';
    key_manage_encrypt_with_conf_key_1(IPC_IV_CONFIG_FILE, (pv8)encrypted, (s32)plain_len);
    s32 ret = _write_file_atomic(IPC_IV_CONFIG_FILE, encrypted, plain_len, file_mode);
    memset(encrypted, 0, plain_len);
    free(encrypted);
    return ret;
}

static s32 _migrate_business_config(void)
{
    s32 current_ret = _validate_business_config(NULL, 0);
    if (current_ret == IPC_SUCCESS) {
        return _remove_if_exists(IPC_IV_LEGACY_CONFIG_FILE);
    }

    pu8 current_plain = NULL;
    size_t current_plain_len = 0;
    mode_t current_mode = 0;
    s32 raw_ret = _read_file(IPC_IV_CONFIG_FILE, &current_plain, &current_plain_len, &current_mode);
    if (raw_ret == IPC_SUCCESS && _validate_json(current_plain, current_plain_len) == IPC_SUCCESS) {
        s32 ret = _encrypt_business_config(current_plain, current_plain_len, current_mode);
        if (ret == IPC_SUCCESS) {
            ret = _validate_business_config(current_plain, current_plain_len);
        }
        memset(current_plain, 0, current_plain_len);
        free(current_plain);
        if (ret == IPC_SUCCESS) {
            return _remove_if_exists(IPC_IV_LEGACY_CONFIG_FILE);
        }
        return ret;
    }
    free(current_plain);

    pu8 legacy = NULL;
    size_t legacy_len = 0;
    mode_t legacy_mode = 0;
    s32 ret = _read_file(IPC_IV_LEGACY_CONFIG_FILE, &legacy, &legacy_len, &legacy_mode);
    if (ret == IPC_NOT_FOUND) {
        return current_ret == IPC_NOT_FOUND ? IPC_SUCCESS : current_ret;
    }
    if (ret < 0 || _validate_json(legacy, legacy_len) < 0) {
        free(legacy);
        return ret < 0 ? ret : IPC_PARSE_FAILED;
    }

    ret = _encrypt_business_config(legacy, legacy_len, legacy_mode);
    if (ret == IPC_SUCCESS) {
        ret = _validate_business_config(legacy, legacy_len);
    }
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_LEGACY_CONFIG_FILE);
    }
    memset(legacy, 0, legacy_len);
    free(legacy);
    return ret;
}

static s32 _migrate_plain_file(pcv8 legacy_path, pcv8 persistent_path)
{
    struct stat persistent_stat;
    if (lstat(persistent_path, &persistent_stat) == 0) {
        return S_ISREG(persistent_stat.st_mode) ? IPC_SUCCESS : IPC_NOT_MATCH;
    }
    if (errno != ENOENT) {
        return IPC_OPEN_ERROR;
    }

    pu8 legacy = NULL;
    size_t legacy_len = 0;
    mode_t legacy_mode = 0;
    s32 ret = _read_file(legacy_path, &legacy, &legacy_len, &legacy_mode);
    if (ret == IPC_NOT_FOUND) {
        return IPC_SUCCESS;
    }
    if (ret < 0) {
        return ret;
    }

    ret = _encrypt_file(persistent_path, legacy, legacy_len, legacy_mode);
    if (ret == IPC_SUCCESS) {
        pu8 verify = NULL;
        size_t verify_len = 0;
        ret = _decrypt_file(persistent_path, &verify, &verify_len, NULL);
        if (ret == IPC_SUCCESS
            && (verify_len != legacy_len || memcmp(verify, legacy, legacy_len) != 0)) {
            ret = IPC_NOT_MATCH;
        }
        free(verify);
    }
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(legacy_path);
    }
    memset(legacy, 0, legacy_len);
    free(legacy);
    return ret;
}

static s32 _migrate_plain_tree(pcv8 legacy_path, pcv8 persistent_path)
{
    struct stat legacy_stat;
    if (lstat(legacy_path, &legacy_stat) != 0) {
        return errno == ENOENT ? IPC_SUCCESS : IPC_OPEN_ERROR;
    }
    if (S_ISREG(legacy_stat.st_mode)) {
        return _migrate_plain_file(legacy_path, persistent_path);
    }
    if (!S_ISDIR(legacy_stat.st_mode)) {
        return IPC_NOT_MATCH;
    }

    s32 ret = ipc_mkdirs((pv8)persistent_path);
    if (ret < 0) {
        return ret;
    }
    DIR* dir = opendir(legacy_path);
    if (!dir) {
        return IPC_OPEN_ERROR;
    }

    struct dirent* entry = NULL;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        v8 legacy_child[384];
        v8 persistent_child[384];
        if (_join_path(legacy_path, entry->d_name, legacy_child, sizeof(legacy_child)) < 0
            || _join_path(persistent_path, entry->d_name, persistent_child, sizeof(persistent_child)) < 0) {
            ret = IPC_INVALID_ARGS;
            break;
        }
        ret = _migrate_plain_tree(legacy_child, persistent_child);
        if (ret < 0) {
            break;
        }
    }
    closedir(dir);
    return ret;
}

static s32 _restore_file(pcv8 persistent_path, pcv8 runtime_path)
{
    pu8 plain = NULL;
    size_t plain_len = 0;
    mode_t file_mode = 0;
    s32 ret = _decrypt_file(persistent_path, &plain, &plain_len, &file_mode);
    if (ret == IPC_NOT_MATCH) {
        ret = _read_file(persistent_path, &plain, &plain_len, &file_mode);
        if (ret == IPC_SUCCESS) {
            ret = _encrypt_file(persistent_path, plain, plain_len, file_mode);
        }
    }
    if (ret == IPC_SUCCESS) {
        ret = _write_file_atomic(runtime_path, plain, plain_len, file_mode);
    }
    if (plain) {
        memset(plain, 0, plain_len);
        free(plain);
    }
    return ret;
}

static s32 _restore_tree(pcv8 persistent_dir, pcv8 runtime_dir, u8 root_level)
{
    s32 ret = ipc_mkdirs((pv8)runtime_dir);
    if (ret < 0) {
        return ret;
    }

    DIR* dir = opendir(persistent_dir);
    if (!dir) {
        return errno == ENOENT ? IPC_SUCCESS : IPC_OPEN_ERROR;
    }
    struct dirent* entry = NULL;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")
            || strstr(entry->d_name, ".tmp")) {
            continue;
        }
        if (root_level && !strcmp(entry->d_name, "instaview.json")) {
            continue;
        }

        v8 persistent_path[384];
        v8 runtime_path[384];
        if (_join_path(persistent_dir, entry->d_name, persistent_path, sizeof(persistent_path)) < 0
            || _join_path(runtime_dir, entry->d_name, runtime_path, sizeof(runtime_path)) < 0) {
            ret = IPC_INVALID_ARGS;
            break;
        }

        struct stat stat_info;
        if (lstat(persistent_path, &stat_info) != 0) {
            ret = IPC_READ_ERROR;
        } else if (S_ISDIR(stat_info.st_mode)) {
            ret = _restore_tree(persistent_path, runtime_path, 0);
        } else if (S_ISREG(stat_info.st_mode)) {
            ret = _restore_file(persistent_path, runtime_path);
        } else {
            ret = IPC_NOT_MATCH;
        }
        if (ret < 0) {
            break;
        }
    }
    closedir(dir);
    return ret;
}

static s32 _sync_file(pcv8 runtime_path, pcv8 persistent_path)
{
    pu8 runtime = NULL;
    size_t runtime_len = 0;
    mode_t runtime_mode = 0;
    s32 ret = _read_file(runtime_path, &runtime, &runtime_len, &runtime_mode);
    if (ret < 0) {
        return ret;
    }

    pu8 persistent = NULL;
    size_t persistent_len = 0;
    s32 persistent_ret = _decrypt_file(persistent_path, &persistent, &persistent_len, NULL);
    if (persistent_ret == IPC_SUCCESS && persistent_len == runtime_len
        && memcmp(persistent, runtime, runtime_len) == 0) {
        ret = IPC_SUCCESS;
    } else {
        ret = _encrypt_file(persistent_path, runtime, runtime_len, runtime_mode);
    }

    if (persistent) {
        memset(persistent, 0, persistent_len);
        free(persistent);
    }
    memset(runtime, 0, runtime_len);
    free(runtime);
    return ret;
}

static s32 _sync_tree(pcv8 runtime_dir, pcv8 persistent_dir, u8 root_level)
{
    s32 ret = ipc_mkdirs((pv8)persistent_dir);
    if (ret < 0) {
        return ret;
    }

    DIR* runtime = opendir(runtime_dir);
    if (!runtime) {
        return errno == ENOENT ? IPC_SUCCESS : IPC_OPEN_ERROR;
    }
    struct dirent* entry = NULL;
    while ((entry = readdir(runtime))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")
            || strstr(entry->d_name, ".tmp")) {
            continue;
        }

        v8 runtime_path[384];
        v8 persistent_path[384];
        if (_join_path(runtime_dir, entry->d_name, runtime_path, sizeof(runtime_path)) < 0
            || _join_path(persistent_dir, entry->d_name, persistent_path, sizeof(persistent_path)) < 0) {
            ret = IPC_INVALID_ARGS;
            break;
        }
        struct stat stat_info;
        if (lstat(runtime_path, &stat_info) != 0) {
            ret = IPC_READ_ERROR;
        } else if (S_ISDIR(stat_info.st_mode)) {
            ret = _sync_tree(runtime_path, persistent_path, 0);
        } else if (S_ISREG(stat_info.st_mode)) {
            ret = _sync_file(runtime_path, persistent_path);
        } else {
            ret = IPC_NOT_MATCH;
        }
        if (ret < 0) {
            break;
        }
    }
    closedir(runtime);
    if (ret < 0) {
        return ret;
    }

    DIR* persistent = opendir(persistent_dir);
    if (!persistent) {
        return IPC_OPEN_ERROR;
    }
    while ((entry = readdir(persistent))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")
            || strstr(entry->d_name, ".tmp")) {
            continue;
        }
        if (root_level && !strcmp(entry->d_name, "instaview.json")) {
            continue;
        }

        v8 runtime_path[384];
        v8 persistent_path[384];
        if (_join_path(runtime_dir, entry->d_name, runtime_path, sizeof(runtime_path)) < 0
            || _join_path(persistent_dir, entry->d_name, persistent_path, sizeof(persistent_path)) < 0) {
            ret = IPC_INVALID_ARGS;
            break;
        }
        if (access(runtime_path, F_OK) != 0 && errno == ENOENT) {
            ret = _remove_if_exists(persistent_path);
            if (ret < 0) {
                break;
            }
        }
    }
    closedir(persistent);
    return ret;
}

static pcv8 _watch_path(s32 wd)
{
    for (s32 i = 0; i < _g_watch_num; i++) {
        if (_g_watches[i].wd == wd) {
            return _g_watches[i].path;
        }
    }
    return NULL;
}

static void _watch_remove(s32 wd)
{
    for (s32 i = 0; i < _g_watch_num; i++) {
        if (_g_watches[i].wd != wd) {
            continue;
        }
        _g_watch_num--;
        if (i != _g_watch_num) {
            _g_watches[i] = _g_watches[_g_watch_num];
        }
        return;
    }
}

static s32 _watch_add_tree(pcv8 path)
{
    for (s32 i = 0; i < _g_watch_num; i++) {
        if (!strcmp(_g_watches[i].path, path)) {
            return IPC_SUCCESS;
        }
    }
    u32 mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_CREATE | IN_DELETE
        | IN_DELETE_SELF | IN_MOVE_SELF;
    s32 wd = inotify_add_watch(_g_inotify_fd, path, mask);
    if (wd < 0) {
        return IPC_OPEN_ERROR;
    }

    s32 watch_idx = _g_watch_num;
    for (s32 i = 0; i < _g_watch_num; i++) {
        if (_g_watches[i].wd == wd) {
            watch_idx = i;
            break;
        }
    }
    if (watch_idx == _g_watch_num && _g_watch_num >= IPC_IV_MAX_WATCH_NUM) {
        inotify_rm_watch(_g_inotify_fd, wd);
        return IPC_OUT_OF_RANGE;
    }
    _g_watches[watch_idx].wd = wd;
    snprintf(_g_watches[watch_idx].path, sizeof(_g_watches[watch_idx].path), "%s", path);
    if (watch_idx == _g_watch_num) {
        _g_watch_num++;
    }

    DIR* dir = opendir(path);
    if (!dir) {
        return IPC_OPEN_ERROR;
    }
    s32 ret = IPC_SUCCESS;
    struct dirent* entry = NULL;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        v8 child[384];
        if (_join_path(path, entry->d_name, child, sizeof(child)) < 0) {
            ret = IPC_INVALID_ARGS;
            break;
        }
        struct stat stat_info;
        if (lstat(child, &stat_info) == 0 && S_ISDIR(stat_info.st_mode)) {
            ret = _watch_add_tree(child);
            if (ret < 0) {
                break;
            }
        }
    }
    closedir(dir);
    return ret;
}

static u8 _consume_inotify_events(void)
{
    u8 need_sync = 0;
    u8 event_buff[4096] __attribute__((aligned(8)));
    while (1) {
        ssize_t read_len = read(_g_inotify_fd, event_buff, sizeof(event_buff));
        if (read_len < 0 && (errno == EAGAIN || errno == EINTR)) {
            return need_sync;
        }
        if (read_len <= 0) {
            return need_sync;
        }

        size_t offset = 0;
        while (offset < (size_t)read_len) {
            struct inotify_event* event = (struct inotify_event*)(event_buff + offset);
            pcv8 parent = _watch_path(event->wd);

            if (event->mask & IN_IGNORED) {
                _watch_remove(event->wd);
            } else if (parent && event->len && (event->mask & IN_ISDIR)
                       && (event->mask & (IN_CREATE | IN_MOVED_TO))) {
                v8 child[384];
                if (_join_path(parent, event->name, child, sizeof(child)) == IPC_SUCCESS) {
                    _watch_add_tree(child);
                }
            }
            if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE
                               | IN_DELETE_SELF | IN_MOVE_SELF)) {
                need_sync = 1;
            } else if ((event->mask & IN_ISDIR) && (event->mask & IN_CREATE)) {
                need_sync = 1;
            }
            offset += sizeof(*event) + event->len;
        }
    }
}

static vptr _storage_monitor_process(vptr usr_arg)
{
    (void)usr_arg;
    struct pollfd poll_fds[2] = {
        { .fd = _g_inotify_fd, .events = POLLIN },
        { .fd = _g_monitor_stop_pipe[0], .events = POLLIN },
    };

    while (1) {
        s32 poll_ret = poll(poll_fds, ARRSIZE(poll_fds), -1);
        if (poll_ret < 0 && errno == EINTR) {
            continue;
        }
        if (poll_ret < 0) {
            break;
        }
        if (poll_fds[1].revents & POLLIN) {
            break;
        }
        if (!(poll_fds[0].revents & POLLIN)) {
            continue;
        }

        u8 need_sync = _consume_inotify_events();
        while (1) {
            poll_ret = poll(poll_fds, ARRSIZE(poll_fds), IPC_IV_STORAGE_DEBOUNCE_MS);
            if (poll_ret < 0 && errno == EINTR) {
                continue;
            }
            if (poll_ret <= 0 || (poll_fds[1].revents & POLLIN)) {
                break;
            }
            if (poll_fds[0].revents & POLLIN) {
                need_sync |= _consume_inotify_events();
            }
        }
        if (poll_fds[1].revents & POLLIN) {
            break;
        }

        pthread_mutex_lock(&_g_storage_mutex);
        if (need_sync && !_g_storage_stopped) {
            s32 ret = _sync_tree(IPC_IV_RUNTIME_PATH, IPC_IV_STORAGE_PATH, 1);
            if (ret < 0) {
                printf("Error, event-driven Instaview storage sync failed: %d\n", ret);
            }
        }
        pthread_mutex_unlock(&_g_storage_mutex);
    }
    return NULL;
}

static void _storage_monitor_cleanup(void)
{
    if (_g_inotify_fd >= 0) {
        close(_g_inotify_fd);
        _g_inotify_fd = -1;
    }
    if (_g_monitor_stop_pipe[0] >= 0) {
        close(_g_monitor_stop_pipe[0]);
        _g_monitor_stop_pipe[0] = -1;
    }
    if (_g_monitor_stop_pipe[1] >= 0) {
        close(_g_monitor_stop_pipe[1]);
        _g_monitor_stop_pipe[1] = -1;
    }
    _g_watch_num = 0;
}

static s32 _storage_monitor_start(void)
{
    _g_inotify_fd = inotify_init();
    if (_g_inotify_fd < 0 || pipe(_g_monitor_stop_pipe) != 0) {
        _storage_monitor_cleanup();
        return IPC_OPEN_ERROR;
    }
    fcntl(_g_inotify_fd, F_SETFL, O_NONBLOCK);
    fcntl(_g_inotify_fd, F_SETFD, FD_CLOEXEC);
    fcntl(_g_monitor_stop_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(_g_monitor_stop_pipe[1], F_SETFD, FD_CLOEXEC);

    s32 ret = _watch_add_tree(IPC_IV_RUNTIME_PATH);
    if (ret < 0) {
        _storage_monitor_cleanup();
        return ret;
    }
    if (pthread_create(&_g_monitor_thread, NULL, _storage_monitor_process, NULL) != 0) {
        _storage_monitor_cleanup();
        return IPC_THREAD_ERROR;
    }
    _g_monitor_started = 1;
    return IPC_SUCCESS;
}

static void _storage_monitor_stop(void)
{
    if (!_g_monitor_started) {
        _storage_monitor_cleanup();
        return;
    }
    u8 stop = 1;
    while (write(_g_monitor_stop_pipe[1], &stop, sizeof(stop)) < 0 && errno == EINTR) {
    }
    pthread_join(_g_monitor_thread, NULL);
    _g_monitor_started = 0;
    _storage_monitor_cleanup();
}

s32 ipc_handler_storage_init(void)
{
    pthread_mutex_lock(&_g_storage_mutex);
    _g_storage_stopped = 0;

    s32 ret = ipc_mkdirs(IPC_IV_STORAGE_PATH);
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_RUNTIME_PATH);
    }
    if (ret == IPC_SUCCESS) {
        ret = ipc_mkdirs(IPC_IV_RUNTIME_PATH);
    }
    if (ret == IPC_SUCCESS) {
        ret = _migrate_business_config();
    }

    for (s32 i = 0; ret == IPC_SUCCESS && i < ARRSIZE(_g_legacy_files); i++) {
        v8 persistent_path[384];
        ret = _join_path(IPC_IV_STORAGE_PATH, _g_legacy_files[i].file_name,
                         persistent_path, sizeof(persistent_path));
        if (ret == IPC_SUCCESS) {
            ret = _migrate_plain_file(_g_legacy_files[i].legacy_path, persistent_path);
        }
    }

    if (ret == IPC_SUCCESS) {
        v8 persistent_path[384];
        ret = _join_path(IPC_IV_STORAGE_PATH, IPC_IV_MBEDTLS_NAME,
                         persistent_path, sizeof(persistent_path));
        if (ret == IPC_SUCCESS) {
            ret = _migrate_plain_tree(IPC_IV_LEGACY_MBEDTLS_PATH, persistent_path);
        }
    }
    if (ret == IPC_SUCCESS) {
        ret = _restore_tree(IPC_IV_STORAGE_PATH, IPC_IV_RUNTIME_PATH, 1);
    }
    for (s32 i = 0; ret == IPC_SUCCESS && i < ARRSIZE(_g_legacy_files); i++) {
        ret = _remove_if_exists(_g_legacy_files[i].legacy_path);
    }
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_LEGACY_MBEDTLS_PATH);
    }

    pthread_mutex_unlock(&_g_storage_mutex);
    if (ret < 0) {
        printf("Error, initialize encrypted Instaview storage failed: %d\n", ret);
        return ret;
    }

    s32 monitor_ret = _storage_monitor_start();
    if (monitor_ret < 0) {
        printf("Warning, start Instaview storage monitor failed: %d, use fallback sync\n", monitor_ret);
    }
    return IPC_SUCCESS;
}

s32 ipc_handler_storage_sync(vptr usr_arg, pu8 tmp_mem, s32 tmp_mem_size)
{
    (void)usr_arg;
    (void)tmp_mem;
    (void)tmp_mem_size;

    pthread_mutex_lock(&_g_storage_mutex);
    if (_g_storage_stopped || _g_monitor_started) {
        pthread_mutex_unlock(&_g_storage_mutex);
        return -1;
    }
    s32 ret = _sync_tree(IPC_IV_RUNTIME_PATH, IPC_IV_STORAGE_PATH, 1);
    pthread_mutex_unlock(&_g_storage_mutex);
    if (ret < 0) {
        printf("Error, sync encrypted Instaview storage failed: %d\n", ret);
    }
    return IPC_IV_STORAGE_FALLBACK_SYNC_MS;
}

void ipc_handler_storage_shutdown(void)
{
    pthread_mutex_lock(&_g_storage_mutex);
    if (!_g_storage_stopped) {
        s32 ret = _sync_tree(IPC_IV_RUNTIME_PATH, IPC_IV_STORAGE_PATH, 1);
        if (ret < 0) {
            printf("Error, final Instaview storage sync failed: %d\n", ret);
        }
    }
    _g_storage_stopped = 1;
    pthread_mutex_unlock(&_g_storage_mutex);

    _storage_monitor_stop();

    pthread_mutex_lock(&_g_storage_mutex);
    _remove_if_exists(IPC_IV_RUNTIME_PATH);
    pthread_mutex_unlock(&_g_storage_mutex);
}

s32 ipc_handler_storage_reset(void)
{
    pthread_mutex_lock(&_g_storage_mutex);
    _g_storage_stopped = 1;
    pthread_mutex_unlock(&_g_storage_mutex);

    _storage_monitor_stop();

    pthread_mutex_lock(&_g_storage_mutex);
    s32 ret = _remove_if_exists(IPC_IV_RUNTIME_PATH);
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_STORAGE_PATH);
    }
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_LEGACY_CONFIG_FILE);
    }
    for (s32 i = 0; ret == IPC_SUCCESS && i < ARRSIZE(_g_legacy_files); i++) {
        ret = _remove_if_exists(_g_legacy_files[i].legacy_path);
    }
    if (ret == IPC_SUCCESS) {
        ret = _remove_if_exists(IPC_IV_LEGACY_MBEDTLS_PATH);
    }
    pthread_mutex_unlock(&_g_storage_mutex);
    return ret;
}
