/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */
/*fixed memory related issues*/

/*
 * cyttsp4_devtree.c
 * Cypress TrueTouch(TM) Standard Product V4 Device Tree Support Driver.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/slab.h>

/* cyttsp */
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_mt.h>
#include <linux/cyttsp4_proximity.h>
#include <linux/cyttsp4_platform.h>

#include "cyttsp4_regs.h"
#include "cyttsp4_devtree.h"
#include <linux/of_gpio.h>

#include "cyttsp4_device_access.h"

#define ENABLE_VIRTUAL_KEYS

#define MAX_NAME_LENGTH		64

#ifdef CONFIG_INPUT_HW_ATE
#define MAX_VIRTUALKEYS_NUM 500
char buf_virtualkeys[MAX_VIRTUALKEYS_NUM];
ssize_t buf_vkey_size = 0;
#endif

enum cyttsp4_device_type {
	DEVICE_MT,
	DEVICE_BTN,
	DEVICE_PROXIMITY,
#if 1				//hauwei 0701
	DEVICE_LOADER,
#endif				//huawei 0701
	DEVICE_TYPE_MAX,
};

struct cyttsp4_device_pdata_func {
	void *(*create_and_get_pdata) (struct device_node *);
	void (*free_pdata) (void *);
};

#ifdef ENABLE_VIRTUAL_KEYS
static struct kobject *board_properties_kobj;

struct cyttsp4_virtual_keys {
	struct kobj_attribute kobj_attr;
	u16 *data;
	int size;
};
#endif

struct cyttsp4_extended_mt_platform_data {
	struct cyttsp4_mt_platform_data pdata;
#ifdef ENABLE_VIRTUAL_KEYS
	struct cyttsp4_virtual_keys vkeys;
#endif
};

static inline int get_inp_dev_name(struct device_node *dev_node,
				   const char **inp_dev_name)
{
	return of_property_read_string(dev_node, "cy,inp_dev_name",
				       inp_dev_name);
}

static u16 *create_and_get_u16_array(struct device_node *dev_node,
				     const char *name, int *size)
{
	const __be32 *values;
	u16 *val_array;
	int len;
	int sz;
	int rc;
	int i;

	values = of_get_property(dev_node, name, &len);
	if (values == NULL)
		return NULL;

	sz = len / sizeof(u32);
	tp_log_debug("%s: %s size:%d\n", __func__, name, sz);

	val_array = kzalloc(sz * sizeof(u16), GFP_KERNEL);
	if (val_array == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < sz; i++)
		val_array[i] = (u16) be32_to_cpup(values++);

	*size = sz;

	return val_array;

fail:
	return ERR_PTR(rc);
}

/*
The caller should free abs and frmwrk pointer safely
The function to do it is: free_touch_framework
*/
static struct touch_framework *create_and_get_touch_framework(struct device_node
							      *dev_node)
{
	struct touch_framework *frmwrk;
	u16 *abs;
	int size;
	int rc;

	abs = create_and_get_u16_array(dev_node, "cy,abs", &size);
	if (IS_ERR_OR_NULL(abs))
		return (void *)abs;

	/* Check for valid abs size */
	if (size % CY_NUM_ABS_SET) {
		rc = -EINVAL;
		goto fail_free_abs;
	}

	frmwrk = kzalloc(sizeof(*frmwrk), GFP_KERNEL);
	if (frmwrk == NULL) {
		rc = -ENOMEM;
		goto fail_free_abs;
	}

	frmwrk->abs = abs;
	frmwrk->size = size;

	return frmwrk;

fail_free_abs:
	kfree(abs);

	return ERR_PTR(rc);
}

static void free_touch_framework(struct touch_framework *frmwrk)
{
	/*If call in failure case when frmwrk is free but abs is not it might crash,
	   use check even its called in failure case */
	if (frmwrk)
		kfree(frmwrk->abs);
	kfree(frmwrk);
}

static struct touch_wakeup_keys *create_and_get_wakeup_keys(struct device_node
							    *dev_node)
{
	struct touch_wakeup_keys *wakeup_keys = NULL;
	u16 *keys = NULL;
	int size = 0;
	int rc = 0;

	keys =
	    create_and_get_u16_array(dev_node, "cy,easy_wakeup_gesture_keys",
				     &size);
	if (IS_ERR_OR_NULL(keys))
		return (void *)keys;

	/* Check for valid abs size */
	if (size > 8) {
		rc = -EINVAL;
		goto fail_free_keys;
	}

	wakeup_keys = kzalloc(sizeof(*wakeup_keys), GFP_KERNEL);
	if (wakeup_keys == NULL) {
		rc = -ENOMEM;
		goto fail_free_keys;
	}

	wakeup_keys->keys = keys;
	wakeup_keys->size = size;

	return wakeup_keys;

fail_free_keys:
	kfree(keys);

	return ERR_PTR(rc);
}

