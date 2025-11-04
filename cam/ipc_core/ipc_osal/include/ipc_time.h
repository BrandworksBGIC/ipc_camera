#ifndef __IPC_TIME_H__
#define __IPC_TIME_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

/**
 * @brief Set system timezone
 *
 * @param zone Second-level offset timezone time
 */
EXAPI void ipc_set_zone(s32 zone);

/**
 * @brief Get system timezone
 *
 * @return Second-level offset timezone time
 */
EXAPI s32 ipc_get_zone(void);

/**
 * @brief Set system daylight saving time
 *
 * @param dts Second-level offset daylight saving time
 */
EXAPI void ipc_set_dts(s32 dts);

/**
 * @brief Get system daylight saving time
 *
 * @return Second-level offset daylight saving time
 */
EXAPI s32 ipc_get_dts(void);

/**
 * @brief Get second-level monotonic increasing time (device uptime)
 *
 * @return Second-level monotonic increasing time
 */
EXAPI u32 ipc_mono_ts(void);

/**
 * @brief Get millisecond-level monotonic increasing time (device uptime)
 *
 * @return Millisecond-level monotonic increasing time
 */
EXAPI u64 ipc_mono_tms(void);

/**
 * @brief Get second-level system real-time (including daylight saving time and timezone offset)
 *
 * @return Second-level system real-time
 */
EXAPI u32 ipc_real_ts(void);

/**
 * @brief Get millisecond-level system real-time (including daylight saving time and timezone offset)
 *
 * @return Millisecond-level system real-time
 */
EXAPI u64 ipc_real_tms(void);

/**
 * @brief Get second-level UTC+0 timezone time
 *
 * @return Second-level UTC+0 timezone time
 */
EXAPI u32 ipc_utc0_ts(void);

/**
 * @brief Get millisecond-level UTC+0 timezone time
 *
 * @return Millisecond-level UTC+0 timezone time
 */
EXAPI u64 ipc_utc0_tms(void);

/**
 * @brief Set system time (second-level)
 *
 * @param utc0_ts UTC+0 timezone second-level time
 */
EXAPI void ipc_set_ts(u32 utc0_ts);

/**
 * @brief Set system time (millisecond-level)
 *
 * @param utc0_tms UTC+0 timezone millisecond-level time
 */
EXAPI void ipc_set_tms(u64 utc0_tms);

/**
 * @brief Thread sleep function
 *
 * @param ts Second-level time to sleep
 */
EXAPI void ipc_sleep(u32 ts);

/**
 * @brief Thread sleep function
 *
 * @param tms Millisecond-level time to sleep
 */
EXAPI void ipc_msleep(u32 tms);

typedef struct {
    s16 year;  ///< Year, e.g., 2021
    s8 mon;    ///< Month, 1-12
    s8 day;    ///< Day, 1-31
    s8 hour;   ///< Hour, 0-23
    s8 min;    ///< Minute, 0-59
    s8 sec;    ///< Second, 0-60
} ipc_date_tm_t, *ipc_date_tm_p;

/**
 * @brief Convert timestamp to year, month, day, hour, minute, second
 *
 * @param time Input second-level timestamp
 * @param date Output year, month, day, hour, minute, second structure
 * @note Pure conversion, without timezone
 */
EXAPI void ipc_ts2date(u32 time, ipc_date_tm_p date);

/**
 * @brief Convert year, month, day, hour, minute, second to timestamp
 *
 * @param date Input year, month, day, hour, minute, second structure
 * @return Converted second-level timestamp
 * @note Pure conversion, without timezone
 */
EXAPI u32 ipc_date2ts(ipc_date_tm_p date);

/**
 * @brief Convert timestamp to string, format reference to strftime function
 *
 */
EXAPI u32 ipc_ts2str(pv8 buff, s32 max, pcv8 format, u32 ts);

#ifdef __cplusplus
}
#endif

#endif // __IPC_TIME_H__