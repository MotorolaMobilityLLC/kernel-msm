/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/



#ifndef _SEP_LOG__H_
#define _SEP_LOG__H_

/* Define different "BUG()" behavior in DEBUG mode */
#ifdef DEBUG
/* It is easier to attach a debugger without causing the exception of "BUG()" */
#define SEP_DRIVER_BUG() do {dump_stack(); while (1); } while (0)
#else
#define SEP_DRIVER_BUG() BUG()
#endif

/* SeP log levels (to be used in sep_log_level and SEP_BASE_LOG_LEVEL) */
#define SEP_LOG_LEVEL_ERR       0
#define SEP_LOG_LEVEL_WARN      1
#define SEP_LOG_LEVEL_INFO      2
#define SEP_LOG_LEVEL_DEBUG     3
#define SEP_LOG_LEVEL_TRACE     4

/* SeP log components (to be used in sep_log_mask and SEP_LOG_CUR_COMPONENT) */
#define SEP_LOG_MASK_MAIN        1 /* dx_driver.c */
#define SEP_LOG_MASK_LLI_MGR     (1<<1)
#define SEP_LOG_MASK_CTX_MGR     (1<<2)
#define SEP_LOG_MASK_DESC_MGR    (1<<3)
#define SEP_LOG_MASK_SYSFS       (1<<4)
#define SEP_LOG_MASK_SEP_INIT    (1<<5)
#define SEP_LOG_MASK_CRYPTO_API  (1<<6)
#define SEP_LOG_MASK_SEP_REQUEST (1<<7)
#define SEP_LOG_MASK_SEP_POWER   (1<<8)
#define SEP_LOG_MASK_SEP_APP     (1<<9)
#define SEP_LOG_MASK_SEP_PRINTF  (1<<31)
#define SEP_LOG_MASK_ALL        (SEP_LOG_MASK_MAIN | SEP_LOG_MASK_SEP_INIT |\
	SEP_LOG_MASK_LLI_MGR | SEP_LOG_MASK_CTX_MGR | SEP_LOG_MASK_DESC_MGR |\
	SEP_LOG_MASK_SYSFS | SEP_LOG_MASK_CRYPTO_API |\
	SEP_LOG_MASK_SEP_REQUEST | SEP_LOG_MASK_SEP_POWER |\
	SEP_LOG_MASK_SEP_APP | SEP_LOG_MASK_SEP_PRINTF)


/* This printk wrapper masks maps log level to KERN_* levels and masks prints *
   from specific components at run time based on SEP_LOG_CUR_COMPONENT and    *
*  sep_log_component_mask.                                                    */
#define MODULE_PRINTK(level, format, ...) do {			\
	if (sep_log_mask & SEP_LOG_CUR_COMPONENT)		\
		printk(level MODULE_NAME ":%s: " format,	\
			__func__, ##__VA_ARGS__);		\
} while (0)

extern int sep_log_level;
extern int sep_log_mask;


/* change this to set the preferred log level */
#ifdef DEBUG
#define SEP_BASE_LOG_LEVEL SEP_LOG_LEVEL_TRACE
#else
#define SEP_BASE_LOG_LEVEL SEP_LOG_LEVEL_WARN
#endif

#define SEP_LOG_ERR(format, ...) \
	MODULE_PRINTK(KERN_ERR, format, ##__VA_ARGS__);

#if (SEP_BASE_LOG_LEVEL >= SEP_LOG_LEVEL_WARN)
#define SEP_LOG_WARN(format, ...) do {				\
	if (sep_log_level >= SEP_LOG_LEVEL_WARN)		\
		MODULE_PRINTK(KERN_WARNING, format,		\
		##__VA_ARGS__);					\
} while (0)
#else
#define SEP_LOG_WARN(format, arg...) do {} while (0)
#endif

#if (SEP_BASE_LOG_LEVEL >= SEP_LOG_LEVEL_INFO)
#define SEP_LOG_INFO(format, ...) do {				\
	if (sep_log_level >= SEP_LOG_LEVEL_INFO)		\
		MODULE_PRINTK(KERN_INFO, format, ##__VA_ARGS__); \
} while (0)
#else
#define SEP_LOG_INFO(format, arg...) do {} while (0)
#endif

#if (SEP_BASE_LOG_LEVEL >= SEP_LOG_LEVEL_DEBUG)
#define SEP_LOG_DEBUG(format, ...) do {				\
	if (sep_log_level >= SEP_LOG_LEVEL_DEBUG)		\
		MODULE_PRINTK(KERN_DEBUG, format, ##__VA_ARGS__);\
} while (0)
#else
#define SEP_LOG_DEBUG(format, arg...) do {} while (0)
#endif

#if (SEP_BASE_LOG_LEVEL >= SEP_LOG_LEVEL_TRACE)
#define SEP_LOG_TRACE(format, ...) do {				\
	if (sep_log_level >= SEP_LOG_LEVEL_TRACE)		\
		MODULE_PRINTK(KERN_DEBUG, "<trace> " format,	\
			      ##__VA_ARGS__);			\
} while (0)
#else
#define SEP_LOG_TRACE(format, arg...) do {} while (0)
#endif

#undef pr_fmt
#define pr_fmt(fmt)     KBUILD_MODNAME ": %s:%d: " fmt, __func__, __LINE__

#endif

