/*
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#ifndef MDFLD_HDCP_REG_H
#define MDFLD_HDCP_REG_H

/* Integrated HDMI specific registers */

#define RESERVED2(x,y)  x##y
#define RESERVED1(x,y)  RESERVED2(x,y)
#define RANDOMNUMBER	__LINE__	// __COUNTER__
#define UNIQUENAME(ValueName) RESERVED1(ValueName, RANDOMNUMBER)

/* TBD: This may change when tested on actual system */
#define HDCP_MAX_RI_QUERY_COUNT 4
#define HDCP_MAX_NUM_DWORDS 4	//128 bits
#define HDCP_MAX_RANDOMNUM_LENGTH 2	//In DWORD => 64 bits
#define HDCP_MAX_RETRY_DISABLE 2
/*All sizes are defined in bytes */
#define HDCP_SIZEOF_AKSV    8
#define HDCP_SIZEOF_BKSV    8
#define HDCP_SIZEOF_AN        5
#define HDCP_SIZEOF_RI        2
#define HDCP_ENCRYPTED_KEY_SIZE 12	//Akeys, IV and MAC
#define HDCP_NUM_AKEYS 40
#define HDCP_NEXT_RI_FRAME 126
#define HDCP_MAX_RANDOM_NUM_SIZE 4	//in dwords

#define HDCP_CONVERT_BIG_ENDIAN(x) (((x&0x000000ff)<<24)|\
                                    ((x&0x0000ff00)<<8)|\
                                    ((x&0x00ff0000)>>8)|\
                                    ((x&0xff000000)>>24))

#define HDCP_MAX_AN_RETRY 100

#define HDCP_AN_LO_INDEX 0
#define HDCP_AN_HI_INDEX 1

uint32_t hdcp_invalid_an_list[6][2] = {
	{0x881cf9e4, 0x38155bf4}
	,
	{0xb0e81640, 0xb5cac2ec}
	,
	{0x514fa3e7, 0x5bbb3806}
	,
	{0xd1b4923a, 0x6172afbb}
	,
	{0x0c16fd1c, 0x1b28baf5}
	,
	{0x00000000, 0x00000000}
};

/* HDMI HDCP Regs */
/* HDCP config */

typedef enum _mdfld_hdcp_config_enum {
	HDCP_Off = 0,
	HDCP_CAPTURE_AN = 1,
	HDCP_DECRYPT_KEYS = 2,
	HDCP_AUTHENTICATE_AND_ENCRYPT = 3,
	HDCP_UNIQUE_MCH_ID = 5,
	HDCP_ENCRYPT_KEYS = 6,
	HDCP_CYPHER_CHECK_MODE = 7
} mdfld_hdcp_config_en;

#define MDFLD_HDCP_CONFIG_REG 0x61400
#define MDFLD_HDCP_CONFIG_PRESERVED_BITS    BITRANGE(3,31)
typedef union _mdfld_hdcp_config {
	uint32_t value;

	struct {
		uint32_t hdcp_config:3;	//bit 2:0; uses HDCP_CONFIGURATION_EN
		uint32_t UNIQUENAME(Reserved):29;	//bit 3:31
	};
} mdfld_hdcp_config_t;

/* HDCP_STATUS */

#define MDFLD_HDCP_STATUS_REG 0x61448
#define MDFLD_HDCP_STATUS_PRESERVED_BITS    BITRANGE(24,31)
typedef union _mdfld_hdcp_status {
	uint32_t value;

	struct {
		uint32_t ainfo:8;	//Bit 7:0
		uint32_t frame_count:8;	//Bit 15:8
		uint32_t cipher_hdcp_status:1;	//Bit 16
		uint32_t cipher_an_status:1;	//Bit 17
		uint32_t cipher_ri_ready_status:1;	//Bit 18
		uint32_t cipher_ri_match_status:1;	//Bit 19
		uint32_t cipher_encrypting_status:1;	//Bit 20
		uint32_t cipher_ready_for_encryption:1;	//Bit 21
		uint32_t cipher_mch_id_ready:1;	//Bit 22
		uint32_t cipher_mac_status:1;	//Bit 23
		uint32_t UNIQUENAME(Reserved):8;	//Bit 31:24
	};
} mdfld_hdcp_status_t;

/* HDCP_RI */
#define MDFLD_HDCP_RECEIVER_RI_REG 0x61418
#define MDFLD_HDCP_RECEIVER_RI_PRESERVED_BITS    BITRANGE(16,31)
typedef union _mdfld_hdcp_receiver_ri {
	uint32_t value;

	struct {
		uint32_t ri:16;	//bit 15:0
		uint32_t UNIQUENAME(Reserved):16;	//bit 31:16
	};
} mdfld_hdcp_receiver_ri_t;

