#ifndef __IV_SDK_LOG_HEADER_FILE__
#define __IV_SDK_LOG_HEADER_FILE__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IVLOG_LEVEL_VERBOSE,
    IVLOG_LEVEL_DEBUG,
    IVLOG_LEVEL_NOTICE,
    IVLOG_LEVEL_CONTROL,
    IVLOG_LEVEL_INFO,
    IVLOG_LEVEL_WARNING,
    IVLOG_LEVEL_ERROR,
 	IVLOG_LEVEL_MAX,
} IVLogLevel;
    
#if 0

/**
 * @brief Set the log level.
 *
 * @param level The desired log level.
 */
void iv_sdk_SetLogLevel(const LogLevel level);

/**
 * @brief Enable or disable logging.
 *
 * @param enabled Set to 1 to enable logging, 0 to disable.
 */
void iv_sdk_SetLogEnabled(const int enabled);

/**
 * @brief Set the directory path for log files.
 *
 * @param directory The directory path for log files.
 */
void iv_sdk_SetLogPath(const char* path);
#endif


/**
 * @brief Write a log message with the specified log level and format.
 *
 * @param level The log level of the message.
 * @param format The format string of the log message.
 * @param ... The additional arguments to be formatted and logged.
 */
void iv_sdk_LogWriteFun(const char* fun_name, int line,  IVLogLevel level, const char* format, ...);

    
#define iv_sdk_LogWrite(level, format, ...)  iv_sdk_LogWriteFun(__FUNCTION__, __LINE__, level, format, ##__VA_ARGS__)


/**
 * @brief Set serial output flag file path
 *
 * @param const char* path 
 */
void iv_sdk_SetSerialOutputFlagPath(const char* path);


/*
    * @brief Flush the log file
    *
    * @return 0 on success, -1 on failure
    when catch segmenation fault, call this function to flush the log file to avoid log loss

*/
int iv_sdk_Logfsync(void);


#ifdef __cplusplus
}
#endif

#endif