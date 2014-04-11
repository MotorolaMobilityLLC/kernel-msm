/*
 * sec_debug.h
 *
 * header file supporting debug functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>

struct proc_dir_entry {
	unsigned int low_ino;
	umode_t mode;
	nlink_t nlink;
	kuid_t uid;
	kgid_t gid;
	loff_t size;
	const struct inode_operations *proc_iops;
	const struct file_operations *proc_fops;
	struct proc_dir_entry *next, *parent, *subdir;
	void *data;
	atomic_t count;		/* use count */
	atomic_t in_use;	/* number of callers into module in progress; */
			/* negative -> it's going away RSN */
	struct completion *pde_unload_completion;
	struct list_head pde_openers;	/* who did ->open, but not ->release */
	spinlock_t pde_unload_lock; /* proc_fops checks and pde_users bumps */
	u8 namelen;
	char name[];
};

extern void *restart_reason;
extern void pde_put(struct proc_dir_entry *pde);
extern struct inode *proc_get_inode(struct super_block *sb,
		struct proc_dir_entry *de);

/* Enable CONFIG_RESTART_REASON_DDR
   to use DDR address for saving restart reason
*/
#ifdef CONFIG_RESTART_REASON_DDR
extern void *restart_reason_ddr_address;
#endif

#ifdef CONFIG_SEC_DEBUG
extern int sec_debug_init(void);
extern int sec_debug_dump_stack(void);
extern void sec_debug_hw_reset(void);

extern struct uts_namespace init_uts_ns;
#ifdef CONFIG_TOUCHSCREEN_MMS252
extern void dump_tsp_log(void);
#endif
#ifdef CONFIG_SEC_PERIPHERAL_SECURE_CHK
extern void sec_peripheral_secure_check_fail(void);
#endif
#ifdef CONFIG_SEC_MISC
extern void drop_pagecache_sb(struct super_block *sb, void *unused);
extern void drop_slab(void);
#endif
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
		u32 frames);
extern void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
		u32 addr1);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
		void *p_events, void *p_system);
extern void sec_getlog_supply_kloginfo(void *klog_buf);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);
extern int sec_debug_is_enabled(void);
extern int sec_debug_is_enabled_for_ssr(void);
extern int silent_log_panic_handler(void);
#else
static inline int sec_debug_init(void)
{
	return 0;
}

static inline int sec_debug_dump_stack(void)
{
	return 0;
}
static inline void sec_debug_check_crash_key(unsigned int code, int value) {}
level
static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}

static inline int sec_debug_is_enabled(void) {return 0; }
#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void sec_debug_task_sched_log_short_msg(char *msg);
extern void sec_debug_secure_log(u32 svc_id, u32 cmd_id);
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_sched_log_end(void);
extern void sec_debug_timer_log(unsigned int type, int int_lock, void *fn);
extern void sec_debug_sched_log_init(void);
#define secdbg_sched_msg(fmt, ...) \
	do { \
		char ___buf[16]; \
		snprintf(___buf, sizeof(___buf), fmt, ##__VA_ARGS__); \
		sec_debug_task_sched_log_short_msg(___buf); \
	} while (0)
#else
static inline void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
}
static inline void sec_debug_secure_log(u32 svc_id, u32 cmd_id)
{
}
static inline void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
}
static inline void sec_debug_irq_sched_log_end(void)
{
}
static inline void sec_debug_timer_log(unsigned int type,
						int int_lock, void *fn)
{
}
static inline void sec_debug_sched_log_init(void)
{
}
#define secdbg_sched_msg(fmt, ...)
#endif
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
extern void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time);
#else
static inline void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time)
{
}
#endif

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
extern void debug_semaphore_init(void);
extern void debug_semaphore_down_log(struct semaphore *sem);
extern void debug_semaphore_up_log(struct semaphore *sem);
extern void debug_rwsemaphore_init(void);
extern void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir);
extern void debug_rwsemaphore_up_log(struct rw_semaphore *sem);
#define debug_rwsemaphore_down_read_log(x) \
	debug_rwsemaphore_down_log(x, READ_SEM)
