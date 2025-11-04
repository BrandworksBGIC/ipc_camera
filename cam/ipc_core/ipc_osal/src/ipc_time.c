#include "ipc_time.h"
#include "ipc_json.h"
#include "ipc_misc.h"
#include <time.h>
#include <unistd.h>

#define SESSION "time"
static s32 _g_zone              = 5 * 3600 + 3600 / 2;
static s32 _g_dts               = 0;
static ipc_json_t _g_zone_conf[] = {
    json_int("zone", _g_zone),
};
static ipc_json_t _g_dts_conf[] = {
    json_int("dts", _g_dts),
};

void ipc_set_zone(s32 zone)
{
    _g_zone = zone;
    ipc_json_wrconf(SESSION, _g_zone_conf, ARRSIZE(_g_zone_conf));
}

s32 ipc_get_zone(void)
{
    static u8 first = 1;
    if (first) {
        first = 0;
        ipc_json_rdconf(SESSION, _g_zone_conf, ARRSIZE(_g_zone_conf));
    }
    return _g_zone;
}

void ipc_set_dts(s32 dts)
{
    _g_dts = dts;
    ipc_json_wrconf(SESSION, _g_dts_conf, ARRSIZE(_g_dts_conf));
}

s32 ipc_get_dts(void)
{
    static u8 first = 1;
    if (first) {
        first = 0;
        ipc_json_rdconf(SESSION, _g_dts_conf, ARRSIZE(_g_dts_conf));
    }
    return _g_dts;
}

u32 ipc_mono_ts(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec;
}

u64 ipc_mono_tms(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000LL + time.tv_nsec / (1000LL * 1000LL);
}

u32 ipc_utc0_ts(void)
{
    return time(NULL);
}

u64 ipc_utc0_tms(void)
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time); // The system time zone is 0
    return time.tv_sec * 1000LL + time.tv_nsec / (1000LL * 1000LL);
}

u32 ipc_real_ts(void)
{
    return ipc_utc0_ts() + ipc_get_zone() + ipc_get_dts();
}

u64 ipc_real_tms(void)
{
    return ipc_utc0_tms() + (ipc_get_zone() + ipc_get_dts()) * 1000LL;
}

void ipc_set_ts(u32 utc0_ts)
{
    struct timespec time = {
        .tv_sec = utc0_ts,
    };
    clock_settime(CLOCK_REALTIME, &time); // The system time zone is 0
}

void ipc_set_tms(u64 utc0_tms)
{
    struct timespec time = {
        .tv_sec  = utc0_tms / 1000LL,
        .tv_nsec = (utc0_tms % 1000LL) * 1000LL * 1000LL,
    };
    clock_settime(CLOCK_REALTIME, &time); // The system time zone is 0
}

void ipc_sleep(u32 ts)
{
    if (!ts)
        return;
    sleep(ts);
}

void ipc_msleep(u32 tms)
{
    if (tms >= 1000) {
        ipc_sleep(tms / 1000);
        tms %= 1000;
    }
    if (!tms)
        return;
    usleep(tms * 1000);
}

void ipc_ts2date(u32 time, ipc_date_tm_p date)
{
    if (!date)
        return;
    time_t time_ts = time;
    struct tm time_tm;
    gmtime_r(&time_ts, &time_tm);
    date->year = time_tm.tm_year + 1900;
    date->mon  = time_tm.tm_mon + 1;
    date->day  = time_tm.tm_mday;
    date->hour = time_tm.tm_hour;
    date->min  = time_tm.tm_min;
    date->sec  = time_tm.tm_sec;
}

u32 ipc_date2ts(ipc_date_tm_p date)
{
    if (!date)
        return 0;
    s32 year = date->year;
    s32 mon  = date->mon - 2;
    if (mon <= 0) {
        mon += 12;
        year -= 1;
    }
    return ((((u32)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + date->day) + year * 365 - 719499) * 24
             + date->hour /* now have hours */
             ) * 60
            + date->min /* now have minutes */
            ) * 60
           + date->sec; /* finally seconds */
}

