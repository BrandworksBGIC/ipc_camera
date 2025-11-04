#include "ipc_log.h"
#include "ipc_memory.h"
#include "ipc_misc.h"
#include "ipc_thread.h"
#include "ipc_time.h"
#include <stdio.h>

#define LOCAL_IDX 0

typedef ipc_log_info_t log_t, *log_p;

typedef struct {
    ipc_lock_t mutex;                 ///< Mutex for processes to prevent concurrent initialization
    u8 glevel[IPC_LOG_CTRL_NUM];      ///< Log level for local printing (globally preset for new log modules)
    u16 proxy_port[IPC_LOG_CTRL_NUM]; ///< UDP proxy port
    u32 pool_size;                   ///< Total size of the log pool
    u32 pool_used;                   ///< Current usage of the log pool
    u16 log_num;                     ///< Number of existing logs
    log_t log_pool[0];               ///< Log poll
} log_man_t, *log_man_p;

// Set the local printing hook function to handle log display
static void (*_g_display_hook_f)(ipc_log_proxy_p proxy);
void ipc_log_display_hook(void (*hook_f)(ipc_log_proxy_p proxy))
{
    _g_display_hook_f = hook_f;
}

// Get the log manager object, which is used to manage the log module and memory allocation
static log_man_p _get_log_man(void)
{
    static log_man_p _h_man = NULL;
    if (_h_man)
        return _h_man;

#ifndef IPC_LOG_MAN_BUFF
#define IPC_LOG_MAN_BUFF 1024
#endif

    u8 first_create = 0;
    _h_man          = ipc_shmalloc('l', IPC_LOG_MAN_BUFF, &first_create);
    if (_h_man == NULL)
        return NULL;

    if (first_create) {
        _h_man->pool_size = 0;
        memset((vptr)_h_man, 0, IPC_LOG_MAN_BUFF);
        ipc_lock_init(_h_man->mutex, IPC_PROCESS_MUTEX);
        _h_man->pool_size = IPC_LOG_MAN_BUFF - sizeof(log_man_t);
    }

    return _h_man;
}

// Get the log object based on the log manager object and the log name
static log_p _get_log_obj(log_man_p h_man, pcv8 name)
{
    log_p h_log = h_man->log_pool;

    for (s32 cur = 0; cur < h_man->log_num; cur++) {
        if (!strncmp(h_log->name, name, IPC_LOG_NAME_LEN))
            return h_log;
        h_log = (log_p)((pv8)h_log + ALIGN(sizeof(log_t) + h_log->desc_len));
    }
    return NULL;
}

// Initialize the log module
s32 ipc_log_init(ipc_log_p h_this, ipc_log_p father, pcv8 name, u8 level, pcv8 desc)
{
    if (!h_this || !name || !name[0])
        return IPC_INVALID_ARGS;
    if (h_this->h_self)
        return IPC_EXIST;
    if (level > IPC_LOG_TRACE)
        level = IPC_LOG_TRACE;
    if (!desc)
        desc = "\0";

    memset(h_this, 0, sizeof(*h_this));

    log_man_p h_man = _get_log_man();
    if (!h_man)
        return IPC_NOMEM;

    u8 desc_len = strlen(desc) + 1;
    u16 add_len = sizeof(log_t) + desc_len;

    ipc_lock(h_man->mutex);

    log_p h_log = _get_log_obj(h_man, name);
    if (h_log == NULL && h_man->pool_used + add_len <= h_man->pool_size) {
        h_log = (log_p)((pv8)h_man->log_pool + h_man->pool_used);
        memset(h_log, 0, sizeof(*h_log));
        h_log->level[LOCAL_IDX] = h_man->glevel[LOCAL_IDX] != IPC_LOG_NONE ? h_man->glevel[LOCAL_IDX] : level;
        h_log->desc_len         = desc_len;
        snprintf(h_log->name, sizeof(h_log->name), "%s", name);
        snprintf(h_log->desc, desc_len, "%s", desc);
        h_man->pool_used += ALIGN(add_len);
        h_man->log_num++;
    }

    ipc_unlock(h_man->mutex);

    if (h_log == NULL)
        return IPC_NOBUF;

    h_this->h_self   = h_log;
    h_this->h_father = father;

    return IPC_SUCCESS;
}

