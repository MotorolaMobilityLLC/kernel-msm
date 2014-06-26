#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include<linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include "gpiodebug.h"

struct gpiodebug_data {
	struct gpio_debug *debug;
	int gpio;
	unsigned int type;
};

enum {
	REGISTER_FOPS = 0,
	NORMAL_FOPS,
	COUNT_FOPS,
};

static struct {
	unsigned	fops_type;
	unsigned	type;
	char		*available_name;
	char		*current_name;
} global_array[] = {
	{REGISTER_FOPS, TYPE_CONF_REG, "conf_reg", "conf_reg"},
	{NORMAL_FOPS, TYPE_PIN_VALUE, "available_value",
		"current_value"},
	{NORMAL_FOPS, TYPE_DIRECTION, "available_direction",
		"current_direction"},
	{NORMAL_FOPS, TYPE_IRQ_TYPE, "available_irqtype",
		"current_irqtype"},
	{NORMAL_FOPS, TYPE_PINMUX, "available_pinmux",
		"current_pinmux"},
	{NORMAL_FOPS, TYPE_PULLMODE, "available_pullmode",
		"current_pullmode"},
	{NORMAL_FOPS, TYPE_PULLSTRENGTH, "available_pullstrength",
		"current_pullstrength"},
	{NORMAL_FOPS, TYPE_OPEN_DRAIN, "available_opendrain",
		"current_opendrain"},
	{COUNT_FOPS, TYPE_IRQ_COUNT, "irq_count", "irq_count"},
	{NORMAL_FOPS, TYPE_WAKEUP, "available_wakeup", "current_wakeup"},
	{COUNT_FOPS, TYPE_WAKEUP_COUNT, "wakeup_count", "wakeup_count"},
	{NORMAL_FOPS, TYPE_DEBOUNCE, "available_debounce",
		"current_debounce"},
	{NORMAL_FOPS, TYPE_OVERRIDE_OUTDIR, "available_override_outdir",
		"current_override_outdir"},
	{NORMAL_FOPS, TYPE_OVERRIDE_OUTVAL, "available_override_outval",
		"current_override_outval"},
	{NORMAL_FOPS, TYPE_OVERRIDE_INDIR, "available_override_indir",
		"current_override_indir"},
	{NORMAL_FOPS, TYPE_OVERRIDE_INVAL, "available_override_inval",
		"current_override_inval"},
	{NORMAL_FOPS, TYPE_SBY_OVR_IO, "available_standby_trigger",
		"current_standby_trigger"},
	{NORMAL_FOPS, TYPE_SBY_OVR_OUTVAL, "available_standby_outval",
		"current_standby_outval"},
	{NORMAL_FOPS, TYPE_SBY_OVR_INVAL, "available_standby_inval",
		"current_standby_inval"},
	{NORMAL_FOPS, TYPE_SBY_OVR_OUTDIR, "available_standby_outdir",
		"current_standby_outdir"},
	{NORMAL_FOPS, TYPE_SBY_OVR_INDIR, "available_standby_indir",
		"current_standby_indir"},
	{NORMAL_FOPS, TYPE_SBY_PUPD_STATE, "available_standby_pullmode",
		"current_standby_pullmode"},
	{NORMAL_FOPS, TYPE_SBY_OD_DIS, "available_standby_opendrain",
		"current_standby_opendrain"},
	{NORMAL_FOPS, TYPE_IRQ_LINE, "available_irq_line",
		"current_irq_line"},

};

static struct dentry *gpio_root[ARCH_NR_GPIOS];
static struct gpiodebug_data global_data[ARCH_NR_GPIOS][TYPE_MAX];

static struct dentry *gpiodebug_debugfs_root;

struct gpio_control *find_gpio_control(struct gpio_control *control, int num,
			unsigned type)
{
	int i;

	for (i = 0; i < num; i++) {
		if ((control+i)->type == type)
			break;
	}

	if (i < num)
		return control+i;

	return NULL;
}

int find_pininfo_num(struct gpio_control *control, const char *info)
{
	int num = 0;

	while (num < control->num) {
		if (!strcmp(*(control->pininfo+num), info))
			break;
		num++;
	}

	if (num < control->num)
		return num;

	return -1;
}

static struct dentry *gpiodebug_create_file(const char *name,
			umode_t mode, struct dentry *parent,
			void *data, const struct file_operations *fops)
{
	struct dentry *ret;

	ret = debugfs_create_file(name, mode, parent, data, fops);
	if (!ret)
		pr_warn("Could not create debugfs '%s' entry\n", name);

	return ret;
}

