#ifndef __IPC_NETWORK_H__
#define __IPC_NETWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

typedef enum {
    IPC_WIFI_MODE_OPEN     = 0,
    IPC_WIFI_MODE_WPA      = 1 << 0,
    IPC_WIFI_MODE_WPA2     = 1 << 1,
    IPC_WIFI_MODE_WPA3     = 1 << 2,
    IPC_WIFI_MODE_WPA_WPA2 = IPC_WIFI_MODE_WPA | IPC_WIFI_MODE_WPA2,
    IPC_WIFI_MODE_MAX,
} ipc_wifi_mode_e;

typedef struct {
    v8 bssid[18];        ///< AP BSSID, MAC address
    v8 ssid[129];        ///< AP name, if hidden SSID then ""
    s32 channel;         ///< Channel of this AP
    f32 frequency;       ///< Frequency of this AP in GHz
    s32 signal_dbm;      ///< Signal strength of this AP
    s32 signal_pct;      ///< Signal percentage of this AP
    ipc_wifi_mode_e mode; ///< Connection modes supported by this AP
} ipc_wifi_info_t, *ipc_wifi_info_p;

typedef struct {
    v8 iccid[21];   ///< SIM card ICCID
    v8 imei[16];    ///< SIM card IMEI
    v8 imsi[16];    ///< SIM card IMSI
    v8 version[64]; ///< Module version
} ipc_4g_info_t, *ipc_4g_info_p;

typedef struct {
    v8 type[8];     ///< wifi or 4G
    v8 name[32];    ///< Name of the wireless module lsusb
    v8 version[64]; ///< Version number of the wireless module
    v8 support_ble; ///< Whether Bluetooth is supported 0: Not supported, 1: Supported
} ipc_wireless_info_t;

typedef enum {
    IPC_NET_EVENT_WIRED_CONNECT,       ///< Ethernet cable connected
    IPC_NET_EVENT_WIRED_DISCONNECT,    ///< Ethernet cable disconnected
    IPC_NET_WIFI_INIT_SUCCESS,         ///< Detected wifi module and driver loaded successfully
    IPC_NET_EVENT_STA_CONNECT_SUCCESS, ///< Station connected successfully (triggered by plugging/unplugging Ethernet cable)
    IPC_NET_EVENT_STA_CONNECT_FAILED,  ///< Station connection failed (triggered by plugging/unplugging Ethernet cable, will attempt to reconnect, so
                                      ///< may be triggered multiple times)
    IPC_NET_EVENT_STA_PASSWORD_ERROR,  ///< Incorrect WiFi password
    IPC_NET_EVENT_STA_DISCONNECT,      ///< Router disconnected (ping fails or WPA confirms router disconnection)
    IPC_NET_EVENT_STA_RECONNECT,       ///< Reconnect to router
    IPC_NET_EVENT_INSMODE_WIFI_DRIVER, ///< Notification after loading wifi driver
    IPC_NET_EVENT_4G_CONNECT_SUCCESS,  ///< 4G network connected successfully
    IPC_NET_EVENT_4G_CONNECT_FAILED,   ///< 4G network connection failed
    IPC_NET_EVENT_4G_RECONNECT,        ///< 4G network reconnected
    IPC_NET_EVENT_4G_HAS_SIM_CARD,     ///< 4G module detects SIM card
    IPC_NET_EVENT_4G_NO_SIM_CARD       ///< 4G module does not detect SIM card
} ipc_net_event_e;

typedef enum {
    WIRELESS_MODULE_WIFI,
    WIRELESS_MODULE_4G,
    WIRED_MODULE_ETH,
} ipc_wireless_module_e;

typedef void (*ipc_net_event_f)(ipc_net_event_e event);

/**
 * @brief Wireless module signal strength callback time
 *
 * @param module Wireless module ipc_wireless_module_e
 * @param rssi Signal strength
 */
typedef void (*ipc_wireless_signal_f)(ipc_wireless_module_e module, s32 rssi);