// Determine whether printing is allowed according to the log level and index
static inline u8 _can_print(ipc_log_p h_this, u8 level, s32 idx)
{
    log_p h_log = NULL;

    for (; h_this && h_this->h_self; h_this = h_this->h_father) {
        h_log = h_this->h_self;
        if (level > h_log->level[idx])
            return 0;
    }
    return 1;
}
// Generate log header information
static s32 _gen_header(ipc_log_p h_this, pv8 buff, s32 size)
{
    log_p h_log = h_this->h_self;

    if (h_this->h_father == NULL || ((ipc_log_p)h_this->h_father)->h_self == NULL) {
        return snprintf(buff, size, "[%s]", h_log->name);
    }

    s32 used = _gen_header(h_this->h_father, buff, size);
    if (used >= size)
        return size;

    return used + snprintf(buff + used, size - used, "[%s]", h_log->name);
}

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
// Send log information to the specified UDP port
static inline void _proxy_send(u16 port, vptr data, s32 len)
{
    if (!port)
        return;

    static s32 fd = 0;
    s32 ret = 0;

    if (fd <= 0) {
        fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (fd <= 0)
            return;
    }

    struct sockaddr_in addr[1];
    memset(addr, 0, sizeof(addr));

    addr->sin_family      = AF_INET;
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = inet_addr(LOCAL_HOST);

    ret = sendto(fd, data, len, 0, (struct sockaddr*)addr, sizeof(addr));
    if (ret < 0) {
        ipcerror("sengdto error:%d\n", ret);
    }
}
// Define a mapping relationship between log level and color/tag
static struct {
    u8 color;
    v8 tag;
} _color_map[] = {
    [IPC_LOG_FATAL] = { 41, 'F' }, [IPC_LOG_ERROR] = { 31, 'E' }, [IPC_LOG_WARN] = { 33, 'W' },
    [IPC_LOG_INFO] = { 32, 'I' },  [IPC_LOG_DEBUG] = { 36, 'D' }, [IPC_LOG_TRACE] = { 37, 'T' },
};

// Log display function, which can be hooked by the application layer
s32 ipc_log_display(ipc_log_proxy_p p_proxy, s32 (*printf_f)(pcv8 format, ...))
{
    if (printf_f == NULL)
        printf_f = printf;

    u32 tms = p_proxy->uptime % 1000;
    u32 ts  = p_proxy->uptime / 1000;

    return printf_f("[%u:%02u:%02u.%03u]\033[1m\033[%dm[%c]%s\033[0m", ts / 3600, ts % 3600 / 60, ts % 60, tms,
                    _color_map[p_proxy->level].color, _color_map[p_proxy->level].tag, p_proxy->log_ctx);
}
// Check whether the current log level can be printed
s32 ipc_log_check(ipc_log_p h_this, u8 level)
{
    if (!h_this || !h_this->h_self)
        return IPC_INVALID_ARGS;

    for (s32 idx = 0; idx < IPC_LOG_CTRL_NUM; idx++) {
        if (_can_print(h_this, level, idx))
            return IPC_SUCCESS;
    }
    return IPC_NOT_ALLOW;
}

// Log handling function for printing and forwarding logs
static void _log_handle(ipc_log_p h_this, u8 level, ipc_log_proxy_p p_proxy, s32 log_len)
{
    log_man_p h_man = _get_log_man();
    if (h_man == NULL)
        return;

    p_proxy->level  = level;
    p_proxy->uptime = ipc_mono_tms();
    if (p_proxy->log_ctx[log_len - 1] != '\n') {
        p_proxy->log_ctx[log_len]     = '\n';
        p_proxy->log_ctx[log_len + 1] = '\0';
        log_len += 1;
    }
    p_proxy->log_len = log_len;

    for (s32 idx = 0; idx < IPC_LOG_CTRL_NUM; idx++) {
        if (!_can_print(h_this, level, idx))
            continue;
        if (idx != LOCAL_IDX) {
            _proxy_send(h_man->proxy_port[idx], p_proxy, log_len + sizeof(*p_proxy));
            continue;
        }
        if (_g_display_hook_f) {
            _g_display_hook_f(p_proxy);
        } else {
            ipc_log_display(p_proxy, NULL);
        }
    }
}

// Log printing function, which supports formatting output
void ipc_log_printf(ipc_log_p h_this, u8 level, pcv8 format, ...)
{
    if (!h_this || !h_this->h_self || !level || !format || !format[0])
        return;
    if (ipc_log_check(h_this, level) < 0)
        return;

    v8 buff[512];
    ipc_log_proxy_p p_proxy = (ipc_log_proxy_p)buff;
    s32 max                = sizeof(buff) - sizeof(ipc_log_proxy_t) - 1;

    s32 head_len = _gen_header(h_this, p_proxy->log_ctx, max);
    if (head_len >= max)
        return;
    max -= head_len;

    va_list va_argp;
    va_start(va_argp, format);
    s32 tmp_len = vsnprintf(p_proxy->log_ctx + head_len, max, format, va_argp);
    va_end(va_argp);

    if (tmp_len < max) {
        _log_handle(h_this, level, p_proxy, head_len + tmp_len);
    } else {
        tmp_len += 1;
        v8 buff_var[sizeof(ipc_log_proxy_t) + head_len + tmp_len + 1];
        ipc_log_proxy_p p_proxy_var = (ipc_log_proxy_p)buff_var;
        strncpy(p_proxy_var->log_ctx, p_proxy->log_ctx, head_len);

        va_start(va_argp, format);
        tmp_len = vsnprintf(p_proxy_var->log_ctx + head_len, tmp_len, format, va_argp);
        va_end(va_argp);

        _log_handle(h_this, level, p_proxy_var, head_len + tmp_len);
    }
}

