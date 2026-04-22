#include <errno.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ipc_decrypt.h"
#include "ipc_log.h"
#include "ipc_network.h"
#include "ipc_ping.h"
#include "ipc_platform_api.h"
#include "ipc_std.h"
#include "ipc_time.h"
#include "ipc_uart.h"
#include "ipc_unix_socket.h"

#include "ipc_factory.h"
#include "ipc_hex_bin.h"
#include "ipc_internel.h"
#include "internel/ipc_wpa.h"

#define WIRED_DEV "eth0"
#define WLAN_DEV "wlan0"
#define RNDIS_DEV "usb0"
#define WIFI_DRIVERS_PATH "/app/drivers"
#define WPA_INTERFACE "/var/run/wpa_supplicant"
#define WPA_INTERFACE_FILE(f) WPA_INTERFACE "/" f
#define WPA_CONF_FILE "/tmp/wpa_supplicant.conf"
#define WPA_CONF_FILE_HEAD "ctrl_interface=/var/run/wpa_supplicant\n\n"

#define UDHCPC_SCRIPT "/tmp/udhcpc.script"

#define STATIC_IP_CONF "/conf/static_ip.conf"

#define WIFI_CHIP_NAME "/tmp/wifi_chip_name"

static struct {
    u8 gorun;                      ///< thread control flag
    u8 alive;                      ///< thread alive feedback
    u8 has_wifi;                   ///< whether wifi is detected
    u8 wifi_module;                ///< wifi module type
    u8 sta_gorun;                  ///< station control flag
    u8 sta_alive;                  ///< station alive feedback
    u8 sta_lock;                   ///< external network configuration lock, prevent reconnection mechanism
    u8 smart_switch;               ///< smart disconnect wifi when ethernet cable plugged/unplugged
    u8 sta_reconn_gorun;           ///< station reconnection thread control
    u8 sta_reconn_alive;           ///< station reconnection thread feedback
    u8 sta_need_to_reconn;         ///< station reconnection mark
    u8 wired_conn_gorun;           ///< ethernet connection thread control flag
    u8 wired_conn_alive;           ///< ethernet connection thread alive feedback
    v8 hostname[32];               ///< custom hostname
    v8 ssid[33];                   ///< wifi ssid
    v8 pwd[65];                    ///< wifi password
    v8 country_code[6];            ///< country code
    ipc_net_event_f f_event;        ///< event notification
    ipc_wireless_signal_f f_signal; ///< signal strength notification
    ipc_4g_info_t info_4g;          ///< 4g info buffer
    u8 had_get_4g_info;            ///< whether 4g module info has been successfully retrieved
    u8 is_supported_ipv6;
    u32 next_reboot_wireless_mono_s;
    u8 wired_card_failure;    ///< network card hardware failure flag
    u8 wired_dhipc_fail_count; ///< ethernet DHCP continuous failure count
    u64 wired_last_rx_bytes;  ///< ethernet last RX bytes count
} _gh_net = {
    .hostname = "Smart_Camera",
    .f_event  = NOT_DO_ANYTHING,
    .f_signal = NOT_DO_ANYTHING,
};

/******************************** wifi module ********************************/

enum {
    MODULE_WIFI_RTL8188,
    MODULE_WIFI_RTL8733bu,
    MODULE_WIFI_ATBM6032,
    MODULE_WIFI_ATBM6062,
    MODULE_WIFI_SV6255,
    MODULE_4G_EG800G_EU,
    MODULE_4G_EC800E_CN,
    MODULE_4G_EC800K_CN,
    MODULE_WIFI_ATBM6012,
    MODULE_WIFI_ATBM6132,
    MODULE_4G_LE370,
    MODULE_WIFI_ATBM6062C,
    MODULE_WIFI_ATBM6132C,
    WIFI_MODULE_MAX,
};

typedef struct module_4G_command {
    pv8 command;
    struct {
        // return 1 if condition is correct, 0 if not correct
        s32 (*result_cb)(s32 uart_fd, pv8 result, void* _user);
        pv8 result;             // result string
        s32 inverse_condition;  // whether to invert judgment 0:normal, 1:inverted
        s32 next_command_index; // next command index, -1 means end and exit
    } recve[3];
    s32 read_timeout_ms;
} MODULE_4G_CMD_S, *P_MODULE_4G_CMD_S;

#define NEW_WIRELESS_MODULE(module_name, usb_enum, driver_name, expand_args, module_type, support_ble)                                               \
    [module_name] = { #module_name, usb_enum, driver_name, expand_args, module_type, support_ble }

static struct {
    pcv8 module_name;
    pcv8 usb_enum;
    pcv8 driver_name;
    pcv8 expand_args;
    cs32 module_type;
    v8 support_ble;
} _g_wifi_map[WIFI_MODULE_MAX] = {
    NEW_WIRELESS_MODULE(MODULE_WIFI_RTL8188, "0bda:f179", "8188fu", "", WIRELESS_MODULE_WIFI, 0),
    NEW_WIRELESS_MODULE(MODULE_WIFI_RTL8733bu, "0bda:b733", "8733bu", "", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6032, "007a:8888", "atbm603x_wifi_usb", "", WIRELESS_MODULE_WIFI, 0),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6062, "007a:6052", "atbm606x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6062C, "007a:6055", "ATBM6062C_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6132C, "007a:6162", "atbm6132c_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_SV6255, "8065:6000", "ssv6x5x", "stacfgpath=" WIFI_DRIVERS_PATH "/ssv6x5x-wifi.cfg", WIRELESS_MODULE_WIFI, 0),
    NEW_WIRELESS_MODULE(MODULE_4G_EG800G_EU, "2c7c:0904", "EG800G_EU", "/dev/ttyUSB0", WIRELESS_MODULE_4G, 0),
    NEW_WIRELESS_MODULE(MODULE_4G_EC800E_CN, "2c7c:0903", "EC800E_CN", "/dev/ttyUSB1", WIRELESS_MODULE_4G, 0),
    NEW_WIRELESS_MODULE(MODULE_4G_EC800K_CN, "2c7c:6002", "EC800K_CN", "/dev/ttyUSB1", WIRELESS_MODULE_4G, 0),
    NEW_WIRELESS_MODULE(MODULE_4G_LE370, "2cb7:0d01", "LE370", "/dev/ttyACM0", WIRELESS_MODULE_4G, 0),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6012, "007a:888b", "atbm603x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6132, "007a:8890", "atbm613x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),

};

static s32 _ifconfig(pv8 dev, s8 sw);
static s64 _get_network_stats(pv8 dev);

static u8 _hex_char_to_bin(v8 c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }

    return 0xff;
}

static u8 _hex_to_bin(pv8 p)
{
    u8 tmp = 0;

    if (strlen(p) < 2) {
        return 0;
    }

    tmp = (u8)(_hex_char_to_bin(p[0]) << 4) | _hex_char_to_bin(p[1]);
    return tmp;
}

static s32 _lock_net_device_mac(pv8 net_dev)
{
#define ETH_ALEN 6

    u8 mac[ETH_ALEN]             = { 0 };
    v8 mac_str[ETH_ALEN * 2 + 1] = { 0 };
    v8 key[32]                   = { 0 };
    u8 is_need_set_mac           = 0;
    s32 ret                      = 0;
    s32 try_count                = 0;
    struct ifreq req;
    memset(&req, 0, sizeof(req));

    s32 fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ipcerror("%s open socket failed!", __func__);
        return IPC_OPEN_ERROR;
    }

    strncpy(req.ifr_name, net_dev, sizeof(req.ifr_name) - 1);
    req.ifr_hwaddr.sa_family = ARPHRD_ETHER;

retry:
    try_count++;
    ret = ioctl(fd, SIOCGIFHWADDR, &req);
    if (ret != 0) {
        if (try_count < 3) {
            ipc_msleep(500);
            goto retry;
        }
        ipcerror("%s %s Ioctl SIOCGIFHWADDR failed!", __func__, net_dev);
        close(fd);
        return IPC_IOCTL_ERROR;
    }

    snprintf(key, sizeof(key), "%s_mac", net_dev);

    ipc_json_t json[] = { json_string(key, mac_str) };
    ipc_json_rdconf("ipc", json, ARRSIZE(json));

    if (strlen(mac_str) > 0) {
        ipcdebug("saved mac  %s", mac_str);
        s32 i = 0;
        for (i = 0; i < ETH_ALEN; i++) {
            mac[i] = _hex_to_bin(mac_str + i * 2);
        }

        if (memcmp(mac, req.ifr_hwaddr.sa_data, sizeof(mac)) != 0) {
            is_need_set_mac = 1;
        }
    } else {
        if (req.ifr_hwaddr.sa_data[0] == 0 || req.ifr_hwaddr.sa_data[1] == 0) {

            for (s32 i = 0; i < ARRSIZE(mac); i++) {
                mac[i] = ipc_rand() + 1;
            }

            mac[0] &= ~(0x1 << 0);
            mac[0] |= (0x1 << 1);
            is_need_set_mac = 1;
        } else {
            memcpy(mac, req.ifr_hwaddr.sa_data, sizeof(mac));
        }

        // coverity[SECURE_CODING :SUPPRESS]
        sprintf(mac_str, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        ipcdebug("saving mac  %s", mac_str);

        ipc_json_wrconf("ipc", json, ARRSIZE(json));
    }

    if (is_need_set_mac) {
        _ifconfig(net_dev, 0);

        memcpy(req.ifr_hwaddr.sa_data, mac, sizeof(mac));

        ipcdebug("set lock mac to %s", net_dev);
        iphdebug(mac, sizeof(mac));

        ret = ioctl(fd, SIOCSIFHWADDR, &req);
        if (ret != 0) {
            ipcerror("%s %s Ioctl SIOCSIFHWADDR failed!", __func__, net_dev);
            close(fd);
            return IPC_IOCTL_ERROR;
        }
    }

    close(fd);

    return IPC_SUCCESS;
}

static s64 _get_network_stats(pv8 dev)
{
    v8 path[256];
    FILE* fp;
    s64 rx_bytes = -1;

    if (!dev) {
        return -1;
    }

    // Try to get RX bytes through sysfs interface
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", dev);
    fp = fopen(path, "r");

    if (fp) {
        if (fscanf(fp, "%lld", &rx_bytes) == 1) {
            fclose(fp);
            return rx_bytes;
        }
        fclose(fp);
    }

    ipcdebug("Failed to read network stats from sysfs: %s", path);
    return -1;
}

static void _wireless_module_reboot()
{
    ipc_mpp_net_wireless_io_ctrl(0);
    ipc_msleep(500);
    ipc_mpp_net_wireless_io_ctrl(1);
    ipc_sleep(1);
}

static void _wifi_quirk_connect(void)
{
}

static void _check_insmod_cfg_mac_80211(s32 num)
{
    ipcwarn("wifi num: %d\n", num);
    pv8 insmod_ko[2] = { 0 };
    if (MODULE_WIFI_ATBM6062 == num) { // atbm6062 supports wifi6, need to use cfg80211 driver that supports wifi6
        insmod_ko[0] = WIFI_DRIVERS_PATH "/cfg80211_wifi6.ko";
    } else { // other wifi use ordinary cfg80211 and mac80211
        insmod_ko[0] = WIFI_DRIVERS_PATH "/cfg80211.ko";
        insmod_ko[1] = WIFI_DRIVERS_PATH "/mac80211.ko";
    }

    for (s32 idx = 0; idx < ARRSIZE(insmod_ko); idx++) {
        if (NULL == insmod_ko[idx])
            continue;

        if (0
            == access(insmod_ko[idx],
                      F_OK)) // compatible with other chip platforms that do not integrate with atbm6062, load driver only if it exists
            ipc_exec("insmod %s", insmod_ko[idx]);
    }
}

static s32 _check_usb_module_type(void)
{
    if (ipc_mpp_net_wireless_io_ctrl(1) != IPC_SUCCESS) {
        ipcwarn("io Not inited yet!");
        return IPC_FAILED;
    }

    FILE* fp = popen("lsusb", "r");
    if (fp == NULL) {
        ipcerror("lsusb failed!");
        return IPC_FAILED;
    }

    v8 buffer[256] = { 0 };
    s32 ret        = fread(buffer, sizeof(buffer), 1, fp);
    pclose(fp);

    if (ret < 0) {
        return IPC_FAILED;
    }

    for (s32 idx = 0; idx < ARRSIZE(_g_wifi_map); idx++) {
        if (strstr(buffer, _g_wifi_map[idx].usb_enum)) {
            _check_insmod_cfg_mac_80211(idx);
            return idx;
        }
    }

    ipcwarn("Not match usb module!");
    return IPC_NOT_FOUND;
}