static void free_wakeup_keys(struct touch_wakeup_keys *wakeup_keys)
{
	/*If call in failure case when frmwrk is free but abs is not it might crash,
	   use check even its called in failure case */
	if (wakeup_keys)
		kfree(wakeup_keys->keys);
	kfree(wakeup_keys);
}


#ifdef ENABLE_VIRTUAL_KEYS
#define VIRTUAL_KEY_ELEMENT_SIZE	5
static ssize_t virtual_keys_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	struct cyttsp4_virtual_keys *vkeys = container_of(attr,
							  struct
							  cyttsp4_virtual_keys,
							  kobj_attr);
	u16 *data = vkeys->data;
	int size = vkeys->size;
	int index;
	int i;

	index = 0;
	for (i = 0; i < size; i += VIRTUAL_KEY_ELEMENT_SIZE)
		index += scnprintf(buf + index, CY_MAX_PRBUF_SIZE - index,
				   "0x01:%d:%d:%d:%d:%d\n",
				   data[i], data[i + 1], data[i + 2],
				   data[i + 3], data[i + 4]);

#ifdef CONFIG_INPUT_HW_ATE
	if (index < MAX_VIRTUALKEYS_NUM) {
		memcpy(buf_virtualkeys, buf, index);
		buf_vkey_size = index;
		tp_log_debug("%s: virtual_keys_show : buf_vkey_size = %d\n",
			     __func__, index);
	} else {
		tp_log_debug("%s: virtual_keys_show : index = %d\n", __func__,
			     index);
	}
#endif

	return index;
}

static int setup_virtual_keys(struct device_node *dev_node,
			      const char *inp_dev_name,
			      struct cyttsp4_virtual_keys *vkeys)
{
	char *name;
	u16 *data;
	int size;
	int rc;

	data = create_and_get_u16_array(dev_node, "cy,virtual_keys", &size);
	if (data == NULL) {
		/*if virtual keys are not supported  return error */
		return -ENOMEM;
	} else if (IS_ERR(data)) {
		rc = PTR_ERR(data);
		goto fail;
	}

	/* Check for valid virtual keys size */
	if (size % VIRTUAL_KEY_ELEMENT_SIZE) {
		rc = -EINVAL;
		goto fail_free_data;
	}

	name = kzalloc(MAX_NAME_LENGTH, GFP_KERNEL);
	if (name == NULL) {
		rc = -ENOMEM;
		goto fail_free_data;
	}

	snprintf(name, MAX_NAME_LENGTH, "virtualkeys.%s", inp_dev_name);

	vkeys->data = data;
	vkeys->size = size;

	/* TODO: Instantiate in board file and export it */
	if (board_properties_kobj == NULL)
		board_properties_kobj =
		    kobject_create_and_add("board_properties", NULL);
	if (board_properties_kobj == NULL) {
		tp_log_err("%s: Cannot get board_properties kobject!\n",
			   __func__);
		rc = -EINVAL;
		goto fail_free_name;
	}

	/* Initialize dynamic SysFs attribute */
	sysfs_attr_init(&vkeys->kobj_attr.attr);
	vkeys->kobj_attr.attr.name = name;
	vkeys->kobj_attr.attr.mode = S_IRUGO;
	vkeys->kobj_attr.show = virtual_keys_show;

	rc = sysfs_create_file(board_properties_kobj, &vkeys->kobj_attr.attr);
	if (rc)
		goto fail_del_kobj;

	return 0;

fail_del_kobj:
	kobject_del(board_properties_kobj);
fail_free_name:
	kfree(name);
	vkeys->kobj_attr.attr.name = NULL;
fail_free_data:
	kfree(data);
	vkeys->data = NULL;
	vkeys->size = 0;
fail:
	return rc;
}

static void free_virtual_keys(struct cyttsp4_virtual_keys *vkeys)
{
	if (board_properties_kobj)
		sysfs_remove_file(board_properties_kobj,
				  &vkeys->kobj_attr.attr);

	kfree(vkeys->data);
	vkeys->data = NULL;
	vkeys->size = 0;

	kfree(vkeys->kobj_attr.attr.name);
	vkeys->kobj_attr.attr.name = NULL;
}
#endif

