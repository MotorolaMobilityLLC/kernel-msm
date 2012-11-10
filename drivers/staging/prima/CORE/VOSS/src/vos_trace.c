/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**=========================================================================

  \file  vos_trace.c

  \brief virtual Operating System Servies (vOS)

   Trace, logging, and debugging definitions and APIs

   Copyright 2008,2011 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  09/16/08    hvm    Adding ability to set multiple trace levels per component
  09/11/08    lac    Added trace levels per component.  Cleanup from review.
  08/14/08    vpai   Particular modules and desired level can be selected
  06/20/08    vpai   Created Module
===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_trace.h>

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define VOS_TRACE_BUFFER_SIZE ( 512 )

// macro to map vos trace levels into the bitmask
#define VOS_TRACE_LEVEL_TO_MODULE_BITMASK( _level ) ( ( 1 << (_level) ) )

typedef struct
{
   // Trace level for a module, as a bitmask.  The bits in this mask
   // are ordered by VOS_TRACE_LEVEL.  For example, each bit represents
   // one of the bits in VOS_TRACE_LEVEL that may be turned on to have
   // traces at that level logged, i.e. if VOS_TRACE_LEVEL_ERROR is
   // == 2, then if bit 2 (low order) is turned ON, then ERROR traces
   // will be printed to the trace log.
   //
   // Note that all bits turned OFF means no traces.
   v_U16_t moduleTraceLevel;

   // 3 character string name for the module
   unsigned char moduleNameStr[ 4 ];   // 3 chars plus the NULL

} moduleTraceInfo;

#define VOS_DEFAULT_TRACE_LEVEL \
   ((1<<VOS_TRACE_LEVEL_FATAL)|(1<<VOS_TRACE_LEVEL_ERROR))

// Array of static data that contains all of the per module trace
// information.  This includes the trace level for the module and
// the 3 character 'name' of the module for marking the trace logs.
moduleTraceInfo gVosTraceInfo[ VOS_MODULE_ID_MAX ] =
{
   [VOS_MODULE_ID_BAP]        = { VOS_DEFAULT_TRACE_LEVEL, "BAP" },
   [VOS_MODULE_ID_TL]         = { VOS_DEFAULT_TRACE_LEVEL, "TL " },
#ifndef FEATURE_WLAN_INTEGRATED_SOC
   [VOS_MODULE_ID_BAL]        = { VOS_DEFAULT_TRACE_LEVEL, "BAL" },
   [VOS_MODULE_ID_SAL]        = { VOS_DEFAULT_TRACE_LEVEL, "SAL" },
   [VOS_MODULE_ID_SSC]        = { VOS_DEFAULT_TRACE_LEVEL, "SSC" },
#endif
#ifdef FEATURE_WLAN_INTEGRATED_SOC
   [VOS_MODULE_ID_WDI]        = { VOS_DEFAULT_TRACE_LEVEL, "WDI"},
#endif
   [VOS_MODULE_ID_HDD]        = { VOS_DEFAULT_TRACE_LEVEL, "HDD" },
   [VOS_MODULE_ID_SME]        = { VOS_DEFAULT_TRACE_LEVEL, "SME" },
   [VOS_MODULE_ID_PE]         = { VOS_DEFAULT_TRACE_LEVEL, "PE " },
#ifndef FEATURE_WLAN_INTEGRATED_SOC
   [VOS_MODULE_ID_HAL]        = { VOS_DEFAULT_TRACE_LEVEL, "HAL" },
#else
   [VOS_MODULE_ID_WDA]        = { VOS_DEFAULT_TRACE_LEVEL, "WDA" },
#endif
   [VOS_MODULE_ID_SYS]        = { VOS_DEFAULT_TRACE_LEVEL, "SYS" },
   [VOS_MODULE_ID_VOSS]       = { VOS_DEFAULT_TRACE_LEVEL, "VOS" },
#ifdef WLAN_SOFTAP_FEATURE
   [VOS_MODULE_ID_SAP]        = { VOS_DEFAULT_TRACE_LEVEL, "SAP" },
   [VOS_MODULE_ID_HDD_SOFTAP] = { VOS_DEFAULT_TRACE_LEVEL, "HSP" },
#endif
};


/*-------------------------------------------------------------------------
  Functions
  ------------------------------------------------------------------------*/
void vos_trace_setLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level )
{
   // Make sure the caller is passing in a valid LEVEL.
   if ( level >= VOS_TRACE_LEVEL_MAX )
   {
      pr_err("%s: Invalid trace level %d passed in!\n", __FUNCTION__, level);
      return;
   }

   // Treat 'none' differently.  NONE means we have to run off all
   // the bits in the bit mask so none of the traces appear.  Anything other
   // than 'none' means we need to turn ON a bit in the bitmask.
   if ( VOS_TRACE_LEVEL_NONE == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = VOS_TRACE_LEVEL_NONE;
   }
   else
   {
      // Set the desired bit in the bit mask for the module trace level.
      gVosTraceInfo[ module ].moduleTraceLevel |= VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level );
   }
}

