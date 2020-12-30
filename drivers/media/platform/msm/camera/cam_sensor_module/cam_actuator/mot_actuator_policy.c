/*
 * Copyright (C) 2020 Motorola Mobility LLC.
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
#include <linux/module.h>
#include <linux/types.h>
#include "cam_debug_util.h"
#include "mot_actuator_policy.h"

extern int mot_actuator_on_vibrate_start(void);
extern int mot_actuator_on_vibrate_stop(void);

static atomic_t mot_actuator_ref_count = ATOMIC_INIT(0);
static unsigned long mot_actuator_consumers = ATOMIC_INIT(0);
static struct mutex actuator_res_lock = __MUTEX_INITIALIZER(actuator_res_lock);

int mot_actuator_get(mot_actuator_client user)
{
	if (user <= ACTUATOR_CLIENT_INVALID || user >= ACTUATOR_CLIENT_MAX) {
		CAM_ERR(CAM_ACTUATOR, "INVALID CLIENT!!!");
		return 0;
	}

	CAM_DBG(CAM_ACTUATOR, "Obtain actuator from: %d", user);

	test_and_set_bit(user, &mot_actuator_consumers);

	return atomic_add_return(1, &mot_actuator_ref_count);
}
EXPORT_SYMBOL(mot_actuator_get);

int mot_actuator_put(mot_actuator_client user)
{
	unsigned long consumers = 0;
	if (user <= ACTUATOR_CLIENT_INVALID || user >= ACTUATOR_CLIENT_MAX) {
		CAM_ERR(CAM_ACTUATOR, "INVALID CLIENT!!!");
		return 0;
	}

	CAM_DBG(CAM_ACTUATOR, "Release actuator from: %d", user);

	test_and_clear_bit(user, &mot_actuator_consumers);

	consumers = mot_actuator_get_consumers();
	if (((consumers & CLINET_VIBRATOR_MASK) == CLINET_VIBRATOR_MASK) &&
		(user != ACTUATOR_CLIENT_VIBRATOR)) {
		//If only vibrator noise eliminator exists, move lens to required position
		CAM_DBG(CAM_ACTUATOR, "Set actuator pos for vibrator");
		mot_actuator_on_vibrate_start();
		if ((mot_actuator_get_consumers() & CLINET_VIBRATOR_MASK) == 0) {
			mot_actuator_on_vibrate_stop();
		}
	}

	return atomic_sub_return(1, &mot_actuator_ref_count);
}
EXPORT_SYMBOL(mot_actuator_put);

int mot_actuator_get_ref_count(void)
{
	return atomic_read(&mot_actuator_ref_count);
}
EXPORT_SYMBOL(mot_actuator_get_ref_count);

unsigned int mot_actuator_get_consumers(void)
{
	return mot_actuator_consumers;
}
EXPORT_SYMBOL(mot_actuator_get_consumers);

void mot_actuator_lock(void)
{
	mutex_lock(&actuator_res_lock);
}
EXPORT_SYMBOL(mot_actuator_lock);

void mot_actuator_unlock(void)
{
	mutex_unlock(&actuator_res_lock);
}
EXPORT_SYMBOL(mot_actuator_unlock);

ssize_t mot_actuator_dump(char *buf)
{
	int item_len = 0;
	unsigned int consumers = mot_actuator_get_consumers();

	item_len += sprintf(buf+item_len, "refCount: %d\n", mot_actuator_get_ref_count());
	item_len += sprintf(buf+item_len, "Consumers (0x%x):\n", consumers);

	if (consumers & CLINET_CAMERA_MASK) {
		item_len += sprintf(buf+item_len, "camera");
	}

	if (consumers & CLINET_VIBRATOR_MASK) {
		item_len += sprintf(buf+item_len, " vibrator");
	}

	item_len += sprintf(buf+item_len, "\n");

	CAM_DBG(CAM_ACTUATOR, "ref cnt:%d, consumers:0x%x", mot_actuator_get_ref_count(), mot_actuator_get_consumers());
	return item_len;
}
EXPORT_SYMBOL(mot_actuator_dump);
