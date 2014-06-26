/*
 * panic_gbuffer.h
 *
 * Copyright (C) 2013 Intel Corp
 *
 * Expose a generic buffer header to be passed to the panic handler in
 * order to dump buffer content in case of kernel panic.
 *
 * -----------------------------------------------------------------------
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

#ifndef _LINUX_PANIC_GBUFFER_H
#define _LINUX_PANIC_GBUFFER_H

struct g_buffer_header {
	unsigned char *base;
	size_t size;
	size_t woff;
	size_t head;
};

void panic_set_gbuffer(struct g_buffer_header *gbuffer);

#endif /* _LINUX_PANIC_GBUFFER_H */
