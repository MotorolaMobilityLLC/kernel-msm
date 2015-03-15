/**
 * \file
 *
 * \authors   Evan Lojewski <evan.lojewski@emmicro-us.com>
 *
 * \brief     EEPROM firmware image.
 *
 * \copyright (C) 2013-2014 EM Microelectronic – US, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef EEPROM_IMAGE_H
#define EEPROM_IMAGE_H

#include <linux/em718x_types.h>


/* EEPROM Image File format
 * 2bytes   MagicNumber
 * 2bytes   Flags
 * 4bytes   Text CRC32-CCITT
 * 4bytes   Data CRC32-CCITT
 * 2bytes   Text Length
 * 2bytes   Data Length
 * Nbytes   Text
 * Nbytes   Data
 */


#define EEPROM_MAGIC_VALUE 0x652A /**< special value that must be present in the magic field of the EEPROMHeader structure */

/** \brief values of the ROMVerExp field */
#define ROM_VERSION_ANY 0 /**< the file is compatible with any ROM version */
#define ROM_VERSION_DI01 1 /**< this file requires ROM di01 */
#define ROM_VERSION_DI02 2 /**< this file requires ROM di02 */

/** \brief corresponding values of the ROM Version register */
#define ROM_VER_REG_DI01 0x7A8 /**< this value corresponds to ROM di01 */
#define ROM_VER_REG_DI02 0x9E6 /**< this value corresponds to ROM di02 */

/**
 * \brief EEPROM boot flags.
 */
PREPACK typedef union MIDPACK {
	/**
	 * \brief Direct access to all flags.
	 */
	u16 value;                                                     /**< the 16 bit register value for the EEPROM flags register */
	struct
	{
		/** \brief Do not execute the EEPROM image immediately after upload */
		u16   EEPROMNoExec:1;
		/** \brief Reserved */
		u16   Reserved:7;
		/** \brief The clock speed to upload the firmware at */
		u16   I2CClockSpeed:3;
		/** \brief Rom version expected */
		u16   ROMVerExp:4;
		/** \brief Reserved */
		u16   reserved1:1;
	}
	bits;                                                             /**< the bit fields within the EEPROM flags register */
}
EEPROMFlags;                                                         /**< typedef for storing EEPROM flags values */
POSTPACK

/**
 * \brief EEPROM header format
 *
 * NOTE: a ROM version may also be useful to ensure that an
 * incorrect ram binary is not used. This is currently not
 * implemented, however the RAM / EEPROM start code can double
 * check this before it starts if needed.
 */
PREPACK typedef struct MIDPACK {
	/** \brief The firmware magic number */
	u16 magic;                                                     // Already read
	/** \brief Flags used to notify and control the boot process */
	EEPROMFlags flags;
	/** \brief The CRC32-CCITT of the firmware text segment */
	u32 text_CRC;
	/** \brief reserved 32 bit area 1 */
	u32 reserved1;
	/** \brief The number of program bytes to upload */
	u16 text_length;
	/** \brief reserved 16 bit area 2 */
	u16 reserved2;
}
EEPROMHeader;                                                        /**< typedef for storing EEPROM headers */
POSTPACK

#endif /* EEPROM_IMAGE_H */