static void *create_and_get_mt_pdata(struct device_node *dev_node)
{
	struct cyttsp4_extended_mt_platform_data *ext_pdata;
	struct cyttsp4_mt_platform_data *pdata;
	u32 value;
	int rc;

	ext_pdata = kzalloc(sizeof(*ext_pdata), GFP_KERNEL);
	if (ext_pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	pdata = &ext_pdata->pdata;

	rc = get_inp_dev_name(dev_node, &pdata->inp_dev_name);
	if (rc)
		goto fail_free_pdata;

	/* Optional fields */
	rc = of_property_read_u32(dev_node, "cy,flags", &value);
	if (!rc)
		pdata->flags = value;

	rc = of_property_read_u32(dev_node, "cy,vkeys_x", &value);
	if (!rc)
		pdata->vkeys_x = value;

	rc = of_property_read_u32(dev_node, "cy,vkeys_y", &value);
	if (!rc)
		pdata->vkeys_y = value;

	pdata->wakeup_keys = create_and_get_wakeup_keys(dev_node);
	if (IS_ERR_OR_NULL(pdata->wakeup_keys)) {
		tp_log_err("%s: Wakeup gesture is not configured!\n", __func__);
		pdata->wakeup_keys = NULL;
	} else {
		tp_log_debug
		    ("%s: Wakeup gesture is configured for %d guestures\n",
		     __func__, pdata->wakeup_keys->size);
	}

	/* Required fields */
	pdata->frmwrk = create_and_get_touch_framework(dev_node);
	if (pdata->frmwrk == NULL) {
		rc = -EINVAL;
		goto fail_free_pdata;
	} else if (IS_ERR(pdata->frmwrk)) {
		rc = PTR_ERR(pdata->frmwrk);
		goto fail_free_pdata;
	}
#ifdef ENABLE_VIRTUAL_KEYS
	rc = setup_virtual_keys(dev_node, pdata->inp_dev_name,
				&ext_pdata->vkeys);
	if (rc) {
		tp_log_err
		    ("%s: Cannot setup virtual keys, only TP will work now!\n",
		     __func__);
		//If the virtual keys are not supported the TP should work fine;
	}
#endif
	return pdata;

fail_free_pdata:
#ifdef ENABLE_VIRTUAL_KEYS
	free_touch_framework(pdata->frmwrk);
#endif
	free_wakeup_keys(pdata->wakeup_keys);
	kfree(ext_pdata);
fail:
	return ERR_PTR(rc);
}

static void free_mt_pdata(void *pdata)
{
	struct cyttsp4_mt_platform_data *mt_pdata =
	    (struct cyttsp4_mt_platform_data *)pdata;
	struct cyttsp4_extended_mt_platform_data *ext_mt_pdata =
	    container_of(mt_pdata,
			 struct cyttsp4_extended_mt_platform_data, pdata);

	if (mt_pdata) {
		free_touch_framework(mt_pdata->frmwrk);
		free_wakeup_keys(mt_pdata->wakeup_keys);
	}

	free_touch_framework(mt_pdata->frmwrk);
#ifdef ENABLE_VIRTUAL_KEYS
	free_virtual_keys(&ext_mt_pdata->vkeys);
#endif
	kfree(ext_mt_pdata);
}

static void *create_and_get_btn_pdata(struct device_node *dev_node)
{
	struct cyttsp4_btn_platform_data *pdata;
	int rc;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = get_inp_dev_name(dev_node, &pdata->inp_dev_name);
	if (rc)
		goto fail_free_pdata;

	return pdata;

fail_free_pdata:
	kfree(pdata);
fail:
	return ERR_PTR(rc);
}

static void free_btn_pdata(void *pdata)
{
	struct cyttsp4_btn_platform_data *btn_pdata =
	    (struct cyttsp4_btn_platform_data *)pdata;

	kfree(btn_pdata);
}

static void *create_and_get_proximity_pdata(struct device_node *dev_node)
{
	struct cyttsp4_proximity_platform_data *pdata;
	int rc;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = get_inp_dev_name(dev_node, &pdata->inp_dev_name);
	if (rc)
		goto fail_free_pdata;

	pdata->frmwrk = create_and_get_touch_framework(dev_node);
	if (pdata->frmwrk == NULL) {
		rc = -EINVAL;
		goto fail_free_pdata;
	} else if (IS_ERR(pdata->frmwrk)) {
		rc = PTR_ERR(pdata->frmwrk);
		goto fail_free_pdata;
	}

	/*now memory is allocated for frmwrk */

	return pdata;

fail_free_pdata:
	kfree(pdata);
fail:
	return ERR_PTR(rc);
}

static void free_proximity_pdata(void *pdata)
{
	struct cyttsp4_proximity_platform_data *proximity_pdata =
	    (struct cyttsp4_proximity_platform_data *)pdata;

	if (proximity_pdata)
		free_touch_framework(proximity_pdata->frmwrk);

	kfree(proximity_pdata);
}

#if 1				//huawei 0701
static inline int get_firmware_name(struct device_node *dev_node,
				    const char **firmware_name)
{
	return of_property_read_string(dev_node, "cy,firmware_name",
				       firmware_name);
}

static inline int get_tp_location(struct device_node *dev_node,
				  tp_location * tp_location)
{
	int rc;
	char const *get_tp_location;

	rc = of_property_read_string(dev_node, "cy,tp_location",
				     &get_tp_location);
	if (rc) {
		tp_log_err("%s: read string error, rc=%d\n", __func__, rc);
		goto fail_get_string;
	}

	if (!strncmp("COF", get_tp_location, MAX_NAME_LENGTH)) {
		*tp_location = TP_COF;
	} else if (!strncmp("COB", get_tp_location, MAX_NAME_LENGTH)) {
		*tp_location = TP_COB;
	} else {
		*tp_location = TP_UNKNOW;
		tp_log_err("%s: tp_location is unknown, get_tp_location=%s\n",
			   __func__, get_tp_location);
		goto fail_get_string;
	}

fail_get_string:
	return rc;
}


static inline int get_product_family(struct device_node *dev_node,
				     const char **tp_product_family)
{
	return of_property_read_string(dev_node, "cy,product_family",
				       tp_product_family);
}


static void *create_and_get_loader_pdata(struct device_node *dev_node)
{
	struct cyttsp4_loader_platform_data *pdata;
	int rc;
	u32 value;

#if 0
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		//if (&_cyttsp4_loader_platform_data == NULL) {
		rc = -ENOMEM;
		goto fail;
	}
#else
	pdata = &_cyttsp4_loader_platform_data;
#endif
	rc = get_firmware_name(dev_node, &pdata->firmware_name);
	if (rc)
		goto fail_free_pdata;

	rc = get_tp_location(dev_node, &pdata->tp_location);
	if (rc)
		goto fail_free_pdata;

	/*add for force recalibration */
	rc = of_property_read_u32(dev_node, "cy,flags", &value);
	if (!rc)
		pdata->flags = value;

	rc = get_product_family(dev_node, &pdata->tp_product_family);
	if (rc)
		goto fail_free_pdata;

	return pdata;
fail_free_pdata:
	//kfree(pdata);
//fail:
	return ERR_PTR(rc);
}