static int gpiodebug_open_file(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static const char readme_msg[] =
	"\n GPIO Debug Tool-HOWTO (Example):\n\n"
	"# mount -t debugfs nodev /sys/kernel/debug\n\n"
	"# cat /sys/kernel/debug/gpio_debug/gpio0/available_pullmode\n"
	"nopull	pullup	pulldown\n\n"
	"# cat /sys/kernel/debug/gpio_debug/gpio0/current_pullmode\n"
	"nopull\n"
	"# echo pullup > /sys/kernel/debug/gpio_debug/gpio0/current_pullmode\n"
	"# cat /sys/kernel/debug/gpio_debug/gpio0/current_pullmode\n"
	"pullup\n\n"
	"# cat conf_reg\n"
	"0x00003120\n"
	"# echo 0x00003121 > conf_reg\n"
	"0x00003121\n\n"
	"# cat irq_count\n"
	"1\n";

/* gpio_readme_fops */
static ssize_t show_gpio_readme(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	ret = simple_read_from_buffer(ubuf, cnt, ppos, readme_msg,
		strlen(readme_msg));

	return ret;
}

static const struct file_operations gpio_readme_fops = {
	.open		= gpiodebug_open_file,
	.read		= show_gpio_readme,
	.llseek		= generic_file_llseek,
};

/* gpio_reginfo_fops */
static ssize_t show_gpio_reginfo(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpio_debug *debug = filp->private_data;
	unsigned long size;
	char *buf;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	if (debug->ops->get_register_msg) {
		debug->ops->get_register_msg(&buf, &size);
		ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, size);
	}

	return ret;
}

static const struct file_operations gpio_reginfo_fops = {
	.open		= gpiodebug_open_file,
	.read		= show_gpio_reginfo,
	.llseek		= generic_file_llseek,
};


/* gpio_conf_fops */
static ssize_t gpio_conf_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	int gpio = data->gpio;
	char *buf;
	unsigned int value = 0;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (debug->ops->get_conf_reg)
		value = debug->ops->get_conf_reg(debug, gpio);

	if (value == -EINVAL)
		ret = sprintf(buf, "Invalid pin\n");
	else
		ret = sprintf(buf, "0x%08x\n", value);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t gpio_conf_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	int i, gpio = data->gpio;
	char *buf, *start;
	unsigned int value;

	ret = cnt;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	start = buf;

	while (*start == ' ')
		start++;

	/* strip ending whitespace. */
	for (i = cnt - 1; i > 0 && isspace(buf[i]); i--)
		buf[i] = 0;

	if (kstrtouint(start, 16, &value))
		return -EINVAL;

	if (debug->ops->set_conf_reg)
		debug->ops->set_conf_reg(debug, gpio, value);

	*ppos += ret;

	return ret;
}

static const struct file_operations gpio_conf_fops = {
	.open		= gpiodebug_open_file,
	.read		= gpio_conf_read,
	.write		= gpio_conf_write,
	.llseek		= generic_file_llseek,
};

/* show_gpiodebug_fops */
static ssize_t gpiodebug_show_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	unsigned int type = data->type;
	int i, num = 0;
	int gpio = data->gpio;
	char *buf, **avl_buf = NULL;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* debug->ops->get_avl_info */
	if (debug->ops->get_avl_pininfo) {
		avl_buf = debug->ops->get_avl_pininfo(debug, gpio, type, &num);

		for (i = 0; i < num; i++)
			sprintf(buf, "%s%s\t", buf, *(avl_buf+i));
	}

	ret = sprintf(buf, "%s\n", buf);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static const struct file_operations show_gpiodebug_fops = {
	.open		= gpiodebug_open_file,
	.read		= gpiodebug_show_read,
	.llseek		= generic_file_llseek,
};

/* set_gpiodebug_fops */
static ssize_t gpiodebug_set_gpio_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	unsigned int type = data->type;
	int gpio = data->gpio;
	char *buf, *cur_info = NULL;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (debug->ops->get_cul_pininfo)
		cur_info = debug->ops->get_cul_pininfo(debug, gpio, type);

	if (cur_info)
		ret = sprintf(buf, "%s\n", cur_info);
	else
		ret = sprintf(buf, "\n");

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static ssize_t gpiodebug_set_gpio_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	unsigned int type = data->type;
	int i, gpio = data->gpio;
	char *buf;

	ret = cnt;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	/* strip ending whitespace. */
	for (i = cnt - 1; i > 0 && isspace(buf[i]); i--)
		buf[i] = 0;

	if (debug->ops->set_pininfo)
		debug->ops->set_pininfo(debug, gpio, type, buf);

	*ppos += ret;

	return ret;
}

