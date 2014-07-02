/*
 * Copyright (C) 2013 Intel Corp
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

#ifndef _REBOOT_TARGET_H_
#define _REBOOT_TARGET_H_

struct reboot_target {
	int (*set_reboot_target)(const char *name, const int id);
};

extern const char *reboot_target_id2name(int id);

extern int reboot_target_register(struct reboot_target *);
extern int reboot_target_unregister(struct reboot_target *);

#endif	/* _REBOOT_TARGET_H_ */
