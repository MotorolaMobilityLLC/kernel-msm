// SPDX-License-Identifier: GPL-2.0
/*
 * USB Typec-C DisplayPort Alternate Mode driver
 *
 * Copyright (C) 2018 Intel Corporation
 * Author: Heikki Krogerus <heikki.krogerus@linux.intel.com>
 *
 * DisplayPort is trademark of VESA (www.vesa.org)
 */

#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb/pd_vdo.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_dp.h>
#include "displayport.h"

#define MOTO_ALTMODE(fmt,...) pr_err("MMI_DETECT: D-PORT[%s]: " fmt "\n", __func__, ##__VA_ARGS__)

#define DP_HEADER(_dp, ver, cmd)	(VDO((_dp)->alt->svid, 1, ver, cmd)	\
					 | VDO_OPOS(USB_TYPEC_DP_MODE))

enum {
	DP_CONF_USB,
	DP_CONF_DFP_D,
	DP_CONF_UFP_D,
	DP_CONF_DUAL_D,
};

/* Pin assignments that use USB3.1 Gen2 signaling to carry DP protocol */
#define DP_PIN_ASSIGN_GEN2_BR_MASK	(BIT(DP_PIN_ASSIGN_A) | \
					 BIT(DP_PIN_ASSIGN_B))

/* Pin assignments that use DP v1.3 signaling to carry DP protocol */
#define DP_PIN_ASSIGN_DP_BR_MASK	(BIT(DP_PIN_ASSIGN_C) | \
					 BIT(DP_PIN_ASSIGN_D) | \
					 BIT(DP_PIN_ASSIGN_E) | \
					 BIT(DP_PIN_ASSIGN_F))

/* DP only pin assignments */
#define DP_PIN_ASSIGN_DP_ONLY_MASK	(BIT(DP_PIN_ASSIGN_A) | \
					 BIT(DP_PIN_ASSIGN_C) | \
					 BIT(DP_PIN_ASSIGN_E))

/* Pin assignments where one channel is for USB */
#define DP_PIN_ASSIGN_MULTI_FUNC_MASK	(BIT(DP_PIN_ASSIGN_B) | \
					 BIT(DP_PIN_ASSIGN_D) | \
					 BIT(DP_PIN_ASSIGN_F))

struct dp_typec_bridge {
	void *dp_priv;
	int (*notifier_cb)(void *priv, void *data, size_t len);
};

static struct dp_typec_bridge bridge;

enum dp_state {
	DP_STATE_IDLE,
	DP_STATE_ENTER,
	DP_STATE_UPDATE,
	DP_STATE_CONFIGURE,
	DP_STATE_EXIT,
};

struct dp_altmode {
	struct typec_displayport_data data;

	enum dp_state state;

	struct mutex lock; /* device lock */
	struct work_struct work;
	struct typec_altmode *alt;
	const struct typec_altmode *port;
	struct dp_typec_bridge *typec_bridge;
};

#if IS_ENABLED(CONFIG_TYPEC_QTI_ALTMODE)
extern unsigned int typec_get_portid(struct typec_port *port);

