/*
 * SiW touch interrupt driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_IRQ_H
#define __SIW_TOUCH_IRQ_H

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>

/* for irq_desc */
#define istate core_internal_state__do_not_mess_with_it
#ifndef IRQS_PENDING
#define IRQS_PENDING 0x00000200
#endif


extern void siw_touch_enable_irq(struct device *dev, unsigned int irq);
extern void siw_touch_disable_irq(struct device *dev, unsigned int irq);

extern void siw_touch_resume_irq(struct device *dev);

extern void siw_touch_irq_control(struct device *dev, int on);

extern int siw_touch_request_irq(struct siw_ts *ts,
						irq_handler_t handler,
						irq_handler_t thread_fn,
						unsigned long flags,
						const char *name);
extern void siw_touch_free_irq(struct siw_ts *ts);


#endif	/* __SIW_TOUCH_IRQ_H */
