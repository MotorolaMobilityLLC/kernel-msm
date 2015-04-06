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

#ifdef WIN_MOBILE7
#include <ntddk.h>
#endif

#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#define ATH_MODULE_NAME hif
#include "a_debug.h"

#include "targaddrs.h"
#include "hif.h"
#include "if_ath_sdio.h"
#include "regtable.h"

#define CPU_DBG_SEL_ADDRESS                      0x00000483
#define CPU_DBG_ADDRESS                          0x00000484

/* set the window address register (using 4-byte register access ).
 * This mitigates host interconnect issues with non-4byte aligned bus requests, some
 * interconnects use bus adapters that impose strict limitations.
 * Since diag window access is not intended for performance critical operations, the 4byte mode should
 * be satisfactory even though it generates 4X the bus activity.  */
static A_STATUS ar6000_SetAddressWindowRegister(HIF_DEVICE *hifDevice, A_UINT32 RegisterAddr, A_UINT32 Address)
{
    A_STATUS status;
    static A_UINT32 address;

    address = Address;

/*AR6320 use new window register access procedure,not need confuse operation as below*/
#if 0
    for (i = 1; i <= 3; i++) {
            /* fill the buffer with the address byte value we want to hit 4 times*/
        addrValue[0] = ((A_UINT8 *)&Address)[i];
        addrValue[1] = addrValue[0];
        addrValue[2] = addrValue[0];
        addrValue[3] = addrValue[0];

            /* hit each byte of the register address with a 4-byte write operation to the same address,
             * this is a harmless operation */
        status = HIFReadWrite(hifDevice,
                              RegisterAddr+i,
                              addrValue,
                              4,
                              HIF_WR_SYNC_BYTE_FIX,
                              NULL);
        if (status != A_OK) {
            break;
        }
    }

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write initial bytes of 0x%x to window reg: 0x%X \n",
            Address, RegisterAddr));
        return status;
    }
#endif
/*AR6320,just write the 4-byte address to window register*/
    status = HIFReadWrite(hifDevice,
                          RegisterAddr,
                          (A_UCHAR *)(&address),
                          4,
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write 0x%x to window reg: 0x%X \n",
            Address, RegisterAddr));
        return status;
    }

    return A_OK;
}


/*
 * Read from the AR6000 through its diagnostic window.
 * No cooperation from the Target is required for this.
 */
A_STATUS
HIFDiagReadAccess(HIF_DEVICE *hifDevice, A_UINT32 address, A_UINT32 *data)
{
    A_STATUS status;
    static A_UINT32 readvalue;

    if(address&0x03)
    {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("[%s]addr is not 4 bytes align.addr[0x%08x]\n",__func__, address));
        return A_BAD_ADDRESS;
    }

        /* set window register to start read cycle */
    status = ar6000_SetAddressWindowRegister(hifDevice,
                                             WINDOW_READ_ADDR_ADDRESS,
                                             address);

    if (status != A_OK) {
        return status;
    }

        /* read the data */
    status = HIFReadWrite(hifDevice,
                          WINDOW_DATA_ADDRESS,
                          (A_UCHAR *)&readvalue,
                          sizeof(A_UINT32),
                          HIF_RD_SYNC_BYTE_INC,
                          NULL);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot read from WINDOW_DATA_ADDRESS\n"));
        return status;
    }

    *data = readvalue;
    return status;
}


/*
 * Write to the AR6000 through its diagnostic window.
 * No cooperation from the Target is required for this.
 */
A_STATUS  HIFDiagWriteAccess(HIF_DEVICE *hifDevice, A_UINT32 address, A_UINT32 data)
{
    A_STATUS status;
    static A_UINT32 writeValue;

    if(address&0x03)
    {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("[%s]addr is not 4 bytes align.addr[0x%08x]\n",__func__, address));
        return A_BAD_ADDRESS;
    }

    writeValue = data;

        /* set write data */
    status = HIFReadWrite(hifDevice,
                          WINDOW_DATA_ADDRESS,
                          (A_UCHAR *)&writeValue,
                          sizeof(A_UINT32),
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write 0x%x to WINDOW_DATA_ADDRESS\n", data));
        return status;
    }

        /* set window register, which starts the write cycle */
    return ar6000_SetAddressWindowRegister(hifDevice,
                                           WINDOW_WRITE_ADDR_ADDRESS,
                                           address);
}

