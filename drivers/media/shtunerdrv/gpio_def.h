/**************************************************************************************************/
/** 
	@file		gpio_def.h
	@brief		GPIO Definition Header
*/
/**************************************************************************************************/

#ifndef GPIO_DEF
	#define GPIO_DEF

#define PM8952_GPIO_BASE		1000
#define PM8952_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio + PM8952_GPIO_BASE)

#include "gpio_def_ext.h"

typedef struct GPIO_DEF {
	unsigned int id;		/* GPIO Number (ID) */
	unsigned int no;		/* GPIO PORT Number */
	int direction;			/* I/O Direction */
	int out_val;			/* Initialized Value */
	int init_done;			/* GPIO Initialized ? 1:Complete (Don't Care) */
	char *label;			/* labels may be useful for diagnostics */
} stGPIO_DEF;

typedef struct GPIO_DEF_spi {
	unsigned int no;		/* GPIO PORT Number */
	int direction;			/* I/O Direction */
	int out_val;			/* Initialized Value */
	int init_done;			/* GPIO Initialized ? 1:Complete (Don't Care) */
	char *label;			/* labels may be useful for diagnostics */
} stGPIO_DEF_spi;

#define DirctionIn (0)
#define DirctionOut (1)

#define GPIO_DTVEN_PORTNO			(3)

//#define GPIO_DTVEN_PORTNO			(PM8841_GPIO_PM_TO_SYS(4))
//#define GPIO_DTVRST_PORTNO		(3)
//#define GPIO_DTVLNAEN_PORTNO		(xx)
//#define GPIO_DTVANTSW_PORTNO		(PM8841_GPIO_PM_TO_SYS(15))
//#define GPIO_DTVMANTSL_PORTNO		(PM8841_GPIO_PM_TO_SYS(1))
//#define GPIO_DTVUANTSL_PORTNO		(PM8841_GPIO_PM_TO_SYS(2))
//#define GPIO_DTVCANTSL_PORTNO		(PM8841_GPIO_PM_TO_SYS(18)
//#define GPIO_DTVHWREV_PORTNO		(31)

//#define CLOCK_CONTROL_PORTNO	(32)

#define GPIO_DTVSPIMOSI_PORTNO		(20)
#define GPIO_DTVSPIMISO_PORTNO		(21)
#define GPIO_DTVSPICSN_PORTNO		(22)
#define GPIO_DTVSPICLK_PORTNO		(23)

#endif	//GPIO_DEF
