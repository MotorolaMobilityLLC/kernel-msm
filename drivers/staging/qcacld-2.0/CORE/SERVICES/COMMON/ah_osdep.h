/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


#ifndef _ATH_AH_OSDEP_H_

#define _ATH_AH_OSDEP_H_

/*

 * Atheros Hardware Access Layer (HAL) OS Dependent Definitions.

 */



/*

 * We're releasing binary HAL as a stand-alone module, so we don't

 * need to worry about compatibilities regarding calling convention.

 * By default, Windows uses PASCAL convention.

 */

#ifndef __ahdecl

#define __ahdecl

#endif



#define __va_list   va_list

#define OS_INLINE   __inline

#ifndef inline

#define inline      __inline

#endif

//typedef unsigned int        uintptr_t;

//typedef unsigned long dma_addr_t;

typedef int       bus_space_tag_t;

typedef char *    bus_space_handle_t;

typedef u_int32_t bus_addr_t;



typedef void* HAL_SOFTC;

typedef bus_space_handle_t HAL_BUS_HANDLE;

typedef void* HAL_ADAPTER_HANDLE;

typedef u_int32_t HAL_BUS_ADDR;         /* XXX architecture dependent */

typedef bus_space_tag_t		HAL_BUS_TAG;

/*

 * Atomic interface

 */

typedef u_int32_t os_atomic_t;


/* no-ops */
#define HAL_USE_INTERSPERSED_READS
#define HAL_NO_INTERSPERSED_READS

//#define OS_ATOMIC_READ(_patomic_arg)           (*(_patomic_arg))

//#define OS_ATOMIC_SET(_patomic_arg, v)         InterlockedExchange(_patomic_arg, (v))

//#define OS_ATOMIC_ADD(v, _patomic_arg)         InterlockedExchangeAdd(_patomic_arg, (v))

#define OS_ATOMIC_INC(_patomic_arg)            ((*_patomic_arg)++)

#define OS_ATOMIC_DEC(_patomic_arg)            ((*_patomic_arg)--)

//#define OS_ATOMIC_DEC_AND_TEST(_patomic_arg)   (NdisInterlockedDecrement(_patomic_arg) == 0)



//#define OS_CMPXCHG(_m, _old, _new)             InterlockedCompareExchange(_m, _new, _old)



/*

 * Supported Bus types

 */

typedef enum ath_hal_bus_type {

   HAL_BUS_TYPE_PCI,
   HAL_BUS_TYPE_AHB,
   HAL_BUS_TYPE_SIM, /* simulator */
   HAL_BUS_TYPE_SDIO,
} HAL_BUS_TYPE;





/*

 * Bus to hal context handoff

 */

typedef struct hal_bus_context {

    HAL_BUS_TAG     bc_tag;

    HAL_BUS_HANDLE  bc_handle;

    HAL_BUS_TYPE    bc_bustype;

} HAL_BUS_CONTEXT;



#ifndef abs

#define abs(_a)       ( (_a) < 0 ? -(_a) : (_a) )

#endif



#define	__printflike(_a,_b) \
	__attribute__ ((__format__ (__printf__, _a, _b)))



struct ath_hal;

struct hal_reg_parm {

    u_int8_t             halPciePowerSaveEnable;             // Program Serdes; Use ASPM

    u_int8_t             halPcieL1SKPEnable;                 // Enable L1 SKP workaround

    u_int8_t             halPcieClockReq;

    u_int32_t            halPciePowerReset;

    u_int32_t            halPcieWaen;

    u_int32_t            halPcieDetach;

    u_int8_t             halPcieRestore;

    u_int8_t             halPllPwrSave;

    u_int8_t             htEnable;               // Enable/disable 11n mode

    u_int32_t            ofdmTrigLow;

    u_int32_t            ofdmTrigHigh;

    u_int32_t            cckTrigHigh;

    u_int32_t            cckTrigLow;

    u_int32_t            enableANI;

    u_int8_t             noiseImmunityLvl;

    u_int32_t            ofdmWeakSigDet;

    u_int32_t            cckWeakSigThr;

    u_int8_t             spurImmunityLvl;

    u_int8_t             firStepLvl;

    int8_t               rssiThrHigh;

    int8_t               rssiThrLow;

    u_int16_t            diversityControl;                // Enable/disable antenna diversity

    u_int16_t            antennaSwitchSwap;

    u_int32_t            forceBias;

    u_int32_t            forceBiasAuto;

    int                  calInFlash;

#ifdef ATH_SUPPORT_TxBF

    u_int8_t             CVTimeOut;

    u_int16_t            TxBFCtl;

#endif

};



/*

 * Byte order/swapping support.

 */

#define AH_LITTLE_ENDIAN    1234

#define AH_BIG_ENDIAN       4321



#ifdef MIPS32

#define AH_BYTE_ORDER AH_BIG_ENDIAN

#endif



#if AH_BYTE_ORDER == AH_BIG_ENDIAN

/*

 * This could be optimized but since we only use it for

 * a few registers there's little reason to do so.

 */

static OS_INLINE u_int32_t

__bswap32(u_int32_t _x)

{

    return ((u_int32_t)(

          (((const u_int8_t *)(&_x))[0]    ) |

          (((const u_int8_t *)(&_x))[1]<< 8) |

          (((const u_int8_t *)(&_x))[2]<<16) |

          (((const u_int8_t *)(&_x))[3]<<24))

    );

}



#ifndef	__BIG_ENDIAN__

#define	__BIG_ENDIAN__

#endif



#else

#define __bswap32(_x)   (_x)

#endif



