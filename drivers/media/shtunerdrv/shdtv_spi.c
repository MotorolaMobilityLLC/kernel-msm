/***************************************************************************************/
/* 
	@file		shdtv_spi.c
	@brief		SPI I/F driver for FC8180
	@author		[SC] miya
	@version	1.0
	@note		2015/03/01

	Copyright (c) 2015 Sharp Corporation
*/
/***************************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/input/mt.h>

#include <linux/mfd/pm8xxx/pm8921.h>
//#include <mach/cpuidle.h>
#include <linux/pm_qos.h>

/* ------------------------------------------------------------------------- */
/* DEFINE                                                                    */
/* ------------------------------------------------------------------------- */
//#define __LOGOUT__

#define SHDTV_SPI_NAME				"shdtvspi"
#define SHDTV_SPI_BUFFER			(20)
//#define SHDTV_SPI_CLK_SPEED		(10000000)
#define SHDTV_SPI_CLK_SPEED			(4800000)
#define SHDTV_SPI_BIT_WORD			(8)
#define SHDTV_SPI_TSP_READ_NUM		(15)

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
//static int __devinit shdtv_dev_spi_probe(struct spi_device *spi);
//static int __devexit shdtv_dev_spi_remove(struct spi_device *spi);
static int shdtv_dev_spi_probe(struct spi_device *spi);
static int shdtv_dev_spi_remove(struct spi_device *spi);

static int shdtvspi_open(struct inode *inode, struct file *file);
static int shdtvspi_close(struct inode *inode, struct file *file);
static ssize_t shdtvspi_read(struct file* filp, char* buf, size_t count, loff_t* offset);

static int shdtv_spi_init(void);
static void shdtv_spi_exit(void);

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
/* --------------------------------------------------------- */
/* Serial Interface Parameter                                */
/* --------------------------------------------------------- */
static struct spi_device *spi_dev = NULL;

#ifdef CONFIG_OF						// Open firmware must be defined for dts usage
static struct of_device_id  shdtv_dev_spi_table [] = {
	{ . compatible = "sharp,shdtv_spi" ,},			// Compatible node must match dts
	{ },
};
#else
#define shdtv_dev_spi_table  NULL
#endif

static struct spi_driver  shdtv_dev_driver = {
	.probe = shdtv_dev_spi_probe,
//	.remove = __devexit_p(shdtv_dev_spi_remove),
	.remove = shdtv_dev_spi_remove,
	.driver = {
		.of_match_table = shdtv_dev_spi_table,
		.name = "shdtv_spi",
		.owner = THIS_MODULE,
	},
};


/* --------------------------------------------------------- */
/* file_operations                                           */
/* --------------------------------------------------------- */
static struct file_operations  shdtvspi_fops = {
	.owner          = THIS_MODULE,
	.open           = shdtvspi_open,
	.release        = shdtvspi_close,
	.read           = shdtvspi_read,
};


/* --------------------------------------------------------- */
/* Software Parameter                                        */
/* --------------------------------------------------------- */
struct shdtv_spi_drv {
	struct input_dev	*input;
	struct wake_lock	wake_lock;
};

static dev_t				shdtvspi_dev;
static struct cdev 			shdtvspi_cdev;
static struct class* 		shdtvspi_class;
static struct device*		shdtvspi_device;
static struct shdtv_spi_drv	dtv_ctrl;

static struct mutex 		shdtvspi_io_lock;
static unsigned char		*gRbuf;