static void free_loader_pdata(void *pdata)
{
	//now the loader's pdata is _cyttsp4_loader_platform_data
#if 0
	struct cyttsp4_loader_platform_data *loader_pdata =
	    (struct cyttsp4_loader_platform_data *)pdata;

	kfree(loader_pdata);
#endif
}
#endif //huawei 0701

static struct cyttsp4_device_pdata_func device_pdata_funcs[DEVICE_TYPE_MAX] = {
	[DEVICE_MT] = {
		       .create_and_get_pdata = create_and_get_mt_pdata,
		       .free_pdata = free_mt_pdata,
		       },
	[DEVICE_BTN] = {
			.create_and_get_pdata = create_and_get_btn_pdata,
			.free_pdata = free_btn_pdata,
			},
	[DEVICE_PROXIMITY] = {
			      .create_and_get_pdata =
			      create_and_get_proximity_pdata,
			      .free_pdata = free_proximity_pdata,
			      },
#if 1				//huawei 0701
	[DEVICE_LOADER] = {
			   .create_and_get_pdata = create_and_get_loader_pdata,
			   .free_pdata = free_loader_pdata,
			   },
#endif //huawei 0701
};

static const char *device_names[DEVICE_TYPE_MAX] = {
	[DEVICE_MT] = "cy,mt",
	[DEVICE_BTN] = "cy,btn",
	[DEVICE_PROXIMITY] = "cy,proximity",
#if 1				//huawei 0701
	[DEVICE_LOADER] = "cy,loader",
#endif //huawei 0701
};

static int get_device_type(struct device_node *dev_node,
			   enum cyttsp4_device_type *type)
{
	const char *name;
	enum cyttsp4_device_type t;
	int rc;

	rc = of_property_read_string(dev_node, "name", &name);
	if (rc)
		return rc;

	for (t = 0; t < DEVICE_TYPE_MAX; t++)
		if (!strncmp(name, device_names[t], MAX_NAME_LENGTH)) {
			*type = t;
			return 0;
		}

	return -EINVAL;
}