static u8 _is_number(pv8 str)
{
    for (s32 idx = 0; str[idx]; idx++) {
        if (str[idx] < '0' || str[idx] > '9')
            return 0;
    }
    return 1;
}

u32 ipc_ts2str(pv8 buf, s32 max, pcv8 format, u32 ts)
{
    time_t t = ts;
    struct tm st;
    gmtime_r(&t, &st);
    memset(buf, 0, max);

    v8 tmp_buff[32];
    v8 strftime_format[64];
    v8 snprintf_format[64];
    s32 strftime_len = 0;
    s32 snprintf_len = 0;

    s32 len        = 0;
    u8 has_pct     = 0;
    u8 pct_num     = 0;
    s32 format_max = strlen(format);

    for (s32 idx = 0; idx < format_max; idx++) {

        if (format[idx] == '%') {
            has_pct = 1;
            pct_num++;
            if (pct_num == 2) {
                has_pct                         = 0;
                pct_num                         = 0; //%% is the escape character for %
                strftime_format[strftime_len++] = '%';
                strftime_format[strftime_len++] = '%';
            }
            continue; // Skip '%' first, then compensate later
        }

        if (!has_pct) { // Normal symbols, assign directly
            strftime_format[strftime_len++] = format[idx];
            continue;
        }

        // There is a '%' in front -> %m %0.1m %-2m...
        if ((format[idx] >= '0' && format[idx] <= '9') || format[idx] == '.' || format[idx] == '-'
            || format[idx] == '+') { // Use snprintf
            if (strftime_len) {      // There is normal strftime format data in front
                strftime_format[strftime_len] = '\0';
                len += strftime(buf + len, max - len, strftime_format, &st);
                if (len >= max)
                    return max;
                strftime_len = 0;
            }
            if (pct_num) {
                pct_num                         = 0;
                snprintf_format[snprintf_len++] = '%'; // Recover '%'
            }
            snprintf_format[snprintf_len++]
                = format[idx]; // Continuously obtain characters belonging to the snprintf format -> %0.1 %-2, etc.
            continue;
        }

        // The first non-character after %
        if (snprintf_len) { // There are snprintf pattern characters
            strftime_format[0] = '%';
            strftime_format[1] = format[idx];
            strftime_format[2] = '\0';
            memset(tmp_buff, 0, sizeof(tmp_buff));
            strftime(tmp_buff, sizeof(tmp_buff), strftime_format, &st);
            if (_is_number(tmp_buff)) {                // Make sure it is a number
                snprintf_format[snprintf_len++] = 'd'; // %-x.xd
                snprintf_format[snprintf_len]   = '\0';
                // coverity[PW.NON_CONST_PRINTF_FORMAT_STRING :SUPPRESS]
                len += snprintf(buf + len, max - len, snprintf_format, atoi(tmp_buff));
                if (len >= max)
                    return max;
            } else {
                snprintf_format[snprintf_len++] = 's'; // %-x.xs
                snprintf_format[snprintf_len]   = '\0';
                // coverity[PW.NON_CONST_PRINTF_FORMAT_STRING :SUPPRESS]
                len += snprintf(buf + len, max - len, snprintf_format, tmp_buff);
                if (len >= max)
                    return max;
            }
            snprintf_len = 0;
        } else {
            if (pct_num) {
                pct_num                         = 0;
                strftime_format[strftime_len++] = '%'; // Recover '%'
            }
            strftime_format[strftime_len++] = format[idx];
        }
        has_pct = 0;
    }

    if (strftime_len) { // There is normal strftime format data in front
        strftime_format[strftime_len] = '\0';
        len += strftime(buf + len, max - len, strftime_format, &st);
        if (len >= max)
            return max;
    }
    return len;
}
