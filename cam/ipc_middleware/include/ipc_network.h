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
    v8 bssid[18];        ///< ap bssid, mac address
    v8 ssid[129];        ///< ap name, empty string "" if ssid is hidden
    s32 channel;         ///< channel of this ap
    f32 frequency;       ///< channel frequency in Ghz
    s32 signal_dbm;      ///< signal strength of this ap
    s32 signal_pct;      ///< signal percentage of this ap
    ipc_wifi_mode_e mode; ///< connection mode supported by this ap
} ipc_wifi_info_t, *ipc_wifi_info_p;

typedef struct {
    v8 iccid[21];   ///< sim card ICCID
    v8 imei[16];    ///< sim card IMEI
    v8 imsi[16];    ///< sim card IMSI
    v8 version[64]; ///< module version
} ipc_4g_info_t, *ipc_4g_info_p;

typedef struct {
    v8 type[8];     ///< wifi or 4G
    v8 name[32];    ///< wireless module lsusb name
    v8 version[64]; ///< wireless module version
    v8 support_ble; ///< bluetooth support 0:not supported, 1:supported
    v8 wifi_band;   ///< wifi frequency band info 0:not read 1:single band(2.4G) 2:dual band(2.4G+5G)
} ipc_wireless_info_t;

typedef enum {
    IPC_NET_EVENT_WIRED_CONNECT,       ///< ethernet cable plugged in
    IPC_NET_EVENT_WIRED_DISCONNECT,    ///< ethernet cable unplugged
    IPC_NET_WIFI_INIT_SUCCESS,         ///< wifi module detected and driver loaded
    IPC_NET_EVENT_STA_CONNECT_SUCCESS, ///< station connection successful (triggered only by ethernet cable plug/unplug)
    IPC_NET_EVENT_STA_CONNECT_FAILED,  ///< station connection failed (triggered only by ethernet cable plug/unplug, will retry continuously, may
                                      ///< trigger multiple times)
    IPC_NET_EVENT_STA_PASSWORD_ERROR,  ///< wifi password error
    IPC_NET_EVENT_STA_DISCONNECT,      ///< router disconnected (ping failed or wpa router disconnected)
    IPC_NET_EVENT_STA_RECONNECT,       ///< reconnect to router
    IPC_NET_EVENT_INSMODE_WIFI_DRIVER, ///< notification after wifi driver loaded
    IPC_NET_EVENT_4G_CONNECT_SUCCESS,  ///< 4G network connection successful
    IPC_NET_EVENT_4G_CONNECT_FAILED,   ///< 4G network connection failed
    IPC_NET_EVENT_4G_RECONNECT,        ///< 4G network reconnection
    IPC_NET_EVENT_4G_HAS_SIM_CARD,     ///< 4G module detected SIM card
    IPC_NET_EVENT_4G_NO_SIM_CARD,      ///< 4G module did not detect SIM card
    IPC_NET_EVENT_STA_BEFORE_CONNECTED, ///< event reported before station connection
} ipc_net_event_e;

typedef enum {
    WIRELESS_MODULE_WIFI,
    WIRELESS_MODULE_4G,
    WIRED_MODULE_ETH,
} ipc_wireless_module_e;

typedef void (*ipc_net_event_f)(ipc_net_event_e event);

/**
 * @brief Wireless module signal strength callback registration
 *
 * @param module wireless module ipc_wireless_module_e
 * @param rssi signal strength
 */
typedef void (*ipc_wireless_signal_f)(ipc_wireless_module_e module, s32 rssi);

/**
 * @brief Initialize network module
 *
 * @param smart_switch smart disconnect wifi station when ethernet cable plugged/unplugged
 * @param f_event      event callback
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_net_init(u8 smart_switch, ipc_net_event_f f_event);

/**
 * @brief Destroy network module (only thread exit and resource release, will not uninstall driver or disconnect network)
 *
 * @param is_wait 1: block and wait for thread exit 0: non-block notification for thread exit
 */
