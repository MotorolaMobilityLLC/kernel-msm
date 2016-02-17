/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "slimport.h"
#ifdef QUICK_CHARGE_SUPPORT
#include "quick_charge.h"

static struct blocking_notifier_head charger_notifier_head;

struct blocking_notifier_head *get_notifier_list_head(void)
{
	return &charger_notifier_head;
}

EXPORT_SYMBOL(get_notifier_list_head);


int register_slimport_charge_status(struct notifier_block *nb)
{

	return blocking_notifier_chain_register(&charger_notifier_head, nb);

}
int unregister_slimport_charge_status(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charger_notifier_head, nb);
}

int get_slimport_max_charge_voltage(void)
{
	return get_dongle_capability();
}

int get_slimport_charger_type(void)
{
	return get_input_charger_type();

}

int request_slimport_charge_voltage(int voltage)
{
	return  set_request_voltage(voltage);
}

int is_request_ack_back(void)
{
	return is_sink_charge_ack_back();
}
#endif/*#ifdef QUICK_CHARGE_SUPPORT*/