static inline void *create_and_get_device_pdata(struct device_node *dev_node,
						enum cyttsp4_device_type type)
{
	return device_pdata_funcs[type].create_and_get_pdata(dev_node);
}

static inline void free_device_pdata(void *pdata, enum cyttsp4_device_type type)
{
	device_pdata_funcs[type].free_pdata(pdata);
}

static int register_device(struct device_node *dev_node, const char *core_id)
{
	struct cyttsp4_device_info info = { 0 };
	enum cyttsp4_device_type type;
	struct cyttsp4_loader_platform_data *pdata;
	int rc;

	info.core_id = core_id;

	rc = of_property_read_string(dev_node, "cy,name", &info.name);
	if (rc) {
		tp_log_err("%s: OF error rc=%d at line %d for cy,name\n",
			   __func__, rc, __LINE__);
		goto fail;
	} else
		tp_log_debug("%s: OF cy,name: %s\n", __func__, info.name);

	rc = get_device_type(dev_node, &type);
	if (rc) {
		tp_log_err("%s: Get device type failed: %s\n", __func__,
			   info.name);
		goto fail;
	}

	info.platform_data = create_and_get_device_pdata(dev_node, type);
	if (DEVICE_LOADER == type) {
		pdata =
		    (struct cyttsp4_loader_platform_data *)info.platform_data;
	}
	if (IS_ERR(info.platform_data)) {
		tp_log_err("%s: Invalid platform data for: %s\n", __func__,
			   info.name);
		rc = PTR_ERR(info.platform_data);
		goto fail;
	}

	rc = cyttsp4_register_device(&info);
	if (rc) {
		tp_log_err("%s: register device error for: %s\n", __func__,
			   info.name);
		goto fail_free;
	}

	return 0;

fail_free:
	free_device_pdata(info.platform_data, type);
fail:
	return rc;
}

static struct touch_settings *create_and_get_touch_setting(struct device_node
							   *core_node,
							   const char *name)
{
	struct touch_settings *setting;
	char *tag_name;
	u32 tag_value;
	u16 *data;
	int size;
	int rc;

	data = create_and_get_u16_array(core_node, name, &size);
	if (IS_ERR_OR_NULL(data))
		return (void *)data;

	tp_log_debug("%s: Touch setting:'%s' size:%d\n", __func__, name, size);

	setting = kzalloc(sizeof(*setting), GFP_KERNEL);
	if (setting == NULL) {
		rc = -ENOMEM;
		goto fail_free_data;
	}

	setting->data = (u8 *) data;
	setting->size = size;

	tag_name = kzalloc(MAX_NAME_LENGTH, GFP_KERNEL);
	if (tag_name == NULL) {
		rc = -ENOMEM;
		goto fail_free_setting;
	}

	snprintf(tag_name, MAX_NAME_LENGTH, "%s-tag", name);

	rc = of_property_read_u32(core_node, tag_name, &tag_value);
	if (!rc)
		setting->tag = tag_value;

	kfree(tag_name);

	return setting;

fail_free_setting:
	kfree(setting);
fail_free_data:
	kfree(data);

	return ERR_PTR(rc);
}

static void free_touch_setting(struct touch_settings *setting)
{
	if (setting) {
		kfree(setting->data);
		kfree(setting);
	}
}

static char *touch_setting_names[CY_IC_GRPNUM_NUM] = {
	NULL,			/* CY_IC_GRPNUM_RESERVED */
	"cy,cmd_regs",		/* CY_IC_GRPNUM_CMD_REGS */
	"cy,tch_rep",		/* CY_IC_GRPNUM_TCH_REP */
	"cy,data_rec",		/* CY_IC_GRPNUM_DATA_REC */
	"cy,test_rec",		/* CY_IC_GRPNUM_TEST_REC */
	"cy,pcfg_rec",		/* CY_IC_GRPNUM_PCFG_REC */
	"cy,tch_parm_val",	/* CY_IC_GRPNUM_TCH_PARM_VAL */
	"cy,tch_parm_size",	/* CY_IC_GRPNUM_TCH_PARM_SIZE */
	NULL,			/* CY_IC_GRPNUM_RESERVED1 */
	NULL,			/* CY_IC_GRPNUM_RESERVED2 */
	"cy,opcfg_rec",		/* CY_IC_GRPNUM_OPCFG_REC */
	"cy,ddata_rec",		/* CY_IC_GRPNUM_DDATA_REC */
	"cy,mdata_rec",		/* CY_IC_GRPNUM_MDATA_REC */
	"cy,test_regs",		/* CY_IC_GRPNUM_TEST_REGS */
	"cy,btn_keys",		/* CY_IC_GRPNUM_BTN_KEYS */
	NULL,			/* CY_IC_GRPNUM_TTHE_REGS */
};

