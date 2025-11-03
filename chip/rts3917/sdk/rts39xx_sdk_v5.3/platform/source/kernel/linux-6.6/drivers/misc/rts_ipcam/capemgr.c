// SPDX-License-Identifier: GPL-2.0
/* Realtek cape manager driver
 *
 * based on https://lore.kernel.org/patchwork/patch/350014/
 *
 * Copyright(c) 2019-2020 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Rui Feng <rui_feng@realsil.com.cn>
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/memory.h>
#include <linux/kthread.h>

struct rts_capemgr_info;

struct slot_attribute {
	struct device_attribute devattr;
	unsigned int field;
	struct rts_cape_slot *slot;	/* this is filled when instantiated */
};
#define to_slot_attribute(x) \
	container_of((x), struct slot_attribute, devattr)

struct rts_cape_slot {
	struct list_head	node;
	struct rts_capemgr_info *info;
	unsigned long			slotno;
	unsigned int		probed : 1;
	char			text_id[256];
	char			signature[128];
	/* quick access */
	char			part_number[32+1];
	char			version[32+1];

	/* attribute group */
	char			*attr_name;
	int			attrs_count;
	struct slot_attribute *attrs;
	struct attribute	**attrs_tab;
	struct attribute_group	attrgroup;

	unsigned int		loading : 1;
	unsigned int		loaded : 1;
	char			*dtbo;
	const struct firmware	*fw;
	struct device_node	*overlay;
	int			ov_id;

	/* loader thread */
	struct task_struct	*loader_thread;
};

struct rts_capemap {
	struct list_head node;
	char *part_number;
	struct device_node *map_node;
};

struct rts_capemgr_info {
	struct platform_device	*pdev;

	atomic_t next_slot_nr;
	struct list_head	slot_list;
	struct mutex		slots_list_mutex;

	int capemaps_nr;
	struct list_head	capemap_list;
	struct mutex		capemap_mutex;
};

static int rts_slot_fill_override(struct rts_cape_slot *slot,
		struct device_node *node,
		const char *part_number, const char *version);
static struct rts_cape_slot *rts_capemgr_add_slot(
		struct rts_capemgr_info *info, struct device_node *node,
		const char *part_number, const char *version);
static int rts_capemgr_load(struct rts_cape_slot *slot);
static int rts_capemgr_unload(struct rts_cape_slot *slot);

/* cape field definitions */
#define CAPE_FIELD_PART_NUMBER	0
#define CAPE_FIELD_VERSION		1


struct cape_field {
	const char	*name;
	int		start;
	int		size;
	unsigned int	ascii : 1;
	unsigned int	strip_trailing_dots : 1;
	const char	*override;
};

/* cape definitions */
static const struct cape_field cape_sig_fields[] = {
	[CAPE_FIELD_PART_NUMBER] = {
		.name		= "part-number",
		.start		= 0,
		.size		= 32,
		.ascii		= 1,
		.strip_trailing_dots = 1,
		.override	= "Override Part#",
	},
	[CAPE_FIELD_VERSION] = {
		.name		= "version",
		.start		= 32,
		.size		= 32,
		.ascii		= 1,
		.override	= "00A0",
	},
};

static char *field_get(const struct cape_field *sig_field,
		const void *data, int field, char *buf, int bufsz)
{
	int len;

	/* enough space? */
	if (bufsz < sig_field->size + sig_field->ascii)
		return NULL;

	memcpy(buf, (char *)data + sig_field->start, sig_field->size);

	/* terminate ascii field */
	if (sig_field->ascii)
		buf[sig_field->size] = '\0';

	if (sig_field->strip_trailing_dots) {
		len = strlen(buf);
		while (len > 1 && buf[len - 1] == '.')
			buf[--len] = '\0';
	}

	return buf;
}

char *cape_field_get(const void *data,
		int field, char *buf, int bufsz)
{
	if ((unsigned int)field >= ARRAY_SIZE(cape_sig_fields))
		return NULL;

	return field_get(&cape_sig_fields[field], data, field, buf, bufsz);
}

