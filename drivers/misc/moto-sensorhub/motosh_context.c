/* Copyright (c) 2016, Motorola Mobility, Inc.
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

#include <linux/irq.h>
#include <linux/motosh.h>
#include <linux/motosh_context.h>
#include <linux/ktime.h>
#include <linux/time.h>

#define VIBE_DEFAULT 2000 /* assume long vibe duration */
#define MAX_READ_RATE 200000000 /* 200 ms in ns */

/*
 * Basic check assuming a default vibe duration
 */
uint8_t motosh_tabletop_mode(void)
{
	return motosh_tabletop_mode_hold(VIBE_DEFAULT);
}

/*
 * This function checks the flat (tabletop) state of the
 * device and can hold in tabletop mode for 3 seconds beyond
 * the vibe duration in anticipation of a next vibe trigger
 *
 * millis - duration of vibe pulse in millis
 *          if set to MOTOSH_CONTEXT_TT_NOHOLD,
 *          do not apply any holdoff.
 *
 * Returns 1 when device is flat up or flat down
 * and continues to report 1 if called repetitvely
 * within the hold time (normally millis + 3 sec).
 * Returns 0 otherwise.
 */
uint8_t motosh_tabletop_mode_hold(int millis)
{
	/* default to table top mode */
	static uint8_t tabletop = 1;

	unsigned char cmd;
	unsigned char flat = 0;
	int err;
	static time_t last_flat_time;
	static int64_t last_read_time_ns;
	int64_t now_ns;
	struct timespec ts;
	int holdtime;

	get_monotonic_boottime(&ts);
	now_ns = ts.tv_sec*1000000000LL + ts.tv_nsec;

	/* Gate the rate of reads to sensorhub */
	if (now_ns - last_read_time_ns < MAX_READ_RATE)
		return tabletop;

	if (motosh_misc_data == NULL)
		return tabletop;

	if (motosh_misc_data->mode > BOOTMODE) {

		if (millis < 0)
			holdtime = 0; /* always read flat status */
		else
			holdtime = (millis + 3000)/1000; /* vibe plus 3 sec */

		if (ts.tv_sec - last_flat_time >= holdtime ||
		    !last_flat_time) {
			cmd = FLAT_DATA;
			err = motosh_i2c_write_read(motosh_misc_data,
						    &cmd, &flat, 1, 1);
			last_read_time_ns = now_ns;

			/* read OK and not flat */
			if (err >= 0 && !flat)
				tabletop = 0;
			else
				tabletop = 1;

			dev_dbg(&motosh_misc_data->client->dev,
				"TT mode: %d, err %d", tabletop, err);
		} else {
			dev_dbg(&motosh_misc_data->client->dev,
				 "TT mode: holding TT %ld sec left",
				 holdtime - (ts.tv_sec - last_flat_time));
		}

		/* while vibe continues to trigger in TT mode, keep
		   bumping hold time */
		if (tabletop)
			last_flat_time = ts.tv_sec;

	} else {
		tabletop = 1;
		dev_err(&motosh_misc_data->client->dev,
			 "TT mode: unavailable");
	}
	return tabletop;
}
