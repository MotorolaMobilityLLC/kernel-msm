/**************************************************************************************************/
/** 
	@file		gpio_def_ext.h
	@brief		GPIO Definition Header
*/
/**************************************************************************************************/

#ifndef GPIO_DEF_EXT
	#define GPIO_DEF_EXT

/* PORT ID */
#define GPIO_DTVEN_PORTID		1
#define GPIO_DTVRST_PORTID		2
#define GPIO_DTVLNAEN_PORTID	3
#define GPIO_DTVANTSW_PORTID	4
#define GPIO_DTVMANTSL_PORTID	5
#define GPIO_DTVUANTSL_PORTID	6
#define GPIO_DTVCANTSL_PORTID	7
#define GPIO_DTVHWREV_PORTID	8


/* IS VALID ? */
#define GPIO_INVALID	0
#define GPIO_VALID		1

#define VALID_GPIO_DTVEN		GPIO_VALID
#define VALID_GPIO_DTVRST		GPIO_INVALID
#define VALID_GPIO_DTVLNAEN		GPIO_INVALID
#define VALID_GPIO_DTVANTSW		GPIO_INVALID
#define VALID_GPIO_DTVMANTSL	GPIO_INVALID
#define VALID_GPIO_DTVUANTSL	GPIO_INVALID
#define VALID_GPIO_DTVCANTSL	GPIO_INVALID
#define VALID_GPIO_DTVHWREV		GPIO_INVALID


/* IO CONTROL  */
typedef struct __ioctl_cmd{
	unsigned int id;
	unsigned int val;
} ioctl_cmd;

#define TUNERDRV_IOCTL_MAGIC 't'
#define IOC_GPIO_VAL_SET	_IOW(TUNERDRV_IOCTL_MAGIC, 0x01, int)
#define IOC_GPIO_VAL_GET	_IOR(TUNERDRV_IOCTL_MAGIC, 0x02, int)
#define IOC_VREG_ENABLE		_IOW(TUNERDRV_IOCTL_MAGIC, 0x03, int)
#define IOC_VREG_DISABLE	_IOW(TUNERDRV_IOCTL_MAGIC, 0x04, int)
#define IOC_CLK_ENABLE		_IOW(TUNERDRV_IOCTL_MAGIC, 0x05, int)
#define IOC_CLK_DISABLE		_IOW(TUNERDRV_IOCTL_MAGIC, 0x06, int)
#define IOC_HW_REV_GET		_IOW(TUNERDRV_IOCTL_MAGIC, 0x07, int)
#define IOC_PERF_LOCK		_IOW(TUNERDRV_IOCTL_MAGIC, 0x08, int)
#define IOC_PERF_UNLOCK		_IOW(TUNERDRV_IOCTL_MAGIC, 0x09, int)
#define IOC_VREG12V_ENABLE	_IOW(TUNERDRV_IOCTL_MAGIC, 0x0A, int)
#define IOC_VREG12V_DISABLE	_IOW(TUNERDRV_IOCTL_MAGIC, 0x0B, int)
#endif	//GPIO_DEF_EXT