static s32 _insmod_wifi(s32 num)
{
    _wireless_module_reboot();

    s32 ret = ipc_exec("insmod %s/%s.ko %s", WIFI_DRIVERS_PATH, _g_wifi_map[num].driver_name, _g_wifi_map[num].expand_args);
    if (ret != 0) {
        ipcwarn("insmod %s failed!", _g_wifi_map[num].driver_name);
        return IPC_FAILED;
    }
    ipcinfo("insmod %s.ko %s success!", _g_wifi_map[num].driver_name, _g_wifi_map[num].expand_args);

    // Lock MAC address before notifying external, so that external can customize MAC address modification
    _lock_net_device_mac(WLAN_DEV);

    if (strcmp(_g_wifi_map[num].driver_name, "8733bu") == 0) {

        ipc_exec("modprobe bluetooth");
        ipc_exec("insmod %s/rtk_btusb.ko", WIFI_DRIVERS_PATH);
    }

    ipc_wireless_module_after_insmod_driver_notify(WIRELESS_MODULE_WIFI, WLAN_DEV);

    _gh_net.f_event(IPC_NET_EVENT_INSMODE_WIFI_DRIVER);

    return IPC_SUCCESS;
}

static s32 _insmod_4G(s32 num)
{
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 ret;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    v8 vid[5] = { 0 };
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    v8 pid[5] = { 0 };

    if (0 == access("/dev/ttyUSB1", F_OK) || 0 == access("/dev/ttyUSB2", F_OK) || 0 == access("/dev/ttyACM0", F_OK)) {
        ipcwarn("%s aready insmod!", _g_wifi_map[num].driver_name);
        return IPC_NOT_NEED;
    }

    sscanf(_g_wifi_map[num].usb_enum, "%[^:] : %[^:]", vid, pid);

    ret = ipc_exec("echo %s %s > /sys/bus/usb-serial/drivers/option1/new_id", vid, pid);
    if (ret != 0) {
        ipcwarn("insmod %s failed!", _g_wifi_map[num].driver_name);
        return IPC_FAILED;
    }

    /* Slightly delay, after registration /dev/ttyUSBx will not appear immediately */
    ipc_msleep(500);

    if (access(_g_wifi_map[num].expand_args, F_OK)) {
        ipcwarn("%s not exist, insmod %s failed!", _g_wifi_map[num].expand_args, _g_wifi_map[num].driver_name);
        return IPC_FAILED;
    }

    ipcinfo("insmod %s success!", _g_wifi_map[num].driver_name);

    ipc_wireless_module_after_insmod_driver_notify(WIRELESS_MODULE_4G, RNDIS_DEV);

    _gh_net.f_event(IPC_NET_EVENT_INSMODE_WIFI_DRIVER);

    return IPC_SUCCESS;
}

static void _rmmod_wifi(s32 num)
{
    if (num < 0 || num >= WIFI_MODULE_MAX)
        return;
    ipc_exec("rmmod %s", _g_wifi_map[num].driver_name);
    ipcinfo("rmmod %s", _g_wifi_map[num].driver_name);
}

/******************************** network tools ********************************/

static s32 _ip_clear(pv8 dev)
{
    return ipc_exec("ifconfig %s 0.0.0.0", dev) ? IPC_FAILED : IPC_SUCCESS;
}

static s32 _ifconfig(pv8 dev, s8 sw)
{
    return ipc_exec("ifconfig %s %s", dev, sw ? "up" : "down") ? IPC_FAILED : IPC_SUCCESS;
}

static void _udhcpc_stop(pv8 dev)
{
    ipc_exec("kill -9 `cat /tmp/udhcpc_%s.pid` ; rm /tmp/udhcpc_%s.pid", dev, dev);
}

/* coverity[TAINTED_STRING:SUPPRESS] */
/*
 * Coverity suppression justification:
 * This function reads network configuration from a controlled file (/conf/static_ip.conf)
 * The configuration file contains only trusted network parameters set by system administrators
 * Environment variables are set only for validated interface names and their corresponding values
 * The data source is trusted and controlled, making the setenv usage safe in this context
 */
static s32 _read_static_ip_address(pv8 dev)
{
    FILE* fp;
    v8 str[60];

    if (strstr(dev, WLAN_DEV) == NULL && strstr(dev, WIRED_DEV) == NULL) {
        return IPC_NOT_SUPPORT;
    }

    fp = fopen(STATIC_IP_CONF, "r");
    if (fp == NULL) {
        return IPC_FAILED;
    }
    setenv("interface", dev, 1);
    while (fgets(str, 60, fp)) {
        ipcinfo("%s\n", str);
        char* st = strchr(str, '\n');
        if (st) {
            *st = '\0';
        }

        st = strchr(str, '=');
        if (st) {
            *st = '\0';
            // coverity[SECURE_CODING:SUPPRESS] - Using setenv with validated configuration data
            // This is safe because str is read from a controlled configuration file
            // and contains only valid network configuration parameters
            // coverity[TAINTED_STRING:SUPPRESS] - Configuration file data is trusted and controlled
            setenv(str, st + 1, 1);
        }
    }

    fclose(fp);

    return IPC_SUCCESS;
}

static s32 _try_use_static_ip(pv8 dev)
{
    s32 ret = 0;

    ret = _read_static_ip_address(dev);
    if (ret != IPC_SUCCESS) {
        return ret;
    }

    ret = ipc_exec(UDHCPC_SCRIPT " deconfig");
    if (ret != IPC_SUCCESS) {
        return ret;
    }

    ret = ipc_exec(UDHCPC_SCRIPT " renew");
    if (ret != IPC_SUCCESS) {
        return ret;
    }
    return IPC_SUCCESS;
}

static s32 _udhcpc_start(pv8 dev, s32 timeout_ts)
{
    pv8 extern_args = "";
    if (access(UDHCPC_SCRIPT, F_OK | X_OK) == 0) {
        extern_args = "-s " UDHCPC_SCRIPT;
    }

    // Avoid udhcpc process leak
    _udhcpc_stop(dev);

    if (_try_use_static_ip(dev) == IPC_SUCCESS) {
        return IPC_SUCCESS;
    }

    if (timeout_ts <= 0) {
        return ipc_exec("udhcpc -x hostname:%s -i %s -p /tmp/udhcpc_%s.pid %s -b", _gh_net.hostname, dev, dev, extern_args) ? IPC_FAILED : IPC_SUCCESS;
    } else {
        return ipc_exec("udhcpc -T %d -t %d -n -x hostname:%s -i %s -p /tmp/udhcpc_%s.pid  %s ", 3, (timeout_ts + 2) / 3, _gh_net.hostname, dev, dev,
                       extern_args)
                   ? IPC_TIMEOUT
                   : IPC_SUCCESS;
    }
}

static s32 _udhcpc_renew_lease(pv8 dev)
{
    // dhcp lease renewal
    ipcdebug("%s: %s", __func__, dev);
    return ipc_exec("kill -%d `cat /tmp/udhcpc_%s.pid`", SIGUSR1, dev);
}

static s32 _udhcpd_start(ipc_wifi_ap_ex_p ex)
{
    ipc_wifi_ap_ex_t ex_shadow = { NULL, NULL };
    ex                        = ex ?: &ex_shadow;

    v8 ip[16];
    snprintf(ip, sizeof(ip), "%s", ex->ip ?: "192.168.1.1");
    ipc_exec("ifconfig %s %s", WLAN_DEV, ip);

    pv8 last = strrchr(ip, '.');
    if (last == NULL)
        return IPC_INVALID_ARGS;

    *last = '\0'; // 192.168.1.1 -> 192.168.1 1
    last++;
    pv8 segment = ip; // now ip only has the network segment

    s32 this = atoi(last);
    if (this <= 0)
        return IPC_INVALID_ARGS;

    s32 start = this < 128 ? this + 1 : 1;
    s32 end   = this < 128 ? 254 : this - 1;

    pv8 conf_format
        = "start %s.%d\n"
          "end   %s.%d\n"
          "opt router %s.%d\n"
          "opt dns %s.%d\n"
          "opt subnet %s\n\n"
          "interface " WLAN_DEV "\n";

    v8 conf[512];
    // coverity[FORMAT_STRING_INJECTION :SUPPRESS]
    snprintf(conf, sizeof(conf), conf_format, segment, start, segment, end, segment, this, segment, this,
             // coverity[PW.NON_CONST_PRINTF_FORMAT_STRING :SUPPRESS]
             ex->mask ?: "255.255.255.0");

#define UDHCPD_CONF "/tmp/udhcpd.conf"
    s32 ret = ipc_file_write_once(UDHCPD_CONF, conf, strlen(conf), __IPC_LOG__);
    if (ret < 0) {
        ipcerror("Write udhcpd configuration file failed! retcode=[%d]", ret);
        return IPC_WRITE_ERROR;
    }

    ret = ipc_exec("touch /var/lib/misc/udhcpd.leases && udhcpd -f %s &", UDHCPD_CONF);
    if (ret != 0) {
        ipcerror("Run udhcpd failed!");
        return IPC_FAILED;
    }

    ipcinfo("udhcpd start!");

    return IPC_SUCCESS;
}

static void _udhcpd_stop(void)
{
    ipc_exec("killall -9 udhcpd");
    ipcinfo("udhcpd stop!");
}

/********************************** station *************************************/

#define WPA_SUPPLICANT_PID_FILE "/tmp/wpa_supplicant.pid"

static s32 _wpa_server_start(void)
{
    pv8 wpa_ctx = "ctrl_interface=" WPA_INTERFACE;
    s32 ret     = ipc_file_write_once(WPA_CONF_FILE, wpa_ctx, strlen(wpa_ctx), __IPC_LOG__);
    if (ret <= 0) {
        ipcerror("Write wpa configuration file failed! retcode=[%d]", ret);
        return IPC_WRITE_ERROR;
    }

    ret = ipc_exec("wpa_supplicant -Dnl80211 -i %s -c %s -B -P %s", WLAN_DEV, WPA_CONF_FILE, WPA_SUPPLICANT_PID_FILE);
    if (ret != 0) {
        ipcerror("Run wpa_supplicant failed!");

        _rmmod_wifi(_gh_net.wifi_module);
        _insmod_wifi(_gh_net.wifi_module);
        return ret;
    }

    ipc_msleep(500);
    ipcinfo("wpa start success");

    return IPC_SUCCESS;
}

static void _wpa_server_stop(void)
{
    s32 cnt = 0;
    for (; !ipc_exec("killall -2 wpa_supplicant") && cnt < 20; cnt++)
        ipc_msleep(200);
    if (cnt >= 20)
        ipc_exec("killall -9 wpa_supplicant");

    ipc_exec("rm -f %s", WPA_SUPPLICANT_PID_FILE);

    ipc_exec("rm -f %s", WPA_CONF_FILE);
}

static s32 _wpa_cmd(vptr h_sock, pv8 format, ...)
{
    v8 cmd[128];
    va_list va_argp;
    va_start(va_argp, format);
    vsnprintf(cmd, sizeof(cmd), format, va_argp);
    va_end(va_argp);

    s32 ret = ipc_unix_socket_send(h_sock, cmd, strlen(cmd));
    if (ret < 0) {
        ipcerror("Unix socket send failed! retcode=[%d]", ret);
        return ret;
    }

    ipcdebug("Send: %s", cmd);

    v8 buf[1024] = { 0 };
    ret          = ipc_unix_socket_recv(h_sock, buf, sizeof(buf), 2 * 1000);
    if (ret < 0) {
        ipcerror("Unix socket recv failed! retcode=[%d]", ret);
        return ret;
    }

    ipcdebug("Recv: %s", buf);

    if (strncmp(buf, "OK\n", sizeof(buf)) && strncmp(buf, "0\n", sizeof(buf)))
        return IPC_VERIFY_FAILED;

    return IPC_SUCCESS;
}

