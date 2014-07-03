/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Hitesh K. Patel <hitesh.k.patel@intel.com>
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *    Jari Luoma-aho  <jari.luoma-aho@intel.com>
 *    Jari Nippula    <jari.nippula@intel.com>
 *
 */

#if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE)

#include <stddef.h>
#include <linux/ctype.h>

#include "utilf.h"

#include "gburst.h"
#include <gburst_hw.h>

#define MAX_NUM_CORES 2
#define MAX_NUM_COUNTERS 8


/**
 * struct pcel_s -- Performance counter element.
 * Data for each counter.
 */
struct pcel_s {
	/* for calculating delta */
	uint32_t last_value;
	/* pce_time_stamp - gpu clock divided by 16. */
	uint32_t pce_time_stamp;
};

struct gburst_stats_data_s {
	/**
	 * gsd_stats_initialized - Set to !0 when initialization has been
	 * completed.
	 */
	int           gsd_stats_initialized;

	int           gsd_gpu_freq_mhz;
	unsigned int  gsd_num_cores;
	unsigned int  gsd_num_counters_hw;
	unsigned int  gsd_first_active_counter;
	unsigned int  gsd_last_active_counter;
	unsigned int  gsd_num_counters;

	/* gsd_pcd - performance counter data */
	struct pcel_s gsd_pcd[MAX_NUM_CORES][MAX_NUM_COUNTERS];
};


/* Variables visible at file scope */

struct gburst_stats_data_s gsdat;


/* Forward references */
static int gburst_stats_init(void);


/**
 * gburst_stats_initialization_complete() - Attempt initialization,
 * if not already done.
 *
 * Function return value:  1 if initialized, 0 if not.
 */
static inline int gburst_stats_initialization_complete(void)
{
	if (!gsdat.gsd_stats_initialized)
		return gburst_stats_init();
	return 1;
}


/**
 * gpu_init_perf_counters() - Initialize local data that describes the
 * performance counters.  Causes old history to be forgotten.
 */
static void gpu_init_perf_counters(void)
{

	/* All zero, except for counter limit max. */
	memset(&gsdat.gsd_pcd, 0, sizeof(gsdat.gsd_pcd));

}


/**
 * update_perf_counter_value() - Store one hw counter into database.
 * @ix_core - gpu core index
 * @ndx     - our counter index
 * @value   - raw value of counter, as read from device.
 * @time_stamp - clocks divided by 16
 * @counters_storable - non-zero to store and use counter value.
 *                  Ancilliary actions may be taken regardless.
 * Called from function gburst_stats_gfx_hw_perf_record.
 */
static uint32_t update_perf_counter_value(int ix_core, int ndx,
	uint32_t value, uint32_t time_stamp, uint32_t counters_storable)
{
	struct pcel_s *pce;
	int value_index;
	uint32_t numerator;
	uint32_t denominator;
	uint32_t utilization;
	uint32_t counter_coeff;

	numerator = 0;
	denominator = 0;
	utilization = 0;
	pce = &gsdat.gsd_pcd[ix_core][ndx];

	if (counters_storable) {
		/* Update counter data history only when periodic event arrives */
		counter_coeff = gburst_hw_inq_counter_coeff(ndx);
		if ((time_stamp > pce->pce_time_stamp)
			&& (counter_coeff != 0)
			&& (value > pce->last_value)) {

			/*  Calculate counter utilization percentage */
			numerator = 100*(value - pce->last_value);
			denominator = counter_coeff * (time_stamp -
				pce->pce_time_stamp);
			utilization = (uint32_t) (numerator / denominator);
		}
		pce->pce_time_stamp = time_stamp;
		pce->last_value = value;
	} else if (time_stamp < pce->pce_time_stamp ||
				value < pce->last_value) {
		/* counter reset or roll over between periodic events
		=> start over with new counter values */
		pce->pce_time_stamp = time_stamp;
		pce->last_value = value;
	}

	if (gburst_debug_msg_on)
		printk(KERN_ALERT "GBUTIL: %d %d %d\n",
			ix_core, ndx, utilization);

	return utilization;
}


