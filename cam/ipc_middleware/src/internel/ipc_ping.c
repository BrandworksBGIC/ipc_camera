#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>

#include "ipc_ping.h"

#define PING4_BUFF_SIZE (ICMP_MINLEN + 56)

static u16 _calcsum(pu16 buffer, s32 length)
{
    u32 sum;
    /* initialize sum to zero and loop until length (in words) is 0 */
    for (sum = 0; length > 1; length -= 2) /* sizeof() returns number of bytes, we're interested in number of words */
        sum += *buffer++; /* add 1 word of buffer to sum and proceed to the next */

    /* we may have an extra byte */
    if (length == 1)
        sum += *(pu8)buffer;

    sum = (sum >> 16) + (sum & 0xFFFF); /* add high 16 to low 16 */
    sum += (sum >> 16); /* add carry */
    return ~((u16)(sum & 0xffff));
}

static s32 _dns_resolver(const char *domain, char* ipaddr, struct in_addr *inaddr)
{
    if (!domain || !ipaddr) return -1;

    struct hostent* host=gethostbyname(domain);
    if (!host) {
        return -1;
    }

    if (NULL != ipaddr)
        strncpy(ipaddr, inet_ntoa(*(struct in_addr*)host->h_addr), 16);

    if (NULL != inaddr)
        memcpy(inaddr, host->h_addr, sizeof(struct in_addr));

    return 0;
}

s32 ipc_ping4(s32 fd, pv8 ip, s32 time_out_tms)
{
    s32 ret = 0;
    u32 start_ts = ipc_mono_ts();
    u16 cur_pid  = ipc_getpid();

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(ip);
    if (INADDR_NONE == dest_addr.sin_addr.s_addr) {
        v8 ipaddr[32] = {0};
        ret = _dns_resolver(ip, ipaddr, &dest_addr.sin_addr);
        if (ret < 0) {
            ipcerror("_dns_resolver error\n");
            return IPC_FAILED;
        }

        ipcwarn("====== get %s ip %s ======\n", ip, ipaddr);
    }

    static u16 seq = 0;
    u8 ping4_buffer[PING4_BUFF_SIZE] = {0};

    struct icmp* pkt = (struct icmp*)ping4_buffer;
    pkt->icmp_type  = ICMP_ECHO;
    pkt->icmp_seq   = htons(++seq);
    pkt->icmp_id    = cur_pid;
    pkt->icmp_cksum = _calcsum((pu16)ping4_buffer, sizeof(ping4_buffer));
    ret = sendto(fd, pkt, sizeof(ping4_buffer), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (ret < 0) {
        ipcerror("Send icmp failed! ip=[%s] errmsg=[%s]", ip, strerror(errno));
        return IPC_WRITE_ERROR;
    }

    struct timeval interval = { time_out_tms / 1000, (time_out_tms % 1000) * 1000 };
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &interval, sizeof(struct timeval));
    if (ret < 0) {
        ipcdebug("SO_RCVTIMEO failed!");
    }

    while (1) {

        if (ipc_mono_ts() > start_ts + time_out_tms / 1000) {
            ipcwarn("Recv icmp reply timeout!");
            return IPC_TIMEOUT;
        }

        ret = recv(fd, ping4_buffer, sizeof(ping4_buffer), 0);
        if (ret < 0) {
            if (errno != EINTR) { 
                ipcwarn("Recv icmp reply timeout!");
                return IPC_TIMEOUT;
            }
            return IPC_BREAK_OFF; 
        }

        if (ret == sizeof(ping4_buffer)) { /* ip + icmp */
            struct iphdr* iphdr = (struct iphdr*)ping4_buffer;
            pkt = (struct icmp*)(ping4_buffer + (iphdr->ihl << 2)); /* skip ip hdr */
            if (pkt->icmp_id   == cur_pid 
             && pkt->icmp_type == ICMP_ECHOREPLY) {
                ipcdebug("Accept icmp reply");
                return IPC_SUCCESS;
            }
        }
    }
}

s32 ipc_ping4_init(pv8 net_dev)
{
    clog_init("ping", "ping");

    s32 sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        ipcerror("Create icmp socket failed! errmsg=[%s]", strerror(errno));
        return IPC_OPEN_ERROR;
    }

    s32 optval = PING4_BUFF_SIZE + 10;

    // CID 21623: Add return value check for setsockopt
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0) {
        ipcerror("Failed to set socket receive buffer size: %s", strerror(errno));
        // Continue despite this error as it's not critical
    }

    if (NULL != net_dev) {
        struct ifreq nif = { 0 };
        strncpy(nif.ifr_name, net_dev, sizeof(nif.ifr_name) - 1);

        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char*)&nif, sizeof(nif)) < 0) {
            ipcerror("Error, %s bind interface %s fail\n", __func__, net_dev);
            close(sockfd);
            return IPC_FAILED;
        }
    }

    ipcinfo("Create icmp socket success!");
    return sockfd;
}

void ipc_ping4_uninit(s32 fd)
{  
    if (fd < 0) return ;
    ipcinfo("Close icmp socket!");
    close(fd); 
}

s32 ipc_get_route(pv8 ip, pv8 net_dev)
{
    v8 cmd[128] = {0};
    v8 readline[100] = {0};

    snprintf(cmd, sizeof(cmd), "route -n | grep %s", net_dev);
    FILE* fp = popen(cmd, "r");
    if (fp == NULL) return IPC_FAILED;
 
#define IGNORE "%*[^\t \n]%*[\t \n]"
#define SELECT "%[^\t \n]%*[\t \n]"
#define ROUTE  SELECT

    while(fgets(readline, sizeof(readline), fp)) {
        // coverity[SECURE_CODING :SUPPRESS]
        sscanf(readline, IGNORE ROUTE, ip);
        if (!strcmp("0.0.0.0", ip)) continue;
        if (!strcmp("*", ip)) continue;
        pclose(fp);
        return IPC_SUCCESS;
    }

    pclose(fp);
    ip[0] = '\0';

    return IPC_FAILED;
}

#ifdef PING_TEST

s32 main(void)
{
    v8 ip[64] = {0};
    ipc_get_route(ip, "enp1s0");
    printf("%s\n", ip);
    s32 fd = ipc_ping4_init();
    s32 ret = ipc_ping4(fd, ip, 3000);
    printf("%d\n", ret);

    return 0;
}

#endif