/* Configure and connect to Wi-Fi */
static s32 _wpa_set(vptr h_sock, pv8 ssid, pv8 pwd, pv8 country_code)
{
    // Detach from the current network to avoid conflicts during configuration
    _wpa_cmd(h_sock, "DETACH");

    v8 ssid_hex[strlen(ssid) * 2 + 1];

    // Clear ssid_hex memory
    memset(ssid_hex, 0, sizeof(ssid_hex));

    // Convert ssid to hexadecimal format
    ipc_bin_to_hex((pu8)ssid, strlen(ssid), ssid_hex, sizeof(ssid_hex));

#define STA_WPA3_CONF_FORMAT                                                                                                                         \
    "network={\n"                                                                                                                                    \
    " ssid=%s\n"                                                                                                                                     \
    " scan_ssid=1\n"                                                                                                                                 \
    " key_mgmt=SAE\n"                                                                                                                                \
    " pairwise=CCMP TKIP\n"                                                                                                                          \
    " group=CCMP TKIP\n"                                                                                                                             \
    " proto=RSN\n"                                                                                                                                   \
    " sae_password=%s\n"                                                                                                                             \
    " priority=3\n"                                                                                                                                  \
    " ieee80211w=2\n"                                                                                                                                \
    "}\n\n"

#define STA_WPA_CONF_FORMAT                                                                                                                          \
    "network={\n"                                                                                                                                    \
    " ssid=%s\n"                                                                                                                                     \
    " scan_ssid=1\n"                                                                                                                                 \
    " proto=WPA2\n"                                                                                                                                  \
    " key_mgmt=WPA-PSK\n"                                                                                                                            \
    " pairwise=CCMP TKIP\n"                                                                                                                          \
    " group=CCMP TKIP\n"                                                                                                                             \
    " psk=%s\n"                                                                                                                                      \
    " priority=1\n"                                                                                                                                  \
    "}\n\n"

#define STA_NO_PASSWORD_CONF_FORMAT                                                                                                                  \
    "network={\n"                                                                                                                                    \
    " ssid=%s\n"                                                                                                                                     \
    " scan_ssid=1\n"                                                                                                                                 \
    " key_mgmt=NONE\n"                                                                                                                               \
    "}\n\n"

    FILE* fp;
    // Open the configuration file for writing
    // coverity[NULL_RETURNS :SUPPRESS]
    fp = fopen(WPA_CONF_FILE, "w+");

    // coverity[FORWARD_NULL :SUPPRESS]
    // coverity[NULL_RETURNS :SUPPRESS]
    fprintf(fp, "%s", WPA_CONF_FILE_HEAD);

    fprintf(fp, "country=%s\n", "IND");

    if (pwd && pwd[0]) {

        if (strlen(pwd) >= 8) {
            v8 psk[128] = { 0 };
            v8 pwd_hex[strlen(pwd) * 2 + 1];
            memset(pwd_hex, 0, sizeof(pwd_hex));
            ipc_bin_to_hex((pu8)pwd, strlen(pwd), pwd_hex, sizeof(pwd_hex));
            ipc_wpa_get_password_psk(ssid, pwd, psk, sizeof(psk));
            // coverity[SENSITIVE_DATA_LEAK :SUPPRESS]
            fprintf(fp, STA_WPA3_CONF_FORMAT, ssid_hex, pwd_hex);
            // coverity[SENSITIVE_DATA_LEAK :SUPPRESS]
            fprintf(fp, STA_WPA_CONF_FORMAT, ssid_hex, psk);
        }
    } else {
        fprintf(fp, STA_NO_PASSWORD_CONF_FORMAT, ssid_hex);
    }

    fclose(fp);

    s32 ret = 0;
    ret     = _wpa_cmd(h_sock, "RECONFIGURE");
    if (ret != IPC_SUCCESS) {
        return ret;
    }

    return IPC_SUCCESS;
}

static s32 __check_is_wpa1(vptr h_sock)
{
    v8 recv[1024];
    s32 ret         = 0;
    s32 retry_count = 0;
    s32 max_retries = 5;

    ipc_sleep(5);

    for (retry_count = 0; retry_count < max_retries; retry_count++) {
        if (retry_count > 0) {
            ipcdebug("WPA security check retry attempt %d/%d", retry_count + 1, max_retries);
            ipc_sleep(1);
        }

        if (ipc_unix_socket_send(h_sock, "STATUS-VERBOSE", strlen("STATUS-VERBOSE")) < 0) {
            if (retry_count == max_retries - 1) {
                ipcerror("Failed to send STATUS-VERBOSE command after %d attempts", max_retries);
                return IPC_FAILED;
            }
            continue;
        }

        ret = ipc_unix_socket_recv(h_sock, recv, sizeof(recv) - 1, 1000);
        if (ret <= 0 && ret != IPC_TIMEOUT) {
            ipcerror("Recv error! retcode=[%d]", ret);
            if (retry_count == max_retries - 1) {
                return ret;
            }
            continue;
        }

        if (ret > 0) {
            recv[ret] = '\0';
            ipcdebug("%s\n", recv);

            if (strstr(recv, "key_mgmt=WPA2-PSK") || strstr(recv, "key_mgmt=SAE")) {
                return IPC_SUCCESS;
            }
        }
    }

    printf("is not wpa2 or wpa3 after %d retry attempts", max_retries);
    return IPC_FAILED;
}

// Declare a static function _wpa_wait_connected, which accepts a parameter of type vptr and a parameter of type u32
static s32 _wpa_wait_connected(vptr h_sock, u32 timeout)
{
    // Get the current time, which is the starting time
    u32 start = ipc_mono_ts();
    // Initialize the end time to the starting time
    u32 end = start;
    // Initialize the variable len to record the number of bytes received
    s32 len = 0;
    // Define an array of type v8 to store received data
    v8 recv[1024];

    s32 ret = _wpa_cmd(h_sock, "ATTACH");
    if (ret != IPC_SUCCESS)
        return ret;

    // Enter into an endless loop to continuously receive data
    while (_gh_net.sta_gorun) {
        // Call the ipc_unix_socket_recv function to receive data, setting a timeout of 1000 milliseconds
        ret = len = ipc_unix_socket_recv(h_sock, recv, sizeof(recv) - 1, 1000);
        // If the return value is less than or equal to 0, and it is not due to timeout, it means an error occurred during data reception
        if (ret <= 0 && ret != IPC_TIMEOUT) {
            // Output an error message indicating the failure of data reception and the specific error code
            ipcerror("Recv error! retcode=[%d]", ret);
            // return the error code
            return ret;
        }

        // If data is received, set the last byte of the received data to the null terminator
        if (len > 0) {
            recv[len] = '\0';
            // Print out the received data
            ipcdebug("%s", recv);
            // If the received data contains "CTRL-EVENT-CONNECTED", it means the connection is successful, and IPC_SUCCESS is returned
            if (strstr(recv, "CTRL-EVENT-CONNECTED")) {
                return __check_is_wpa1(h_sock);
            }
            // If the received data contains "4-Way Handshake failed" or it contains "CTRL-EVENT-" and "auth_failures", as well as "reason=WRONG_KEY",
            // it means the connection failed or the wrong key was used, and IPC_VERIFY_FAILED is returned
            if (strstr(recv, "4-Way Handshake failed")
                || (strstr(recv, "CTRL-EVENT-") && strstr(recv, "auth_failures") && strstr(recv, "reason=WRONG_KEY"))) {
                ipcwarn("Connect password error");
                return IPC_VERIFY_FAILED;
            }
        }

        // Get the current time, which is the end time
        end = ipc_mono_ts();
        // If the time difference between the end time and the start time exceeds the set timeout duration, it means a timeout has occurred
        if (end - start > timeout) {
            // Output a timeout prompt message
            ipcwarn("Connect timeout");
            // Return a timeout error code
            return IPC_TIMEOUT;
        }
    }

    // If the connection is not established within the timeout duration, IPC_TIMEOUT is returned
    return IPC_TIMEOUT;
}

static vptr _pth_sta_listen(vptr h_sock)
{
    s32 fail_cnt = 0;
    s32 ret      = 0;
    u8 reinsmod  = 0;
    v8 recv[1024];
    s32 unix_timeout_count = 0;
    s32 renew_lease_count  = 1;
    v8 route_ip[64]        = { 0 };
    s32 ping_fd            = -1;
    s32 is_connected       = 1;

    _gh_net.sta_alive = 1;

    ret = ipc_get_route(route_ip, WLAN_DEV);
    if (ret < 0) {
        goto exit;
    }

    ipcinfo("route_ip:%s\n", route_ip);

    ping_fd = ipc_ping4_init(WLAN_DEV);
    ipcdebug("ping_fd:%d\n", ping_fd);
    if (ping_fd < 0) {
        goto exit;
    }

    while (_gh_net.sta_gorun) {

        renew_lease_count++;
        if ((renew_lease_count % (30 * 60 * 60 / 2)) == 0) {
            _udhcpc_renew_lease(WLAN_DEV);
        }

        ipc_sleep(2);

        if (ipc_unix_socket_send(h_sock, "STATUS-VERBOSE", strlen("STATUS-VERBOSE")) < 0) {
            _gh_net.sta_need_to_reconn = 1;
            continue;
        }

        if (_gh_net.sta_gorun == 0 || unix_timeout_count > 15) {
            break;
        }

        if ((renew_lease_count % 15) == 0) {
            ipc_ping4(ping_fd, route_ip, 500);
        }

        do {
            ret = ipc_unix_socket_recv(h_sock, recv, sizeof(recv) - 1, 1 * 1000);
            if (ret <= 0) {
                ipcdebug("Error receiving WPA status! Return code=[%d]", ret);
                // Detected timeout, check if wpa_supplicant is still running
                unix_timeout_count++;
                break;
            }

            /* If unable to connect to the cloud platform, reconnect WiFi immediately, using unix_timeout_count as a flag to break out */
            if (_gh_net.next_reboot_wireless_mono_s && ipc_mono_ts() > _gh_net.next_reboot_wireless_mono_s) {
                unix_timeout_count                  = 16;
                _gh_net.next_reboot_wireless_mono_s = 0;
                ipcerror("Cloud disconnected for a very long time, there seems to be an issue with the WiFi module");
                break;
            }

            unix_timeout_count = 0;

            recv[ret] = '\0';
            ipcdebug("%s", recv);

            char* wpa_state = strstr(recv, "wpa_state=");
            if (wpa_state) {
                if (!strstr(wpa_state, "wpa_state=COMPLETED")) {
                    renew_lease_count = 1;
                    fail_cnt++;
                    // In case of timeout or unavailability, reload the driver directly
                    if (fail_cnt >= 30 || strstr(wpa_state, "wpa_state=INACTIVE")) {
                        if (reinsmod) { // Previous load has already failed
                            _gh_net.sta_need_to_reconn = 1;
                            // coverity[UNUSED_VALUE :SUPPRESS]
                            reinsmod = 0;
                            goto exit;
                        }
                        fail_cnt = 0;
                        _gh_net.f_event(IPC_NET_EVENT_STA_DISCONNECT);
                        ipcinfo("Reload WiFi driver!");
                        _ifconfig(WLAN_DEV, 0);
                        _rmmod_wifi(_gh_net.wifi_module);
                        _insmod_wifi(_gh_net.wifi_module);
                        reinsmod = 1;
                    }
                } else {
                    fail_cnt = 0;
                    if (reinsmod) {
                        reinsmod = 0;
                        _gh_net.f_event(IPC_NET_EVENT_STA_RECONNECT);
                    }
                }
            }

            if (strstr(recv, "CTRL-EVENT-CONNECTED") && (!is_connected)) {
                ret = __check_is_wpa1(h_sock);
                if (ret != IPC_SUCCESS) {
                    _gh_net.sta_need_to_reconn = 1;
                    break;
                }

                ret = _udhcpc_start(WLAN_DEV, 60);
                if (ret < 0) {
                    ipcwarn("Routing connected, but udhcpc start failed!");
                    _gh_net.sta_need_to_reconn = 1;
                } else {
                    ipcdebug("Routing connected");
                    is_connected = 1;
                    _gh_net.f_event(IPC_NET_EVENT_STA_RECONNECT);
                }
            } else if (strstr(recv, "CTRL-EVENT-DISCONNECTED")) { /* Router initiated disconnection, no ping */
                is_connected = 0;
                _udhcpc_stop(WLAN_DEV);
                _ip_clear(WLAN_DEV);

                ipcdebug("Routing disconnected");
                _gh_net.f_event(IPC_NET_EVENT_STA_DISCONNECT);
            }
        } while (_gh_net.sta_gorun);
    }

exit:
    _wpa_cmd(h_sock, "DETACH");
    ipc_unix_socket_close(h_sock);
    _gh_net.sta_alive = 0;

    if (ping_fd >= 0) {
        ipc_ping4_uninit(ping_fd);
    }

    return NULL;
}

