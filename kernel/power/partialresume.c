/* kernel/power/partialresume.c
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/partialresume.h>
#include <linux/wakeup_reason.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/mutex.h>


static DEFINE_MUTEX(pr_handlers_lock);
static LIST_HEAD(pr_handlers);
static LIST_HEAD(pr_matches);
static struct partial_resume_stats match_stats;
static struct partial_resume_stats consensus_stats;
static struct kobject *partialresume;

bool suspend_again_match(const struct list_head *irqs,
			 const struct list_head *unfinished)
{
	const struct wakeup_irq_node *i;
	struct partial_resume *h, *match;

	INIT_LIST_HEAD(&pr_matches);

	match_stats.total++;

	if (!irqs || list_empty(irqs))
		return false;

	list_for_each_entry(i, irqs, next) {
		match = NULL;
		list_for_each_entry(h, &pr_handlers, next_handler) {
			if (i->irq == h->irq) {
				match = h;
				break;
			}
		}
		if (!match) {
			pr_debug("%s: wakeup irq %d does not have a handler\n", __func__, i->irq);
			return false;
		}
		list_add(&match->next_match, &pr_matches);
	}

	match_stats.total_yes++;

	return true;
}


bool suspend_again_consensus(void)
{
	struct partial_resume *h;

	BUG_ON(list_empty(&pr_matches));
	list_for_each_entry(h, &pr_matches, next_match) {
		h->stats.total++;
		if (!h->partial_resume(h)) {
			pr_debug("%s: partial-resume for %d: false\n", __func__, h->irq);
			return false;
		}
		h->stats.total_yes++;
		pr_debug("%s: partial-resume for %d: true\n", __func__, h->irq);
	}

	consensus_stats.total_yes++;

	return true;
}


int register_partial_resume(struct partial_resume *handler)
{
	struct partial_resume *e;

	if (!handler || !handler->irq || !handler->partial_resume)
		return -EINVAL;

	mutex_lock(&pr_handlers_lock);
	list_for_each_entry(e, &pr_handlers, next_handler) {
		if (e->irq == handler->irq) {
			if (e->partial_resume == handler->partial_resume) {
				mutex_unlock(&pr_handlers_lock);
				return 0;
			}
			pr_err("%s: error registering %pF for irq %d: "\
			       "%pF already registered\n",
				__func__,
				handler->partial_resume,
				e->irq,
				e->partial_resume);
			mutex_unlock(&pr_handlers_lock);
			return -EIO;
		}
	}

	list_add(&handler->next_handler, &pr_handlers);
	mutex_unlock(&pr_handlers_lock);
	return 0;
}

void unregister_partial_resume(struct partial_resume *handler)
{
	mutex_lock(&pr_handlers_lock);
	list_del(&handler->next_handler);
	mutex_unlock(&pr_handlers_lock);
}


static ssize_t partialresume_stats_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	ssize_t offset = 0;
	struct partial_resume *h;

	offset += sprintf(buf + offset, "global: %d %d %d \n",
			match_stats.total,
			match_stats.total_yes,
			consensus_stats.total_yes);

	mutex_lock(&pr_handlers_lock);

	list_for_each_entry(h, &pr_handlers, next_handler) {
		offset += sprintf(buf + offset, "%d: %d %d\n",
				h->irq,
				h->stats.total,
				h->stats.total_yes);
	}

	mutex_unlock(&pr_handlers_lock);

	return offset;
}

static struct kobj_attribute partialresume_stats = __ATTR_RO(partialresume_stats);

static struct attribute *partialresume_stats_attrs[] = {
	&partialresume_stats.attr,
	NULL,
};
static struct attribute_group partialresume_stats_attr_group = {
	.attrs = partialresume_stats_attrs,
};

int __init partial_resume_init(void)
{
	int rc = -EIO;

	partialresume = kobject_create_and_add("partialresume", kernel_kobj);
	if (!partialresume) {
		pr_warning("%s: failed to create a sysfs kobject\n", __func__);
		goto fail;
	}

	rc = sysfs_create_group(partialresume, &partialresume_stats_attr_group);
	if (rc) {
		pr_warning("%s: failed to create a sysfs group\n", __func__);
		goto fail_kobject_put;
	}

	return 0;

#if 0
fail_remove_group:
	sysfs_remove_group(partialresume, &partialresume_stats_attr_group);
#endif
fail_kobject_put:
	kobject_put(partialresume);
fail:
	return rc;
}

subsys_initcall(partial_resume_init);
