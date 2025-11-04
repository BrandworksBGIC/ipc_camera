#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#include "ipc_unix_socket.h"

typedef struct {
    s32 fd;
    struct sockaddr_un src;
    struct sockaddr_un dest;
} wpa_t, *wpa_p; 

void ipc_unix_socket_close(vptr h_this)
{
    if (!h_this) return ;

    wpa_p h_wpa = (wpa_p)h_this;
    unlink(h_wpa->src.sun_path);
    if (h_wpa->fd >= 0) close(h_wpa->fd);
    ipc_free(h_wpa);
}

vptr ipc_unix_socket_open(pv8 src_path, pv8 dest_path)
{
    if (!src_path || !src_path[0] || !dest_path || !dest_path[0]) return NULL;
    
    wpa_p h_wpa = ipc_malloc(sizeof(*h_wpa), sizeof(*h_wpa));
    if (h_wpa == NULL) return NULL;

    h_wpa->fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (h_wpa->fd < 0) goto FAILED;

    h_wpa->src.sun_family = AF_UNIX;
    s32 ret = snprintf(h_wpa->src.sun_path, sizeof(h_wpa->src.sun_path), "%s", src_path);
    if (ret < 0 || ret >= sizeof(h_wpa->src.sun_path) - 1) goto FAILED;

    unlink(h_wpa->src.sun_path);
    ret = bind(h_wpa->fd, (struct sockaddr *)&h_wpa->src, sizeof(h_wpa->src));
    if (ret < 0) goto FAILED;

    h_wpa->dest.sun_family = AF_UNIX;
    ret = snprintf(h_wpa->dest.sun_path, sizeof(h_wpa->dest.sun_path), "%s", dest_path);
    if (ret < 0 || ret >= sizeof(h_wpa->dest.sun_path) - 1) goto FAILED;
    
    ret = connect(h_wpa->fd, (struct sockaddr *)&h_wpa->dest, sizeof(h_wpa->dest));
    if (ret < 0) goto FAILED;

    return h_wpa;

FAILED:
    // coverity[RESOURCE_LEAK :SUPPRESS]
    ipc_unix_socket_close(h_wpa);
    return NULL;
}

vptr ipc_unix_socket_recv_only_open(pv8 src_path)
{
    if (!src_path || !src_path[0])
        return NULL;

    wpa_p h_wpa = ipc_malloc(sizeof(*h_wpa), sizeof(*h_wpa));
    if (h_wpa == NULL)
        return NULL;

    h_wpa->fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (h_wpa->fd < 0)
        goto FAILED;

    h_wpa->src.sun_family = AF_UNIX;
    s32 ret               = snprintf(h_wpa->src.sun_path, sizeof(h_wpa->src.sun_path), "%s", src_path);
    if (ret < 0 || ret >= sizeof(h_wpa->src.sun_path) - 1)
        goto FAILED;

    unlink(h_wpa->src.sun_path); /* 防重复 */
    ret = bind(h_wpa->fd, (struct sockaddr*)&h_wpa->src, sizeof(h_wpa->src));
    if (ret < 0)
        goto FAILED;

    return h_wpa;

FAILED:
    // coverity[RESOURCE_LEAK :SUPPRESS]
    ipc_unix_socket_close(h_wpa);
    return NULL;
}

s32 ipc_unix_socket_send(vptr h_this, vptr buf, s32 len)
{   
    if (!h_this || !buf || len <= 0) return IPC_INVALID_ARGS;

    wpa_p h_wpa = (wpa_p)h_this;
    s32 ret = send(h_wpa->fd, buf, len, 0);
    if (ret <= 0) return IPC_WRITE_ERROR;
    return ret;
}

s32 ipc_unix_socket_recv(vptr h_this, vptr buf, s32 max, s32 timeout_ms)
{
    if (!h_this || !buf || max <= 0 || timeout_ms <= 0) return IPC_INVALID_ARGS;

    wpa_p h_wpa = (wpa_p)h_this;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(h_wpa->fd, &rfds);

    s32 ret = select(h_wpa->fd + 1, &rfds, NULL, NULL, &tv);
    if (ret == 0) return IPC_TIMEOUT;
    if (ret < 0 || !FD_ISSET(h_wpa->fd, &rfds)) return IPC_FAILED;
    ret = recv(h_wpa->fd, buf, max, 0);
    if (ret < 0) return IPC_READ_ERROR;
    return ret;
}