static cypress4_tp_cap_info tp_cap_data = {
	.min_mut_cap = -2950,
	.max_mut_cap = -1500,
	.min_self_cap = -600,
	.max_self_cap = 1300,
	.tx_lines = 16,
	.rx_lines = 28,
	.self_cap_num = 28,
	.data_ok = true,
	.ignore_list_mut_cap = NULL,
	.ignore_list_size_mut_cap = 0,
	.ignore_list_size_self_cap = 0,
	.data_start_byte = 0,
	.tp_ic_version = CY_TMD445,
};


cypress4_tp_cap_info tp_get_cap_data_f(void)
{
	return tp_cap_data;
}

static int atoi(const char *psz_buf)
{
	int i = 0, val = 0, sym = 1;

	while (psz_buf[i] != '\0') {
		if (' ' != psz_buf[i]) {
			if ('-' == psz_buf[i]) {
				sym = -1;
			} else if ('+' == psz_buf[i]) {
				sym = 1;
			} else if ((psz_buf[i] >= '0') && (psz_buf[i] <= '9')) {
				val = val * 10 + (psz_buf[i] - '0');
			} else {
				tp_log_err("%s: unexpect symbol %c\n", __func__,
					   psz_buf[i]);
				return sym * val;
			}

		}

		i++;

	}

	return sym * val;

}

static void get_tp_cap_pdata(struct device_node *core_node)
{
	const char *min_mut_cap_str;
	const char *max_mut_cap_str;
	const char *min_self_cap_str;
	const char *max_self_cap_str;
	char const *get_tp_ic_version;

	u32 value;
	int rc = 0;
	int size = 0;
	u16 *data = NULL;

	/* min_mut_cap */
	rc = of_property_read_string(core_node, "cy,min_mut_cap",
				     &min_mut_cap_str);
	if (!rc) {
		tp_log_info("%s: OF cy,min_mut_cap: %s\n", __func__,
			    min_mut_cap_str);
		tp_cap_data.min_mut_cap = atoi(min_mut_cap_str);
	} else {
		goto tp_cap_get_fail;
	}

	/* max_mut_cap */
	//memset(tmp_str, 0, CYP_CAP_NUM_MAX);
	rc = of_property_read_string(core_node, "cy,max_mut_cap",
				     &max_mut_cap_str);
	if (!rc) {
		tp_log_info("%s: OF cy,max_mut_cap: %s\n", __func__,
			    max_mut_cap_str);
		tp_cap_data.max_mut_cap = atoi(max_mut_cap_str);
	} else {
		goto tp_cap_get_fail;
	}

	/* min_self_cap */
	//memset(tmp_str, 0, CYP_CAP_NUM_MAX);
	rc = of_property_read_string(core_node, "cy,min_self_cap",
				     &min_self_cap_str);
	if (!rc) {
		tp_log_info("%s: OF cy,min_self_cap: %s\n", __func__,
			    min_self_cap_str);
		tp_cap_data.min_self_cap = atoi(min_self_cap_str);
	} else {
		goto tp_cap_get_fail;
	}

	/* max_self_cap */
	//memset(tmp_str, 0, CYP_CAP_NUM_MAX);
	rc = of_property_read_string(core_node, "cy,max_self_cap",
				     &max_self_cap_str);
	if (!rc) {
		tp_log_info("%s: OF cy,max_self_cap: %s\n", __func__,
			    max_self_cap_str);
		tp_cap_data.max_self_cap = atoi(max_self_cap_str);
	} else {
		goto tp_cap_get_fail;
	}

	data =
	    create_and_get_u16_array(core_node, "cy,ignore_list_mut_cap",
				     &size);
	if (!IS_ERR_OR_NULL(data)) {
		tp_cap_data.ignore_list_size_mut_cap = size;
		tp_cap_data.ignore_list_mut_cap = data;
	} else {
		tp_cap_data.ignore_list_size_mut_cap = 0;
		tp_cap_data.ignore_list_mut_cap = NULL;
	}

	data =
	    create_and_get_u16_array(core_node, "cy,ignore_list_self_cap",
				     &size);
	if (!IS_ERR_OR_NULL(data)) {
		tp_cap_data.ignore_list_size_self_cap = size;
		tp_cap_data.ignore_list_self_cap = data;
	} else {
		tp_cap_data.ignore_list_size_self_cap = 0;
		tp_cap_data.ignore_list_self_cap = NULL;
	}

	rc = of_property_read_u32(core_node, "cy,tx_lines", &value);
	if (!rc) {
		tp_log_info("%s: OF cy,tx_lines: %d\n", __func__, value);
		tp_cap_data.tx_lines = value;
	} else {
		goto tp_cap_get_fail;
	}

	rc = of_property_read_u32(core_node, "cy,data_start_byte", &value);
	if (!rc) {
		tp_log_info("%s: OF cy,data_start_byte: %d\n", __func__, value);
		tp_cap_data.data_start_byte = value;
	} else {
		goto tp_cap_get_fail;
	}

	rc = of_property_read_u32(core_node, "cy,rx_lines", &value);
	if (!rc) {
		tp_log_info("%s: OF cy,rx_lines: %d\n", __func__, value);
		tp_cap_data.rx_lines = value;
	} else {
		goto tp_cap_get_fail;
	}

	rc = of_property_read_u32(core_node, "cy,self_cap_num", &value);
	if (!rc) {
		tp_log_info("%s: OF cy,self_cap_num: %d\n", __func__, value);
		tp_cap_data.self_cap_num = value;
	} else {
		goto tp_cap_get_fail;
	}
	/*tp_ic_version */
	rc = of_property_read_string(core_node, "cy,tp_ic_version",
				     &get_tp_ic_version);
	if (rc) {
		tp_log_err("%s: read tp ic version error, rc=%d\n", __func__,
			   rc);
		goto tp_cap_get_fail;
	}

	if (!strncmp("TMD463", get_tp_ic_version, MAX_NAME_LENGTH)) {
		tp_cap_data.tp_ic_version = CY_TMD463;
	} else if (!strncmp("TMD445", get_tp_ic_version, MAX_NAME_LENGTH)) {
		tp_cap_data.tp_ic_version = CY_TMD445;
	} else {
		tp_cap_data.tp_ic_version = CY_TMDUNKNOW;
		tp_log_err("%s: tp ic version is unknown\n", __func__);
		goto tp_cap_get_fail;
	}
	return;

tp_cap_get_fail:
	tp_log_err("%s: get capacitance info fail!!!\n", __func__);
	tp_cap_data.data_ok = false;
	return;
}