/*
   QTI DP driver (dp_displayport.c) needs notifications in form of specific callbacks
   defined in its ops (configure_cb, attention_cb, and disconnect_cb) that get called
   from dp_altmode driver (incidentally, from a function called dp_altmode_notify, not
   to be confused with its namesake here).

   The callbacks rely on a bunch of DP driver specific fields in a private structure that
   need to be populated. That job is done by QTI altmode driver (dp_altmode.c),
   and the purpose of this "bridge" function is to "translate" the data coming from TCPM
   into format that will be understood by dp_altmode driver.
*/
int dp_altmode_bridge(struct dp_altmode *dp)
{
	struct dp_typec_bridge *bridge = dp->typec_bridge;
	struct typec_port *port = typec_altmode2port(dp->alt);
	int orientation = typec_get_orientation(port);
	u8 state = 0, payload[10];

	if (!bridge || !bridge->notifier_cb || !bridge->dp_priv)
		return -ENODEV;

	/* Only go to MODAL state if we got something in config
	   Otherwise the only reason we are here is for disconnect_cb()
	   and then we need to keep the state empty.
	*/
	if (DP_CONF_GET_PIN_ASSIGN(dp->data.conf))
		state = TYPEC_MODAL_STATE(get_count_order(DP_CONF_GET_PIN_ASSIGN(dp->data.conf)));

	MOTO_ALTMODE("con=0x%x, conf=0x%x, orientation=%d, state=0x%x",
			DP_STATUS_CONNECTION(dp->data.status),
			DP_CONF_CURRENTLY(dp->data.conf), orientation, state);

	/* Emulate data from QTI glink message
           dp_altmode driver enumerates orientations from 0, but uses value 2 to indicate none
	   It needs orientation to control DP PHY muxing
	   It does not really do anything with the port ID, except for logging
	*/
	memset(payload, 0, sizeof(payload));
	payload[0] = typec_get_portid(port);
	payload[1] = orientation ? orientation - 1 : 2;

	/* QTI enumerates DP configurations from 1, with 0 indicating HPD_OUT.
	   But PD, DP, and MUX drivers enumerate DP states from 2, with 0,
	   indicating SAFE_MODE and 1 indicating USB mode.
	   Subtract 1 here to make DP configuration indexes match

	   dp_altmode driver looks at the lower 6 bits for pin configuration
	   If we got some config, but are not connected yet, it will be configure_cb()
	   If we are connected already, it will be attention_cb()
	   If we got no config, it will be disconnect_cb()
	*/
	payload[8] |= (state ? state - 1 : 0) & 0x3f; // pin

	/* These will come with ATTENTION vdm sometime after CONFIGURE
	   The dp_display driver waits for hpd_state high to start talking to UFP_D
           When hpd_irq gets high, altmode talk is over and DP protocol begins
	*/
	payload[8] |= (dp->data.status & DP_STATUS_HPD_STATE) >> 1; // hpd_state
	payload[8] |= (dp->data.status & DP_STATUS_IRQ_HPD) >> 2;   // hpd_irq

	MOTO_ALTMODE("calling bridge; payload: [0]=0x%02x, [1]=0x%02x, [8]=0x%02x",
			payload[0], payload[1], payload[8]);

	/* Call dp_altmode driver, looking smug like we came from altmode-glink driver */
	bridge->notifier_cb(bridge->dp_priv, (void *)payload, sizeof(payload));

	return 0;
}

int dp_altmode_typec_bridge_register(void *priv, int (cb)(void *, void *, size_t))
{
	bridge.dp_priv = priv;
	bridge.notifier_cb = cb;

	return 0;
}
#else
int dp_altmode_bridge(struct dp_altmode *dp) { return -ENOTSUPP; }
int dp_altmode_typec_bridge_register(void *priv, int (cb)(void *, void *, size_t)) { return -ENOTSUPP; }
#endif /* CONFIG_TYPEC_QTI_ALTMODE */

EXPORT_SYMBOL_GPL(dp_altmode_typec_bridge_register);

static int dp_altmode_dispatch(struct typec_altmode *alt,
		unsigned long conf, void *data);

static int dp_altmode_notify(struct dp_altmode *dp)
{
	u8 state = get_count_order(DP_CONF_GET_PIN_ASSIGN(dp->data.conf));

	dp_altmode_dispatch(dp->alt, TYPEC_MODAL_STATE(state), &dp->data);

	return typec_altmode_notify(dp->alt, TYPEC_MODAL_STATE(state),
				   &dp->data);
}

