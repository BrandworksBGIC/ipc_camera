#ifndef __RTS_CRYPTO_LOG_H__
#define __RTS_CRYPTO_LOG_H__

enum RTS_CRYPTO_LOG_LEVEL {
	CRYPTO_LOG_NONE = -1,
	CRYPTO_LOG_ERROR = 0,
	CRYPTO_LOG_WARN,
	CRYPTO_LOG_DEBUG,
	CRYPTO_LOG_INFO,
};

extern enum RTS_CRYPTO_LOG_LEVEL crypto_log_level;

#define rts_crypto_log(level, prefix, fmt, args...) \
	do { \
		if (level <= crypto_log_level) \
			fprintf(stdout, "%s [%s:%d]: "fmt, \
				prefix, __func__, __LINE__, ##args);\
	} while (0)

#define rts_crypto_info(fmt, args...) \
		rts_crypto_log(CRYPTO_LOG_INFO, "info", fmt, ##args)
#define rts_crypto_debug(fmt, args...) \
		rts_crypto_log(CRYPTO_LOG_DEBUG, "dbg", fmt, ##args)
#define rts_crypto_warn(fmt, args...) \
		rts_crypto_log(CRYPTO_LOG_WARN, "warn", fmt, ##args)
#define rts_crypto_error(fmt, args...) \
		rts_crypto_log(CRYPTO_LOG_ERROR, "err", fmt, ##args)


#endif
