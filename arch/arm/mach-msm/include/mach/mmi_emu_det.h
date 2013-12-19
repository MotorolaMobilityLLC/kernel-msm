/*
 *  * Copyright (C) 2011-2012 Motorola, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __MMI_EMU_DETECTION_H__
#define __MMI_EMU_DETECTION_H__

struct mmi_emu_det_platform_data {
	int accy_support;
	int (*enable_5v)(int on);
	int (*core_power)(int on);
	int (*id_protect)(int on);
	int (*alt_mode)(int on);
	void (*gpio_mode)(int mode);
	int (*adc_id)(void);
	void (*dp_dm_mode)(int mode);
	void (*gsbi_ctrl)(int restore);
	void (*cfg_l2_err)(int enable);
};

#define ACCY_SUPPORT_FULL	0
#define ACCY_SUPPORT_BASIC	1

#define GPIO_MODE_STANDARD	0
#define GPIO_MODE_ALTERNATE	1
#define GPIO_MODE_ALTERNATE_2	2

/*
	Definition of EMU detection's IOs:
	name/id, optional, #-of-configs
*/
#undef DECLARE
#define EMU_DET_IOs  { \
	DECLARE(EMU_MUX_CTRL0_GPIO, 0, 1), \
	DECLARE(EMU_MUX_CTRL1_GPIO, 0, 1), \
	DECLARE(EMU_SCI_OUT_GPIO, 0, 1), \
	DECLARE(EMU_ID_EN_GPIO, 1, 2), \
	DECLARE(SEMU_PPD_DET_GPIO, 0, 1), \
	DECLARE(SEMU_ALT_MODE_EN_GPIO, 1, 2), \
	DECLARE(EMU_ID_GPIO, 1, 1), \
	DECLARE(DMB_PPD_DET_GPIO, 1, 1), \
	DECLARE(DPLUS_GPIO, 0, 2), \
	DECLARE(DMINUS_GPIO, 0, 2), \
	DECLARE(WHISPER_UART_TX_GPIO, 0, 2), \
	DECLARE(WHISPER_UART_RX_GPIO, 0, 2), \
	DECLARE(TX_PAIR_GPIO, 0, 2), \
	DECLARE(RX_PAIR_GPIO, 0, 2), \
}

#define DECLARE(_id, _opt, _cfgs)	_id
enum mmi_emu_det_gpios EMU_DET_IOs;
#define EMU_DET_GPIO_MAX (RX_PAIR_GPIO + 1)
#undef DECLARE

void emu_det_register_notify(struct notifier_block *nb);
int emu_det_get_accy(void);

#endif  /* __MMI_EMU_DETECTION_H__ */