static int dp_altmode_configure(struct dp_altmode *dp, u8 con)
{
	u32 conf = DP_CONF_SIGNALING_DP; /* Only DP signaling supported */
	u8 pin_assign = 0, alt_pins = 0, port_pins = 0;

	MOTO_ALTMODE("enter; con=%02x, alt_vdo=0x%X, port_vdo=0x%X",
			con, dp->alt->vdo, dp->port->vdo);
	switch (con) {
	case DP_STATUS_CON_DISABLED:
		return 0;
	case DP_STATUS_CON_DFP_D:
		conf |= DP_CONF_UFP_U_AS_DFP_D;
		alt_pins = (dp->alt->vdo & DP_CAP_RECEPTACLE)
			? DP_CAP_DFP_D_PIN_ASSIGN(dp->alt->vdo)
			: DP_CAP_UFP_D_PIN_ASSIGN(dp->alt->vdo);
		port_pins = (dp->port->vdo & DP_CAP_RECEPTACLE)
			? DP_CAP_DFP_D_PIN_ASSIGN(dp->port->vdo)
			: DP_CAP_UFP_D_PIN_ASSIGN(dp->port->vdo);
		pin_assign = alt_pins & port_pins;
		break;
	case DP_STATUS_CON_UFP_D:
	case DP_STATUS_CON_BOTH: /* NOTE: First acting as DP source */
		conf |= DP_CONF_UFP_U_AS_UFP_D;
		alt_pins = (dp->alt->vdo & DP_CAP_RECEPTACLE)
			? DP_CAP_UFP_D_PIN_ASSIGN(dp->alt->vdo)
			: DP_CAP_DFP_D_PIN_ASSIGN(dp->alt->vdo);
		port_pins = (dp->port->vdo & DP_CAP_RECEPTACLE)
			? DP_CAP_UFP_D_PIN_ASSIGN(dp->port->vdo)
			: DP_CAP_DFP_D_PIN_ASSIGN(dp->port->vdo);
		pin_assign = alt_pins & port_pins;
		break;
	default:
		break;
	}
	MOTO_ALTMODE("conf 0x%02x, status 0x%02x; incoming pin assign 0x%02x",
		DP_CONF_GET_PIN_ASSIGN(dp->data.conf), dp->data.status, pin_assign);

	/* Determining the initial pin assignment. */
	if (!DP_CONF_GET_PIN_ASSIGN(dp->data.conf)) {
		/* Is USB together with DP preferred */
		if (dp->data.status & DP_STATUS_PREFER_MULTI_FUNC &&
			pin_assign & DP_PIN_ASSIGN_MULTI_FUNC_MASK) {
			pin_assign &= DP_PIN_ASSIGN_MULTI_FUNC_MASK;
			/* hypothetical, as only D is valid for DP_CONF_SIGNALING_DP */
			if (pin_assign & BIT(DP_PIN_ASSIGN_D))
				pin_assign &= BIT(DP_PIN_ASSIGN_D);
		} else if (pin_assign & DP_PIN_ASSIGN_DP_ONLY_MASK) {
			pin_assign &= DP_PIN_ASSIGN_DP_ONLY_MASK;
			/* UFP may send both E and C, mask one out */
			if (pin_assign & BIT(DP_PIN_ASSIGN_C))
				pin_assign &= BIT(DP_PIN_ASSIGN_C);
			else if (pin_assign & BIT(DP_PIN_ASSIGN_E))
				pin_assign &= BIT(DP_PIN_ASSIGN_E);
		}

		/* there must be no more than 1 bit left set here */
		MOTO_ALTMODE("pin assign 0x%02x", pin_assign);
		if (!pin_assign)
			return -EINVAL;

		conf |= DP_CONF_SET_PIN_ASSIGN(pin_assign);
	}

	dp->data.conf = conf;

	return 0;
}

static int dp_altmode_status_update(struct dp_altmode *dp)
{
	bool configured = !!DP_CONF_GET_PIN_ASSIGN(dp->data.conf);
	u8 con = DP_STATUS_CONNECTION(dp->data.status);
	int ret = 0;

	if (configured && (dp->data.status & DP_STATUS_SWITCH_TO_USB)) {
		dp->data.conf = 0;
		dp->state = DP_STATE_CONFIGURE;
	} else if (dp->data.status & DP_STATUS_EXIT_DP_MODE) {
		dp->state = DP_STATE_EXIT;
	} else if (!(con & DP_CONF_CURRENTLY(dp->data.conf))) {
		ret = dp_altmode_configure(dp, con);
		MOTO_ALTMODE("dp_altmode_configure rc=%d", ret);
		if (!ret)
			dp->state = DP_STATE_CONFIGURE;
		else {
			/* if we could not configure it,
			   no point staying in this alternate mode */
			dp->data.status = 0;
			dp->data.conf = 0;
			dp->state = DP_STATE_EXIT;
                }
	}

	return ret;
}

