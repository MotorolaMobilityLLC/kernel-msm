/*
 * Source file for:
 * Cypress CapSense(TM) firmware downloader.
 *
 * Copyright (c) 2012-2013 Motorola Mobility LLC
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
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/irqflags.h>
#include "cycapsense_issp.h"

#define SEND_BIT(b, c_gpio, d_gpio)		do { \
		gpio_set_value(d_gpio, b); \
		gpio_set_value(c_gpio, 1); \
		gpio_set_value(c_gpio, 0); \
	} while (0)

#define SEND_PART_BYTE(b, c_gpio, d_gpio, c)	do { \
		int __c = c; unsigned char __v = b; \
		while (__c > 0) { \
			SEND_BIT((__v & 0x80) >> 7, c_gpio, d_gpio); \
			__v <<= 1; \
			__c -= 1; \
		} \
	} while (0)

#define SEND_BYTE(b, c_gpio, d_gpio)		do { \
		SEND_PART_BYTE(b, c_gpio, d_gpio, 8); \
	} while (0)


#define GET_BIT(c_gpio, d_gpio)	cycapsense_issp_get_bit(c_gpio, d_gpio)
#define GET_BYTE(c_gpio, d_gpio)	((unsigned char)(\
	(GET_BIT(c_gpio, d_gpio) << 7) | \
	(GET_BIT(c_gpio, d_gpio) << 6) | \
	(GET_BIT(c_gpio, d_gpio) << 5) | \
	(GET_BIT(c_gpio, d_gpio) << 4) | \
	(GET_BIT(c_gpio, d_gpio) << 3) | \
	(GET_BIT(c_gpio, d_gpio) << 2) | \
	(GET_BIT(c_gpio, d_gpio) << 1) | \
	(GET_BIT(c_gpio, d_gpio) << 0)\
	))

const unsigned char ascii2bin[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
#define a_2_b(a)	(ascii2bin[((a) & 0xff) - 0x30])
#define a_a_2_byte(a1, a2)	((a_2_b(a1) << 4) | a_2_b(a2))

const unsigned int id_set1_len = 594;
const unsigned char id_set1[] = {
	0xCA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0D, 0xEE, 0x21, 0xF7, 0xF0, 0x27, 0xDC, 0x40,
	0x9F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xE7, 0xC1,
	0xD7, 0x9F, 0x20, 0x7E, 0x7D, 0x88, 0x7D, 0xEE,
	0x21, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 0x1F, 0x70,
	0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0, 0x1F, 0xDE,
	0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0, 0x13, 0xF7,
	0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D, 0x18, 0x7D,
	0xFE, 0x25, 0x80
};
const unsigned int    wait_and_poll_end_len = 30;
const unsigned char    wait_and_poll_end[] = {
	0x00, 0x00, 0x00, 0x00
};
const unsigned int id_set2_len = 418;
const unsigned char id_set2[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0,
	0x0D, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D,
	0x18, 0x7D, 0xFE, 0x25, 0x80
};
const unsigned int sync_en_len = 110;
const unsigned char sync_en[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C
};
const unsigned int rd_id1_len = 11;
const unsigned char rd_id1[] = {
	0xBF, 0x00
};
const unsigned int rd_id2_len = 12;
const unsigned char rd_id2[] = {
	0xDF, 0x90
};
const unsigned int rd_id3_len = 12;
const unsigned char rd_id3[] = {
	0xFF, 0x30
};
const unsigned int rd_id4_len = 12;
const unsigned char rd_id4[] = {
	0xFF, 0x00
};
const unsigned int sync_dis_len = 110;
const unsigned char sync_dis[] = {
	0xDE, 0xE2, 0x1F, 0x71, 0x00, 0x7D, 0xFC, 0x01,
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C
};
const unsigned int erase_len = 396;
const unsigned char erase[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x85,
	0xFD, 0xFC, 0x01, 0xF7, 0x10, 0x07, 0xDC, 0x00,
	0x7F, 0x7B, 0x80, 0x7D, 0xE0, 0x0B, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x04, 0x7D, 0xF0,
	0x01, 0xF7, 0xC9, 0x87, 0xDF, 0x48, 0x1F, 0x7F,
	0x89, 0x60
};
const unsigned int cs_setup_len = 418;
const unsigned char cs_setup[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0,
	0x0F, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D,
	0x18, 0x7D, 0xFE, 0x25, 0x80
};
const unsigned int rd_cs1_len = 11;
const unsigned char rd_cs1[] = {
	0xBF, 0x20
};
const unsigned int rd_cs2_len = 12;
const unsigned char rd_cs2[] = {
	0xDF, 0x80
};
const unsigned int rw_setup_len = 66;
const unsigned char rw_setup[] = {
	0xDE, 0xF0, 0x1F, 0x78, 0x00, 0x7D, 0xA0, 0x03, 0xC0
};

const unsigned int wr_start_len = 4;
const unsigned char wr_start = 0x90;
const unsigned int wr_end_len = 3;
const unsigned char wr_end = 0xE0;

const unsigned int block_n_len = 11;
const unsigned char block_n[] = {
	0x9F, 0x40
};
const unsigned int block_n_end_len = 3;
const unsigned char block_n_end = 0xE0;

const unsigned int prog_and_verify_len = 440;
const unsigned char prog_and_verify[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x57, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

const unsigned int verify_setup_len = 440;
const unsigned char verify_setup[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA8,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x0F, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

const unsigned int secure_len = 440;
const unsigned char secure[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x27, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

const unsigned int rd_start_len = 4;
const unsigned char rd_start = 0xB0;
const unsigned int rd_end_len = 1;
const unsigned char rd_end = 0x80;

const unsigned int rd_security_setup_len = 88;
const unsigned char rd_security_setup[] = {
	0xDE, 0xE2, 0x1F, 0x60, 0x88, 0x7D, 0x84, 0x21,
	0xF7, 0xB8, 0x07
};
const unsigned int rd_sec1_start_len = 78;
const unsigned char rd_sec1_start[] = {
	0xDE, 0xE2, 0x1F, 0x72, 0x87, 0x7D, 0xCA, 0x01,
	0xF7, 0x28
};
const unsigned int rd_sec1_end_len = 25;
const unsigned char rd_sec1_end[] = {
	0xFB, 0x94, 0x03, 0x80
};

const unsigned int rd_sec2_len = 198;
const unsigned char rd_sec2[] = {
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01,
	0xF7, 0xB0, 0x07, 0xDF, 0x0B, 0xBF, 0x7C, 0xF2,
	0xFD, 0xF4, 0x61, 0xF7, 0xB8, 0x87, 0xDF, 0xE2,
	0x58
};

const unsigned int rd_sec3_start_len = 122;
const unsigned char rd_sec3_start[] = {
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01,
	0xF7, 0xB0, 0x07, 0xDF, 0x0A, 0x7F, 0x7C, 0xC0
};

const unsigned int rd_sec3_end_len = 47;
const unsigned char rd_sec3_end[] = {
	0xFB, 0xE8, 0xC3, 0xEF, 0xF1, 0x2C
};

static inline int cycapsense_issp_get_bit(int c_gpio, int d_gpio)
{
	gpio_set_value(c_gpio, 0);
	gpio_set_value(c_gpio, 1);
	return gpio_get_value(d_gpio);
}
static inline void cycapsense_issp_w2r(int c_gpio)
{
	gpio_set_value(c_gpio, 0);
	gpio_set_value(c_gpio, 1);
	gpio_set_value(c_gpio, 0);
	gpio_set_value(c_gpio, 1);
}
static inline void cycapsense_issp_r2w(int c_gpio)
{
	gpio_set_value(c_gpio, 0);
	gpio_set_value(c_gpio, 1);
}
static void cycapsense_issp_send_bits(int c_gpio, int d_gpio,
			const unsigned char *pbuf, int nbits)
{
	unsigned char val;

	gpio_direction_output(d_gpio, 1);
	while (nbits >= 8) {
		val = *pbuf++;
		nbits -= 8;
		SEND_BYTE(val, c_gpio, d_gpio);
	}
	SEND_PART_BYTE(*pbuf, c_gpio, d_gpio, nbits);
	gpio_direction_input(d_gpio);
}
/* this function has to be called with IRQ disabled by local_irq_save()*/
static bool cycapsense_issp_wait_and_poll(int c_gpio, int d_gpio)
{
	bool cont = true;
	u64 time_out = 2 * NSEC_PER_MSEC;
	unsigned int cpu = smp_processor_id();
	time_out += sched_clock_cpu(cpu);
	gpio_set_value(c_gpio, 1);
	do {
		gpio_set_value(c_gpio, 0);
		if (gpio_get_value(d_gpio))
			break;
		gpio_set_value(c_gpio, 1);
		cont = time_before64(sched_clock_cpu(cpu), time_out);
	} while (cont);
	if (!cont) {
		pr_debug("%s: start of mnemonic execution expired\n", __func__);
		return false;
	}
	time_out = 200 * NSEC_PER_MSEC;
	time_out += sched_clock_cpu(cpu);
	do {
		if (!gpio_get_value(d_gpio))
			break;
		cont = time_before64(sched_clock_cpu(cpu), time_out);
	} while (cont);
	if (!cont) {
		pr_debug("%s: mnemonic execution is too long\n", __func__);
		return false;
	}
	cycapsense_issp_send_bits(c_gpio, d_gpio, wait_and_poll_end,
			wait_and_poll_end_len);
	return true;
}

