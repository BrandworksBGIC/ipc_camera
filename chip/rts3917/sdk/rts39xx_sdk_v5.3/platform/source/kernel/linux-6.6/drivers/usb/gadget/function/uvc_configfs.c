// SPDX-License-Identifier: GPL-2.0
/*
 * uvc_configfs.c
 *
 * Configfs support for the uvc function.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzejtp2010@gmail.com>
 */

#include "uvc_configfs.h"
#include "android_uvc.h"
#include "f_uvc.h"

#include <linux/sort.h>
#include <linux/usb/video.h>

/* -----------------------------------------------------------------------------
 * Global Utility Structures and Macros
 */
#define UVC_ATTR(prefix, cname, aname) \
static struct configfs_attribute prefix##attr_##cname = { \
	.ca_name	= __stringify(aname),				\
	.ca_mode	= S_IRUGO | S_IWUGO,				\
	.ca_owner	= THIS_MODULE,					\
	.show		= prefix##cname##_show,				\
	.store		= prefix##cname##_store,			\
}

#define UVC_ATTR_RO(prefix, cname, aname) \
static struct configfs_attribute prefix##attr_##cname = { \
	.ca_name	= __stringify(aname),				\
	.ca_mode	= S_IRUGO,					\
	.ca_owner	= THIS_MODULE,					\
	.show		= prefix##cname##_show,				\
}

#define le8_to_cpu(x)	(x)
#define cpu_to_le8(x)	(x)

static int uvcg_config_compare_u32(const void *l, const void *r)
{
	u32 li = *(const u32 *)l;
	u32 ri = *(const u32 *)r;

	return li < ri ? -1 : li == ri ? 0 : 1;
}

static inline int __uvcg_count_item_entries(char *buf, void *priv, unsigned int size)
{
	++*((int *)priv);
	return 0;
}

static inline int __uvcg_fill_item_entries(char *buf, void *priv, unsigned int size)
{
	unsigned int num;
	u8 **values;
	int ret;

	ret = kstrtouint(buf, 0, &num);
	if (ret)
		return ret;

	if (num != (num & GENMASK((size * 8) - 1, 0)))
		return -ERANGE;

	values = priv;
	memcpy(*values, &num, size);
	*values += size;

	return 0;
}

static int __uvcg_iter_item_entries(const char *page, size_t len,
				    int (*fun)(char *, void *, unsigned int),
				    void *priv, unsigned int size)
{
	/* sign, base 2 representation, newline, terminator */
	unsigned int bufsize = 1 + size * 8 + 1 + 1;
	const char *pg = page;
	int i, ret = 0;
	char *buf;

	if (!fun)
		return -EINVAL;

	buf = kzalloc(bufsize, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	while (pg - page < len) {
		i = 0;
		while (i < bufsize && (pg - page < len) &&
		       *pg != '\0' && *pg != '\n')
			buf[i++] = *pg++;
		if (i == bufsize) {
			ret = -EINVAL;
			goto out_free_buf;
		}
		while ((pg - page < len) && (*pg == '\0' || *pg == '\n'))
			++pg;
		buf[i] = '\0';
		ret = fun(buf, priv, size);
		if (ret)
			goto out_free_buf;
	}

out_free_buf:
	kfree(buf);
	return ret;
}

struct uvcg_config_group_type {
	struct config_item_type type;
	const char *name;
	const struct uvcg_config_group_type **children;
	int (*create_children)(struct config_group *group);
};

static void uvcg_config_item_release(struct config_item *item)
{
	struct config_group *group = to_config_group(item);

	kfree(group);
}

static struct configfs_item_operations uvcg_config_item_ops = {
	.release	= uvcg_config_item_release,
};

static int uvcg_config_create_group(struct config_group *parent,
				    const struct uvcg_config_group_type *type);

static int uvcg_config_create_children(struct config_group *group,
				const struct uvcg_config_group_type *type)
{
	const struct uvcg_config_group_type **child;
	int ret;

	if (type->create_children)
		return type->create_children(group);

	for (child = type->children; child && *child; ++child) {
		ret = uvcg_config_create_group(group, *child);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int uvcg_config_create_group(struct config_group *parent,
				    const struct uvcg_config_group_type *type)
{
	struct config_group *group;

	group = kzalloc(sizeof(*group), GFP_KERNEL);
	if (!group)
		return -ENOMEM;

	config_group_init_type_name(group, type->name, &type->type);
	configfs_add_default_group(group, parent);

	return uvcg_config_create_children(group, type);
}

static void uvcg_config_remove_children(struct config_group *group)
{
	struct config_group *child, *n;

	list_for_each_entry_safe(child, n, &group->default_groups, group_entry) {
		list_del(&child->group_entry);
		uvcg_config_remove_children(child);
		config_item_put(&child->cg_item);
	}
}

/* -----------------------------------------------------------------------------
 * control/header/<NAME>
 * control/header
 */

#define UVCG_CTRL_HDR_ATTR(cname, aname, bits, limit)			\
static ssize_t uvcg_control_header_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct uvcg_control_header *ch = to_uvcg_control_header(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &ch->item.ci_group->cg_subsys->su_mutex;\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = ch->item.ci_parent->ci_parent->ci_parent;		\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(ch->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t								\
uvcg_control_header_##cname##_store(struct config_item *item,		\
			   const char *page, size_t len)		\
{									\
	struct uvcg_control_header *ch = to_uvcg_control_header(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &ch->item.ci_group->cg_subsys->su_mutex;\
	int ret;							\
	u##bits num;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = ch->item.ci_parent->ci_parent->ci_parent;		\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	if (ch->linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtou##bits(page, 0, &num);				\
	if (ret)							\
		goto end;						\
									\
	if (num > limit) {						\
		ret = -EINVAL;						\
		goto end;						\
	}								\
	ch->desc.aname = cpu_to_le##bits(num);				\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_control_header_, cname, aname)

UVCG_CTRL_HDR_ATTR(bcd_uvc, bcdUVC, 16, 0xffff);

UVCG_CTRL_HDR_ATTR(dw_clock_frequency, dwClockFrequency, 32, 0x7fffffff);

#undef UVCG_CTRL_HDR_ATTR

static struct configfs_attribute *uvcg_control_header_attrs[] = {
	&uvcg_control_header_attr_bcd_uvc,
	&uvcg_control_header_attr_dw_clock_frequency,
	NULL,
};

static const struct config_item_type uvcg_control_header_type = {
	.ct_item_ops	= &uvcg_config_item_ops,
	.ct_attrs	= uvcg_control_header_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_item *uvcg_control_header_make(struct config_group *group,
						    const char *name)
{
	struct uvcg_control_header *h;

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	h->desc.bLength			= UVC_DT_HEADER_SIZE(1);
	h->desc.bDescriptorType		= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType	= UVC_VC_HEADER;
	h->desc.bcdUVC			= cpu_to_le16(0x0110);
	h->desc.dwClockFrequency	= cpu_to_le32(48000000);

	config_item_init_type_name(&h->item, name, &uvcg_control_header_type);

	return &h->item;
}

static struct configfs_group_operations uvcg_control_header_grp_ops = {
	.make_item		= uvcg_control_header_make,
};

static const struct uvcg_config_group_type uvcg_control_header_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_control_header_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "header",
};

/* -----------------------------------------------------------------------------
 * control/processing/default
 */

#define UVCG_DEFAULT_PROCESSING_ATTR(cname, aname, bits)		\
static ssize_t uvcg_default_processing_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct config_group *group = to_config_group(item);		\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvc_processing_unit_descriptor *pd;			\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
	pd = &opts->uvc_processing;					\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(pd->aname));	\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_default_processing_, cname, aname)

UVCG_DEFAULT_PROCESSING_ATTR(b_unit_id, bUnitID, 8);
UVCG_DEFAULT_PROCESSING_ATTR(b_source_id, bSourceID, 8);
UVCG_DEFAULT_PROCESSING_ATTR(i_processing, iProcessing, 8);

#undef UVCG_DEFAULT_PROCESSING_ATTR

static ssize_t uvcg_default_processing_w_max_multiplier_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_processing_unit_descriptor *pd;
	int result;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	pd = &opts->uvc_processing;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", le16_to_cpu(pd->wMaxMultiplier));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

static ssize_t uvcg_default_processing_w_max_multiplier_store(
		struct config_item *item,
		const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_processing_unit_descriptor *pd;
	int ret = -EINVAL;
	u16 num;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	pd = &opts->uvc_processing;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou16(page, 0, &num);
	if (ret)
		goto end;

	pd->wMaxMultiplier = cpu_to_le16(num);
	ret = len;
end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}
UVC_ATTR(uvcg_default_processing_, w_max_multiplier, wMaxMultiplier);

static ssize_t uvcg_default_processing_bm_controls_store(
	struct config_item *item, const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_processing_unit_descriptor *pd;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	u8 *bm_controls, *tmp;
	unsigned int i;
	int ret, n = 0;

	mutex_lock(su_mutex);

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	pd = &opts->uvc_processing;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n,
				       sizeof(u8));
	if (ret)
		goto unlock;

	if (n > pd->bControlSize) {
		ret = -EINVAL;
		goto unlock;
	}

	tmp = bm_controls = kcalloc(n, sizeof(u8), GFP_KERNEL);
	if (!bm_controls) {
		ret = -ENOMEM;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &tmp,
				       sizeof(u8));
	if (ret)
		goto free_mem;

	for (i = 0; i < n; i++)
		pd->bmControls[i] = bm_controls[i];

	ret = len;

free_mem:
	kfree(bm_controls);
unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

static ssize_t uvcg_default_processing_bm_controls_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_processing_unit_descriptor *pd;
	int result, i;
	char *pg = page;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	pd = &opts->uvc_processing;

	mutex_lock(&opts->lock);
	for (result = 0, i = 0; i < pd->bControlSize; ++i) {
		result += sprintf(pg, "%u\n", pd->bmControls[i]);
		pg = page + result;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

UVC_ATTR(uvcg_default_processing_, bm_controls, bmControls);

static struct configfs_attribute *uvcg_default_processing_attrs[] = {
	&uvcg_default_processing_attr_b_unit_id,
	&uvcg_default_processing_attr_b_source_id,
	&uvcg_default_processing_attr_w_max_multiplier,
	&uvcg_default_processing_attr_bm_controls,
	&uvcg_default_processing_attr_i_processing,
	NULL,
};

static const struct uvcg_config_group_type uvcg_default_processing_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_default_processing_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "default",
};

/* -----------------------------------------------------------------------------
 * control/processing
 */

static const struct uvcg_config_group_type uvcg_processing_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "processing",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_default_processing_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * control/terminal/camera/default
 */

#define UVCG_DEFAULT_CAMERA_ATTR(cname, aname, bits)			\
static ssize_t uvcg_default_camera_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct config_group *group = to_config_group(item);		\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvc_camera_terminal_descriptor *cd;			\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent->	\
			ci_parent;					\
	opts = to_f_uvc_opts(opts_item);				\
	cd = &opts->uvc_camera_terminal;				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(cd->aname));	\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
									\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_default_camera_, cname, aname)

UVCG_DEFAULT_CAMERA_ATTR(b_terminal_id, bTerminalID, 8);
UVCG_DEFAULT_CAMERA_ATTR(w_terminal_type, wTerminalType, 16);
UVCG_DEFAULT_CAMERA_ATTR(b_assoc_terminal, bAssocTerminal, 8);
UVCG_DEFAULT_CAMERA_ATTR(i_terminal, iTerminal, 8);
UVCG_DEFAULT_CAMERA_ATTR(w_objective_focal_length_min, wObjectiveFocalLengthMin,
			 16);
UVCG_DEFAULT_CAMERA_ATTR(w_objective_focal_length_max, wObjectiveFocalLengthMax,
			 16);
UVCG_DEFAULT_CAMERA_ATTR(w_ocular_focal_length, wOcularFocalLength,
			 16);

#undef UVCG_DEFAULT_CAMERA_ATTR

static ssize_t uvcg_default_camera_bm_controls_store(
	struct config_item *item, const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_camera_terminal_descriptor *cd;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	u8 *bm_controls, *tmp;
	unsigned int i;
	int ret, n = 0;

	mutex_lock(su_mutex);

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent->
			ci_parent;
	opts = to_f_uvc_opts(opts_item);
	cd = &opts->uvc_camera_terminal;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n,
				       sizeof(u8));
	if (ret)
		goto unlock;

	if (n > cd->bControlSize) {
		ret = -EINVAL;
		goto unlock;
	}

	tmp = bm_controls = kcalloc(n, sizeof(u8), GFP_KERNEL);
	if (!bm_controls) {
		ret = -ENOMEM;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &tmp,
				       sizeof(u8));
	if (ret)
		goto free_mem;

	for (i = 0; i < n; i++)
		cd->bmControls[i] = bm_controls[i];

	ret = len;

free_mem:
	kfree(bm_controls);
unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

static ssize_t uvcg_default_camera_bm_controls_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_camera_terminal_descriptor *cd;
	int result, i;
	char *pg = page;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent->
			ci_parent;
	opts = to_f_uvc_opts(opts_item);
	cd = &opts->uvc_camera_terminal;

	mutex_lock(&opts->lock);
	for (result = 0, i = 0; i < cd->bControlSize; ++i) {
		result += sprintf(pg, "%u\n", cd->bmControls[i]);
		pg = page + result;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
	return result;
}

UVC_ATTR(uvcg_default_camera_, bm_controls, bmControls);

static struct configfs_attribute *uvcg_default_camera_attrs[] = {
	&uvcg_default_camera_attr_b_terminal_id,
	&uvcg_default_camera_attr_w_terminal_type,
	&uvcg_default_camera_attr_b_assoc_terminal,
	&uvcg_default_camera_attr_i_terminal,
	&uvcg_default_camera_attr_w_objective_focal_length_min,
	&uvcg_default_camera_attr_w_objective_focal_length_max,
	&uvcg_default_camera_attr_w_ocular_focal_length,
	&uvcg_default_camera_attr_bm_controls,
	NULL,
};

static const struct uvcg_config_group_type uvcg_default_camera_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_default_camera_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "default",
};

/* -----------------------------------------------------------------------------
 * control/terminal/camera
 */

static const struct uvcg_config_group_type uvcg_camera_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "camera",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_default_camera_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * control/terminal/output/default
 */

#define UVCG_DEFAULT_OUTPUT_ATTR(cname, aname, bits)			\
static ssize_t uvcg_default_output_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct config_group *group = to_config_group(item);		\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvc_output_terminal_descriptor *cd;			\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->		\
			ci_parent->ci_parent;				\
	opts = to_f_uvc_opts(opts_item);				\
	cd = &opts->uvc_output_terminal;				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(cd->aname));	\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
									\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_default_output_, cname, aname)

UVCG_DEFAULT_OUTPUT_ATTR(b_terminal_id, bTerminalID, 8);
UVCG_DEFAULT_OUTPUT_ATTR(w_terminal_type, wTerminalType, 16);
UVCG_DEFAULT_OUTPUT_ATTR(b_assoc_terminal, bAssocTerminal, 8);
UVCG_DEFAULT_OUTPUT_ATTR(i_terminal, iTerminal, 8);

