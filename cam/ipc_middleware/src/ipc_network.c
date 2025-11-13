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
    u8 gorun;                      ///< Flag indicating thread control
    u8 alive;                      ///< Feedback on thread survivability
    u8 has_wifi;                   ///< Whether Wi-Fi has been detected
    u8 wifi_module;                ///< Wi-Fi module type
    u8 sta_gorun;                  ///< Station control flag
    u8 sta_alive;                  ///< Station survival feedback
    u8 sta_lock;                   ///< When externally networking, lock to prevent the reconnection mechanism from taking effect
    u8 smart_switch;               ///< Intelligent disconnection of Wi-Fi when plugging and unplugging the network port
    u8 sta_reconn_gorun;           ///< Control of the station reconnection thread
    u8 sta_reconn_alive;           ///< Feedback from the station reconnection thread
    u8 sta_need_to_reconn;         ///< Station reconnection mark
    u8 wired_conn_gorun;           ///< Network port connection thread control flag
    u8 wired_conn_alive;           ///< Network port connection thread survival feedback
    v8 hostname[32];               ///< Custom hostname
    v8 ssid[33];                   ///< Wi-Fi SSID
    v8 pwd[65];                    ///< Wi-Fi password
    v8 country_code[6];            ///< Country code
    ipc_net_event_f f_event;        ///< Event notification
    ipc_wireless_signal_f f_signal; ///< Signal strength notification
    ipc_4g_info_t info_4g;          ///< 4G information buffer
    u8 had_get_4g_info;            ///< 4G module information has been successfully obtained
    u8 is_supported_ipv6;
    u32 next_reboot_wireless_mono_s;
} _gh_net = {
    .hostname = "Smart_Camera",
    .f_event  = NOT_DO_ANYTHING,
    .f_signal = NOT_DO_ANYTHING,
};

/******************************** wifi module ********************************/

enum {
    MODULE_WIFI_RTL8188,
    MODULE_WIFI_ATBM6032,
    MODULE_WIFI_ATBM6062,
    MODULE_WIFI_SV6255,
    MODULE_WIFI_ATBM6012,
    MODULE_WIFI_ATBM6132,

    WIFI_MODULE_MAX,
};

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
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6032, "007a:8888", "atbm603x_wifi_usb", "", WIRELESS_MODULE_WIFI, 0),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6062, "007a:6052", "atbm606x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_SV6255, "8065:6000", "ssv6x5x", "stacfgpath=" WIFI_DRIVERS_PATH "/ssv6x5x-wifi.cfg", WIRELESS_MODULE_WIFI, 0),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6012, "007a:888b", "atbm603x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
    NEW_WIRELESS_MODULE(MODULE_WIFI_ATBM6132, "007a:8890", "atbm613x_wifi_usb", "wifi_bt_comb=1", WIRELESS_MODULE_WIFI, 1),
};