/*
 * Write a block data to the AR6000 through its diagnostic window.
 * This function may take some time.
 * No cooperation from the Target is required for this.
 */
A_STATUS
HIFDiagWriteMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data, int nbytes)
{
    A_STATUS status;
    A_INT32 i;
    A_UINT32 tmp_data;

    if((address&0x03) || (nbytes&0x03))
    {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("[%s]addr or length is not 4 bytes align.addr[0x%08x] len[0x%08x]\n",__func__, address, nbytes));
        return A_BAD_ADDRESS;
    }

    for(i=0;i<nbytes;i+=4)
    {
        tmp_data = data[i] | (data[i+1]<<8) | (data[i+2]<<16) | (data[i+3]<<24);
        status = HIFDiagWriteAccess(hif_device, address + i , tmp_data);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Diag Write mem failed.addr[0x%08x] value[0x%08x]\n", address + i , tmp_data));
            return status;
        }
    }

    return A_OK;
}

/*
 * Reaad a block data to the AR6000 through its diagnostic window.
 * This function may take some time.
 * No cooperation from the Target is required for this.
 */
A_STATUS
HIFDiagReadMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data, int nbytes)
{
    A_STATUS status;
    A_INT32 i;
    A_UINT32 tmp_data;

    if((address&0x03) || (nbytes&0x03))
    {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("[%s]addr or length is not 4 bytes align.addr[0x%08x] len[0x%08x]\n",__func__, address, nbytes));
        return A_BAD_ADDRESS;
    }

    for(i=0;i<nbytes;i+=4)
    {
        status = HIFDiagReadAccess(hif_device, address + i , &tmp_data);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Diag Write mem failed.addr[0x%08x] value[0x%08x]\n", address + i , tmp_data));
            return status;
        }
        data[i] = tmp_data & 0xff;
        data[i+1] = tmp_data>>8 & 0xff;
        data[i+2] = tmp_data>>16 & 0xff;
        data[i+3] = tmp_data>>24 & 0xff;
    }

    return A_OK;
}

/* TODO .. the following APIs are only available on register-based HIFs where the CPU_DBG_SEL_ADDRESS
 * register is available */

A_STATUS
ar6k_ReadTargetRegister(HIF_DEVICE *hifDevice, int regsel, A_UINT32 *regval)
{
    A_STATUS status;
    A_UCHAR vals[4];
    A_UCHAR register_selection[4];

    register_selection[0] = register_selection[1] = register_selection[2] = register_selection[3] = (regsel & 0xff);
    status = HIFReadWrite(hifDevice,
                          CPU_DBG_SEL_ADDRESS,
                          register_selection,
                          4,
                          HIF_WR_SYNC_BYTE_FIX,
                          NULL);

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write CPU_DBG_SEL (%d)\n", regsel));
        return status;
    }

    status = HIFReadWrite(hifDevice,
                          CPU_DBG_ADDRESS,
                          (A_UCHAR *)vals,
                          sizeof(vals),
                          HIF_RD_SYNC_BYTE_INC,
                          NULL);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot read from CPU_DBG_ADDRESS\n"));
        return status;
    }

    *regval = vals[0]<<0 | vals[1]<<8 | vals[2]<<16 | vals[3]<<24;

    return status;
}

void
ar6k_FetchTargetRegs(HIF_DEVICE *hifDevice, A_UINT32 *targregs)
{
    int i;
    A_UINT32 val;

    for (i=0; i<AR6003_FETCH_TARG_REGS_COUNT; i++) {
        val=0xffffffff;
        (void)ar6k_ReadTargetRegister(hifDevice, i, &val);
        targregs[i] = val;
    }
}


