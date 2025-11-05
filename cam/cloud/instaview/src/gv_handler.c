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

#define SESSION "instaview"

static ipc_dev_info_t g_dev_info;

void ipc_handler_read_int(pv8 key, ps32 value)
{
    ipc_json_t json[] = {json_int(key, *value)};
    ipc_json_rdconf(SESSION, json, ARRSIZE(json));
}
void ipc_handler_write_int(pv8 key, s32 value)
{
    ipc_json_t json[] = {json_int(key, value)};
    ipc_json_wrconf(SESSION, json, ARRSIZE(json));
}


int MFG_GetWifiIPAddress(char* ip_addr, const int ip_addr_size)
{
    s32 ret = -1;
    if (ip_addr == NULL) {
        goto _exit;
    }

    s32 i = 0;
    s32 sockfd;

    struct ifconf ifconf;
    u8 buf[512] = {0};
    struct ifreq *ifreq;
    struct sockaddr_in *sin = NULL;

    ifconf.ifc_len = 512;
    ifconf.ifc_buf = (caddr_t)buf;

    u8 *addr = NULL;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        goto _exit;
    }

    ioctl(sockfd, SIOCGIFCONF, &ifconf);

    ifreq = (struct ifreq *)buf;
    for (i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; i--)
    {
        if (strcmp(ifreq->ifr_name, "lo") != 0)
        {
            sin = ((struct sockaddr_in *)&(ifreq->ifr_addr));
            addr = (u8 *)&(sin->sin_addr.s_addr);
            sprintf(ip_addr, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
        }
        ifreq++;
    }

    close(sockfd);

    ret = 0;
_exit:
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