static bool cycapsense_issp_send_erase(int c_gpio, int d_gpio)
{
	unsigned long flags;
	bool ret;

	local_irq_save(flags);
	cycapsense_issp_send_bits(c_gpio, d_gpio, erase, erase_len);
	ret = cycapsense_issp_wait_and_poll(c_gpio, d_gpio);
	local_irq_restore(flags);

	if (ret)
		pr_info("%s: Capsense IC erasing successful\n", __func__);
	else
		pr_err("%s: Failed to erase capsense IC\n", __func__);
	return ret;
}

static void cycapsense_issp_send_block(const struct issp_data *d, int block)
{
	unsigned long flags;
	int offs, i;
	unsigned char addr;

	offs = d->block_size * block;
	addr = 0;/* 7-bit only, to have them in MSB will be incremented by 2 */

	local_irq_save(flags);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rw_setup, rw_setup_len);

	gpio_direction_output(d->d_gpio, 1);
	for (i = 0; i < d->block_size; i++, offs++, addr += 2) {
		SEND_PART_BYTE(wr_start, d->c_gpio, d->d_gpio, wr_start_len);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		SEND_BYTE(*(d->inf.data + offs), d->c_gpio, d->d_gpio);
		SEND_PART_BYTE(wr_end, d->c_gpio, d->d_gpio, wr_end_len);
	}
	gpio_direction_input(d->d_gpio);
	local_irq_restore(flags);
}

