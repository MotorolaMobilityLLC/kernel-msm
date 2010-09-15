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
*per-axi
*DESCRIPTION
*Header File for Functions related to AXI bus performance counter manipulations.
*/

#ifndef __PER_AXI_H__
#define __PER_AXI_H__
unsigned long pm_get_axi_cycle_count(void);
unsigned long pm_get_axi_evt0_count(void);
unsigned long pm_get_axi_evt1_count(void);
unsigned long pm_get_axi_evt2_count(void);
unsigned long pm_get_axi_ten_min_count(void);
unsigned long pm_get_axi_ten_max_count(void);
unsigned long pm_get_axi_ten_total_count(void);
unsigned long pm_get_axi_ten_last_count(void);

unsigned long get_axi_sel_reg0(void);
unsigned long get_axi_sel_seg1(void);
unsigned long get_axi_ten_sel_reg(void);
unsigned long get_axi_valid(void);
unsigned long get_axi_enable(void);
unsigned long get_axi_clear(void);

void pm_axi_clear_cnts(void);
void pm_axi_update_cnts(void);

void pm_axi_init(void);
void pm_axi_start(void);
void pm_axi_update(void);
void pm_axi_disable(void);
void pm_axi_enable(void);

struct perf_mon_axi_cnts{
  unsigned long long cycles;
  unsigned long long cnt0;
  unsigned long long cnt1;
  unsigned long long tenure_total;
  unsigned long long tenure_min;
  unsigned long long tenure_max;
  unsigned long long tenure_last;
};

struct perf_mon_axi_data{
  struct proc_dir_entry *proc;
  unsigned long enable;
  unsigned long clear;
  unsigned long valid;
  unsigned long sel_reg0;
  unsigned long sel_reg1;
  unsigned long ten_sel_reg;
  unsigned long refresh;
};

extern struct perf_mon_axi_data pm_axi_info;
extern struct perf_mon_axi_cnts axi_cnts;

void pm_axi_set_proc_entry(char *name, unsigned long *var,
	struct proc_dir_entry *d, int hex);
void pm_axi_get_cnt_proc_entry(char *name, struct perf_mon_axi_cnts *var,
	struct proc_dir_entry *d, int hex);

#endif