static s32 _wifi_sta_connect(pv8 ssid, pv8 pwd, pv8 country_code, u32 timeout)
{
    ipc_wifi_sta_disconnect();
    ipc_wifi_ap_destroy();

    ipc_exec("iwconfig %s mode managed", WLAN_DEV);

    ipcdebug("ssid=[%s], pwd=[%s]", ssid, pwd ?: "NONE");

    _wifi_quirk_connect();

    _ifconfig(WLAN_DEV, 1);

    _gh_net.f_event(IPC_NET_EVENT_STA_BEFORE_CONNECTED);

    s32 ret = _wpa_server_start();
    if (ret < 0)
        return ret;

    _gh_net.sta_gorun = 1;
    _gh_net.sta_alive = 1;

    vptr h_sock = ipc_unix_socket_open(WPA_INTERFACE_FILE("ctrl"), WPA_INTERFACE_FILE(WLAN_DEV));
    if (h_sock == NULL) {
        ret = IPC_OPEN_ERROR;
        ipcerror("Open unix socket failed!");
        goto FAILED;
    }

    ret = _wpa_set(h_sock, ssid, pwd, country_code);
    if (ret < 0)
        goto FAILED;

    ret = _wpa_wait_connected(h_sock, timeout);
    if (ret < 0) {
        goto FAILED;
    }

    ret = _udhcpc_start(WLAN_DEV, timeout);
    if (ret < 0)
        goto FAILED;

    ipcinfo("Connect [%s:%s] success!", ssid, pwd ?: "NONE");
    ret = ipc_create_thread("ipc_station", _pth_sta_listen, h_sock, 64 * 1024, 0);
    if (ret >= 0)
        return IPC_SUCCESS;

FAILED:
    if (h_sock)
        ipc_unix_socket_close(h_sock);
    _udhcpc_stop(WLAN_DEV);
    _wpa_server_stop();
    _ip_clear(WLAN_DEV);
    _ifconfig(WLAN_DEV, 0);
    _gh_net.sta_gorun = 0;
    _gh_net.sta_alive = 0;
    ipcwarn("Connect [%s:%s] failed!", ssid, pwd ?: "NONE");
    return ret;
}

s32 ipc_wifi_sta_connect(pv8 ssid, pv8 pwd, pv8 country_code, u32 timeout)
{
    s32 ret = IPC_SUCCESS;

    if (!ssid || !ssid[0]) {
        snprintf(_gh_net.ssid, sizeof(_gh_net.ssid), "%s", ssid ?: "");
        return ret;
    }

    if (timeout) {
        if (!_gh_net.has_wifi)
            return IPC_NOT_SUPPORT;
        _gh_net.sta_lock = 1; // Lock
        ret              = _wifi_sta_connect(ssid, pwd, country_code, timeout);
        _gh_net.sta_lock = 0;
        if (ret < 0) {
            _gh_net.f_event(ret == IPC_VERIFY_FAILED ? IPC_NET_EVENT_STA_PASSWORD_ERROR : IPC_NET_EVENT_STA_CONNECT_FAILED);
            return ret;
        }
        _gh_net.f_event(IPC_NET_EVENT_STA_CONNECT_SUCCESS);
    }

    // Save network configuration after successful connection for convenient reconnection
    snprintf(_gh_net.country_code, sizeof(_gh_net.country_code), "%s", country_code ?: "");
    snprintf(_gh_net.pwd, sizeof(_gh_net.pwd), "%s", pwd ?: "");
    snprintf(_gh_net.ssid, sizeof(_gh_net.ssid), "%s", ssid);

    return ret;
}

void ipc_wifi_sta_disconnect(void)
{
    _gh_net.sta_gorun = 0;
    _udhcpc_stop(WLAN_DEV); // Kill first to interrupt ongoing connection
    while (_gh_net.sta_alive)
        ipc_msleep(100);
    _wpa_server_stop();
    _ip_clear(WLAN_DEV);
    _ifconfig(WLAN_DEV, 0);
    ipcinfo("Disconnect station");
}

void ipc_wifi_sta_set_hostname(pv8 hostname)
{
    if (!hostname || !hostname[0])
        return;
    snprintf(_gh_net.hostname, sizeof(_gh_net.hostname), "%s", hostname);
}

static s32 _chan_to_frequency(s32 chan)
{
    if (chan == 0 || chan > 13) {
        chan = 6;
    }

    s32 frequencys[] = { 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472 };

    return frequencys[chan - 1];
}

static s32 _ap_setup(pv8 ssid, pv8 pwd, u8 chn)
{
    s32 ret     = 0;
    vptr h_sock = ipc_unix_socket_open(WPA_INTERFACE_FILE("ctrl"), WPA_INTERFACE_FILE(WLAN_DEV));
    if (h_sock == NULL) {
        ret = IPC_OPEN_ERROR;
        ipcerror("Open unix socket failed!");
        goto FAILED;
    }

    _wpa_cmd(h_sock, "DETACH"); /* separation mode, to prevent recv conflict during configuration */

#define AP_WPA_CONF_FORMAT                                                                                                                           \
    "network={\n"                                                                                                                                    \
    "    ssid=\"%s\"\n"                                                                                                                              \
    "    scan_ssid=1\n"                                                                                                                              \
    "    key_mgmt=WPA-PSK\n"                                                                                                                         \
    "    proto=WPA2\n"                                                                                                                               \
    "    pairwise=CCMP TKIP\n"                                                                                                                       \
    "    group=CCMP TKIP\n"                                                                                                                          \
    "    psk=\"%s\"\n"                                                                                                                               \
    "    mode=2\n"                                                                                                                                   \
    "    frequency=%d\n"                                                                                                                             \
    "}\n\n"

#define AP_NO_PASSWORD_CONF_FORMAT                                                                                                                   \
    "network={\n"                                                                                                                                    \
    "    ssid=\"%s\"\n"                                                                                                                              \
    "    scan_ssid=1\n"                                                                                                                              \
    "    key_mgmt=NONE\n"                                                                                                                            \
    "    mode=2\n"                                                                                                                                   \
    "    frequency=%d\n"                                                                                                                             \
    "}\n\n"

    // coverity[RETURNED_NULL :SUPPRESS]
    // coverity[VAR_ASSIGNED :SUPPRESS]
    FILE* fp = fopen(WPA_CONF_FILE, "w+");
    if (fp) {
        // coverity[DEREFERENCE :SUPPRESS]
        fprintf(fp, "%s", WPA_CONF_FILE_HEAD);
        if (pwd && pwd[0]) {
            // coverity[SENSITIVE_DATA_LEAK :SUPPRESS]
            fprintf(fp, AP_WPA_CONF_FORMAT, ssid, pwd, _chan_to_frequency(chn));
        } else {
            fprintf(fp, AP_NO_PASSWORD_CONF_FORMAT, ssid, _chan_to_frequency(chn));
        }
        fclose(fp);
    }
    ret = _wpa_cmd(h_sock, "RECONFIGURE");

FAILED:
    if (h_sock)
        ipc_unix_socket_close(h_sock);

    return ret;
}

s32 ipc_wifi_ap_create(pv8 ssid, pv8 pwd, u8 chn, ipc_wifi_ap_ex_p ex)
{
    if (!_gh_net.has_wifi)
        return IPC_NOT_SUPPORT;

    if (!ssid || !ssid[0]) {
        ipcerror("Parameter error! ssid=[%s]", ssid);
        return IPC_INVALID_ARGS;
    }

    ipc_wifi_sta_disconnect();
    ipc_wifi_ap_destroy();

    _wpa_server_start();

    ipcdebug("ssid=[%s], pwd=[%s]", ssid, pwd ?: "NONE");

    _ifconfig(WLAN_DEV, 1);

    // coverity[SENSITIVE_DATA_LEAK :SUPPRESS]
    s32 ret = _ap_setup(ssid, pwd, chn);
    if (ret < 0) {
        ipcerror("ap setup failed!");
        goto FAILED;
    }

    ret = _udhcpd_start(ex);
    if (ret < 0) {
        ipcerror("udhcpcd start failed!");
        goto FAILED;
    }

    ipcinfo("AP [%s:%s] build success!", ssid, pwd ?: "NONE");

    return IPC_SUCCESS;

FAILED:
    _udhcpd_stop();
    _ip_clear(WLAN_DEV);
    _ifconfig(WLAN_DEV, 0);
    return ret;
}

void ipc_wifi_ap_destroy(void)
{
    _udhcpd_stop();
    _ip_clear(WLAN_DEV);
    _ifconfig(WLAN_DEV, 0);
    ipcinfo("AP destroy success!");
}

/********************************* eth0 & main ***************************/

static u8 _wired_is_conn(s32 fd)
{
    // First check if network card has been marked as failed
    if (_gh_net.wired_card_failure) {
        ipcdebug("Network card marked as failed, returning disconnected");
        return 0;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), WIRED_DEV);

    s32 ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (ret < 0) {
        ipcdebug("Socket ioctl failed! Errmsg=[%s]", strerror(errno));
        return 0;
    }

    if (!(ifr.ifr_flags & IFF_UP)) {
        ifr.ifr_flags |= IFF_UP;
        ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
        if (ret < 0) {
            ipcdebug("Socket ioctl failed! Errmsg=[%s]", strerror(errno));
            return 0;
        }

        memset(&ifr, 0, sizeof(ifr));
        ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
        if (ret < 0) {
            ipcdebug("Socket ioctl failed! Errmsg=[%s]", strerror(errno));
            return 0;
        }
    }

    return !!(ifr.ifr_flags & IFF_RUNNING);
}

static vptr _pth_wired_conn(vptr args)
{
    _gh_net.wired_conn_alive = 1;

    s32 ret = 0;
    while (_gh_net.wired_conn_gorun) {
        ret = _udhcpc_start(WIRED_DEV, 30);
        if (ret >= 0) {
            ipcwarn("Wired connect!");
            // Reset failure state on successful connection
            _gh_net.wired_card_failure    = 0;
            _gh_net.wired_dhipc_fail_count = 0;
            _gh_net.f_event(IPC_NET_EVENT_WIRED_CONNECT);
            _gh_net.f_signal(WIRED_MODULE_ETH, 0);
            break;
        } else {
            // Increment DHCP failure counter
            _gh_net.wired_dhipc_fail_count++;
            ipcwarn("DHCP failure count: %d", _gh_net.wired_dhipc_fail_count);

            // After 2 consecutive failures, check network card health
            if (_gh_net.wired_dhipc_fail_count >= 2) {
                s64 rx_bytes = _get_network_stats(WIRED_DEV);
                ipcwarn("Network card RX bytes: %lld", rx_bytes);

                // If RX bytes is 0 or -1 (error), mark as hardware failure
                if (rx_bytes <= 0) {
                    ipcwarn("Network card hardware failure detected! RX bytes: %lld", rx_bytes);
                    // If network port IO is floating, it may cause the driver to misjudge network port presence, even causing continuous connection
                    // attempts to the port while offline Here we judge that if no data is received in two udhcpc attempts, the network port is
                    // considered problematic and disabled directly. In Ethernet, it's unlikely to not receive any broadcast packets for even a minute

                    _gh_net.wired_card_failure  = 1;
                    _gh_net.wired_last_rx_bytes = 0;
                    break; // Exit the thread
                } else {
                    // Store current RX bytes for next comparison
                    _gh_net.wired_last_rx_bytes = rx_bytes;
                    ipcwarn("Network card appears healthy, RX bytes: %lld", rx_bytes);
                }
            }
        }
    }

    _gh_net.wired_conn_alive = 0;

    return NULL;
}

static s32 _wired_connect(void)
{
    _gh_net.wired_conn_gorun = 1;
    return ipc_create_thread("ipc_wired_conn", _pth_wired_conn, NULL, 16 * 1024, 0);
}