static bool cycapsense_issp_flash_block(const struct issp_data *d, int block)
{
	unsigned long flags;
	bool ret = true;

	local_irq_save(flags);

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, block_n, block_n_len);
	gpio_direction_output(d->d_gpio, 1);
	SEND_BYTE(block, d->c_gpio, d->d_gpio);
	SEND_PART_BYTE(block_n_end, d->c_gpio, d->d_gpio, block_n_end_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, prog_and_verify,
			prog_and_verify_len);

	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		ret = false;

	local_irq_restore(flags);

	return ret;
}

static int cycapsense_issp_read_status(const struct issp_data *d)
{
	unsigned long flags;
	unsigned int status;

	local_irq_save(flags);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_id1, rd_id1_len);
	cycapsense_issp_w2r(d->c_gpio);
	status = GET_BYTE(d->c_gpio, d->d_gpio);
	cycapsense_issp_r2w(d->c_gpio);
	gpio_direction_output(d->d_gpio, 1);
	SEND_BIT(1, d->c_gpio, d->d_gpio);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	local_irq_restore(flags);

	return status;
}

static bool cycapsense_issp_verify_setup(const struct issp_data *d, int block)
{
	unsigned long flags;
	bool ret = true;

	local_irq_save(flags);

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rw_setup, rw_setup_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, block_n, block_n_len);
	gpio_direction_output(d->d_gpio, 1);
	SEND_BYTE(block, d->c_gpio, d->d_gpio);
	SEND_PART_BYTE(block_n_end, d->c_gpio, d->d_gpio, block_n_end_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, verify_setup,
			verify_setup_len);

	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		ret = false;

	local_irq_restore(flags);

	return ret;
}

