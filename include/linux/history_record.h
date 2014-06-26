#ifndef _LINUX_HISTORY_RECORD_H
#define _LINUX_HISTORY_RECORD_H

#ifdef __KERNEL__

#define SAVED_HISTORY_MAX 250

struct saved_history_record {
	unsigned long long ts;
	unsigned int type;
	union {
		struct {
			unsigned int HostIrqCountSample;
			unsigned int InterruptCount;
		} sgx; /*type = 1,2;*/

		struct {
			unsigned int pipe_nu;
			unsigned int pipe_stat_val;
		} pipe; /*type = 3;*/

		unsigned long msvdx_stat; /*type = 4;*/
		unsigned long vdc_stat; /*type = 5;*/
		unsigned long value; /*type = 0;*/
	} record_value;
};

extern struct saved_history_record *get_new_history_record(void);
extern void interrupt_dump_history(void);

#endif

#endif