#ifdef CONFIG_OF
static const struct of_device_id rts_capemgr_of_match[] = {
	{
		.compatible = "realtek,rts-capemgr",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, rts_capemgr_of_match);

#endif

static int rts_slot_scan(struct rts_cape_slot *slot)
{
	const u8 *p;

	if (slot->probed)
		return 0;

	slot->probed = 1;

	p = slot->signature;

	cape_field_get(slot->signature,
			CAPE_FIELD_PART_NUMBER,
			slot->part_number, sizeof(slot->part_number));
	cape_field_get(slot->signature,
			CAPE_FIELD_VERSION,
			slot->version, sizeof(slot->version));

	/* part_number,version */
	snprintf(slot->text_id, sizeof(slot->text_id) - 1,
			"%s,%s", slot->part_number, slot->version);

	/* terminate always */
	slot->text_id[sizeof(slot->text_id) - 1] = '\0';

	return 0;
}

static ssize_t slot_attr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct slot_attribute *slot_attr = to_slot_attribute(attr);
	struct rts_cape_slot *slot = slot_attr->slot;
	const struct cape_field *sig_field;
	int len;
	char *p, *s;

	/* add newline for ascii fields */
	sig_field = &cape_sig_fields[slot_attr->field];

	len = sig_field->size + sig_field->ascii;
	p = kmalloc(len, GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	s = cape_field_get(slot->signature, slot_attr->field, p, len);
	if (s == NULL)
		return -EINVAL;

	/* add newline for ascii fields and return */
	if (sig_field->ascii) {
		len = sprintf(buf, "%s\n", s);
		goto out;
	} else {
		*buf = '\0';
		len = 0;
	}

out:
	kfree(p);

	return len;
}

#define SLOT_ATTR(_name, _field) \
	{ \
		.devattr = __ATTR(_name, 0440, slot_attr_show, NULL), \
		.field = CAPE_FIELD_##_field, \
		.slot = NULL, \
	}

static const struct slot_attribute slot_attrs[] = {
	SLOT_ATTR(part-number, PART_NUMBER),
	SLOT_ATTR(version, VERSION),
};

static int rts_cape_slot_sysfs_register(struct rts_cape_slot *slot)
{
	struct rts_capemgr_info *info = slot->info;
	struct device *dev = &info->pdev->dev;
	struct slot_attribute *slot_attr;
	struct attribute_group *attrgroup;
	int i, err, sz;

	slot->attr_name = kasprintf(GFP_KERNEL, "slot-%lu", slot->slotno);
	if (slot->attr_name == NULL) {
		dev_err(dev, "slot #%lu: Failed to allocate attr_name\n",
				slot->slotno);
		err = -ENOMEM;
		goto err_fail_no_attr_name;
	}

	slot->attrs_count = ARRAY_SIZE(slot_attrs);

	sz = slot->attrs_count * sizeof(*slot->attrs);
	slot->attrs = kmalloc(sz, GFP_KERNEL);
	if (slot->attrs == NULL) {
		dev_err(dev, "slot #%lu: Failed to allocate attrs\n",
				slot->slotno);
		err = -ENOMEM;
		goto err_fail_no_attrs;
	}

	attrgroup = &slot->attrgroup;
	memset(attrgroup, 0, sizeof(*attrgroup));
	attrgroup->name = slot->attr_name;

	sz = sizeof(*slot->attrs_tab) * (slot->attrs_count + 1);
	attrgroup->attrs = kmalloc(sz, GFP_KERNEL);
	if (attrgroup->attrs == NULL) {
		dev_err(dev, "slot #%lu: Failed to allocate attrs_tab\n",
				slot->slotno);
		err = -ENOMEM;
		goto err_fail_no_attrs_tab;
	}
	/* copy everything over */
	memcpy(slot->attrs, slot_attrs, sizeof(slot_attrs));

	/* bind this attr to the slot */
	for (i = 0; i < slot->attrs_count; i++) {
		slot_attr = &slot->attrs[i];
		slot_attr->slot = slot;
		attrgroup->attrs[i] = &slot_attr->devattr.attr;
	}
	attrgroup->attrs[i] = NULL;

	err = sysfs_create_group(&dev->kobj, attrgroup);
	if (err != 0) {
		dev_err(dev, "slot #%lu: Failed to allocate attrs_tab\n",
				slot->slotno);
		err = -ENOMEM;
		goto err_fail_no_attrs_group;
	}

	return 0;

err_fail_no_attrs_group:
	kfree(slot->attrs_tab);
err_fail_no_attrs_tab:
	kfree(slot->attrs);
err_fail_no_attrs:
	kfree(slot->attr_name);
err_fail_no_attr_name:
	return err;
}

static void rts_cape_slot_sysfs_unregister(struct rts_cape_slot *slot)
{
	struct rts_capemgr_info *info = slot->info;
	struct device *dev = &info->pdev->dev;

	sysfs_remove_group(&dev->kobj, &slot->attrgroup);
	kfree(slot->attrs_tab);
	kfree(slot->attrs);
	kfree(slot->attr_name);
}

static ssize_t slots_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_capemgr_info *info = platform_get_drvdata(pdev);
	struct rts_cape_slot *slot;
	ssize_t len, sz;

	mutex_lock(&info->slots_list_mutex);
	sz = 0;
	list_for_each_entry(slot, &info->slot_list, node) {

		len = sprintf(buf, "%2lu: %c%c%c %s\n",
				slot->slotno,
				slot->probed       ? 'P' : '-',
				slot->loading	   ? 'l' : '-',
				slot->loaded	   ? 'L' : '-',
				slot->text_id);

		buf += len;
		sz += len;
	}
	mutex_unlock(&info->slots_list_mutex);

	return sz;
}

