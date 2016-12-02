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

#include <soc/qcom/camera2.h>

#include "stmvl53l1-i2c.h"
#include "stmvl53l1.h"

#ifndef CAMERA_CCI


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


#ifdef STM_TEST
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

#endif


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

struct msm_pinctrl_info g_pinctrl_info;

/**
 *  parse dev tree for pwren gpio and init it
 *
 * @param dev      dev associated to i2c_client
 * @param i2c_data private data structure
 * @return 0 on success even if parse fail and work w/o
 */
int parse_pwr_en(struct device *dev, struct i2c_data *i2c_data)
{
	int rc;
	/* TODO parse tree for pwr en gpio */
	/** @note ST test mode => pwr_en gpio pin hardcoded in the i2c object*/
	if (i2c_data->pwren_gpio != -1) {
		/* request the gpio */
		rc = gpio_request(i2c_data->pwren_gpio, "vl53l1_pwr");
		if (rc != 0) {
			/** @warning on ST platform pwr_en acquire fail but is
			 * ok at risk to clash with user space usage
			 * revisit or force this if same behavior is required
			 */
#ifndef STM_TEST
			vl53l1_errmsg("fail acquire gpio %d xsdn\n",
					i2c_data->pwren_gpio);
			/* kill internal value but keep going w/o pwr control*/
			i2c_data->pwren_gpio = -1;
			rc = 0;
			goto done_pwr;
#else
			vl53l1_info("TEST MODE pwr_en gpio %d not owned",
					i2c_data->xsdn_gpio);
#endif
		} else {
			i2c_data->io_flag.pwr_owned = 1;
		}

		rc = gpio_direction_output(i2c_data->pwren_gpio, 0);
		if (rc) {
			vl53l1_errmsg("fail to set pwr %d as out\n",
					i2c_data->pwren_gpio);
			/* kill gpio use and release if needed*/
			if (i2c_data->io_flag.pwr_owned) {
				gpio_free(i2c_data->pwren_gpio);
				i2c_data->io_flag.pwr_owned = 0;
			}
			i2c_data->pwren_gpio = -1;
			rc = 0;
		}
	}
	goto done_pwr;

done_pwr:
	if (i2c_data->pwren_gpio == -1) {
		vl53l1_errmsg("no pwr_en gpio default to no pwr control\n");
		/* FIXME if pwr en is mandatory return error here*/
		return 0;
	} else {
		vl53l1_info("pwr_en gpio is %d", i2c_data->pwren_gpio);
		return 0;
	}
}

/**
 * parse tree and init xsdn (shut down  reset )
 *
 * @param dev      dev associated to i2c_client
 * @param i2c_data private data structure
 * @return 0 on success even if parse fail and work w/o
 */
static int parse_xsdn(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;
	/** add here code to parse tree for xsdn gpio
	 * on st test mode it already come in data
	 */
	if (dev->of_node != NULL) {
		if (of_gpio_count(dev->of_node) >= 2)
			i2c_data->xsdn_gpio = of_get_gpio(dev->of_node, 0);
	}
	if (i2c_data->xsdn_gpio != -1) {
		rc = gpio_request(i2c_data->xsdn_gpio, "vl53l1_xsdn");
		if (rc != 0) {
		/**@warning on ST platform xsdn_gpio acquire fail but is ok at
		 * risk to clash with user space usage
		 * revisit or force this if same behavior is required
		 */
#ifndef STM_TEST
			vl53l1_errmsg("acquire gpio %d xsdn for\n",
					i2c_data->xsdn_gpio);

			/* kill io value and mask error 0 if ok to run
			 * w/o xsdn control
			 * if not let rc and branch to done
			 */
			i2c_data->xsdn_gpio = -1;
			rc = 0;
#else
			rc = 0;
			vl53l1_errmsg("TEST MODE gpio %d xsdn fail but ok\n",
					i2c_data->xsdn_gpio);
#endif
		} else {
			i2c_data->io_flag.xsdn_owned = 1;
		}
		/* note that we are here putting the device under reset !*/
		rc = gpio_direction_output(i2c_data->xsdn_gpio, 0);
		if (rc) {
			vl53l1_errmsg("fail to set xsdn gpio %d out\n",
					i2c_data->xsdn_gpio);
			/**@note even if direction set fail it may be possible
			 * to keep w/o xsdn revisit
			 * free gpio , kill io and make rc=0
			 */
			goto err_io;
		}
	}
	goto done_xsdn;
done_xsdn:
	if (i2c_data->xsdn_gpio == -1) {
		vl53l1_errmsg("no xsdn gpio default to no reset control\n");
		/* if xsdn mandatory return error here  */
	} else
		vl53l1_info("xsdn gpio is %d", i2c_data->xsdn_gpio);
	return rc;
err_io:
	if (i2c_data->io_flag.xsdn_owned) {
		gpio_free(i2c_data->xsdn_gpio);
		i2c_data->io_flag.xsdn_owned = 0;
	}
	return rc;
}

