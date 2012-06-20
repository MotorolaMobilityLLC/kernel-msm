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

/*
 * */
#if !defined( PALTYPES_H__ )
#define PALTYPES_H__

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file palTypes.h
  
    \brief Exports and types for the Platform Abstraction Layer typedefs.
    These are common typedefs that can be used across Platforms (OS/compiler
    and bus types).  All common code should adhere to these common types.
  
    $Id$ 
  
    Copyright (C) 2006 Airgo Networks, Incorporated
  
    ... description...
  
   ========================================================================== */
#ifndef WINXP_APPS_BUILD
#include "vos_types.h"
#include "vos_api.h"
#endif /* WINXP_APPS_BUILD */

#ifdef FEATURE_WLAN_INTEGRATED_SOC
#include "halLegacyPalTypes.h"
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

#ifndef MK_IMAGE_HDR
// values to check against for code that is Client/AP specific.  
// the build will define one of the following: 
// ANI_PRODUCT_TYPE_CLIENT 
// ANI_PRODUCT_TYPE_AP
// ANI_PRODUCT_TYPE_AP_SDK 
//
// Validate the Product type being built....
//
#if ( defined( ANI_PRODUCT_TYPE_CLIENT ) && ( defined( ANI_PRODUCT_TYPE_AP ) || defined( ANI_PRODUCT_TYPE_AP_SDK ) ) ) 
#error "more than one ANI_PRODUCT_TYPE_xxx is defined for this build"

#elif ( defined( ANI_PRODUCT_TYPE_AP ) && ( defined( ANI_PRODUCT_TYPE_CLIENT ) || defined( ANI_PRODUCT_TYPE_AP_SDK ) ) ) 
#error "more than one ANI_PRODUCT_TYPE_xxx is defined for this build"

#elif ( defined( ANI_PRODUCT_TYPE_AP_SDK ) && ( defined( ANI_PRODUCT_TYPE_CLIENT ) || defined( ANI_PRODUCT_TYPE_AP ) ) ) 
#error "more than one ANI_PRODUCT_TYPE_xxx is defined for this build"

#elif !( defined( ANI_PRODUCT_TYPE_CLIENT ) || defined( ANI_PRODUCT_TYPE_AP ) || defined( ANI_PRODUCT_TYPE_AP_SDK ) )
#error "NONE of the ANI_PRODUCT_TYPE_xxxx are defined for this build"

#endif


//
// Validate the Bus type being built....
//
#if defined(ANI_BUS_TYPE_PCI)

#if defined( ANI_BUS_TYPE_PCIe ) || defined( ANI_BUS_TYPE_USB ) || defined(ANI_BUS_TYPE_SDIO) || defined( ANI_BUS_TYPE_PLATFORM )
#error "more than one ANI_BUS_TYPE_xxx is defined for this build"
#endif //

#elif defined( ANI_BUS_TYPE_PCIe )

#if defined( ANI_BUS_TYPE_PCI ) || defined( ANI_BUS_TYPE_USB ) || defined(ANI_BUS_TYPE_SDIO) || defined( ANI_BUS_TYPE_PLATFORM )
#error "more than one ANI_BUS_TYPE_xxx is defined for this build"
#endif

#elif defined( ANI_BUS_TYPE_USB )

#if defined( ANI_BUS_TYPE_PCIe ) || defined( ANI_BUS_TYPE_PCI ) || defined(ANI_BUS_TYPE_SDIO) || defined( ANI_BUS_TYPE_PLATFORM )
#error "more than one ANI_BUS_TYPE_xxx is defined for this build"
#endif

#elif defined( ANI_BUS_TYPE_SDIO )

#if defined( ANI_BUS_TYPE_PCIe ) || defined( ANI_BUS_TYPE_USB ) || defined(ANI_BUS_TYPE_PCI) || defined( ANI_BUS_TYPE_PLATFORM )
#error "more than one ANI_BUS_TYPE_xxx is defined for this build"
#endif

#elif defined( ANI_BUS_TYPE_PLATFORM )

#if defined( ANI_BUS_TYPE_PCIe ) || defined( ANI_BUS_TYPE_USB ) || defined(ANI_BUS_TYPE_PCI) || defined( ANI_BUS_TYPE_SDIO )
#error "more than one ANI_BUS_TYPE_xxx is defined for this build"
#endif

#elif !( defined( ANI_BUS_TYPE_PCIe ) || defined( ANI_BUS_TYPE_USB ) || defined(ANI_BUS_TYPE_PCI) || defined(ANI_BUS_TYPE_SDIO) || defined( ANI_BUS_TYPE_PLATFORM ) )

#error "NONE of the ANI_BUS_TYPE_xxx are defined for this build"

#endif


//
// Validate the OS Type being built...
//
#if ( defined( ANI_OS_TYPE_WINDOWS ) && defined( ANI_OS_TYPE_LINUX ) && defined(ANI_OS_TYPE_OSX) && defined(ANI_OS_TYPE_AMSS) && \
      defined( ANI_OS_TYPE_ANDROID ) )
#error "more than one ANI_OS_TYPE_xxx is defined for this build"

#elif !( defined( ANI_OS_TYPE_WINDOWS ) || defined( ANI_OS_TYPE_LINUX ) || defined(ANI_OS_TYPE_OSX) || defined(ANI_OS_TYPE_AMSS) \
         || defined (ANI_OS_TYPE_ANDROID) )