static ssize_t slots_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_capemgr_info *info = platform_get_drvdata(pdev);
	struct rts_cape_slot *slot;
	char *s, *part_number, *version;
	int ret, found = 0;
	unsigned long slotno;

	/* check for remove slot */
	if (strlen(buf) > 0 && buf[0] == '-') {
		if (kstrtoul(buf + 1, 10, &slotno))
			return -EINVAL;

		/* now load each (take lock to be sure */
		mutex_lock(&info->slots_list_mutex);
		list_for_each_entry(slot, &info->slot_list, node) {
			if (slotno == slot->slotno) {
				found = 1;
				break;
			}
		}
		mutex_unlock(&info->slots_list_mutex);

		if (!found)
			return -ENODEV;

		rts_capemgr_unload(slot);

		return strlen(buf);
	}

	part_number = kstrdup(buf, GFP_KERNEL);
	if (part_number == NULL)
		return -ENOMEM;

	/* remove trailing spaces dots and newlines */
	s = part_number + strlen(part_number);
	while (s > part_number &&
			(isspace(s[-1]) || s[-1] == '\n' || s[-1] == '.'))
		*--s = '\0';

	version = strchr(part_number, ':');
	if (version != NULL)
		*version++ = '\0';

	dev_info(&pdev->dev, "part_number '%s', version '%s'\n",
			part_number, version ? version : "N/A");

	slot = rts_capemgr_add_slot(info, NULL,
				part_number, version);

	if (IS_ERR_OR_NULL(slot)) {
		dev_err(&pdev->dev, "Failed to add slot #%d\n",
			atomic_read(&info->next_slot_nr) - 1);
		ret = slot ? PTR_ERR(slot) : -ENODEV;
		slot = NULL;
		goto err_fail;
	}

	kfree(part_number);

	ret = rts_capemgr_load(slot);

	return ret == 0 ? strlen(buf) : ret;
err_fail:
	kfree(part_number);
	return ret;
}

static DEVICE_ATTR_RW(slots);

static int rts_capemgr_info_sysfs_register(struct rts_capemgr_info *info)
{
	struct device *dev = &info->pdev->dev;

	return device_create_file(dev, &dev_attr_slots);
}

static void rts_capemgr_info_sysfs_unregister(struct rts_capemgr_info *info)
{
	struct device *dev = &info->pdev->dev;

	device_remove_file(dev, &dev_attr_slots);
}