static struct cyttsp4_core_platform_data *create_and_get_core_pdata(struct
								    device_node
								    *core_node)
{
	struct cyttsp4_core_platform_data *pdata;
	u32 value;
	int rc;
	int i;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	value = of_get_named_gpio(core_node, "cy,irq_gpio", 0);
	if (!gpio_is_valid(value)) {
		goto fail_free;
	}
	pdata->irq_gpio = value;

	value = of_get_named_gpio(core_node, "cy,rst_gpio", 0);
	if (gpio_is_valid(value)) {
		pdata->rst_gpio = value;
	}
	tp_log_info("%s: irq-gpio=%d,rst-gpio=%d\n", __func__, pdata->irq_gpio,
		    pdata->rst_gpio);
#if 0
	/* Required fields */
	rc = of_property_read_u32(core_node, "cy,irq_gpio", &value);
	if (rc)
		goto fail_free;
	pdata->irq_gpio = value;

	/* Optional fields */
	/* rst_gpio is optional since a platform may use
	 * power cycling instead of using the XRES pin
	 */

	rc = of_property_read_u32(core_node, "cy,rst_gpio", &value);
	if (!rc)
		pdata->rst_gpio = value;
#endif
	rc = of_property_read_u32(core_node, "cy,level_irq_udelay", &value);
	if (!rc)
		pdata->level_irq_udelay = value;

	rc = of_property_read_u32(core_node, "cy,max_xfer_len", &value);
	if (!rc)
		pdata->max_xfer_len = value;

	rc = of_property_read_u32(core_node, "cy,flags", &value);
	if (!rc)
		pdata->flags = value;

	rc = of_property_read_u32(core_node,
				  "cy,easy_wakeup_supported_gestures", &value);
	if (!rc)
		pdata->easy_wakeup_supported_gestures = value;
	/*add for glove function */
	rc = of_property_read_u32(core_node, "cy,use_configure_sensitivity",
				  &value);
	if (!rc)
		pdata->use_configure_sensitivity = value;

	rc = of_property_read_u32(core_node, "cy,easy_wakeup_gesture", &value);
	if (!rc)
		pdata->easy_wakeup_gesture = (u8) value;

	/* read tp capacitance infomation */
	get_tp_cap_pdata(core_node);

	for (i = 0; (unsigned int)i < ARRAY_SIZE(touch_setting_names); i++) {
		if (touch_setting_names[i] == NULL)
			continue;

		pdata->sett[i] = create_and_get_touch_setting(core_node,
							      touch_setting_names
							      [i]);
		if (IS_ERR(pdata->sett[i])) {
			rc = PTR_ERR(pdata->sett[i]);
			goto fail_free_sett;
		} else if (pdata->sett[i] == NULL)
			tp_log_debug("%s: No data for setting '%s'\n", __func__,
				     touch_setting_names[i]);
	}

	tp_log_debug("%s: irq_gpio:%d rst_gpio:%d level_irq_udelay:%d\n"
		     "max_xfer_len:%d flags:%d easy_wakeup_gesture:%d\n",
		     __func__, pdata->irq_gpio, pdata->rst_gpio,
		     pdata->level_irq_udelay, pdata->max_xfer_len, pdata->flags,
		     pdata->easy_wakeup_gesture);

	pdata->xres = cyttsp4_xres;
	pdata->init = cyttsp4_init;
	pdata->power = cyttsp4_power;
#ifdef CYTTSP4_DETECT_HW
	pdata->detect = cyttsp4_detect;
#endif
	pdata->irq_stat = cyttsp4_irq_stat;

	pdata->loader_pdata = &_cyttsp4_loader_platform_data;

	return pdata;

fail_free_sett:
	for (i--; i >= 0; i--)
		free_touch_setting(pdata->sett[i]);
fail_free:
	kfree(pdata);
fail:
	return ERR_PTR(rc);
}