void vos_trace_setValue( VOS_MODULE_ID module, VOS_TRACE_LEVEL level, v_U8_t on)
{
   // Make sure the caller is passing in a valid LEVEL.
   if ( level < 0  || level >= VOS_TRACE_LEVEL_MAX )
   {
      pr_err("%s: Invalid trace level %d passed in!\n", __FUNCTION__, level);
      return;
   }

   // Make sure the caller is passing in a valid module.
   if ( module < 0 || module >= VOS_MODULE_ID_MAX )
   {
      pr_err("%s: Invalid module id %d passed in!\n", __FUNCTION__, module);
      return;
   }

   // Treat 'none' differently.  NONE means we have to turn off all
   // the bits in the bit mask so none of the traces appear.
   if ( VOS_TRACE_LEVEL_NONE == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = VOS_TRACE_LEVEL_NONE;
   }
   // Treat 'All' differently.  All means we have to turn on all
   // the bits in the bit mask so all of the traces appear.
   else if ( VOS_TRACE_LEVEL_ALL == level )
   {
      gVosTraceInfo[ module ].moduleTraceLevel = 0xFFFF;
   }

   else
   {
      if (on)
         // Set the desired bit in the bit mask for the module trace level.
         gVosTraceInfo[ module ].moduleTraceLevel |= VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level );
      else
         // Clear the desired bit in the bit mask for the module trace level.
         gVosTraceInfo[ module ].moduleTraceLevel &= ~(VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level ));
   }
}


v_BOOL_t vos_trace_getLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level )
{
   v_BOOL_t traceOn = VOS_FALSE;

   if ( ( VOS_TRACE_LEVEL_NONE == level ) ||
        ( VOS_TRACE_LEVEL_ALL  == level ) ||
        ( level >= VOS_TRACE_LEVEL_MAX  )    )
   {
      traceOn = VOS_FALSE;
   }
   else
   {
      traceOn = ( level & gVosTraceInfo[ module ].moduleTraceLevel ) ? VOS_TRUE : VOS_FALSE;
   }

   return( traceOn );
}

void vos_snprintf(char *strBuffer, unsigned  int size, char *strFormat, ...)
{
    va_list val;

    va_start( val, strFormat );
    snprintf (strBuffer, size, strFormat, val);
    va_end( val );
}



#ifdef VOS_ENABLE_TRACING

/*----------------------------------------------------------------------------

  \brief vos_trace_msg() - Externally called trace function

  Checks the level of severity and accordingly prints the trace messages

  \param module - module identifier.   A member of the VOS_MODULE_ID
         enumeration that identifies the module issuing the trace message.

  \param level - trace level.   A member of the VOS_TRACE_LEVEL
         enumeration indicating the severity of the condition causing the
         trace message to be issued.   More severe conditions are more
         likely to be logged.

  \param strFormat - format string.  The message to be logged.  This format
         string contains printf-like replacement parameters, which follow
         this parameter in the variable argument list.

  \return  nothing

  \sa

  --------------------------------------------------------------------------*/
void vos_trace_msg( VOS_MODULE_ID module, VOS_TRACE_LEVEL level, char *strFormat, ... )
{
   char strBuffer[VOS_TRACE_BUFFER_SIZE];
   int n;

   // Print the trace message when the desired level bit is set in the module
   // tracel level mask.
   if ( gVosTraceInfo[ module ].moduleTraceLevel & VOS_TRACE_LEVEL_TO_MODULE_BITMASK( level ) )
   {
      // the trace level strings in an array.  these are ordered in the same order
      // as the trace levels are defined in the enum (see VOS_TRACE_LEVEL) so we
      // can index into this array with the level and get the right string.  The
      // vos trace levels are...
      // none, Fata, Error, Warning, Info, InfoHigh, InfoMed, InfoLow
      static const char * TRACE_LEVEL_STR[] = { "  ", "F ", "E ", "W ", "I ", "IH", "IM", "IL" };
      va_list val;
      va_start(val, strFormat);

      // print the prefix string into the string buffer...
      n = snprintf(strBuffer, VOS_TRACE_BUFFER_SIZE, "[WLAN][%d:%2s:%3s] ",
                   in_interrupt() ? 0 : current->pid,
                   (char *) TRACE_LEVEL_STR[ level ],
                   (char *) gVosTraceInfo[ module ].moduleNameStr );

      // print the formatted log message after the prefix string.
      vsnprintf(strBuffer + n, VOS_TRACE_BUFFER_SIZE - n, strFormat, val );
      pr_err("%s\n", strBuffer);
      va_end( val);
   }
}

void vos_trace_display(void)
{
   VOS_MODULE_ID moduleId;

   pr_err("     1)FATAL  2)ERROR  3)WARN  4)INFO  5)INFO_H  6)INFO_M  7)INFO_L\n");
   for (moduleId = 0; moduleId < VOS_MODULE_ID_MAX; ++moduleId)
   {
      pr_err("%2d)%s    %s        %s       %s       %s        %s         %s         %s\n",
             (int)moduleId,
             gVosTraceInfo[moduleId].moduleNameStr,
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_FATAL)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_ERROR)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_WARN)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_HIGH)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_MED)) ? "X":" ",
             (gVosTraceInfo[moduleId].moduleTraceLevel & (1 << VOS_TRACE_LEVEL_INFO_LOW)) ? "X":" "
         );
   }
}

#endif