/**
 * gburst_stats_gfx_hw_perf_counters_set() -- Specify counter visibility.
 * @buf: A null terminated string that specifies a configuration of counters
 * to be used for utilization computations.
 *
 * Function return value: < 0 for error, otherwise 0.
 *
 * This function allows changing which counters are visible.
 *
 * This function scans counter specifications from a null-terminated string
 * which is expected to be from write to a /proc file (with explicit null
 * termination added by this function's caller.
 *
 * The input string specifies which counters are to be visible as
 * whitespace-separated (e.g. space,tab,newline) groups of: %u:%u:%u:%u:%u:%u
 * which correspond to counter:group:bit:coeff:cntrbit:summux.
 * then assigns the values into data structures for all cores.
 * These per-counter values are:
 * 1. counter - An index into this module's counter data arrays.
 * 2. group -- The hardware "group" from which this counter is taken.
 * 3. bit   -- The hardware bit (for this group) that selects this counter.
 * 4. coeff -- A counter specific increment value.
 * 5. cntrbit -- Counter bits, MSB of 16-bits group to be output to counter mux
 * 6. summux -- SumMux register value, 1=Sum, 0=Mux. Selected counter bits are
 *                either summed or the MSB is counted to counter
 * Example input string: "1:1:0:16:3:1   6:0:24:32:0:0"
 */
int gburst_stats_gfx_hw_perf_counters_set(const char *buf)
{
	int i;
	int sts;
	uint32_t counter;
	uint32_t group;
	uint32_t bit;
	uint32_t coeff;
	unsigned int ix_core;
	uint32_t cntrbits;
	uint32_t summux;	
	const int svix_counter = 0;
	const int svix_group = 1;
	const int svix_bit = 2;
	const int svix_coeff = 3;
	const int nitems = 6;
	const int svix_cntrbits = 4;
	const int svix_summux = 5;
	int sval[nitems];
	const char *pstr;

	if (!gburst_stats_initialization_complete())
		return -EINVAL;

	pstr = buf;
	for (;;) {
		int nchrs = 0;

		while (isspace(*pstr))
			pstr++;

		/**
		 * The "%n" transfer will affect the return value, and will
		 * only be done if there are enough characters in the input
		 * string (e.g., terminating newline) to reach it.
		 */
		i = sscanf(pstr, " %u:%u:%u:%u:%u:%u%n",
			sval+0, sval+1, sval+2, sval+3, sval+4, sval+5, &nchrs);
		pstr += nchrs;
		if ((*pstr != '\0') && !isspace(*pstr))
			return -EINVAL;

		if (i < 6)
			return -EINVAL;

		counter = sval[svix_counter];
		group = sval[svix_group];
		bit = sval[svix_bit];
		coeff = sval[svix_coeff];
		cntrbits = sval[svix_cntrbits];
		summux = sval[svix_summux];

		printk(KERN_INFO "#:%u, g:%u, b:%u, c:%u, cb:%u, s:%u\n",
			counter, group, bit, coeff, cntrbits, summux);

		/* Must call to gburst_hw_set_perf_status_periodic, below. */
		sts = gburst_hw_set_counter_id(counter, group, bit, cntrbits, summux);
		if (sts < 0)
			return sts;

		for (ix_core = 0; ix_core < gsdat.gsd_num_cores; ix_core++) {
			gburst_hw_set_counter_coeff(counter, coeff);
		}

		/*
		 * When all characters are handled continue to call
		 * gburst_hw_set_perf_status_periodic()
		 */
		if (*pstr == '\n')
			break;

	}

	return gburst_hw_set_perf_status_periodic(1);
}


/**
 * gburst_stats_gfx_hw_perf_record() - Record performance info.
 *
 * Function return value: Utilization value (0-100) or
 * < 0 if error (indicating not inited).
 */