#undef UVCG_DEFAULT_OUTPUT_ATTR

static ssize_t uvcg_default_output_b_source_id_show(struct config_item *item,
						    char *page)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_output_terminal_descriptor *cd;
	int result;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->
			ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	cd = &opts->uvc_output_terminal;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%u\n", le8_to_cpu(cd->bSourceID));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

static ssize_t uvcg_default_output_b_source_id_store(struct config_item *item,
						     const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_output_terminal_descriptor *cd;
	int result;
	u8 num;

	result = kstrtou8(page, 0, &num);
	if (result)
		return result;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->
			ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	cd = &opts->uvc_output_terminal;

	mutex_lock(&opts->lock);
	cd->bSourceID = num;
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return len;
}
UVC_ATTR(uvcg_default_output_, b_source_id, bSourceID);

static struct configfs_attribute *uvcg_default_output_attrs[] = {
	&uvcg_default_output_attr_b_terminal_id,
	&uvcg_default_output_attr_w_terminal_type,
	&uvcg_default_output_attr_b_assoc_terminal,
	&uvcg_default_output_attr_b_source_id,
	&uvcg_default_output_attr_i_terminal,
	NULL,
};

static const struct uvcg_config_group_type uvcg_default_output_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_default_output_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "default",
};

/* -----------------------------------------------------------------------------
 * control/terminal/output
 */

static const struct uvcg_config_group_type uvcg_output_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "output",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_default_output_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * control/terminal
 */

static const struct uvcg_config_group_type uvcg_terminal_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "terminal",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_camera_grp_type,
		&uvcg_output_grp_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * control/extensions
 */

#define UVCG_EXTENSION_ATTR(cname, aname, ro...)			\
static ssize_t uvcg_extension_##cname##_show(struct config_item *item,	\
					     char *page)		\
{									\
	struct config_group *group = to_config_group(item->ci_parent);	\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvcg_extension *xu = to_uvcg_extension(item);		\
	struct config_item *opts_item;					\
	struct f_uvc_opts *opts;					\
	int ret;							\
									\
	mutex_lock(su_mutex);						\
									\
	opts_item = item->ci_parent->ci_parent->ci_parent;		\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	ret = sprintf(page, "%u\n", xu->desc.aname);			\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
									\
	return ret;							\
}									\
UVC_ATTR##ro(uvcg_extension_, cname, aname)

UVCG_EXTENSION_ATTR(b_length, bLength, _RO);
UVCG_EXTENSION_ATTR(b_unit_id, bUnitID, _RO);
UVCG_EXTENSION_ATTR(i_extension, iExtension, _RO);

static ssize_t uvcg_extension_b_num_controls_store(struct config_item *item,
						   const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	int ret;
	u8 num;

	ret = kstrtou8(page, 0, &num);
	if (ret)
		return ret;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	xu->desc.bNumControls = num;
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return len;
}
UVCG_EXTENSION_ATTR(b_num_controls, bNumControls);

/*
 * In addition to storing bNrInPins, this function needs to realloc the
 * memory for the baSourceID array and additionally expand bLength.
 */
static ssize_t uvcg_extension_b_nr_in_pins_store(struct config_item *item,
						 const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	void *tmp_buf;
	int ret;
	u8 num;

	ret = kstrtou8(page, 0, &num);
	if (ret)
		return ret;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	if (num == xu->desc.bNrInPins) {
		ret = len;
		goto unlock;
	}

	tmp_buf = krealloc_array(xu->desc.baSourceID, num, sizeof(u8),
				 GFP_KERNEL | __GFP_ZERO);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto unlock;
	}

	xu->desc.baSourceID = tmp_buf;
	xu->desc.bNrInPins = num;
	xu->desc.bLength = UVC_DT_EXTENSION_UNIT_SIZE(xu->desc.bNrInPins,
						      xu->desc.bControlSize);

	ret = len;

unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}
UVCG_EXTENSION_ATTR(b_nr_in_pins, bNrInPins);

/*
 * In addition to storing bControlSize, this function needs to realloc the
 * memory for the bmControls array and additionally expand bLength.
 */
static ssize_t uvcg_extension_b_control_size_store(struct config_item *item,
						   const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	void *tmp_buf;
	int ret;
	u8 num;

	ret = kstrtou8(page, 0, &num);
	if (ret)
		return ret;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	if (num == xu->desc.bControlSize) {
		ret = len;
		goto unlock;
	}

	tmp_buf = krealloc_array(xu->desc.bmControls, num, sizeof(u8),
				 GFP_KERNEL | __GFP_ZERO);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto unlock;
	}

	xu->desc.bmControls = tmp_buf;
	xu->desc.bControlSize = num;
	xu->desc.bLength = UVC_DT_EXTENSION_UNIT_SIZE(xu->desc.bNrInPins,
						      xu->desc.bControlSize);

	ret = len;

unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

UVCG_EXTENSION_ATTR(b_control_size, bControlSize);

static ssize_t uvcg_extension_guid_extension_code_show(struct config_item *item,
						       char *page)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	memcpy(page, xu->desc.guidExtensionCode, sizeof(xu->desc.guidExtensionCode));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return sizeof(xu->desc.guidExtensionCode);
}

static ssize_t uvcg_extension_guid_extension_code_store(struct config_item *item,
							const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	int ret;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	memcpy(xu->desc.guidExtensionCode, page,
	       min(sizeof(xu->desc.guidExtensionCode), len));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	ret = sizeof(xu->desc.guidExtensionCode);

	return ret;
}

UVC_ATTR(uvcg_extension_, guid_extension_code, guidExtensionCode);

static ssize_t uvcg_extension_ba_source_id_show(struct config_item *item,
						char *page)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	char *pg = page;
	int ret, i;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	for (ret = 0, i = 0; i < xu->desc.bNrInPins; ++i) {
		ret += sprintf(pg, "%u\n", xu->desc.baSourceID[i]);
		pg = page + ret;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return ret;
}

static ssize_t uvcg_extension_ba_source_id_store(struct config_item *item,
						 const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	u8 *source_ids, *iter;
	int ret, n = 0;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n,
				       sizeof(u8));
	if (ret)
		goto unlock;

	iter = source_ids = kcalloc(n, sizeof(u8), GFP_KERNEL);
	if (!source_ids) {
		ret = -ENOMEM;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &iter,
				       sizeof(u8));
	if (ret) {
		kfree(source_ids);
		goto unlock;
	}

	kfree(xu->desc.baSourceID);
	xu->desc.baSourceID = source_ids;
	xu->desc.bNrInPins = n;
	xu->desc.bLength = UVC_DT_EXTENSION_UNIT_SIZE(xu->desc.bNrInPins,
						      xu->desc.bControlSize);

	ret = len;

unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}
UVC_ATTR(uvcg_extension_, ba_source_id, baSourceID);

static ssize_t uvcg_extension_bm_controls_show(struct config_item *item,
					       char *page)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	char *pg = page;
	int ret, i;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	for (ret = 0, i = 0; i < xu->desc.bControlSize; ++i) {
		ret += sprintf(pg, "%u\n", xu->desc.bmControls[i]);
		pg = page + ret;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return ret;
}

static ssize_t uvcg_extension_bm_controls_store(struct config_item *item,
						const char *page, size_t len)
{
	struct config_group *group = to_config_group(item->ci_parent);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	u8 *bm_controls, *iter;
	int ret, n = 0;

	mutex_lock(su_mutex);

	opts_item = item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n,
				       sizeof(u8));
	if (ret)
		goto unlock;

	iter = bm_controls = kcalloc(n, sizeof(u8), GFP_KERNEL);
	if (!bm_controls) {
		ret = -ENOMEM;
		goto unlock;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &iter,
				       sizeof(u8));
	if (ret) {
		kfree(bm_controls);
		goto unlock;
	}

	kfree(xu->desc.bmControls);
	xu->desc.bmControls = bm_controls;
	xu->desc.bControlSize = n;
	xu->desc.bLength = UVC_DT_EXTENSION_UNIT_SIZE(xu->desc.bNrInPins,
						      xu->desc.bControlSize);

	ret = len;

unlock:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

UVC_ATTR(uvcg_extension_, bm_controls, bmControls);

static struct configfs_attribute *uvcg_extension_attrs[] = {
	&uvcg_extension_attr_b_length,
	&uvcg_extension_attr_b_unit_id,
	&uvcg_extension_attr_b_num_controls,
	&uvcg_extension_attr_b_nr_in_pins,
	&uvcg_extension_attr_b_control_size,
	&uvcg_extension_attr_guid_extension_code,
	&uvcg_extension_attr_ba_source_id,
	&uvcg_extension_attr_bm_controls,
	&uvcg_extension_attr_i_extension,
	NULL,
};

static void uvcg_extension_release(struct config_item *item)
{
	struct uvcg_extension *xu = container_of(item, struct uvcg_extension, item);

	kfree(xu);
}

static int uvcg_extension_allow_link(struct config_item *src, struct config_item *tgt)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(src);
	struct config_item *gadget_item;
	struct gadget_string *string;
	struct config_item *strings;
	int ret = 0;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	/* Validate that the target of the link is an entry in strings/<langid> */
	gadget_item = src->ci_parent->ci_parent->ci_parent->ci_parent->ci_parent;
	strings = config_group_find_item(to_config_group(gadget_item), "strings");
	if (!strings || tgt->ci_parent->ci_parent != strings) {
		ret = -EINVAL;
		goto put_strings;
	}

	string = to_gadget_string(tgt);
	xu->string_descriptor_index = string->usb_string.id;

put_strings:
	config_item_put(strings);
	mutex_unlock(su_mutex);

	return ret;
}

static void uvcg_extension_drop_link(struct config_item *src, struct config_item *tgt)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvcg_extension *xu = to_uvcg_extension(src);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = src->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	xu->string_descriptor_index = 0;

	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
}

static struct configfs_item_operations uvcg_extension_item_ops = {
	.release	= uvcg_extension_release,
	.allow_link	= uvcg_extension_allow_link,
	.drop_link	= uvcg_extension_drop_link,
};

static const struct config_item_type uvcg_extension_type = {
	.ct_item_ops	= &uvcg_extension_item_ops,
	.ct_attrs	= uvcg_extension_attrs,
	.ct_owner	= THIS_MODULE,
};

static void uvcg_extension_drop(struct config_group *group, struct config_item *item)
{
	struct uvcg_extension *xu = container_of(item, struct uvcg_extension, item);
	struct config_item *opts_item;
	struct f_uvc_opts *opts;

	opts_item = group->cg_item.ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);

	config_item_put(item);
	list_del(&xu->list);
	kfree(xu->desc.baSourceID);
	kfree(xu->desc.bmControls);

	mutex_unlock(&opts->lock);
}

static struct config_item *uvcg_extension_make(struct config_group *group, const char *name)
{
	struct config_item *opts_item;
	struct uvcg_extension *xu;
	struct f_uvc_opts *opts;

	opts_item = group->cg_item.ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	xu = kzalloc(sizeof(*xu), GFP_KERNEL);
	if (!xu)
		return ERR_PTR(-ENOMEM);

	xu->desc.bLength = UVC_DT_EXTENSION_UNIT_SIZE(0, 0);
	xu->desc.bDescriptorType = USB_DT_CS_INTERFACE;
	xu->desc.bDescriptorSubType = UVC_VC_EXTENSION_UNIT;
	xu->desc.bNumControls = 0;
	xu->desc.bNrInPins = 0;
	xu->desc.baSourceID = NULL;
	xu->desc.bControlSize = 0;
	xu->desc.bmControls = NULL;

	mutex_lock(&opts->lock);

	xu->desc.bUnitID = ++opts->last_unit_id;

	config_item_init_type_name(&xu->item, name, &uvcg_extension_type);
	list_add_tail(&xu->list, &opts->extension_units);

	mutex_unlock(&opts->lock);

	return &xu->item;
}

static struct configfs_group_operations uvcg_extensions_grp_ops = {
	.make_item	= uvcg_extension_make,
	.drop_item	= uvcg_extension_drop,
};

static const struct uvcg_config_group_type uvcg_extensions_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_extensions_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "extensions",
};

/* -----------------------------------------------------------------------------
 * control/class/{fs|ss}
 */

struct uvcg_control_class_group {
	struct config_group group;
	const char *name;
};

static inline struct uvc_descriptor_header
**uvcg_get_ctl_class_arr(struct config_item *i, struct f_uvc_opts *o)
{
	struct uvcg_control_class_group *group =
		container_of(i, struct uvcg_control_class_group,
			     group.cg_item);

	if (!strcmp(group->name, "fs"))
		return o->uvc_fs_control_cls;

	if (!strcmp(group->name, "ss"))
		return o->uvc_ss_control_cls;

	return NULL;
}

static int uvcg_control_class_allow_link(struct config_item *src,
					 struct config_item *target)
{
	struct config_item *control, *header;
	struct f_uvc_opts *opts;
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvc_descriptor_header **class_array;
	struct uvcg_control_header *target_hdr;
	int i;
	int ret = -EINVAL;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	control = src->ci_parent->ci_parent;
	header = config_group_find_item(to_config_group(control), "header");
	if (!header || target->ci_parent != header)
		goto out;

	opts = to_f_uvc_opts(control->ci_parent);

	mutex_lock(&opts->lock);

	class_array = uvcg_get_ctl_class_arr(src, opts);
	if (!class_array)
		goto unlock;
	if (opts->refcnt || class_array[0]) {
		ret = -EBUSY;
		goto unlock;
	}

	target_hdr = to_uvcg_control_header(target);
	++target_hdr->linked;
	class_array[0] = (struct uvc_descriptor_header *)&target_hdr->desc;
	if (UVC_STREAMING_NUMS > 1) {
		for (i = 1; i < UVC_STREAMING_NUMS; i++)
			class_array[BASIC_ENTITY_NUMS + i] =
				class_array[BASIC_ENTITY_NUMS];

		class_array[BASIC_ENTITY_NUMS + UVC_STREAMING_NUMS] = NULL;
	}

	ret = 0;

unlock:
	mutex_unlock(&opts->lock);
out:
	config_item_put(header);
	mutex_unlock(su_mutex);
	return ret;
}

static void uvcg_control_class_drop_link(struct config_item *src,
					struct config_item *target)
{
	struct config_item *control, *header;
	struct f_uvc_opts *opts;
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvc_descriptor_header **class_array;
	struct uvcg_control_header *target_hdr;
	int i;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	control = src->ci_parent->ci_parent;
	header = config_group_find_item(to_config_group(control), "header");
	if (!header || target->ci_parent != header)
		goto out;

	opts = to_f_uvc_opts(control->ci_parent);

	mutex_lock(&opts->lock);

	class_array = uvcg_get_ctl_class_arr(src, opts);
	if (!class_array || opts->refcnt)
		goto unlock;

