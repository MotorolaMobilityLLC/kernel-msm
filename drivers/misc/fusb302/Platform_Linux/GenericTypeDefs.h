/*******************************************************************

                  Generic Type Definitions

********************************************************************
 FileName:        GenericTypeDefs.h
 Dependencies:    Linux kernel
 Processor:       ARM, ARM64, x86, x64
 Compiler:        GCC 4.8.3 or greater
********************************************************************/

#ifndef _FUSB30X_GENERIC_TYPE_DEFS_H_
#define _FUSB30X_GENERIC_TYPE_DEFS_H_

#define FSC_HOSTCOMM_BUFFER_SIZE    64	// Length of the hostcomm buffer, needed in both core and platform

#if defined(PLATFORM_LINUX)

/* Specify an extension for GCC based compilers */
#if defined(__GNUC__)
#define __EXTENSION __extension__
#else
#define __EXTENSION
#endif

#if !defined(__PACKED)
#define __PACKED
#endif

/* get linux-specific type definitions (NULL, size_t, etc) */
#include <linux/types.h>

#ifndef U8_MAX
#define U8_MAX  ((__u8)~0U)
#endif

#ifndef VOID
typedef void VOID;
#endif // VOID

#if !defined(BOOL) && !defined(FALSE) && !defined(TRUE)
typedef enum _BOOL { FALSE = 0, TRUE } BOOL;	/* Undefined size */
#endif // !BOOL && !FALSE && !TRUE

#ifndef BIT
typedef enum _BIT { CLEAR = 0, SET } BIT;
#endif // BIT

/* INT is processor specific in length may vary in size */
#ifndef INT
typedef __s32 INT;		// 32-bit signed
#endif // INT

#ifndef INT8
typedef __s8 INT8;		// 8-bit signed
#endif // INT8

#ifndef INT16
typedef __s16 INT16;		// 16-bit signed
#endif // INT16

#ifndef INT32
typedef __s32 INT32;		// 32-bit signed
#endif // INT32

#ifndef INT64
typedef __s64 INT64;		// 64-bit signed
#endif // INT64

/* UINT is processor specific in length may vary in size */
#ifndef UINT
typedef __u32 UINT;		// 32-bit unsigned
#endif // UINT

#ifndef UINT8
typedef __u8 UINT8;		// 8-bit unsigned
#endif // UINT8

#ifndef UINT16
typedef __u16 UINT16;		// 16-bit unsigned
#endif // UINT16

#ifndef UINT32
typedef __u32 UINT32;		// Other name for 32-bit unsigned
#endif // UINT32

#ifndef UINT64
typedef __u64 UINT64;		// 64-bit unsigned
#endif // UINT64

#undef __EXTENSION

#endif

#endif /* _FUSB30X_GENERIC_TYPE_DEFS_H_ */