int shdtv_err_log  = 1;
int shdtv_warn_log = 0;
int shdtv_info_log = 0;
int shdtv_dbg_log  = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
module_param(shdtv_err_log,  int, 0600);
module_param(shdtv_warn_log, int, 0600);
module_param(shdtv_info_log, int, 0600);
module_param(shdtv_dbg_log,  int, 0600);
#endif /* CONFIG_ANDROID_ENGINEERING */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDTV_ERR(fmt, args...) \
		if(shdtv_err_log == 1) { \
			printk("[SHDTV_ERROR][%s] " fmt, __func__, ## args); \
		}

#define SHDTV_WARN(fmt, args...) \
		if(shdtv_warn_log == 1) { \
			printk("[SHDTV_WARN][%s] " fmt, __func__, ## args); \
		}

#define SHDTV_INFO(fmt, args...) \
		if(shdtv_info_log == 1) { \
			printk("[SHDTV_INFO][%s] " fmt, __func__, ## args); \
		}

#define SHDTV_DBG(fmt, args...) \
		if(shdtv_dbg_log == 1) { \
			printk("[SHDTV_DBG][%s] " fmt, __func__, ## args); \
		}

static int shdtv_drv_init(struct shdtv_spi_drv *ctrl)
{
	int ret;

	ctrl->input = input_allocate_device();
	if (!(ctrl->input)) {
		printk("%s:%d input_allocate_device() failed \n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	ctrl->input->name         = SHDTV_SPI_NAME;
	ctrl->input->phys         = "shdtvspi/input0";
	ctrl->input->id.vendor    = 0x0001;
	ctrl->input->id.product   = 0x0001;
	ctrl->input->id.version   = 0x0001;
	ctrl->input->dev.parent   = &spi_dev->dev;

	ret = input_register_device(ctrl->input);
	if (ret) {
		printk("%s:%d input_register_device() failed \n", __FILE__, __LINE__);
		input_free_device(ctrl->input);
		return ret;
	}

	wake_lock_init(&(ctrl->wake_lock), WAKE_LOCK_SUSPEND, "shdtvspi_wake_lock");

	mutex_init(&shdtvspi_io_lock);
	return 0;
}


static void shdtv_drv_exit(struct shdtv_spi_drv *ctrl)
{
	mutex_destroy(&shdtvspi_io_lock);

	wake_lock_destroy(&(ctrl->wake_lock));

	input_unregister_device(ctrl->input);

	return;
}


//static int __devinit shdtv_dev_spi_probe(struct spi_device *spi)
static int shdtv_dev_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct shdtv_spi_drv *ctrl;

	ctrl = &dtv_ctrl;
	spi_set_drvdata(spi, ctrl);
	spi_dev = spi;

	ret = shdtv_spi_init();
	if (ret) {
		printk("%s:%d shdtv_spi_init() failed \n", __FILE__, __LINE__);
		return ret;
	}

	ret = spi_setup( spi_dev );
	if ( ret < 0 ) {
		printk("%s:%d spi_setup() failed \n", __FILE__, __LINE__);
		return ret;
	}

	ret = shdtv_drv_init(ctrl);
	if (ret != 0) {
		printk("%s:%d shdtv_drv_init() failed \n", __FILE__, __LINE__);
		return ret;
	}

//	printk("%s:%d shdtv_dev_spi_probe() OK \n", __FILE__, __LINE__);
	return ret;
}


//static int __devexit shdtv_dev_spi_remove(struct spi_device *spi)
static int shdtv_dev_spi_remove(struct spi_device *spi)
{
	struct shdtv_spi_drv *ctrl;

	ctrl = &dtv_ctrl;
	spi_set_drvdata(spi, ctrl);
	spi_dev = spi;

	shdtv_drv_exit(ctrl);

	shdtv_spi_exit();

	return 0;
}


static int __init shdtv_dev_init(void)
{
	int ret;

	/* spi_register_driver */
	ret = spi_register_driver(&shdtv_dev_driver);
	if (ret) {
		printk("%s:%d spi_register_driver() failed \n", __FILE__, __LINE__);
	}
	else {
//		printk("%s:%d spi_register_driver() OK \n", __FILE__, __LINE__);
	}

	return ret;
}
module_init(shdtv_dev_init);


static void __exit shdtv_dev_exit(void)
{
	spi_unregister_driver(&shdtv_dev_driver);
	return;
}
module_exit(shdtv_dev_exit);


//static int __init shdtv_spi_init(void)
static int shdtv_spi_init(void)
{
	int ret;
	struct shdtv_spi_drv *ctrl;

	ctrl = &dtv_ctrl;

	ret = alloc_chrdev_region(&shdtvspi_dev, 0, 1, SHDTV_SPI_NAME);
	if (ret < 0) {
		printk("%s:%d alloc_chrdev_region() failed \n", __FILE__, __LINE__);
		return ret;
	}

	shdtvspi_class = class_create(THIS_MODULE, SHDTV_SPI_NAME);
	if (IS_ERR(shdtvspi_class)) {
		ret = PTR_ERR(shdtvspi_class);
		printk("%s:%d class_create() failed \n", __FILE__, __LINE__);
		goto error_class_create;
	}

	shdtvspi_device = device_create(shdtvspi_class, NULL, 
									shdtvspi_dev, &shdtvspi_cdev, SHDTV_SPI_NAME);
	if (IS_ERR(shdtvspi_device)) {
		ret = PTR_ERR(shdtvspi_device);
		printk("%s:%d device_create() failed \n", __FILE__, __LINE__);
		goto error_device_create;
	}

	cdev_init(&shdtvspi_cdev, &shdtvspi_fops);
	shdtvspi_cdev.owner = THIS_MODULE;
	shdtvspi_cdev.ops   = &shdtvspi_fops;

	ret = cdev_add(&shdtvspi_cdev, shdtvspi_dev, 1);
	if (ret) {
		printk("%s:%d cdev_add() failed \n", __FILE__, __LINE__);
		goto error_cdev_add;
	}

//	printk("%s:%d shdtv_spi_init() OK \n", __FILE__, __LINE__);

	return 0;

error_cdev_add:
	cdev_del(&shdtvspi_cdev);
error_device_create:
	class_destroy(shdtvspi_class);
error_class_create:
	unregister_chrdev_region(shdtvspi_dev, 1);

	printk("%s:%d shdtv_spi_init() failed \n", __FILE__, __LINE__);
	return ret;
}
//module_init(shdtv_spi_init);


//static void __exit shdtv_spi_exit(void)
static void shdtv_spi_exit(void)
{
	cdev_del(&shdtvspi_cdev);
	class_destroy(shdtvspi_class);
	unregister_chrdev_region(shdtvspi_dev, 1);

	return;
}
//module_exit(shdtv_spi_exit);

#if 0
#define BIT_NUM (8)
static unsigned char bitflip8(unsigned char base)
{
	unsigned char temp;
	unsigned char val;
	int i;
	
	temp = 0xFF & base;
	val = 0;
	
	for (i = 0; i < BIT_NUM; i++) {
		val |= ((temp >> i) & 0x01) << (BIT_NUM - 1 - i);
	}
	
	return val;
}
#endif


static int shdtv_spi_read8(struct shdtv_spi_drv *ctrl, unsigned long adr, unsigned char *data, ssize_t *readsize)
{
	struct spi_message  msg;
	struct spi_transfer *x, xfer;
	int ret = 0;
	unsigned char senddata[SHDTV_SPI_BUFFER];


	if (!spi_dev) {
		printk("%s:%d spi_dev is NULL \n", __FILE__, __LINE__);
		return -EINVAL;
	}

	*readsize = 0;

	/************************/
	/* Write 5 config bytes */
	/************************/
	memset(&xfer, 0, sizeof(xfer));
	x = &xfer;
	x->bits_per_word    = SHDTV_SPI_BIT_WORD;
	x->len              = 5;
	x->speed_hz         = SHDTV_SPI_CLK_SPEED;
//	x->deassert_wait    = 0;

	senddata[0] = (unsigned char)(adr & 0x00FF);
	senddata[1] = (unsigned char)((adr & 0xFF00) >> 8);
	senddata[2] = 0x40;										/* LEN[18:16] = 0	*/
	senddata[3] = 0x0B;										/* LEN[15:8] = 0x0B	*/
	senddata[4] = 0x04;										/* LEN[7:0] = 0x04	*/	/* LEN[18:0] = 0x00B04 = 2820 = 188*15 */

	spi_message_init(&msg);
	x->tx_buf           = senddata;
	spi_message_add_tail(x, &msg);

	ret = spi_sync(spi_dev, &msg);
	if (ret) {
		printk("%s:%d spi_sync err. [ret=%d] \n", __FILE__, __LINE__, ret);
		return ret;
	}

	/* M receives data from S */
	memset(&xfer, 0, sizeof(xfer));
	x = &xfer;
	x->bits_per_word    = SHDTV_SPI_BIT_WORD;
	x->len              = 188*SHDTV_SPI_TSP_READ_NUM;
	x->speed_hz         = SHDTV_SPI_CLK_SPEED;
//	x->deassert_wait    = 0;

	spi_message_init(&msg);
	x->tx_buf           = NULL;
	x->rx_buf           = data;
	spi_message_add_tail(x, &msg);
	
	ret = spi_sync(spi_dev, &msg);
	if (ret) {
		printk("%s:%d spi_sync err. [ret=%d] \n", __FILE__, __LINE__, ret);
		return ret;
	}

	*readsize = 188*SHDTV_SPI_TSP_READ_NUM;

	return ret;
}


static int shdtvspi_open(struct inode *inode, struct file *file)
{
	struct shdtv_spi_drv *ctrl;

	ctrl = &dtv_ctrl;
	wake_lock(&(ctrl->wake_lock));

	gRbuf = kmalloc( 188*SHDTV_SPI_TSP_READ_NUM, GFP_KERNEL );
	if ( gRbuf == NULL ) {
		wake_unlock(&(ctrl->wake_lock));
		printk("%s:%d kmalloc() failed \n", __FILE__, __LINE__);
		return -EFAULT;
	}

//	printk("%s:%d shdtvspi_open() called \n", __FILE__, __LINE__);
	return 0;
}


static int shdtvspi_close(struct inode *inode, struct file *file)
{
	struct shdtv_spi_drv *ctrl;

	ctrl = &dtv_ctrl;
	wake_unlock(&(ctrl->wake_lock));

	kfree( gRbuf );

//	printk("%s:%d shdtvspi_close() called \n", __FILE__, __LINE__);
	return 0;
}


static ssize_t shdtvspi_read(struct file* filp, char* buf, size_t count, loff_t* offset)
{
	int ret = 0;
	struct shdtv_spi_drv *ctrl;
	ssize_t readsize;

//	printk("shdtvspi_read() start \n");

	if ( count < 188*SHDTV_SPI_TSP_READ_NUM ) {
		readsize = 0;
		printk("shdtvspi_read()  invalid size to read \n");
	}
	else {
		ctrl = &dtv_ctrl;
		memset(gRbuf, 0, 188*SHDTV_SPI_TSP_READ_NUM);
		mutex_lock(&shdtvspi_io_lock);

		ret = shdtv_spi_read8(ctrl, 0x0008, gRbuf, &readsize);

		if ( ret == 0 ) {
			if ( copy_to_user( (unsigned char*)buf, gRbuf, 188*SHDTV_SPI_TSP_READ_NUM ) ) {
				printk("%s:%d copy_to_user failed \n", __FILE__, __LINE__);
				mutex_unlock(&shdtvspi_io_lock);

				return -EFAULT;
			}
		}

		mutex_unlock(&shdtvspi_io_lock);
	}

//	printk("shdtvspi_read() end  ret = %d \n", ret);
	return readsize;
}

