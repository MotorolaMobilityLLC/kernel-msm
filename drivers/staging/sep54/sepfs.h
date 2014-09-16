/*
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* When defined in a policy for the command ID this will lock down access to the
   entire TA by restricting who can open a session to the TA. */
#define RESTRICT_OPEN -666

/**
 * Determine if the currently connected client is allowed to access the
 * TA, using the specified command
 * @uuid The TA that is being connected to
 * @cmd_id The command that is being invoked in the TA,
 * RESTRICT_OPEN if it is an Open command
 * return true if access is allowed
 */
bool is_permitted(const u8 *uuid, int cmd_id);
