/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file stmvl53l1_module-i2c.c
 *
 *  implement STM VL53L1 module interface i2c wrapper + control
 *  using linux native i2c + gpio + reg api
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/version.h>

/*
 * power specific includes
 */
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "stmvl53l1-i2c.h"
#include "stmvl53l1.h"

/** @ingroup drv_port
 * @{
 */

/**
 * control specific debug message echo
 *
 * Set to 0/1 do not remove
 *
 * most dbg warn err messages goes true main driver macro
 * this one permit some specific debug without activating all main dbg
 */
#define MODI2C_DEBUG	0

#define VL53L1_VDD_MIN 2850000
#define VL53L1_VDD_MAX 2850000

struct vl53l1_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
} g_pinctrl_info;

/**
 * i2c client assigned to our driver
 *
 * this is use for stm test purpose as we fake client create and regstration
 * we stores the i2c client for release in clean-up overwise we wan't reload
 * the module multiple time
 *
 * in a normal dev tree prod system this is not required
 */
static struct i2c_client *stm_test_i2c_client;

/* TODO this shall be an array if to register multiple device
 * not is null until set durign probe drievr add
 */

/*
 * pi3:
 * insmod stmvl53l1.ko force_device=1 adapter_nb=1 xsdn_gpio_nb=19
 * intr_gpio_nb=16 pwren_gpio_nb=12
 *
 * panda
 * insmod stmvl53l1.ko force_device=1 adapter_nb=4 xsdn_gpio_nb=56
 * intr_gpio_nb=59 pwren_gpio_nb=55
*/

static int force_device;
static int adapter_nb = -1;
static int xsdn_gpio_nb = -1;
static int pwren_gpio_nb = -1;
static int intr_gpio_nb = -1;

module_param(force_device, int, 0);
MODULE_PARM_DESC(force_device, "force device insertion at module init");

module_param(adapter_nb, int, 0);
MODULE_PARM_DESC(adapter_nb, "i2c adapter to use");

module_param(xsdn_gpio_nb, int, 0);
MODULE_PARM_DESC(xsdn_gpio_nb, "select gpio numer to use for vl53l1 reset");

module_param(pwren_gpio_nb, int, 0);
MODULE_PARM_DESC(pwren_gpio_nb, "select gpio numer to use for vl53l1 power");

module_param(intr_gpio_nb, int, 0);
MODULE_PARM_DESC(intr_gpio_nb, "select gpio numer to use for vl53l1 interrupt");

/**
 * warn message
 *
 * @warning use only in scope where i2c_data ptr is present
 **/