static s32 _ifconfig(pv8 dev, s8 sw);

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
    if (MODULE_WIFI_ATBM6062 == num) { // atbm6062 supports wifi6 and requires a cfg80211 driver that supports wifi6
        insmod_ko[0] = WIFI_DRIVERS_PATH "/cfg80211_wifi6.ko";
    } else { // Other wifi use ordinary cfg80211 and mac80211
        insmod_ko[0] = WIFI_DRIVERS_PATH "/cfg80211.ko";
        insmod_ko[1] = WIFI_DRIVERS_PATH "/mac80211.ko";
    }

    for (s32 idx = 0; idx < ARRSIZE(insmod_ko); idx++) {
        if (NULL == insmod_ko[idx])
            continue;

        if (0
            == access(insmod_ko[idx],
                      F_OK)) // Compatible with other chip platforms that did not interface with atbm6062, the driver is loaded only if it exists
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

    // Lock the MAC address before notifying the outside, so that the outside can customize the MAC address
    _lock_net_device_mac(WLAN_DEV);

    ipc_wireless_module_after_insmod_driver_notify(WIRELESS_MODULE_WIFI, WLAN_DEV);

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

static s32 _udhcpc_start(pv8 dev, s32 timeout_ts)
{
    pv8 extern_args = "";
    if (access(UDHCPC_SCRIPT, F_OK | X_OK) == 0) {
        extern_args = "-s " UDHCPC_SCRIPT;
    }

    // Avoid udhcpc process leaks
    _udhcpc_stop(dev);

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
    // DHCP renewal
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
    pv8 segment = ip;

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
    ret         = ipc_unix_socket_recv(h_sock, buf, sizeof(buf), 2 * 1000);
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
    " sae_password=\"%s\"\n"                                                                                                                         \
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

    fprintf(fp, "country=%s\n", country_code && country_code[0] ? country_code : "CN");

    if (pwd && pwd[0]) {

        if (strlen(pwd) >= 8) {
            v8 psk[128] = { 0 };
            ipc_wpa_get_password_psk(ssid, pwd, psk, sizeof(psk));
            // coverity[SENSITIVE_DATA_LEAK :SUPPRESS]
            fprintf(fp, STA_WPA3_CONF_FORMAT, ssid_hex, pwd);
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
    s32 ret = 0;
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
        _gh_net.sta_lock = 1; // Locked
        ret              = _wifi_sta_connect(ssid, pwd, country_code, timeout);
        _gh_net.sta_lock = 0;
        if (ret < 0) {
            _gh_net.f_event(ret == IPC_VERIFY_FAILED ? IPC_NET_EVENT_STA_PASSWORD_ERROR : IPC_NET_EVENT_STA_CONNECT_FAILED);
            return ret;
        }
        _gh_net.f_event(IPC_NET_EVENT_STA_CONNECT_SUCCESS);
    }

    // Leave the network configuration information after successful setting for easy reconnection
    snprintf(_gh_net.country_code, sizeof(_gh_net.country_code), "%s", country_code ?: "");
    snprintf(_gh_net.pwd, sizeof(_gh_net.pwd), "%s", pwd ?: "");
    snprintf(_gh_net.ssid, sizeof(_gh_net.ssid), "%s", ssid);

    return ret;
}

void ipc_wifi_sta_disconnect(void)
{
    _gh_net.sta_gorun = 0;

    // First kill to interrupt the connection that is halfway through
    _udhcpc_stop(WLAN_DEV);

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

    _wpa_cmd(h_sock, "DETACH");

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
    if (ret < 0)
        goto FAILED;

    ret = _udhcpd_start(ex);
    if (ret < 0)
        goto FAILED;

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
            _gh_net.f_event(IPC_NET_EVENT_WIRED_CONNECT);
            _gh_net.f_signal(WIRED_MODULE_ETH, 0);
            break;
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
    ipc_mpp_eth_rst_io_ctrl();
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
        _gh_net.f_event(IPC_NET_EVENT_STA_CONNECT_FAILED);

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
        return IPC_EXIST; // Prevent re-entry or the station is alive, no need to reconnect or being locked

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

    /* Initialize, the default network port is not plugged in */
    u8 now_stat  = 0;
    u8 last_stat = 0;
    _ip_clear(WIRED_DEV);
    _ifconfig(WIRED_DEV, 0);
    _udhcpc_stop(WIRED_DEV);

    while (_gh_net.gorun) {

        if (!_gh_net.has_wifi && cnt < 10) { /* Check a maximum of 10 times */
            ret = _check_usb_module_type();
            if (ret < 0) {
                cnt++;
                if (cnt >= 10) {
                    ipcwarn("Check wifi dost not exist!");
                }
            } else {
                _insmod_wifi(ret);
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
                    _sta_reconn_uninit();
                }
                // Network port MAC address lock can only be placed here, otherwise it will cause delay in detecting external WIFI module, which may
                // lead to BUG
                _lock_net_device_mac(WIRED_DEV);

                // Wait for the network port to change the MAC address and be ready, you must wait here, not in the outer loop, otherwise it will
                // cause the network cable to be plugged in and start, and connect to WiFi once first
                _wired_wait_to_be_ready(wired_fd);

                _wired_connect();
            } else {
                _wired_disconnect();
            }
        }

        if (!_gh_net.smart_switch || !now_stat) { // Not in smart mode or in smart mode, but not connected to the network cable
            _sta_reconn_init();                   // Continuously poll to see if station networking is required
        }

        if (_gh_net.sta_need_to_reconn) {
            _sta_reconn_uninit();
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

/************************************************** search *******************************************/

static inline pv8 _strstr(pv8* src, pv8 sub)
{
    pv8 cur = strstr(*src, sub);
    return cur ? *src = cur : NULL;
}

#define IS_FIELD(cur, key, rule, val) (_strstr(&cur, key) && sscanf(cur, key rule, val)) // coverity[SECURE_CODING :SUPPRESS]

static s32 _wifi_search(ipc_wifi_info_p info, s32 max, pv8 node)
{
    _ifconfig(node, 1);

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

        if (IS_FIELD(cur, "ESSID", "%*[^\"]\"%[^\r\n]", tmpctx)) {
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

    s32 stat = pclose(fp);
    if (stat == -1 || stat == 127)
        return IPC_FAILED;
    if (!WIFEXITED(stat) || WEXITSTATUS(stat))
        return IPC_FAILED;
    return idx + 1;
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
        if (strstr(_g_wifi_map[_gh_net.wifi_module].driver_name, "atbm")) {
            v8 _path[64] = { 0 };
            // coverity[DC.STRING_BUFFER :SUPPRESS]
            // coverity[SECURE_CODING :SUPPRESS]
            sprintf(_path, "/sys/module/%s/atbmfs/atbm_sys", _g_wifi_map[_gh_net.wifi_module].driver_name);
            FILE* fp = fopen(_path, "r");
            if (fp != NULL) {
                v8 buf[64] = { 0 };
                while (fgets(buf, sizeof(buf), fp) != NULL) {
                    if (strstr(buf, "CHIP NAME") != NULL) {
                        sscanf(buf, "%*[^[][%[^]]", _g_wireless_info.name);
                    }
                }
                fclose(fp);
            }
        } else {
            // coverity[DC.STRING_BUFFER :SUPPRESS]
            sprintf(_g_wireless_info.name, "%s", _g_wifi_map[_gh_net.wifi_module].module_name);
        }
    }

    _g_wireless_info.support_ble = _g_wifi_map[_gh_net.wifi_module].support_ble;

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