/**
 *  parse dev tree for pwren gpio and init it
 *
 * @param dev      dev associated to i2c_client
 * @param i2c_data private data structure
 * @return 0 on success even if parse fail and work w/o
 */
static int parse_irq(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;
	/** TODO add code to parse tree an set i2c_data->intr_gpio = your gpio
	 * if irq is not gpio driven but coem diretcly from tree/plat i2c_data
	 * set i2c_data->irq  = my_irq (-1 if invalid )
	 * return 0;
	 */
	if (dev->of_node != NULL) {
		if (of_gpio_count(dev->of_node) >= 2)
			i2c_data->intr_gpio = of_get_gpio(dev->of_node, 1);
	}

	g_pinctrl_info.pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(g_pinctrl_info.pinctrl)) {
		vl53l1_errmsg("Getting pinctrl handle failed\n");
		goto err_irq;
	}

	g_pinctrl_info.gpio_state_active =
		pinctrl_lookup_state(g_pinctrl_info.pinctrl, "laser_default");

	g_pinctrl_info.gpio_state_suspend =
		pinctrl_lookup_state(g_pinctrl_info.pinctrl, "laser_suspend");

	i2c_data->irq = -1; /* init to no irq hooked */
	if (i2c_data->intr_gpio != -1) {
		rc = gpio_request(i2c_data->intr_gpio, "vl53l1_intr");
		if (rc != 0) {
/**@note if intr gpio request fail but it's ok to use revisit this code
 * do the same as xsdn or pwr_en io
 * bypass check but do not set intr owned
 */
			vl53l1_errmsg("fail to acquire gpio %d intr\n",
					i2c_data->intr_gpio);
			goto err_irq;
		} else
			i2c_data->io_flag.intr_owned = 1;

		rc = gpio_direction_input(i2c_data->intr_gpio);
		if (rc) {
			vl53l1_errmsg("fail set intr gpio %d as input\n",
					i2c_data->intr_gpio);
			goto err_irq;
		}
		i2c_data->irq = gpio_to_irq(i2c_data->intr_gpio);
		if (i2c_data->irq < 0) {
			vl53l1_errmsg("fail to map GPIO: %d to interrupt:%d\n",
					i2c_data->intr_gpio, i2c_data->irq);
			goto err_irq;
		}
	}
/* done_intr: */
/* branch here to finalize irq parsing if ok to use w/o irq */
	/* gpio int >0 and irq>0  only if  irq setup ok*/
	if (i2c_data->intr_gpio == -1) {
		vl53l1_info("no interrupt in tree poll only");
	} else
	if (i2c_data->irq < 0) {
		vl53l1_info("intr gpio %d issues falling back to poll only",
				i2c_data->intr_gpio);
		rc = 0; /* mask any error */
	} else {
		vl53l1_info("intr gpio %d irq %d", i2c_data->intr_gpio,
				i2c_data->irq);
	}
	return rc;

/* branch here to release resources and return actual rc */
err_irq:
	i2c_data->irq = -1; /* do not use irq */
	if (i2c_data->io_flag.intr_owned && i2c_data->intr_gpio != -1) {
		gpio_free(i2c_data->intr_gpio);
		i2c_data->io_flag.intr_owned = 0;
	}
	return rc;
}

static void pwr_gpio_free(struct i2c_data *i2c_data)
{
	if (i2c_data->pwren_gpio != -1 && i2c_data->io_flag.pwr_owned) {
		vl53l1_dbgmsg("pwren_gpio free %d", i2c_data->pwren_gpio);
		gpio_free(i2c_data->pwren_gpio);
		i2c_data->io_flag.pwr_owned = 0;
	}
}

static void xsdn_gpio_free(struct i2c_data *i2c_data)
{
	if (i2c_data->xsdn_gpio != -1 && i2c_data->io_flag.xsdn_owned) {
		vl53l1_dbgmsg("xsdn gpio free %d", i2c_data->xsdn_gpio);
		gpio_free(i2c_data->xsdn_gpio);
		i2c_data->io_flag.xsdn_owned = 0;
	}
}

/**
 *  parse dev tree for all platform specific input
 */
