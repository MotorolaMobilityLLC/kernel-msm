#include <linux/module.h>
#include <linux/debugfs.h>
#include "tp2e.h"

/* Create generic TP2E tracepoint event */
#define CREATE_TRACE_POINTS
#include <trace/events/tp2e.h>

EXPORT_TRACEPOINT_SYMBOL(tp2e_generic_event);

/* uncomment to compile test */
/* #define TP2E_TEST */
#ifdef TP2E_TEST
#include "tp2e_test.c"
#endif


struct tp2e_element {
	char *system;
	char *name;
	void *probe_fn;
	struct list_head list;
};

/* List of tp2e_element objects */
static struct list_head tp2e_list;

/* A tp2e_system_dir element is associated to each system directory
 * in the tp2e debugfs, e.g. /sys/kernel/debug/tp2e/events/my_system/
 * Each system includes one or several tracepoint events (list of
 * tp2e_event_file elements).
 */
struct tp2e_system_dir {
	struct list_head list;
	const char *name;
	struct dentry *dir;
	struct list_head event_files;
};

/* A tp2e_event_file element is associated to each tp2e element
 * (i.e. tracepoint event supported by tp2e) in the tp2e debugfs,
 * e.g. /sys/kernel/debug/tp2e/events/my_system/my_event/
 */
struct tp2e_event_file {
	struct list_head list;
	struct tp2e_element *elt;
	bool enabled;
};

/* This is the root dir in the tp2e debugfs, i.e. /sys/kernel/debug/tp2e */
static struct dentry *d_tp2e;

/* List of tp2e_system_dir elements (lists all the systems supported by tp2e) */
static struct list_head tp2e_systems_list;



#define DECLARE_TP2E_ELT
# include "tp2e_probes.h"
#undef DECLARE_TP2E_ELT



/* This method is used to enable/disable the tp2e probe for a given
 * tracepoint event.
 */
static int tp2e_event_enable_disable(struct tp2e_event_file *file,
				     int enable)
{
	if (!file->elt)
		return -EINVAL;

	if (enable && !file->enabled) {
		tracepoint_probe_register(file->elt->name,
					  file->elt->probe_fn,
					  NULL);
		file->enabled = true;
	} else if (!enable && file->enabled) {
		tracepoint_probe_unregister(file->elt->name,
					    file->elt->probe_fn,
					    NULL);
		file->enabled = false;
	}

	return 0;
}

static int tp2e_generic_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

/* Write method that is associated to the 'enable' debugfs file
 * associated to each tp2e element in the tp2e debugfs, e.g.
 * /sys/kernel/debug/tp2e/events/my_system/my_event/enable
 *
 * Writing '0' (resp. '1') in this file disables (resp. enables) the
 * tp2e probe for the tracepoint event.
 */
static ssize_t event_enable_write(struct file *filp, const char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	struct tp2e_event_file *event_file = filp->private_data;
	unsigned long val;
	int ret;

	if (!event_file)
		return -EINVAL;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	switch (val) {
	case 0:
	case 1:
		ret = tp2e_event_enable_disable(event_file, val);
		break;
	default:
		return -EINVAL;
	}

	*ppos += cnt;

	return ret ? ret : cnt;
}

/* read method that is associated to the 'enable' file associated
 * to each tp2e element in the tp2e debugfs, e.g.
 * /sys/kernel/debug/tp2e/events/my_system/my_event/enable
 *
 * Reading '0' (resp. '1') means that the tp2e probe for the tracepoint
 * event is disabled (resp. enabled).
 */
static ssize_t event_enable_read(struct file *filp, char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	struct tp2e_event_file *event_file = filp->private_data;
	char *buf;

	if (!event_file)
		return -EINVAL;

	if (event_file->enabled)
		buf = "1\n";
	else
		buf = "0\n";

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, strlen(buf));
}

/* Write method that is associated to the 'enable' debugfs file
 * located in each system directory in the tp2e debugfs, e.g.
 * /sys/kernel/debug/tp2e/events/my_system/enable
 *
 * Writing '0' (resp. '1') in this file disables (resp. enables) the
 * tp2e probe for the all the tracepoint events of the system.
 */