static int rts_capemgr_load(struct rts_cape_slot *slot)
{
	struct rts_capemgr_info *info = slot->info;
	struct device *dev = &info->pdev->dev;
	struct device_node *node;
	struct property *prop;
	const char *dtbo;
	int found, err;
	struct rts_capemap *capemap;
	void *dt;

	if (slot->loaded)
		return -EAGAIN;

	mutex_lock(&info->capemap_mutex);
	found = 0;
	list_for_each_entry(capemap, &info->capemap_list, node) {
		if (strcmp(capemap->part_number, slot->part_number) == 0) {
			found = 1;
			break;
		}
	}

	/* found? */
	if (found) {
		if (capemap->map_node == NULL) {
			mutex_unlock(&info->capemap_mutex);
			/* need to match programatically; not supported yet */
			dev_err(dev, "slot #%lu: Failed to find capemap for '%s'\n",
					slot->slotno, slot->part_number);
			return -ENODEV;
		}

		/* locate first match */
		dtbo = NULL;
		for_each_child_of_node(capemap->map_node, node) {

			/* dtbo must exist */
			if (of_property_read_string(node, "dtbo", &dtbo) != 0)
				continue;

			/* get version property (if any) */
			prop = of_find_property(node, "version", NULL);

			/* if no version node exists, we match */
			if (prop == NULL)
				break;

			if (of_prop_cmp(prop->value, slot->version) == 0)
				break;
		}

		if (node == NULL) {
			/* can't find dtbo version node? try the default */
			if (of_property_read_string(capemap->map_node,
						"dtbo", &dtbo) != 0) {
				mutex_unlock(&info->capemap_mutex);
				dev_err(dev, "slot #%lu: Failed to find dtbo for '%s,%s'\n",
						slot->slotno, slot->part_number,
						slot->version);
				return -ENODEV;
			}
		}

		slot->dtbo = kstrdup(dtbo, GFP_KERNEL);
		of_node_put(node);	/* handles NULL */
	} else {
		dev_info(dev, "slot #%lu: Requesting part number/version based %s_%s.dtbo\n",
				slot->slotno,
				slot->part_number, slot->version);

		/* no specific capemap node; request the part number + .dtbo*/
		slot->dtbo = kasprintf(GFP_KERNEL, "%s_%s.dtbo",
				slot->part_number, slot->version);
	}

	if (slot->dtbo == NULL) {
		mutex_unlock(&info->capemap_mutex);
		dev_err(dev, "slot #%lu: Failed to get dtbo '%s'\n",
				slot->slotno, dtbo);
		return -ENOMEM;
	}

	dev_info(dev, "slot #%lu: Requesting firmware '%s' for board-name '%s', version '%s'\n",
			slot->slotno,
			slot->dtbo, slot->part_number, slot->version);

	slot->loading = 1;
	slot->loaded = 0;
	err = request_firmware(&slot->fw, slot->dtbo, dev);
	if (err != 0) {
		dev_err(dev, "failed to load firmware '%s'\n", slot->dtbo);
		mutex_unlock(&info->capemap_mutex);
		goto err_fail_no_fw;
	}

	dev_info(dev, "slot #%lu: dtbo '%s' loaded; converting to live tree\n",
			slot->slotno, slot->dtbo);

	mutex_unlock(&info->capemap_mutex);

	of_overlay_fdt_apply((void *)slot->fw->data, slot->fw->size,
				&slot->ov_id, NULL);
	if (slot->ov_id < 0) {
		dev_err(dev, "slot #%lu: Failed to create overlay\n",
					slot->slotno);
			goto err_fail_create;
	}

	dev_info(dev, "slot #%lu: Applied #%d overlays.\n",
			slot->slotno, slot->ov_id);

	slot->loading = 0;
	slot->loaded = 1;

	return 0;
err_fail_create:
	kfree(dt);
err_fail:
	slot->overlay = NULL;
	release_firmware(slot->fw);
	slot->fw = NULL;

err_fail_no_fw:
	return err;
}

static int rts_capemgr_unload(struct rts_cape_slot *slot)
{
	struct rts_capemgr_info *info = slot->info;
	struct device *dev = &info->pdev->dev;
	int ret = 0;

	if (slot->loaded) {
		ret = of_overlay_remove(&slot->ov_id);
		if (ret)
			dev_err(dev, "slot #%lu: Failed to destroy overlay\n",
					slot->slotno);
	}

	if (!ret) {
		rts_cape_slot_sysfs_unregister(slot);
		list_del(&slot->node);
		slot->ov_id = -1;
		slot->loaded = 0;
		dev_info(dev, "slot #%lu: '%s' removed\n",
				slot->slotno, slot->text_id);
	}

	return ret;
}

