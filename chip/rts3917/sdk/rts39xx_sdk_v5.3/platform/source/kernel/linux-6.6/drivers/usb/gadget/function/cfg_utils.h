#include <linux/configfs.h>

#define USB_MAX_STRING_LEN_WITH_NULL (USB_MAX_STRING_LEN + 1)

#define F_STR_ATTR(fname, oname, cname)				\
static ssize_t f_##fname##_##cname##_show(struct config_item *item,\
					char *page)		\
{								\
	struct oname *opts = to_##oname(item);			\
	int ret;						\
								\
	mutex_lock(&opts->lock);				\
	ret = sprintf(page, "%s\n", opts->cname ?: "");		\
	mutex_unlock(&opts->lock);				\
								\
	return ret;						\
}								\
								\
static ssize_t f_##fname##_##cname##_store(struct config_item *item,	\
			const char *page, size_t len)		\
{								\
	struct oname *opts = to_##oname(item);			\
	int ret;						\
	char *str;						\
	char *copy = opts->cname;				\
								\
	mutex_lock(&opts->lock);				\
	if (opts->refcnt) {					\
		ret = -EBUSY;					\
		goto end;					\
	}							\
								\
	ret = strlen(page);					\
	if (ret > USB_MAX_STRING_LEN) {				\
		ret = -EOVERFLOW;				\
		goto end;					\
	}							\
								\
	if (copy) {						\
		str = copy;					\
	} else {						\
		str = kmalloc(USB_MAX_STRING_LEN_WITH_NULL,	\
				GFP_KERNEL);			\
		if (!str) {					\
			ret = -ENOMEM;				\
			goto end;				\
		}						\
	}							\
	strcpy(str, page);					\
	if (str[ret - 1] == '\n')				\
		str[ret - 1] = '\0';				\
	opts->cname = str;					\
	ret = len;						\
end:								\
	mutex_unlock(&opts->lock);				\
	return ret;						\
}								\
								\
CONFIGFS_ATTR(f_##fname##_, cname)