static bool cycapsense_issp_verify_block(const struct issp_data *d, int block)
{

	unsigned long flags;
	bool ret = true;

	int offs, i;
	unsigned char addr, val;

	offs = d->block_size * block;
	addr = 0;/* 7-bit only, to have them in MSB will be incremented by 2 */

	local_irq_save(flags);

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rw_setup, rw_setup_len);

	gpio_direction_output(d->d_gpio, 1);
	for (i = 0; i < d->block_size; i++, offs++, addr += 2) {
		SEND_PART_BYTE(rd_start, d->c_gpio, d->d_gpio, rd_start_len);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		cycapsense_issp_w2r(d->c_gpio);
		gpio_direction_input(d->d_gpio);
		val = GET_BYTE(d->c_gpio, d->d_gpio);
		cycapsense_issp_r2w(d->c_gpio);
		gpio_direction_output(d->d_gpio, 1);
		SEND_PART_BYTE(rd_end, d->c_gpio, d->d_gpio, rd_end_len);
		if (*(d->inf.data + offs) != val) {
			ret = false;
			break;
		}
	}
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	local_irq_restore(flags);

	return ret;
}

static bool cycapsense_issp_flash_security(const struct issp_data *d)
{
	unsigned long flags;
	bool ret = true;
	int i;
	unsigned char addr;

	local_irq_save(flags);

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rw_setup, rw_setup_len);
	gpio_direction_output(d->d_gpio, 1);
	/* addr is 7-bit only, to have bits in MSB will be incremented by 2 */
	for (i = 0, addr = 0; i < d->secure_bytes; i++, addr += 2) {
		SEND_PART_BYTE(wr_start, d->c_gpio, d->d_gpio, wr_start_len);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		SEND_BYTE(*(d->inf.s_data + i), d->c_gpio, d->d_gpio);
		SEND_PART_BYTE(wr_end, d->c_gpio, d->d_gpio, wr_end_len);
	}
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, secure, secure_len);
	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		ret = false;

	local_irq_restore(flags);
	return ret;

}