#define debug_rwsemaphore_down_write_log(x) \
	debug_rwsemaphore_down_log(x, WRITE_SEM)
#else
static inline void debug_semaphore_init(void)
{
}
static inline void debug_semaphore_down_log(struct semaphore *sem)
{
}
static inline void debug_semaphore_up_log(struct semaphore *sem)
{
}
static inline void debug_rwsemaphore_init(void)
{
}
static inline void debug_rwsemaphore_down_read_log(struct rw_semaphore *sem)
{
}
static inline void debug_rwsemaphore_down_write_log(struct rw_semaphore *sem)
{
}
static inline void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
}
#endif

/* schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 512
#define TZ_LOG_MAX 64

struct irq_log {
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	int preempt_count;
	void *context;
};

struct secure_log {
	unsigned long long time;
	u32 svc_id, cmd_id;
};

struct irq_exit_log {
	unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct sched_log {
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
};


struct timer_log {
	unsigned long long time;
	unsigned int type;
	int int_lock;
	void *fn;
};

#endif	/* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
#define SEMAPHORE_LOG_MAX 100
struct sem_debug {
	struct list_head list;
	struct semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	/* char comm[TASK_COMM_LEN]; */
};

enum {
	READ_SEM,
	WRITE_SEM
};

#define RWSEMAPHORE_LOG_MAX 100
struct rwsem_debug {
	struct list_head list;
	struct rw_semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	int direction;
	/* char comm[TASK_COMM_LEN]; */
};

#endif	/* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
extern asmlinkage int sec_debug_msg_log(void *caller, const char *fmt, ...);
#define MSG_LOG_MAX 1024
struct secmsg_log {
	unsigned long long time;
	char msg[64];
	void *caller0;
	void *caller1;
	char *task;
};
#define secdbg_msg(fmt, ...) \
	sec_debug_msg_log(__builtin_return_address(0), fmt, ##__VA_ARGS__)
#else
#define secdbg_msg(fmt, ...)
#endif

/* KNOX_SEANDROID_START */
#ifdef CONFIG_SEC_DEBUG_AVC_LOG
extern asmlinkage int sec_debug_avc_log(const char *fmt, ...);
#define AVC_LOG_MAX 256
struct secavc_log {
	char msg[256];
};
#define secdbg_avc(fmt, ...) \
	sec_debug_avc_log(fmt, ##__VA_ARGS__)
#else
#define secdbg_avc(fmt, ...)
#endif
/* KNOX_SEANDROID_END */

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
#define DCVS_LOG_MAX 256

struct dcvs_debug {
	unsigned long long time;
	int cpu_no;
	unsigned int prev_freq;
	unsigned int new_freq;
};
extern void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
			unsigned int new_freq);
#else
static inline void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
					unsigned int new_freq)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
#define FG_LOG_MAX 128

struct fuelgauge_debug {
	unsigned long long time;
	unsigned int voltage;
	unsigned short soc;
	unsigned short charging_status;
};
extern void sec_debug_fuelgauge_log(unsigned int voltage, unsigned short soc,
			unsigned short charging_status);
#else
static inline void sec_debug_fuelgauge_log(unsigned int voltage,
			unsigned short soc,	unsigned short charging_status)
{
}

#endif

/* for sec debug level */
#define KERNEL_SEC_DEBUG_LEVEL_LOW	(0x574F4C44)
#define KERNEL_SEC_DEBUG_LEVEL_MID	(0x44494D44)
#define KERNEL_SEC_DEBUG_LEVEL_HIGH	(0x47494844)
#define ANDROID_DEBUG_LEVEL_LOW		0x4f4c
#define ANDROID_DEBUG_LEVEL_MID		0x494d
#define ANDROID_DEBUG_LEVEL_HIGH	0x4948

/* for dump mode */
#define KERNEL_SEC_DUMP_MODE_QPST	(0x51505354)
#define KERNEL_SEC_DUMP_MODE_RDX	(0x52445853)

