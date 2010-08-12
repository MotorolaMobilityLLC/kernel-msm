/*
 *  Originally from linux/arch/arm/lib/delay.S
 *
 *  Copyright (C) 1995, 1996 Russell King
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/delay.h>

/*
 * loops = usecs * HZ * loops_per_jiffy / 1000000
 *
 * Oh, if only we had a cycle counter...
 */
void __delay(unsigned long loops)
{
	asm volatile(
	"1:	subs %0, %0, #1 \n"
	"	bhi 1b		\n"
	: /* No output */
	: "r" (loops)
	);
}
EXPORT_SYMBOL(__delay);

/*
 * 0 <= xloops <= 0x7fffff06
 * loops_per_jiffy <= 0x01ffffff (max. 3355 bogomips)
 */
void __const_udelay(unsigned long xloops)
{
	unsigned long lpj;
	unsigned long loops;

	xloops >>= 14;			/* max = 0x01ffffff */
	lpj = loops_per_jiffy >> 10;	/* max = 0x0001ffff */
	loops = lpj * xloops;		/* max = 0x00007fff */
	loops >>= 6;			/* max = 2^32-1 */

	if (loops)
		__delay(loops);
}
EXPORT_SYMBOL(__const_udelay);

/*
 * usecs  <= 2000
 * HZ  <= 1000
 */
void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * ((2199023*HZ)>>11));
}
EXPORT_SYMBOL(__udelay);