static bool cycapsense_issp_verify_security(const struct issp_data *d)
{
	unsigned long flags;
	bool ret = true;
	unsigned char addr, val;

	local_irq_save(flags);

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_security_setup,
		rd_security_setup_len);

	for (addr = 0; addr < d->secure_bytes * 2; addr += 2) {
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en,
			sync_en_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_sec1_start,
			rd_sec1_start_len);
		gpio_direction_output(d->d_gpio, 1);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_sec1_end,
			rd_sec1_end_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis,
			sync_dis_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_sec2,
			rd_sec2_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio,
			wait_and_poll_end, wait_and_poll_end_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_sec3_start,
			rd_sec3_start_len);
		gpio_direction_output(d->d_gpio, 1);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_sec3_end,
			rd_sec3_end_len);
		cycapsense_issp_send_bits(d->c_gpio, d->d_gpio,
			wait_and_poll_end, wait_and_poll_end_len);
	}

	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);

	gpio_direction_output(d->d_gpio, 1);
	for (addr = 0; addr < d->secure_bytes * 2; addr += 2) {
		SEND_PART_BYTE(rd_start, d->c_gpio, d->d_gpio, rd_start_len);
		SEND_PART_BYTE(addr, d->c_gpio, d->d_gpio, 7);
		cycapsense_issp_w2r(d->c_gpio);
		gpio_direction_input(d->d_gpio);
		val = GET_BYTE(d->c_gpio, d->d_gpio);
		cycapsense_issp_r2w(d->c_gpio);
		gpio_direction_output(d->d_gpio, 1);
		SEND_PART_BYTE(rd_end, d->c_gpio, d->d_gpio, rd_end_len);
		if (*(d->inf.s_data + addr/2) != val) {
			ret = false;
			break;
		}
	}
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	local_irq_restore(flags);

	return ret;
}


int cycapsense_issp_get_cs(const struct issp_data *d, unsigned int *cs)
{
	unsigned long flags;
	int ret = -EIO;

	local_irq_save(flags);
	/* 1: CHECKSUM-SETUP*/
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, cs_setup, cs_setup_len);
	/* 2: WAIT-AND-POLL  */
	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		goto get_cs_end;
	/* 3: SYNC-ENABLE  */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	/* 4: READ-CHECKSUM */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_cs1, rd_cs1_len);
	cycapsense_issp_w2r(d->c_gpio);
	*cs = GET_BYTE(d->c_gpio, d->d_gpio) << 8;
	cycapsense_issp_r2w(d->c_gpio);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_cs2, rd_cs2_len);
	cycapsense_issp_w2r(d->c_gpio);
	*cs |= GET_BYTE(d->c_gpio, d->d_gpio);
	cycapsense_issp_r2w(d->c_gpio);
	gpio_direction_output(d->d_gpio, 1);
	SEND_BIT(1, d->c_gpio, d->d_gpio);
	/* 5: SYNC-DISABLE */
	/* will set direction back to input*/
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	ret = 0;
get_cs_end:
	local_irq_restore(flags);
	if (ret)
		pr_err("%s: Could not get checksum\n", __func__);
	return ret;
}