	target_hdr = to_uvcg_control_header(target);
	--target_hdr->linked;
	class_array[0] = NULL;
	for (i = 1; i < UVC_STREAMING_NUMS; i++)
		class_array[BASIC_ENTITY_NUMS + i] = NULL;

unlock:
	mutex_unlock(&opts->lock);
out:
	config_item_put(header);
	mutex_unlock(su_mutex);
}

static struct configfs_item_operations uvcg_control_class_item_ops = {
	.release	= uvcg_config_item_release,
	.allow_link	= uvcg_control_class_allow_link,
	.drop_link	= uvcg_control_class_drop_link,
};

static const struct config_item_type uvcg_control_class_type = {
	.ct_item_ops	= &uvcg_control_class_item_ops,
	.ct_owner	= THIS_MODULE,
};

/* -----------------------------------------------------------------------------
 * control/class
 */

static int uvcg_control_class_create_children(struct config_group *parent)
{
	static const char * const names[] = { "fs", "ss" };
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(names); ++i) {
		struct uvcg_control_class_group *group;

		group = kzalloc(sizeof(*group), GFP_KERNEL);
		if (!group)
			return -ENOMEM;

		group->name = names[i];

		config_group_init_type_name(&group->group, group->name,
					    &uvcg_control_class_type);
		configfs_add_default_group(&group->group, parent);
	}

	return 0;
}

static const struct uvcg_config_group_type uvcg_control_class_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "class",
	.create_children = uvcg_control_class_create_children,
};

/* -----------------------------------------------------------------------------
 * control
 */

static ssize_t uvcg_default_control_b_interface_number_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	int result = 0;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = item->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	result += sprintf(page, "%u\n", opts->control_interface);
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

UVC_ATTR_RO(uvcg_default_control_, b_interface_number, bInterfaceNumber);

static ssize_t uvcg_default_control_enable_interrupt_ep_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	int result = 0;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = item->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	result += sprintf(page, "%u\n", opts->enable_interrupt_ep);
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

static ssize_t uvcg_default_control_enable_interrupt_ep_store(
	struct config_item *item, const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	ssize_t ret;
	u8 num;

	ret = kstrtou8(page, 0, &num);
	if (ret)
		return ret;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = item->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	opts->enable_interrupt_ep = num;
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return len;
}
UVC_ATTR(uvcg_default_control_, enable_interrupt_ep, enable_interrupt_ep);

static struct configfs_attribute *uvcg_default_control_attrs[] = {
	&uvcg_default_control_attr_b_interface_number,
	&uvcg_default_control_attr_enable_interrupt_ep,
	NULL,
};

static const struct uvcg_config_group_type uvcg_control_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_default_control_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "control",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_control_header_grp_type,
		&uvcg_processing_grp_type,
		&uvcg_terminal_grp_type,
		&uvcg_control_class_grp_type,
		&uvcg_extensions_grp_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * streaming/uncompressed
 * streaming/mjpeg
 */

static const char * const uvcg_format_names[] = {
	"uncompressed",
	"mjpeg",
	"h264",
	"h265",
};

static struct uvcg_color_matching *
uvcg_format_get_default_color_match(struct config_item *streaming)
{
	struct config_item *color_matching_item, *cm_default;
	struct uvcg_color_matching *color_match;

	color_matching_item = config_group_find_item(to_config_group(streaming),
						     "color_matching");
	if (!color_matching_item)
		return NULL;

	cm_default = config_group_find_item(to_config_group(color_matching_item),
					    "default");
	config_item_put(color_matching_item);
	if (!cm_default)
		return NULL;

	color_match = to_uvcg_color_matching(to_config_group(cm_default));
	config_item_put(cm_default);

	return color_match;
}

static int uvcg_format_allow_link(struct config_item *src, struct config_item *tgt)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvcg_color_matching *color_matching_desc;
	struct config_item *streaming, *color_matching;
	struct uvcg_format *fmt;
	int ret = 0;

	mutex_lock(su_mutex);

	streaming = src->ci_parent->ci_parent;
	color_matching = config_group_find_item(to_config_group(streaming), "color_matching");
	if (!color_matching || color_matching != tgt->ci_parent) {
		ret = -EINVAL;
		goto out_put_cm;
	}

	fmt = to_uvcg_format(src);

	/*
	 * There's always a color matching descriptor associated with the format
	 * but without a symlink it should only ever be the default one. If it's
	 * not the default, there's already a symlink and we should bail out.
	 */
	color_matching_desc = uvcg_format_get_default_color_match(streaming);
	if (fmt->color_matching != color_matching_desc) {
		ret = -EBUSY;
		goto out_put_cm;
	}

	color_matching_desc->refcnt--;

	color_matching_desc = to_uvcg_color_matching(to_config_group(tgt));
	fmt->color_matching = color_matching_desc;
	color_matching_desc->refcnt++;

out_put_cm:
	config_item_put(color_matching);
	mutex_unlock(su_mutex);

	return ret;
}

static void uvcg_format_drop_link(struct config_item *src, struct config_item *tgt)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvcg_color_matching *color_matching_desc;
	struct config_item *streaming;
	struct uvcg_format *fmt;

	mutex_lock(su_mutex);

	color_matching_desc = to_uvcg_color_matching(to_config_group(tgt));
	color_matching_desc->refcnt--;

	streaming = src->ci_parent->ci_parent;
	color_matching_desc = uvcg_format_get_default_color_match(streaming);

	fmt = to_uvcg_format(src);
	fmt->color_matching = color_matching_desc;
	color_matching_desc->refcnt++;

	mutex_unlock(su_mutex);
}

static struct configfs_item_operations uvcg_format_item_operations = {
	.release	= uvcg_config_item_release,
	.allow_link	= uvcg_format_allow_link,
	.drop_link	= uvcg_format_drop_link,
};