// Log printing function for data in hexadecimal format
void ipc_log_hexdump(ipc_log_p h_this, u8 level, vptr data, s32 len)
{
    if (!h_this || !h_this->h_self || !level || !data || len <= 0)
        return;
    if (ipc_log_check(h_this, level) < 0)
        return;

#ifndef IPC_LOG_LINE_DUMP_NUM
#define IPC_LOG_LINE_DUMP_NUM 16
#endif

    v8 buff[15 + IPC_LOG_LINE_DUMP_NUM * 4];
    s32 line_idx = 0;
    s32 cur_idx  = 0;
    s32 log_len  = 0;

    for (line_idx = 0; line_idx < len; line_idx += IPC_LOG_LINE_DUMP_NUM) {
        log_len = 0;
        memset(buff, 0, sizeof(buff));
        // coverity[SECURE_CODING :SUPPRESS]
        log_len += sprintf(buff + log_len, "%08x:", line_idx);

        for (cur_idx = line_idx; cur_idx < line_idx + IPC_LOG_LINE_DUMP_NUM && cur_idx < len; cur_idx++) {
            // coverity[SECURE_CODING :SUPPRESS]
            log_len += sprintf(buff + log_len, " %02x", ((pu8)data)[cur_idx]);
        }

        for (; cur_idx < line_idx + IPC_LOG_LINE_DUMP_NUM; cur_idx++) {
            // coverity[SECURE_CODING :SUPPRESS]
            log_len += sprintf(buff + log_len, "   ");
        }
        // coverity[SECURE_CODING :SUPPRESS]
        log_len += sprintf(buff + log_len, "    ");

        for (cur_idx = line_idx; cur_idx < line_idx + IPC_LOG_LINE_DUMP_NUM && cur_idx < len; cur_idx++) {
            v8 ch = ((pv8)data)[cur_idx];
            // coverity[SECURE_CODING :SUPPRESS]
            log_len += sprintf(buff + log_len, "%c", ch > 0x7f || ch < 0x20 ? '.' : ch);
        }
        ipc_log_printf(h_this, level, "%s", buff);
    }
}

/***************************************************************************************************/

// Get the index corresponding to the specified proxy port in the log manager object
static s32 _get_ctrl_idx(log_man_p h_man, u16 proxy_port)
{
    if (!proxy_port)
        return LOCAL_IDX;

    for (s32 idx = 1; idx < IPC_LOG_CTRL_NUM; idx++) {
        if (h_man->proxy_port[idx] == proxy_port)
            return idx;
    }
    return IPC_NOT_FOUND;
}

// Register a new proxy port in the log manager object and return the index
static s32 _reg_proxy_port(log_man_p h_man, u16 proxy_port)
{
    for (s32 idx = 1; idx < IPC_LOG_CTRL_NUM; idx++) {
        if (h_man->proxy_port[idx] == 0) {
            h_man->proxy_port[idx] = proxy_port;
            return idx;
        }
    }
    return IPC_NOBUF;
}

// Log control function, used to set the log level or close the log function of a certain module or all modules
s32 ipc_log_ctrl(u16 proxy_port, pv8 name, u8 level)
{
    if (level > IPC_LOG_TRACE)
        level = IPC_LOG_TRACE;

    log_man_p h_man = _get_log_man();
    if (h_man == NULL)
        return IPC_NOMEM;

    s32 ctrl_idx = _get_ctrl_idx(h_man, proxy_port);
    if (ctrl_idx < 0) {
        if (level == IPC_LOG_NONE)
            return IPC_SUCCESS;
        ipc_lock(h_man->mutex);
        ctrl_idx = _reg_proxy_port(h_man, proxy_port);
        ipc_unlock(h_man->mutex);
        if (ctrl_idx < 0)
            return ctrl_idx;
    }

    log_p h_log = h_man->log_pool;

    if (name) {
        h_log = _get_log_obj(h_man, name);
        if (h_log == NULL)
            return IPC_NOT_FOUND;

        h_log->level[ctrl_idx] = level;
        if (level == IPC_LOG_NONE) {
            for (s32 cur = 0; cur < h_man->log_num; cur++) {
                if (h_log->level[ctrl_idx] != IPC_LOG_NONE)
                    return IPC_SUCCESS;
                h_log = (log_p)((pv8)h_log + ALIGN(sizeof(log_t) + h_log->desc_len));
            }
            h_man->proxy_port[ctrl_idx] = 0;
        }
        return IPC_SUCCESS;
    }

    h_man->glevel[ctrl_idx] = level;
    for (s32 cur = 0; cur < h_man->log_num; cur++) {
        h_log->level[ctrl_idx] = level;
        h_log                  = (log_p)((pv8)h_log + ALIGN(sizeof(log_t) + h_log->desc_len));
    }

    if (level == IPC_LOG_NONE) {
        h_man->proxy_port[ctrl_idx] = 0;
    }

    return IPC_SUCCESS;
}

log_p ipc_log_iter(log_p h_log)
{
    log_man_p h_man = _get_log_man();
    if (h_man == NULL)
        return NULL;

    h_log = h_log ? (log_p)((pv8)h_log + ALIGN(sizeof(log_t) + h_log->desc_len)) : h_man->log_pool;
    if ((pv8)h_log - (pv8)h_man->log_pool < h_man->pool_used)
        return h_log;

    return NULL;
}
