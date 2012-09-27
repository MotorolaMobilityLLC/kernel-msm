/*
 * Copyright (C) 2011 Motorola, Inc.
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

#ifndef _W1_DS2502_H
#define _W1_DS2502_H

#define NUM_BATTERY_COMMAND           3

#define SHIFT_RIGHT_MSB_SET           0x80
#define SHIFT_RIGHT_MSB_CLEAR         0x00
#define SHIFT_RIGHT_LSB               0x01

#define READ_DATA_COMMAND             0xC3
#define SEARCH_ROM_COMMAND            0xF0
#define DATA_ADDRESS_MSB              0x00
#define DATA_ADDRESS_LSB              0x00

#define MEM_BIT_COUNT                 64

#define MAX_RETRIES                   4

#define BATT_AUX_SEL                  0x2F
#define MAIN_BATT_PRESENT             0

#define NUM_SERIAL_NUMBER             3
#define NUM_SECURITY_EPROM            8

#define UID_SIZE                      8
#define EEPROM_SIZE                   192
#define EEPROM_PAGE_SIZE              0x20

#define MMI_MANUFACTURE_CODE          0x5000

extern ssize_t w1_ds2502_read_eeprom(struct w1_slave *sl,
				     char *buf,
				     size_t count);
extern ssize_t w1_ds2502_read_uid(struct w1_slave *sl, char *buf, size_t count);

#endif /* !_W1_DS2502_H */