static int rts_slot_fill_override(struct rts_cape_slot *slot,
		struct device_node *node,
		const char *part_number, const char *version)
{
	const struct cape_field *sig_field;
	struct property *prop;
	int i, len, has_part_number;
	char *p;

	slot->probed = 0;

	/* zero out signature */
	memset(slot->signature, 0,
			sizeof(slot->signature));

	/* first, fill in all with override defaults */
	for (i = 0; i < ARRAY_SIZE(cape_sig_fields); i++) {

		sig_field = &cape_sig_fields[i];

		/* point to the entry */
		p = slot->signature + sig_field->start;

		if (sig_field->override)
			memcpy(p, sig_field->override,
					sig_field->size);
		else
			memset(p, 0, sig_field->size);
	}

	/* and now, fill any override data from the node */
	has_part_number = 0;
	if (node != NULL) {
		for (i = 0; i < ARRAY_SIZE(cape_sig_fields); i++) {

			sig_field = &cape_sig_fields[i];

			/* find property with the same name (if any) */
			prop = of_find_property(node, sig_field->name, NULL);
			if (prop == NULL)
				continue;

			/* point to the entry */
			p = slot->signature + sig_field->start;

			/* copy and zero out any remainder */
			len = prop->length;
			if (prop->length > sig_field->size)
				len = sig_field->size;
			memcpy(p, prop->value, len);
			if (len < sig_field->size)
				memset(p + len, 0, sig_field->size - len);

			/* remember if we got a part number which is required */
			if (i == CAPE_FIELD_PART_NUMBER && len > 0)
				has_part_number = 1;
		}
	}

	/* if a part_number is supplied use it */
	if (part_number && strlen(part_number) > 0) {
		len = strlen(part_number);
		sig_field = &cape_sig_fields[CAPE_FIELD_PART_NUMBER];

		/* point to the entry */
		p = slot->signature + sig_field->start;

		/* copy and zero out any remainder */
		if (len > sig_field->size)
			len = sig_field->size;
		memcpy(p, part_number, len);
		if (len < sig_field->size)
			memset(p + len, 0, sig_field->size - len);

		has_part_number = 1;
	}

	/* if a version is supplied use it */
	if (version && strlen(version) > 0) {
		len = strlen(version);
		sig_field = &cape_sig_fields[CAPE_FIELD_VERSION];

		/* point to the entry */
		p = slot->signature + sig_field->start;

		/* copy and zero out any remainder */
		if (len > sig_field->size)
			len = sig_field->size;
		memcpy(p, version, len);
		if (len < sig_field->size)
			memset(p + len, 0, sig_field->size - len);
	}

	/* we must have a board name */
	if (!has_part_number)
		return -EINVAL;

	return 0;
}

static struct rts_cape_slot *
rts_capemgr_add_slot(struct rts_capemgr_info *info, struct device_node *node,
		const char *part_number, const char *version)
{
	struct rts_cape_slot *slot;
	struct device *dev = &info->pdev->dev;
	unsigned long slotno;
	int ret;

	slotno = atomic_inc_return(&info->next_slot_nr) - 1;

	slot = devm_kzalloc(dev, sizeof(*slot), GFP_KERNEL);
	if (slot == NULL) {
		ret = -ENOMEM;
		goto err_no_mem;
	}
	slot->info = info;
	slot->slotno = slotno;

	/* fill in everything with defaults first */
	ret = rts_slot_fill_override(slot, node, part_number, version);
	if (ret != 0) {
		dev_err(dev, "slot #%lu: override failed\n",
				slotno);
		goto err_bad_scan;
	}

	ret = rts_slot_scan(slot);
	if (ret != 0) {
		dev_err(dev, "slot #%lu: No cape found\n",
				slotno);
		/* but all is fine */
	} else {
		dev_info(dev, "slot #%lu: '%s'\n",
				slotno, slot->text_id);
		ret = rts_cape_slot_sysfs_register(slot);
		if (ret != 0) {
			dev_err(dev, "slot #%lu: sysfs register failed\n",
					slotno);
			goto err_no_sysfs;
		}
	}

	/* add to the slot list */
	mutex_lock(&info->slots_list_mutex);
	list_add_tail(&slot->node, &info->slot_list);
	mutex_unlock(&info->slots_list_mutex);

	return slot;

err_no_sysfs:
err_bad_scan:
	devm_kfree(dev, slot);

err_no_mem:
	return ERR_PTR(ret);
}

static int rts_capemgr_loader(void *data)
{
	struct rts_cape_slot *slot = data;

	return rts_capemgr_load(slot);
}