#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
extern bool kernel_sec_set_normal_pwroff(int value);
extern int kernel_sec_get_normal_pwroff(void);
#endif


extern bool kernel_sec_set_debug_level(int level);
extern int kernel_sec_get_debug_level(void);
extern bool kernel_sec_set_dump_level(int level);
extern int kernel_sec_get_dump_level(void);
extern int ssr_panic_handler_for_sec_dbg(void);
/*__weak void dump_all_task_info(void);
__weak void dump_cpu_stat(void);*/
extern void emerg_pet_watchdog(void);
extern void msm_restart(char mode, const char *cmd);
#define LOCAL_CONFIG_PRINT_EXTRA_INFO

/* #define CONFIG_SEC_DEBUG_SUBSYS */
#ifdef CONFIG_SEC_DEBUG_SUBSYS

extern void sec_debug_subsys_fill_fbinfo(int idx, void *fb, u32 xres,
				u32 yres, u32 bpp, u32 color_mode);

#define SEC_DEBUG_SUBSYS_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SUBSYS_MAGIC1 0x5ECDEB6
#define SEC_DEBUG_SUBSYS_MAGIC2 0x14F014F0
 /* high word : major version
  * low word : minor version
  * minor version changes should not affect LK behavior
  */
#define SEC_DEBUG_SUBSYS_MAGIC3 0x00010004


#define TZBSP_CPU_COUNT           4
/* CPU context for the monitor. */
struct tzbsp_dump_cpu_ctx_s {
	unsigned int mon_lr;
	unsigned int mon_spsr;
	unsigned int usr_r0;
	unsigned int usr_r1;
	unsigned int usr_r2;
	unsigned int usr_r3;
	unsigned int usr_r4;
	unsigned int usr_r5;
	unsigned int usr_r6;
	unsigned int usr_r7;
	unsigned int usr_r8;
	unsigned int usr_r9;
	unsigned int usr_r10;
	unsigned int usr_r11;
	unsigned int usr_r12;
	unsigned int usr_r13;
	unsigned int usr_r14;
	unsigned int irq_spsr;
	unsigned int irq_r13;
	unsigned int irq_r14;
	unsigned int svc_spsr;
	unsigned int svc_r13;
	unsigned int svc_r14;
	unsigned int abt_spsr;
	unsigned int abt_r13;
	unsigned int abt_r14;
	unsigned int und_spsr;
	unsigned int und_r13;
	unsigned int und_r14;
	unsigned int fiq_spsr;
	unsigned int fiq_r8;
	unsigned int fiq_r9;
	unsigned int fiq_r10;
	unsigned int fiq_r11;
	unsigned int fiq_r12;
	unsigned int fiq_r13;
	unsigned int fiq_r14;
};

struct tzbsp_dump_buf_s {
	unsigned int magic;
	unsigned int version;
	unsigned int cpu_count;
	unsigned int sc_status[TZBSP_CPU_COUNT];
	struct tzbsp_dump_cpu_ctx_s sc_ns[TZBSP_CPU_COUNT];
	struct tzbsp_dump_cpu_ctx_s sec;
	unsigned int wdt0_sts[TZBSP_CPU_COUNT];
};

struct core_reg_info {
	char name[12];
	unsigned int value;
};

struct sec_debug_subsys_excp {
	char type[16];
	char task[16];
	char file[32];
	int line;
	char msg[256];
	struct core_reg_info core_reg[64];
};

struct sec_debug_subsys_excp_krait {
	char pc_sym[64];
	char lr_sym[64];
	char panic_caller[64];
	char panic_msg[128];
	char thread[32];
};

struct sec_debug_subsys_log {
	unsigned int idx_paddr;
	unsigned int log_paddr;
	unsigned int size;
};

struct rgb_bit_info {
	unsigned char r_off;
	unsigned char r_len;
	unsigned char g_off;
	unsigned char g_len;
	unsigned char b_off;
	unsigned char b_len;
	unsigned char a_off;
	unsigned char a_len;
};

