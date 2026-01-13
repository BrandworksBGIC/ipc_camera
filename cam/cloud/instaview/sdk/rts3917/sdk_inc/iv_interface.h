#ifndef __IV_SDK_INTERFACE_HEADER_FILE__
#define __IV_SDK_INTERFACE_HEADER_FILE__

#ifdef __cplusplus
extern "C" {
#endif
#include "iv_callback_type.h"
#include "iv_log.h"
	/*
	* @param[in]  MFG_CALL_BACK_FUNCTIONS*   callback functions
	* @return     int                         0: success, -1: failed
	* @note       must call this function before iv_sdk_init()
	*
	*/
	int iv_sdk_register_callback_functions(MFG_CALLBACK_FUNCTIONS*);

    /*
    * @param[out] version  string buffer
    * @param[in]  len      string buffer length
    * @return     int  
    */
    int iv_sdk_get_version(char* version, int len);

    /*
    * @param[out] device_id     device_id buffer
    * @param[in]  size          device_id size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_device_id(char* device_id, int size);

    /*
    * @param[out] space_id      space_id buffer
    * @param[in]  size          space_id size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_space_id(char* space_id, int size);

    /*
    * @param[out] partner_id    partner_id buffer
    * @param[in]  size          partner_id size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_partner_id(char* partner_id, int size);
    
    /*
    * @param[out] mcu_access_token  mcu_access_token buffer
    * @param[in]  size              mcu_access_token size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_mcu_access_token(char* mcu_access_token, int size);
    
    /*
    * @param[out] mcu_mqtt_url  mcu_mqtt_url buffer
    * @param[in]  size          mcu_mqtt_url size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_mcu_mqtt_url(char* mcu_mqtt_url, int size);

    /*
    * @param[out] client_id     client_id buffer
    * @param[in]  size          client_id size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_client_id(char* client_id, int size);

    /*
    * @param[out] thing_name    thing_name buffer
    * @param[in]  size          thing_name size
    * @return     int           0:success, -1:failed
    */
    int iv_sdk_get_thing_name(char* thing_name, int size);

	/*
	* @param[in]  set cfg path for home security,default is "/config"
	*/
	void iv_sdk_set_cfg_path(const char* path);	

	/*
	* @param[in]  set sd card mount path
	*/
    void iv_sdk_set_sdcard_mount_path(const char* path);

	/*
	* @param[in]  set ota version
	*/
    void iv_sdk_set_ota_version(const char* version);


    /*
    * @param[in]  const char *  mcu version
    * @return     void
    */
    void iv_sdk_set_mcu_version(const char *version);

	/*
    * @param[in]  const char *  certified firmware version
    * @return     void
    */
    void iv_sdk_set_certified_firmware_version(const char *version);
	
	/*
	must call above function,when you call iv_sdk_init function
	* @return     int                         0: success, -1: failed
	* @note       
	* 		   must call iv_sdk_deinit() when exit
	* 		   must call iv_sdk_register_callback_functions() before iv_sdk_init()
	*/
	int iv_sdk_init();

	/*
	* @return     int                         0: success, -1: failed
	* @note       must call this function when exit
	*
	*/
	int iv_sdk_deinit();

	
	/**
	 * @brief Configures Wi-Fi settings for the camera over Bluetooth Low Energy (BLE).
	 * 
	 * This function allows the configuration of the camera's Wi-Fi settings using BLE. It takes in
	 * parameters for the Wi-Fi SSID, password, session key from the IV app, server environment type, 
	 * and the region. These settings are necessary for connecting the camera to a Wi-Fi network and 
	 * ensuring it communicates with the correct server.
	 * 
	 * @param ssid
	 *        The SSID of the Wi-Fi network to which the camera should connect.
	 *        Type: const char*
	 *        UUID: 0x12345678-1234-5678-1234-56789abcdef5
	 *        Properties: Read/Write
	 * 
	 * @param pwd
	 *        The password for the Wi-Fi network.
	 *        Type: const char*
	 *        UUID: 0x12345678-1234-5678-1234-56789abcdef6
	 *        Properties: Read/Write
	 * 
	 * @param session_key
	 *        The session key provided by the IV app for secure communication.
	 *        Type: const char*
	 *        UUID: 0x12345678-1234-5678-1234-56789abcdef7
	 *        Properties: Write
	 * 
	 * @param env
	 *        The server environment type, which can be development, staging, or production.
	 *        Type: const char*
	 *        UUID: 0x12345678-1234-5678-1234-56789abcdef8
	 *        Properties: Read/Write
	 * 
	 * @param region
	 *        The region in which the camera is being used. This can be either US or HK.
	 *        Type: const char*
	 *        UUID: 0x12345678-1234-5678-1234-56789abcdef9
	 *        Properties: Read/Write
	 * 
	 * @return 
	 *        Returns 0 on success, or a negative error code on failure.
	 * 
	 * This function should be used by OEMs to configure the Wi-Fi settings on the camera during the 
	 * initial setup or when the Wi-Fi settings need to be changed. The parameters should be provided 
	 * as strings and should be valid and within the expected length limits for each field.
	 */
	int iv_sdk_config_wifi_by_ble(const char* ssid, const char* pwd, const char* session_key, const char* env, const char* region);

	/*
	* @param[in]  set video encode type: h264/h265
	*/
    void iv_sdk_set_venc_type(E_MEDIA_VIDEO_ENC_TYPE venc_type);
    /*
    * @param[in]  int          0: disable, 1: enable
    * @note       must call this function before iv_sdk_init()
    */
    void iv_sdk_set_substream(int enable);

    /*
    * @param[in]  int          0: unknow, 1: remote, 2: PIR, 3: button
    * @return     int          0:success, -1:failed
    */
    int iv_sdk_set_wakeup_method(int wakeup_method);
#if 0
	/*
	* @brief  lowpower device reset
	*/
    void iv_sdk_lowpowr_reset();
    
	/*
	* @brief  lowpower device power off
	*/
    void iv_sdk_lowpowr_power_off();
    
    /*
	* @brief    doorbell pressed event
    */
    void iv_sdk_doorbell_pressed();
#endif

    /*
	* @brief      button event
    * @param[in]  int     0: NOTFOUND, 
                          1: RESET, 
                          2: POWER_OFF, 
                          3: DOORBELL
    * @return
    */
    void iv_sdk_button_event(E_BUTTON_TYPE button_type);

	/**
	 * @brief Set the log level.
	 *
	 * @param level The desired log level.
	 */
	void iv_sdk_set_logLevel(const IVLogLevel level);

	/**
	 * @brief reset the iv config and play reset audio
	 *
	 */
	int iv_sdk_reset();

	/*if wakeup is remote, can run webrtc first*/
	int iv_sdk_webrtc_start();

	/*set pir md filter result: 0 no motion, 1:has motion*/
	int iv_sdk_set_pir_md_filter_result(int result);

	/*set pir human filter result: 0 no human, 1:has human*/
	int iv_sdk_set_pir_human_filter_result(int result);

	/*set low battery shutdown:0 normal, 1:camera will shutdown*/
	int iv_sdk_set_low_battery_shutdown(int value);
    
#ifdef DEBUG_BACKSTRACE
	/*
	* for signal backstrace
	*/
	void iv_sdk_signal_register();
#endif

	/*
	* @param[in]  set event type (doorbell/video call)
	*/
    void iv_sdk_set_event_type(E_IV_EVENT_TYPE event_type);

	/**
	* Gets the number of seconds of local time (calculated from the Unix epoch)
	* Returns: The number of seconds of local time, of type uint64_t
	*/
	uint64_t iv_sdk_get_local_time(void);
#ifdef __cplusplus
}
#endif


#endif // !__IV_SDK_INTERFACE_HEADER_FILE__