static int rts_capemgr_probe(struct platform_device *pdev)
{
	struct rts_capemgr_info *info;
	struct rts_cape_slot *slot;
	struct device_node *pnode = pdev->dev.of_node;
	struct device_node *slots_node, *capemaps_node, *node;
	const char *part_number;
	struct rts_capemap *capemap;
	int ret, len;

	if (pnode == NULL)
		return -ENOTSUPP;

	info = devm_kzalloc(&pdev->dev,
			sizeof(struct rts_capemgr_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pdev = pdev;
	platform_set_drvdata(pdev, info);

	atomic_set(&info->next_slot_nr, 0);
	INIT_LIST_HEAD(&info->slot_list);
	mutex_init(&info->slots_list_mutex);
	INIT_LIST_HEAD(&info->capemap_list);
	mutex_init(&info->capemap_mutex);

	capemaps_node = NULL;

	/* iterate over any capemaps */
	capemaps_node = of_get_child_by_name(pnode, "capemaps");
	if (capemaps_node != NULL) {

		for_each_child_of_node(capemaps_node, node) {

			/* there must be part-number */
			if (of_property_read_string(node, "part-number",
						&part_number) != 0)
				continue;

			len = sizeof(*capemap) + strlen(part_number) + 1;
			capemap = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
			if (capemap == NULL) {
				dev_err(&pdev->dev, "Failed to allocate capemap\n");
				ret = -ENOMEM;
				goto err_exit;
			}
			capemap->part_number = (char *)(capemap + 1);
			capemap->map_node = of_node_get(node);
			strcpy(capemap->part_number, part_number);

			/* add to the slot list */
			mutex_lock(&info->capemap_mutex);
			list_add_tail(&capemap->node, &info->capemap_list);
			info->capemaps_nr++;
			mutex_unlock(&info->capemap_mutex);
		}
		of_node_put(capemaps_node);
		capemaps_node = NULL;
	}

	/* iterate over any slots */
	slots_node = of_get_child_by_name(pnode, "slots");
	if (slots_node != NULL) {
		for_each_child_of_node(slots_node, node) {

			slot = rts_capemgr_add_slot(info, node,
					NULL, NULL);
			if (IS_ERR(slot)) {
				dev_err(&pdev->dev, "Failed to add slot #%d\n",
					atomic_read(&info->next_slot_nr) - 1);
				ret = PTR_ERR(slot);
				goto err_exit;
			}
		}
		of_node_put(slots_node);
	}
	slots_node = NULL;

	rts_capemgr_info_sysfs_register(info);

	/* now load each (take lock to be sure */
	mutex_lock(&info->slots_list_mutex);
	list_for_each_entry(slot, &info->slot_list, node) {
		if (!slot->loaded) {
			slot->loader_thread = kthread_run(rts_capemgr_loader,
					slot, "capemgr-loader-%lu",
					slot->slotno);
			if (IS_ERR(slot->loader_thread)) {
				dev_warn(&pdev->dev, "slot #%lu: Failed to start loader\n",
					slot->slotno);
				slot->loader_thread = NULL;
			}
		}
	}

	mutex_unlock(&info->slots_list_mutex);

	dev_info(&pdev->dev, "initialized OK.\n");

	return 0;

err_exit:
	of_node_put(capemaps_node);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, info);

	return ret;
}

static int rts_capemgr_remove(struct platform_device *pdev)
{
	struct rts_capemgr_info *info = platform_get_drvdata(pdev);
	struct rts_cape_slot *slot, *slotn;

	mutex_lock(&info->slots_list_mutex);
	list_for_each_entry_safe(slot, slotn, &info->slot_list, node) {

		/* unload just in case */
		rts_capemgr_unload(slot);

		/* if probed OK, remove the sysfs nodes */
		if (slot->probed)
			rts_cape_slot_sysfs_unregister(slot);

		/* remove it from the list */
		list_del(&slot->node);

	}
	mutex_unlock(&info->slots_list_mutex);

	rts_capemgr_info_sysfs_unregister(info);

	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, info);

	return 0;
}

static struct platform_driver rts_capemgr_driver = {
	.probe		= rts_capemgr_probe,
	.remove		= rts_capemgr_remove,
	.driver		= {
		.name	= "rts-capemgr",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rts_capemgr_of_match),
	},
};

module_platform_driver(rts_capemgr_driver);

MODULE_DESCRIPTION("cape manager");
MODULE_LICENSE("GPL");