static void _wired_disconnect(void)
{
    _gh_net.wired_conn_gorun = 0;
    _udhcpc_stop(WIRED_DEV);
    while (_gh_net.wired_conn_alive)
        ipc_msleep(300);
    _ip_clear(WIRED_DEV);
    _ifconfig(WIRED_DEV, 0);
    ipc_mpp_eth_rst_io_ctrl(); /* Sometimes ethernet cable may have abnormalities, need manual io reset control (only succeeds if io is defined,
                                 currently only 4G board enables this) */
    ipcwarn("Wired disconnect!");
    _gh_net.f_event(IPC_NET_EVENT_WIRED_DISCONNECT);
}

static void _wired_wait_to_be_ready(s32 wired_fd)
{
    s32 cnt = 0;
    while (cnt < 50) {
        cnt++;
        if (_wired_is_conn(wired_fd)) {
            break;
        }
        ipctrace("waiting wired be ready");
        ipc_msleep(100);
    }
}

static vptr _pth_sta_reconn(vptr args)
{
    _gh_net.sta_reconn_alive = 1;

    u32 start           = ipc_mono_ts();
    u32 next_rmmod_time = start + 5 * 60;

    s32 ret = 0;
    while (_gh_net.sta_reconn_gorun && !_gh_net.sta_lock && _gh_net.ssid[0]) {
        ret = _wifi_sta_connect(_gh_net.ssid, _gh_net.pwd, _gh_net.country_code, 60);
        if (ret >= 0) {
            ipcinfo("Auto connect [%s:%s] success!", _gh_net.ssid, _gh_net.pwd);
            _gh_net.f_event(IPC_NET_EVENT_STA_CONNECT_SUCCESS);
            break;
        }
        ipcinfo("Auto connect [%s:%s] failed! retcode=[%d]", _gh_net.ssid, _gh_net.pwd, ret);
        _gh_net.f_event(ret == IPC_VERIFY_FAILED ? IPC_NET_EVENT_STA_PASSWORD_ERROR : IPC_NET_EVENT_STA_CONNECT_FAILED);
        if (ipc_mono_ts() > next_rmmod_time) {
            next_rmmod_time = ipc_mono_ts() + 5 * 60;
            _ifconfig(WLAN_DEV, 0);
            _rmmod_wifi(_gh_net.wifi_module);
            _insmod_wifi(_gh_net.wifi_module);
        }
    };

    _gh_net.sta_reconn_alive = 0;

    return NULL;
}

static s32 _sta_reconn_init(void)
{
    if (!_gh_net.has_wifi || !_gh_net.ssid[0])
        return IPC_NOT_SUPPORT;
    if (_gh_net.sta_reconn_alive || _gh_net.sta_alive || _gh_net.sta_lock)
        return IPC_EXIST; // Prevent re-entry or when station is active, no need to reconnect or when being locked

    _gh_net.sta_reconn_gorun = 1;
    return ipc_create_thread("ipc_sta_reconn", _pth_sta_reconn, NULL, 64 * 1024, 0);
}

static void _sta_reconn_uninit(void)
{
    _gh_net.sta_reconn_gorun = 0;
    ipc_wifi_sta_disconnect();
    ipc_wifi_ap_destroy();
    while (_gh_net.sta_reconn_alive)
        ipc_msleep(100);
}

static s32 _dev_ttyUSBx_is_exist(void)
{
    if (access(_g_wifi_map[_gh_net.wifi_module].expand_args, F_OK)) {
        // ipcwarn("%s %s is not ready\n", __func__, _g_wifi_map[_gh_net.wifi_module].expand_args);
        return IPC_FAILED;
    } else {
        return IPC_SUCCESS;
    }
}

static s32 _write_at_cmd_and_recv(s32 uart_fd, pv8 cmd_buf, pv8 recv_buf, s32 recv_buf_len)
{
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 ret = IPC_FAILED;

    ipcdebug("\n\n-------%s-------\n\n", cmd_buf);

    ret = ipc_uart_write(uart_fd, cmd_buf, strlen(cmd_buf));
    if (ret < 0) {
        return IPC_FAILED;
    }

    memset(recv_buf, 0, recv_buf_len);

    ret = ipc_uart_read(uart_fd, recv_buf, recv_buf_len, 500);
    if (ret < 0) {
        return IPC_FAILED;
    }

    ipcdebug("\n\n======%s======\n\n", recv_buf);

    return IPC_SUCCESS;
}

static s32 _write_at_cmd_and_recv_ok(s32 uart_fd, pv8 cmd_buf, pv8 recv_buf, s32 recv_buf_len)
{
    s32 ret = IPC_SUCCESS;
    if (_write_at_cmd_and_recv(uart_fd, cmd_buf, recv_buf, recv_buf_len) != IPC_SUCCESS) {
        ret = IPC_FAILED;
        goto exit;
    }

    if (!strstr(recv_buf, "OK")) {
        ret = IPC_FAILED;
        goto exit;
    }

exit:
    ipc_msleep(30);

    return ret;
}

struct ipc_apn_info_s {
    pv8 mncmcc;
    u8 authtype;
    u8 protocol;
    pv8 apn;
    pv8 user;
    pv8 password;
};

static s32 _4g_read_apns(pv8 sim_mncmcc, s32 uart_fd, s32 (*apn_cb)(s32 uart_fd, struct ipc_apn_info_s* info, void* _user), void* _user)
{
    FILE* fp = fopen("/app/bin/ipc_apns.bin", "rb");
    if (fp == NULL) {
        return IPC_FAILED;
    }

    s32 ret = IPC_SUCCESS;

    while (1) {
        u8 size = 0;
        // coverity[CHECKED_RETURN:SUPPRESS] - Check fread return value for size read
        if (fread(&size, sizeof(size), 1, fp) != 1) {
            ret = IPC_FAILED;
            break;
        }
        if (size == 0) {
            break;
        }
        // coverity[STRING_NULL:SUPPRESS] - Buffer is size+1 and null-terminated
        u8 buffer[size + 1];
        // coverity[TAINTED_SCALAR:SUPPRESS] - Validate buffer size before use
        if (size > 255) { // Reasonable limit for APN data
            ret = IPC_FAILED;
            break;
        }
        // coverity[CHECKED_RETURN:SUPPRESS] - Check fread return value for data read
        if (fread(buffer, size, 1, fp) <= 0) {
            ret = IPC_FAILED;
            break;
        }

        struct ipc_apn_info_s info = { 0 };
        s32 mncmcc_len            = 0;
        info.mncmcc               = (pv8)buffer;
        // coverity[STRING_NULL:SUPPRESS] - Ensure buffer is null-terminated
        buffer[size] = '\0';
        mncmcc_len   = strlen(info.mncmcc);

        if (strncmp(sim_mncmcc, info.mncmcc, mncmcc_len)) {
            continue;
        }

        pv8 start = info.mncmcc + mncmcc_len + 1;
        memcpy(&info.authtype, start, 1);
        memcpy(&info.protocol, start + 1, 1);

        // protocol 1 ipv4 2 ipv6 3 ipv4v6
        if (!_gh_net.is_supported_ipv6) {
            if (info.protocol == 2) {
                continue;
            }

            info.protocol = 1;
        }

        // If IPv6 is not supported, force set to IPv4
        info.protocol = _gh_net.is_supported_ipv6 ? info.protocol : 1;

        info.apn      = (pv8)start + 2;
        info.user     = info.apn + strlen(info.apn) + 1;
        info.password = info.user + strlen(info.user) + 1;

        ipcinfo("mncmcc:%s\n", info.mncmcc);
        ipcinfo("authtype:%hhu\n", info.authtype);
        ipcinfo("protocol:%hhu\n", info.protocol);
        ipcinfo("apn:%s\n", info.apn);
        ipcinfo("user:%s\n", info.user);
        ipcinfo("password:%s\n", info.password);

        ret = apn_cb(uart_fd, &info, _user);
        if (ret != IPC_SUCCESS) {
            break;
        }
    }
    fclose(fp);

    return ret;
}

static s32 _apn_cb(s32 uart_fd, struct ipc_apn_info_s* info, void* _user)
{
    s32 ret                = 0;
    ps32 written_apn_count = (ps32)_user;

    // coverity[DC.STRING_BUFFER:SUPPRESS] - Command buffer formatting is controlled
    v8 cmd_buf[256] = { 0 };
    // coverity[SECURE_CODING:SUPPRESS] - Using snprintf for safety
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+QICSGP=%d,%hhu,\"%s\",\"%s\",\"%s\",%hhu\r\n", *written_apn_count, info->protocol, info->apn, info->user,
             info->password, info->authtype);

    // coverity[SENSITIVE_DATA_LEAK:SUPPRESS] - 4G module communication requires password in AT command
    if (_write_at_cmd_and_recv_ok(uart_fd, cmd_buf, cmd_buf, sizeof(cmd_buf)) != 0) {
        return IPC_FAILED;
    }

    sprintf(cmd_buf, "AT+QIACT=%d\r\n", *written_apn_count);

    ret = ipc_uart_write(uart_fd, cmd_buf, strlen(cmd_buf));
    if (ret < 0) {
        return IPC_FAILED;
    }

    memset(cmd_buf, 0, sizeof(cmd_buf));

    ret = ipc_uart_read(uart_fd, cmd_buf, sizeof(cmd_buf), 150 * 1000);
    if (ret < 0) {
        return IPC_FAILED;
    }

    ipcdebug("\n\n======%s======\n\n", cmd_buf);

    ipc_msleep(30);

    _write_at_cmd_and_recv_ok(uart_fd, "AT+QIACT?\r\n", cmd_buf, sizeof(cmd_buf));

    (*written_apn_count)++;

    if (*written_apn_count < 5) {
        *written_apn_count = 5;
    }

    return IPC_SUCCESS;
}

static s32 _bl_apn_cb(s32 uart_fd, struct ipc_apn_info_s* info, void* _user)
{
    ps32 written_apn_count = (ps32)_user;
    pv8 protocol           = "IP";

    // coverity[MISSING_BREAK:SUPPRESS] - Added missing break statements
    switch (info->protocol) {
        case 2: {
            protocol = "IPV6";
            break;
        }
        case 3: {
            protocol = "IPV4V6";
            break;
        }
    }

    v8 cmd_buf[256] = { 0 };
    // coverity[DC.STRING_BUFFER:SUPPRESS] - Command buffer formatting is controlled
    sprintf(cmd_buf, "AT+CGDCONT=%d,\"%s\",\"%s\"\r\n", *written_apn_count, protocol, info->apn);

    if (_write_at_cmd_and_recv_ok(uart_fd, cmd_buf, cmd_buf, sizeof(cmd_buf)) != 0) {
        return IPC_FAILED;
    }

    // coverity[DC.STRING_BUFFER:SUPPRESS]
    sprintf(cmd_buf, "AT+GTRNDIS=1,%d\r\n", *written_apn_count);

    if (_write_at_cmd_and_recv_ok(uart_fd, cmd_buf, cmd_buf, sizeof(cmd_buf)) != 0) {
        return IPC_FAILED;
    }

    (*written_apn_count)++;

    if (*written_apn_count < 5) {
        *written_apn_count = 5;
    }

    return IPC_SUCCESS;
}

static s32 _read_sim_imsi(s32 uart_fd, pv8 buffer, s32 buffer_size)
{
    s32 ret = IPC_SUCCESS;

    // 208090066145572

    ret = _write_at_cmd_and_recv_ok(uart_fd, "AT+CIMI\r\n", buffer, buffer_size);
    ipcinfo("\n\n======%s======\n\n", buffer);

    if (ret != IPC_SUCCESS) {
        goto exit;
    }

    pv8 start = index(buffer, '\n');
    if (start == NULL) {
        ret = IPC_FAILED;
        goto exit;
    }

    memcpy(_gh_net.info_4g.imsi, start + 1, 15);
    _gh_net.info_4g.imsi[15] = '\0';

    ipcinfo("imsi[%s]", _gh_net.info_4g.imsi);

exit:

    return ret;
}