EXAPI void ipc_net_uninit(u8 is_wait);

EXAPI void ipc_net_rmmod_all_driver(void);

/**
 * @brief Enable ipv6 network support
 *
 */
EXAPI void ipc_net_enable_ipv6(void);

/**
 * @brief Save static ip address, only effective for wlan0 and eth0 network cards
 *
 * @return
 */
EXAPI void ipc_net_save_static_ip(void);

/**
 * @brief Remove static ip address
 *
 * @return
 */
EXAPI void ipc_net_remove_static_ip(void);

/**
 * @brief Establish station connection to router
 *
 * @param ssid router wifi name, if NULL or '\0', clear internal recorded ssid
 * @param pwd  router wifi password, if NULL or "\0", connect with empty password
 * @param country_code  country code, if NULL or "\0", default to CN
 * @param timeout network connection timeout in seconds, if 0, set to internal thread for continuous reconnection
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_sta_connect(pv8 ssid, pv8 pwd, pv8 country_code, u32 timeout);

/**
 * @brief Disconnect wifi connection, destroy station resources
 *
 */
EXAPI void ipc_wifi_sta_disconnect(void);

/**
 * @brief Set hostname sent by udhcpc to router (default to Smart_Camera if not set)
 *
 * @param hostname device name displayed on router (string with '\0' max 32 bytes)
 */
EXAPI void ipc_wifi_sta_set_hostname(pv8 hostname);

/**
 * @brief Get WiFi signal strength
 * @param rssi signal strength
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_get_rssi(ps32 rssi);

typedef struct {
    pv8 ip;
    pv8 mask;
} ipc_wifi_ap_ex_t, *ipc_wifi_ap_ex_p;

/**
 * @brief Create ap hotspot
 *
 * @param ssid ap hotspot name
 * @param pwd  ap hotspot password, if NULL or "\0", then empty password
 * @param chn  ap hotspot channel selection
 * @param info ap additional info settings, ip, mask... if NULL, default ip:192.168.1.1 mask:255.255.255.0
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_wifi_ap_create(pv8 ssid, pv8 pwd, u8 chn, ipc_wifi_ap_ex_p ex);
#define ipc_wifi_ap_build(...) ipc_wifi_ap_create(__VA_ARGS__, NULL)

/**
 * @brief Destroy ap hotspot resources
 */
EXAPI void ipc_wifi_ap_destroy(void);

/**
 * @brief wifi search, temporarily only effective under ipc_wifi_sta_connect state
 *
 * @param info list of surrounding ap info to be searched and retrieved
 * @param num  maximum number of info
 * @return number of aps found, limited by num
 */
EXAPI s32 ipc_wifi_search(ipc_wifi_info_p info, s32 max);

/**
 * @brief Signal strength callback registration
 *
 * @param[in] f_wireless_signal event callback
 */
EXAPI void ipc_wireless_signal_cb(ipc_wireless_signal_f f_wireless_signal);

/**
 * @brief Get 4G module related information
 *
 * @param[out] info ipc_4g_info_p
 * @return success: IPC_SUCCESS / failure: gv error code
 */
EXAPI s32 ipc_4g_module_info_get(ipc_4g_info_p info);

/**
 * @brief Notify driver loading
 * @param module WiFi module type
 * @param dev_name network card node name
 */

EXAPI __WEAK void ipc_wireless_module_after_insmod_driver_notify(ipc_wireless_module_e module, pv8 dev_name);

/**
 * @brief Get wireless module related information
 *
 * @return ipc_wireless_info_t
 */
EXAPI ipc_wireless_info_t* ipc_wireless_info_get(void);

/**
 * @brief Notify ipc_ipc whether connected to cloud platform, used for wireless module to determine if working normally
 *
 * @param is_connected whether cloud platform is connected to network
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_net_notify_cloud_connect_status(u8 is_connected);

#ifdef __cplusplus
}
#endif

#endif //__IPC_NETWORK_H__