#define modi2c_warn(fmt, ...)\
	dev_WARN(&i2c_data->client->dev, fmt, ##__VA_ARGS__)

/**
 * err message
 *
 * @warning use only in scope where i2c_data ptr is present
 */
#define modi2c_err(fmt, ...)\
	dev_err(&i2c_data->client->dev, fmt, ##__VA_ARGS__)



#if MODI2C_DEBUG
#	define modi2c_dbg(fmt, ...)\
		pr_devel("%s "fmt"\n", __func__, ##__VA_ARGS__)
#else
#	define modi2c_dbg(...)	(void)0
#endif

static int insert_device(void)
{
	int ret = 0;
	struct i2c_adapter *adapter;
	struct i2c_board_info info = {
		.type = "stmvl53l1",
		.addr = STMVL53L1_SLAVE_ADDR,
	};

	adapter = i2c_get_adapter(adapter_nb);
	if (!adapter)
		ret = -EINVAL;
	stm_test_i2c_client = i2c_new_device(adapter, &info);
	if (!stm_test_i2c_client)
		ret = -EINVAL;

	return ret;
}

static int get_xsdn(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;

	i2c_data->io_flag.xsdn_owned = 0;
	if (i2c_data->xsdn_gpio == -1) {
		vl53l1_errmsg("reset gpio is required");
		rc = -ENODEV;
		goto no_gpio;
	}

	vl53l1_dbgmsg("request xsdn_gpio %d", i2c_data->xsdn_gpio);
	rc = gpio_request(i2c_data->xsdn_gpio, "vl53l1_xsdn");
	if (rc) {
		vl53l1_errmsg("fail to acquire xsdn %d", rc);
		goto request_failed;
	}

	rc = gpio_direction_output(i2c_data->xsdn_gpio, 0);
	if (rc) {
		vl53l1_errmsg("fail to configure xsdn as output %d", rc);
		goto direction_failed;
	}
	i2c_data->io_flag.xsdn_owned = 1;

	return rc;

direction_failed:
	gpio_free(i2c_data->xsdn_gpio);

request_failed:
no_gpio:
	return rc;
}

static void put_xsdn(struct i2c_data *i2c_data)
{
	if (i2c_data->io_flag.xsdn_owned) {
		vl53l1_dbgmsg("release xsdn_gpio %d", i2c_data->xsdn_gpio);
		gpio_free(i2c_data->xsdn_gpio);
		i2c_data->io_flag.xsdn_owned = 0;
		i2c_data->xsdn_gpio = -1;
	}
	i2c_data->xsdn_gpio = -1;
}

static int get_pwren(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;

	i2c_data->io_flag.pwr_owned = 0;
	if (i2c_data->pwren_gpio == -1) {
		vl53l1_wanrmsg("pwren gpio disable");
		goto no_gpio;
	}

	vl53l1_dbgmsg("request pwren_gpio %d", i2c_data->pwren_gpio);
	rc = gpio_request(i2c_data->pwren_gpio, "vl53l1_pwren");
	if (rc) {
		vl53l1_errmsg("fail to acquire pwren %d", rc);
		goto request_failed;
	}

	rc = gpio_direction_output(i2c_data->pwren_gpio, 0);
	if (rc) {
		vl53l1_errmsg("fail to configure pwren as output %d", rc);
		goto direction_failed;
	}
	i2c_data->io_flag.pwr_owned = 1;

	return rc;

direction_failed:
	gpio_free(i2c_data->xsdn_gpio);

request_failed:
no_gpio:
	return rc;
}

static void put_pwren(struct i2c_data *i2c_data)
{
	if (i2c_data->io_flag.pwr_owned) {
		vl53l1_dbgmsg("release pwren_gpio %d", i2c_data->pwren_gpio);
		gpio_free(i2c_data->pwren_gpio);
		i2c_data->io_flag.pwr_owned = 0;
		i2c_data->pwren_gpio = -1;
	}
	i2c_data->pwren_gpio = -1;
}

static int get_intr(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;

	i2c_data->io_flag.intr_owned = 0;
	if (i2c_data->intr_gpio == -1) {
		vl53l1_wanrmsg("no interrupt gpio");
		goto no_gpio;
	}

	vl53l1_dbgmsg("request intr_gpio %d", i2c_data->intr_gpio);
	rc = gpio_request(i2c_data->intr_gpio, "vl53l1_intr");
	if (rc) {
		vl53l1_errmsg("fail to acquire intr %d", rc);
		goto request_failed;
	}

	rc = gpio_direction_input(i2c_data->intr_gpio);
	if (rc) {
		vl53l1_errmsg("fail to configure intr as input %d", rc);
		goto direction_failed;
	}

	i2c_data->irq = gpio_to_irq(i2c_data->intr_gpio);
	if (i2c_data->irq < 0) {
		vl53l1_errmsg("fail to map GPIO: %d to interrupt:%d\n",
				i2c_data->intr_gpio, i2c_data->irq);
		goto irq_failed;
	}
	i2c_data->io_flag.intr_owned = 1;

	return rc;

irq_failed:
direction_failed:
	gpio_free(i2c_data->intr_gpio);

request_failed:
no_gpio:
	return rc;
}

static void put_intr(struct i2c_data *i2c_data)
{
	if (i2c_data->io_flag.intr_owned) {
		if (i2c_data->io_flag.intr_started) {
			free_irq(i2c_data->irq, i2c_data);
			i2c_data->io_flag.intr_started = 0;
		}
		vl53l1_dbgmsg("release intr_gpio %d", i2c_data->intr_gpio);
		gpio_free(i2c_data->intr_gpio);
		i2c_data->io_flag.intr_owned = 0;
	}
	i2c_data->intr_gpio = -1;
}

/**
 *  parse dev tree for all platform specific input
 */
static int stmvl53l1_parse_tree(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;

	/* if force device is in use then gpio nb comes from module param else
	 * we use devicetree.
	 */
	i2c_data->vdd = NULL;
	i2c_data->pwren_gpio = -1;
	i2c_data->xsdn_gpio = -1;
	i2c_data->intr_gpio = -1;
	if (force_device) {
		i2c_data->xsdn_gpio = xsdn_gpio_nb;
		i2c_data->pwren_gpio = pwren_gpio_nb;
		i2c_data->intr_gpio = intr_gpio_nb;
	} else if (dev->of_node) {
		/* power : either vdd or pwren_gpio. try reulator first */
		i2c_data->vdd = regulator_get(dev, "vdd-vl53l1");
		if (IS_ERR(i2c_data->vdd) || i2c_data->vdd == NULL) {
			i2c_data->vdd = NULL;
			/* try gpio */
			rc = of_property_read_u32_array(dev->of_node,
				"pwren-gpio", &i2c_data->pwren_gpio, 1);
			if (rc) {
				i2c_data->pwren_gpio = -1;
				vl53l1_wanrmsg(
			"no regulator, nor power gpio => power ctrl disabled");
			}
		}

		g_pinctrl_info.pinctrl = devm_pinctrl_get(dev);
		if (!IS_ERR_OR_NULL(g_pinctrl_info.pinctrl)) {
			g_pinctrl_info.gpio_state_active =
				pinctrl_lookup_state(g_pinctrl_info.pinctrl,
						"laser_default");

			g_pinctrl_info.gpio_state_suspend =
				pinctrl_lookup_state(g_pinctrl_info.pinctrl,
						"laser_suspend");
		} else
			vl53l1_errmsg("Getting pinctrl handle failed\n");

		rc = of_property_read_u32_array(dev->of_node, "xsdn-gpio",
			&i2c_data->xsdn_gpio, 1);
		if (rc) {
			vl53l1_wanrmsg("Unable to find xsdn-gpio %d %d",
				rc, i2c_data->xsdn_gpio);
        	if (of_gpio_count(dev->of_node) >= 2)
            	i2c_data->xsdn_gpio = of_get_gpio(dev->of_node, 0);
			else
				i2c_data->xsdn_gpio = -1;
		}
		rc = of_property_read_u32_array(dev->of_node, "intr-gpio",
			&i2c_data->intr_gpio, 1);
		if (rc) {
			vl53l1_wanrmsg("Unable to find intr-gpio %d %d",
				rc, i2c_data->intr_gpio);

			if (of_gpio_count(dev->of_node) >= 2)
				i2c_data->intr_gpio = of_get_gpio(dev->of_node, 1);
			else
				i2c_data->intr_gpio = -1;
		}
	}

	/* configure gpios */
	rc = get_xsdn(dev, i2c_data);
	if (rc)
		goto no_xsdn;
	rc = get_pwren(dev, i2c_data);
	if (rc)
		goto no_pwren;
	rc = get_intr(dev, i2c_data);
	if (rc)
		goto no_intr;

	return rc;

no_intr:
	if (i2c_data->vdd) {
		regulator_put(i2c_data->vdd);
		i2c_data->vdd = NULL;
	}
	put_pwren(i2c_data);
no_pwren:
	put_xsdn(i2c_data);
no_xsdn:
	return rc;
}

static void stmvl53l1_release_gpios(struct i2c_data *i2c_data)
{
	put_xsdn(i2c_data);
	if (i2c_data->vdd) {
		regulator_put(i2c_data->vdd);
		i2c_data->vdd = NULL;
	}
	put_pwren(i2c_data);
	put_intr(i2c_data);
}

static int stmvl53l1_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc = 0;
	struct stmvl53l1_data *vl53l1_data = NULL;
	struct i2c_data *i2c_data = NULL;

	vl53l1_dbgmsg("Enter\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
		rc = -EIO;
		return rc;
	}

	vl53l1_data = kzalloc(sizeof(struct stmvl53l1_data), GFP_KERNEL);
	if (!vl53l1_data) {
		rc = -ENOMEM;
		return rc;
	}
	if (vl53l1_data) {
		vl53l1_data->client_object =
				kzalloc(sizeof(struct i2c_data), GFP_KERNEL);
		if (!vl53l1_data)
			goto done_freemem;
		i2c_data = (struct i2c_data *)vl53l1_data->client_object;
	}
	i2c_data->client = client;
	i2c_data->vl53l1_data = vl53l1_data;
	i2c_data->irq = -1 ; /* init to no irq */

	/* parse and configure hardware */
	rc = stmvl53l1_parse_tree(&i2c_data->client->dev, i2c_data);
	if (rc)
		goto done_freemem;

	/* setup device name */
	/* vl53l1_data->dev_name = dev_name(&client->dev); */

	/* setup client data */
	i2c_set_clientdata(client, vl53l1_data);

	/* end up by core driver setup */
	rc = stmvl53l1_setup(vl53l1_data);
	if (rc)
		goto release_gpios;
	vl53l1_dbgmsg("End\n");

	return rc;

release_gpios:
	stmvl53l1_release_gpios(i2c_data);

done_freemem:
	/* kfree safe against NULL */
	kfree(vl53l1_data);
	kfree(i2c_data);

	return -1;
}

static int stmvl53l1_remove(struct i2c_client *client)
{
	struct stmvl53l1_data *data = i2c_get_clientdata(client);
	struct i2c_data *i2c_data = (struct i2c_data *)data->client_object;

	vl53l1_dbgmsg("Enter\n");
	/* main driver cleanup */
	stmvl53l1_cleanup(data);

	/* release gpios */
	stmvl53l1_release_gpios(i2c_data);

	kfree(data->client_object);
	kfree(data);

	vl53l1_dbgmsg("End\n");

	return 0;
}

static const struct i2c_device_id stmvl53l1_id[] = {
	{ STMVL53L1_DRV_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, stmvl53l1_id);

static const struct of_device_id st_stmvl53l1_dt_match[] = {
	{ .compatible = "st,"STMVL53L1_DRV_NAME, },
	{ },
};

static struct i2c_driver stmvl53l1_driver = {
	.driver = {
		.name	= STMVL53L1_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = st_stmvl53l1_dt_match,
	},
	.probe	= stmvl53l1_probe,
	.remove	= stmvl53l1_remove,
	.id_table = stmvl53l1_id,

};

/**
 * give power to device
 *
 * @param object  the i2c layer object
 * @return
 */
int stmvl53l1_power_up_i2c(void *object)
{
	int rc = 0;
	struct i2c_data *data = (struct i2c_data *) object;

	vl53l1_dbgmsg("Enter\n");

	/* turn on power */
	if (data->vdd) {
		rc = regulator_set_voltage(data->vdd, VL53L1_VDD_MIN,
				VL53L1_VDD_MAX);
		if (rc) {
			vl53l1_errmsg("set volt fail %d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vdd);
		if (rc) {
			vl53l1_errmsg("fail to turn on regulator");
			return rc;
		}
	} else if (data->pwren_gpio != -1) {
		gpio_set_value(data->pwren_gpio, 1);
		vl53l1_info("slow power on");
	} else
		vl53l1_wanrmsg("no power control");

	return rc;
}

/**
 * remove power to device (reset it)
 *
 * TODO we may have dedicate reset interface to optimize startup time
 * @param i2c_object the i2c layer object
 * @return 0 on success
 */
int stmvl53l1_power_down_i2c(void *i2c_object)
{
	struct i2c_data *data = (struct i2c_data *) i2c_object;
	int rc = 0;

	vl53l1_dbgmsg("Enter\n");

	/* turn off power */
	if (data->vdd) {
		rc = regulator_disable(data->vdd);
		if (rc)
			vl53l1_errmsg("reg disable failed. rc=%d\n",
				rc);
	} else if (data->pwren_gpio != -1) {
		gpio_set_value(data->pwren_gpio, 0);
	}
	vl53l1_dbgmsg("power off");

	vl53l1_dbgmsg("End\n");

	return rc;
}

/**
 * release device reset
 *
 * @param i2c_object the i2c layer object
 * @return 0 on success
 */
int stmvl53l1_reset_release_i2c(void *i2c_object)
{
	int rc;
	struct i2c_data *data = (struct i2c_data *) i2c_object;

	vl53l1_dbgmsg("Enter\n");

	/* release reset */
	gpio_set_value(data->xsdn_gpio, 1);

	pinctrl_select_state(g_pinctrl_info.pinctrl,
		g_pinctrl_info.gpio_state_active);

	/* and now wait for device end of boot */
	data->vl53l1_data->is_delay_allowed = true;
	rc = VL53L1_WaitDeviceBooted(&data->vl53l1_data->stdev);
	data->vl53l1_data->is_delay_allowed = false;
	if (rc)
		vl53l1_errmsg("boot fail with error %d", rc);

	vl53l1_dbgmsg("End\n");

	return rc;
}

/**
 * put device under reset
 *
 * @param i2c_object the i2c layer object
 * @return 0 on success
 */
int stmvl53l1_reset_hold_i2c(void *i2c_object)
{
	struct i2c_data *data = (struct i2c_data *) i2c_object;

	vl53l1_dbgmsg("Enter\n");

	gpio_set_value(data->xsdn_gpio, 0);

	pinctrl_select_state(g_pinctrl_info.pinctrl,
		g_pinctrl_info.gpio_state_suspend);

	vl53l1_dbgmsg("End\n");

	return 0;
}

int stmvl53l1_init_i2c(void)
{
	int ret = 0;

	vl53l1_dbgmsg("Enter\n");

	/* register as a i2c client device */
	ret = i2c_add_driver(&stmvl53l1_driver);
	if (ret)
		vl53l1_errmsg("%d erro ret:%d\n", __LINE__, ret);

	if (!ret && force_device)
		ret = insert_device();

	vl53l1_dbgmsg("End with rc:%d\n", ret);

	return ret;
}


void stmvl53l1_clean_up_i2c(void)
{
	if (stm_test_i2c_client) {
		vl53l1_dbgmsg("to unregister i2c client\n");
		i2c_unregister_device(stm_test_i2c_client);
	}
}

static irqreturn_t stmvl53l1_irq_handler_i2c(int vec, void *info)
{
	struct i2c_data *i2c_data = (struct i2c_data *)info;

	if (i2c_data->irq == vec) {
		modi2c_dbg("irq");
		stmvl53l1_intr_handler(i2c_data->vl53l1_data);
		modi2c_dbg("over");
	} else {
		if (!i2c_data->msg_flag.unhandled_irq_vec) {
			modi2c_warn("unmatching vec %d != %d\n",
					vec, i2c_data->irq);
			i2c_data->msg_flag.unhandled_irq_vec = 1;
		}
	}

	return IRQ_HANDLED;
}

/**
 * enable and start intr handling
 *
 * TODO we may need the sae for "disable"
 *
 * @param object  our i2c_data specific object
 * @param poll_mode [in/out] set to force mode clear to use irq
 * @return 0 on success and set ->poll_mode if it faill ranging wan't start
 */
int stmvl53l1_start_intr(void *object, int *poll_mode)
{
	struct i2c_data *i2c_data;
	int rc;

	i2c_data = (struct i2c_data *)object;
	/* irq and gpio acquire config done in parse_tree */
	if (i2c_data->irq < 0) {
		/* the i2c tree as no intr force polling mode */
		*poll_mode = -1;
		return 0;
	}
	/* clear irq warning report enabe it again for this session */
	i2c_data->msg_flag.unhandled_irq_vec = 0;
	/* if started do no nothing */
	if (i2c_data->io_flag.intr_started) {
		/* nothing to do */
		*poll_mode = 0;
		return 0;
	}

	vl53l1_dbgmsg("to register_irq:%d\n", i2c_data->irq);
	rc = request_threaded_irq(i2c_data->irq, NULL,
			stmvl53l1_irq_handler_i2c,
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			/* todo adapt name to dev ?*/
			"vl53l1_interrupt",
			(void *)i2c_data);
	if (rc) {
		vl53l1_errmsg("fail to req threaded irq rc=%d\n", rc);
		/**TODO fall back to poll when irq request fail here
		 * revisit as needed
		 */
		vl53l1_errmsg("no irq fall back to poll");
		*poll_mode = -1;
		rc = 0; /* mask error due to fall back*/
	} else {
		vl53l1_dbgmsg("irq %d now handled\n", i2c_data->irq);
		i2c_data->io_flag.intr_started = 1;
		*poll_mode = 0;
	}
	return rc;
}

void __exit stmvl53l1_exit_i2c(void *i2c_object)
{
	vl53l1_dbgmsg("Enter\n");
	i2c_del_driver(&stmvl53l1_driver);
	vl53l1_dbgmsg("End\n");
}