static int dp_altmode_configured(struct dp_altmode *dp)
{
	int ret;

	sysfs_notify(&dp->alt->dev.kobj, "displayport", "configuration");

	if (!dp->data.conf)
		return typec_altmode_notify(dp->alt, TYPEC_STATE_USB,
					    &dp->data);
	ret = dp_altmode_notify(dp);
	if (ret)
		return ret;

	sysfs_notify(&dp->alt->dev.kobj, "displayport", "pin_assignment");

	return 0;
}

static int dp_altmode_configure_vdm(struct dp_altmode *dp, u32 conf)
{
	int svdm_version = typec_altmode_get_svdm_version(dp->alt);
	u32 header;
	int ret;

	MOTO_ALTMODE("enter; conf=0x%04x, vdo=0x%x", conf, dp->data.status);
	if (svdm_version < 0)
		return svdm_version;

	header = DP_HEADER(dp, svdm_version, DP_CMD_CONFIGURE);

	ret = typec_altmode_notify(dp->alt, TYPEC_STATE_SAFE, &dp->data);
	if (ret) {
		dev_err(&dp->alt->dev,
			"unable to put to connector to safe mode\n");
		return ret;
	}

	ret = typec_altmode_vdm(dp->alt, header, &conf, 2);
	MOTO_ALTMODE("typec_altmode_vdm rc=%d", ret);
	if (ret) {
		if (DP_CONF_GET_PIN_ASSIGN(dp->data.conf)) {
			MOTO_ALTMODE("failed to send VDM, with pin!=0, pin=0x%x",
                                     DP_CONF_GET_PIN_ASSIGN(dp->data.conf));
		} else {
			typec_altmode_notify(dp->alt, TYPEC_STATE_USB,
					     &dp->data);
		}
	}

	return ret;
}

static void dp_altmode_work(struct work_struct *work)
{
	struct dp_altmode *dp = container_of(work, struct dp_altmode, work);
	int svdm_version;
	u32 header;
	u32 vdo;
	int ret;

	mutex_lock(&dp->lock);

	switch (dp->state) {
	case DP_STATE_ENTER:
		ret = typec_altmode_enter(dp->alt, NULL);
		MOTO_ALTMODE("typec_altmode_enter: rc=%d", ret);
		if (ret && ret != -EBUSY)
			dev_err(&dp->alt->dev, "failed to enter mode\n");
		break;
	case DP_STATE_UPDATE:
		svdm_version = typec_altmode_get_svdm_version(dp->alt);
		MOTO_ALTMODE("typec_altmode_get_svdm_version: rc=%d", svdm_version);
		if (svdm_version < 0)
			break;
		header = DP_HEADER(dp, svdm_version, DP_CMD_STATUS_UPDATE);
		vdo = 1;
		ret = typec_altmode_vdm(dp->alt, header, &vdo, 2);
		MOTO_ALTMODE("typec_altmode_vdm: rc=%d", ret);
		if (ret)
			dev_err(&dp->alt->dev,
				"unable to send Status Update command (%d)\n",
				ret);
		break;
	case DP_STATE_CONFIGURE:
		ret = dp_altmode_configure_vdm(dp, dp->data.conf);
		MOTO_ALTMODE("dp_altmode_configure_vdm: rc=%d", ret);
		if (ret)
			dev_err(&dp->alt->dev,
				"unable to send Configure command (%d)\n", ret);
		break;
	case DP_STATE_EXIT:
		if (typec_altmode_exit(dp->alt))
			dev_err(&dp->alt->dev, "Exit Mode Failed!\n");
		break;
	default:
		break;
	}

	dp->state = DP_STATE_IDLE;

	mutex_unlock(&dp->lock);
}

static void dp_altmode_attention(struct typec_altmode *alt, const u32 vdo)
{
	struct dp_altmode *dp = typec_altmode_get_drvdata(alt);
	u8 old_state;

	MOTO_ALTMODE("enter; vdo=0x%x", vdo);
	mutex_lock(&dp->lock);

	old_state = dp->state;
	dp->data.status = vdo;

	if (old_state != DP_STATE_IDLE)
		dev_warn(&alt->dev, "ATTENTION while processing state %d\n",
			 old_state);

	if (dp_altmode_status_update(dp))
		dev_warn(&alt->dev, "%s: status update failed\n", __func__);

	if (dp_altmode_notify(dp))
		dev_err(&alt->dev, "%s: notification failed\n", __func__);

	if (old_state == DP_STATE_IDLE && dp->state != DP_STATE_IDLE)
		schedule_work(&dp->work);

	mutex_unlock(&dp->lock);
}