static void free_core_pdata(struct cyttsp4_core_platform_data *pdata)
{
	int i;

	for (i = 0; i < CY_TOUCH_SETTINGS_MAX; i++)
		free_touch_setting(pdata->sett[i]);

	kfree(tp_cap_data.ignore_list_mut_cap);
	kfree(tp_cap_data.ignore_list_self_cap);

	kfree(pdata);
}

static int register_core_device(struct device_node *core_node,
				const char *adap_id, const char **core_id)
{
	struct cyttsp4_core_info info = { 0 };
	int rc;

	rc = of_property_read_string(core_node, "cy,name", &info.name);
	if (rc) {
		tp_log_err("%s: OF error rc=%d at line %d for cy,name\n",
			   __func__, rc, __LINE__);
		goto fail;
	} else
		tp_log_debug("%s: OF cy,name: %s\n", __func__, info.name);

	rc = of_property_read_string(core_node, "cy,id", &info.id);
	if (rc) {
		tp_log_err("%s: OF error rc=%d at line %d for cy,id\n",
			   __func__, rc, __LINE__);
		goto fail;
	} else
		tp_log_debug("%s: OF cy,id: %s\n", __func__, info.id);

	info.platform_data = create_and_get_core_pdata(core_node);
	if (IS_ERR(info.platform_data)) {
		rc = PTR_ERR(info.platform_data);
		goto fail;
	}

	info.adap_id = adap_id;

	rc = cyttsp4_register_core_device(&info);
	if (rc)
		goto fail_free;

	*core_id = info.id;

	return 0;

fail_free:
	free_core_pdata(info.platform_data);
fail:
	return rc;
}

int cyttsp4_devtree_register_devices(struct device *adap_dev)
{
	struct device_node *core_node, *dev_node;
	const char *adap_id;
	int count = 0;
	int rc;

	if (!adap_dev->of_node)
		return 0;

	rc = of_property_read_string(adap_dev->of_node, "cy,adapter_id",
				     &adap_id);
	if (rc)
		return rc;

	/* There should be only one core node */
	for_each_child_of_node(adap_dev->of_node, core_node) {
		const char *core_id = NULL;
		const char *name;

		rc = of_property_read_string(core_node, "name", &name);
		if (!rc)
			tp_log_debug("%s: name:%s\n", __func__, name);

		rc = register_core_device(core_node, adap_id, &core_id);
		if (rc)
			break;
		/* Increment reference count */
		of_node_get(core_node);

		for_each_child_of_node(core_node, dev_node) {
			count++;
			rc = register_device(dev_node, core_id);
			if (rc)
				break;
			/* Increment reference count */
			of_node_get(dev_node);
		}
	}

	tp_log_debug("%s: %d child node(s) found\n", __func__, count);

	return rc;
}

EXPORT_SYMBOL_GPL(cyttsp4_devtree_register_devices);

static int __init cyttsp4_devtree_init(void)
{
	return 0;
}

module_init(cyttsp4_devtree_init);

static void __exit cyttsp4_devtree_exit(void)
{
}

module_exit(cyttsp4_devtree_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cypress Semiconductor");