static int stmvl53l1_parse_tree(struct device *dev, struct i2c_data *i2c_data)
{
	int rc = 0;

	vl53l1_dbgmsg("Enter\n");
#ifdef CFG_STMVL53L1_HAVE_REGULATOR
	if (dev->of_node) {
		i2c_data->vana = regulator_get(dev, "vdd");
		if (IS_ERR(i2c_data->vana)) {
			vl53l1_errmsg("vdd supply is not provided\n");
			/* we may have alternate do not return error */
		}
	} else {
		vl53l1_errmsg(
				"no node find for dev %s will use polling"
				" without reset and no pwr control\n",
				dev->init_name);
		/* FIXME if regulator and dev tree is mandatory do error here */
	}
#endif
	rc = parse_pwr_en(dev, i2c_data);
	if (rc)
		goto err_reg;
	rc = parse_xsdn(dev, i2c_data);
	if (rc)
		goto err_pwr;
	rc = parse_irq(dev, i2c_data);
	if (rc)
		goto err_xsdn;

	/* else we have done info already and we'll work in polling*/
	return rc;
err_xsdn:
	xsdn_gpio_free(i2c_data);
err_pwr:
	pwr_gpio_free(i2c_data);
err_reg:
#ifdef CFG_STMVL53L1_HAVE_REGULATOR
	if (i2c_data->vana) {
		regulator_put(i2c_data->vana);
		i2c_data->vana = NULL;
	}
#endif
	return rc;
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
#ifdef STM_TEST
	/* FIXME tmp hack until we clean things ... */
	/* below 3.8.0 we say we got a panda else we got a pi3 .... */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0)
	/** STM test purpose we do force gpio pin value */
	i2c_data->pwren_gpio = 55; /*panda es wired to 55 */
	i2c_data->xsdn_gpio = 56; /* panda es wired to 56 */
	i2c_data->intr_gpio = 59; /* panda es wired to 59 */
#else
	/** STM test purpose we do force gpio pin value */
	i2c_data->pwren_gpio = 12; /*panda es wired to 12 */
	i2c_data->xsdn_gpio = 19; /* panda es wired to 19 */
	i2c_data->intr_gpio = 16; /* panda es wired to 16 */
#endif
#else
	i2c_data->pwren_gpio = -1;
	i2c_data->xsdn_gpio = -1;
	i2c_data->intr_gpio = -1;
#endif
	/* setup regulator */
	rc = stmvl53l1_parse_tree(&i2c_data->client->dev, i2c_data);
	if (rc)
		goto done_freemem;

	/* setup device name */
	/* vl53l1_data->dev_name = dev_name(&client->dev); */

	/* setup device data */
	dev_set_drvdata(&client->dev, vl53l1_data);

	/* setup client data */
	i2c_set_clientdata(client, vl53l1_data);

	/* init default value */
	i2c_data->power_up = 0;

	/* end up by core driver setup */
	rc = stmvl53l1_setup(vl53l1_data);
	vl53l1_dbgmsg("End\n");
	if (rc)
		;/* TODO free mmem */
	return rc;

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
	/* Power down and reset the device */
	stmvl53l1_power_down_i2c(i2c_data);
	/* main driver cleanup */
	stmvl53l1_cleanup(data);

	pwr_gpio_free(i2c_data);
	xsdn_gpio_free(i2c_data);

	if (i2c_data->irq >= 0 && i2c_data->io_flag.intr_started) {
		vl53l1_dbgmsg("stop handling  irq %d\n", i2c_data->irq);
		free_irq(i2c_data->irq, i2c_data);
	}
	i2c_data->irq = -1;
	if (i2c_data->intr_gpio != -1 && i2c_data->io_flag.intr_owned) {
		/* release irq handler if started ? */
		vl53l1_dbgmsg("intr_gpio free %d", i2c_data->intr_gpio);
		gpio_free(i2c_data->intr_gpio);
		i2c_data->io_flag.intr_owned = 0;
	}

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
 * @param preset_flag  [in/out] indicate if the if to bet set if devcie got
 *  reset do no chnage if reset
 * @return
 */