static int dp_altmode_vdm(struct typec_altmode *alt,
			  const u32 hdr, const u32 *vdo, int count)
{
	struct dp_altmode *dp = typec_altmode_get_drvdata(alt);
	int cmd_type = PD_VDO_CMDT(hdr);
	int cmd = PD_VDO_CMD(hdr);
	int ret = 0;

	MOTO_ALTMODE("enter; cmt_type=%d, cmd=%d, vdo=0x%x, cnt=%d", cmd_type, cmd, *vdo, count);
	mutex_lock(&dp->lock);

	if (dp->state != DP_STATE_IDLE) {
		ret = -EBUSY;
		goto err_unlock;
	}

	switch (cmd_type) {
	case CMDT_RSP_ACK:
		MOTO_ALTMODE("CMDT_RSP_ACK");
		switch (cmd) {
		case CMD_ENTER_MODE:
			dp->state = DP_STATE_UPDATE;
			break;
		case CMD_EXIT_MODE:
			dp->data.status = 0;
			dp->data.conf = 0;
			break;
		case DP_CMD_STATUS_UPDATE:
			dp->data.status = *vdo;
			ret = dp_altmode_status_update(dp);
			break;
		case DP_CMD_CONFIGURE:
			ret = dp_altmode_configured(dp);
			break;
		default:
			break;
		}
		break;
	case CMDT_RSP_NAK:
		MOTO_ALTMODE("CMDT_RSP_NACK");
		switch (cmd) {
		case DP_CMD_CONFIGURE:
			dp->data.conf = 0;
			ret = dp_altmode_configured(dp);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	if (dp->state != DP_STATE_IDLE)
		schedule_work(&dp->work);

err_unlock:
	mutex_unlock(&dp->lock);
	return ret;
}

static int dp_altmode_activate(struct typec_altmode *alt, int activate)
{
	MOTO_ALTMODE("enter; activate=%d", activate);
	return activate ? typec_altmode_enter(alt, NULL) :
			  typec_altmode_exit(alt);
}

/*
    All roads lead to ops->notify().
    The caller is typec_altmode_notify(), which is called by this driver and
    by tcpm, either directly (for USB states) or through dp_altmode_notify(),
    for alternate modes.
    This function ties it all together, between tcpm, displayport, mux/switch,
    and dp_altmode drivers.
*/
static int dp_altmode_dispatch(struct typec_altmode *alt,
		unsigned long conf, void *data)
{
	struct dp_altmode *dp = typec_altmode_get_drvdata(alt);
	u8 state = 0;

	/* only go to MODAL states if we got some active config */
	if (DP_CONF_GET_PIN_ASSIGN(dp->data.conf))
		state = TYPEC_MODAL_STATE(get_count_order(DP_CONF_GET_PIN_ASSIGN(dp->data.conf)));

	MOTO_ALTMODE("conf=0x%x, data=%p, status=0x%x", conf, data, dp->data.status);
	/*
	   If we are in a "modal" state (DP), then it was dp_altmode_notify() that called
	   typec_altmode_notify(), and then we need to call dp_altmode_bridge() too.

	   There 2 possible scenarios where dp_altmode_notify() gets invoked:
	   - we got an ACK for CONFIGURE vdm, and this needs to turn into configure_cb()
	   - we got ATTENTION vdm, and this needs to turn into attention_cb()

	   For disconnect_cb(), we get here from newly added call to typec_altmode_notify()
	   in tcpm_unregister_altmodes(). It will be identifiable by conf == USB_STATE_SAFE
	   and data == NULL. We mark that by clearing dp->data, so the bridge can recognize
	   that too, and mark it for dp_altmode driver.
	*/
	if (state >= TYPEC_STATE_MODAL) {
		if (!data && conf == TYPEC_STATE_SAFE) {
			dp->data.conf = 0;
			dp->data.status = 0;
			MOTO_ALTMODE("Detected disconnect");
		}
		dp_altmode_bridge(dp);
	}
	return 0;
}

static const struct typec_altmode_ops dp_altmode_ops = {
	.attention = dp_altmode_attention,
	.vdm = dp_altmode_vdm,
	.activate = dp_altmode_activate,
	.notify = dp_altmode_dispatch,
};

static const char * const configurations[] = {
	[DP_CONF_USB]	= "USB",
	[DP_CONF_DFP_D]	= "source",
	[DP_CONF_UFP_D]	= "sink",
};

static ssize_t
configuration_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t size)
{
	struct dp_altmode *dp = dev_get_drvdata(dev);
	u32 conf;
	u32 cap;
	int con;
	int ret = 0;

	con = sysfs_match_string(configurations, buf);
	if (con < 0)
		return con;

	mutex_lock(&dp->lock);

	if (dp->state != DP_STATE_IDLE) {
		ret = -EBUSY;
		goto err_unlock;
	}

	cap = DP_CAP_CAPABILITY(dp->alt->vdo);

	if ((con == DP_CONF_DFP_D && !(cap & DP_CAP_DFP_D)) ||
	    (con == DP_CONF_UFP_D && !(cap & DP_CAP_UFP_D))) {
		ret = -EINVAL;
		goto err_unlock;
	}

	conf = dp->data.conf & ~DP_CONF_DUAL_D;
	conf |= con;

	if (dp->alt->active) {
		ret = dp_altmode_configure_vdm(dp, conf);
		if (ret)
			goto err_unlock;
	}

	dp->data.conf = conf;

err_unlock:
	mutex_unlock(&dp->lock);

	return ret ? ret : size;
}

static ssize_t configuration_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dp_altmode *dp = dev_get_drvdata(dev);
	int len;
	u8 cap;
	u8 cur;
	int i;

	mutex_lock(&dp->lock);

	cap = DP_CAP_CAPABILITY(dp->alt->vdo);
	cur = DP_CONF_CURRENTLY(dp->data.conf);

	len = sprintf(buf, "%s ", cur ? "USB" : "[USB]");

	for (i = 1; i < ARRAY_SIZE(configurations); i++) {
		if (i == cur)
			len += sprintf(buf + len, "[%s] ", configurations[i]);
		else if ((i == DP_CONF_DFP_D && cap & DP_CAP_DFP_D) ||
			 (i == DP_CONF_UFP_D && cap & DP_CAP_UFP_D))
			len += sprintf(buf + len, "%s ", configurations[i]);
	}

	mutex_unlock(&dp->lock);

	buf[len - 1] = '\n';
	return len;
}
static DEVICE_ATTR_RW(configuration);