static const struct file_operations set_gpiodebug_fops = {
	.open		= gpiodebug_open_file,
	.read		= gpiodebug_set_gpio_read,
	.write		= gpiodebug_set_gpio_write,
	.llseek		= generic_file_llseek,
};

/* show_count_fops */
static ssize_t show_count_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	ssize_t ret = 0;
	struct gpiodebug_data *data = filp->private_data;
	struct gpio_debug *debug = data->debug;
	unsigned int type = data->type;
	unsigned long count = 0;
	int gpio = data->gpio;
	char *buf;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf = kzalloc(cnt, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (type == TYPE_IRQ_COUNT)
		count = debug->irq_count[gpio];
	else if (type == TYPE_WAKEUP_COUNT)
		count = debug->wakeup_count[gpio];

	ret = sprintf(buf, "%ld\n", count);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static const struct file_operations show_count_fops = {
	.open		= gpiodebug_open_file,
	.read		= show_count_read,
	.llseek		= generic_file_llseek,
};

/******************************************************************************/
struct gpio_debug *gpio_debug_alloc(void)
{
	struct gpio_debug *debug;

	debug = kzalloc(sizeof(struct gpio_debug), GFP_KERNEL);
	if (debug) {
		__set_bit(TYPE_CONF_REG, debug->typebit);
		__set_bit(TYPE_PIN_VALUE, debug->typebit);
		__set_bit(TYPE_DIRECTION, debug->typebit);
		__set_bit(TYPE_IRQ_TYPE, debug->typebit);
		__set_bit(TYPE_PINMUX, debug->typebit);
		__set_bit(TYPE_PULLMODE, debug->typebit);
		__set_bit(TYPE_PULLSTRENGTH, debug->typebit);
		__set_bit(TYPE_OPEN_DRAIN, debug->typebit);
		__set_bit(TYPE_IRQ_COUNT, debug->typebit);
		__set_bit(TYPE_DEBOUNCE, debug->typebit);
	}

	return debug;
}

void gpio_debug_remove(struct gpio_debug *debug)
{
	struct gpio_chip *chip = debug->chip;
	int base = chip->base;
	unsigned ngpio = chip->ngpio;
	int i;

	for (i = base; i < base+ngpio; i++)
		debugfs_remove_recursive(gpio_root[i]);

	kfree(debug);
}

int gpio_debug_register(struct gpio_debug *debug)
{
	struct gpio_chip *chip = debug->chip;
	int base = chip->base;
	unsigned ngpio = chip->ngpio;
	int i, j;
	char gpioname[32];

	for (i = base; i < base+ngpio; i++) {
		sprintf(gpioname, "gpio%d", i);
		gpio_root[i] = debugfs_create_dir(gpioname,
				gpiodebug_debugfs_root);
		if (!gpio_root[i]) {
			pr_warn("gpiodebug: Failed to create debugfs directory\n");
			return -ENOMEM;
		}

		/* register info */
		gpiodebug_create_file("register_info", 0400, gpio_root[i],
			debug, &gpio_reginfo_fops);

		for (j = 0; j < ARRAY_SIZE(global_array); j++) {
			if (test_bit(global_array[j].type, debug->typebit)) {
				global_data[i][j].gpio = i;
				global_data[i][j].debug = debug;
				global_data[i][j].type = global_array[j].type;

				switch (global_array[j].fops_type) {
				case REGISTER_FOPS:
					gpiodebug_create_file(
					  global_array[j].current_name, 0600,
					  gpio_root[i], &global_data[i][j],
					  &gpio_conf_fops);
					break;
				case NORMAL_FOPS:
					gpiodebug_create_file(
					  global_array[j].available_name, 0400,
					  gpio_root[i], &global_data[i][j],
					  &show_gpiodebug_fops);

					gpiodebug_create_file(
					  global_array[j].current_name, 0600,
					  gpio_root[i], &global_data[i][j],
					  &set_gpiodebug_fops);
					break;
				case COUNT_FOPS:
					gpiodebug_create_file(
					  global_array[j].current_name, 0400,
					  gpio_root[i], &global_data[i][j],
					  &show_count_fops);
					break;
				default:
					break;
				}
			}
		}
	}

	return 0;
}

static int __init gpio_debug_init(void)
{
	gpiodebug_debugfs_root = debugfs_create_dir("gpio_debug", NULL);
	if (IS_ERR(gpiodebug_debugfs_root) || !gpiodebug_debugfs_root) {
		pr_warn("gpiodebug: Failed to create debugfs directory\n");
		gpiodebug_debugfs_root = NULL;
	}

	/* readme */
	gpiodebug_create_file("readme", 0400, gpiodebug_debugfs_root,
		NULL, &gpio_readme_fops);

	return 0;
}

subsys_initcall(gpio_debug_init);