struct var_info {
	char name[16];
	int sizeof_type;
	unsigned int var_paddr;
};
struct sec_debug_subsys_simple_var_mon {
	int idx;
	struct var_info var[32];
};

struct sec_debug_subsys_fb {
	unsigned int fb_paddr;
	int xres;
	int yres;
	int bpp;
	struct rgb_bit_info rgb_bitinfo;
};

struct sec_debug_subsys_sched_log {
	unsigned int sched_idx_paddr;
	unsigned int sched_buf_paddr;
	unsigned int sched_struct_sz;
	unsigned int sched_array_cnt;
	unsigned int irq_idx_paddr;
	unsigned int irq_buf_paddr;
	unsigned int irq_struct_sz;
	unsigned int irq_array_cnt;
	unsigned int secure_idx_paddr;
	unsigned int secure_buf_paddr;
	unsigned int secure_struct_sz;
	unsigned int secure_array_cnt;
	unsigned int irq_exit_idx_paddr;
	unsigned int irq_exit_buf_paddr;
	unsigned int irq_exit_struct_sz;
	unsigned int irq_exit_array_cnt;
	unsigned int msglog_idx_paddr;
	unsigned int msglog_buf_paddr;
	unsigned int msglog_struct_sz;
	unsigned int msglog_array_cnt;
};

struct __log_struct_info {
	unsigned int buffer_offset;
	unsigned int w_off_offset;
	unsigned int head_offset;
	unsigned int size_offset;
	unsigned int size_t_typesize;
};
struct __log_data {
	unsigned int log_paddr;
	unsigned int buffer_paddr;
};
struct sec_debug_subsys_logger_log_info {
	struct __log_struct_info stinfo;
	struct __log_data main;
	struct __log_data system;
	struct __log_data events;
	struct __log_data radio;
};
struct sec_debug_subsys_data {
	char name[16];
	char state[16];
	struct sec_debug_subsys_log log;
	struct sec_debug_subsys_excp excp;
	struct sec_debug_subsys_simple_var_mon var_mon;
};

struct sec_debug_subsys_data_modem {
	char name[16];
	char state[16];
	struct sec_debug_subsys_log log;
	struct sec_debug_subsys_excp excp;
	struct sec_debug_subsys_simple_var_mon var_mon;
};

struct sec_debug_subsys_avc_log {
	unsigned int secavc_idx_paddr;
	unsigned int secavc_buf_paddr;
	unsigned int secavc_struct_sz;
	unsigned int secavc_array_cnt;
};

struct sec_debug_subsys_data_krait {
	char name[16];
	char state[16];
	char mdmerr_info[128];
	int nr_cpus;
	struct sec_debug_subsys_log log;
	struct sec_debug_subsys_excp_krait excp;
	struct sec_debug_subsys_simple_var_mon var_mon;
	struct sec_debug_subsys_simple_var_mon info_mon;
	struct tzbsp_dump_buf_s **tz_core_dump;
	struct sec_debug_subsys_fb fb_info;
	struct sec_debug_subsys_sched_log sched_log;
	struct sec_debug_subsys_logger_log_info logger_log;
	struct sec_debug_subsys_avc_log avc_log;
};

struct sec_debug_subsys_private {
	struct sec_debug_subsys_data_krait krait;
	struct sec_debug_subsys_data rpm;
	struct sec_debug_subsys_data_modem modem;
	struct sec_debug_subsys_data dsps;
};

struct sec_debug_subsys {
	unsigned int magic[4];
	struct sec_debug_subsys_data_krait *krait;
	struct sec_debug_subsys_data *rpm;
	struct sec_debug_subsys_data_modem *modem;
	struct sec_debug_subsys_data *dsps;

	struct sec_debug_subsys_private priv;
};

extern int sec_debug_subsys_add_infomon(char *name, unsigned int size,
	unsigned int addr);