/**
 * @brief Initialize network module
 *
 * @param smart_switch Smartly disconnect wifi station when Ethernet cable is plugged/unplugged
 * @param f_event Event callback
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_net_init(u8 smart_switch, ipc_net_event_f f_event);

/**
 * @brief Destroy network module (only exits thread and releases resources, does not unload drivers or disconnect network)
 *
 * @param is_wait 1: Block waiting for thread exit 0: Non-blocking notification to exit thread
 */
EXAPI void ipc_net_uninit(u8 is_wait);

/**
 * @brief Set whether IPv6 network is supported
 *
 */
EXAPI void ipc_net_enable_ipv6(void);

/**
 * @brief Save static IP address, effective only for wlan0 and eth0 interfaces
 *
 */
EXAPI void ipc_net_save_static_ip(void);

/**
 * @brief Remove static IP address
 *
 */
EXAPI void ipc_net_remove_static_ip(void);

/**
 * @brief Establish station connection to router
 *
 * @param ssid Router WiFi name, if NULL or '\0', clears internally stored SSID
 * @param pwd Router WiFi password, if NULL or "\0", connects with no password
 * @param country_code Country code, if NULL or "\0", defaults to CN
 * @param timeout Internet connection timeout (seconds), if 0, directly sets internal thread for continuous reconnection
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_sta_connect(pv8 ssid, pv8 pwd, pv8 country_code, u32 timeout);

/**
 * @brief Disconnect WiFi connection, destroy station resources
 *
 */
EXAPI void ipc_wifi_sta_disconnect(void);

/**
 * @brief Set hostname sent by udhcpc to the router (if not set, defaults to Smart_Camera)
 *
 * @param hostname Device name displayed on the router (string including '\0' maximum limit 32 bytes)
 */
EXAPI void ipc_wifi_sta_set_hostname(pv8 hostname);

/**
 * @brief Get WiFi signal strength
 * @param rssi Signal strength
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_get_rssi(ps32 rssi);

typedef struct {
    pv8 ip;
    pv8 mask;
} ipc_wifi_ap_ex_t, *ipc_wifi_ap_ex_p;

/**
 * @brief Establish AP hotspot
 *
 * @param ssid Hotspot name
 * @param pwd Hotspot password, if NULL or "\0", connects with no password
 * @param chn Hotspot channel selection
 * @param info Additional AP information settings, ip, mask... if NULL, defaults to ip:192.168.1.1 mask:255.255.255.0
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_ap_create(pv8 ssid, pv8 pwd, u8 chn, ipc_wifi_ap_ex_p ex);
#define ipc_wifi_ap_build(...) ipc_wifi_ap_create(__VA_ARGS__, NULL)

/**
 * @brief Destroy AP hotspot resources
 */
EXAPI void ipc_wifi_ap_destroy(void);

/**
 * @brief WiFi scan, currently only effective in ipc_wifi_sta_connect state
 *
 * @param info List of surrounding AP information to search and retrieve
 * @param num Maximum number of info
 * @return Number of APs found, limited by num
 */
EXAPI s32 ipc_wifi_search(ipc_wifi_info_p info, s32 max);

/**
 * @brief Register signal strength callback
 *
 * @param[in] f_wireless_signal Event callback
 */
EXAPI void ipc_wireless_signal_cb(ipc_wireless_signal_f f_wireless_signal);

/**
 * @brief Get 4G module related information
 *
 * @param[out] info ipc_4g_info_p
 * @return Success: IPC_SUCCESS / Failure: gv error code
 */
EXAPI s32 ipc_4g_module_info_get(ipc_4g_info_p info);

/**
 * @brief Notify driver load
 * @param module Type of WiFi module
 * @param dev_name Network interface node name
 */

EXAPI __WEAK void ipc_wireless_module_after_insmod_driver_notify(ipc_wireless_module_e module, pv8 dev_name);

/**
 * @brief Get wireless module related information
 *
 * @return ipc_wireless_info_t
 */
EXAPI ipc_wireless_info_t* ipc_wireless_info_get(void);

/**
 * @brief Notify ipc_middleware whether connected to cloud platform, used for wireless modules to determine if they are working properly
 *
 * @param is_connected Whether connected to the cloud platform
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_net_notify_cloud_connect_status(u8 is_connected);

#ifdef __cplusplus
}
#endif

#endif //__IPC_NETWORK_H__