/* this simplifyied version of Intel HEX, Cypress PSoC specific*/
int cycapsense_issp_parse_hex(struct issp_data *d, const u8 *src, int len)
{
	enum r_type { REC_DATA_T = 0, REC_EOF_T = 1, REC_EXT_L_ADDR_T = 4 };
	struct { int len; enum r_type type; int e_addr; } rec = { 0, 0, 0 };
	enum parser_state { REC_START, REC_LEN, REC_ADDR, REC_TYPE, REC_DATA,
			REC_CS } state = REC_START;
	int i = 0, j = 0, cnt;
	u16 cs = 0, f_cs;
	u8 *dst;
	u8 rec_cs = 0, val;

	if (d == NULL) {
		pr_err("%s: ID is not defined.\n", __func__);
		return -EINVAL;
	}

	cnt = d->block_size * d->num_of_blocks;
	d->inf.data = kzalloc(cnt, GFP_KERNEL);
	if (d->inf.data == NULL) {
		pr_err("%s: Error kzalloc (%d).\n", __func__, cnt);
		return -ENOMEM;
	}
	d->inf.s_data = kzalloc(d->secure_bytes, GFP_KERNEL);
	if (d->inf.s_data == NULL) {
		pr_err("%s: Error kzalloc (%d).\n", __func__, d->secure_bytes);
		kfree(d->inf.data);
		return -ENOMEM;
	}
	dst = d->inf.data; /*data always first in PSoC HEX (see 001-44475 doc)*/
	while (i < len) {
		switch (state) {
		case REC_START:
			if (src[i] == '\r' || src[i] == '\n')
				i++;
			else if (src[i++] != ':')
				goto parse_hex_end_err;
			else
				state = REC_LEN;
			break;
		case REC_LEN:
			if (len - i < 2)
				goto parse_hex_end_err;
			rec.len = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += rec.len;
			state = REC_ADDR;
			break;
		case REC_ADDR:
			if (len - i < 4)
				goto parse_hex_end_err;
			/* addr is not used by PSoC, only for check sum */
			val = a_a_2_byte(src[i], src[i+1]);
			rec_cs += val;
			val = a_a_2_byte(src[i+2], src[i+3]);
			rec_cs += val;
			i += 4;
			state = REC_TYPE;
			break;
		case REC_TYPE:
			if (len - i < 2)
				goto parse_hex_end_err;
			rec.type = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += rec.type;
			switch (rec.type) {
			case REC_DATA_T:
				state = REC_DATA;
				break;
			case REC_EXT_L_ADDR_T:
				if (len - i < 4)
					goto parse_hex_end_err;
				val = a_a_2_byte(src[i], src[i+1]);
				rec_cs += val;
				rec.e_addr = val << 8;
				val = a_a_2_byte(src[i+2], src[i+3]);
				i += 4;
				rec_cs += val;
				rec.e_addr |= val;
				if (rec.e_addr == 0x10) {
					if (j > cnt) {
						pr_err("%s: Too big for %s.\n",
						__func__, d->name);
						goto parse_hex_end_err;
					}
					d->inf.cs = cs;
					dst = d->inf.s_data;
					cnt = d->secure_bytes;
					j = 0;
				} else if (rec.e_addr == 0x20) {
					dst = (u8 *)&f_cs;
					j = 0;
					cnt = 2;
				} else {
					pr_err("%s: Unsupported ext addr\n",
						__func__);
					goto parse_hex_end_err;
				}
				state = REC_CS;
				break;
			case REC_EOF_T:
				if (be16_to_cpu(f_cs) != d->inf.cs) {
					pr_err("%s: Data check sum failed\n",
						__func__);
					goto parse_hex_end_err;
				}
				state = REC_CS;
				break;
			default: /* Cypress supports only types above */
				pr_err("%s: Unsupported record type\n",
					__func__);
				goto parse_hex_end_err;
			}
			break;
		case REC_DATA:
			if (len - i < rec.len * 2)
				goto parse_hex_end_err;
			for (; rec.len; rec.len--, i += 2, dst++, j++) {
				val = a_a_2_byte(src[i], src[i+1]);
				if (j < cnt) {
					*dst = val;
					cs += val;
				}
				rec_cs += val;
			}
			state = REC_CS;
			break;
		case REC_CS:
			if (len - i < 2)
				goto parse_hex_end_err;
			val = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += val;
			if (rec_cs != 0) {
				pr_err("%s: Record check sum failed\n",
					__func__);
				goto parse_hex_end_err;
			}
			state = REC_START;
			break;
		}
	}
	return 0;
parse_hex_end_err:
	pr_err("%s: HEX format error.\n", __func__);
	kfree(d->inf.data);
	d->inf.data = NULL;
	kfree(d->inf.s_data);
	d->inf.s_data = NULL;
	return -EINVAL;
}


