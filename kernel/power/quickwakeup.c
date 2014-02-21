/* kernel/power/quickwakeup.c
 *
 * Copyright (C) 2014 Motorola Mobility LLC.
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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/quickwakeup.h>

static LIST_HEAD(qw_head);
static DEFINE_MUTEX(list_lock);

int quickwakeup_register(struct quickwakeup_ops *ops)
{
	mutex_lock(&list_lock);
	list_add(&ops->list, &qw_head);
	mutex_unlock(&list_lock);

	return 0;
}

void quickwakeup_unregister(struct quickwakeup_ops *ops)
{
	mutex_lock(&list_lock);
	list_del(&ops->list);
	mutex_unlock(&list_lock);
}

static int quickwakeup_check(void)
{
	int check = 0;
	struct quickwakeup_ops *index;

	mutex_lock(&list_lock);

	list_for_each_entry(index, &qw_head, list) {
		int ret = index->qw_check(index->data);
		index->execute = ret;
		check |= ret;
		pr_debug("%s: %s votes for %s\n", __func__, index->name,
			ret ? "execute" : "dont care");
	}

	mutex_unlock(&list_lock);

	return check;
}

/* return 1 => suspend again
   return 0 => continue wakeup
 */
static int quickwakeup_execute(void)
{
	int suspend_again = 0;
	int final_vote = 1;
	struct quickwakeup_ops *index;

	mutex_lock(&list_lock);

	list_for_each_entry(index, &qw_head, list) {
		if (index->execute) {
			int ret = index->qw_execute(index->data);
			index->execute = 0;
			final_vote &= ret;
			suspend_again = final_vote;
			pr_debug("%s: %s votes for %s\n", __func__, index->name,
				ret ? "suspend again" : "wakeup");
		}
	}

	mutex_unlock(&list_lock);

	pr_debug("%s: %s\n", __func__,
		suspend_again ? "suspend again" : "wakeup");

	return suspend_again;
}

/* return 1 => suspend again
   return 0 => continue wakeup
 */
bool quickwakeup_suspend_again(void)
{
	int ret = 0;

	if (quickwakeup_check())
		ret = quickwakeup_execute();

	pr_debug("%s- returning %d %s\n", __func__, ret,
		ret ? "suspend again" : "wakeup");

	return ret;
}