static ssize_t system_enable_write(struct file *filp, const char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	struct tp2e_system_dir *system_dir = filp->private_data;
	struct tp2e_event_file *event_file;
	unsigned long val;
	int ret;

	if (!system_dir)
		return -EINVAL;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	if ((val != 0) && (val != 1))
		return -EINVAL;

	list_for_each_entry(event_file, &system_dir->event_files, list) {
		ret = tp2e_event_enable_disable(event_file, val);
		if (ret)
			return ret;
	}

	*ppos += cnt;

	return cnt;
}

/* read method that is associated to the 'enable' debugfs file
 * located in each system directory in the tp2e debugfs, e.g.
 * /sys/kernel/debug/tp2e/events/my_system/enable
 *
 * Reading '0' (resp. '1') means that the tp2e probe of all the
 * tracepoint events of the system are disabled (resp. enabled).
 */
static ssize_t system_enable_read(struct file *filp, char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	const char set_to_char[4] = { '?', '0', '1', 'X' };
	struct tp2e_system_dir *system_dir = filp->private_data;
	struct tp2e_event_file *event_file;
	char buf[2];
	int set = 0;

	if (!system_dir)
		return -EINVAL;

	list_for_each_entry(event_file, &system_dir->event_files, list) {
		set |= (1 << !!(event_file->enabled));
		if (set == 3)
			break;
	}

	buf[0] = set_to_char[set];
	buf[1] = '\n';

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, 2);
}

/* Write method that is associated to the 'enable' debugfs file
 * located in the 'events'directory, i.e.
 * /sys/kernel/debug/tp2e/events/enable
 *
 * Writing '0' (resp. '1') in this file disables (resp. enables) the
 * tp2e probe of all the tracepoint events that are supported by tp2e
 */
static ssize_t tp2e_enable_write(struct file *filp, const char __user *ubuf,
				 size_t cnt, loff_t *ppos)
{
	struct tp2e_system_dir *system_dir;
	struct tp2e_event_file *event_file;
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	if ((val != 0) && (val != 1))
		return -EINVAL;

	list_for_each_entry(system_dir, &tp2e_systems_list, list) {
		list_for_each_entry(event_file, &system_dir->event_files,
				    list) {
			ret = tp2e_event_enable_disable(event_file, val);
			if (ret)
				return ret;
		}
	}

	*ppos += cnt;

	return cnt;
}

/* read method that is associated to the 'enable' debugfs file
 * located in the 'events'directory, i.e.
 * /sys/kernel/debug/tp2e/events/enable
 *
 * Reading '0' (resp. '1') means that the tp2e probe of all the
 * tracepoint events supported by tp2e are disabled (resp. enabled).
 */
static ssize_t tp2e_enable_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	const char set_to_char[4] = { '?', '0', '1', 'X' };
	struct tp2e_system_dir *system_dir;
	struct tp2e_event_file *event_file;
	char buf[2];
	int set = 0;

	list_for_each_entry(system_dir, &tp2e_systems_list, list) {
		list_for_each_entry(event_file, &system_dir->event_files,
				    list) {
			set |= (1 << !!(event_file->enabled));
			if (set == 3)
				break;
		}
	}

	buf[0] = set_to_char[set];
	buf[1] = '\n';

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, 2);
}

static const struct file_operations tp2e_enable_fops = {
	.open = tp2e_generic_open,
	.read = tp2e_enable_read,
	.write = tp2e_enable_write,
};

static const struct file_operations system_enable_fops = {
	.open = tp2e_generic_open,
	.read = system_enable_read,
	.write = system_enable_write,
};

static const struct file_operations event_enable_fops = {
	.open = tp2e_generic_open,
	.read = event_enable_read,
	.write = event_enable_write,
};


/* Get the system directory in debugfs for a given tp2e element,
 * or create a system directory if not already existing.
 */