int cycapsense_issp_verify_id(const struct issp_data *d)
{
	int ret = -EIO;
	unsigned short id = 0;
	unsigned long flags;

	/* 1: RESET */
	gpio_set_value(d->rst_gpio, 1);
	udelay(1000);
	gpio_set_value(d->rst_gpio, 0);

	local_irq_save(flags);
	/* 2: ID-SETUP-1 */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, id_set1, id_set1_len);
	/* 3: WAIT-AND-POLL  */
	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		goto verify_id_end;
	/* 4: ID-SETUP-2 */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, id_set2, id_set2_len);
	/* 5: WAIT-AND-POLL  */
	if (!cycapsense_issp_wait_and_poll(d->c_gpio, d->d_gpio))
		goto verify_id_end;
	/* 6: SYNC-ENABLE  */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_en, sync_en_len);
	/* 7: READ-ID-WORD */
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_id1, rd_id1_len);
	cycapsense_issp_w2r(d->c_gpio);
	id = GET_BYTE(d->c_gpio, d->d_gpio) << 8;
	cycapsense_issp_r2w(d->c_gpio);
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, rd_id2, rd_id2_len);
	cycapsense_issp_w2r(d->c_gpio);
	id |= GET_BYTE(d->c_gpio, d->d_gpio);
	cycapsense_issp_r2w(d->c_gpio);
	gpio_direction_output(d->d_gpio, 1);
	SEND_BIT(1, d->c_gpio, d->d_gpio);
	/* 8: SYNC-DISABLE */
	/* will set direction back to input*/
	cycapsense_issp_send_bits(d->c_gpio, d->d_gpio, sync_dis, sync_dis_len);
	if (id == d->silicon_id)
		ret = 0;

verify_id_end:
	local_irq_restore(flags);
	if (ret == 0)
		pr_info("%s: Device %s is detected\n", __func__, d->name);
	else if (id != 0)
		pr_err("%s: Detected unsupported device with id 0x%04X\n",
			__func__, id);
	else
		pr_err("%s: Failed to verify silicon id for %s (0x%04X)\n",
			__func__, d->name, d->silicon_id);
	return ret;
}

int cycapsense_issp_erase(const struct issp_data *d)
{
	int ret = -EIO;

	if (cycapsense_issp_verify_id(d) == 0
		&& cycapsense_issp_send_erase(d->c_gpio, d->d_gpio))
		ret = 0;

	return ret;
}

int cycapsense_issp_dnld(const struct issp_data *d)
{
	int i, st;
	unsigned int chks;

	if (d->inf.data == NULL || d->inf.s_data == NULL) {
		pr_err("%s: Invalid input arguments\n", __func__);
		return -EINVAL;
	}

	/* 2: Erase (assuming verify id done by caller) */
	if (!cycapsense_issp_send_erase(d->c_gpio, d->d_gpio))
		return -EIO;
	for (i = 0; i < d->num_of_blocks; i++) {
		cycapsense_issp_send_block(d, i);
		if (!cycapsense_issp_flash_block(d, i)) {
			pr_err("%s: Flash block failed (%d)\n", __func__, i);
			return -EIO;
		}
		st = cycapsense_issp_read_status(d);
		if (st) {
			pr_err("%s: Error flashing: %d(%d)\n", __func__, st, i);
			return -EIO;
		}
	}
	for (i = 0; i < d->num_of_blocks; i++) {
		if (!cycapsense_issp_verify_setup(d, i)) {
			pr_err("%s: Verify setup failed (%d)\n", __func__, i);
			return -EIO;
		}
		st = cycapsense_issp_read_status(d);
		if (st) {
			pr_err("%s: Verify setup staus error: %d(%d)\n",
				__func__, st, i);
			return -EIO;
		}
		if (!cycapsense_issp_verify_block(d, i)) {
			pr_err("%s: Verify block failed (%d)\n", __func__, i);
			return -EIO;
		}
	}
	if (!cycapsense_issp_flash_security(d)) {
		pr_err("%s: Flashing security failed\n", __func__);
		return -EIO;
	}
	if (!cycapsense_issp_verify_security(d)) {
		pr_err("%s: Security verification failed\n", __func__);
		return -EIO;
	}
	if (cycapsense_issp_get_cs(d, &chks) < 0)
		return -EIO;
	if (chks != d->inf.cs) {
		pr_err("%s: Flashing failed. Checksum not match\n", __func__);
		return -EIO;
	}


	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cypress CapSense(R) firmware downloader");
MODULE_AUTHOR("Motorola Mobility LLC");
