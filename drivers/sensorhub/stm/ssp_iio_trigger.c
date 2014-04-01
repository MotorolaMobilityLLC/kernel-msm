/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp.h"
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
/*
 * ssp_iio_data_rdy_trigger_set_state() set data ready interrupt state
 */
static const struct iio_trigger_ops ssp_iio_trigger_ops = {
	.owner = THIS_MODULE,
};

int ssp_iio_probe_trigger(struct ssp_data *data,
	struct iio_dev *indio_dev, struct iio_trigger *trig)
{
	int ret;

	trig = iio_trigger_alloc("%s-dev%d",
					indio_dev->name,
					indio_dev->id);
	if (trig == NULL)
		return -ENOMEM;
	trig->dev.parent = &data->client->dev;
	trig->ops = &ssp_iio_trigger_ops;
	ret = iio_trigger_register(trig);

	if (ret) {
		iio_trigger_free(trig);
		return -EPERM;
	}
	indio_dev->trig = trig;

	return 0;
}

void ssp_iio_remove_trigger(struct iio_trigger *trig)
{
	iio_trigger_unregister(trig);
	iio_trigger_free(trig);
}
