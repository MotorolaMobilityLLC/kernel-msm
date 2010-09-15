/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
perf.h

DESCRIPTION: Reads and writes the performance monitoring registers in the ARM
by using the MRC and MCR instructions.
*/
#ifndef PERF_H
#define PERF_H
extern unsigned long perf_get_cycles(void);
extern void perf_set_count1(unsigned long val);
extern void perf_set_count0(unsigned long val);
extern unsigned long perf_get_count1(void);
extern unsigned long perf_get_count0(void);
extern unsigned long  perf_get_ctrl(void);
extern void perf_set_ctrl(void);
extern void perf_set_ctrl_with(unsigned long v);
extern void perf_enable_counting(void);
extern void perf_disable_counting(void);
extern void perf_set_divider(int d);
extern unsigned long perf_get_overflow(void);
extern void perf_clear_overflow(unsigned long bit);
extern void perf_export_event(unsigned long bit);
extern void perf_reset_counts(void);
extern int perf_set_event(unsigned long index, unsigned long val);
extern unsigned long perf_get_count(unsigned long index);
extern void perf_set_cycles(unsigned long c);

extern void pm_stop_all(void);
extern void l2_pm_stop_all(void);
extern void pm_start_all(void);
extern void l2_pm_start_all(void);
extern void pm_reset_all(void);
extern void l2_pm_reset_all(void);
extern void pm_set_event(unsigned long monitorIndex, unsigned long eventIndex);
extern void l2_pm_set_event(unsigned long monitorIndex,
	unsigned long eventIndex);
extern unsigned long pm_get_count(unsigned long monitorIndex);
extern unsigned long l2_pm_get_count(unsigned long monitorIndex);
extern unsigned long pm_get_cycle_count(void);
extern unsigned long l2_pm_get_cycle_count(void);
extern char *pm_find_event_name(unsigned long index);
extern  void pm_set_local_iu(unsigned long events);
extern  void pm_set_local_xu(unsigned long events);
extern  void pm_set_local_su(unsigned long events);
extern  void pm_set_local_l2(unsigned long events);
extern  void pm_set_local_vu(unsigned long events);
extern  void pm_set_local_bu(unsigned long events);
extern  void pm_set_local_cb(unsigned long events);
extern  void pm_set_local_mp(unsigned long events);
extern  void pm_set_local_sp(unsigned long events);
extern  void pm_set_local_scu(unsigned long events);
extern void pm_initialize(void);
extern void pm_deinitialize(void);
extern void l2_pm_initialize(void);
extern void l2_pm_deinitialize(void);
extern void pm_free_irq(void);
extern void l2_pm_free_irq(void);

extern int per_process_perf_init(void);
extern void per_process_perf_exit(void);
int per_process_read(char *page, char **start, off_t off, int count,
   int *eof, void *data);
int per_process_write_hex(struct file *file, const char *buff,
    unsigned long cnt, void *data);
int per_process_read_decimal(char *page, char **start, off_t off, int count,
   int *eof, void *data);
int per_process_write_dec(struct file *file, const char *buff,
    unsigned long cnt, void *data);
void perfmon_register_callback(void);
void _per_process_switch(unsigned long oldPid, unsigned long newPid);
extern unsigned int pp_loaded;
extern atomic_t pm_op_lock;
#endif /*PERF_H*/
