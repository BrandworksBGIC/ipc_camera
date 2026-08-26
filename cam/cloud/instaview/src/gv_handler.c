#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "ipc_handler.h"

static ipc_dev_info_t g_dev_info;

void ipc_handler_read_int(pv8 key, ps32 value)
{
    ipc_json_t json[] = {json_int(key, *value)};
    ipc_json_rdconf(IPC_IV_CONFIG_SESSION, json, ARRSIZE(json));
}

void ipc_handler_write_int(pv8 key, s32 value)
{
    ipc_json_t json[] = {json_int(key, value)};
    ipc_json_wrconf(IPC_IV_CONFIG_SESSION, json, ARRSIZE(json));
}

int MFG_GetWifiIPAddress(char* ip_addr, const int ip_addr_size)
{
    s32 ret = IPC_FAILED;
    if (ip_addr == NULL || ip_addr_size <= 0) {
        return IPC_INVALID_ARGS;
    }
    ip_addr[0] = '\0';

    s32 sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        goto _exit;
    }

    struct ifconf ifconf = {0};
    u8 buf[512] = {0};
    ifconf.ifc_len = sizeof(buf);
    ifconf.ifc_buf = (caddr_t)buf;

    if (ioctl(sockfd, SIOCGIFCONF, &ifconf) < 0) {
        ret = IPC_IOCTL_ERROR;
        goto _exit;
    }
    if (ifconf.ifc_len < 0 || ifconf.ifc_len > (s32)sizeof(buf)) {
        ret = IPC_OUT_OF_RANGE;
        goto _exit;
    }

    struct ifreq* ifreq = (struct ifreq*)buf;
    s32 interface_count = ifconf.ifc_len / (s32)sizeof(*ifreq);
    for (s32 i = 0; i < interface_count; i++, ifreq++) {
        if (strncmp(ifreq->ifr_name, "lo", IFNAMSIZ) == 0
            || ifreq->ifr_addr.sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* sin = (struct sockaddr_in*)&ifreq->ifr_addr;
        if (!inet_ntop(AF_INET, &sin->sin_addr, ip_addr, (socklen_t)ip_addr_size)) {
            ret = errno == ENOSPC ? IPC_NOBUF : IPC_FAILED;
        } else {
            ret = IPC_SUCCESS;
            break;
        }
    }

_exit:
    if (sockfd >= 0) {
        close(sockfd);
    }
    return ret;
}

ipc_dev_info_p ipc_handler_get_dev_info(void)
{
    return &g_dev_info;
}

s32 ipc_handler_get_mac(nw_mac_p mac)
{
    s32 ret = -1;

    if (NULL == mac) {
        goto _exit;
    }

    FILE *pp = popen("ifconfig " "wlan0", "r");
    if (pp == NULL) {
        goto _exit;
    }

    char tmp[256] = {0};
    while (fgets(tmp, sizeof(tmp), pp) != NULL) {
        char *pMACStart = strstr(tmp, "HWaddr ");
        if (pMACStart != NULL) {
            int x1, x2, x3, x4, x5, x6;
            sscanf(pMACStart + strlen("HWaddr "), "%x:%x:%x:%x:%x:%x", &x1, &x2, &x3, &x4, &x5, &x6);
            mac->mac[0] = x1 & 0xFF;
            mac->mac[1] = x2 & 0xFF;
            mac->mac[2] = x3 & 0xFF;
            mac->mac[3] = x4 & 0xFF;
            mac->mac[4] = x5 & 0xFF;
            mac->mac[5] = x6 & 0xFF;
            break;
        }
    }

    pclose(pp);
    printf("WIFI Get MAC %02X-%02X-%02X-%02X-%02X-%02X\n",
           mac->mac[0], mac->mac[1], mac->mac[2], mac->mac[3], mac->mac[4], mac->mac[5]);

    ret = 0;
_exit:
    return ret;
}