static struct tp2e_system_dir *tp2e_system_dir(struct dentry *parent,
					       struct tp2e_element *elt)
{
	struct dentry *dir;
	struct tp2e_system_dir *system;

	/* Check if this dir has already been created */
	list_for_each_entry(system, &tp2e_systems_list, list)
		if (strcmp(system->name, elt->system) == 0)
			return system;

	/* If not, create a new system dir */
	system = kmalloc(sizeof(*system), GFP_KERNEL);
	if (!system)
		goto out_fail;

	dir = debugfs_create_dir(elt->system, parent);
	if (!dir)
		goto out_free;

	system->name = elt->system;
	system->dir = dir;
	INIT_LIST_HEAD(&system->event_files);
	list_add_tail(&system->list, &tp2e_systems_list);

	if (!debugfs_create_file("enable", 0644, dir, system,
				 &system_enable_fops))
		pr_warn("Could not create debugfs '%s/enable' entry\n",
			   elt->system);

	return system;

out_free:
	kfree(dir);
out_fail:
	pr_warn("No memory to create event system '%s'",
		elt->system);
	return NULL;
}

/* Create the tp2e debugfs */
static int tp2e_create_dirs(void)
{
	struct dentry *d_events, *d_system, *d_event;
	struct tp2e_system_dir *system_dir;
	struct tp2e_event_file *event_file;
	struct tp2e_element *elt;

	d_tp2e = debugfs_create_dir("tp2e", NULL);

	d_events = debugfs_create_dir("events", d_tp2e);

	/* Create the debugfs directory(ies) required for
	 * each tp2e element.
	 */
	list_for_each_entry(elt, &tp2e_list, list) {
		system_dir = tp2e_system_dir(d_events, elt);
		if ((!system_dir) || (!system_dir->dir))
			continue;

		d_system = system_dir->dir;
		d_event = debugfs_create_dir(elt->name, d_system);
		if (!d_event) {
			pr_warn("Could not create debugfs '%s' directory\n",
				elt->name);
			continue;
		}

		event_file = kmalloc(sizeof(*event_file), GFP_KERNEL);
		if (!event_file) {
			pr_warn("No memory to create event '%s'",
				   elt->name);
			return -ENOMEM;
		}
		event_file->enabled = false;
		event_file->elt = elt;

		if (!debugfs_create_file("enable", 0644, d_event, event_file,
					 &event_enable_fops)) {
			pr_warn("Could not create debugfs '%s/enable' entry\n",
				elt->system);
			kfree(event_file);
			continue;
		}

		list_add_tail(&event_file->list, &system_dir->event_files);
	}

	if (!debugfs_create_file("enable", 0644, d_events, NULL,
				 &tp2e_enable_fops))
		pr_warn("Could not create debugfs '%s/enable' entry\n",
			   elt->system);

	return 0;
}

/* Clean-up the tp2e debugfs */
static void tp2e_remove_dirs(void)
{
	struct tp2e_system_dir *current_system, *next_system;
	struct tp2e_event_file *current_event, *next_event;

	list_for_each_entry_safe(current_system, next_system,
				 &tp2e_systems_list, list) {
		list_for_each_entry_safe(current_event, next_event,
					 &current_system->event_files, list) {
			list_del(&current_event->list);
			kfree(current_event);
		}
		list_del(&current_system->list);
		kfree(current_system);
	}

	debugfs_remove_recursive(d_tp2e);
}



static int __init tp2e_init(void)
{
	INIT_LIST_HEAD(&tp2e_list);
	INIT_LIST_HEAD(&tp2e_systems_list);

/* Including tp2e_includes.h file with ADD_TP2E_ELEMENT being defined
 * results in adding all the tp2e elements to the tp2e_list
 */
#define ADD_TP2E_ELT
#include "tp2e_probes.h"
#undef ADD_TP2E_ELT

	tp2e_create_dirs();
	return 0;
}

static void __exit tp2e_exit(void)
{
	tp2e_remove_dirs();
}

fs_initcall(tp2e_init);
module_exit(tp2e_exit);

MODULE_LICENSE("GPL");