int gburst_stats_gfx_hw_perf_record()
{
	int icx;            /* counter index */
	int i;
	int ndx;
	int sts;
	struct pcel_s *pce;
	unsigned int ix_core;
	uint32_t curr_util;
	uint32_t sgx_util;
	int ix_roff;
	int ix_woff;

	if (!gburst_stats_initialization_complete())
		return -EINVAL;

	if (gburst_hw_is_access_denied())
		return -EINVAL;

	curr_util = 0;
	sgx_util = 0;

	/**
	 * The incoming data is in a circular buffer, with ix_roff the
	 * index for reading (which this function does) and ix_woff the index
	 * for writing.
	 * Index ix_roff will be updated as processing is done.  The value
	 * for index ix_woff captured here is used for determination of
	 * buffer empty, as it would be undesirable to use the updated write
	 * index and thereby possibly continue processing around the buffer
	 * without end.  The buffer is empty when ix_roff == ix_woff.
	 */
	sts = gburst_hw_mutex_lock();
	if (sts < 0)
		return sts;

	sts = gburst_hw_perf_data_get_indices(&ix_roff, &ix_woff);

	if (sts < 0) {
		gburst_hw_mutex_unlock();
		return sts;
	}

	sts = gburst_hw_inq_num_counters(&gsdat.gsd_first_active_counter,
		&gsdat.gsd_last_active_counter);
	if (sts < 0) {
		gburst_hw_mutex_unlock();
		return sts;
	}

	if (ix_woff == ix_roff) {
		gburst_hw_mutex_unlock();
		return 0; /* return with zero utilization */
	}

	if (gburst_debug_msg_on)
		printk(KERN_INFO "START_UTIL_CALC %d %d\n", ix_roff, ix_woff);

	while (ix_woff != ix_roff) {
		/* Time stamp is gpu clock divided by 16. */
		uint32_t time_stamp;
		uint32_t counters_storable;
		uint32_t *pdat_base;
		uint32_t *pdat;

		/**
		 * Get information a single entry in the circular buffer, which
		 * includes information such as timestamp and all counter
		 * values.
		 */
		sts = gburst_hw_perf_data_get_data(&time_stamp,
			&counters_storable, &pdat_base);
		if (sts < 0) {
			gburst_hw_mutex_unlock();
			return sts;
		}

		/**
		 * For each active counter, store its value in the database
		 * along with the current timestamp.
		 */
		pdat = pdat_base;
		/* For each core */
		for (ix_core = 0; ix_core < gsdat.gsd_num_cores;
			ix_core++, pdat += gsdat.gsd_num_counters_hw) {
			/* For each counter */
			curr_util = update_perf_counter_value(
				ix_core, gsdat.gsd_first_active_counter,
				pdat[gsdat.gsd_first_active_counter],
				time_stamp, counters_storable);
			if (sgx_util < curr_util)
				sgx_util = curr_util;

			curr_util = update_perf_counter_value(
				ix_core, gsdat.gsd_last_active_counter,
				pdat[gsdat.gsd_last_active_counter],
				time_stamp, counters_storable);
			if (sgx_util < curr_util)
				sgx_util = curr_util;

		}
		sts = gburst_hw_perf_data_read_index_incr(&ix_roff);
		if (sts < 0) {
			gburst_hw_mutex_unlock();
			return sts;
		}
	}

	gburst_hw_mutex_unlock();

	return sgx_util;
}


/**
 * gburst_stats_gfx_hw_perf_counters_to_string() -- output values to string.
 * @ix_in: Initial index with buf at which to store string.
 * @buf: A buffer to hold the output string.
 * @buflen: Length of buf.
 *
 * Output string is guaranteed to be null-terminated.
 *
 * Function return value:
 * negative error code or number of characters in output string (even
 * if only part of the string would fit in buffer).
 */