/* HDCP_BKSV_HI */
#define MDFLD_HDCP_BKSV_HI_REG 0x6140C
#define MDFLD_HDCP_BKSV_HI_PRESERVED_BITS BITRANGE(8,31)
typedef union _mdfld_hdcp_bksv_hi {
	uint32_t value;

	struct {
		uint32_t bksv_hi:8;	//bit 7:0
		uint32_t UNIQUENAME(Reserved):24;	//bit 31:8
	};
} mdfld_hdcp_bksv_hi_t;

/* HDCP_AKEY_HI */
#define MDFLD_HDCP_AKEY_HI_REG 0x61424
#define MDFLD_HDCP_AKEY_HI_PRESERVED_BITS BITRANGE(20,31)
typedef union _mdfld_hdcp_akey_hi {
	uint32_t value;

	struct {
		uint32_t akey_hi:20;	//bit 7:0
		uint32_t UNIQUENAME(Reserved):12;	//bit 31:8
	};
} mdfld_hdcp_akey_hi_t;

/* HDCP_REP: Repeator specific register definitions */

/* Repeater Control register */
typedef enum _mdfld_hdcp_repeater_status_enum {
	HDCP_REPEATER_STATUS_IDLE = 0,
	HDCP_REPEATER_STATUS_BUSY = 1,
	HDCP_REPEATER_STATUS_RDY_NEXT_DATA = 2,
	HDCP_REPEATER_STATUS_COMPLETE_NO_MATCH = 4,
	HDCP_REPEATER_STATUS_COMPLETE_MATCH = 12
} mdfld_hdcp_repeater_status_en;

typedef enum _mdfld_hdcp_repeater_ctrl_enum {
	HDCP_REPEATER_CTRL_IDLE = 0,
	HDCP_REPEATER_32BIT_TEXT_IP = 1,
	HDCP_REPEATER_COMPLETE_SHA1 = 2,
	HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP = 4,
	HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP = 5,
	HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP = 6,
	HDCP_REPEATER_32BIT_MO_IP = 7
} mdfld_hdcp_repeater_ctrl_en;

#define MDFLD_HDCP_REP_REG 0x61444
#define MDFLD_HDCP_REP_PRESERVED_BITS BITRANGE(8,31)
typedef union _mdfld_hdcp_rep {
	uint32_t value;

	struct {
		uint32_t repeater_present:1;	//bit 0
		uint32_t repeater_control:3;	//bit 3:1
		uint32_t UNIQUENAME(Reserved):12;	//bit 15:4 BUN#: 07ww44#1
		const uint32_t repeater_status:4;	//bit 19:16
		uint32_t UNIQUENAME(Reserved):12;	//bit 31:20
	};
} mdfld_hdcp_rep_t;

/* HDCP_BKSV_HI */
#define MDFLD_HDCP_AKSV_HI_REG 0x61450
#define MDFLD_HDCP_AKSV_HI_PRESERVED_BITS BITRANGE(8,31)
typedef union _mdfld_hdcp_aksv_hi {
	uint32_t value;

	struct {
		uint32_t aksv_hi:8;	//bit 7:0
		uint32_t UNIQUENAME(Reserved):24;	//bit 31:8
	};
} mdfld_hdcp_aksv_hi_t;

typedef union _mdfld_hdcp_aksv {
	uint8_t byte[8];

	struct {
		uint32_t low;
		mdfld_hdcp_aksv_hi_t hi;
	} aksv;
} mdfld_hdcp_aksv_t;

/* These holds part of the hash result from the receiver used for repeaters */
#define MDFLD_HDCP_VPRIME_H0 0x6142C
#define MDFLD_HDCP_VPRIME_H1 0x61430
#define MDFLD_HDCP_VPRIME_H2 0x61434
#define MDFLD_HDCP_VPRIME_H3 0x61438
#define MDFLD_HDCP_VPRIME_H4 0x6143C

#define MDFLD_HDCP_SHA1_IN    0x61440

/* Define of registers that don't need register definitions */
#define MDFLD_HDCP_INIT_REG 0x61404
#define MDFLD_HDCP_AN_LOW_REG 0x61410
#define MDFLD_HDCP_AN_HI_REG 0x61414
#define MDFLD_HDCP_BKSV_LOW_REG 0x61408
#define MDFLD_HDCP_AKSV_LOW_REG 0x61454
/* Akey registers */
#define MDFLD_HDCP_AKEY_LO_REG 0x6141C
#define MDFLD_HDCP_AKEY_MED_REG 0x61420

#endif				/* MDFLD_HDCP_REG_H */