int stmvl53l1_power_up_i2c(void *object, unsigned int *preset_flag)
{
	int ret = 0;
	struct i2c_data *data = (struct i2c_data *)object;
	int powered = 0;

	vl53l1_dbgmsg("Enter\n");
#ifdef CFG_STMVL53L1_HAVE_REGULATOR
	if (data->vana) {
		ret = regulator_set_voltage(data->vana,	VL53L1_VDD_MIN,
				VL53L1_VDD_MAX);
		if (ret < 0) {
			vl53l1_errmsg("set volt fail %d\n", ret);
			return ret;
		}
		ret = regulator_enable(data->vana);
		if (ret < 0) {

			vl53l1_errmsg("reg enable(%p) failed.rc=%d\n",
					data->vana, ret);
			return ret;
		}
		powered = 1;
	}
#endif
	pinctrl_select_state(g_pinctrl_info.pinctrl,
			g_pinctrl_info.gpio_state_active);

	/* Do power with gpio if not handle by regulator*/
	if (data->pwren_gpio != -1) {
		if (data->power_up == 0) {
			gpio_set_value(data->pwren_gpio, 1);
			powered = 1;
			vl53l1_info("slow power on");
			msleep(5); /* FIXME FPGA+panda slow power up */
		} else {
			vl53l1_wanrmsg("already on");
		}
	}
	goto power_done; /* avoid waring when cfg reg is not active */
power_done:
	if (powered) {
		vl53l1_dbgmsg("power on");
		data->power_up = 1;
		*preset_flag = 1;
		msleep(1); /* FIXME wait power stable long enough */
	}
	/* if we have reset i/o release reset if set*/
	if (data->xsdn_gpio != -1) {
		vl53l1_dbgmsg("un reset");
		gpio_set_value(data->xsdn_gpio, 1);
		msleep(50); /* FIXME  fpga is very slow too boot */
		*preset_flag = 1;
	}
	vl53l1_dbgmsg("End\n");
	return ret;
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
	int ret = 0;
	struct i2c_data *data = (struct i2c_data *)i2c_object;

	vl53l1_dbgmsg("Enter\n");
	if (data->xsdn_gpio != -1) {
		vl53l1_dbgmsg("xsdn low\n");
		gpio_set_value(data->xsdn_gpio, 0);
	}

#ifdef CFG_STMVL53L1_HAVE_REGULATOR
	ret = regulator_disable(data->vana);
	if (ret < 0)
		vl53l1_errmsg("reg disable(%p) failed.rc=%d\n",
			data->vana, ret);

	data->power_up = 0;
	goto done;
#endif
	if (data->pwren_gpio != -1) {
		if (data->power_up) {
			gpio_set_value(data->pwren_gpio, 0);
			data->power_up = 0;
			vl53l1_dbgmsg("is now off");
		} else
			vl53l1_wanrmsg("already off\n");
	}
	pinctrl_select_state(g_pinctrl_info.pinctrl,
			g_pinctrl_info.gpio_state_suspend);

	goto done; /* avoid warning unused w/o CFG_STMVL53L1_HAVE_REGULATOR*/
done:
	vl53l1_dbgmsg("End\n");
	return ret;
}

int stmvl53l1_init_i2c(void)
{
	int ret = 0;

#ifdef STM_TEST
	struct i2c_adapter *adapter;
	/** for test purpose we fake a tree i2c info with default i2c addr*/
	struct i2c_board_info info = {
		.type = "stmvl53l1",
		.addr = STMVL53L1_SLAVE_ADDR,
	};
#endif

	vl53l1_dbgmsg("Enter\n");

	/* register as a i2c client device */
	ret = i2c_add_driver(&stmvl53l1_driver);
	if (ret)
		vl53l1_errmsg("%d erro ret:%d\n", __LINE__, ret);

#ifdef STM_TEST
	/* FIXME tmp hack until we clean things ... */
	/* below 3.8.0 we say we got a panda else we got a pi3 .... */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0)
	/** for test purpose we fake add our i2c info to i2c bus 4
	 * where the 53l1 device is settling
	 */
	if (!ret) {
		adapter = i2c_get_adapter(4);
		if (!adapter)
			ret = -EINVAL;
		else
			stm_test_i2c_client = i2c_new_device(adapter, &info);
		if (!stm_test_i2c_client)
			ret = -EINVAL;
	}
#else
	/** for test purpose we fake add our i2c info to i2c bus 1
	 * where the 53l1 device is settling
	 */
	if (!ret) {
		adapter = i2c_get_adapter(1);
		if (!adapter)
			ret = -EINVAL;
		else
			stm_test_i2c_client = i2c_new_device(adapter, &info);
		if (!stm_test_i2c_client)
			ret = -EINVAL;
	}
#endif
#endif
	vl53l1_dbgmsg("End with rc:%d\n", ret);
	return ret;
}


void stmvl53l1_clean_up_i2c(void)
{
#ifdef STM_TEST
	if (stm_test_i2c_client) {
		vl53l1_dbgmsg("to unregister i2c client\n");
		i2c_unregister_device(stm_test_i2c_client);
	}
#endif
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

/** @} */ /* ingroup driver_porting */
#endif /* end of NOT CAMERA_CCI */