#define ADD_VAR_TO_INFOMON(var) \
	sec_debug_subsys_add_infomon(#var, sizeof(var), \
		(unsigned int)__pa(&var))
#define ADD_STR_TO_INFOMON(pstr) \
	sec_debug_subsys_add_infomon(#pstr, -1, (unsigned int)__pa(pstr))

extern int sec_debug_subsys_add_varmon(char *name, unsigned int size,
	unsigned int addr);
#define ADD_VAR_TO_VARMON(var) \
	sec_debug_subsys_add_varmon(#var, sizeof(var), \
		(unsigned int)__pa(&var))
#define ADD_STR_TO_VARMON(pstr) \
	sec_debug_subsys_add_varmon(#pstr, -1, (unsigned int)__pa(pstr))

#define VAR_NAME_MAX	30

#define ADD_ARRAY_TO_VARMON(arr, _index, _varname)	\
do {							\
	char name[VAR_NAME_MAX];			\
	short __index = _index;				\
	char _strindex[5];				\
	snprintf(_strindex, 3, "%c%d%c",		\
			'_', __index, '\0');		\
	strlcpy(name, #_varname, VAR_NAME_MAX);		\
	strlcat(name, _strindex, VAR_NAME_MAX);		\
	sec_debug_subsys_add_varmon(name, sizeof(arr),	\
			(unsigned int)__pa(&arr));	\
} while (0)


#define ADD_STR_ARRAY_TO_VARMON(pstrarr, _index, _varname)	\
do {								\
	char name[VAR_NAME_MAX];				\
	short __index = _index;					\
	char _strindex[5];					\
	snprintf(_strindex, 3, "%c%d%c",			\
			'_', __index, '\0');			\
	strlcpy(name, #_varname, VAR_NAME_MAX);			\
	strlcat(name, _strindex, VAR_NAME_MAX);			\
	sec_debug_subsys_add_varmon(name, -1,			\
			(unsigned int)__pa(&pstrarr));		\
} while (0)


extern void get_fbinfo(int fb_num, unsigned int *fb_paddr, unsigned int *xres,
		unsigned int *yres, unsigned int *bpp,
		unsigned char *roff, unsigned char *rlen,
		unsigned char *goff, unsigned char *glen,
		unsigned char *boff, unsigned char *blen,
		unsigned char *aoff, unsigned char *alen);
extern unsigned int msm_shared_ram_phys;
extern char *get_kernel_log_buf_paddr(void);
extern char *get_fb_paddr(void);
#ifdef CONFIG_SEC_DEBUG_MDM_FILE_INFO
extern void sec_modify_restart_level_mdm(int value);
extern void sec_set_mdm_subsys_info(char *str_buf);
#endif
extern unsigned int get_wdog_regsave_paddr(void);

extern unsigned int get_last_pet_paddr(void);
extern void sec_debug_subsys_set_kloginfo(unsigned int *idx_paddr,
	unsigned int *log_paddr, unsigned int *size);
extern int sec_debug_subsys_set_logger_info(
	struct sec_debug_subsys_logger_log_info *log_info);
int sec_debug_save_die_info(const char *str, struct pt_regs *regs);
int sec_debug_save_panic_info(const char *str, unsigned int caller);

extern uint32_t global_pvs;
extern struct class *sec_class;

#endif

/* here are two functions */
extern void sec_debug_save_last_pet(unsigned long long last_pet);
extern void sec_debug_save_last_ns(unsigned long long last_ns);

#ifdef CONFIG_SEC_DEBUG_DOUBLE_FREE
extern void *kfree_hook(void *p, void *caller);
#endif

#ifdef CONFIG_USER_RESET_DEBUG
extern int sec_debug_get_cp_crash_log(char *str);
#endif

#ifdef CONFIG_SEC_DEBUG_VERBOSE_SUMMARY_HTML
enum {
	SAVE_FREQ = 0,
	SAVE_VOLT
};
void sec_debug_save_cpu_freq_voltage(int cpu, int flag, unsigned long value);
#endif

#endif	/* SEC_DEBUG_H */