#error "NONE of the ANI_OS_TYPE_xxx are defined for this build"

#endif


//
// Validate the compiler...
//
#if ( defined( ANI_COMPILER_TYPE_MSVC ) && defined( ANI_COMPILER_TYPE_GCC ) && defined( ANI_COMPILER_TYPE_RVCT ) ) 
#error "more than one ANI_COMPILER_TYPE_xxx is defined for this build"

#elif !( defined( ANI_COMPILER_TYPE_MSVC ) || defined( ANI_COMPILER_TYPE_GCC ) || defined( ANI_COMPILER_TYPE_RVCT ) )
#error "NONE of the ANI_COMPILER_TYPE_xxx are defined for this build"

#endif



// some differences related to the compiler being used...
#if defined ( ANI_COMPILER_TYPE_GCC )

#define ANI_INLINE_FUNCTION static __inline__

#elif defined( ANI_COMPILER_TYPE_MSVC )

#define ANI_INLINE_FUNCTION __inline

#elif defined( ANI_COMPILER_TYPE_RVCT )

#define ANI_INLINE_FUNCTION INLINE

#else 

#error "invalid ANI_COMPILER_TYPE definition"

#endif
#endif




// Common type definitions...

#ifndef FEATURE_WLAN_INTEGRATED_SOC
typedef unsigned char  tANI_U8;
typedef   signed char  tANI_S8;

typedef unsigned short tANI_U16;
typedef   signed short tANI_S16;

typedef unsigned long  tANI_U32;
typedef   signed long  tANI_S32;

#if defined ANI_OS_TYPE_WINDOWS
typedef unsigned __int64 tANI_U64;
#else
typedef unsigned long long tANI_U64;
#endif

typedef tANI_U8 tANI_BYTE;
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

#if defined( ANI_OS_TYPE_WINDOWS )
#if defined(_WIN64)
 typedef unsigned __int64 tANI_U32_OR_PTR;
#else
 typedef unsigned long tANI_U32_OR_PTR;
#endif
#else
typedef tANI_U32     tANI_U32_OR_PTR;
#endif

// Buffer address; could be virt or phys; could be 32- or 64-bit depending on compile option
typedef tANI_U32_OR_PTR    tANI_BUFFER_ADDR;
// which boolean is the most usefule...or both ?
#ifndef FEATURE_WLAN_INTEGRATED_SOC
typedef tANI_U8 tANI_BOOLEAN;
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

typedef enum tagAniBoolean 
{
    eANI_BOOLEAN_FALSE = 0,
    eANI_BOOLEAN_TRUE,
    
    eANI_BOOLEAN_OFF = 0,
    eANI_BOOLEAN_ON = 1,
} eAniBoolean;



//
// MAC address data type...
//
// review the usefulness of this type.  I suspect this type is not
// real useful unless we provide some 'helper' functions to manage 
// the MAC addresses.
//
#define ANI_MAC_ADDR_SIZE ( 6 )
typedef tANI_U8 tAniMacAddr[ ANI_MAC_ADDR_SIZE ];





/** ------------------------------------------------------------------------- * 

    \typedef tHddHandle
    
    \brief Handle to the HDD.  The HDD handle is given to the HAL from 
    the HDD on halOpen.  The HDD handle is an input to all HDD/PAL function
    calls and represents an opaque handle to the HDD instance that is tied
    to the HAL instance, opened through halOpen.  
    
    The HDD must be able to derive it's internal instance structure pointer
    through this handle.  hint hint...
    
    -------------------------------------------------------------------------- */
typedef void *tHddHandle;
// define a value for an invalid HAL handle.....
#define HDD_INVALID_HDD_HANDLE ( NULL )


// For packet classification routines
#define PAL_BIT_MASK(offset)         (1 << (offset))
#define PAL_PKT_FLD_DSCP_OFFSET      0
#define PAL_PKT_FLD_8021P_OFFSET     1

#define PAL_PKT_FLD_DSCP_MASK     PAL_BIT_MASK(PAL_PKT_FLD_DSCP_OFFSET)
#define PAL_PKT_FLD_8021P_MASK    PAL_BIT_MASK(PAL_PKT_FLD_8021P_OFFSET)

#if defined( ANI_OS_TYPE_WINDOWS ) || defined (ANI_OS_TYPE_OSX)
    #define PAL_PKT_FLD_8021P_BIT_OFFSET 0
#elif defined( ANI_OS_TYPE_LINUX )
    #define PAL_PKT_FLD_8021P_BIT_OFFSET 5
#elif defined( ANI_OS_TYPE_AMSS )
    #define PAL_PKT_FLD_8021P_BIT_OFFSET 0
#endif


/*
This represent an object for a spin lock and it is platform dependant
*/
//User of this variable must initialize it to PAL_INVALID_SPINLOCK_HANDLE in order for validation to work.
typedef void * tPalSpinLockHandle;
   
#define PAL_INVALID_SPINLOCK_HANDLE (NULL)

/*
 * This represent an object for a semaphore and it is platform dependant
 */
typedef void * tPalSemaphoreHandle;


#define PAL_TICKS_PER_SECOND        100

#ifndef FEATURE_WLAN_INTEGRATED_SOC
typedef tANI_U32 tANI_TIMESTAMP;
#endif /* FEATURE_WLAN_INTEGRATED_SOC */
#endif