static const char * const pin_assignments[] = {
	[DP_PIN_ASSIGN_A] = "A",
	[DP_PIN_ASSIGN_B] = "B",
	[DP_PIN_ASSIGN_C] = "C",
	[DP_PIN_ASSIGN_D] = "D",
	[DP_PIN_ASSIGN_E] = "E",
	[DP_PIN_ASSIGN_F] = "F",
};

static ssize_t
pin_assignment_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t size)
{
	struct dp_altmode *dp = dev_get_drvdata(dev);
	u8 assignments;
	u32 conf;
	int ret;

	ret = sysfs_match_string(pin_assignments, buf);
	if (ret < 0)
		return ret;

	conf = DP_CONF_SET_PIN_ASSIGN(BIT(ret));
	ret = 0;

	mutex_lock(&dp->lock);

	if (conf & dp->data.conf)
		goto out_unlock;

	if (dp->state != DP_STATE_IDLE) {
		ret = -EBUSY;
		goto out_unlock;
	}

	if (DP_CONF_CURRENTLY(dp->data.conf) == DP_CONF_DFP_D)
		assignments = DP_CAP_UFP_D_PIN_ASSIGN(dp->alt->vdo);
	else
		assignments = DP_CAP_DFP_D_PIN_ASSIGN(dp->alt->vdo);

	if (!(DP_CONF_GET_PIN_ASSIGN(conf) & assignments)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	conf |= dp->data.conf & ~DP_CONF_PIN_ASSIGNEMENT_MASK;

	/* Only send Configure command if a configuration has been set */
	if (dp->alt->active && DP_CONF_CURRENTLY(dp->data.conf)) {
		ret = dp_altmode_configure_vdm(dp, conf);
		if (ret)
			goto out_unlock;
	}

	dp->data.conf = conf;

out_unlock:
	mutex_unlock(&dp->lock);

	return ret ? ret : size;
}

static ssize_t pin_assignment_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct dp_altmode *dp = dev_get_drvdata(dev);
	u8 assignments;
	int len = 0;
	u8 cur;
	int i;

	mutex_lock(&dp->lock);

	cur = get_count_order(DP_CONF_GET_PIN_ASSIGN(dp->data.conf));

	if (DP_CONF_CURRENTLY(dp->data.conf) == DP_CONF_DFP_D)
		assignments = DP_CAP_UFP_D_PIN_ASSIGN(dp->alt->vdo);
	else
		assignments = DP_CAP_DFP_D_PIN_ASSIGN(dp->alt->vdo);

	for (i = 0; assignments; assignments >>= 1, i++) {
		if (assignments & 1) {
			if (i == cur)
				len += sprintf(buf + len, "[%s] ",
					       pin_assignments[i]);
			else
				len += sprintf(buf + len, "%s ",
					       pin_assignments[i]);
		}
	}

	mutex_unlock(&dp->lock);

	buf[len - 1] = '\n';
	return len;
}
static DEVICE_ATTR_RW(pin_assignment);