int gburst_stats_gfx_hw_perf_counters_to_string(int ix_in, char *buf,
	size_t buflen)
{
	int ix;
	int ndx;
	int sts;
	int ix_core;
	int ctr_grp;
	int ctr_bit;
	int cntrbits;
	int summux;

	if (!gburst_stats_initialization_complete())
		return -EINVAL;

	ix = ix_in;

	ix = ut_isnprintf(ix, buf, buflen, "cix   grp    bit   coeff cbits sum\n");
	ix = ut_isnprintf(ix, buf, buflen, "==================================\n");

	for (ix_core = 0; ix_core < gsdat.gsd_num_cores; ix_core++) {
		ix = ut_isnprintf(ix, buf, buflen, "Core %d:\n", ix_core);
		for (ndx = 0; ndx < gsdat.gsd_num_counters; ndx++) {
			sts = gburst_hw_inq_counter_id(ndx, &ctr_grp, &ctr_bit,
				&cntrbits, &summux);
			if (sts < 0)
				return sts;

			if (ndx == gsdat.gsd_first_active_counter ||
				ndx == gsdat.gsd_last_active_counter)
				ix = ut_isnprintf(ix, buf, buflen,
				"%u:  %3u     %2u     %5u   %3u  %2u  *\n", ndx,
				ctr_grp, ctr_bit,
				gburst_hw_inq_counter_coeff(ndx),
				cntrbits, summux);
			else
				ix = ut_isnprintf(ix, buf, buflen,
				"%u:  %3u     %2u     %5u   %3u  %2u\n", ndx,
				ctr_grp, ctr_bit,
				gburst_hw_inq_counter_coeff(ndx),
				cntrbits, summux);

		}
	}

	return ix;
}



/**
 * gburst_stats_gpu_freq_mhz_info() - Set gpu frequency.
 * @freq_mhz: gpu frequency in MHz.
 *
 * The specified gpu frequency is used to update local data and potentially
 * the graphics driver.
 */
int gburst_stats_gpu_freq_mhz_info(int freq_mhz)
{
	gsdat.gsd_gpu_freq_mhz = freq_mhz;

	return gburst_hw_gpu_freq_mhz_info(freq_mhz);
}


/**
 * gburst_stats_cleanup_gfx_load_data() -- clean-up gpu load information
 * storage from all data.
 */
void gburst_stats_cleanup_gfx_load_data(void)
{
	gburst_hw_flush_buffer();
}


/**
 * gburst_stats_init() -- Attempt initialization.
 * Function return value:  1 if initialized, 0 if not.
 */
static int gburst_stats_init(void)
{
	int ncores;
	int sts;

	sts = gburst_hw_init();
	if (sts <= 0)
		return 0;

	ncores = gburst_hw_inq_num_cores();

	if (ncores > MAX_NUM_CORES) {
		printk(KERN_ALERT
			"%s: warning: %u cores present, limiting to %u\n",
			__FILE__, gsdat.gsd_num_cores, MAX_NUM_CORES);
		gsdat.gsd_num_cores = MAX_NUM_CORES;
	} else {
		gsdat.gsd_num_cores = ncores;
	}

	gsdat.gsd_num_counters_hw =
	gburst_hw_inq_num_counters(&gsdat.gsd_first_active_counter,
		&gsdat.gsd_last_active_counter);

	if (gsdat.gsd_num_counters_hw > MAX_NUM_COUNTERS) {
		printk(KERN_ALERT
			"%s: warning: %u counters present, limiting to %u\n",
			__FILE__, gsdat.gsd_num_counters, MAX_NUM_COUNTERS);
		gsdat.gsd_num_counters = MAX_NUM_COUNTERS;
	} else {
		gsdat.gsd_num_counters = gsdat.gsd_num_counters_hw;
	}

	gpu_init_perf_counters();

	sts = gburst_hw_set_perf_status_periodic(1);
	if (sts < 0)
		return 0;

	gsdat.gsd_stats_initialized = 1;

	return 1;
}
#endif /* if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE) */