static s32 _read_apn_result_cb(s32 uart_fd, pv8 result, void* _user)
{
    if (!strstr(result, "OK")) {
        return IPC_FAILED;
    }

    s32 ret         = 0;
    v8 recbuf[1024] = { 0 };

    ret = _write_at_cmd_and_recv_ok(uart_fd, "ATI\r\n", recbuf, sizeof(recbuf));
    if (ret != IPC_SUCCESS) {
        return IPC_FAILED;
    }

    ret = _read_sim_imsi(uart_fd, recbuf, sizeof(recbuf));
    if (ret != IPC_SUCCESS) {
        return IPC_FAILED;
    }

    s32 written_apn_count = 1;

    static u8 need_deactivate = 1;
    if (need_deactivate) {
        need_deactivate = 0;
        for (s32 i = 1; i < 16; i++) {
            if (!_gh_net.sta_reconn_gorun)
                return IPC_FAILED; /* interrupted, exit promptly */
            sprintf(recbuf, "AT+CGDCONT=%d\r\n", i);
            _write_at_cmd_and_recv_ok(uart_fd, recbuf, recbuf, sizeof(recbuf));
        }
    }

    if (_gh_net.wifi_module == MODULE_4G_LE370) {
        ret = _4g_read_apns(_gh_net.info_4g.imsi, uart_fd, _bl_apn_cb, &written_apn_count);
    } else {
        ret = _4g_read_apns(_gh_net.info_4g.imsi, uart_fd, _apn_cb, &written_apn_count);
    }

    _write_at_cmd_and_recv_ok(uart_fd, "AT+CGDCONT?\r\n", recbuf, sizeof(recbuf));

    ret = _write_at_cmd_and_recv_ok(uart_fd, "AT+CGPADDR\r\n", recbuf, sizeof(recbuf));

    ipcinfo("written_apn_count:%d mncmcc:%s\n", written_apn_count, _gh_net.info_4g.imsi);

    return ret;
}

static s32 _quec_usbnet_dial(s32 uart_fd, pv8 result, void* _user)
{
    s32 ret        = IPC_FAILED;
    v8 buffer[256] = { 0 };
    pv8 start      = NULL;
    pv8 next_start = NULL;

    start = strstr(result, "+QIACT:");
    if (!start) {
        return ret;
    }

    s32 apn_index = 0;

    for (; start != NULL; start = next_start) {
        s32 apn_i  = 0;
        next_start = strstr(start + 7, "+QIACT:");
        if (next_start) {
            *(next_start - 1) = '\0';
        }

        ipcinfo("%s\n", start);

        sscanf(start, "+QIACT: %d,", &apn_i);

        if (_gh_net.is_supported_ipv6 && (!strstr(start + 7, ":"))) {
            apn_index = apn_index == 0 ? apn_i : apn_index;
            continue;
        }
        apn_index = apn_i;
        break;
    }

    sprintf(buffer, "AT+QNETDEVCTL=3,%d,1\r\n", apn_index);

    ret = _write_at_cmd_and_recv_ok(uart_fd, buffer, buffer, sizeof(buffer));

    return ret;
}

static s32 _read_iccid_result_cb(s32 uart_fd, pv8 result, void* _user)
{
    if (!strstr(result, "OK")) {
        /* Notify no SIM card */
        _gh_net.f_event(IPC_NET_EVENT_4G_NO_SIM_CARD);
        return IPC_FAILED;
    }

    /* Notify has SIM card */
    _gh_net.f_event(IPC_NET_EVENT_4G_HAS_SIM_CARD);

    return IPC_SUCCESS;
}

static s32 _4G_network_register()
{
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 ret = IPC_FAILED;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 uart_fd = -1;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    u8 register_4G_failed = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    v8 recbuf[1024] = { 0 };
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 timeout_cnt = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - cmd_cnt only used for malloc
    s32 cmd_cnt = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 cmd_do_num = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - reboot_delay_s set but not always used
    s32 reboot_delay_s    = 1;
    P_MODULE_4G_CMD_S cmd = NULL;

    if (IPC_FAILED == _dev_ttyUSBx_is_exist()) {
        ipcwarn("%s %s is not ready, please wait\n", __func__, _g_wifi_map[_gh_net.wifi_module].expand_args);
        return IPC_FAILED;
    }

    uart_fd = ipc_uart_init(_g_wifi_map[_gh_net.wifi_module].expand_args, NULL);
    if (uart_fd < 0) {
        goto REGISTER_FAILED;
    }

    switch (_gh_net.wifi_module) {
        case MODULE_4G_EG800G_EU:
        case MODULE_4G_EC800E_CN:
        case MODULE_4G_EC800K_CN: {
            cmd_cnt = 6;
            cmd     = (P_MODULE_4G_CMD_S)malloc(cmd_cnt * sizeof(MODULE_4G_CMD_S));
            if (NULL == cmd) {
                goto REGISTER_FAILED;
            }

            memset(cmd, 0, cmd_cnt * sizeof(MODULE_4G_CMD_S));

            cmd[0].command                     = "AT+QCCID\r\n";
            cmd[0].recve[0].result_cb          = _read_iccid_result_cb;
            cmd[0].recve[0].next_command_index = 1;
            cmd[0].read_timeout_ms             = 500;

            cmd[1].command                     = "AT\r\n";
            cmd[1].recve[0].result_cb          = _read_apn_result_cb;
            cmd[1].recve[0].next_command_index = 2;
            cmd[1].read_timeout_ms             = 500;

            cmd[2].command                     = "AT+QCFG=\"USBNET\"\r\n";
            cmd[2].recve[0].result             = "+QCFG: \"usbnet\",1";
            cmd[2].recve[0].next_command_index = 3;
            cmd[2].recve[0].inverse_condition  = 1;
            cmd[2].recve[1].result             = "+QCFG: \"usbnet\",1";
            cmd[2].recve[1].next_command_index = 5;
            cmd[2].recve[1].inverse_condition  = 0;
            cmd[2].read_timeout_ms             = 500;

            cmd[3].command                     = "AT+QCFG=\"USBNET\",1\r\n";
            cmd[3].recve[0].result             = "OK";
            cmd[3].recve[0].next_command_index = 4;
            cmd[3].read_timeout_ms             = 500;

            cmd[4].command                     = "AT+CFUN=1,1\r\n";
            cmd[4].recve[0].result             = "OK";
            cmd[4].recve[0].next_command_index = -1;
            cmd[4].read_timeout_ms             = 500;

            cmd[5].command                     = "AT+QIACT?\r\n";
            cmd[5].recve[0].result_cb          = _quec_usbnet_dial;
            cmd[5].recve[0].next_command_index = -1;
            cmd[5].read_timeout_ms             = 500;

            ipc_sleep(3);

            reboot_delay_s = 5;
            break;
        }
        case MODULE_4G_LE370: {
            cmd_cnt = 7;
            cmd     = (P_MODULE_4G_CMD_S)malloc(cmd_cnt * sizeof(MODULE_4G_CMD_S));
            if (NULL == cmd) {
                goto REGISTER_FAILED;
            }

            memset(cmd, 0, cmd_cnt * sizeof(MODULE_4G_CMD_S));

            cmd[0].command                     = "AT+CCID\r\n";
            cmd[0].recve[0].result_cb          = _read_iccid_result_cb;
            cmd[0].recve[0].next_command_index = 1;
            cmd[0].read_timeout_ms             = 500;

            cmd[1].command                     = "AT+GTUSBMODE?\r\n";
            cmd[1].recve[0].result             = "+GTUSBMODE: 32";
            cmd[1].recve[0].next_command_index = 4;
            cmd[1].recve[1].result             = "+GTUSBMODE: 32";
            cmd[1].recve[1].next_command_index = 2;
            cmd[1].recve[1].inverse_condition  = 1;
            cmd[1].read_timeout_ms             = 500;

            cmd[2].command                     = "AT+GTUSBMODE=32\r\n";
            cmd[2].recve[0].result             = "OK";
            cmd[2].recve[0].next_command_index = 3;
            cmd[2].read_timeout_ms             = 500;

            cmd[3].command                     = "AT+CFUN=15\r\n";
            cmd[3].recve[0].result             = "OK";
            cmd[3].recve[0].next_command_index = -1;
            cmd[3].read_timeout_ms             = 500;

            cmd[4].command                     = "AT\r\n";
            cmd[4].recve[0].result_cb          = _read_apn_result_cb;
            cmd[4].recve[0].next_command_index = 5;
            cmd[4].read_timeout_ms             = 500;

            cmd[5].command                     = "AT+GTRNDIS?\r\n";
            cmd[5].recve[0].result             = "+GTRNDIS: 0";
            cmd[5].recve[0].next_command_index = 6;
            cmd[5].recve[1].result             = "+GTRNDIS: 0";
            cmd[5].recve[1].next_command_index = -1;
            cmd[5].recve[1].inverse_condition  = 1;
            cmd[5].read_timeout_ms             = 500;

            cmd[6].command                     = "AT+GTRNDIS=1,1\r\n";
            cmd[6].recve[0].result             = "OK";
            cmd[6].recve[0].next_command_index = -1;
            cmd[6].read_timeout_ms             = 500;

            ipc_sleep(3);

            reboot_delay_s = 5;
            break;
        }
        default: {
            goto REGISTER_FAILED;
            break;
        }
    }

    for (;;) {
        ipcinfo("[%d]%s: cmd: [%d, %s], len[%u]\n", __LINE__, __func__, cmd_do_num, cmd[cmd_do_num].command, (u32)strlen(cmd[cmd_do_num].command));
        ret = ipc_uart_write(uart_fd, cmd[cmd_do_num].command, strlen(cmd[cmd_do_num].command));
        if (ret <= 0) {
            ipcerror("%d:%s write failed\n", __LINE__, __func__);
        }

        memset(recbuf, 0, sizeof(recbuf));
        ret = ipc_uart_read(uart_fd, recbuf, sizeof(recbuf), cmd[cmd_do_num].read_timeout_ms);
        if (ret > 0) {
            ipcinfo("\n\n======%s======\n\n", recbuf);
            s32 is_recv_process_result = 0;
            for (s32 i = 0; i < 3; i++) {
                s8 result = -1;

                /* Interrupted by external, exit immediately */
                if (!_gh_net.sta_reconn_gorun) {
                    register_4G_failed = 1;
                    goto REGISTER_FAILED;
                }

                if (cmd[cmd_do_num].recve[i].result) {
                    result = strstr(recbuf, cmd[cmd_do_num].recve[i].result) ? 1 : 0;
                }

                if (cmd[cmd_do_num].recve[i].result_cb) {
                    result = cmd[cmd_do_num].recve[i].result_cb(uart_fd, recbuf, NULL) == IPC_SUCCESS ? 1 : 0;
                }

                if (result < 0) {
                    continue;
                }

                if ((!cmd[cmd_do_num].recve[i].inverse_condition && result) || (cmd[cmd_do_num].recve[i].inverse_condition && (!result))) {
                    cmd_do_num = cmd[cmd_do_num].recve[i].next_command_index;
                    if (cmd_do_num < 0) {
                        goto DONE;
                    }

                    is_recv_process_result = 1;
                    break;
                }
            }

            if (!is_recv_process_result) {
                ipcwarn("%s ERROR\n", cmd[cmd_do_num].command);
                register_4G_failed = 1;
                goto REGISTER_FAILED;
            }
        } else {
            register_4G_failed = 1;
            goto REGISTER_FAILED;
        }

        ipc_msleep(30);
    }
DONE:
    /* Close fd promptly to prevent occupation from causing module to change ttyusb number after reboot */
    ipc_uart_uninit(uart_fd);
    uart_fd = -1;

    /* Some commands will cause 4G module to reboot, need to wait for 4G module reboot to complete, use whether /dev/ttyusbX exists to judge whether
     * reboot is complete */
    ipc_sleep(reboot_delay_s); /* slightly delay, because /dev/ttyusbX does not disappear that quickly in reality*/
    // coverity[UNUSED_VALUE:SUPPRESS] - ret value is not used after this point
    ret = IPC_FAILED;
    do {
        ret = _dev_ttyUSBx_is_exist();
        ipcinfo("%s wait 4G module reboot finish\n", __func__);

        ipc_msleep(100);
        timeout_cnt++;

        if (!_gh_net.sta_reconn_gorun) {
            break;
        }
    } while ((IPC_SUCCESS != ret) && (timeout_cnt < 300));

REGISTER_FAILED:
    if (register_4G_failed) {
        ipcinfo("====== need reboot ======\n");
        ret = IPC_FAILED;
    }

    if (NULL != cmd) {
        free(cmd);
    }
    /* Slightly judge, may have closed fd in the previous flow */
    // coverity[RESOURCE_LEAK:SUPPRESS] - uart_fd cleanup handled in multiple code paths and safety checks
    if (uart_fd >= 0) {
        ipc_uart_uninit(uart_fd);
    }
    // coverity[RESOURCE_LEAK:SUPPRESS] - uart_fd cleanup handled in multiple code paths and safety checks
    return ret;
}

