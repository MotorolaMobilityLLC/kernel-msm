/*
 * Linux 2.6.32 and later Kernel module for VMware MVP PVTCP Server
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

/**
 * @file
 *
 * @brief Server (offload) side code.
 */

#include "pvtcp.h"

/**
 * @brief Allocates and initializes a net buffer.
 * @param size buffer size.
 * @param channel channel from which to copy.
 * @param header header of this payload.
 * @param copy function to use for copying.
 * @return address of buffer or NULL
 */

void *
PvtcpBufAlloc(unsigned int size,
              CommChannel channel,
              CommPacket *header,
              int (*copy)(CommChannel channel,
                          void *dest,
                          unsigned int size,
                          int kern))
{
   PvtcpOffBuf *buf;
   void *res = NULL;

   /* coverity[alloc_fn] */
   /* coverity[var_assign] */

   if (size == 0) {
      return NULL;
   }

   if ((header->opCode == PVTCP_OP_IO) && (size > PVTCP_SOCK_BUF_SIZE)) {
      /*
       * Since stream sockets always chunk payloads, this is a non-stream output
       * operation that needs fragment allocation. Non-stream payloads are never
       * enqueued, so we can overload the 'len' and 'off' fields.
       * Allocate the header with an iovec array, plus each iovec entry. Note that
       * there will be at least two fragments.
       * The 'off' field is set to USHRT_MAX to distinguish frag-allocations,
       * and 'len' stores the number of fragments.
       */

      unsigned int nmbFrags =
         (size + PVTCP_SOCK_BUF_SIZE - 1) / PVTCP_SOCK_BUF_SIZE;
      unsigned int i;
      struct kvec *vec;

      buf = CommOS_Kmalloc((sizeof *buf - sizeof buf->data) +
                           (sizeof *vec * nmbFrags));
      if (buf) {
         CommOS_ListInit(&buf->link);
         buf->len = (unsigned short)nmbFrags;
         buf->off = USHRT_MAX;
         res = vec = PvtcpOffBufFromInternal(buf);

         for (i = 0; i < nmbFrags; i++) {
            unsigned int fragSize =
               size < PVTCP_SOCK_BUF_SIZE ? size : PVTCP_SOCK_BUF_SIZE;

            vec[i].iov_base = CommOS_Kmalloc(fragSize);
            vec[i].iov_len = fragSize;

            if (!vec[i].iov_base) {
               i++;
               goto undo;
            }
            if (copy(channel, vec[i].iov_base, vec[i].iov_len, 1) !=
                vec[i].iov_len) {
               CommOS_Log(("%s: Failed to copy from channel!\n", __FUNCTION__));
               i++;
               goto undo;
            }

            size -= fragSize;
         }
         goto out;

undo:
         while (i) {
            CommOS_Kfree(vec[--i].iov_base);
         }
         CommOS_Kfree(buf);
         res = NULL;
      }
   } else {
      buf = CommOS_Kmalloc(size + sizeof *buf - sizeof buf->data);
      if (buf) {
         CommOS_ListInit(&buf->link);
         buf->len = (unsigned short)size;
         buf->off = 0;
         res = PvtcpOffBufFromInternal(buf);
         if (copy(channel, res, size, 1) != size) {
            CommOS_Kfree(buf);
            CommOS_Log(("%s: Failed to copy from channel!\n", __FUNCTION__));
            res = NULL;
         }
      }
   }

out:
   return res;
}


/**
 * @brief Deallocates given net buffer.
 * @param buf buffer to deallocate
 * @sideeffect Frees memory
 */

void
PvtcpBufFree(void *buf)
{
   if (buf) {
      PvtcpOffBuf *internalBuf = PvtcpOffInternalFromBuf(buf);

      if (internalBuf->off == USHRT_MAX) {
         struct kvec *vec = buf;

         while (internalBuf->len) {
            CommOS_Kfree(vec[--internalBuf->len].iov_base);
         }
      }
      CommOS_Kfree(internalBuf);
   }
}


/**
 * @brief Initializes the Pvtcp socket offload common fields.
 * @param pvsk pvtcp socket.
 * @param channel Comm channel this socket is associated with.
 * @return 0 if successful, -1 otherwise.
 */

int
PvtcpOffSockInit(PvtcpSock *pvsk,
                 CommChannel channel)
{
   int rc = PvtcpSockInit(pvsk, channel);

   pvsk->opFlags = 0;
   pvsk->flags = 0;
   return rc;
}

