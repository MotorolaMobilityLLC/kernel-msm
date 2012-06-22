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
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file schDebug.cc contains some debug functions.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 * 
 */


#include "schDebug.h"

void schLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...) 
{
#ifdef WLAN_DEBUG
    // Verify against current log level
    if ( loglevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( SIR_SCH_MODULE_ID )] )
        return;
    else
    {
        va_list marker;

        va_start( marker, pString );     /* Initialize variable arguments. */

        logDebug(pMac, SIR_SCH_MODULE_ID, loglevel, pString, marker);
        
        va_end( marker );              /* Reset variable arguments.      */
    }
#endif
}

#if (WNI_POLARIS_FW_PRODUCT==AP)
#ifdef WMM_SA

#include "schClass.h"
#include "schDebug.h"


void dumpSchedule(tANI_U8 index)
{
  tpSchSchedule s = &schedule[index];

  PELOG2(schLog(pMac, LOG2, FL("============== SCHEDULE %d ===============\n"), index);)
  dumpSchedule(s);
}

void schClass::dumpSchedule(tpSchSchedule s)
{
 PELOG2(schLog(pMac, LOG2, FL("\ttotTxop %d totInst %d curInst %d\n"), 
         s->totTxopLo+0x100*s->totTxopHi, s->totInst, s->curInst);)
  for(tANI_U32 i=0; i<s->totInst; i++)
    {
      PELOG2(schLog(pMac, LOG2, FL("\t[%d] "), i);)
      dumpInstruction(&s->inst[i]);
    }
  PELOG2(schLog(pMac, LOG2, FL("\t+++++++++++++++++++++++++++++++++++++++++++\n"));)
}

void dumpInstruction(tpSchInstruction t)
{
  if (t->type == SCH_INST_DATA)
   PELOG2(schLog(pMac, LOG2, "DATA (%d,%d) txop %d feedback %d\n", 
                 t->staidLo+0x10*t->staidHi, t->pri, 
                 t->txopLo+0x100*t->txopHi, t->feedback);)
  else
   PELOG2(schLog(pMac, LOG2, "POLL (%d,%d) txop %d feedback %d\n", 
                 t->staidLo+0x10*t->staidHi, t->pri, 
                 t->txopLo+0x100*t->txopHi, t->feedback);)
}

void dumpQueueSizes()
{
  PELOG3(schLog(pMac, LOG3, FL("------- QUEUE SIZES [sta][pri][DL(0)/UL(1)] -------\n"));)
  for(int i=0; i<256; i++)
    for(int j=0; j<8; j++)
      for(int k=0; k<2; k++)
      {
         if (queue[i][j][k].packets > 0)
            PELOG3(schLog(pMac, LOG3, FL("\tQueueLength[%d][%d][%d] = %d packets %d txop (serviced %d)\n"), 
                          i, j, k,
                          queue[i][j][k].packets, queue[i][j][k].txop, queue[i][j][k].serviced);)
      }
}

void printQosClass(tANI_U8 classId)
{
  schQoSClass *q = &qosClass[classId];

  if (q->activeNext == NULL)
    PELOG3(schLog(pMac, LOG3, FL("printQosClass[%d] : Active list empty\n"), classId);)
  else
    {
      tpSchQueueState ptr = q->activeNext->next;
      tANI_U16 i;
      for(i=1; ptr != q->activeNext; ptr = ptr->next, i++);
     PELOG3(schLog(pMac, LOG3, FL("printQosClass[%d] : Active Qs %d nextPtr %x prevPtr %x creditsRem %d\n"), 
                   classId, i, (tANI_U32) q->activeNext, (tANI_U32) q->activePrev, q->creditsRemaining);)
    }
}

#endif /* WMM_SA */
#endif // (WNI_POLARIS_FW_PRODUCT==AP)

// --------------------------------------------------------------------
