/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "../common.h"
#include "config.h"
#include "flash.h"

#define K (1024)
#define M (K * K)

/*
 * The table contains fundamental data used to program our flash, which
 * would be different according to the vendors.
 */
struct flash_table ft[] = {
    {0xEF, 0x6011, (128 * K), 256, (4 * K), (64 * K)},  /*  W25Q10EW  */
    {0xEF, 0x6012, (256 * K), 256, (4 * K), (64 * K)},  /*  W25Q20EW  */
    {0xC8, 0x6012, (256 * K), 256, (4 * K), (64 * K)},  /*  GD25LQ20B */
    {0xC8, 0x6013, (512 * K), 256, (4 * K), (64 * K)},  /*  GD25LQ40 */
    {0x85, 0x6013, (4 * M), 256, (4 * K), (64 * K)},
    {0xC2, 0x2812, (256 * K), 256, (4 * K), (64 * K)},
    {0x1C, 0x3812, (256 * K), 256, (4 * K), (64 * K)},
};

struct flash_table *flashtab;

int core_flash_poll_busy(void)
{
    int timer = 500, res = 0;

    core_config_ice_mode_write(0x041000, 0x0, 1);   /* CS low */
    core_config_ice_mode_write(0x041004, 0x66aa55, 3);  /* Key */

    core_config_ice_mode_write(0x041008, 0x5, 1);
    while (timer > 0) {
	core_config_ice_mode_write(0x041008, 0xFF, 1);

	mdelay(1);

	if ((core_config_read_write_onebyte(0x041010) & 0x03) == 0x00)
	goto out;
	timer--;
    }

    ipio_err("Polling busy Time out !\n");
    res = ERROR;
out:
    core_config_ice_mode_write(0x041000, 0x1, 1);   /* CS high */
    return res;
}
EXPORT_SYMBOL(core_flash_poll_busy);

int core_flash_write_enable(void)
{
    if (core_config_ice_mode_write(0x041000, 0x0, 1) < 0)
	goto out;
    if (core_config_ice_mode_write(0x041004, 0x66aa55, 3) < 0)
	goto out;
    if (core_config_ice_mode_write(0x041008, 0x6, 1) < 0)
	goto out;
    if (core_config_ice_mode_write(0x041000, 0x1, 1) < 0)
	goto out;

    return 0;

out:
    ipio_err("Write enable failed !\n");
    return -EIO;
}
EXPORT_SYMBOL(core_flash_write_enable);

void core_flash_enable_protect(bool enable)
{
    ipio_info("Set flash protect as (%d)\n", enable);

    if (core_flash_write_enable() < 0) {
	ipio_err("Failed to config flash's write enable\n");
	return;
    }

    core_config_ice_mode_write(0x041000, 0x0, 1);   /* CS low */
    core_config_ice_mode_write(0x041004, 0x66aa55, 3);  /* Key */

    switch (flashtab->mid) {
    case 0xEF:
	if (flashtab->dev_id == 0x6012 || flashtab->dev_id == 0x6011) {
	    core_config_ice_mode_write(0x041008, 0x1, 1);
	    core_config_ice_mode_write(0x041008, 0x00, 1);

	if (enable)
		core_config_ice_mode_write(0x041008, 0x7E, 1);
	else
		core_config_ice_mode_write(0x041008, 0x00, 1);
	}
	break;
    case 0xC8:
	if (flashtab->dev_id == 0x6012 || flashtab->dev_id == 0x6013) {
	    core_config_ice_mode_write(0x041008, 0x1, 1);
	    core_config_ice_mode_write(0x041008, 0x00, 1);

	if (enable)
		core_config_ice_mode_write(0x041008, 0x7A, 1);
	else
		core_config_ice_mode_write(0x041008, 0x00, 1);
	}
	break;
    default:
	ipio_err("Can't find flash id, ignore protection\n");
	break;
    }

    core_config_ice_mode_write(0x041000, 0x1, 1);   /* CS high */
    mdelay(5);
}
EXPORT_SYMBOL(core_flash_enable_protect);

void core_flash_init(uint16_t mid, uint16_t did)
{
    int i = 0;

    ipio_info("M_ID = %x, DEV_ID = %x", mid, did);

    flashtab = kzalloc(sizeof(ft), GFP_KERNEL);
    if (ERR_ALLOC_MEM(flashtab)) {
	ipio_err("Failed to allocate flashtab memory, %ld\n", PTR_ERR(flashtab));
	return;
    }

    for (; i < ARRAY_SIZE(ft); i++) {
	if (mid == ft[i].mid && did == ft[i].dev_id) {
	    ipio_info("Find them in flash table\n");

	    flashtab->mid = mid;
	    flashtab->dev_id = did;
	    flashtab->mem_size = ft[i].mem_size;
	    flashtab->program_page = ft[i].program_page;
	    flashtab->sector = ft[i].sector;
	    flashtab->block = ft[i].block;
	break;
	}
    }

    if (i >= ARRAY_SIZE(ft)) {
	ipio_err("Can't find them in flash table, apply default flash config\n");
	flashtab->mid = mid;
	flashtab->dev_id = did;
	flashtab->mem_size = (256 * K);
	flashtab->program_page = 256;
	flashtab->sector = (4 * K);
	flashtab->block = (64 * K);
    }

    ipio_info("Max Memory size = %d\n", flashtab->mem_size);
    ipio_info("Per program page = %d\n", flashtab->program_page);
    ipio_info("Sector size = %d\n", flashtab->sector);
    ipio_info("Block size = %d\n", flashtab->block);
}
EXPORT_SYMBOL(core_flash_init);

void core_flash_remove(void)
{
    ipio_info("Remove core-flash memebers\n");

    ipio_kfree((void **)&flashtab);
}
EXPORT_SYMBOL(core_flash_remove);
