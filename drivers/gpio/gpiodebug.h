#ifndef __GPIO_DEBUG_H_
#define __GPIO_DEBUG_H_

#include <linux/gpio.h>

struct gpio_debug;

#define TYPE_CONF_REG			0x00
#define TYPE_PIN_VALUE			0x01
#define TYPE_DIRECTION			0x02
#define TYPE_IRQ_TYPE			0x03
#define TYPE_PINMUX			0x04
#define TYPE_PULLMODE			0x05
#define TYPE_PULLSTRENGTH		0x06
#define TYPE_OPEN_DRAIN			0x07

#define TYPE_IRQ_COUNT			0x08
#define TYPE_WAKEUP			0x09
#define TYPE_WAKEUP_COUNT		0x0A
#define TYPE_OVERRIDE_OUTDIR		0x0B
#define TYPE_OVERRIDE_OUTVAL		0x0C
#define TYPE_OVERRIDE_INDIR		0x0D
#define TYPE_OVERRIDE_INVAL		0x0E
#define TYPE_DEBOUNCE			0x0F

#define TYPE_SBY_OVR_IO			0x10
#define TYPE_SBY_OVR_OUTVAL		0x11
#define TYPE_SBY_OVR_INVAL		0x12
#define TYPE_SBY_OVR_OUTDIR		0x13
#define TYPE_SBY_OVR_INDIR		0x14
#define TYPE_SBY_PUPD_STATE		0x15
#define TYPE_SBY_OD_DIS			0x16
#define TYPE_IRQ_LINE			0x17
#define TYPE_MAX			0x18

struct gpio_control {
	unsigned type, num;
	char	 **pininfo;
	u32	reg, invert;
	u32 shift, rshift;
	u32	mask;
	int (*get)(struct gpio_control *control, void *private_data,
		unsigned gpio);
	int (*set)(struct gpio_control *control, void *private_data,
		unsigned gpio, unsigned int num);
	int (*get_handle)(int val);
	void (*set_handle)(unsigned int num, int *val);
};

struct gpio_debug_ops {
	unsigned int (*get_conf_reg)(struct gpio_debug *debug, unsigned gpio);
	void	(*set_conf_reg)(struct gpio_debug *debug, unsigned gpio,
			unsigned int value);
	char	**(*get_avl_pininfo)(struct gpio_debug *debug, unsigned gpio,
			unsigned int type, unsigned *num);
	char	*(*get_cul_pininfo)(struct gpio_debug *debug, unsigned gpio,
			unsigned int type);
	void	(*set_pininfo)(struct gpio_debug *debug, unsigned gpio,
			unsigned int type, const char *info);
	int	(*get_register_msg)(char **buf, unsigned long *size);
};

struct gpio_debug {
	unsigned long		typebit[BITS_TO_LONGS(TYPE_MAX)];
	struct gpio_chip	*chip;
	struct gpio_debug_ops	*ops;
	unsigned long		irq_count[ARCH_NR_GPIOS];
	unsigned long		wakeup_count[ARCH_NR_GPIOS];
	void			*private_data;
};

#ifdef CONFIG_GPIODEBUG

#define DEFINE_DEBUG_IRQ_CONUNT_INCREASE(gpio) (debug->irq_count[gpio]++)

struct gpio_control *find_gpio_control(struct gpio_control *control, int num,
			unsigned type);
int find_pininfo_num(struct gpio_control *control, const char *info);

struct gpio_debug *gpio_debug_alloc(void);
void gpio_debug_remove(struct gpio_debug *debug);
int gpio_debug_register(struct gpio_debug *debug);
#else

#define DEFINE_DEBUG_IRQ_CONUNT_INCREASE(gpio)

static inline struct gpio_control *find_gpio_control(
			struct gpio_control *control, int num, unsigned type)
{
	return NULL;
}
static inline int find_pininfo_num(struct gpio_control *control,
			const char *info)
{
	return 0;
}
static inline struct gpio_debug *gpio_debug_alloc(void)
{
	return NULL;
}

static inline void gpio_debug_remove(struct gpio_debug *debug)
{
	return NULL;
}
static inline int gpio_debug_register(struct gpio_debug *debug)
{
	return 0;
}
#endif
#endif