static struct attribute *dp_altmode_attrs[] = {
	&dev_attr_configuration.attr,
	&dev_attr_pin_assignment.attr,
	NULL
};

static const struct attribute_group dp_altmode_group = {
	.name = "displayport",
	.attrs = dp_altmode_attrs,
};

int dp_altmode_probe(struct typec_altmode *alt)
{
	const struct typec_altmode *port = typec_altmode_get_partner(alt);
	struct dp_altmode *dp;
	int ret;

	/* FIXME: Port can only be DFP_U. */

	/* Make sure we have compatiple pin configurations */
	if (!(DP_CAP_DFP_D_PIN_ASSIGN(port->vdo) &
	      DP_CAP_UFP_D_PIN_ASSIGN(alt->vdo)) &&
	    !(DP_CAP_UFP_D_PIN_ASSIGN(port->vdo) &
	      DP_CAP_DFP_D_PIN_ASSIGN(alt->vdo)))
		return -ENODEV;

	ret = sysfs_create_group(&alt->dev.kobj, &dp_altmode_group);
	if (ret)
		return ret;

	dp = devm_kzalloc(&alt->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	INIT_WORK(&dp->work, dp_altmode_work);
	mutex_init(&dp->lock);
	dp->port = port;
	dp->alt = alt;
	dp->typec_bridge = &bridge;
	MOTO_ALTMODE("bridge pointer assigned");

	alt->desc = "DisplayPort";
	alt->ops = &dp_altmode_ops;

	typec_altmode_set_drvdata(alt, dp);

	dp->state = DP_STATE_ENTER;
	schedule_work(&dp->work);

	return 0;
}
EXPORT_SYMBOL_GPL(dp_altmode_probe);

void dp_altmode_remove(struct typec_altmode *alt)
{
	struct dp_altmode *dp = typec_altmode_get_drvdata(alt);

	sysfs_remove_group(&alt->dev.kobj, &dp_altmode_group);
	cancel_work_sync(&dp->work);
}
EXPORT_SYMBOL_GPL(dp_altmode_remove);

static const struct typec_device_id dp_typec_id[] = {
	{ USB_TYPEC_DP_SID, USB_TYPEC_DP_MODE },
	{ },
};
MODULE_DEVICE_TABLE(typec, dp_typec_id);

static struct typec_altmode_driver dp_altmode_driver = {
	.id_table = dp_typec_id,
	.probe = dp_altmode_probe,
	.remove = dp_altmode_remove,
	.driver = {
		.name = "typec_displayport",
		.owner = THIS_MODULE,
	},
};
module_typec_altmode_driver(dp_altmode_driver);

MODULE_AUTHOR("Heikki Krogerus <heikki.krogerus@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DisplayPort Alternate Mode");
