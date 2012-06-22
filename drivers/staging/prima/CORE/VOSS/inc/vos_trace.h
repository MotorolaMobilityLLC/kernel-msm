/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#if !defined( __VOS_TRACE_H )
#define __VOS_TRACE_H

/**=========================================================================
  
  \file  vos_trace.h
  
  \brief virtual Operating System Servies (vOS)
               
   Trace, logging, and debugging definitions and APIs
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include  <vos_types.h>  // For VOS_MODULE_ID...
#include  <stdarg.h>       // For va_list...

/*-------------------------------------------------------------------------- 
  Type declarations 
  ------------------------------------------------------------------------*/

typedef enum 
{
   // NONE means NO traces will be logged.  This value is in place for the 
   // vos_trace_setlevel() to allow the user to turn off all traces.
   VOS_TRACE_LEVEL_NONE = 0,
   
   // the following trace levels are the ones that 'callers' of VOS_TRACE()
   // can specify in for the VOS_TRACE_LEVEL parameter.  Traces are classified
   // by severity (FATAL being more serious than INFO for example).   
   VOS_TRACE_LEVEL_FATAL,
   VOS_TRACE_LEVEL_ERROR, 
   VOS_TRACE_LEVEL_WARN,  
   VOS_TRACE_LEVEL_INFO,
   VOS_TRACE_LEVEL_INFO_HIGH,
   VOS_TRACE_LEVEL_INFO_MED,
   VOS_TRACE_LEVEL_INFO_LOW,

   // ALL means all trace levels will be active.  This value is in place for the 
   // vos_trace_setlevel() to allow the user to turn ON all traces.
   VOS_TRACE_LEVEL_ALL, 

   
   // not a real level.  Used to identify the maximum number of 
   // VOS_TRACE_LEVELs defined.
   VOS_TRACE_LEVEL_MAX   
    
} VOS_TRACE_LEVEL;

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define ASSERT_BUFFER_SIZE ( 512 )

// below definition is obsolete and is no longer being used in BMP and WM
// TODO: remove this once this is not used on Android
#define VOS_ENABLE_TRACING 

#include  <i_vos_trace.h>   

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
  
  \brief vos_trace_setLevel() - Set the trace level for a particular module
  
  This is an external API that allows trace levels to be set for each module.
  
  \param level - trace level.   A member of the VOS_TRACE_LEVEL 
         enumeration indicating the severity of the condition causing the
         trace message to be issued.   More severe conditions are more 
         likely to be logged.
         
  \return  nothing
    
  \sa
  --------------------------------------------------------------------------*/
void vos_trace_setLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level );

/**----------------------------------------------------------------------------
  
  \brief vos_trace_getLevel() - Get the trace level
  
  This is an external API that returns a boolean value to signify if a 
  particular trace level is set for the specified module.
  
  \param level - trace level.   A member of the VOS_TRACE_LEVEL enumeration 
                 indicating the severity of the condition causing the trace 
                 message to be issued.
         
                 Note that individual trace levels are the only valid values
                 for this API.  VOS_TRACE_LEVEL_NONE and VOS_TRACE_LEVEL_ALL
                 are not valid input and will return FALSE

  \return  VOS_FALSE - the specified trace level for the specified module is OFF 
    
           VOS_TRUE - the specified trace level for the specified module is ON
    
  \sa vos_trace_setLevel()
  --------------------------------------------------------------------------*/
v_BOOL_t vos_trace_getLevel( VOS_MODULE_ID module, VOS_TRACE_LEVEL level );

#endif