static ssize_t uvcg_format_bma_controls_show(struct uvcg_format *f, char *page)
{
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &f->group.cg_subsys->su_mutex;
	int result, i;
	char *pg = page;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = f->group.cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	result = sprintf(pg, "0x");
	pg += result;
	for (i = 0; i < UVCG_STREAMING_CONTROL_SIZE; ++i) {
		result += sprintf(pg, "%x\n", f->bmaControls[i]);
		pg = page + result;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
	return result;
}

static ssize_t uvcg_format_bma_controls_store(struct uvcg_format *ch,
					      const char *page, size_t len)
{
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &ch->group.cg_subsys->su_mutex;
	int ret = -EINVAL;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = ch->group.cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	if (ch->linked || opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	if (len < 4 || *page != '0' ||
	    (*(page + 1) != 'x' && *(page + 1) != 'X'))
		goto end;
	ret = hex2bin(ch->bmaControls, page + 2, 1);
	if (ret < 0)
		goto end;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

/* -----------------------------------------------------------------------------
 * streaming/header/<NAME>
 * streaming/header
 */

static void uvcg_format_set_indices(struct config_group *fmt);

static int uvcg_streaming_header_allow_link(struct config_item *src,
					    struct config_item *target)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	struct uvc_device *uvc;
	struct uvcg_streaming_header *src_hdr;
	struct uvcg_format *target_fmt = NULL;
	struct uvcg_format_ptr *format_ptr;
	int i, ret = -EINVAL;

	src_hdr = to_uvcg_streaming_header(src);
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = src->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	uvc = opts->uvc;

	mutex_lock(&opts->lock);

	if (src_hdr->linked) {
		ret = -EBUSY;
		goto out;
	}

	/*
	 * Linking is only allowed to direct children of the format nodes
	 * (streaming/uncompressed or streaming/mjpeg nodes). First check that
	 * the grand-parent of the target matches the grand-parent of the source
	 * (the streaming node), and then verify that the target parent is a
	 * format node.
	 */
	if (src->ci_parent->ci_parent != target->ci_parent->ci_parent)
		goto out;

	for (i = 0; i < ARRAY_SIZE(uvcg_format_names); ++i) {
		if (!strcmp(target->ci_parent->ci_name, uvcg_format_names[i]))
			break;
	}

	if (i == ARRAY_SIZE(uvcg_format_names))
		goto out;

	target_fmt = container_of(to_config_group(target), struct uvcg_format,
				  group);
	if (!target_fmt)
		goto out;

	uvcg_format_set_indices(to_config_group(target));
	for (i = 0; i < target_fmt->strm_idx; i++) {
		if (!uvc->streaming_desc_num[i]) {
			pr_err("Please link previous stream format first\n");
			goto out;
		}
	}

	for (i = target_fmt->strm_idx + 1; i < UVC_STREAMING_NUMS; i++) {
		if (uvc->streaming_desc_num[i]) {
			pr_err("strm %d not allow to link again\n",
					target_fmt->strm_idx);
			goto out;
		}
	}

	format_ptr = kzalloc(sizeof(*format_ptr), GFP_KERNEL);
	if (!format_ptr) {
		ret = -ENOMEM;
		goto out;
	}
	/* stream header desc */
	if (!uvc->streaming_desc_num[target_fmt->strm_idx])
		uvc->streaming_desc_num[target_fmt->strm_idx]++;
	/* format desc, frame desc, color matching desc */
	uvc->streaming_desc_num[target_fmt->strm_idx] +=
		target_fmt->num_frames + 2;
	if (opts->uvc_still_frame[target_fmt->type].bNumImageSizePatterns)
		uvc->streaming_desc_num[target_fmt->strm_idx]++;
	ret = 0;
	format_ptr->fmt = target_fmt;
	list_add_tail(&format_ptr->entry, &src_hdr->formats);
	++src_hdr->num_fmt[target_fmt->strm_idx];
	++target_fmt->linked;

out:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

static void uvcg_streaming_header_drop_link(struct config_item *src,
					   struct config_item *target)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	struct uvcg_streaming_header *src_hdr;
	struct uvcg_format *target_fmt = NULL;
	struct uvcg_format_ptr *format_ptr, *tmp;

	src_hdr = to_uvcg_streaming_header(src);
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = src->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	target_fmt = container_of(to_config_group(target), struct uvcg_format,
				  group);
	if (!target_fmt)
		goto out;

	list_for_each_entry_safe(format_ptr, tmp, &src_hdr->formats, entry)
		if (format_ptr->fmt == target_fmt) {
			list_del(&format_ptr->entry);
			kfree(format_ptr);
			--src_hdr->num_fmt[target_fmt->strm_idx];
			break;
		}

	--target_fmt->linked;

out:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
}

static struct configfs_item_operations uvcg_streaming_header_item_ops = {
	.release	= uvcg_config_item_release,
	.allow_link	= uvcg_streaming_header_allow_link,
	.drop_link	= uvcg_streaming_header_drop_link,
};

#define UVCG_STREAMING_HEADER_ATTR(cname, aname, bits)			\
static ssize_t uvcg_streaming_header_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct uvcg_streaming_header *sh = to_uvcg_streaming_header(item); \
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &sh->item.ci_group->cg_subsys->su_mutex;\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = sh->item.ci_parent->ci_parent->ci_parent;		\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(sh->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_streaming_header_, cname, aname)

UVCG_STREAMING_HEADER_ATTR(bm_info, bmInfo, 8);
UVCG_STREAMING_HEADER_ATTR(b_terminal_link, bTerminalLink, 8);
UVCG_STREAMING_HEADER_ATTR(b_trigger_support, bTriggerSupport, 8);
UVCG_STREAMING_HEADER_ATTR(b_trigger_usage, bTriggerUsage, 8);

static ssize_t uvcg_streaming_header_b_still_capture_method_show(
	struct config_item *item, char *page)
{
	struct uvcg_streaming_header *sh = to_uvcg_streaming_header(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &sh->item.ci_group->cg_subsys->su_mutex;
	int result;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = sh->item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	result = sprintf(page, "%u\n",
		le8_to_cpu(sh->desc.bStillCaptureMethod));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
	return result;
}

static ssize_t uvcg_streaming_header_b_still_capture_method_store(
	struct config_item *item, const char *page, size_t len)
{
	struct uvcg_streaming_header *sh = to_uvcg_streaming_header(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &sh->item.ci_group->cg_subsys->su_mutex;
	int ret = -EINVAL;
	u8 num;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = sh->item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}
	ret = kstrtou8(page, 0, &num);
	if (ret)
		goto end;
	sh->desc.bStillCaptureMethod = cpu_to_le8(num);
	if (sh->desc.bStillCaptureMethod == 0 ||
		sh->desc.bStillCaptureMethod == 2) {
		ret = len;
	} else {
		pr_err("bStillCaptureMethod must be 0 or 2, now set %d\n",
			sh->desc.bStillCaptureMethod);
		ret = -EINVAL;
	}
end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}
UVC_ATTR(uvcg_streaming_header_, b_still_capture_method, bStillCaptureMethod);
#undef UVCG_STREAMING_HEADER_ATTR

static struct configfs_attribute *uvcg_streaming_header_attrs[] = {
	&uvcg_streaming_header_attr_bm_info,
	&uvcg_streaming_header_attr_b_terminal_link,
	&uvcg_streaming_header_attr_b_still_capture_method,
	&uvcg_streaming_header_attr_b_trigger_support,
	&uvcg_streaming_header_attr_b_trigger_usage,
	NULL,
};

static const struct config_item_type uvcg_streaming_header_type = {
	.ct_item_ops	= &uvcg_streaming_header_item_ops,
	.ct_attrs	= uvcg_streaming_header_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_item
*uvcg_streaming_header_make(struct config_group *group, const char *name)
{
	struct uvcg_streaming_header *h;

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&h->formats);
	h->desc.bDescriptorType		= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType	= UVC_VS_INPUT_HEADER;
	h->desc.bNumFormats		= 1;
	h->desc.bTerminalLink		= 5;
	h->desc.bControlSize		= UVCG_STREAMING_CONTROL_SIZE;

	config_item_init_type_name(&h->item, name, &uvcg_streaming_header_type);

	return &h->item;
}

static struct configfs_group_operations uvcg_streaming_header_grp_ops = {
	.make_item		= uvcg_streaming_header_make,
};

static const struct uvcg_config_group_type uvcg_streaming_header_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_streaming_header_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "header",
};

/* -----------------------------------------------------------------------------
 * streaming/<mode>/<format>/<NAME>
 */

#define UVCG_STILL_FRAME_ATTR(fname, cname, aname, bits, max)		\
static ssize_t uvcg_##fname##_##cname##_show(struct config_item *item,	\
						char *page)		\
{									\
	struct config_group *group = to_config_group(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;	\
	struct uvc_still_frame_descriptor *sd;			\
	int result, j;			\
	char *parent_name;		\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	parent_name = group->cg_item.ci_parent->ci_name;	\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);	\
								\
	for (j = 0; j < FORMAT_CNT; j++) {		\
		if (strstr(uvcg_format_names[j], parent_name)) {	\
			sd = &opts->uvc_still_frame[j];	\
			break;					\
		}				\
	}							\
	if (sd == NULL)	\
		return -EINVAL;		\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", sd->aname);			\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t  uvcg_##fname##_##cname##_store(struct config_item *item,\
					   const char *page, size_t len)\
{									\
	struct config_group *group = to_config_group(item);		\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvc_still_frame_descriptor *sd;				\
	int ret, j;							\
	char *parent_name;						\
	u8 num;								\
	struct uvcg_format *fmt;					\
									\
	parent_name = group->cg_item.ci_parent->ci_name;		\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
	fmt = to_uvcg_format(group->cg_item.ci_parent);			\
	if (fmt->linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
	for (j = 0; j < FORMAT_CNT; j++) {				\
		if (strstr(uvcg_format_names[j], parent_name)) {	\
			sd = &opts->uvc_still_frame[j];			\
			break;						\
		}							\
	}								\
	if (sd == NULL) {						\
		mutex_unlock(su_mutex);					\
		return -EINVAL;						\
	}								\
									\
	mutex_lock(&opts->lock);					\
	ret = kstrtou##bits(page, 0, &num);				\
	if (ret)							\
		goto end;						\
									\
	if (max >= 0 && num > max) {					\
		ret = -EINVAL;						\
		goto end;						\
	}								\
	sd->aname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_##fname##_, cname, aname);
UVCG_STILL_FRAME_ATTR(still_frame, b_endpoint_addr, bEndpointAddress, 8, -1);
UVCG_STILL_FRAME_ATTR(still_frame, b_numimages_sizepatterns, bNumImageSizePatterns, 8, 15);
UVCG_STILL_FRAME_ATTR(still_frame, b_numcompression_pattern, bNumCompressionPattern, 8, 0);
#undef UVCG_STILL_FRAME_ATTR

static ssize_t uvcg_still_frame_w_width_w_height_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_still_frame_descriptor *sd;
	u8 cnt;
	char *pg = page;
	int result, i, j;
	char *parent_name;

	parent_name = group->cg_item.ci_parent->ci_name;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	for (j = 0; j < FORMAT_CNT; j++) {
		if (strstr(uvcg_format_names[j], parent_name)) {
			sd = &opts->uvc_still_frame[j];
			break;
		}
	}
	if (sd == NULL)
		return -EINVAL;

	mutex_lock(&opts->lock);
	cnt = sd->bNumImageSizePatterns * 2;
	for (result = 0, i = 0; i < cnt; ++i) {
		result += sprintf(pg, "%u\n", sd->wWidth_wHeight[i]);
		pg = page + result;
	}

	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
	return result;
}

static ssize_t uvcg_still_frame_w_width_w_height_store(
	struct config_item *item, const char *page, size_t len)
{
	struct config_group *group = to_config_group(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct uvc_still_frame_descriptor *sd;
	int ret = 0, n = 0;
	u16 *res_intrv, *tmp;
	int i = 0, j = 0;
	char *parent_name;

	parent_name = group->cg_item.ci_parent->ci_name;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	for (j = 0; j < FORMAT_CNT; j++) {
		if (strstr(uvcg_format_names[j], parent_name)) {
			sd = &opts->uvc_still_frame[j];
			break;
		}
	}
	if (!sd)
		return -EINVAL;

	mutex_lock(&opts->lock);
	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n,
				       sizeof(u16));
	if (ret)
		goto end;

	tmp = res_intrv = kcalloc(n, sizeof(u16), GFP_KERNEL);
	if (!res_intrv) {
		ret = -ENOMEM;
		goto end;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &tmp,
				       sizeof(u16));
	if (ret) {
		kfree(res_intrv);
		goto end;
	}

	sd->bNumImageSizePatterns = n / 2;
	for (i = 0; i < n; i++)
		sd->wWidth_wHeight[i] = res_intrv[i];

	ret = len;
end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}
UVC_ATTR(uvcg_still_frame_, w_width_w_height, wWidth_wHeight);

static struct configfs_attribute *uvcg_still_frame_attrs[] = {
	&uvcg_still_frame_attr_b_endpoint_addr,
	&uvcg_still_frame_attr_b_numimages_sizepatterns,
	&uvcg_still_frame_attr_w_width_w_height,
	&uvcg_still_frame_attr_b_numcompression_pattern,
	NULL,
};

static const struct uvcg_config_group_type still_frame_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_still_frame_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "still_image",
};

/* -----------------------------------------------------------------------------
 * streaming/<mode>/<format>/<NAME>
 */

#define UVCG_FRAME_ATTR(fname, cname, aname, bits)			\
static ssize_t uvcg_##fname##_##cname##_show(struct config_item *item,	\
						char *page)		\
{									\
	struct uvcg_frame *f = to_uvcg_frame(item);			\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &f->item.ci_group->cg_subsys->su_mutex;\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = f->item.ci_parent->ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", f->fname.cname);			\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t  uvcg_##fname##_##cname##_store(struct config_item *item,\
					   const char *page, size_t len)\
{									\
	struct uvcg_frame *f = to_uvcg_frame(item);			\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct uvcg_format *fmt;					\
	struct mutex *su_mutex = &f->item.ci_group->cg_subsys->su_mutex;\
	typeof(f->fname.cname) num;					\
	int ret;							\
									\
	ret = kstrtou##bits(page, 0, &num);				\
	if (ret)							\
		return ret;						\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = f->item.ci_parent->ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
	fmt = to_uvcg_format(f->item.ci_parent);			\
									\
	mutex_lock(&opts->lock);					\
	if (fmt->linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	f->fname.cname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_##fname##_, cname, aname);
static ssize_t __b_frame_index_show(struct config_item *item,
			char *page, int h26x)
{
	struct uvcg_frame *f = to_uvcg_frame(item);
	struct uvcg_format *fmt;
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct config_item *fmt_item;
	struct mutex *su_mutex = &f->item.ci_group->cg_subsys->su_mutex;
	int result;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	fmt_item = f->item.ci_parent;
	fmt = to_uvcg_format(fmt_item);

	if (!fmt->linked) {
		result = -EBUSY;
		goto out;
	}

	opts_item = fmt_item->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	if (h26x)
		result = sprintf(page, "%u\n",
				f->h26x_frame.b_frame_index);
	else
		result = sprintf(page, "%u\n",
				f->frame.b_frame_index);
	mutex_unlock(&opts->lock);

out:
	mutex_unlock(su_mutex);
	return result;
}
static ssize_t uvcg_frame_b_frame_index_show(struct config_item *item,
					     char *page)
{
	return __b_frame_index_show(item, page, 0);
}

UVC_ATTR_RO(uvcg_frame_, b_frame_index, bFrameIndex);

UVCG_FRAME_ATTR(frame, bm_capabilities, bmCapabilities, 8);
UVCG_FRAME_ATTR(frame, w_width, wWidth, 16);
UVCG_FRAME_ATTR(frame, w_height, wHeight, 16);
UVCG_FRAME_ATTR(frame, dw_min_bit_rate, dwMinBitRate, 32);
UVCG_FRAME_ATTR(frame, dw_max_bit_rate, dwMaxBitRate, 32);
UVCG_FRAME_ATTR(frame, dw_max_video_frame_buffer_size,
			dwMaxVideoFrameBufferSize, 32);
UVCG_FRAME_ATTR(frame, dw_default_frame_interval,
			dwDefaultFrameInterval, 32);


static ssize_t uvcg_h26x_frame_b_frame_index_show(
		struct config_item *item, char *page)
{
	return __b_frame_index_show(item, page, 1);
}

UVC_ATTR_RO(uvcg_h26x_frame_, b_frame_index, bFrameIndex);

#ifdef CONFIG_USB_RTSX_UVC_15
UVCG_FRAME_ATTR(h26x_frame, w_sar_width, wSARwidth, 16);
UVCG_FRAME_ATTR(h26x_frame, w_sar_height, wSARheight, 16);
UVCG_FRAME_ATTR(h26x_frame, w_profile, wProfile, 16);
UVCG_FRAME_ATTR(h26x_frame, b_level_idc, bLevelIDC, 8);

static ssize_t uvcg_h26x_frame_bm_capabilities_show(
	struct config_item *item, char *page)
{
	struct uvcg_frame *f = to_uvcg_frame(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &f->item.ci_group->cg_subsys->su_mutex;
	struct uvcg_format *fmt;
	int ret, i;
	char *pg = page;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = f->item.ci_parent->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	fmt = to_uvcg_format(f->item.ci_parent);

	mutex_lock(&opts->lock);

	for (ret = 0, i = 0; i < 2; ++i) {
		ret += sprintf(pg, "%d\n", f->h26x_frame.bm_capabilities[i]);
		pg = page + ret;
	}
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);

	return ret;
}

static ssize_t uvcg_h26x_frame_bm_capabilities_store(
		struct config_item *item,
		const char *page, size_t len)
{
	struct uvcg_frame *f = to_uvcg_frame(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &f->item.ci_group->cg_subsys->su_mutex;
	struct uvcg_format *fmt;
	int ret = -EINVAL;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = f->item.ci_parent->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	fmt = to_uvcg_format(f->item.ci_parent);

	mutex_lock(&opts->lock);
	if (fmt->linked || opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	if (len < 6 || *page != '0' ||
	    (*(page + 1) != 'x' && *(page + 1) != 'X'))
		goto end;

	ret = hex2bin(f->h26x_frame.bm_capabilities, page + 2, 2);
	if (ret < 0)
		goto end;

	ret = 7;
end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);

	return ret;
}
UVC_ATTR(uvcg_h26x_frame_, bm_capabilities, bmCapabilities);
#else
UVCG_FRAME_ATTR(h26x_frame, bm_capabilities, bmCapabilitie, 8);
UVCG_FRAME_ATTR(h26x_frame, dw_bytes_per_line, dwBytesPerLine, 32);
#endif
UVCG_FRAME_ATTR(h26x_frame, w_width, wWidth, 16);
UVCG_FRAME_ATTR(h26x_frame, w_height, wHeight, 16);
UVCG_FRAME_ATTR(h26x_frame, dw_min_bit_rate, dwMinBitRate, 32);
UVCG_FRAME_ATTR(h26x_frame, dw_max_bit_rate, dwMaxBitRate, 32);
UVCG_FRAME_ATTR(h26x_frame, dw_default_frame_interval,
			dwDefaultFrameInterval, 32);
#undef UVCG_FRAME_ATTR

static ssize_t uvcg_frame_dw_frame_interval_show(struct config_item *item,
						 char *page)
{
	struct uvcg_frame *frm = to_uvcg_frame(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &frm->item.ci_group->cg_subsys->su_mutex;
	struct uvcg_format *fmt;
	int result, i;
	u8 cnt;
	char *pg = page;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = frm->item.ci_parent->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	fmt = to_uvcg_format(frm->item.ci_parent);

	mutex_lock(&opts->lock);
	if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265)
		cnt = frm->h26x_frame.b_frame_interval_type;
	else
		cnt = frm->frame.b_frame_interval_type;
	for (result = 0, i = 0; i < cnt; ++i) {
		result += sprintf(pg, "%u\n", frm->dw_frame_interval[i]);
		pg = page + result;
	}
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);
	return result;
}

static ssize_t uvcg_frame_dw_frame_interval_store(struct config_item *item,
						  const char *page, size_t len)
{
	struct uvcg_frame *ch = to_uvcg_frame(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct uvcg_format *fmt;
	struct mutex *su_mutex = &ch->item.ci_group->cg_subsys->su_mutex;
	int ret = 0, n = 0;
	u32 *frm_intrv, *tmp;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = ch->item.ci_parent->ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);
	fmt = to_uvcg_format(ch->item.ci_parent);

	mutex_lock(&opts->lock);
	if (fmt->linked || opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_count_item_entries, &n, sizeof(u32));
	if (ret)
		goto end;

	tmp = frm_intrv = kcalloc(n, sizeof(u32), GFP_KERNEL);
	if (!frm_intrv) {
		ret = -ENOMEM;
		goto end;
	}

	ret = __uvcg_iter_item_entries(page, len, __uvcg_fill_item_entries, &tmp, sizeof(u32));
	if (ret) {
		kfree(frm_intrv);
		goto end;
	}

	kfree(ch->dw_frame_interval);
	ch->dw_frame_interval = frm_intrv;
	if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265)
		ch->h26x_frame.b_frame_interval_type = n;
	else
		ch->frame.b_frame_interval_type = n;
	sort(ch->dw_frame_interval, n, sizeof(*ch->dw_frame_interval),
	     uvcg_config_compare_u32, NULL);
	ret = len;

end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

UVC_ATTR(uvcg_frame_, dw_frame_interval, dwFrameInterval);

static struct configfs_attribute *uvcg_frame_attrs[] = {
	&uvcg_frame_attr_b_frame_index,
	&uvcg_frame_attr_bm_capabilities,
	&uvcg_frame_attr_w_width,
	&uvcg_frame_attr_w_height,
	&uvcg_frame_attr_dw_min_bit_rate,
	&uvcg_frame_attr_dw_max_bit_rate,
	&uvcg_frame_attr_dw_max_video_frame_buffer_size,
	&uvcg_frame_attr_dw_default_frame_interval,
	&uvcg_frame_attr_dw_frame_interval,
	NULL,
};

static const struct config_item_type uvcg_frame_type = {
	.ct_item_ops	= &uvcg_config_item_ops,
	.ct_attrs	= uvcg_frame_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_attribute *uvcg_h26x_frame_attrs[] = {
	&uvcg_h26x_frame_attr_b_frame_index,
	&uvcg_h26x_frame_attr_bm_capabilities,
	&uvcg_h26x_frame_attr_w_width,
	&uvcg_h26x_frame_attr_w_height,
	&uvcg_h26x_frame_attr_dw_min_bit_rate,
	&uvcg_h26x_frame_attr_dw_max_bit_rate,
	&uvcg_h26x_frame_attr_dw_default_frame_interval,
#ifdef CONFIG_USB_RTSX_UVC_15
	&uvcg_h26x_frame_attr_w_sar_width,
	&uvcg_h26x_frame_attr_w_sar_height,
	&uvcg_h26x_frame_attr_w_profile,
	&uvcg_h26x_frame_attr_b_level_idc,
#else
	&uvcg_h26x_frame_attr_dw_bytes_per_line,
#endif
	&uvcg_frame_attr_dw_frame_interval,
	NULL,
};
static const struct config_item_type uvcg_h26x_frame_type = {
	.ct_item_ops	= &uvcg_config_item_ops,
	.ct_attrs	= uvcg_h26x_frame_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_item *uvcg_frame_make(struct config_group *group,
					   const char *name)
{
	struct uvcg_frame *h;
	struct uvcg_format *fmt;
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct uvcg_frame_ptr *frame_ptr;

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	h->frame.b_descriptor_type		= USB_DT_CS_INTERFACE;
	h->frame.b_frame_index			= 1;
	h->frame.w_width			= 640;
	h->frame.w_height			= 360;
	h->frame.dw_min_bit_rate		= 18432000;
	h->frame.dw_max_bit_rate		= 55296000;
	h->frame.dw_max_video_frame_buffer_size	= 460800;
	h->frame.dw_default_frame_interval	= 666666;

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	fmt = to_uvcg_format(&group->cg_item);
	if (fmt->type == UVCG_UNCOMPRESSED ||
		fmt->type == UVCG_UNCOMPRESSED_NV12) {
		h->frame.b_descriptor_subtype = UVC_VS_FRAME_UNCOMPRESSED;
		h->fmt_type = UVCG_UNCOMPRESSED;
	} else if (fmt->type == UVCG_MJPEG) {
		h->frame.b_descriptor_subtype = UVC_VS_FRAME_MJPEG;
		h->fmt_type = UVCG_MJPEG;
	} else if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265) {
#ifdef CONFIG_USB_RTSX_UVC_15
		h->h26x_frame.b_descriptor_type	= USB_DT_CS_INTERFACE;
		h->h26x_frame.b_frame_index	= 1;
		h->h26x_frame.w_width		= cpu_to_le16(640);
		h->h26x_frame.w_height		= cpu_to_le16(360);
		h->h26x_frame.dw_min_bit_rate	= cpu_to_le32(18432000);
		h->h26x_frame.dw_max_bit_rate	= cpu_to_le32(55296000);
		h->h26x_frame.dw_default_frame_interval	=
						cpu_to_le32(666666);
		h->h26x_frame.b_descriptor_subtype = VS_FRAME_H264;
		h->h26x_frame.w_sar_width = 0x0001;
		h->h26x_frame.w_sar_height = 0x0001;
		h->h26x_frame.w_profile = 0x6400;
		h->h26x_frame.b_level_idc = 40;
		h->h26x_frame.w_constrained_toolset = 0;
		h->h26x_frame.bm_supported_usages[0] = 0x01;
		h->h26x_frame.bm_supported_usages[1] = 0x00;
		h->h26x_frame.bm_supported_usages[2] = 0x01;
		h->h26x_frame.bm_supported_usages[3] = 0x0;
		h->h26x_frame.bm_capabilities[0] = H264_ENC_CAPBILITY;
		h->h26x_frame.bm_capabilities[1] = 0x0;
		h->h26x_frame.bm_svc_capabilities[0] = 0;
		h->h26x_frame.bm_svc_capabilities[1] = 0;
		h->h26x_frame.bm_svc_capabilities[2] = 0;
		h->h26x_frame.bm_svc_capabilities[3] = 0;
		h->h26x_frame.bm_mvc_capabilities[0] = 0;
		h->h26x_frame.bm_mvc_capabilities[1] = 0;
		h->h26x_frame.bm_mvc_capabilities[2] = 0;
		h->h26x_frame.bm_mvc_capabilities[3] = 0;
#else
		h->h26x_frame.b_descriptor_subtype = UVC_VS_FRAME_FRAME_BASED;
		h->h26x_frame.dw_default_frame_interval	= cpu_to_le32(666666);
		h->h26x_frame.bm_capabilities = 0;
		h->h26x_frame.dw_bytes_per_line = 0; //Variable Size
#endif
		if (fmt->type == UVCG_H264)
			h->fmt_type = UVCG_H264;
		else
			h->fmt_type = UVCG_H265;
	} else {
		mutex_unlock(&opts->lock);
		kfree(h);
		return ERR_PTR(-EINVAL);
	}

	frame_ptr = kzalloc(sizeof(*frame_ptr), GFP_KERNEL);
	if (!frame_ptr) {
		mutex_unlock(&opts->lock);
		kfree(h);
		return ERR_PTR(-ENOMEM);
	}

	frame_ptr->frm = h;
	list_add_tail(&frame_ptr->entry, &fmt->frames);
	++fmt->num_frames;
	mutex_unlock(&opts->lock);

	if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265)
		config_item_init_type_name(&h->item, name,
					&uvcg_h26x_frame_type);
	else
		config_item_init_type_name(&h->item, name, &uvcg_frame_type);

	return &h->item;
}

static void uvcg_frame_drop(struct config_group *group, struct config_item *item)
{
	struct uvcg_format *fmt;
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct uvcg_frame *target_frm = NULL;
	struct uvcg_frame_ptr *frame_ptr, *tmp;

	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	target_frm = container_of(item, struct uvcg_frame, item);
	fmt = to_uvcg_format(&group->cg_item);

	list_for_each_entry_safe(frame_ptr, tmp, &fmt->frames, entry)
		if (frame_ptr->frm == target_frm) {
			list_del(&frame_ptr->entry);
			kfree(frame_ptr);
			--fmt->num_frames;
			break;
		}
	mutex_unlock(&opts->lock);

	config_item_put(item);
}

static void uvcg_format_set_indices(struct config_group *fmt)
{
	struct config_item *ci;
	unsigned int i = 1;

	list_for_each_entry(ci, &fmt->cg_children, ci_entry) {
		struct uvcg_frame *frm;

		if (ci->ci_type != &uvcg_frame_type)
			continue;

		frm = to_uvcg_frame(ci);
		frm->frame.b_frame_index = i++;
	}
}

/* -----------------------------------------------------------------------------
 * streaming/uncompressed/<NAME>
 */

static struct configfs_group_operations uvcg_uncompressed_group_ops = {
	.make_item		= uvcg_frame_make,
	.drop_item		= uvcg_frame_drop,
};

static ssize_t uvcg_uncompressed_guid_format_show(struct config_item *item,
							char *page)
{
	struct uvcg_uncompressed *ch = to_uvcg_uncompressed(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &ch->fmt.group.cg_subsys->su_mutex;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = ch->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	memcpy(page, ch->desc.guidFormat, sizeof(ch->desc.guidFormat));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return sizeof(ch->desc.guidFormat);
}

static ssize_t uvcg_uncompressed_guid_format_store(struct config_item *item,
						   const char *page, size_t len)
{
	static char yuy2[] = {
		'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00,
		0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
	};
	static char nv12[] = {
		'N',  'V',  '1',  '2', 0x00, 0x00, 0x10, 0x00,
		0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
	};

	struct uvcg_uncompressed *ch = to_uvcg_uncompressed(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &ch->fmt.group.cg_subsys->su_mutex;
	int ret;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = ch->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	if (ch->fmt.linked || opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	if (strstr(page, "NV12")) {
		ch->fmt.type = UVCG_UNCOMPRESSED_NV12;
		memcpy(ch->desc.guidFormat, nv12, sizeof(nv12));
		ret = len;
	} else if (strstr(page, "YUY2")) {
		memcpy(ch->desc.guidFormat, yuy2, sizeof(yuy2));
		ret = len;
	} else {
		memcpy(ch->desc.guidFormat, page,
			min(sizeof(ch->desc.guidFormat), len));
		ret = sizeof(ch->desc.guidFormat);
	}

end:
	mutex_unlock(&opts->lock);
	mutex_unlock(su_mutex);
	return ret;
}

UVC_ATTR(uvcg_uncompressed_, guid_format, guidFormat);

#define UVCG_UNCOMPRESSED_ATTR_RO(cname, aname, bits)			\
static ssize_t uvcg_uncompressed_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct uvcg_uncompressed *u = to_uvcg_uncompressed(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(u->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_uncompressed_, cname, aname);

#define UVCG_UNCOMPRESSED_ATTR(cname, aname, bits)			\
static ssize_t uvcg_uncompressed_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct uvcg_uncompressed *u = to_uvcg_uncompressed(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(u->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t								\
uvcg_uncompressed_##cname##_store(struct config_item *item,		\
				    const char *page, size_t len)	\
{									\
	struct uvcg_uncompressed *u = to_uvcg_uncompressed(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int ret;							\
	u8 num;								\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	if (u->fmt.linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtou8(page, 0, &num);					\
	if (ret)							\
		goto end;						\
									\
	/* index values in uvc are never 0 */				\
	if (!num) {							\
		ret = -EINVAL;						\
		goto end;						\
	}								\
									\
	u->desc.aname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_uncompressed_, cname, aname);

UVCG_UNCOMPRESSED_ATTR_RO(b_format_index, bFormatIndex, 8);
UVCG_UNCOMPRESSED_ATTR(b_bits_per_pixel, bBitsPerPixel, 8);
UVCG_UNCOMPRESSED_ATTR(b_default_frame_index, bDefaultFrameIndex, 8);
UVCG_UNCOMPRESSED_ATTR_RO(b_aspect_ratio_x, bAspectRatioX, 8);
UVCG_UNCOMPRESSED_ATTR_RO(b_aspect_ratio_y, bAspectRatioY, 8);
UVCG_UNCOMPRESSED_ATTR_RO(bm_interlace_flags, bmInterlaceFlags, 8);

#undef UVCG_UNCOMPRESSED_ATTR
#undef UVCG_UNCOMPRESSED_ATTR_RO

static inline ssize_t
uvcg_uncompressed_bma_controls_show(struct config_item *item, char *page)
{
	struct uvcg_uncompressed *unc = to_uvcg_uncompressed(item);
	return uvcg_format_bma_controls_show(&unc->fmt, page);
}

static inline ssize_t
uvcg_uncompressed_bma_controls_store(struct config_item *item,
				     const char *page, size_t len)
{
	struct uvcg_uncompressed *unc = to_uvcg_uncompressed(item);
	return uvcg_format_bma_controls_store(&unc->fmt, page, len);
}

UVC_ATTR(uvcg_uncompressed_, bma_controls, bmaControls);

static struct configfs_attribute *uvcg_uncompressed_attrs[] = {
	&uvcg_uncompressed_attr_b_format_index,
	&uvcg_uncompressed_attr_guid_format,
	&uvcg_uncompressed_attr_b_bits_per_pixel,
	&uvcg_uncompressed_attr_b_default_frame_index,
	&uvcg_uncompressed_attr_b_aspect_ratio_x,
	&uvcg_uncompressed_attr_b_aspect_ratio_y,
	&uvcg_uncompressed_attr_bm_interlace_flags,
	&uvcg_uncompressed_attr_bma_controls,
	NULL,
};

static const struct config_item_type uvcg_uncompressed_type = {
	.ct_item_ops	= &uvcg_format_item_operations,
	.ct_group_ops	= &uvcg_uncompressed_group_ops,
	.ct_attrs	= uvcg_uncompressed_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *uvcg_uncompressed_make(struct config_group *group,
						   const char *name)
{
	static char guid[] = {
		'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
	};
	struct uvcg_color_matching *color_match;
	struct config_item *streaming;
	struct uvcg_uncompressed *h;
	char buf[MAX_NAME_LEN];
	char *idx_str;
	u8 idx;
	int ret = 0;

	ret = snprintf(buf, MAX_NAME_LEN, "%s", name);
	if (ret >= MAX_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	idx_str = strchr(buf, '.');
	if (!idx_str) {
		pr_err("Unable to locate . in name.strm_idx\n");
		return ERR_PTR(-EINVAL);
	}
	*idx_str = '\0';
	idx_str++;
	if (!strlen(buf))
		return ERR_PTR(-EINVAL);
	ret = kstrtou8(idx_str, 0, &idx);
	if (ret)
		return ERR_PTR(ret);

	if (idx >= UVC_STREAMING_NUMS)
		return ERR_PTR(-EINVAL);


	streaming = group->cg_item.ci_parent;
	color_match = uvcg_format_get_default_color_match(streaming);
	if (!color_match)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	h->desc.bLength			= UVC_DT_FORMAT_UNCOMPRESSED_SIZE;
	h->desc.bDescriptorType		= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED;
	h->desc.bFormatIndex		= 0;
	h->desc.bNumFrameDescriptors	= 0;
	memcpy(h->desc.guidFormat, guid, sizeof(guid));
	h->desc.bBitsPerPixel		= 16;
	h->desc.bDefaultFrameIndex	= 1;
	h->desc.bAspectRatioX		= 0;
	h->desc.bAspectRatioY		= 0;
	h->desc.bmInterlaceFlags	= 0;
	h->desc.bCopyProtect		= 0;

	INIT_LIST_HEAD(&h->fmt.frames);
	h->fmt.type = UVCG_UNCOMPRESSED;
	h->fmt.color_matching = color_match;
	h->fmt.strm_idx = idx;
	color_match->refcnt++;
	config_group_init_type_name(&h->fmt.group, name,
				    &uvcg_uncompressed_type);

	return &h->fmt.group;
}

static struct configfs_group_operations uvcg_uncompressed_grp_ops = {
	.make_group		= uvcg_uncompressed_make,
};

static const struct uvcg_config_group_type uvcg_uncompressed_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_uncompressed_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "uncompressed",
	.children = (const struct uvcg_config_group_type*[]) {
		&still_frame_grp_type,
	}
};

static struct configfs_group_operations uvcg_h26x_group_ops = {
	.make_item		= uvcg_frame_make,
	.drop_item		= uvcg_frame_drop,
};
#define UVCG_H26X_ATTR(cname, aname, bits)			\
static ssize_t uvcg_h26x_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct uvcg_h26x *h = to_uvcg_h26x(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &h->fmt.group.cg_subsys->su_mutex;	\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = h->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(h->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t								\
uvcg_h26x_##cname##_store(struct config_item *item,		\
				    const char *page, size_t len)	\
{									\
	struct uvcg_h26x *h = to_uvcg_h26x(item);	\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &h->fmt.group.cg_subsys->su_mutex;	\
	int ret;							\
	u8 num;								\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = h->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	if (h->fmt.linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtou8(page, 0, &num);					\
	if (ret)							\
		goto end;						\
									\
	if (num > 255) {						\
		ret = -EINVAL;						\
		goto end;						\
	}								\
	h->desc.aname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_h26x_, cname, aname);
#ifndef CONFIG_USB_RTSX_UVC_15

UVCG_H26X_ATTR(b_bits_per_pixel, bBitsPerPixel, 8);
UVCG_H26X_ATTR(b_default_frame_index, bDefaultFrameIndex, 8);
UVCG_H26X_ATTR(b_variable_size, bVariableSize, 8);

#undef UVCG_H26X_ATTR
static ssize_t uvcg_h26x_guid_format_show(struct config_item *item,
							char *page)
{
	struct uvcg_h26x *ch = to_uvcg_h26x(item);
	struct f_uvc_opts *opts;
	struct config_item *opts_item;
	struct mutex *su_mutex = &ch->fmt.group.cg_subsys->su_mutex;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = ch->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	memcpy(page, ch->desc.guidFormat, sizeof(ch->desc.guidFormat));
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return sizeof(ch->desc.guidFormat);
}
UVC_ATTR_RO(uvcg_h26x_, guid_format, guidFormat);

static struct configfs_attribute *uvcg_h26x_attrs[] = {
	&uvcg_h26x_attr_guid_format,
	&uvcg_h26x_attr_b_bits_per_pixel,
	&uvcg_h26x_attr_b_default_frame_index,
	&uvcg_h26x_attr_b_variable_size,
	NULL,
};
#else
UVCG_H26X_ATTR(b_default_frame_index, bDefaultFrameIndex, 8);
UVCG_H26X_ATTR(b_max_codec_config_delay, bMaxCodecConfigDelay, 8);
UVCG_H26X_ATTR(bm_supported_slice_modes, bmSupportedSliceModes, 8);
UVCG_H26X_ATTR(bm_supported_sync_frame_types, bmSupportedSyncFrameTypes, 8);
UVCG_H26X_ATTR(b_resolution_scaling, bResolutionScaling, 8);
UVCG_H26X_ATTR(bm_supported_rate_control_modes, bmSupportedRateControlModes, 8);
#undef UVCG_H26X_ATTR

static struct configfs_attribute *uvcg_h26x_attrs[] = {
	&uvcg_h26x_attr_b_default_frame_index,
	&uvcg_h26x_attr_b_max_codec_config_delay,
	&uvcg_h26x_attr_bm_supported_slice_modes,
	&uvcg_h26x_attr_bm_supported_sync_frame_types,
	&uvcg_h26x_attr_b_resolution_scaling,
	&uvcg_h26x_attr_bm_supported_rate_control_modes,
	NULL,
};
#endif

static const struct config_item_type uvcg_h26x_type = {
	.ct_item_ops    = &uvcg_config_item_ops,
	.ct_group_ops	= &uvcg_h26x_group_ops,
	.ct_attrs	= uvcg_h26x_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *uvcg_h26x_make(struct config_group *group,
					const char *name, int flag)
{
	struct uvcg_color_matching *color_match;
	struct config_item *streaming;
	struct uvcg_h26x *h;
	char buf[MAX_NAME_LEN];
	char *idx_str;
	u8 idx;
	int ret = 0;
#ifndef CONFIG_USB_RTSX_UVC_15
	static char guid[20] = {
			'H',  '2',  '6',  'x', 0x00, 0x00, 0x10, 0x00,
			0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

	if (flag == 5)
		guid[3] = '5';
	else
		guid[3] = '4';

#endif

	ret = snprintf(buf, MAX_NAME_LEN, "%s", name);
	if (ret >= MAX_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	idx_str = strchr(buf, '.');
	if (!idx_str) {
		pr_err("Unable to locate . in name.strm_idx\n");
		return ERR_PTR(-EINVAL);
	}
	*idx_str = '\0';
	idx_str++;
	if (!strlen(buf))
		return ERR_PTR(-EINVAL);
	ret = kstrtou8(idx_str, 0, &idx);
	if (ret)
		return ERR_PTR(ret);

	if (idx >= UVC_STREAMING_NUMS)
		return ERR_PTR(-EINVAL);

	streaming = group->cg_item.ci_parent;
	color_match = uvcg_format_get_default_color_match(streaming);
	if (!color_match)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

#ifndef CONFIG_USB_RTSX_UVC_15
	h->desc.bLength			= UVC_DT_FORMAT_H26X_SIZE;
	h->desc.bDescriptorType		= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType	= UVC_VS_FORMAT_FRAME_BASED;
	h->desc.bFormatIndex		= 0;
	h->desc.bNumFrameDescriptors	= 0;
	memcpy(h->desc.guidFormat, guid, sizeof(guid));
	h->desc.bBitsPerPixel		= 16;
	h->desc.bDefaultFrameIndex	= 1;
	h->desc.bVariableSize		= 1;
#else
	h->desc.bLength				= UVC_DT_FORMAT_H26X_SIZE;
	h->desc.bDescriptorType			= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType		= VS_FORMAT_H264;
	h->desc.bFormatIndex			= 0;
	h->desc.bNumFrameDescriptors		= 0;
	h->desc.bDefaultFrameIndex		= 1;
	h->desc.bMaxCodecConfigDelay		= 0x01;
	h->desc.bmSupportedSliceModes		= 0;
	h->desc.bmSupportedSyncFrameTypes	= 0x03;
	h->desc.bResolutionScaling		= 0x03;
	h->desc.Reserved1			= 0x00;
	h->desc.bmSupportedRateControlModes	= H264_RATE_CONTROL_MODE;
	h->desc.wMaxMBperSecOneResolutionNoScalability		= 0x6C00;
	h->desc.wMaxMBperSecTwoResolutionsNoScalability		= 0x0000;
	h->desc.wMaxMBperSecThreeResolutionsNoScalability	= 0x0000;
	h->desc.wMaxMBperSecFourResolutionsNoScalability	= 0x0000;
	h->desc.wMaxMBperSecOneResolutionTemporalScalability	= 0x6C00;
	h->desc.wMaxMBperSecTwoResolutionsTemporalScalablility	= 0x0000;
	h->desc.wMaxMBperSecThreeResolutionsTemporalScalability	= 0x0000;
	h->desc.wMaxMBperSecFourResolutionsTemporalScalability	= 0x0000;
	h->desc.wMaxMBperSecOneResolutionTemporalQualityScalability
								= 0x0000;
	h->desc.wMaxMBperSecTwoResolutionsTemporalQualityScalability
								= 0x0000;
	h->desc.wMaxMBperSecThreeResolutionsTemporalQualityScalability
								= 0x0000;
	h->desc.wMaxMBperSecFourResolutionsTemporalQualityScalability
								= 0x0000;
	h->desc.wMaxMBperSecOneResolutionTemporalSpatialScalability
								= 0x0000;
	h->desc.wMaxMBperSecTwoResolutionsTemporalSpatialScalability
								= 0x0000;
	h->desc.wMaxMBperSecThreeResolutionsTemporalSpatialScalability
								= 0x0000;
	h->desc.wMaxMBperSecFourResolutionsTemporalSpatialScalability
								= 0x0000;
	h->desc.wMaxMBperSecOneResolutionFullScalability	= 0x0000;
	h->desc.wMaxMBperSecTwoResolutionsFullScalability	= 0x0000;
	h->desc.wMaxMBperSecThreeResolutionsFullScalability	= 0x0000;
	h->desc.wMaxMBperSecFourResolutionsFullScalability	= 0x0000;
#endif

	INIT_LIST_HEAD(&h->fmt.frames);
	if (flag == 5)
		h->fmt.type = UVCG_H265;
	else
		h->fmt.type = UVCG_H264;
	h->fmt.color_matching = color_match;
	color_match->refcnt++;
	h->fmt.strm_idx = idx;
	config_group_init_type_name(&h->fmt.group, name,
				    &uvcg_h26x_type);

	return &h->fmt.group;
}

static struct config_group *uvcg_h264_make(struct config_group *group,
						   const char *name)
{
	return uvcg_h26x_make(group, name, 4);
}

static struct config_group *uvcg_h265_make(struct config_group *group,
						   const char *name)
{
	return uvcg_h26x_make(group, name, 5);
}

static struct configfs_group_operations uvcg_h264_grp_ops = {
	.make_group		= uvcg_h264_make,
};

static const struct uvcg_config_group_type uvcg_h264_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_h264_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "h264",
	.children = (const struct uvcg_config_group_type*[]) {
		&still_frame_grp_type,
	}
};

/* -----------------------------------------------------------------------------
 * streaming/h265/<NAME>
 */

static struct configfs_group_operations uvcg_h265_grp_ops = {
	.make_group		= uvcg_h265_make,
};

static const struct uvcg_config_group_type uvcg_h265_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_h265_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "h265",
	.children = (const struct uvcg_config_group_type*[]) {
		&still_frame_grp_type,
	}
};


/* -----------------------------------------------------------------------------
 * streaming/mjpeg/<NAME>
 */

static struct configfs_group_operations uvcg_mjpeg_group_ops = {
	.make_item		= uvcg_frame_make,
	.drop_item		= uvcg_frame_drop,
};

#define UVCG_MJPEG_ATTR_RO(cname, aname, bits)				\
static ssize_t uvcg_mjpeg_##cname##_show(struct config_item *item, char *page)\
{									\
	struct uvcg_mjpeg *u = to_uvcg_mjpeg(item);			\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(u->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
UVC_ATTR_RO(uvcg_mjpeg_, cname, aname)

#define UVCG_MJPEG_ATTR(cname, aname, bits)				\
static ssize_t uvcg_mjpeg_##cname##_show(struct config_item *item, char *page)\
{									\
	struct uvcg_mjpeg *u = to_uvcg_mjpeg(item);			\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", le##bits##_to_cpu(u->desc.aname));\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t								\
uvcg_mjpeg_##cname##_store(struct config_item *item,			\
			   const char *page, size_t len)		\
{									\
	struct uvcg_mjpeg *u = to_uvcg_mjpeg(item);			\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &u->fmt.group.cg_subsys->su_mutex;	\
	int ret;							\
	u8 num;								\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = u->fmt.group.cg_item.ci_parent->ci_parent->ci_parent;\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	if (u->fmt.linked || opts->refcnt) {				\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtou8(page, 0, &num);					\
	if (ret)							\
		goto end;						\
									\
	/* index values in uvc are never 0 */				\
	if (!num) {							\
		ret = -EINVAL;						\
		goto end;						\
	}								\
									\
	u->desc.aname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(su_mutex);						\
	return ret;							\
}									\
									\
UVC_ATTR(uvcg_mjpeg_, cname, aname)

UVCG_MJPEG_ATTR_RO(b_format_index, bFormatIndex, 8);
UVCG_MJPEG_ATTR(b_default_frame_index, bDefaultFrameIndex, 8);
UVCG_MJPEG_ATTR_RO(bm_flags, bmFlags, 8);
UVCG_MJPEG_ATTR_RO(b_aspect_ratio_x, bAspectRatioX, 8);
UVCG_MJPEG_ATTR_RO(b_aspect_ratio_y, bAspectRatioY, 8);
UVCG_MJPEG_ATTR_RO(bm_interlace_flags, bmInterlaceFlags, 8);

#undef UVCG_MJPEG_ATTR
#undef UVCG_MJPEG_ATTR_RO

static inline ssize_t
uvcg_mjpeg_bma_controls_show(struct config_item *item, char *page)
{
	struct uvcg_mjpeg *u = to_uvcg_mjpeg(item);
	return uvcg_format_bma_controls_show(&u->fmt, page);
}

static inline ssize_t
uvcg_mjpeg_bma_controls_store(struct config_item *item,
				     const char *page, size_t len)
{
	struct uvcg_mjpeg *u = to_uvcg_mjpeg(item);
	return uvcg_format_bma_controls_store(&u->fmt, page, len);
}

UVC_ATTR(uvcg_mjpeg_, bma_controls, bmaControls);

static struct configfs_attribute *uvcg_mjpeg_attrs[] = {
	&uvcg_mjpeg_attr_b_format_index,
	&uvcg_mjpeg_attr_b_default_frame_index,
	&uvcg_mjpeg_attr_bm_flags,
	&uvcg_mjpeg_attr_b_aspect_ratio_x,
	&uvcg_mjpeg_attr_b_aspect_ratio_y,
	&uvcg_mjpeg_attr_bm_interlace_flags,
	&uvcg_mjpeg_attr_bma_controls,
	NULL,
};

static const struct config_item_type uvcg_mjpeg_type = {
	.ct_item_ops	= &uvcg_format_item_operations,
	.ct_group_ops	= &uvcg_mjpeg_group_ops,
	.ct_attrs	= uvcg_mjpeg_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *uvcg_mjpeg_make(struct config_group *group,
						   const char *name)
{
	struct uvcg_color_matching *color_match;
	struct config_item *streaming;
	struct uvcg_mjpeg *h;
	char buf[MAX_NAME_LEN];
	char *idx_str;
	u8 idx;
	int ret = 0;

	ret = snprintf(buf, MAX_NAME_LEN, "%s", name);
	if (ret >= MAX_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	idx_str = strchr(buf, '.');
	if (!idx_str) {
		pr_err("Unable to locate . in name.strm_idx\n");
		return ERR_PTR(-EINVAL);
	}
	*idx_str = '\0';
	idx_str++;
	if (!strlen(buf))
		return ERR_PTR(-EINVAL);
	ret = kstrtou8(idx_str, 0, &idx);
	if (ret)
		return ERR_PTR(ret);

	if (idx >= UVC_STREAMING_NUMS)
		return ERR_PTR(-EINVAL);

	streaming = group->cg_item.ci_parent;
	color_match = uvcg_format_get_default_color_match(streaming);
	if (!color_match)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	h->desc.bLength			= UVC_DT_FORMAT_MJPEG_SIZE;
	h->desc.bDescriptorType		= USB_DT_CS_INTERFACE;
	h->desc.bDescriptorSubType	= UVC_VS_FORMAT_MJPEG;
	h->desc.bFormatIndex		= 0;
	h->desc.bNumFrameDescriptors	= 0;
	h->desc.bmFlags			= 1;
	h->desc.bDefaultFrameIndex	= 1;
	h->desc.bAspectRatioX		= 0;
	h->desc.bAspectRatioY		= 0;
	h->desc.bmInterlaceFlags	= 0;
	h->desc.bCopyProtect		= 0;

	INIT_LIST_HEAD(&h->fmt.frames);
	h->fmt.type = UVCG_MJPEG;
	h->fmt.color_matching = color_match;
	color_match->refcnt++;
	h->fmt.strm_idx = idx;
	config_group_init_type_name(&h->fmt.group, name,
				    &uvcg_mjpeg_type);

	return &h->fmt.group;
}

static struct configfs_group_operations uvcg_mjpeg_grp_ops = {
	.make_group		= uvcg_mjpeg_make,
};

static const struct uvcg_config_group_type uvcg_mjpeg_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_mjpeg_grp_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "mjpeg",
	.children = (const struct uvcg_config_group_type*[]) {
		&still_frame_grp_type,
	}
};

/* -----------------------------------------------------------------------------
 * streaming/color_matching/default
 */

#define UVCG_COLOR_MATCHING_ATTR(cname, aname, bits)			\
static ssize_t uvcg_color_matching_##cname##_show(			\
	struct config_item *item, char *page)				\
{									\
	struct config_group *group = to_config_group(item);		\
	struct uvcg_color_matching *color_match =			\
		to_uvcg_color_matching(group);				\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	int result;							\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n",					\
			 le##bits##_to_cpu(color_match->desc.aname));	\
	mutex_unlock(&opts->lock);					\
									\
	mutex_unlock(su_mutex);						\
	return result;							\
}									\
									\
static ssize_t uvcg_color_matching_##cname##_store(			\
	struct config_item *item, const char *page, size_t len)		\
{									\
	struct config_group *group = to_config_group(item);		\
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;		\
	struct uvcg_color_matching *color_match =			\
		to_uvcg_color_matching(group);				\
	struct f_uvc_opts *opts;					\
	struct config_item *opts_item;					\
	int ret;							\
	u##bits num;							\
									\
	ret = kstrtou##bits(page, 0, &num);				\
	if (ret)							\
		return ret;						\
									\
	mutex_lock(su_mutex); /* for navigating configfs hierarchy */	\
									\
	if (color_match->refcnt) {					\
		ret = -EBUSY;						\
		goto unlock_su;						\
	}								\
									\
	opts_item = group->cg_item.ci_parent->ci_parent->ci_parent;	\
	opts = to_f_uvc_opts(opts_item);				\
									\
	mutex_lock(&opts->lock);					\
									\
	color_match->desc.aname = num;					\
	ret = len;							\
									\
	mutex_unlock(&opts->lock);					\
unlock_su:								\
	mutex_unlock(su_mutex);						\
									\
	return ret;							\
}									\
UVC_ATTR(uvcg_color_matching_, cname, aname)

UVCG_COLOR_MATCHING_ATTR(b_color_primaries, bColorPrimaries, 8);
UVCG_COLOR_MATCHING_ATTR(b_transfer_characteristics, bTransferCharacteristics, 8);
UVCG_COLOR_MATCHING_ATTR(b_matrix_coefficients, bMatrixCoefficients, 8);

#undef UVCG_COLOR_MATCHING_ATTR

static struct configfs_attribute *uvcg_color_matching_attrs[] = {
	&uvcg_color_matching_attr_b_color_primaries,
	&uvcg_color_matching_attr_b_transfer_characteristics,
	&uvcg_color_matching_attr_b_matrix_coefficients,
	NULL,
};

static void uvcg_color_matching_release(struct config_item *item)
{
	struct uvcg_color_matching *color_match =
		to_uvcg_color_matching(to_config_group(item));

	kfree(color_match);
}

static struct configfs_item_operations uvcg_color_matching_item_ops = {
	.release	= uvcg_color_matching_release,
};

static const struct config_item_type uvcg_color_matching_type = {
	.ct_item_ops	= &uvcg_color_matching_item_ops,
	.ct_attrs	= uvcg_color_matching_attrs,
	.ct_owner	= THIS_MODULE,
};

/* -----------------------------------------------------------------------------
 * streaming/color_matching
 */

static struct config_group *uvcg_color_matching_make(struct config_group *group,
						     const char *name)
{
	struct uvcg_color_matching *color_match;

	color_match = kzalloc(sizeof(*color_match), GFP_KERNEL);
	if (!color_match)
		return ERR_PTR(-ENOMEM);

	color_match->desc.bLength = UVC_DT_COLOR_MATCHING_SIZE;
	color_match->desc.bDescriptorType = USB_DT_CS_INTERFACE;
	color_match->desc.bDescriptorSubType = UVC_VS_COLORFORMAT;

	config_group_init_type_name(&color_match->group, name,
				    &uvcg_color_matching_type);

	return &color_match->group;
}

static struct configfs_group_operations uvcg_color_matching_grp_group_ops = {
	.make_group	= uvcg_color_matching_make,
};

static int uvcg_color_matching_create_children(struct config_group *parent)
{
	struct uvcg_color_matching *color_match;

	color_match = kzalloc(sizeof(*color_match), GFP_KERNEL);
	if (!color_match)
		return -ENOMEM;

	color_match->desc.bLength = UVC_DT_COLOR_MATCHING_SIZE;
	color_match->desc.bDescriptorType = USB_DT_CS_INTERFACE;
	color_match->desc.bDescriptorSubType = UVC_VS_COLORFORMAT;
	color_match->desc.bColorPrimaries = UVC_COLOR_PRIMARIES_BT_709_SRGB;
	color_match->desc.bTransferCharacteristics = UVC_TRANSFER_CHARACTERISTICS_BT_709;
	color_match->desc.bMatrixCoefficients = UVC_MATRIX_COEFFICIENTS_SMPTE_170M;

	config_group_init_type_name(&color_match->group, "default",
				    &uvcg_color_matching_type);
	configfs_add_default_group(&color_match->group, parent);

	return 0;
}

static const struct uvcg_config_group_type uvcg_color_matching_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_group_ops	= &uvcg_color_matching_grp_group_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "color_matching",
	.create_children = uvcg_color_matching_create_children,
};

/* -----------------------------------------------------------------------------
 * streaming/class/{fs|hs|ss}
 */

struct uvcg_streaming_class_group {
	struct config_group group;
	const char *name;
};

static inline struct uvc_descriptor_header
***__uvcg_get_stream_class_arr(struct config_item *i, struct f_uvc_opts *o)
{
	struct uvcg_streaming_class_group *group =
		container_of(i, struct uvcg_streaming_class_group,
			     group.cg_item);

	if (!strcmp(group->name, "fs"))
		return &o->uvc_fs_streaming_cls;

	if (!strcmp(group->name, "hs"))
		return &o->uvc_hs_streaming_cls;

	if (!strcmp(group->name, "ss"))
		return &o->uvc_ss_streaming_cls;

	return NULL;
}

enum uvcg_strm_type {
	UVCG_HEADER = 0,
	UVCG_FORMAT,
	UVCG_FRAME,
	UVCG_STILL_FRAME,
	UVCG_COLOR_MATCHING,
};

/*
 * Iterate over a hierarchy of streaming descriptors' config items.
 * The items are created by the user with configfs.
 *
 * It "processes" the header pointed to by @priv1, then for each format
 * that follows the header "processes" the format itself and then for
 * each frame inside a format "processes" the frame.
 *
 * As a "processing" function the @fun is used.
 *
 * __uvcg_iter_strm_cls() is used in two context: first, to calculate
 * the amount of memory needed for an array of streaming descriptors
 * and second, to actually fill the array.
 *
 * @h: streaming header pointer
 * @priv2: an "inout" parameter (the caller might want to see the changes to it)
 * @priv3: an "inout" parameter (the caller might want to see the changes to it)
 * @fun: callback function for processing each level of the hierarchy
 */
static int __uvcg_iter_strm_cls(struct uvcg_streaming_header *h,
	void *priv2, void *priv3, void *priv4,
	int (*fun)(void *, void *, void *, void *, int, int,
			enum uvcg_strm_type type))
{
	struct uvcg_format_ptr *f;
	struct config_group *grp;
	struct config_item *item;
	struct uvcg_frame *frm;
	struct f_uvc_opts *opts = priv4;
	int ret, i, j;
	int strm = 0;

	if (!fun)
		return -EINVAL;

	i = j = 0;
	ret = fun(h, priv2, priv3, priv4, 0, strm, UVCG_HEADER);
	if (ret)
		return ret;
	list_for_each_entry(f, &h->formats, entry) {
		j = 0;
		if (f->fmt->strm_idx != strm) {
			i = 0;
			ret = fun(h, priv2, priv3, priv4, 0,
					f->fmt->strm_idx, UVCG_HEADER);
			if (ret)
				return ret;
			strm = f->fmt->strm_idx;
		}

		ret = fun(f->fmt, priv2, priv3, priv4, i++, strm, UVCG_FORMAT);
		if (ret)
			return ret;
		grp = &f->fmt->group;
		list_for_each_entry(item, &grp->cg_children, ci_entry) {
			frm = to_uvcg_frame(item);
			ret = fun(frm, priv2, priv3, priv4,
					j++, strm, UVCG_FRAME);
			if (ret)
				return ret;
		}
		if (!f->fmt->strm_idx) {
			ret = fun(&opts->uvc_still_frame[f->fmt->type],
					priv2, priv3, priv4, f->fmt->type,
					strm, UVCG_STILL_FRAME);
			if (ret)
				return ret;
		}
		ret = fun(f->fmt->color_matching, priv2, priv3, priv4,
				j++, strm, UVCG_COLOR_MATCHING);
		if (ret)
			return ret;
	}

	return ret;
}

/*
 * Count how many bytes are needed for an array of streaming descriptors.
 *
 * @priv1: pointer to a header, format or frame
 * @priv2: inout parameter, accumulated size of the array
 * @priv3: inout parameter, accumulated number of the array elements
 * @n: unused, this function's prototype must match @fun in __uvcg_iter_strm_cls
 */
static int __uvcg_cnt_strm(void *priv1, void *priv2, void *priv3,
			void *priv4, int n, int strm_idx,
			enum uvcg_strm_type type)
{
	size_t *size = priv2;
	size_t *count = priv3;
	struct f_uvc_opts *opts = priv4;
	int sz;

	switch (type) {
	case UVCG_HEADER: {
		struct uvcg_streaming_header *h = priv1;

		*size += sizeof(h->desc);
		/* bmaControls */
		*size += h->num_fmt[strm_idx] * UVCG_STREAMING_CONTROL_SIZE;
	}
	break;
	case UVCG_FORMAT: {
		struct uvcg_format *fmt = priv1;

		if (fmt->type == UVCG_UNCOMPRESSED ||
			fmt->type == UVCG_UNCOMPRESSED_NV12) {
			struct uvcg_uncompressed *u =
				container_of(fmt, struct uvcg_uncompressed,
					     fmt);

			*size += sizeof(u->desc);
		} else if (fmt->type == UVCG_MJPEG) {
			struct uvcg_mjpeg *m =
				container_of(fmt, struct uvcg_mjpeg, fmt);

			*size += sizeof(m->desc);
		} else if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265) {
			struct uvcg_h26x *f =
				container_of(fmt, struct uvcg_h26x, fmt);

			*size += sizeof(f->desc);
		} else {
			return -EINVAL;
		}
	}
	break;
	case UVCG_FRAME: {
		struct uvcg_frame *frm = priv1;
		sz = sizeof(frm->dw_frame_interval);

		if (frm->fmt_type == UVCG_H264 || frm->fmt_type == UVCG_H265) {
			*size += sizeof(frm->h26x_frame);
			*size += frm->h26x_frame.b_frame_interval_type * sz;
		} else {
			*size += sizeof(frm->frame);
			*size += frm->frame.b_frame_interval_type * sz;
		}
	}
	break;
	case UVCG_STILL_FRAME:
		if (opts->uvc_still_frame[n].wWidth_wHeight[0] == 0) {
			--*count;
			break;
		}
		sz = sizeof(opts->uvc_still_frame[n].wWidth_wHeight);
		*size += sizeof(opts->uvc_still_frame[n]);
		*size += opts->uvc_still_frame[n].bNumImageSizePatterns * sz;
	break;
	case UVCG_COLOR_MATCHING: {
		struct uvcg_color_matching *color_match = priv1;

		*size += sizeof(color_match->desc);
	}
	break;
	}

	++*count;

	return 0;
}

/*
 * Fill an array of streaming descriptors.
 *
 * @priv1: pointer to a header, format or frame
 * @priv2: inout parameter, pointer into a block of memory
 * @priv3: inout parameter, pointer to a 2-dimensional array
 */
static int __uvcg_fill_strm(void *priv1, void *priv2, void *priv3,
			void *priv4, int n, int strm_idx,
			enum uvcg_strm_type type)
{
	void **dest = priv2;
	struct uvc_descriptor_header ***array = priv3;
	struct f_uvc_opts *opts = priv4;
	size_t sz;

	**array = *dest;
	++*array;

	switch (type) {
	case UVCG_HEADER: {
		struct uvc_input_header_descriptor *ihdr = *dest;
		struct uvcg_streaming_header *h = priv1;
		struct uvcg_format_ptr *f;

		memcpy(*dest, &h->desc, sizeof(h->desc));
		*dest += sizeof(h->desc);
		sz = UVCG_STREAMING_CONTROL_SIZE;
		list_for_each_entry(f, &h->formats, entry) {
			memcpy(*dest, f->fmt->bmaControls, sz);
			*dest += sz;
		}
		ihdr->bLength = sizeof(h->desc) + h->num_fmt[strm_idx] * sz;
		ihdr->bNumFormats = h->num_fmt[strm_idx];
	}
	break;
	case UVCG_FORMAT: {
		struct uvcg_format *fmt = priv1;

		if (fmt->type == UVCG_UNCOMPRESSED ||
			fmt->type == UVCG_UNCOMPRESSED_NV12) {
			struct uvcg_uncompressed *u =
				container_of(fmt, struct uvcg_uncompressed,
					     fmt);

			u->desc.bFormatIndex = n + 1;
			u->desc.bNumFrameDescriptors = fmt->num_frames;
			memcpy(*dest, &u->desc, sizeof(u->desc));
			*dest += sizeof(u->desc);
		} else if (fmt->type == UVCG_MJPEG) {
			struct uvcg_mjpeg *m =
				container_of(fmt, struct uvcg_mjpeg, fmt);

			m->desc.bFormatIndex = n + 1;
			m->desc.bNumFrameDescriptors = fmt->num_frames;
			memcpy(*dest, &m->desc, sizeof(m->desc));
			*dest += sizeof(m->desc);
		} else if (fmt->type == UVCG_H264 || fmt->type == UVCG_H265) {
			struct uvcg_h26x *f =
				container_of(fmt, struct uvcg_h26x, fmt);

			f->desc.bFormatIndex = n + 1;
			f->desc.bNumFrameDescriptors = fmt->num_frames;
			memcpy(*dest, &f->desc, sizeof(f->desc));
			*dest += sizeof(f->desc);
		} else {
			return -EINVAL;
		}
	}
	break;
	case UVCG_FRAME: {
		struct uvcg_frame *frm = priv1;
		struct uvc_descriptor_header *h = *dest;

		if (frm->fmt_type == UVCG_H264 || frm->fmt_type == UVCG_H265) {
			sz = sizeof(frm->h26x_frame);
			frm->h26x_frame.b_frame_index = n + 1;
			memcpy(*dest, &frm->h26x_frame, sz);
			*dest += sz;
			sz = frm->h26x_frame.b_frame_interval_type *
				sizeof(*frm->dw_frame_interval);
		} else {
			sz = sizeof(frm->frame);
			frm->frame.b_frame_index = n + 1;
			memcpy(*dest, &frm->frame, sz);
			*dest += sz;
			sz = frm->frame.b_frame_interval_type *
				sizeof(*frm->dw_frame_interval);
		}
		memcpy(*dest, frm->dw_frame_interval, sz);
		*dest += sz;
		if (frm->fmt_type == UVCG_UNCOMPRESSED)
			h->bLength = UVC_DT_FRAME_UNCOMPRESSED_SIZE(
				frm->frame.b_frame_interval_type);
		else if (frm->fmt_type == UVCG_MJPEG)
			h->bLength = UVC_DT_FRAME_MJPEG_SIZE(
				frm->frame.b_frame_interval_type);
		else if (frm->fmt_type == UVCG_H264 ||
				frm->fmt_type == UVCG_H265)
			h->bLength = UVC_DT_FRAME_H26X_SIZE(
				frm->h26x_frame.b_frame_interval_type);
	}
	break;
	case UVCG_STILL_FRAME: {
		if (opts) {
			if (opts->uvc_still_frame[n].wWidth_wHeight[0] == 0) {
				--*array;
				break;
			}
			opts->uvc_still_frame[n].bLength =
			UVCG_STILL_FRAME_DESC_SIZE(
			opts->uvc_still_frame[n].bNumImageSizePatterns,
			opts->uvc_still_frame[n].bNumCompressionPattern);

			memcpy(*dest, &opts->uvc_still_frame[n],
				sizeof(opts->uvc_still_frame[n]));
			*dest += sizeof(opts->uvc_still_frame[n]);
		}
	}
	break;
	case UVCG_COLOR_MATCHING: {
		struct uvcg_color_matching *color_match = priv1;

		memcpy(*dest, &color_match->desc, sizeof(color_match->desc));
		*dest += sizeof(color_match->desc);
	}
	break;
	}

	return 0;
}

static u32 __uvcg_fill_still_image_flag(struct f_uvc_opts *opts,
	int fmt_type, u16 width, u16 height)
{
	int sz = 0, i = 0;
	u32 result = 0;

	sz = sizeof(opts->uvc_still_frame[fmt_type].wWidth_wHeight);
	for (i = 0; i < sz - 1; i++) {
		if (opts->uvc_still_frame[fmt_type].wWidth_wHeight[i] == width &&
			opts->uvc_still_frame[fmt_type].wWidth_wHeight[++i] == height)
			result = 1;
	}
	return result;
}

static int __uvcg_fill_rtsx_fmt(struct uvcg_streaming_header *h,
	struct f_uvc_opts *opts)
{
	struct uvcg_format_ptr *f;
	struct config_group *grp;
	struct config_item *item;
	struct uvcg_frame *frm;
	int fmt_idx = 0;
	int strm_idx = -1;
	u32 fmt;
	u32 still_image = 0;

	list_for_each_entry(f, &h->formats, entry) {
		grp = &f->fmt->group;
		if (strm_idx != f->fmt->strm_idx) {
			fmt_idx = 0;
			strm_idx = f->fmt->strm_idx;
		}

		if (f->fmt->type == UVCG_UNCOMPRESSED)
			fmt = V4L2_PIX_FMT_YUYV;
		else if (f->fmt->type == UVCG_UNCOMPRESSED_NV12)
			fmt = V4L2_PIX_FMT_NV12;
		else if (f->fmt->type == UVCG_MJPEG)
			fmt = V4L2_PIX_FMT_MJPEG;
		else if (f->fmt->type == UVCG_H264)
			fmt = V4L2_PIX_FMT_H264;
		else if (f->fmt->type == UVCG_H265)
			fmt = V4L2_PIX_FMT_HEVC;
		else
			return -EINVAL;

		list_for_each_entry(item, &grp->cg_children, ci_entry) {
			frm = to_uvcg_frame(item);
			if (f->fmt->type == UVCG_H264 ||
					f->fmt->type == UVCG_H265) {
				still_image = __uvcg_fill_still_image_flag(opts,
					f->fmt->type, frm->h26x_frame.w_width,
					frm->h26x_frame.w_height);
				rtsx_fill_format_map(f->fmt->strm_idx,
						fmt_idx, fmt,
						frm->h26x_frame.w_width,
						frm->h26x_frame.w_height,
						still_image);
			} else {
				still_image = __uvcg_fill_still_image_flag(opts,
					f->fmt->type, frm->frame.w_width,
					frm->frame.w_height);
				rtsx_fill_format_map(f->fmt->strm_idx,
						fmt_idx, fmt,
						frm->frame.w_width,
						frm->frame.w_height,
						still_image);
			}
		}
		fmt_idx++;
	}

	return 0;
}

static int uvcg_streaming_class_allow_link(struct config_item *src,
					   struct config_item *target)
{
	struct config_item *streaming, *header;
	struct f_uvc_opts *opts;
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvc_descriptor_header ***class_array, **cl_arr;
	struct uvcg_streaming_header *target_hdr;
	void *data, *data_save;
	size_t size = 0, count = 0;
	int ret = -EINVAL;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	streaming = src->ci_parent->ci_parent;
	header = config_group_find_item(to_config_group(streaming), "header");
	if (!header || target->ci_parent != header)
		goto out;

	opts = to_f_uvc_opts(streaming->ci_parent);

	mutex_lock(&opts->lock);

	class_array = __uvcg_get_stream_class_arr(src, opts);
	if (!class_array || *class_array || opts->refcnt) {
		ret = -EBUSY;
		goto unlock;
	}

	target_hdr = to_uvcg_streaming_header(target);
	ret = __uvcg_iter_strm_cls(target_hdr, &size, &count, opts,
			__uvcg_cnt_strm);
	if (ret)
		goto unlock;

	count += 1; /* still image, NULL */
	*class_array = kcalloc(count, sizeof(void *), GFP_KERNEL);
	if (!*class_array) {
		ret = -ENOMEM;
		goto unlock;
	}

	data = data_save = kzalloc(size, GFP_KERNEL);
	if (!data) {
		kfree(*class_array);
		*class_array = NULL;
		ret = -ENOMEM;
		goto unlock;
	}
	cl_arr = *class_array;
	ret = __uvcg_iter_strm_cls(target_hdr, &data, &cl_arr,
			opts, __uvcg_fill_strm);
	if (ret) {
		kfree(*class_array);
		*class_array = NULL;
		/*
		 * __uvcg_fill_strm() called from __uvcg_iter_stream_cls()
		 * might have advanced the "data", so use a backup copy
		 */
		kfree(data_save);
		goto unlock;
	}
	if (!target_hdr->linked)
		__uvcg_fill_rtsx_fmt(target_hdr, opts);

	++target_hdr->linked;
	ret = 0;

unlock:
	mutex_unlock(&opts->lock);
out:
	config_item_put(header);
	mutex_unlock(su_mutex);
	return ret;
}

static void uvcg_streaming_class_drop_link(struct config_item *src,
					  struct config_item *target)
{
	struct config_item *streaming, *header;
	struct f_uvc_opts *opts;
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct uvc_descriptor_header ***class_array;
	struct uvcg_streaming_header *target_hdr;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	streaming = src->ci_parent->ci_parent;
	header = config_group_find_item(to_config_group(streaming), "header");
	if (!header || target->ci_parent != header)
		goto out;

	opts = to_f_uvc_opts(streaming->ci_parent);

	mutex_lock(&opts->lock);

	class_array = __uvcg_get_stream_class_arr(src, opts);
	if (!class_array || !*class_array)
		goto unlock;

	if (opts->refcnt)
		goto unlock;

	target_hdr = to_uvcg_streaming_header(target);
	--target_hdr->linked;
	kfree(**class_array);
	kfree(*class_array);
	*class_array = NULL;

unlock:
	mutex_unlock(&opts->lock);
out:
	config_item_put(header);
	mutex_unlock(su_mutex);
}

static struct configfs_item_operations uvcg_streaming_class_item_ops = {
	.release	= uvcg_config_item_release,
	.allow_link	= uvcg_streaming_class_allow_link,
	.drop_link	= uvcg_streaming_class_drop_link,
};

static const struct config_item_type uvcg_streaming_class_type = {
	.ct_item_ops	= &uvcg_streaming_class_item_ops,
	.ct_owner	= THIS_MODULE,
};

/* -----------------------------------------------------------------------------
 * streaming/class
 */

static int uvcg_streaming_class_create_children(struct config_group *parent)
{
	static const char * const names[] = { "fs", "hs", "ss" };
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(names); ++i) {
		struct uvcg_streaming_class_group *group;

		group = kzalloc(sizeof(*group), GFP_KERNEL);
		if (!group)
			return -ENOMEM;

		group->name = names[i];

		config_group_init_type_name(&group->group, group->name,
					    &uvcg_streaming_class_type);
		configfs_add_default_group(&group->group, parent);
	}

	return 0;
}

static const struct uvcg_config_group_type uvcg_streaming_class_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_owner	= THIS_MODULE,
	},
	.name = "class",
	.create_children = uvcg_streaming_class_create_children,
};

/* -----------------------------------------------------------------------------
 * streaming
 */

static ssize_t uvcg_default_streaming_b_interface_number_show(
	struct config_item *item, char *page)
{
	struct config_group *group = to_config_group(item);
	struct mutex *su_mutex = &group->cg_subsys->su_mutex;
	struct config_item *opts_item;
	struct f_uvc_opts *opts;
	int result = 0;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	opts_item = item->ci_parent;
	opts = to_f_uvc_opts(opts_item);

	mutex_lock(&opts->lock);
	result += sprintf(page, "%u\n", opts->streaming_interface);
	mutex_unlock(&opts->lock);

	mutex_unlock(su_mutex);

	return result;
}

UVC_ATTR_RO(uvcg_default_streaming_, b_interface_number, bInterfaceNumber);

static struct configfs_attribute *uvcg_default_streaming_attrs[] = {
	&uvcg_default_streaming_attr_b_interface_number,
	NULL,
};

static const struct uvcg_config_group_type uvcg_streaming_grp_type = {
	.type = {
		.ct_item_ops	= &uvcg_config_item_ops,
		.ct_attrs	= uvcg_default_streaming_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "streaming",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_streaming_header_grp_type,
		&uvcg_uncompressed_grp_type,
		&uvcg_mjpeg_grp_type,
		&uvcg_h264_grp_type,
		&uvcg_h265_grp_type,
		&uvcg_color_matching_grp_type,
		&uvcg_streaming_class_grp_type,
		NULL,
	},
};

/* -----------------------------------------------------------------------------
 * UVC function
 */

static void uvc_func_item_release(struct config_item *item)
{
	struct f_uvc_opts *opts = to_f_uvc_opts(item);

	uvcg_config_remove_children(to_config_group(item));
	usb_put_function_instance(&opts->func_inst);
}

static int uvc_func_allow_link(struct config_item *src, struct config_item *tgt)
{
	struct mutex *su_mutex = &src->ci_group->cg_subsys->su_mutex;
	struct gadget_string *string;
	struct config_item *strings;
	struct f_uvc_opts *opts;
	int ret = 0;

	mutex_lock(su_mutex); /* for navigating configfs hierarchy */

	/* Validate that the target is an entry in strings/<langid> */
	strings = config_group_find_item(to_config_group(src->ci_parent->ci_parent),
					 "strings");
	if (!strings || tgt->ci_parent->ci_parent != strings) {
		ret = -EINVAL;
		goto put_strings;
	}

	string = to_gadget_string(tgt);

	opts = to_f_uvc_opts(src);
	mutex_lock(&opts->lock);

	if (!strcmp(tgt->ci_name, "iad_desc"))
		opts->iad_index = string->usb_string.id;
	else if (!strcmp(tgt->ci_name, "vs0_desc"))
		opts->vs0_index = string->usb_string.id;
	else if (!strcmp(tgt->ci_name, "vs1_desc"))
		opts->vs1_index = string->usb_string.id;
	else
		ret = -EINVAL;

	mutex_unlock(&opts->lock);

put_strings:
	config_item_put(strings);
	mutex_unlock(su_mutex);

	return ret;
}

static void uvc_func_drop_link(struct config_item *src, struct config_item *tgt)
{
	struct f_uvc_opts *opts;

	opts = to_f_uvc_opts(src);
	mutex_lock(&opts->lock);

	if (!strcmp(tgt->ci_name, "iad_desc"))
		opts->iad_index = 0;
	else if (!strcmp(tgt->ci_name, "vs0_desc"))
		opts->vs0_index = 0;
	else if (!strcmp(tgt->ci_name, "vs1_desc"))
		opts->vs1_index = 0;

	mutex_unlock(&opts->lock);
}

static struct configfs_item_operations uvc_func_item_ops = {
	.release	= uvc_func_item_release,
	.allow_link	= uvc_func_allow_link,
	.drop_link	= uvc_func_drop_link,
};

#define UVCG_OPTS_ATTR(cname, aname, limit)				\
static ssize_t f_uvc_opts_##cname##_show(				\
	struct config_item *item, char *page)				\
{									\
	struct f_uvc_opts *opts = to_f_uvc_opts(item);			\
	int result;							\
									\
	mutex_lock(&opts->lock);					\
	result = sprintf(page, "%u\n", opts->cname);			\
	mutex_unlock(&opts->lock);					\
									\
	return result;							\
}									\
									\
static ssize_t								\
f_uvc_opts_##cname##_store(struct config_item *item,			\
			   const char *page, size_t len)		\
{									\
	struct f_uvc_opts *opts = to_f_uvc_opts(item);			\
	unsigned int num;						\
	int ret;							\
									\
	mutex_lock(&opts->lock);					\
	if (opts->refcnt) {						\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtouint(page, 0, &num);				\
	if (ret)							\
		goto end;						\
									\
	if (num > limit) {						\
		ret = -EINVAL;						\
		goto end;						\
	}								\
	opts->cname = num;						\
	ret = len;							\
end:									\
	mutex_unlock(&opts->lock);					\
	return ret;							\
}									\
									\
UVC_ATTR(f_uvc_opts_, cname, cname)

UVCG_OPTS_ATTR(streaming_interval, streaming_interval, 16);
UVCG_OPTS_ATTR(streaming_maxpacket, streaming_maxpacket, 3072);
UVCG_OPTS_ATTR(streaming_maxburst, streaming_maxburst, 15);

#undef UVCG_OPTS_ATTR

#define UVCG_OPTS_STRING_ATTR(cname, aname)				\
static ssize_t f_uvc_opts_string_##cname##_show(struct config_item *item,\
					 char *page)			\
{									\
	struct f_uvc_opts *opts = to_f_uvc_opts(item);			\
	int result;							\
									\
	mutex_lock(&opts->lock);					\
	result = snprintf(page, sizeof(opts->aname), "%s", opts->aname);\
	mutex_unlock(&opts->lock);					\
									\
	return result;							\
}									\
									\
static ssize_t f_uvc_opts_string_##cname##_store(struct config_item *item,\
					  const char *page, size_t len)	\
{									\
	struct f_uvc_opts *opts = to_f_uvc_opts(item);			\
	int size = min(sizeof(opts->aname), len + 1);			\
	int ret = 0;							\
									\
	mutex_lock(&opts->lock);					\
	if (opts->refcnt) {						\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = strscpy(opts->aname, page, size);				\
	if (ret == -E2BIG)						\
		ret = size - 1;						\
									\
end:									\
	mutex_unlock(&opts->lock);					\
	return ret;							\
}									\
									\
UVC_ATTR(f_uvc_opts_string_, cname, aname)

UVCG_OPTS_STRING_ATTR(function_name, function_name);

#undef UVCG_OPTS_STRING_ATTR

static struct configfs_attribute *uvc_attrs[] = {
	&f_uvc_opts_attr_streaming_interval,
	&f_uvc_opts_attr_streaming_maxpacket,
	&f_uvc_opts_attr_streaming_maxburst,
	&f_uvc_opts_string_attr_function_name,
	NULL,
};

static const struct uvcg_config_group_type uvc_func_type = {
	.type = {
		.ct_item_ops	= &uvc_func_item_ops,
		.ct_attrs	= uvc_attrs,
		.ct_owner	= THIS_MODULE,
	},
	.name = "",
	.children = (const struct uvcg_config_group_type*[]) {
		&uvcg_control_grp_type,
		&uvcg_streaming_grp_type,
		NULL,
	},
};

int uvcg_attach_configfs(struct f_uvc_opts *opts)
{
	int ret;

	config_group_init_type_name(&opts->func_inst.group, uvc_func_type.name,
				    &uvc_func_type.type);

	ret = uvcg_config_create_children(&opts->func_inst.group,
					  &uvc_func_type);
	if (ret < 0)
		config_group_put(&opts->func_inst.group);

	return ret;
}