/*

 * Register read/write; we assume the registers will always

 * be memory-mapped.  Note that register accesses are done

 * using target-specific functions when debugging is enabled

 * (AH_DEBUG) or we are explicitly configured this way.  The

 * latter is used on some platforms where the full i/o space

 * cannot be directly mapped.

 *

 * The hardware registers are native little-endian byte order.

 * Big-endian hosts are handled by enabling hardware byte-swap

 * of register reads and writes at reset.  But the PCI clock

 * domain registers are not byte swapped!  Thus, on big-endian

 * platforms we have to byte-swap thoese registers specifically.

 * Most of this code is collapsed at compile time because the

 * register values are constants.

 */



#if AH_BYTE_ORDER == AH_BIG_ENDIAN



#define _OS_REG_WRITE(_ah, _reg, _val) \
    *((volatile u_int32_t *)(AH_PRIVATE(_ah)->ah_sh + (_reg))) = (_val)

#define _OS_REG_READ(_ah, _reg) \
    *((volatile u_int32_t *)(AH_PRIVATE(_ah)->ah_sh + (_reg)))



#else /* _BYTE_ORDER == _LITTLE_ENDIAN */

#define __bswap32(_x)	(_x)

#define	_OS_REG_WRITE(_ah, _reg, _val)					\
    bus_space_write_4(AH_PRIVATE(_ah)->ah_st, AH_PRIVATE(_ah)->ah_sh, (_reg), (_val))

#define _OS_REG_READ(_ah, _reg) \
    ((u_int32_t) bus_space_read_4(AH_PRIVATE(_ah)->ah_st, AH_PRIVATE(_ah)->ah_sh, (_reg)))

#endif /* _BYTE_ORDER */


#ifndef ATH_SUPPORT_HTC

/* no-op for non-USB solutions */
#define ENABLE_REG_WRITE_BUFFER
#define DISABLE_REG_WRITE_BUFFER
#define OS_REG_WRITE_FLUSH(_ah)
#define HTC_SET_PS_STATE(_ah, _mode)

#else /* ATH_SUPPORT_HTC */

#define ENABLE_REG_WRITE_BUFFER \
    if (!AH_PRIVATE(ah)->ah_reg_write_buffer_flag) \
        AH_PRIVATE(ah)->ah_reg_write_buffer_flag = 1; \
    else \
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: Error:REG_WRITE_BUFFER " \
                 "had not been correctly disabled!\n", __func__);

#define DISABLE_REG_WRITE_BUFFER \
    if (AH_PRIVATE(ah)->ah_reg_write_buffer_flag) \
        AH_PRIVATE(ah)->ah_reg_write_buffer_flag = 0; \
    else \
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: Error:REG_WRITE_BUFFER " \
                 "had not been correctly disabled!\n", _func__);

#define OS_REG_WRITE_FLUSH(_ah) \
    if (AH_PRIVATE(_ah)->ah_reg_write_buffer_flag) { \
        _OS_REG_WRITE_FLUSH(_ah); \
    } else \
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: Error:REG_WRITE_BUFFER " \
                 "must be enabled!\n", _func__);

extern void ath_hal_wmi_ps_set_state(struct ath_hal *ah, u_int16_t mode);

#define HTC_SET_PS_STATE(_ah, _mode) \
    ath_hal_wmi_ps_set_state(_ah, _mode);

#endif /* ATH_SUPPORT_HTC */


#if defined(AH_DEBUG) || defined(AH_REGOPS_FUNC) || defined(AH_DEBUG_ALQ)

#define OS_REG_WRITE(_ah, _reg, _val)   ath_hal_reg_write(_ah, _reg, _val)
#define OS_REG_READ(_ah, _reg)      ath_hal_reg_read(_ah, _reg)

u_int32_t __ahdecl ath_hal_reg_read(struct ath_hal *, u_int32_t);
void __ahdecl ath_hal_reg_write(struct ath_hal *, u_int32_t, u_int32_t);

#else

#define OS_REG_WRITE(_ah, _reg, _val)   _OS_REG_WRITE(_ah, _reg, _val)
#define OS_REG_READ(_ah, _reg)      _OS_REG_READ(_ah, _reg)

#endif /* AH_DEBUG || AH_REGFUNC || AH_DEBUG_ALQ */


#ifdef AH_DEBUG_ALQ
extern  void __ahdecl OS_MARK(struct ath_hal *, u_int id, u_int32_t value);
#else
#define OS_MARK(_ah, _id, _v)
#endif


/*
 * Linux-specific attach/detach methods needed for module reference counting.
 *
 * XXX We can't use HAL_STATUS because the type isn't defined at this
 *     point (circular dependency); we wack the type and patch things
 *     up in the function.
 *
 * NB: These are intentionally not marked __ahdecl since they are
 *     compiled with the default calling convetion and are not called
 *     from within the HAL.
 */

typedef u_int32_t (*HAL_BUS_CONFIG_READER)(HAL_SOFTC sc, u_int32_t offset, void *pBuffer, u_int32_t length);

struct ath_hal_callback {
    /* Callback Functions */
    HAL_BUS_CONFIG_READER read_pci_config_space;
};


/*
 * osdev_t specific flags.
 */
#define	ATH_ATTACHED		0x0001	/* attach has succeeded */
#define	ATH_ENABLED		0x0002	/* chip is enabled */
#define	ATH_RXREFILL		0x0004	/* Refill the recv bufs */

#define	ATH_IS_ENABLED(osdev)	((osdev)->sc_flags & ATH_ENABLED)

#ifdef AR9100
extern void ath_hal_ahb_mac_reset(void);
extern void ath_hal_get_chip_revisionid(u_int32_t *);
#endif

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a)         (sizeof(a) / sizeof((a)[0]))
#endif

#endif /* _ATH_AH_OSDEP_H_ */