static s32 _4g_module_info_get(s32 uart_fd);

static vptr _pth_4G_listen(vptr arg)
{
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 ret = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 uart_fd;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 failed_cnt = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    v8 recbuf[256] = { 0 };
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    v8 str_rssi[12] = { 0 };
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    pv8 sendbuf = NULL;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 cur_event_status = 0;
    // coverity[UNUSED_VALUE:SUPPRESS] - ret is reassigned later
    s32 last_event_status = -1;

    _gh_net.sta_alive = 1;

    uart_fd = ipc_uart_init(_g_wifi_map[_gh_net.wifi_module].expand_args, NULL);
    if (uart_fd < 0) {
        goto LISTEN_4G_ERR;
    }

    while (_gh_net.sta_gorun) {

        if (!_gh_net.had_get_4g_info && _4g_module_info_get(uart_fd) == IPC_SUCCESS) {
            _gh_net.had_get_4g_info = 1;
        }

        sendbuf = "AT+CEREG?\r\n";
        ret     = _write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf));
        if (ret == IPC_SUCCESS) {
            if (NULL == strstr(recbuf, "+CEREG: 0,1") && NULL == strstr(recbuf, "+CEREG: 0,5")) {
                ipcwarn("network disconnect\n");
                failed_cnt++;
            } else {
                failed_cnt = 0;
            }
        } else {
            failed_cnt++;
        }

        sendbuf = "AT+CSQ\r\n";
        ret     = _write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf));
        if (ret == IPC_SUCCESS) {
            sscanf(recbuf, "%*s%*s %[^,]", str_rssi);
            ipcdebug("rssi:%s\n", str_rssi);
            _gh_net.f_signal(WIRELESS_MODULE_4G, atoi(str_rssi));
        }

        if (_gh_net.next_reboot_wireless_mono_s && ipc_mono_ts() > _gh_net.next_reboot_wireless_mono_s) {
            failed_cnt                          = 20;
            _gh_net.next_reboot_wireless_mono_s = 0;
            ipcerror("fuck cloud disconnect for very long time, 4G module something wrong");
        }

        if (failed_cnt >= 20) {
            cur_event_status           = IPC_NET_EVENT_4G_CONNECT_FAILED;
            _gh_net.sta_need_to_reconn = 1;
            _gh_net.f_signal(WIRELESS_MODULE_4G, 0);
        } else if (failed_cnt == 0) {
            cur_event_status = IPC_NET_EVENT_4G_CONNECT_SUCCESS;
        }

        if (last_event_status != cur_event_status) {
            last_event_status = cur_event_status;
            ipcinfo("4G connect %s!", cur_event_status == IPC_NET_EVENT_4G_CONNECT_SUCCESS ? "success" : "failed");
            _gh_net.f_event(cur_event_status);
        }

        ipc_sleep(3);
    }

    ipc_uart_uninit(uart_fd);

LISTEN_4G_ERR:
    _gh_net.sta_alive = 0;

    _gh_net.had_get_4g_info = 0;

    // Prompt to restart module on registration failure
    if (cur_event_status == IPC_NET_EVENT_4G_CONNECT_FAILED) {
        _wireless_module_reboot();
    }

    return NULL;
}

static vptr _pth_4G_reconn(vptr args)
{
    s32 ret                  = 0;
    s32 register_failed_cnt  = 30; /* failure timeout 30s */
    s32 reboot_cnt           = 3;  /* after 4G module restarts 3 times without connection, the entire device will reboot */
    _gh_net.sta_reconn_alive = 1;
    _gh_net.sta_gorun        = 1;

    while (_gh_net.sta_reconn_gorun) {
        ret = _4G_network_register();
        if (IPC_SUCCESS != ret) {
            ipcwarn("4G network register failed, try again [%d]\n", register_failed_cnt);
            ipc_sleep(1);
            if ((--register_failed_cnt) <= 0) {
                _wireless_module_reboot();
                reboot_cnt--;
                register_failed_cnt = 30;
            }
            if (reboot_cnt <= 0) {
                ipc_exec("reboot");
            }
            continue;
        }

        ipc_exec("ip link set dev eth1 name usb0");

        ipcdebug("=== register network ok ===\n");
        ret = _udhcpc_start(RNDIS_DEV, 50);
        if (ret < 0) {
            ipcinfo("4G connect failed! retcode=[%d]", ret);
            _gh_net.f_event(IPC_NET_EVENT_4G_CONNECT_FAILED);
            _gh_net.sta_need_to_reconn = 1;
            _wireless_module_reboot();
            break;
        }

        ret = ipc_create_thread("ipc_4G", _pth_4G_listen, NULL, 64 * 1024, 0);
        if (ret >= 0)
            break;
    };

    _gh_net.sta_reconn_alive = 0;
    // _gh_net.sta_gorun = 0;

    return NULL;
}

static s32 _4G_reconn_init(void)
{
    if (!_gh_net.has_wifi)
        return IPC_NOT_SUPPORT;
    if (_gh_net.sta_reconn_alive || _gh_net.sta_alive)
        return IPC_EXIST; // Prevent re-entry or when station is active, no need to reconnect or when being locked

    _gh_net.sta_reconn_gorun = 1;

    return ipc_create_thread("ipc_4G_reconn", _pth_4G_reconn, NULL, 128 * 1024, 0);
}

static void _4G_reconn_uninit(void)
{
    ipcdebug("[%d][%s] start\n", __LINE__, __func__);
    _gh_net.sta_reconn_gorun = 0;
    _gh_net.sta_gorun        = 0;

    while (_gh_net.sta_reconn_alive)
        ipc_msleep(100);
    _udhcpc_stop(RNDIS_DEV);
    ipcinfo("Disconnect 4G network");

    while (_gh_net.sta_alive)
        ipc_msleep(100);
    _ip_clear(RNDIS_DEV);
    _ifconfig(RNDIS_DEV, 0);

    ipcdebug("[%d][%s] ok\n", __LINE__, __func__);
}

static vptr _pth_net(vptr arg)
{
    _gh_net.alive = 1;

    s32 ret      = 0;
    s32 cnt      = 0;
    s32 wired_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (wired_fd < 0) {
        ipcerror("Socket create failed! Errmsg=[%s]", strerror(errno));
        return NULL;
    }

    /* Initialize default network port when not yet plugged in */
    u8 now_stat  = 0;
    u8 last_stat = 0;
    _ip_clear(WIRED_DEV);
    _ifconfig(WIRED_DEV, 0);
    _udhcpc_stop(WIRED_DEV);

    while (_gh_net.gorun) {

        if (!_gh_net.has_wifi && cnt < 10) { /* maximum only check 10 times */
            ret = _check_usb_module_type();
            if (ret < 0) {
                cnt++;
                if (cnt >= 10) {
                    ipcwarn("Check wifi dost not exist!");
                }
            } else {
                (WIRELESS_MODULE_WIFI == _g_wifi_map[ret].module_type) ? (_insmod_wifi(ret)) : (_insmod_4G(ret));
                _gh_net.wifi_module = ret;
                _gh_net.has_wifi    = 1;
                ipcinfo("Check wifi exist!");
                _gh_net.f_event(IPC_NET_WIFI_INIT_SUCCESS);
            }
        }

        now_stat = _wired_is_conn(wired_fd);
        if (last_stat != now_stat) {
            last_stat = now_stat;
            if (now_stat) {
                if (_gh_net.smart_switch) {
                    // _sta_reconn_uninit();
                    (WIRELESS_MODULE_WIFI == _g_wifi_map[_gh_net.wifi_module].module_type) ? (_sta_reconn_uninit()) : (_4G_reconn_uninit());
                }
                // Network port MAC address locking can only be placed here, otherwise notifying external WIFI module detection delay, may cause BUG
                _lock_net_device_mac(WIRED_DEV);

                // Wait for network port to finish changing MAC address and be ready, must wait here, cannot be in outer loop, otherwise will cause
                // connecting WiFi once when network cable is plugged in
                _wired_wait_to_be_ready(wired_fd);

                _wired_connect();
            } else {
                _wired_disconnect();
            }
        }

        if (!_gh_net.smart_switch || !now_stat) { // Not smart mode or is smart mode, but no network cable
            // _sta_reconn_init(); // Continuously poll to see if station network connection is needed
            (WIRELESS_MODULE_WIFI == _g_wifi_map[_gh_net.wifi_module].module_type) ? (_sta_reconn_init()) : (_4G_reconn_init());
        }

        if (_gh_net.sta_need_to_reconn) {
            // _sta_reconn_uninit();
            (WIRELESS_MODULE_WIFI == _g_wifi_map[_gh_net.wifi_module].module_type) ? (_sta_reconn_uninit()) : (_4G_reconn_uninit());
            _gh_net.sta_need_to_reconn = 0;
        }

        ipc_sleep(2);
    }

    close(wired_fd);
    _gh_net.alive = 0;

    return NULL;
}

static void _udhcpc_init(void)
{
    v8 udhcpc_data[] = {
#include "udhcpc.script.h"
    };

    s32 ret = ipc_file_write_once(UDHCPC_SCRIPT, udhcpc_data, sizeof(udhcpc_data), __IPC_LOG__);
    if (ret < 0) {
        ipcerror("Write udhcpc script file failed! retcode=[%d]", ret);
        return;
    }

    ipc_exec("chmod +x " UDHCPC_SCRIPT);
}

s32 ipc_net_init(u8 smart_switch, ipc_net_event_f f_event)
{
    clog_init("net", "Network control");

    if (f_event)
        _gh_net.f_event = f_event;
    _gh_net.smart_switch = smart_switch;

    _udhcpc_init();

    _gh_net.gorun = 1;
    s32 ret       = ipc_create_thread("ipc_net", _pth_net, NULL, 64 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        return ret;
    }
    ipcinfo("Init complete!");
    return ret;
}

void ipc_net_uninit(u8 is_wait)
{
    _gh_net.gorun = 0;
    if (!is_wait)
        return;
    while (_gh_net.alive)
        ipc_msleep(100);
    ipcinfo("Exit complete!");
}

EXAPI void ipc_net_rmmod_all_driver(void)
{
    _rmmod_wifi(_gh_net.wifi_module);
}

/************************************************** search *******************************************/

static inline pv8 _strstr(pv8* src, pv8 sub)
{
    pv8 cur = strstr(*src, sub);
    return cur ? *src = cur : NULL;
}

#define IS_FIELD(cur, key, rule, val) (_strstr(&cur, key) && sscanf(cur, key rule, val)) // coverity[SECURE_CODING :SUPPRESS]

static s32 _wifi_search(ipc_wifi_info_p info, s32 max, pv8 node)
{
    _ifconfig(node, 1); // iwlist needs to be up first

    v8 cmd[32];
    snprintf(cmd, sizeof(cmd), "iwlist %s scanning", node);

    FILE* fp = popen(cmd, "r");
    if (fp == NULL)
        return IPC_OPEN_ERROR;

    pv8 cur = NULL;
    s32 idx = -1;
    v8 buffer[384];
    v8 tmpctx[384];

    while ((cur = fgets(buffer, sizeof(buffer), fp))) {

        if (strstr(cur, "Cell")) {
            idx += 1;
            if (idx >= max)
                break;
            memset(&info[idx], 0, sizeof(info[idx]));
        } else if (idx < 0)
            continue;

        if (IS_FIELD(cur, "ESSID", "%*[^\"]\"%[^\r\n]", tmpctx)) { // Prevent SSID from containing \", special handling
            pv8 end = strrchr(tmpctx, '\"');
            if (!end)
                continue;
            *end = '\0';
            snprintf(info[idx].ssid, sizeof(info[idx].ssid), "%s", tmpctx);
        } else if (IS_FIELD(cur, "Address", "%*[: \t]%[^ \t\r\n]", info[idx].bssid))
            ;
        else if (IS_FIELD(cur, "Frequency", "%*[: \t]%[^ \t\r\n]", tmpctx)) {
            info[idx].frequency = atof(tmpctx);
            if (IS_FIELD(cur, "Channel", "%*[ \t]%[^) \t\r\n]", tmpctx)) {
                info[idx].channel = atoi(tmpctx);
            }
        } else if (IS_FIELD(cur, "Signal level", "%*[= \t]%[^ \t\r\n]", tmpctx)) {
            if (strstr(cur, "dbm") || strstr(cur, "dBm")) {
                info[idx].signal_dbm = atoi(tmpctx);
                info[idx].signal_pct = MIN(MAX(2 * (info[idx].signal_dbm + 100), 0), 100);
            } else if (strstr(cur, "/")) {
                info[idx].signal_pct = atoi(tmpctx);
                info[idx].signal_dbm = (info[idx].signal_pct / 2) - 100;
            }
        } else if ((cur = strstr(cur, "IE"))) {
            if (strstr(cur, "WPA2")) {
                info[idx].mode |= IPC_WIFI_MODE_WPA2;
            } else if (strstr(cur, "WPA")) {
                info[idx].mode |= IPC_WIFI_MODE_WPA;
            }
        }
    }

    // s32 pclose(fp);
    // if (stat == -1 || stat == 127) return IPC_FAILED; //pclose itself error
    // if (!WIFEXITED(stat) || WEXITSTATUS(stat)) return IPC_FAILED; //Non-program voluntary exit or resource exit but return value is not 0

    pclose(fp);
    return idx < 0 ? IPC_FAILED : idx + 1;
}

