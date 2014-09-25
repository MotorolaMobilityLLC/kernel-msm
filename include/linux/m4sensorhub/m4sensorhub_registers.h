/*
 * Copyright (c) 2012, Motorola, Inc. All Rights Reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __M4SENSORHUB_MEMMAP_H__
#define __M4SENSORHUB_MEMMAP_H__

#include "m4sensorhub_bank_enum.h"
#include "m4sensorhub_reg_enum.h"

#ifdef __KERNEL__

/* Global (kernel) definitions */

#define M4SH_MAX_REG_SIZE               2048
#define M4SH_MAX_STACK_BUF_SIZE         32
#define m4sh_no_mask                    NULL

#endif /*__KERNEL__ */
#endif /*__M4SENSORHUB_MEMMAP_H__ */
