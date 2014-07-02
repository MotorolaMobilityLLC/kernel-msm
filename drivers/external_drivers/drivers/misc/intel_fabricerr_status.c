/*
 * drivers/misc/intel_fabricerr_status.c
 *
 * Copyright (C) 2011 Intel Corp
 * Author: winson.w.yung@intel.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/types.h>
#include <asm/intel-mid.h>

#include "intel_fabricid_def.h"

static char *FullChip_FlagStatusLow32_pnw[] = {
	"cdmi_iocp (IA Burst Timeout)",		/* bit 0 */
	"cha_iahb (IA Burst Timeout)",		/* bit 1 */
	"nand_iaxi (IA Burst Timeout)",		/* bit 2 */
	"otg_iahb (IA Burst Timeout)",		/* bit 3 */
	"usb_iahb (IA Burst Timeout)",		/* bit 4 */
	"usc0a_iahb (IA Burst Timeout)",	/* bit 5 */
	"usc0b_iahb (IA Burst Timeout)",	/* bit 6 */
	"usc2_iahb (IA Burst Timeout)",		/* bit 7 */
	"tra0_iocp (IA Burst Timeout)",		/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"cdmi_iocp (IA Resp Timeout)",		/* bit 16 */
	"cha_iahb (IA Resp Timeout)",		/* bit 17 */
	"nand_iaxi (IA Resp Timeout)",		/* bit 18 */
	"otg_iahb (IA Resp Timeout)",		/* bit 19 */
	"usb_iahb (IA Resp Timeout)",		/* bit 20 */
	"usc0a_iahb (IA Resp Timeout)",		/* bit 21 */
	"usc0b_iahb (IA Resp Timeout)",		/* bit 22 */
	"usc2_iahb (IA Resp Timeout)",		/* bit 23 */
	"tra0_iocp (IA Resp Timeout)",		/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *FullChip_FlagStatusLow32_clv[] = {
	"cdmi_iocp (IA Burst Timeout)",		/* bit 0 */
	"cha_iahb (IA Burst Timeout)",		/* bit 1 */
	"",					/* bit 2 */
	"otg_iahb (IA Burst Timeout)",		/* bit 3 */
	"usb_iahb (IA Burst Timeout)",		/* bit 4 */
	"usc0a_iahb (IA Burst Timeout)",	/* bit 5 */
	"usc0b_iahb (IA Burst Timeout)",	/* bit 6 */
	"usc2_iahb (IA Burst Timeout)",		/* bit 7 */
	"tra0_iocp (IA Burst Timeout)",		/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"cdmi_iocp (IA Resp Timeout)",		/* bit 16 */
	"cha_iahb (IA Resp Timeout)",		/* bit 17 */
	"",					/* bit 18 */
	"otg_iahb (IA Resp Timeout)",		/* bit 19 */
	"usb_iahb (IA Resp Timeout)",		/* bit 20 */
	"usc0a_iahb (IA Resp Timeout)",		/* bit 21 */
	"usc0b_iahb (IA Resp Timeout)",		/* bit 22 */
	"usc2_iahb (IA Resp Timeout)",		/* bit 23 */
	"tra0_iocp (IA Resp Timeout)",		/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *FullChip_FlagStatusLow32_tng[] = {
	"iosf2ocp_i0 (IA Burst Timeout)",	/* bit 0 */
	"usb3_i0 (IA Burst Timeout)",		/* bit 1 */
	"usb3_i1 (IA Burst Timeout)",		/* bit 2 */
	"mfth_i0 (IA Burst Timeout)",		/* bit 3 */
	"cha_i0 (IA Burst Timeout)",		/* bit 4 */
	"otg_i0 (IA Burst Timeout)",		/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"iosf2ocp_i0 (IA Response Timeout)",	/* bit 10 */
	"usb3_i0 (IA Response Timeout)",	/* bit 11 */
	"usb3_i1 (IA Response Timeout)",	/* bit 12 */
	"mfth_i0 (IA Response Timeout)",	/* bit 13 */
	"cha_i0 (IA Response Timeout)",		/* bit 14 */
	"otg_i0 (IA Response Timeout)",		/* bit 15 */
	"",					/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"iosf2ocp_i0 (IA InBand Error)",	/* bit 20 */
	"usb3_i0 (IA InBand Error)",		/* bit 21 */
	"usb3_i1 (IA InBand Error)",		/* bit 22 */
	"mfth_i0 (IA InBand Error)",		/* bit 23 */
	"cha_i0 (IA InBand Error)",		/* bit 24 */
	"otg_i0 (IA InBand Error)",		/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"iosf2ocp_t0 (TA Request Timeout)",	/* bit 30 */
	"usb3_t0 (TA Request Timeout)"		/* bit 31 */
};

static char *FullChip_FlagStatusHi32_pnw[] = {
	"cdmi_iocp (IA Inband Error)",		/* bit 32 */
	"cha_iahb (IA Inband Error)",		/* bit 33 */
	"nand_iaxi (IA Inband Error)",		/* bit 34 */
	"otg_iahb (IA Inband Error)",		/* bit 35 */
	"usb_iahb (IA Inband Error)",		/* bit 36 */
	"usc0a_iahb (IA Inband Error)",		/* bit 37 */
	"usc0b_iahb (IA Inband Error)",		/* bit 38 */
	"usc2_iahb (IA Inband Error)",		/* bit 39 */
	"tra0_iocp (IA Inband Error)",		/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"cdmi_tocp (TA Req Timeout)",		/* bit 48 */
	"cha_tahb (TA Req Timeout)",		/* bit 49 */
	"nand_taxi (TA Req Timeout)",		/* bit 50 */
	"nandreg_taxi (TA Req Timeout)",	/* bit 51 */
	"otg_tahb (TA Req Timeout)",		/* bit 52 */
	"usb_tahb (TA Req Timeout)",		/* bit 53 */
	"usc0a_tahb (TA Req Timeout)",		/* bit 54 */
	"usc0b_tahb (TA Req Timeout)",		/* bit 55 */
	"usc2_tahb (TA Req Timeout)",		/* bit 56 */
	"pti_tocp (TA Req Timeout)",		/* bit 57 */
	"tra0_tocp (TA Req Timeout)",		/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *FullChip_FlagStatusHi32_clv[] = {
	"cdmi_iocp (IA Inband Error)",		/* bit 32 */
	"cha_iahb (IA Inband Error)",		/* bit 33 */
	"",					/* bit 34 */
	"otg_iahb (IA Inband Error)",		/* bit 35 */
	"usb_iahb (IA Inband Error)",		/* bit 36 */
	"usc0a_iahb (IA Inband Error)",		/* bit 37 */
	"usc0b_iahb (IA Inband Error)",		/* bit 38 */
	"usc2_iahb (IA Inband Error)",		/* bit 39 */
	"tra0_iocp (IA Inband Error)",		/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"cdmi_tocp (TA Req Timeout)",		/* bit 48 */
	"cha_tahb (TA Req Timeout)",		/* bit 49 */
	"",					/* bit 50 */
	"",					/* bit 51 */
	"otg_tahb (TA Req Timeout)",		/* bit 52 */
	"usb_tahb (TA Req Timeout)",		/* bit 53 */
	"usc0a_tahb (TA Req Timeout)",		/* bit 54 */
	"usc0b_tahb (TA Req Timeout)",		/* bit 55 */
	"usc2_tahb (TA Req Timeout)",		/* bit 56 */
	"pti_tocp (TA Req Timeout)",		/* bit 57 */
	"tra0_tocp (TA Req Timeout)",		/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *FullChip_FlagStatusHi32_tng[] = {
	"ptistm_t0 (TA Request Timeout)",	/* bit 32 */
	"ptistm_t1 (TA Request Timeout)",	/* bit 33 */
	"ptistm_t2 (TA Request Timeout)",	/* bit 34 */
	"mfth_t0 (TA Request Timeout)",		/* bit 35 */
	"cha_t0 (TA Request Timeout)",		/* bit 36 */
	"otg_t0 (TA Request Timeout)",		/* bit 37 */
	"runctl_t0 (TA Request Timeout)",	/* bit 38 */
	"usb3phy_t0 (TA Request Timeout)",	/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"",					/* bit 48 */
	"",					/* bit 49 */
	"iosf2ocp_t0 (Access Control Violation)",/* bit 50 */
	"usb3_t0 (Access Control Violation)",	/* bit 51 */
	"ptistm_t0 (Access Control Violation)",	/* bit 52 */
	"ptistm_t1 (Access Control Violation)",	/* bit 53 */
	"ptistm_t2 (Access Control Violation)",	/* bit 54 */
	"mfth_t0 (Access Control Violation)",	/* bit 55 */
	"cha_t0 (Access Control Violation)",	/* bit 56 */
	"otg_t0 (Access Control Violation)",	/* bit 57 */
	"runctl_t0 (Access Control Violation)",	/* bit 58 */
	"usb3phy_t0 (Access Control Violation)",/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *Secondary_FlagStatusLow32[] = {
	"usc1a_iahb (IA Burst Timeout)",	/* bit 0 */
	"usc1b_iahb (IA Burst Timeout)",	/* bit 1 */
	"hsidma_iahb (IA Burst Timeout)",	/* bit 2 */
	"tra1_iocp (IA Burst Timeout)",		/* bit 3 */
	"dfx_iahb (IA Burst Timeout)",		/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"usc1a_iahb (IA Resp Timeout)",		/* bit 16 */
	"usc1b_iahb (IA Resp Timeout)",		/* bit 17 */
	"hsidma_iahb (IA Resp Timeout)",	/* bit 18 */
	"tra1_iocp (IA Resp Timeout)",		/* bit 19 */
	"dfx_iahb (IA Resp Timeout)",		/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"",					/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *Secondary_FlagStatusLow32_tng[] = {
	"sdio0_i0 (IA Burst Timeout)",		/* bit 0 */
	"emmc01_i0 (IA Burst Timeout)",		/* bit 1 */
	"",					/* bit 2 */
	"sdio1_i0 (IA Burst Timeout)",		/* bit 3 */
	"hsi_i0 (IA Burst Timeout)",		/* bit 4 */
	"mph_i0 (IA Burst Timeout)",		/* bit 5 */
	"sfth_i0 (IA Burst Timeout)",		/* bit 6 */
	"dfxsctap_i0 (IA Burst Timeout)",	/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"sdio0_i0 (IA Response Timeout)",	/* bit 10 */
	"emmc01_i0 (IA Response Timeout)",	/* bit 11 */
	"",					/* bit 12 */
	"sdio1_i0 (IA Response Timeout)",	/* bit 13 */
	"hsi_i0 (IA Response Timeout)",		/* bit 14 */
	"mph_i0 (IA Response Timeout)",		/* bit 15 */
	"sfth_i0 (IA Response Timeout)",	/* bit 16 */
	"dfxsctap_i0 (IA Response Timeout)",	/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"sdio0_i0 (IA InBand Error)",		/* bit 20 */
	"emmc01_i0 (IA InBand Error)",		/* bit 21 */
	"",					/* bit 22 */
	"sdio1_i0 (IA InBand Error)",		/* bit 23 */
	"hsi_i0 (IA InBand Error)",		/* bit 24 */
	"mph_i0 (IA InBand Error)",		/* bit 25 */
	"sfth_i0 (IA InBand Error)",		/* bit 26 */
	"dfxsctap_i0 (IA InBand Error)",	/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"sram_t0 (TA Request Timeout)",		/* bit 30 */
	"sdio0_t0 (TA Request Timeout)"		/* bit 31 */
};

static char *Secondary_FlagStatusHi32[] = {
	"usc1a_iahb (IA Inband Error)",		/* bit 32 */
	"usc1b_iahb (IA Inband Error)",		/* bit 33 */
	"hsidma_iahb (IA Inband Error)",	/* bit 34 */
	"tra1_iocp (IA Inband Error)",		/* bit 35 */
	"dfx_iahb (IA Inband Error)",		/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"usc1a_tahb (TA Req Timeout)",		/* bit 48 */
	"usc1b_tahb (TA Req Timeout)",		/* bit 49 */
	"hsi_tocp (TA Req Timeout)",		/* bit 50 */
	"hsidma_tahb (TA Req Timeout)",		/* bit 51 */
	"sram_tocp (TA Req Timeout)",		/* bit 52 */
	"tra1_tocp (TA Req Timeout)",		/* bit 53 */
	"i2c3ssc_tocp (TA Req Timeout)",	/* bit 54 */
	"",					/* bit 55 */
	"",					/* bit 56 */
	"",					/* bit 57 */
	"",					/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *Secondary_FlagStatusHi32_tng[] = {
	"emmc01_t0 (TA Request Timeout)",	/* bit 32 */
	"",					/* bit 33 */
	"sdio1_t0 (TA Request Timeout)",	/* bit 34 */
	"hsi_t0 (TA Request Timeout)",		/* bit 35 */
	"mph_t0 (TA Request Timeout)",		/* bit 36 */
	"sfth_t0 (TA Request Timeout)",		/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"",					/* bit 48 */
	"",					/* bit 49 */
	"sram_t0 (Access Control Violation)",	/* bit 50 */
	"sdio0_t0 (Access Control Violation)",	/* bit 51 */
	"emmc01_t0 (Access Control Violation)",	/* bit 52 */
	"",					/* bit 53 */
	"sdio1_t0 (Access Control Violation)",	/* bit 54 */
	"hsi_t0 (Access Control Violation)",	/* bit 55 */
	"mph_t0 (Access Control Violation)",	/* bit 56 */
	"sfth_t0 (Access Control Violation)",	/* bit 57 */
	"",					/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *Audio_FlagStatusLow32[] = {
	"aes_iahb (IA Burst Timeout)",		/* bit 0 */
	"adma_iahb (IA Burst Timeout)",		/* bit 1 */
	"adma2_iahb (IA Burst Timeout)",	/* bit 2 */
	"",					/* bit 3 */
	"",					/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"aes_iahb (IA Resp Timeout)",		/* bit 16 */
	"adma_iahb (IA Resp Timeout)",		/* bit 17 */
	"adma2_iahb (IA Resp Timeout)",		/* bit 18 */
	"",					/* bit 19 */
	"",					/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"",					/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *Audio_FlagStatusLow32_tng[] = {
	"pifocp_i0 (IA Burst Timeout)",		/* bit 0 */
	"adma0_i0 (IA Burst Timeout)",		/* bit 1 */
	"adma0_i1 (IA Burst Timeout)",		/* bit 2 */
	"adma1_i0 (IA Burst Timeout)",		/* bit 3 */
	"adma1_i1 (IA Burst Timeout)",		/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"pifocp_i0 (IA Response Timeout)",	/* bit 10 */
	"adma0_i0 (IA Response Timeout)",	/* bit 11 */
	"adma0_i1 (IA Response Timeout)",	/* bit 12 */
	"adma1_i0 (IA Response Timeout)",	/* bit 13 */
	"adma1_i1 (IA Response Timeout)",	/* bit 14 */
	"",					/* bit 15 */
	"",					/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"pifocp_i0 (IA InBand Error)",		/* bit 20 */
	"adma0_i0 (IA InBand Error)",		/* bit 21 */
	"adma0_i1 (IA InBand Error)",		/* bit 22 */
	"adma1_i0 (IA InBand Error)",		/* bit 23 */
	"adma1_i1 (IA InBand Error)",		/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"ssp0_t0 (TA Request Timeout)",		/* bit 30 */
	"ssp1_t0 (TA Request Timeout)"		/* bit 31 */
};

static char *Audio_FlagStatusHi32_pnw[] = {
	"aes_iahb (IA Inband Error)",		/* bit 32 */
	"adma_iahb (IA Inband Error)",		/* bit 33 */
	"adma2_iahb (IA Inband Error)",		/* bit 34 */
	"",					/* bit 35 */
	"",					/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"aes_tahb (TA Req Timeout)",		/* bit 48 */
	"adma_tahb (TA Req Timeout)",		/* bit 49 */
	"adram2_tocp (TA Req Timeout)",		/* bit 50 */
	"adram_tocp (TA Req Timeout)",		/* bit 51 */
	"airam_tocp (TA Req Timeout)",		/* bit 52 */
	"assp1_1_tapb (TA Req Timeout)",	/* bit 53 */
	"assp2_2_tahb (TA Req Timeout)",	/* bit 54 */
	"adma2_tahb (TA Req Timeout)",		/* bit 55 */
	"slim0_iocp (TA Req Timeout)",		/* bit 56 */
	"slim1_iocp (TA Req Timeout)",		/* bit 57 */
	"slim2_iocp (TA Req Timeout)",		/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *Audio_FlagStatusHi32_clv[] = {
	"aes_iahb (IA Inband Error)",		/* bit 32 */
	"adma_iahb (IA Inband Error)",		/* bit 33 */
	"adma2_iahb (IA Inband Error)",		/* bit 34 */
	"",					/* bit 35 */
	"",					/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"aes_tahb (TA Req Timeout)",		/* bit 48 */
	"adma_tahb (TA Req Timeout)",		/* bit 49 */
	"adram2_tocp (TA Req Timeout)",		/* bit 50 */
	"adram_tocp (TA Req Timeout)",		/* bit 51 */
	"airam_tocp (TA Req Timeout)",		/* bit 52 */
	"assp1_1_tapb (TA Req Timeout)",	/* bit 53 */
	"assp_2_tapb (TA Req Timeout)",		/* bit 54 */
	"adma2_tahb (TA Req Timeout)",		/* bit 55 */
	"assp_3_tapb (TA Req Timeout)",		/* bit 56 */
	"",					/* bit 57 */
	"",					/* bit 58 */
	"assp_4_tapb (TA Req Timeout)",		/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *Audio_FlagStatusHi32_tng[] = {
	"ssp2_t0 (TA Request Timeout)",		/* bit 32 */
	"slim1_t0 (TA Request Timeout)",	/* bit 33 */
	"pifocp_t0 (TA Request Timeout)",	/* bit 34 */
	"adma0_t0 (TA Request Timeout)",	/* bit 35 */
	"adma1_t0 (TA Request Timeout)",	/* bit 36 */
	"mboxram_t0 (TA Request Timeout)",	/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"",					/* bit 48 */
	"",					/* bit 49 */
	"ssp0_t0 (Access Control Violation)",	/* bit 50 */
	"ssp1_t0 (Access Control Violation)",	/* bit 51 */
	"ssp2_t0 (Access Control Violation)",	/* bit 52 */
	"slim1_t0 (Access Control Violation)",	/* bit 53 */
	"pifocp_t0 (Access Control Violation)",	/* bit 54 */
	"adma0_t0 (Access Control Violation)",	/* bit 55 */
	"adma1_t0 (Access Control Violation)",	/* bit 56 */
	"mboxram_t0 (Access Control Violation)",/* bit 57 */
	"",					/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *GP_FlagStatusLow32[] = {
	"gpdma_iahb (IA Burst Timeout)",	/* bit 0 */
	"",					/* bit 1 */
	"",					/* bit 2 */
	"",					/* bit 3 */
	"",					/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"gpdma_iahb (IA Resp Timeout)",		/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"",					/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"",					/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *GP_FlagStatusLow32_tng[] = {
	"gpdma_i0 (IA Burst Timeout)",		/* bit 0 */
	"gpdma_i1 (IA Burst Timeout)",		/* bit 1 */
	"",					/* bit 2 */
	"",					/* bit 3 */
	"",					/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"gpdma_i0 (IA Response Timeout)",	/* bit 10 */
	"gpdma_i1 (IA Response Timeout)",	/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"",					/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"gpdma_i0 (IA InBand Error)",		/* bit 20 */
	"gpdma_i1 (IA InBand Error)",		/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"",					/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"spi5_t0 (TA Request Timeout)",		/* bit 30 */
	"ssp6_t0 (TA Request Timeout)"		/* bit 31 */
};

static char *GP_FlagStatusHi32[] = {
	"gpdma_iahb (IA Inband Error)",		/* bit 32 */
	"",					/* bit 33 */
	"",					/* bit 34 */
	"",					/* bit 35 */
	"",					/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"gpio1_tocp (TA Req Timeout)",		/* bit 48 */
	"i2c0_tocp (TA Req Timeout)",		/* bit 49 */
	"i2c1_tocp (TA Req Timeout)",		/* bit 50 */
	"i2c2_tocp (TA Req Timeout)",		/* bit 51 */
	"i2c3hdmi_tocp (TA Req Timeout)",	/* bit 52 */
	"i2c4_tocp (TA Req Timeout)",		/* bit 53 */
	"i2c5_tocp (TA Req Timeout)",		/* bit 54 */
	"spi1_tocp (TA Req Timeout)",		/* bit 55 */
	"spi2_tocp (TA Req Timeout)",		/* bit 56 */
	"spi3_tocp (TA Req Timeout)",		/* bit 57 */
	"gpdma_tahb (TA Req Timeout)",		/* bit 58 */
	"i2c3scc_tocp (TA Req Timeout)",	/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *GP_FlagStatusHi32_tng[] = {
	"gpdma_t0 (TA Request Timeout)",	/* bit 32 */
	"i2c12_t0 (TA Request Timeout)",	/* bit 33 */
	"i2c12_t1 (TA Request Timeout)",	/* bit 34 */
	"i2c3_t0 (TA Request Timeout)",		/* bit 35 */
	"i2c45_t0 (TA Request Timeout)",	/* bit 36 */
	"i2c45_t1 (TA Request Timeout)",	/* bit 37 */
	"i2c67_t0 (TA Request Timeout)",	/* bit 38 */
	"i2c67_t1 (TA Request Timeout)",	/* bit 39 */
	"ssp3_t0 (TA Request Timeout)",		/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"",					/* bit 48 */
	"",					/* bit 49 */
	"spi5_t0 (Access Control Violation)",	/* bit 50 */
	"ssp6_t0 (Access Control Violation)",	/* bit 51 */
	"gpdma_t0 (Access Control Violation)",	/* bit 52 */
	"i2c12_t0 (Access Control Violation)",	/* bit 53 */
	"i2c12_t1 (Access Control Violation)",	/* bit 54 */
	"i2c3_t0 (Access Control Violation)",	/* bit 55 */
	"i2c45_t0 (Access Control Violation)",	/* bit 56 */
	"i2c45_t1 Access Control Violation)",	/* bit 57 */
	"i2c67_t0 (Access Control Violation)",	/* bit 58 */
	"i2c67_t1 (Access Control Violation)",	/* bit 59 */
	"ssp3_t0 (Access Control Violation)",	/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *SC_FlagStatusLow32_pnw[] = {
	"MFlag0 (Audio)",			/* bit 0 */
	"MFlag1 (Secondary)",			/* bit 1 */
	"MFlag2 (FullChip)",			/* bit 2 */
	"MFlag3 (GP)",				/* bit 3 */
	"",					/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"arc_iocp (IA Burst Timeout)",		/* bit 8 */
	"scdma_iocp (IA Burst Timeout)",	/* bit 9 */
	"uart_iocp (IA Burst Timeout)",		/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"arc_iocp (IA Resp Timeout)",		/* bit 16 */
	"scdma_iocp (IA Resp Timeout)",		/* bit 17 */
	"uart_iocp (IA Resp Timeout)",		/* bit 18 */
	"",					/* bit 19 */
	"",					/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"arc_iocp (IA Inband Error)",		/* bit 24 */
	"scdma_iocp (IA Inband Error)",		/* bit 25 */
	"uart_iocp (IA Inband Error)",		/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *SC_FlagStatusLow32_clv[] = {
	"MFlag0 (Audio)",			/* bit 0 */
	"MFlag1 (Secondary)",			/* bit 1 */
	"MFlag2 (FullChip)",			/* bit 2 */
	"MFlag3 (GP)",				/* bit 3 */
	"",					/* bit 4 */
	"",					/* bit 5 */
	"",					/* bit 6 */
	"ilb_iocp (IA Burst Timeout)",		/* bit 7 */
	"arc_iocp (IA Burst Timeout)",		/* bit 8 */
	"scdma_iocp (IA Burst Timeout)",	/* bit 9 */
	"uart_iocp (IA Burst Timeout)",		/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"arc_iocp (IA Resp Timeout)",		/* bit 16 */
	"scdma_iocp (IA Resp Timeout)",		/* bit 17 */
	"uart_iocp (IA Resp Timeout)",		/* bit 18 */
	"ilb_iocp (IA Resp Timeout)",		/* bit 19 */
	"",					/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"arc_iocp (IA Inband Error)",		/* bit 24 */
	"scdma_iocp (IA Inband Error)",		/* bit 25 */
	"uart_iocp (IA Inband Error)",		/* bit 26 */
	"ilb_iocp (IA Inband Error)",		/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

static char *SC_FlagStatusLow32_tng[] = {
	"ADF Flag Status",			/* bit 0 */
	"SDF Flag Status",			/* bit 1 */
	"MNF Flag Status",			/* bit 2 */
	"GPF Flag Status",			/* bit 3 */
	"ilb_i0 (IA Burst Timeout)",		/* bit 4 */
	"scdma_i0 (IA Burst Timeout)",		/* bit 5 */
	"scdma_i1 (IA Burst Timeout)",		/* bit 6 */
	"arc_i0 (IA Burst Timeout)",		/* bit 7 */
	"uart_i0 (IA Burst Timeout)",		/* bit 8 */
	"psh_i0 (IA Burst Timeout)",		/* bit 9 */
	"ilb_i0 (IA Response Timeout)",		/* bit 10 */
	"scdma_i0 (IA Response Timeout)",	/* bit 11 */
	"scdma_i1 (IA Response Timeout)",	/* bit 12 */
	"arc_i0 (IA Response Timeout)",		/* bit 13 */
	"uart_i0 (IA Response Timeout)",	/* bit 14 */
	"psh_i0 (IA Response Timeout)",		/* bit 15 */
	"",					/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"ilb_i0 (IA InBand Error)",		/* bit 20 */
	"scdma_i0 (IA InBand Error)",		/* bit 21 */
	"scdma_i1 (IA InBand Error)",		/* bit 22 */
	"arc_i0 (IA InBand Error)",		/* bit 23 */
	"uart_i0 (IA InBand Error)",		/* bit 24 */
	"psh_i0 (IA InBand Error)",		/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"ilb_t0 (TA Request Timeout)",		/* bit 30 */
	"ipc1_t0 (TA Request Timeout)"		/* bit 31 */
};

static char *SC_FlagStatusHi32_pnw[] = {
	"",					/* bit 32 */
	"",					/* bit 33 */
	"",					/* bit 34 */
	"",					/* bit 35 */
	"",					/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"gpio_tocp (TA Req Timeout)",		/* bit 48 */
	"uart_tocp (TA Req Timeout)",		/* bit 49 */
	"ipc1_tocp (TA Req Timeout)",		/* bit 50 */
	"ipc2_tocp (TA Req Timeout)",		/* bit 51 */
	"kbd_tocp (TA Req Timeout)",		/* bit 52 */
	"pmu_tocp (TA Req Timeout)",		/* bit 53 */
	"scdma_tocp (TA Req Timeout)",		/* bit 54 */
	"spi0_tocp (TA Req Timeout)",		/* bit 55 */
	"tim_ocp (TA Req Timeout)",		/* bit 56 */
	"vrtc_tocp (TA Req Timeout)",		/* bit 57 */
	"arcs_tocp (TA Req Timeout)",		/* bit 58 */
	"",					/* bit 59 */
	"",					/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *SC_FlagStatusHi32_clv[] = {
	"",					/* bit 32 */
	"",					/* bit 33 */
	"",					/* bit 34 */
	"",					/* bit 35 */
	"",					/* bit 36 */
	"",					/* bit 37 */
	"",					/* bit 38 */
	"",					/* bit 39 */
	"",					/* bit 40 */
	"",					/* bit 41 */
	"",					/* bit 42 */
	"",					/* bit 43 */
	"",					/* bit 44 */
	"",					/* bit 45 */
	"",					/* bit 46 */
	"",					/* bit 47 */
	"gpio_tocp (TA Req Timeout)",		/* bit 48 */
	"uart_tocp (TA Req Timeout)",		/* bit 49 */
	"ipc1_tocp (TA Req Timeout)",		/* bit 50 */
	"ipc2_tocp (TA Req Timeout)",		/* bit 51 */
	"kbd_tocp (TA Req Timeout)",		/* bit 52 */
	"pmu_tocp (TA Req Timeout)",		/* bit 53 */
	"scdma_tocp (TA Req Timeout)",		/* bit 54 */
	"spi0_tocp (TA Req Timeout)",		/* bit 55 */
	"tim_ocp (TA Req Timeout)",		/* bit 56 */
	"vrtc_tocp (TA Req Timeout)",		/* bit 57 */
	"arcs_tocp (TA Req Timeout)",		/* bit 58 */
	"ilb_tocp (TA Req Timeout)",		/* bit 59 */
	"ilbmb0_tocp (TA Req Timeout)",		/* bit 60 */
	"",					/* bit 61 */
	"",					/* bit 62 */
	""					/* bit 63 */
};

static char *SC_FlagStatusHi32_tng[] = {
	"ipc2_t0 (TA Request Timeout)",		/* bit 32 */
	"mbb_t0 (TA Request Timeout)",		/* bit 33 */
	"spi4_t0 (TA Request Timeout)",		/* bit 34 */
	"scdma_t0 (TA Request Timeout)",	/* bit 35 */
	"kbd_t0 (TA Request Timeout)",		/* bit 36 */
	"sccb_t0 (TA Request Timeout)",		/* bit 37 */
	"timers_t0 (TA Request Timeout)",	/* bit 38 */
	"pmu_t0 (TA Request Timeout)",		/* bit 39 */
	"arc_t0 (TA Request Timeout)",		/* bit 40 */
	"gpio192_t0 (TA Request Timeout)",	/* bit 41 */
	"i2c0_t0 (TA Request Timeout)",		/* bit 42 */
	"uart_t0 (TA Request Timeout)",		/* bit 43 */
	"ssc_t0 (TA Request Timeout)",		/* bit 44 */
	"pwm_t0 (TA Request Timeout)",		/* bit 45 */
	"psh_t0 (TA Request Timeout)",		/* bit 46 */
	"pcache_t0 (TA Request Timeout)",	/* bit 47 */
	"i2c89_t0 (TA Request Timeout)",	/* bit 48 */
	"i2c89_t1 (TA Request Timeout)",	/* bit 49 */
	"ilb_t0 (Access Control Violation)",	/* bit 50 */
	"ipc1_t0 (Access Control Violation)",	/* bit 51 */
	"ipc2_t0 (Access Control Violation)",	/* bit 52 */
	"spi4_t0 (Access Control Violation)",	/* bit 53 */
	"sccb_t0 (Access Control Violation)",	/* bit 54 */
	"timers_t0 (Access Control Violation)",	/* bit 55 */
	"pmu_t0 (Access Control Violation)",	/* bit 56 */
	"arc_t0 (Access Control Violation)",	/* bit 57 */
	"gpio192_t0 (Access Control Violation)",/* bit 58 */
	"i2c0_t0 (Access Control Violation)",	/* bit 59 */
	"ssc_t0 (Access Control Violation)",	/* bit 60 */
	"pcache_t0 (Access Control Violation)",	/* bit 61 */
	"i2c89_t0 (Access Control Violation)",	/* bit 62 */
	"i2c89_t1 (Access Control Violation)"	/* bit 63 */
};

static char *SC_FlagStatus1Low32_tng[] = {
	"mbb_t0 (Access Control Violation)",	/* bit 0 */
	"scdma_t0 (Access Control Violation)",	/* bit 1 */
	"kbd_t0 (Access Control Violation)",	/* bit 2 */
	"uart_t0 (Access Control Violation)",	/* bit 3 */
	"pwm_t0 (Access Control Violation)",	/* bit 4 */
	"psh_t0 (Access Control Violation)",	/* bit 5 */
	"",					/* bit 6 */
	"",					/* bit 7 */
	"",					/* bit 8 */
	"",					/* bit 9 */
	"",					/* bit 10 */
	"",					/* bit 11 */
	"",					/* bit 12 */
	"",					/* bit 13 */
	"",					/* bit 14 */
	"",					/* bit 15 */
	"",					/* bit 16 */
	"",					/* bit 17 */
	"",					/* bit 18 */
	"",					/* bit 19 */
	"",					/* bit 20 */
	"",					/* bit 21 */
	"",					/* bit 22 */
	"",					/* bit 23 */
	"",					/* bit 24 */
	"",					/* bit 25 */
	"",					/* bit 26 */
	"",					/* bit 27 */
	"",					/* bit 28 */
	"",					/* bit 29 */
	"",					/* bit 30 */
	""					/* bit 31 */
};

char *fabric_error_lookup(u32 fab_id, u32 error_index, int use_hidword)
{
	if (error_index > 31) /* Out of range of 32bit */
		return NULL;

	switch (fab_id) {
	case FAB_ID_FULLCHIP:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_PENWELL:
			return use_hidword ?
				FullChip_FlagStatusHi32_pnw[error_index] :
				FullChip_FlagStatusLow32_pnw[error_index];
		case INTEL_MID_CPU_CHIP_CLOVERVIEW:
			return use_hidword ?
				FullChip_FlagStatusHi32_clv[error_index] :
				FullChip_FlagStatusLow32_clv[error_index];
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				FullChip_FlagStatusHi32_tng[error_index] :
				FullChip_FlagStatusLow32_tng[error_index];
		default:
			return NULL;
		}

	case FAB_ID_SECONDARY:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_PENWELL:
		case INTEL_MID_CPU_CHIP_CLOVERVIEW:
			return use_hidword ?
				Secondary_FlagStatusHi32[error_index] :
				Secondary_FlagStatusLow32[error_index];
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				Secondary_FlagStatusHi32_tng[error_index] :
				Secondary_FlagStatusLow32_tng[error_index];
		default:
			return NULL;
		}

	case FAB_ID_AUDIO:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_PENWELL:
			return use_hidword ?
				Audio_FlagStatusHi32_pnw[error_index] :
				Audio_FlagStatusLow32[error_index];
		case INTEL_MID_CPU_CHIP_CLOVERVIEW:
			return use_hidword ?
				Audio_FlagStatusHi32_clv[error_index] :
				Audio_FlagStatusLow32[error_index];
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				Audio_FlagStatusHi32_tng[error_index] :
				Audio_FlagStatusLow32_tng[error_index];
		default:
			return NULL;
		}

	case FAB_ID_GP:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_PENWELL:
		case INTEL_MID_CPU_CHIP_CLOVERVIEW:
			return use_hidword ?
				GP_FlagStatusHi32[error_index] :
				GP_FlagStatusLow32[error_index];
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				GP_FlagStatusHi32_tng[error_index] :
				GP_FlagStatusLow32_tng[error_index];
		default:
			return NULL;
		}

	case FAB_ID_SC:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_PENWELL:
			return use_hidword ?
				SC_FlagStatusHi32_pnw[error_index] :
				SC_FlagStatusLow32_pnw[error_index];
		case INTEL_MID_CPU_CHIP_CLOVERVIEW:
			return use_hidword ?
				SC_FlagStatusHi32_clv[error_index] :
				SC_FlagStatusLow32_clv[error_index];
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				SC_FlagStatusHi32_tng[error_index] :
				SC_FlagStatusLow32_tng[error_index];
		default:
			return NULL;
		}

	case FAB_ID_SC1:
		switch (intel_mid_identify_cpu()) {
		case INTEL_MID_CPU_CHIP_TANGIER:
		case INTEL_MID_CPU_CHIP_ANNIEDALE:
			return use_hidword ?
				NULL :
				SC_FlagStatus1Low32_tng[error_index];
		default:
			return NULL;
		}

	default:
		return NULL;
	}

	return NULL;
}

static char *ScuBoot_ErrorTypes[] = {
	"Unknown",
	"Memory error",
	"Instruction error",
	"Fabric error",
	"Shared SRAM ECC error",
	"Unknown",
	"North Fuses failure",
	"Unknown",
	"Unknown",
	"Unknown",
	"Kernel DLT expired",
	"Kernel WDT expired",
	"SCU CHAABI watchdog expired",
	"FabricError xml request reset",
};

static char *ScuRuntime_ErrorTypes[] = {
	"PLL Lock Slip",
	"Unknown",
	"Undefined L1 Interrupt"
	"Punit Interrupt MBB Timeout_reset",
	"Volt attack violation reset",
	"Volt attack/SAI violation reset",
	"LPE unknown interrupt",
	"PSH Unknown interrupt",
	"Fuse unknown interrupt",
	"IPC2 unsupported error",
	"Invalid KWDT IPC"
};

static char *ScuFabric_ErrorTypes[] = {
	"Unknown",				/* 00 */
	"Unknown",				/* 01 */
	"Unknown",				/* 02 */
	"Unknown",				/* 03 */
	"Unknown",				/* 04 */
	"Unknown",				/* 05 */
	"Punit force reset",			/* 06 */
	"Unknown",				/* 07 */
	"Unknown",				/* 08 */
	"Unknown",				/* 09 */
	"Unsupported command error",		/* 0A */
	"Address hole error",			/* 0B */
	"Protection error",			/* 0C */
	"Memory error assertion detected",	/* 0D */
	"Request Timeout, Not acccepted",	/* 0E */
	"Request Timeout, No response",		/* 0F */
	"Request Timeout, Data not accepted",	/* 10 */
};

#define BEGIN_MAIN_FABRIC_REGID		16
#define SC_FABRIC_ARC_I0_REGID		18
#define SC_FABRIC_PSH_I0_REGID		17
#define END_MAIN_FABRIC_REGID		24
#define BEGIN_SC_FABRIC_REGID		25
#define END_SC_FABRIC_REGID		29
#define BEGIN_GP_FABRIC_REGID		30
#define END_GP_FABRIC_REGID		32
#define BEGIN_AUDIO_FABRIC_REGID	33
#define END_AUDIO_FABRIC_REGID		38
#define BEGIN_SEC_FABRIC_REGID		39
#define END_SEC_FABRIC_REGID		48
#define BEGIN_PM_MAIN_FABRIC_REGID	49
#define END_PM_MAIN_FABRIC_REGID	59
#define BEGIN_PM_AUDIO_FABRIC_REGID	60
#define END_PM_AUDIO_FABRIC_REGID	68
#define BEGIN_PM_SEC_FABRIC_REGID	69
#define END_PM_SEC_FABRIC_REGID		76
#define BEGIN_PM_SC_FABRIC_REGID	77
#define END_PM_SC_FABRIC_REGID		97
#define BEGIN_PM_GP_FABRIC_REGID	98
#define END_PM_GP_FABRIC_REGID		109
#define END_FABRIC_REGID		110

static char *FabricFlagStatusErrLogDetail_tng[] = {
	"Main Fabric Flag Status",
	"Audio Fabric Flag Status",
	"Secondary Fabric Flag Status",
	"GP Fabric Flag Status",
	"Lower 64bit part SC Fabric Flag Status",
	"Upper 64bit part SC Fabric Flag Status",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"IA ERROR LOG Register for the initiator iosf2ocp_i0 in Main Fabric @200MHz{mnf}",	/*16*/
	"IA ERROR LOG Register for the initiator psh_i0 in SC Fabric @100MHz{scf}",		/*17*/
	"IA ERROR LOG Register for the initiator arc_i0 in SC Fabric @100MHz{scf}",		/*18*/
	"IA ERROR LOG Register for the initiator usb3_i0 in Main Fabric @200MHz{mnf}",		/*19*/
	"IA ERROR LOG Register for the initiator usb3_i1 in Main Fabric @200MHz{mnf}",		/*20*/
	"IA ERROR LOG Register for the initiator mfth_i0 in Main Fabric @200MHz{mnf}",		/*21*/
	"IA ERROR LOG Register for the initiator cha_i0 in Main Fabric @200MHz{mnf}",		/*22*/
	"IA ERROR LOG Register for the initiator otg_i0 in Main Fabric @200MHz{mnf}",		/*23*/
	"IA ERROR LOG Register for the initiator sdf2mnf_i0 in Main Fabric @200MHz{mnf}",	/*24*/
	"IA ERROR LOG Register for the initiator sdf2scf_i0 in SC Fabric @100MHz{scf}",		/*25*/
	"IA ERROR LOG Register for the initiator ilb_i0 in SC Fabric @100MHz{scf}",		/*26*/
	"IA ERROR LOG Register for the initiator scdma_i0 in SC Fabric @100MHz{scf}",		/*27*/
	"IA ERROR LOG Register for the initiator scdma_i1 in SC Fabric @100MHz{scf}",		/*28*/
	"IA ERROR LOG Register for the initiator uart_i0 in SC Fabric @100MHz{scf}",		/*29*/
	"IA ERROR LOG Register for the initiator gpdma_i0 in GP Fabric @100MHz{gpf}",		/*30*/
	"IA ERROR LOG Register for the initiator gpdma_i1 in GP Fabric @100MHz{gpf}",		/*31*/
	"IA ERROR LOG Register for the initiator sdf2gpf_i0 in GP Fabric @100MHz{gpf}",		/*32*/
	"IA ERROR LOG Register for the initiator pifocp_i0 in Audio Fabric @50MHz{adf}",	/*33*/
	"IA ERROR LOG Register for the initiator adma0_i0 in Audio Fabric @50MHz{adf}",		/*34*/
	"IA ERROR LOG Register for the initiator adma0_i1 in Audio Fabric @50MHz{adf}",		/*35*/
	"IA ERROR LOG Register for the initiator adma1_i0 in Audio Fabric @50MHz{adf}",		/*36*/
	"IA ERROR LOG Register for the initiator adma1_i1 in Audio Fabric @50MHz{adf}",		/*37*/
	"IA ERROR LOG Register for the initiator sdf2adf_i0 in Audio Fabric @50MHz{adf}",	/*38*/
	"IA ERROR LOG Register for the initiator sdio0_i0 in Secondary Fabric @100MHz{sdf}",	/*39*/
	"IA ERROR LOG Register for the initiator emmc01_i0 in Secondary Fabric @100MHz{sdf}",	/*40*/
	"IA ERROR LOG Register for the initiator sdio1_i0 in Secondary Fabric @100MHz{sdf}",	/*41*/
	"IA ERROR LOG Register for the initiator hsi_i0 in Secondary Fabric @100MHz{sdf}",	/*42*/
	"IA ERROR LOG Register for the initiator mph_i0 in Secondary Fabric @100MHz{sdf}",	/*43*/
	"IA ERROR LOG Register for the initiator sfth_i0 in Secondary Fabric @100MHz{sdf}",	/*44*/
	"IA ERROR LOG Register for the initiator mnf2sdf_i0 in Secondary Fabric @100MHz{sdf}",	/*45*/
	"IA ERROR LOG Register for the initiator gpf2sdf_i0 in Secondary Fabric @100MHz{sdf}",	/*46*/
	"IA ERROR LOG Register for the initiator scf2sdf_i0 in Secondary Fabric @100MHz{sdf}",	/*47*/
	"IA ERROR LOG Register for the initiator adf2sdf_i0 in Secondary Fabric @100MHz{sdf}",	/*48*/
	"PM ERROR LOG register mnf_rt in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG register iosf2ocp_t0 in Main Fabric @200MHz{mnf}",			/*50*/
	"PM ERROR LOG Register usb3_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register ptistm_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register ptistm_t0_regon0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register ptistm_t2 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register mfth_t0 in Main Fabric @200MHz{mnf}",				/*55*/
	"PM ERROR LOG Register cha_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register otg_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register runctl_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register UBS3PHY_t0 in Main Fabric @200MHz{mnf}",
	"PM ERROR LOG Register adf_rt in Audio Fabric @50MHz{adf}",				/*60*/
	"PM ERROR LOG Register ssp0_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register ssp1_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register ssp2_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register slim1_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register pifocp_t0 in Audio Fabric @50MHz{adf}",				/*65*/
	"PM ERROR LOG Register adma0_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register adma1_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register mboxram_t0 in Audio Fabric @50MHz{adf}",
	"PM ERROR LOG Register sdf_rt in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register sram_t0 in Secondary Fabric @100MHz{adf}",			/*70*/
	"PM ERROR LOG Register sdio0_t0 in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register emmc01_t1 in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register sdio1_t0 in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register hsi_t0 in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register mph_t0 in Secondary Fabric @100MHz{adf}",			/*75*/
	"PM ERROR LOG Register sfth_t0 in Secondary Fabric @100MHz{adf}",
	"PM ERROR LOG Register scf_rt in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register ilb_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register ipc1_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register ipc2_t0 in SC Fabric @100MHz{adf}",				/*80*/
	"PM ERROR LOG Register mbb_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register spi4_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register scdma_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register kbd_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register sccb_t0 in SC Fabric @100MHz{adf}",				/*85*/
	"PM ERROR LOG Register timers_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register pmu_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register arc_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register gpio192_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c0_t0 in SC Fabric @100MHz{adf}",				/*90*/
	"PM ERROR LOG Register uart_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register ssc_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register pwm_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register psh_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register pcache_t0 in SC Fabric @100MHz{adf}",				/*95*/
	"PM ERROR LOG Register ii2c89_t0 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register ii2c89_t1 in SC Fabric @100MHz{adf}",
	"PM ERROR LOG Register gpf_rt in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register spi5_t0 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register ssp6_t0 in GP Fabric @100MHz{adf}",				/*100*/
	"PM ERROR LOG Register gpdma_t0 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c12_t0 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c12_t1 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c3_t0 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c45_t0 in GP Fabric @100MHz{adf}",				/*105*/
	"PM ERROR LOG Register i2c45_t1 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register i2c67_t0 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register ic267_t1 in GP Fabric @100MHz{adf}",
	"PM ERROR LOG Register ssp3_t0 in GP Fabric @100MHz{adf}",				/*109*/
	""											/*110*/
};

#define CLV_BEGIN_MAIN_FABRIC_REGID		16
#define CLV_END_MAIN_FABRIC_REGID		35
#define CLV_BEGIN_SC_FABRIC_REGID		36
#define CLV_END_SC_FABRIC_REGID			57
#define CLV_BEGIN_GP_FABRIC_REGID		58
#define CLV_END_GP_FABRIC_REGID			70
#define CLV_BEGIN_AUDIO_FABRIC_REGID		71
#define CLV_END_AUDIO_FABRIC_REGID		83
#define CLV_BEGIN_SEC_FABRIC_REGID		84
#define CLV_END_SEC_FABRIC_REGID		99
#define CLV_END_FABRIC_REGID			100

#define CLV_SC_FABRIC_PSH_T0_REGID		18
#define CLV_SC_FABRIC_PSH_I0_REGID		19
#define CLV_SC_FABRIC_ARC_T0_REGID		20
#define CLV_SC_FABRIC_ARC_I0_REGID		21

static char *FabricFlagStatusErrLogDetail_pnw_clv[] = {
	"Main Fabric Flag Status",
	"Audio Fabric Flag Status",
	"Secondary Fabric Flag Status",
	"GP Fabric Flag Status",
	"Lower 64bit part SC Fabric Flag Status",
	"Upper 64bit part SC Fabric Flag Status",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"TA ERROR LOG register for initiator iosf2ocp_t0 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator iosf2ocp_i0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator psh_t0 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator psh_i0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator arc_t0 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator arc_i0 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator usb3_i0 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator usb3_i1 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator mfth_i0 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator cha_i0 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator otg_i0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator usb3_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator ptistm_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator ptistm_t1 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator ptistm_t2 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator mfth_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator cha_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator otg_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator runctl_t0 in Main Fabric @200MHz{mnf}",
	"TA ERROR LOG register for initiator usb3phy_t0 in Main Fabric @200MHz{mnf}",
	"IA ERROR LOG Register for initiator ilb_i0 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator scdma_i0 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator scdma_i1 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator uart_i0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator ilb_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator ipc1_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator ipc2_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator mbb_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator spi4_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator scdma_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator kbd_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator sccb_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator timers_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator pmu_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator gpio192_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator i2c0_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator uart_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator ssc_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator pwm_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator pcache_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator i2c89_t0 in SC Fabric @100MHz{scf}",
	"TA ERROR LOG register for initiator i2c89_t1 in SC Fabric @100MHz{scf}",
	"IA ERROR LOG Register for initiator gpdma_i0 in GP Fabric @100MHz{gpf}",
	"IA ERROR LOG Register for initiator gpdma_i1 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator spi5_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator ssp6_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator gpdma_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c12_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c12_t1 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c3_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c45_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c45_t1 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c67_t0 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator i2c67_t1 in GP Fabric @100MHz{gpf}",
	"TA ERROR LOG register for initiator ssp3_t0 in GP Fabric @100MHz{gpf}",
	"IA ERROR LOG Register for initiator pifocp_i0 in Audio Fabric @50MHz{adf}",
	"IA ERROR LOG Register for initiator adma0_i0 in Audio Fabric @50MHz{adf}",
	"IA ERROR LOG Register for initiator adma0_i1 in Audio Fabric @50MHz{adf}",
	"IA ERROR LOG Register for initiator adma1_i0 in Audio Fabric @50MHz{adf}",
	"IA ERROR LOG Register for initiator adma1_i1 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator ssp0_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator ssp1_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator ssp2_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator slim1_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator pifocp_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator adma0_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator adma1_t0 in Audio Fabric @50MHz{adf}",
	"TA ERROR LOG register for initiator mboxram_t0 in Audio Fabric @50MHz{adf}",
	"IA ERROR LOG Register for initiator sdio0_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator emmc01_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator emmc01_i1 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator sdio1_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator hsi_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator mph_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator sfth_i0 in Secondary Fabric @100MHz{sdf}",
	"IA ERROR LOG Register for initiator dfxsctap_i0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator sram_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator sdio0_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator emmc01_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator emmc01_t1 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator sdio1_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator hsi_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator mph_t0 in Secondary Fabric @100MHz{sdf}",
	"TA ERROR LOG register for initiator sfth_t0 in Secondary Fabric @100MHz{sdf}",
	"",
};

#define MAX_FULLCHIP_INITID_VAL		16
#define MAX_SECONDARY_INITID_VAL	18
#define MAX_AUDIO_INITID_VAL		5
#define MAX_SC_INITID_VAL			9
#define MAX_GP_INITID_VAL			2

static char *init_id_str_fullchip_tng[] = {
	"iosf2ocp_i0 (thread 0)",
	"iosf2ocp_i0 (thread 1)",
	"iosf2ocp_i0 (thread 2)",
	"iosf2ocp_i0 (thread 3)",
	"sdf2mnf_i0 (thread 0)",
	"sdf2mnf_i0 (thread 1)",
	"sdf2mnf_i0 (thread 2)",
	"sdf2mnf_i0 (thread 3)",
	"sdf2mnf_i0 (thread 4)",
	"sdf2mnf_i0 (thread 5)",
	"sdf2mnf_i0 (thread 6)",
	"sdf2mnf_i0 (thread 7)",
	"usb3_i0",
	"usb3_i1",
	"mfth_i0",
	"cha_i0",
	"otg_i0"
};

static char *init_id_str_secondary_tng[] = {
	"mnf2sdf_i0 (thread 0)",
	"mnf2sdf_i0 (thread 1)",
	"sdio0_i0",
	"gpf2sdf_i0 (thread 0)",
	"gpf2sdf_i0 (thread 1)",
	"emmc01_i0",
	"scf2sdf_i0 (thread 0)",
	"scf2sdf_i0 (thread 1)",
	"scf2sdf_i0 (thread 2)",
	"scf2sdf_i0 (thread 3)",
	"sdio1_i0",
	"adf2sdf_i0 (thread 0)",
	"adf2sdf_i0 (thread 1)",
	"adf2sdf_i0 (thread 2)",
	"hsi_i0 (thread 0)",
	"hsi_i0 (thread 1)",
	"mph_i0",
	"sfth_i0",
	"dfxsctap_i0"
};

static char *init_id_str_audio_tng[] = {
	"sdfadf_i0",
	"pif2ocp_i0",
	"adma0_i0",
	"adma0_i1",
	"adma1_i0",
	"adma1_i1"
};

static char *init_id_str_sc_tng[] = {
	"ilb_i0",
	"sdf2scf_i0 (thread 0)",
	"sdf2scf_i0 (thread 1)",
	"scdma_i0",
	"scdma_i1",
	"arc_i0 (thread 0)",
	"arc_i0 (thread 1)",
	"arc_i0 (thread 2)",
	"uart_i0",
	"psh_i0"
};

static char *init_id_str_gp_tng[] = {
	"sd2gpf_i0",
	"gpdma_i0",
	"gpdma_i1"
};

static char *init_id_str_unknown = "unknown";

char *get_element_flagsts_detail(u8 id)
{
	if ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_PENWELL) ||
		(intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW)) {
		if (id < CLV_END_FABRIC_REGID)
			return FabricFlagStatusErrLogDetail_pnw_clv[id];
		else
			return FabricFlagStatusErrLogDetail_pnw_clv[CLV_END_FABRIC_REGID];
	}
	if (id < END_FABRIC_REGID)
		return FabricFlagStatusErrLogDetail_tng[id];

	return FabricFlagStatusErrLogDetail_tng[END_FABRIC_REGID];
}

char *get_element_errorlog_detail_pnw_clv(u8 id, u32 *fabric_type)
{
	if (id >= CLV_BEGIN_MAIN_FABRIC_REGID && id <= CLV_END_MAIN_FABRIC_REGID &&
		id != CLV_SC_FABRIC_PSH_T0_REGID && id != CLV_SC_FABRIC_PSH_I0_REGID &&
		id != CLV_SC_FABRIC_ARC_T0_REGID && id != CLV_SC_FABRIC_ARC_I0_REGID)
		*fabric_type = FAB_ID_FULLCHIP;

	else if (id >= CLV_BEGIN_SEC_FABRIC_REGID &&
		 id <= CLV_END_SEC_FABRIC_REGID)
		*fabric_type = FAB_ID_SECONDARY;

	else if (id >= CLV_BEGIN_AUDIO_FABRIC_REGID &&
		 id <= CLV_END_AUDIO_FABRIC_REGID)
		*fabric_type = FAB_ID_AUDIO;

	else if (id >= CLV_BEGIN_GP_FABRIC_REGID &&
			id <= CLV_END_GP_FABRIC_REGID)
		*fabric_type = FAB_ID_GP;

	else if ((id >= CLV_BEGIN_SC_FABRIC_REGID &&
			id <= CLV_END_SC_FABRIC_REGID) ||
			id == CLV_SC_FABRIC_PSH_T0_REGID ||
			id == CLV_SC_FABRIC_PSH_I0_REGID ||
			id == CLV_SC_FABRIC_ARC_T0_REGID ||
			id == CLV_SC_FABRIC_ARC_I0_REGID)
		*fabric_type = FAB_ID_SC;
	else
		*fabric_type = FAB_ID_UNKNOWN;
	return get_element_flagsts_detail(id);
}

char *get_element_errorlog_detail_tng(u8 id, u32 *fabric_type)
{
	if (id >= BEGIN_MAIN_FABRIC_REGID && id <= END_MAIN_FABRIC_REGID &&
		id != SC_FABRIC_PSH_I0_REGID && id != SC_FABRIC_ARC_I0_REGID)
		*fabric_type = FAB_ID_FULLCHIP;

	else if (id >= BEGIN_SEC_FABRIC_REGID &&
		 id <= END_SEC_FABRIC_REGID)
		*fabric_type = FAB_ID_SECONDARY;

	else if (id >= BEGIN_AUDIO_FABRIC_REGID &&
		 id <= END_AUDIO_FABRIC_REGID)
		*fabric_type = FAB_ID_AUDIO;

	else if (id >= BEGIN_GP_FABRIC_REGID &&
			id <= END_GP_FABRIC_REGID)
		*fabric_type = FAB_ID_GP;

	else if ((id >= BEGIN_SC_FABRIC_REGID &&
			id <= END_SC_FABRIC_REGID) ||
			id == SC_FABRIC_PSH_I0_REGID ||
			id == SC_FABRIC_ARC_I0_REGID)
		*fabric_type = FAB_ID_SC;

	else if (id >= BEGIN_PM_MAIN_FABRIC_REGID &&
		 id <= END_PM_MAIN_FABRIC_REGID)
		*fabric_type = FAB_ID_PM_FULLCHIP;

	else if (id >= BEGIN_PM_AUDIO_FABRIC_REGID &&
		 id <= END_PM_AUDIO_FABRIC_REGID)
		*fabric_type = FAB_ID_PM_AUDIO;

	else if (id >= BEGIN_PM_SEC_FABRIC_REGID &&
		 id <= END_PM_SEC_FABRIC_REGID)
		*fabric_type = FAB_ID_PM_SECONDARY;

	else if (id >= BEGIN_PM_SC_FABRIC_REGID &&
		 id <= END_PM_SC_FABRIC_REGID)
		*fabric_type = FAB_ID_PM_SC;

	else if (id >= BEGIN_PM_GP_FABRIC_REGID &&
		 id <= END_PM_GP_FABRIC_REGID)
		*fabric_type = FAB_ID_PM_GP;

	else
		*fabric_type = FAB_ID_UNKNOWN;

	return get_element_flagsts_detail(id);
}

char *get_element_errorlog_detail(u8 id, u32 *fabric_type)
{
	if ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_PENWELL) ||
		(intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW))
			return get_element_errorlog_detail_pnw_clv(id, fabric_type);
	return get_element_errorlog_detail_tng(id, fabric_type);
}

char *get_errortype_str(u16 error_type)
{
	u16 error = error_type & 0xFF;

	if (!error_type)
		return "Not set";

	switch(error_type & 0xFF00) {

	case 0xE100 :

		if (error < ARRAY_SIZE(ScuBoot_ErrorTypes))
			return ScuBoot_ErrorTypes[error];
		return "Unknown";

	case 0xE600 :

		if (error < ARRAY_SIZE(ScuRuntime_ErrorTypes))
			return ScuRuntime_ErrorTypes[error];
		return "Unknown";

	case 0xF000 :

		if (error < ARRAY_SIZE(ScuFabric_ErrorTypes))
			return ScuFabric_ErrorTypes[error];
		return "Unknown";

	default :

		return "Unknown";
	}
}

char *get_initiator_id_str(int init_id, u32 fabric_id)
{
	switch (fabric_id) {

	case FAB_ID_PM_FULLCHIP:
	case FAB_ID_FULLCHIP:

		if (init_id > MAX_FULLCHIP_INITID_VAL)
			return init_id_str_unknown;

		return init_id_str_fullchip_tng[init_id];

	case FAB_ID_PM_AUDIO:
	case FAB_ID_AUDIO:

		if (init_id > MAX_AUDIO_INITID_VAL)
			return init_id_str_unknown;

		return init_id_str_audio_tng[init_id];

	case FAB_ID_PM_SECONDARY:
	case FAB_ID_SECONDARY:

		if (init_id > MAX_SECONDARY_INITID_VAL)
			return init_id_str_unknown;

		return init_id_str_secondary_tng[init_id];

	case FAB_ID_PM_GP:
	case FAB_ID_GP:

		if (init_id > MAX_GP_INITID_VAL)
			return init_id_str_unknown;

		return init_id_str_gp_tng[init_id];

	case FAB_ID_PM_SC:
	case FAB_ID_SC:

		if (init_id > MAX_SC_INITID_VAL)
			return init_id_str_unknown;

		return init_id_str_sc_tng[init_id];

	default:
		return init_id_str_unknown;
	}
}

int errorlog_element_type(u8 id_type)
{
	if (id_type >= 0 && id_type <= 15)
		return 0; /* flag_status */
	else
		return 1; /* ia_errorlog */
}