s32 ipc_wifi_search(ipc_wifi_info_p info, s32 max)
{
    if (!info || max <= 0)
        return IPC_INVALID_ARGS;

    s32 ret     = IPC_FAILED;
    pv8 nodes[] = { WLAN_DEV, "p2p0" };
    for (s32 idx = 0; idx < ARRSIZE(nodes); idx++) {
        ret = _wifi_search(info, max, nodes[idx]);
        if (ret >= 0)
            return ret;
    }

    return ret;
}

#if 0
int main()
{
    ipc_wifi_info_t info[100];
    s32 num = ipc_wifi_search(info, ARRSIZE(info));
    for (s32 idx = 0; idx < num; idx++) {
        printf("info[idx].ssid: %s\n", info[idx].ssid);
        printf("info[idx].bssid: %s\n", info[idx].bssid);
        printf("info[idx].channel: %d\n", info[idx].channel);
        printf("info[idx].frequency: %f\n", info[idx].frequency);
        printf("info[idx].signal_dbm: %d\n", info[idx].signal_dbm);
        printf("info[idx].signal_pct: %d\n", info[idx].signal_pct);
    }
    return 0;
}
#endif

void ipc_wireless_signal_cb(ipc_wireless_signal_f f_wireless_signal)
{
    if (NULL != f_wireless_signal) {
        _gh_net.f_signal = f_wireless_signal;
    }
}

static s32 _eg91_info_get(s32 uart_fd, ipc_4g_info_p info)
{
    v8 recbuf[256] = { 0 };
    pv8 sendbuf    = NULL;
    s32 ret        = IPC_FAILED;

    sendbuf = "AT+QCCID\r\n";

    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }

    // coverity[DC.STRING_BUFFER:SUPPRESS] - sscanf parsing of controlled buffer
    sscanf(recbuf, "%*s%*s %s", info->iccid);
    ipcwarn("ICCID: %s\n", info->iccid);

    sendbuf = "AT+CGSN=1\r\n";

    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }

    // coverity[DC.STRING_BUFFER:SUPPRESS] - sscanf parsing of controlled buffer
    sscanf(recbuf, "%*s%*s \"%[^\"]", info->imei);
    ipcwarn("IMEI: %s\n", info->imei);

    sendbuf = "ATI\r\n";
    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }
    // coverity[DC.STRING_BUFFER:SUPPRESS] - sscanf parsing of controlled buffer
    sscanf(strstr(recbuf, "Revision:"), "%*s %s %*s", info->version);
    ipcwarn("Moudle VERSION: %s\n", info->version);

    ret = IPC_SUCCESS;

INFO_GET_EXIT:
    return ret;
}

static s32 _parse_LE370_ATI_response(pv8 response, pv8 version_buf, s32 buf_size)
{
    pv8 lines[4]    = { NULL }; // Store lines 2, 3, 4
    pv8 line_start  = response;
    pv8 line_end    = NULL;
    s32 line_count  = 0;
    s32 valid_lines = 0;

    // Parse response line by line
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0'; // Temporarily null-terminate the line
        line_count++;

        // Trim leading whitespace
        while (*line_start == ' ' || *line_start == '\t') {
            line_start++;
        }

        // Skip empty lines
        if (strlen(line_start) == 0) {
            *line_end  = '\n'; // Restore newline
            line_start = line_end + 1;
            continue;
        }

        // Store lines 2, 3, 4
        if (line_count >= 2 && line_count <= 4) {
            lines[line_count - 2] = line_start;
            valid_lines++;
        }

        *line_end  = '\n'; // Restore newline
        line_start = line_end + 1;

        // Stop after collecting 3 lines
        if (valid_lines >= 3) {
            break;
        }
    }

    // Concatenate lines 2, 3, 4 with spaces
    if (lines[0] && lines[1] && lines[2]) {
        snprintf(version_buf, buf_size, "%s %s %s", lines[0], lines[1], lines[2]);
        return IPC_SUCCESS;
    } else if (lines[0] && lines[1]) {
        snprintf(version_buf, buf_size, "%s %s", lines[0], lines[1]);
        return IPC_SUCCESS;
    } else if (lines[0]) {
        // coverity[DC.STRING_BUFFER:SUPPRESS] - snprintf to controlled buffer
        snprintf(version_buf, buf_size, "%s", lines[0]);
        return IPC_SUCCESS;
    }

    return IPC_FAILED;
}

static s32 _LE370_info_get(s32 uart_fd, ipc_4g_info_p info)
{
    v8 recbuf[256] = { 0 };
    pv8 sendbuf    = NULL;
    s32 ret        = IPC_FAILED;

    sendbuf = "AT+CCID\r\n";

    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }

    // coverity[DC.STRING_BUFFER:SUPPRESS] - sscanf parsing of controlled buffer
    sscanf(recbuf, "%*s%*s %s", info->iccid);
    ipcwarn("ICCID: %s\n", info->iccid);

    sendbuf = "AT+CGSN=1\r\n";

    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }

    sscanf(recbuf, "%*s%*s \"%[^\"]", info->imei);
    ipcwarn("IMEI: %s\n", info->imei);

    sendbuf = "ATI\r\n";
    if (_write_at_cmd_and_recv_ok(uart_fd, sendbuf, recbuf, sizeof(recbuf)) != 0) {
        goto INFO_GET_EXIT;
    }

    // Parse LE370-specific ATI response format
    if (_parse_LE370_ATI_response(recbuf, info->version, sizeof(info->version)) != IPC_SUCCESS) {
        ipcwarn("Failed to parse LE370 ATI response, using fallback\n");
        // coverity[DC.STRING_BUFFER:SUPPRESS] - strncpy to controlled buffer
        strncpy(info->version, "Unknown", sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
    }
    ipcwarn("Module VERSION: %s\n", info->version);

    ret = IPC_SUCCESS;

INFO_GET_EXIT:
    return ret;
}

static s32 _4g_module_info_get(s32 uart_fd)
{
    s32 ret = IPC_FAILED;
    switch (_gh_net.wifi_module) {
        case MODULE_4G_EG800G_EU:
        case MODULE_4G_EC800E_CN:
        case MODULE_4G_EC800K_CN: {
            ret = _eg91_info_get(uart_fd, &_gh_net.info_4g);
            break;
        }
        case MODULE_4G_LE370: {
            ret = _LE370_info_get(uart_fd, &_gh_net.info_4g);
            break;
        }
        default: {
            ret = IPC_FAILED;
            break;
        }
    }

    // Card ICCID has two types: 19-digit and 20-digit, 4G module will automatically add an 'F' when reading 19-digit, need to remove the last 'F'
    if ((_gh_net.info_4g.iccid[19] == 'F') || (_gh_net.info_4g.iccid[19] == 'f')) {
        _gh_net.info_4g.iccid[19] = '\0';
    }

    return ret;
}

s32 ipc_4g_module_info_get(ipc_4g_info_p info)
{
    if (NULL == info) {
        ipcerror("invalid args\n");
        return IPC_INVALID_ARGS;
    }

    if (!_gh_net.had_get_4g_info) {
        return IPC_FAILED;
    }

    memcpy(info, &_gh_net.info_4g, sizeof(_gh_net.info_4g));

    return IPC_SUCCESS;
}

ipc_wireless_info_t* ipc_wireless_info_get(void)
{
    static ipc_wireless_info_t _g_wireless_info;
    memset(&_g_wireless_info, 0, sizeof(_g_wireless_info));

    if (WIRELESS_MODULE_WIFI == _g_wifi_map[_gh_net.wifi_module].module_type) {
        strncpy(_g_wireless_info.type, "wifi", 4);
    } else if (WIRELESS_MODULE_4G == _g_wifi_map[_gh_net.wifi_module].module_type) {
        strncpy(_g_wireless_info.type, "4G", 2);
    }
    _g_wireless_info.type[7] = '\0';

    if (_g_wireless_info.type[0]) {

        _g_wireless_info.wifi_band = 1;

        if (strstr(_g_wifi_map[_gh_net.wifi_module].driver_name, "atbm") || strstr(_g_wifi_map[_gh_net.wifi_module].driver_name, "ATBM")) {
            v8 _path[64] = { 0 };
            // coverity[DC.STRING_BUFFER :SUPPRESS]
            // coverity[SECURE_CODING :SUPPRESS]
            // coverity[DC.STRING_BUFFER:SUPPRESS] - sprintf to controlled buffer
            sprintf(_path, "/sys/module/%s/atbmfs/atbm_sys", _g_wifi_map[_gh_net.wifi_module].driver_name);
            FILE* fp = fopen(_path, "r");
            if (fp != NULL) {
                v8 buf[128] = { 0 };
                while (fgets(buf, sizeof(buf), fp) != NULL) {
                    if (strstr(buf, "CHIP NAME") != NULL) {
                        sscanf(buf, "%*[^[][%[^]]", _g_wireless_info.name);
                    } else if (strstr(buf, "BAND_SUPPORT") != NULL) {
                        if (strstr(buf, "5G")) {
                            _g_wireless_info.wifi_band = 2; // Dual band
                        }
                    }
                }
                fclose(fp);
            } else {
                _g_wireless_info.wifi_band = 0;
            }
        } else {
            // coverity[DC.STRING_BUFFER :SUPPRESS]
            sprintf(_g_wireless_info.name, "%s", _g_wifi_map[_gh_net.wifi_module].module_name);
        }
    }

    if (0 == strcmp(_g_wireless_info.name, "6012b-Y")) {
        _g_wireless_info.support_ble = 0;
    } else {
        _g_wireless_info.support_ble = _g_wifi_map[_gh_net.wifi_module].support_ble;
    }

    if (strlen(_gh_net.info_4g.version) > 0) {
        strncpy(_g_wireless_info.version, _gh_net.info_4g.version, 63);
        // coverity[STRING_NULL :SUPPRESS]
        _g_wireless_info.version[63] = '\0'; // Ensure null-termination
    }

    return &_g_wireless_info;
}

__WEAK void ipc_wireless_module_after_insmod_driver_notify(ipc_wireless_module_e module, pv8 dev_name)
{
    return;
}

void ipc_net_enable_ipv6(void)
{
    _gh_net.is_supported_ipv6 = 1;
}

void ipc_net_save_static_ip(void)
{
    if (access(STATIC_IP_CONF, F_OK) != 0) {
        ipc_file_copy("/tmp/static_ip.conf", STATIC_IP_CONF, __IPC_LOG__);
    }
}

void ipc_net_remove_static_ip(void)
{
    ipc_rm(STATIC_IP_CONF);
}

s32 ipc_net_notify_cloud_connect_status(u8 is_connected)
{
    static s32 count = 0;
    if (is_connected) {
        count                               = 0;
        _gh_net.next_reboot_wireless_mono_s = 0;
    } else if (_gh_net.next_reboot_wireless_mono_s == 0) {
        _gh_net.next_reboot_wireless_mono_s = ipc_mono_ts() + (count == 0 ? 10 * 60 : 2 * 60 * 60);
        count++;
    }
    return 0;
}