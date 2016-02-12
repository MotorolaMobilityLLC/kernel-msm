/*
 * Copyright (C) 2015-2016 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP

#include <linux/motosh.h>

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CTRL_ACCESS_BLK)
#include <linux/control_access_block.h>

static void touch_access_pack(uint8_t *, struct touch_control_access_block *);
static int touch_access_size(struct touch_control_access_block *);

/**
 * \brief Check with the touch driver for any configuration
 *        updates that sensorhub needs to know about.  If there
 *        are updates write them down to the sensorhub.
 *
 * \param[in] check_type can be NORMAL_CHECK or FORCE_UPDATE.
 *
 * \return 0: No updates 1: Sensorhub updated
 */
int motosh_check_touch_config_locked(enum check_touch_type check_type)
{
	struct touch_control_access_block *cab;
	int updated = 0;

	/* Call into the touch driver and obtain
	 * the control access block */
	cab = control_access_block_get();

	if (cab != NULL) {
		if (cab->do_sync || check_type == FORCE_UPDATE) {
			uint8_t *write_buffer;
			int write_size;
			int err;

			write_size = MOTOSH_REG_SIZE +
					touch_access_size(cab);

			write_buffer = kzalloc(
					write_size,
					GFP_KERNEL);

			if (write_buffer != NULL) {
				write_buffer[0] = TOUCH_CONFIG;
				touch_access_pack(&write_buffer[1], cab);

				err = motosh_i2c_write(motosh_misc_data,
						write_buffer,
						write_size);

				kzfree(write_buffer);

				if (err == 0) {
					/* Clear the do_sync variable in the
					 * cab to indicate this update was
					 * processed. */
					cab->do_sync = 0;
					updated = 1;
				}
			}
		}
	}

	return updated;
}

/**
 * \brief Pack the touch access block into the specified buffer
 *
 * \param[out] data The packed cab.
 * \param[in] cab The input cab structure.
 */
static void touch_access_pack(uint8_t *data,
		struct touch_control_access_block *cab)
{
	int i, j, m;
	uint8_t grp_num = 0;
	uint8_t *data_start;

	if (cab == NULL)
		return;

	/* skip the sync and group parameters because
	 * they will be written at the end */
	data_start = data;
	data += sizeof(cab->do_sync) + sizeof(cab->grp_num);

	/* Loop over each group in the cab and copy out its data */
	for (i = 0; i < cab->grp_num; i++) {
		struct grp_info *ginfo = cab->grp_ptr[i];

		/* skip empty slots */
		if (!ginfo)
			continue;

		/* Valid group with data */
		grp_num++;

		/* Copy out this groups data */
		memcpy(data, &(ginfo->group), sizeof(ginfo->group));
		data += sizeof(ginfo->group);

		memcpy(data, &(ginfo->data_blk_cnt),
			sizeof(ginfo->data_blk_cnt));
		data += sizeof(ginfo->data_blk_cnt);

		/* Loop over all the blocks in the group and copy them out */
		for (m = 0; m < ginfo->data_blk_cnt; m++) {
			if (ginfo->group & MODE_WO) {
				/* If this block is WO copy out its data */
				struct op_write *wo;

				if (ginfo->wo_data == NULL)
					continue;

				if (ginfo->wo_data[m] == NULL)
					continue;

				wo = ginfo->wo_data[m];

				memcpy(data, &(wo->paddr), sizeof(wo->paddr));
				data += sizeof(wo->paddr);

				memcpy(data, &(wo->blk_sz), sizeof(wo->blk_sz));
				data += sizeof(wo->blk_sz);

				memcpy(data, &(wo->blk_cnt),
					sizeof(wo->blk_cnt));
				data += sizeof(wo->blk_cnt);

				memcpy(data, &(wo->mask), sizeof(wo->mask));
				data += sizeof(wo->mask);

				if (wo->data == NULL)
					continue;

				/* Copy out this block WO data bytes */
				for (j = 0; j < wo->blk_cnt; j++) {
					memcpy(data,
						&wo->data[j * wo->blk_sz],
						wo->blk_sz);
					data += wo->blk_sz;
				}
			} else {
				/* If this block is RO copy out its data */
				struct op_read *ro;

				if (ginfo->ro_data == NULL)
					continue;

				if (ginfo->ro_data[m] == NULL)
					continue;

				ro = ginfo->ro_data[m];

				memcpy(data, &(ro->paddr), sizeof(ro->paddr));
				data += sizeof(ro->paddr);

				memcpy(data, &(ro->blk_sz), sizeof(ro->blk_sz));
				data += sizeof(ro->blk_sz);

				memcpy(data,
					&(ro->blk_cnt),
					sizeof(ro->blk_cnt));
				data += sizeof(ro->blk_cnt);
			}
		}
	}

	/* Copy out the top level cab data */
	memcpy(data_start, &(cab->do_sync), sizeof(cab->do_sync));

	/* Save the actual packed group number, instead of
	 * the number from the cab because that will include
	 * empty slots */
	memcpy(data_start + sizeof(cab->do_sync),
		&grp_num,
		sizeof(grp_num));
}

/*!
 * \brief Calculate the size of the packed cab
 *
 * \param[in] cab The structure to find the size of.
 *
 * \return size of the packed cab
 */
static int touch_access_size(struct touch_control_access_block *cab)
{
	int i, m;
	int size = 0;

	if (cab != NULL) {
		size += sizeof(cab->do_sync);
		size += sizeof(cab->grp_num);

		/* Loop over each group in the cab and count its size */
		for (i = 0; i < cab->grp_num; i++) {
			struct grp_info *ginfo = cab->grp_ptr[i];

			/* skip empty slots */
			if (!ginfo)
				continue;

			size += sizeof(ginfo->group);
			size += sizeof(ginfo->data_blk_cnt);

			/* Loop over all the blocks in the group
			 * and count its size */
			for (m = 0; m < ginfo->data_blk_cnt; m++) {
				if (ginfo->group & MODE_WO) {
					struct op_write *wo;

					if (ginfo->wo_data == NULL)
						continue;

					if (ginfo->wo_data[m] == NULL)
						continue;

					wo = ginfo->wo_data[m];

					size += sizeof(wo->paddr);
					size += sizeof(wo->blk_sz);
					size += sizeof(wo->blk_cnt);
					size += sizeof(wo->mask);

					if (wo->data == NULL)
						continue;

					size += wo->blk_sz * wo->blk_cnt;
				} else {
					struct op_read *ro;

					if (ginfo->ro_data == NULL)
						continue;

					if (ginfo->ro_data[m] == NULL)
						continue;

					ro = ginfo->ro_data[m];

					size += sizeof(ro->paddr);
					size += sizeof(ro->blk_sz);
					size += sizeof(ro->blk_cnt);
				}
			}
		}
	}

	return size;
}

#else

int motosh_check_touch_config_locked(enum check_touch_type check_type)
{
	((void)(check_type));
	return 0;
}

#endif /* #if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CTRL_ACCESS_BLK) */

#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */
