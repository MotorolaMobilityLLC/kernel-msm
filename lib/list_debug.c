/*
 * Copyright 2006, Red Hat, Inc., Dave Jones
 * Released under the General Public License (GPL).
 *
 * This file contains the linked list implementations for
 * DEBUG_LIST.
 */

#include <linux/export.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/rculist.h>

#ifdef CONFIG_X86_64
static unsigned long count_bits(unsigned long value)
{
	value = value - ((value >> 1) & 0x5555555555555555);
	value = (value & 0x3333333333333333) + ((value >> 2) & 0x3333333333333333);
	return (((value + (value >> 4)) & 0xF0F0F0F0F0F0F0F) * 0x101010101010101) >> 56;
}
#else
static unsigned long count_bits(unsigned long value)
{
	value = value - ((value >> 1) & 0x55555555);
	value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
	return (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
#endif

inline int check_list_corruption(void *ptr1, void *ptr2, void *ptr3, const char *func_name,
		const char *ptr1_name, const char *ptr2_name, const char *ptr3_name) {
	unsigned long delta_bits = (unsigned long)ptr1 ^ (unsigned long)ptr2;
	if (!delta_bits)
		return 0;

	if (count_bits(delta_bits)  < 3) {
		/* less than 3 bits differ; probably a bit flip...*/
		panic("Bit flip in %s: value %p should be %p\n", func_name, ptr2, ptr1);
	}

	return WARN(1,
		"%s corruption. %s should be "
		"%s (%p), but was %p. (%s=%p).\n",
		func_name, ptr1_name, ptr2_name, ptr2, ptr1, ptr3_name, ptr3);
}



/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */

void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	check_list_corruption(next->prev, prev, next, __func__, "next->prev", "prev", "next");
	check_list_corruption(prev->next, next, prev, __func__, "prev->next", "next", "prev");
	WARN(new == prev || new == next,
	     "list_add double add: new=%p, prev=%p, next=%p.\n",
	     new, prev, next);
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}
EXPORT_SYMBOL(__list_add);

void __list_del_entry(struct list_head *entry)
{
	struct list_head *prev, *next;

	prev = entry->prev;
	next = entry->next;

	if (WARN(next == LIST_POISON1,
		"list_del corruption, %p->next is LIST_POISON1 (%p)\n",
		entry, LIST_POISON1) ||
	    WARN(prev == LIST_POISON2,
		"list_del corruption, %p->prev is LIST_POISON2 (%p)\n",
		entry, LIST_POISON2) ||
	    check_list_corruption(next->prev, entry, next,
		__func__, "next->prev", "entry", "next") ||
	    check_list_corruption(prev->next, entry, prev,
		__func__, "prev->next", "entry", "prev"))
		return;

	__list_del(prev, next);
}
EXPORT_SYMBOL(__list_del_entry);

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}
EXPORT_SYMBOL(list_del);

/*
 * RCU variants.
 */
void __list_add_rcu(struct list_head *new,
		    struct list_head *prev, struct list_head *next)
{
	check_list_corruption(next->prev, prev, next, __func__, "next->prev", "prev", "next");
	check_list_corruption(prev->next, next, prev, __func__, "prev->next", "next", "prev");
	new->next = next;
	new->prev = prev;
	rcu_assign_pointer(list_next_rcu(prev), new);
	next->prev = new;
}
EXPORT_SYMBOL(__list_add_rcu);
