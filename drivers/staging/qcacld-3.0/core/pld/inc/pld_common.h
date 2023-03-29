/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#ifndef __PLD_COMMON_H__
#define __PLD_COMMON_H__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <osapi_linux.h>

#if IS_ENABLED(CONFIG_CNSS_UTILS)
#include <net/cnss_utils.h>
#endif

#define PLD_IMAGE_FILE               "athwlan.bin"
#define PLD_UTF_FIRMWARE_FILE        "utf.bin"
#define PLD_BOARD_DATA_FILE          "fakeboar.bin"
#define PLD_OTP_FILE                 "otp.bin"
#define PLD_SETUP_FILE               "athsetup.bin"
#define PLD_EPPING_FILE              "epping.bin"
#define PLD_EVICTED_FILE             ""
#define PLD_MHI_STATE_L0	1

#define TOTAL_DUMP_SIZE         0x00200000

#if IS_ENABLED(CONFIG_WCNSS_MEM_PRE_ALLOC)
#include <net/cnss_prealloc.h>
#endif

/**
 * enum pld_bus_type - bus type
 * @PLD_BUS_TYPE_NONE: invalid bus type, only return in error cases
 * @PLD_BUS_TYPE_PCIE: PCIE bus
 * @PLD_BUS_TYPE_SNOC: SNOC bus
 * @PLD_BUS_TYPE_SDIO: SDIO bus
 * @PLD_BUS_TYPE_USB : USB bus
 * @PLD_BUS_TYPE_SNOC_FW_SIM : SNOC FW SIM bus
 * @PLD_BUS_TYPE_PCIE_FW_SIM : PCIE FW SIM bus
 * @PLD_BUS_TYPE_IPCI : IPCI bus
 * @PLD_BUS_TYPE_IPCI_FW_SIM : IPCI FW SIM bus
 */
enum pld_bus_type {
	PLD_BUS_TYPE_NONE = -1,
	PLD_BUS_TYPE_PCIE = 0,
	PLD_BUS_TYPE_SNOC,
	PLD_BUS_TYPE_SDIO,
	PLD_BUS_TYPE_USB,
	PLD_BUS_TYPE_SNOC_FW_SIM,
	PLD_BUS_TYPE_PCIE_FW_SIM,
	PLD_BUS_TYPE_IPCI,
	PLD_BUS_TYPE_IPCI_FW_SIM,
};

#define PLD_MAX_FIRMWARE_SIZE (1 * 1024 * 1024)

/**
 * enum pld_bus_width_type - bus bandwidth
 * @PLD_BUS_WIDTH_NONE: don't vote for bus bandwidth
 * @PLD_BUS_WIDTH_IDLE: vote for idle bandwidth
 * @PLD_BUS_WIDTH_LOW: vote for low bus bandwidth
 * @PLD_BUS_WIDTH_MEDIUM: vote for medium bus bandwidth
 * @PLD_BUS_WIDTH_HIGH: vote for high bus bandwidth
 * @PLD_BUS_WIDTH_VERY_HIGH: vote for very high bus bandwidth
 * @PLD_BUS_WIDTH_LOW_LATENCY: vote for low latency bus bandwidth
 */
enum pld_bus_width_type {
	PLD_BUS_WIDTH_NONE,
	PLD_BUS_WIDTH_IDLE,
	PLD_BUS_WIDTH_LOW,
	PLD_BUS_WIDTH_MEDIUM,
	PLD_BUS_WIDTH_HIGH,
	PLD_BUS_WIDTH_VERY_HIGH,
	PLD_BUS_WIDTH_LOW_LATENCY,
};

#define PLD_MAX_FILE_NAME NAME_MAX

/**
 * struct pld_fw_file - WLAN FW file names
 * @image_file: WLAN FW image file
 * @board_data: WLAN FW board data file
 * @otp_data: WLAN FW OTP file
 * @utf_file: WLAN FW UTF file
 * @utf_board_data: WLAN FW UTF board data file
 * @epping_file: WLAN FW EPPING mode file
 * @evicted_data: WLAN FW evicted file
 * @setup_file: WLAN FW setup file
 *
 * pld_fw_files is used to store WLAN FW file names
 */
struct pld_fw_files {
	char image_file[PLD_MAX_FILE_NAME];
	char board_data[PLD_MAX_FILE_NAME];
	char otp_data[PLD_MAX_FILE_NAME];
	char utf_file[PLD_MAX_FILE_NAME];
	char utf_board_data[PLD_MAX_FILE_NAME];
	char epping_file[PLD_MAX_FILE_NAME];
	char evicted_data[PLD_MAX_FILE_NAME];
	char setup_file[PLD_MAX_FILE_NAME];
	char ibss_image_file[PLD_MAX_FILE_NAME];
};

/**
 * enum pld_platform_cap_flag - platform capability flag
 * @PLD_HAS_EXTERNAL_SWREG: has external regulator
 * @PLD_HAS_UART_ACCESS: has UART access
 * @PLD_HAS_DRV_SUPPORT: has PCIe DRV support
 */
enum pld_platform_cap_flag {
	PLD_HAS_EXTERNAL_SWREG = 0x01,
	PLD_HAS_UART_ACCESS = 0x02,
	PLD_HAS_DRV_SUPPORT = 0x04,
};

/**
 * struct pld_platform_cap - platform capabilities
 * @cap_flag: capabilities flag
 *
 * pld_platform_cap provides platform capabilities which are
 * extracted from DTS.
 */
struct pld_platform_cap {
	u32 cap_flag;
};

/**
 * enum pld_uevent - PLD uevent event types
 * @PLD_FW_DOWN: firmware is down
 * @PLD_FW_CRASHED: firmware has crashed
 * @PLD_FW_RECOVERY_START: firmware is starting recovery
 */
enum pld_uevent {
	PLD_FW_DOWN,
	PLD_FW_CRASHED,
	PLD_FW_RECOVERY_START,
	PLD_FW_HANG_EVENT,
	PLD_SMMU_FAULT,
};

/**
 * struct pld_uevent_data - uevent status received from platform driver
 * @uevent: uevent type
 * @fw_down: FW down info
 */
struct pld_uevent_data {
	enum pld_uevent uevent;
	union {
		struct {
			bool crashed;
		} fw_down;
		struct {
			void *hang_event_data;
			u16 hang_event_data_len;
		} hang_data;
	};
};

/**
 * struct pld_ce_tgt_pipe_cfg - copy engine target pipe configuration
 * @pipe_num: pipe number
 * @pipe_dir: pipe direction
 * @nentries: number of entries
 * @nbytes_max: max number of bytes
 * @flags: flags
 * @reserved: reserved
 *
 * pld_ce_tgt_pipe_cfg is used to store copy engine target pipe
 * configuration.
 */
struct pld_ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

/**
 * struct pld_ce_svc_pipe_cfg - copy engine service pipe configuration
 * @service_id: service ID
 * @pipe_dir: pipe direction
 * @pipe_num: pipe number
 *
 * pld_ce_svc_pipe_cfg is used to store copy engine service pipe
 * configuration.
 */
struct pld_ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

/**
 * struct pld_shadow_reg_cfg - shadow register configuration
 * @ce_id: copy engine ID
 * @reg_offset: register offset
 *
 * pld_shadow_reg_cfg is used to store shadow register configuration.
 */
struct pld_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

/**
 * struct pld_shadow_reg_v2_cfg - shadow register version 2 configuration
 * @addr: shadow register physical address
 *
 * pld_shadow_reg_v2_cfg is used to store shadow register version 2
 * configuration.
 */
struct pld_shadow_reg_v2_cfg {
	u32 addr;
};

/**
 * struct pld_rri_over_ddr_cfg_s - rri_over_ddr configuration
 * @base_addr_low: lower 32bit
 * @base_addr_high: higher 32bit
 *
 * pld_rri_over_ddr_cfg_s is used in Genoa to pass rri_over_ddr configuration
 * to firmware to update ring/write index in host DDR.
 */
struct pld_rri_over_ddr_cfg {
	u32 base_addr_low;
	u32 base_addr_high;
};

/**
 * struct pld_wlan_enable_cfg - WLAN FW configuration
 * @num_ce_tgt_cfg: number of CE target configuration
 * @ce_tgt_cfg: CE target configuration
 * @num_ce_svc_pipe_cfg: number of CE service configuration
 * @ce_svc_cfg: CE service configuration
 * @num_shadow_reg_cfg: number of shadow register configuration
 * @shadow_reg_cfg: shadow register configuration
 * @num_shadow_reg_v2_cfg: number of shadow register version 2 configuration
 * @shadow_reg_v2_cfg: shadow register version 2 configuration
 * @rri_over_ddr_cfg_valid: valid flag for rri_over_ddr config
 * @rri_over_ddr_cfg: rri over ddr config
 *
 * pld_wlan_enable_cfg stores WLAN FW configurations. It will be
 * passed to WLAN FW when WLAN host driver calls wlan_enable.
 */
struct pld_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct pld_ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct pld_ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct pld_shadow_reg_cfg *shadow_reg_cfg;
	u32 num_shadow_reg_v2_cfg;
	struct pld_shadow_reg_v2_cfg *shadow_reg_v2_cfg;
	bool rri_over_ddr_cfg_valid;
	struct pld_rri_over_ddr_cfg rri_over_ddr_cfg;
};

/**
 * enum pld_driver_mode - WLAN host driver mode
 * @PLD_MISSION: mission mode
 * @PLD_FTM: FTM mode
 * @PLD_EPPING: EPPING mode
 * @PLD_WALTEST: WAL test mode, FW standalone test mode
 * @PLD_OFF: OFF mode
 * @PLD_COLDBOOT_CALIBRATION: Cold Boot Calibration Mode
 * @PLD_FTM_COLDBOOT_CALIBRATION: Cold Boot Calibration for FTM Mode
 */
enum pld_driver_mode {
	PLD_MISSION,
	PLD_FTM,
	PLD_EPPING,
	PLD_WALTEST,
	PLD_OFF,
	PLD_COLDBOOT_CALIBRATION = 7,
	PLD_FTM_COLDBOOT_CALIBRATION = 10
};

/**
 * struct pld_device_version - WLAN device version info
 * @family_number: family number of WLAN SOC HW
 * @device_number: device number of WLAN SOC HW
 * @major_version: major version of WLAN SOC HW
 * @minor_version: minor version of WLAN SOC HW
 *
 * pld_device_version is used to store WLAN device version info
 */

struct pld_device_version {
	u32 family_number;
	u32 device_number;
	u32 major_version;
	u32 minor_version;
};

#define PLD_MAX_TIMESTAMP_LEN 32

/**
 * struct pld_soc_info - SOC information
 * @v_addr: virtual address of preallocated memory
 * @p_addr: physical address of preallcoated memory
 * @chip_id: chip ID
 * @chip_family: chip family
 * @board_id: board ID
 * @soc_id: SOC ID
 * @fw_version: FW version
 * @fw_build_timestamp: FW build timestamp
 * @device_version: WLAN device version info
 *
 * pld_soc_info is used to store WLAN SOC information.
 */
struct pld_soc_info {
	void __iomem *v_addr;
	phys_addr_t p_addr;
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
	char fw_build_timestamp[PLD_MAX_TIMESTAMP_LEN + 1];
	struct pld_device_version device_version;
};

/**
 * enum pld_recovery_reason - WLAN host driver recovery reason
 * @PLD_REASON_DEFAULT: default
 * @PLD_REASON_LINK_DOWN: PCIe link down
 */
enum pld_recovery_reason {
	PLD_REASON_DEFAULT,
	PLD_REASON_LINK_DOWN
};

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * enum pld_wlan_time_sync_trigger_type - WLAN time sync trigger type
 * @PLD_TRIGGER_POSITIVE_EDGE: Positive edge trigger
 * @PLD_TRIGGER_NEGATIVE_EDGE: Negative edge trigger
 */
enum pld_wlan_time_sync_trigger_type {
	PLD_TRIGGER_POSITIVE_EDGE,
	PLD_TRIGGER_NEGATIVE_EDGE
};
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

/**
 * struct pld_driver_ops - driver callback functions
 * @probe: required operation, will be called when device is detected
 * @remove: required operation, will be called when device is removed
 * @idle_shutdown: required operation, will be called when device is doing
 *                 idle shutdown after interface inactivity timer has fired
 * @idle_restart: required operation, will be called when device is doing
 *                idle restart after idle shutdown
 * @shutdown: optional operation, will be called during SSR
 * @reinit: optional operation, will be called during SSR
 * @crash_shutdown: optional operation, will be called when a crash is
 *                  detected
 * @suspend: required operation, will be called for power management
 *           is enabled
 * @resume: required operation, will be called for power management
 *          is enabled
 * @modem_status: optional operation, will be called when platform driver
 *                sending modem power status to WLAN FW
 * @uevent: optional operation, will be called when platform driver
 *                 updating driver status
 * @runtime_suspend: optional operation, prepare the device for a condition
 *                   in which it won't be able to communicate with the CPU(s)
 *                   and RAM due to power management.
 * @runtime_resume: optional operation, put the device into the fully
 *                  active state in response to a wakeup event generated by
 *                  hardware or at the request of software.
 * @suspend_noirq: optional operation, complete the actions started by suspend()
 * @resume_noirq: optional operation, prepare for the execution of resume()
 * @set_curr_therm_cdev_state: optional operation, will be called when there is
 *                        change in the thermal level triggered by the thermal
 *                        subsystem thus requiring mitigation actions. This will
 *                        be called every time there is a change in the state
 *                        and after driver load.
 */
struct pld_driver_ops {
	int (*probe)(struct device *dev,
		     enum pld_bus_type bus_type,
		     void *bdev, void *id);
	void (*remove)(struct device *dev,
		       enum pld_bus_type bus_type);
	int (*idle_shutdown)(struct device *dev,
			      enum pld_bus_type bus_type);
	int (*idle_restart)(struct device *dev,
			     enum pld_bus_type bus_type);
	void (*shutdown)(struct device *dev,
			 enum pld_bus_type bus_type);
	int (*reinit)(struct device *dev,
		      enum pld_bus_type bus_type,
		      void *bdev, void *id);
	void (*crash_shutdown)(struct device *dev,
			       enum pld_bus_type bus_type);
	int (*suspend)(struct device *dev,
		       enum pld_bus_type bus_type,
		       pm_message_t state);
	int (*resume)(struct device *dev,
		      enum pld_bus_type bus_type);
	int (*reset_resume)(struct device *dev,
		      enum pld_bus_type bus_type);
	void (*modem_status)(struct device *dev,
			     enum pld_bus_type bus_type,
			     int state);
	void (*uevent)(struct device *dev, struct pld_uevent_data *uevent);
	int (*runtime_suspend)(struct device *dev,
			       enum pld_bus_type bus_type);
	int (*runtime_resume)(struct device *dev,
			      enum pld_bus_type bus_type);
	int (*suspend_noirq)(struct device *dev,
			     enum pld_bus_type bus_type);
	int (*resume_noirq)(struct device *dev,
			    enum pld_bus_type bus_type);
	int (*set_curr_therm_cdev_state)(struct device *dev,
					 unsigned long state,
					 int mon_id);
};

int pld_init(void);
void pld_deinit(void);

/**
 * pld_set_mode() - set driver mode in PLD module
 * @mode: driver mode
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_set_mode(u8 mode);

int pld_register_driver(struct pld_driver_ops *ops);
void pld_unregister_driver(void);

int pld_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
		    enum pld_driver_mode mode);
int pld_wlan_disable(struct device *dev, enum pld_driver_mode mode);
int pld_set_fw_log_mode(struct device *dev, u8 fw_log_mode);
void pld_get_default_fw_files(struct pld_fw_files *pfw_files);
int pld_get_fw_files_for_target(struct device *dev,
				struct pld_fw_files *pfw_files,
				u32 target_type, u32 target_version);
int pld_prevent_l1(struct device *dev);
void pld_allow_l1(struct device *dev);

/**
 * pld_set_pcie_gen_speed() - Set PCIE gen speed
 * @dev: device
 * @pcie_gen_speed: Required PCIE gen speed
 *
 * Send required PCIE Gen speed to platform driver
 *
 * Return: 0 for success. Negative error codes.
 */
int pld_set_pcie_gen_speed(struct device *dev, u8 pcie_gen_speed);

void pld_is_pci_link_down(struct device *dev);
void pld_get_bus_reg_dump(struct device *dev, uint8_t *buf, uint32_t len);
int pld_shadow_control(struct device *dev, bool enable);
void pld_schedule_recovery_work(struct device *dev,
				enum pld_recovery_reason reason);

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
int pld_get_audio_wlan_timestamp(struct device *dev,
				 enum pld_wlan_time_sync_trigger_type type,
				 uint64_t *ts);
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

#if IS_ENABLED(CONFIG_CNSS_UTILS)
/**
 * pld_set_wlan_unsafe_channel() - Set unsafe channel
 * @dev: device
 * @unsafe_ch_list: unsafe channel list
 * @ch_count: number of channel
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_set_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 ch_count)
{
	return cnss_utils_set_wlan_unsafe_channel(dev, unsafe_ch_list,
						  ch_count);
}
/**
 * pld_get_wlan_unsafe_channel() - Get unsafe channel
 * @dev: device
 * @unsafe_ch_list: buffer to unsafe channel list
 * @ch_count: number of channel
 * @buf_len: buffer length
 *
 * Return WLAN unsafe channel to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_get_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 *ch_count, u16 buf_len)
{
	return cnss_utils_get_wlan_unsafe_channel(dev, unsafe_ch_list,
						  ch_count, buf_len);
}
/**
 * pld_wlan_set_dfs_nol() - Set DFS info
 * @dev: device
 * @info: DFS info
 * @info_len: info length
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_wlan_set_dfs_nol(struct device *dev, void *info,
				       u16 info_len)
{
	return cnss_utils_wlan_set_dfs_nol(dev, info, info_len);
}
/**
 * pld_wlan_get_dfs_nol() - Get DFS info
 * @dev: device
 * @info: buffer to DFS info
 * @info_len: info length
 *
 * Return DFS info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_wlan_get_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return cnss_utils_wlan_get_dfs_nol(dev, info, info_len);
}
/**
 * pld_get_wlan_mac_address() - API to query MAC address from Platform
 * Driver
 * @dev: Device Structure
 * @num: Pointer to number of MAC address supported
 *
 * Platform Driver can have MAC address stored. This API needs to be used
 * to get those MAC address
 *
 * Return: Pointer to the list of MAC address
 */
static inline uint8_t *pld_get_wlan_mac_address(struct device *dev,
						uint32_t *num)
{
	return cnss_utils_get_wlan_mac_address(dev, num);
}

/**
 * pld_get_wlan_derived_mac_address() - API to query derived MAC address
 * from platform Driver
 * @dev: Device Structure
 * @num: Pointer to number of MAC address supported
 *
 * Platform Driver can have MAC address stored. This API needs to be used
 * to get those MAC address
 *
 * Return: Pointer to the list of MAC address
 */
static inline uint8_t *pld_get_wlan_derived_mac_address(struct device *dev,
							uint32_t *num)
{
	return cnss_utils_get_wlan_derived_mac_address(dev, num);
}

/**
 * pld_increment_driver_load_cnt() - Maintain driver load count
 * @dev: device
 *
 * This function maintain a count which get increase whenever wiphy
 * is registered
 *
 * Return: void
 */
static inline void pld_increment_driver_load_cnt(struct device *dev)
{
	cnss_utils_increment_driver_load_cnt(dev);
}
/**
 * pld_get_driver_load_cnt() - get driver load count
 * @dev: device
 *
 * This function provide total wiphy registration count from starting
 *
 * Return: driver load count
 */
static inline int pld_get_driver_load_cnt(struct device *dev)
{
	return cnss_utils_get_driver_load_cnt(dev);
}
#else
static inline int pld_set_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 ch_count)
{
	return 0;
}
static inline int pld_get_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 *ch_count, u16 buf_len)
{
	*ch_count = 0;

	return 0;
}
static inline int pld_wlan_set_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return -EINVAL;
}
static inline int pld_wlan_get_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return -EINVAL;
}
static inline uint8_t *pld_get_wlan_mac_address(struct device *dev,
						uint32_t *num)
{
	*num = 0;
	return NULL;
}

static inline uint8_t *pld_get_wlan_derived_mac_address(struct device *dev,
							uint32_t *num)
{
	*num = 0;
	return NULL;
}

static inline void pld_increment_driver_load_cnt(struct device *dev) {}
static inline int pld_get_driver_load_cnt(struct device *dev)
{
	return -EINVAL;
}
#endif
int pld_wlan_pm_control(struct device *dev, bool vote);
void *pld_get_virt_ramdump_mem(struct device *dev, unsigned long *size);

/**
 * pld_release_virt_ramdump_mem() - Release virtual ramdump memory
 * @dev: device
 * @address: buffer to virtual memory address
 *
 * Return: void
 */
void pld_release_virt_ramdump_mem(struct device *dev, void *address);
void pld_device_crashed(struct device *dev);
void pld_device_self_recovery(struct device *dev,
			      enum pld_recovery_reason reason);
void pld_intr_notify_q6(struct device *dev);
void pld_request_pm_qos(struct device *dev, u32 qos_val);
void pld_remove_pm_qos(struct device *dev);
int pld_request_bus_bandwidth(struct device *dev, int bandwidth);
int pld_get_platform_cap(struct device *dev, struct pld_platform_cap *cap);
int pld_get_sha_hash(struct device *dev, const u8 *data,
		     u32 data_len, u8 *hash_idx, u8 *out);
void *pld_get_fw_ptr(struct device *dev);
int pld_auto_suspend(struct device *dev);
int pld_auto_resume(struct device *dev);
int pld_force_wake_request(struct device *dev);

/**
 * pld_force_wake_request_sync() - Request to awake MHI synchronously
 * @dev: device
 * @timeout_us: timeout in micro-sec request to wake
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_force_wake_request_sync(struct device *dev, int timeout_us);

/**
 * pld_exit_power_save() - Send EXIT_POWER_SAVE QMI to FW
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_exit_power_save(struct device *dev);
int pld_is_device_awake(struct device *dev);
int pld_force_wake_release(struct device *dev);
int pld_ce_request_irq(struct device *dev, unsigned int ce_id,
		       irqreturn_t (*handler)(int, void *),
		       unsigned long flags, const char *name, void *ctx);
int pld_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx);
void pld_enable_irq(struct device *dev, unsigned int ce_id);
void pld_disable_irq(struct device *dev, unsigned int ce_id);
int pld_get_soc_info(struct device *dev, struct pld_soc_info *info);
int pld_get_mhi_state(struct device *dev);
int pld_is_pci_ep_awake(struct device *dev);
int pld_get_ce_id(struct device *dev, int irq);
int pld_get_irq(struct device *dev, int ce_id);
void pld_lock_pm_sem(struct device *dev);
void pld_release_pm_sem(struct device *dev);
void pld_lock_reg_window(struct device *dev, unsigned long *flags);
void pld_unlock_reg_window(struct device *dev, unsigned long *flags);
int pld_power_on(struct device *dev);
int pld_power_off(struct device *dev);
int pld_athdiag_read(struct device *dev, uint32_t offset, uint32_t memtype,
		     uint32_t datalen, uint8_t *output);
int pld_athdiag_write(struct device *dev, uint32_t offset, uint32_t memtype,
		      uint32_t datalen, uint8_t *input);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
void *pld_smmu_get_domain(struct device *dev);
#else
void *pld_smmu_get_mapping(struct device *dev);
#endif
int pld_smmu_map(struct device *dev, phys_addr_t paddr,
		 uint32_t *iova_addr, size_t size);
#ifdef CONFIG_SMMU_S1_UNMAP
int pld_smmu_unmap(struct device *dev,
		   uint32_t iova_addr, size_t size);
#else
static inline int pld_smmu_unmap(struct device *dev,
				 uint32_t iova_addr, size_t size)
{
	return 0;
}
#endif
int pld_get_user_msi_assignment(struct device *dev, char *user_name,
				int *num_vectors, uint32_t *user_base_data,
				uint32_t *base_vector);
int pld_get_msi_irq(struct device *dev, unsigned int vector);
void pld_get_msi_address(struct device *dev, uint32_t *msi_addr_low,
			 uint32_t *msi_addr_high);
int pld_is_drv_connected(struct device *dev);
unsigned int pld_socinfo_get_serial_number(struct device *dev);
int pld_is_qmi_disable(struct device *dev);
int pld_is_fw_down(struct device *dev);
int pld_force_assert_target(struct device *dev);
int pld_force_collect_target_dump(struct device *dev);
int pld_qmi_send_get(struct device *dev);
int pld_qmi_send_put(struct device *dev);
int pld_qmi_send(struct device *dev, int type, void *cmd,
		 int cmd_len, void *cb_ctx,
		 int (*cb)(void *ctx, void *event, int event_len));
bool pld_is_fw_dump_skipped(struct device *dev);

/**
 * pld_is_pdr() - Check WLAN PD is Restarted
 *
 * Help the driver decide whether FW down is due to
 * WLAN PD Restart.
 *
 * Return: 1 WLAN PD is Restarted
 *         0 WLAN PD is not Restarted
 */
int pld_is_pdr(struct device *dev);

/**
 * pld_is_fw_rejuvenate() - Check WLAN fw is rejuvenating
 *
 * Help the driver decide whether FW down is due to
 * SSR or FW rejuvenate.
 *
 * Return: 1 FW is rejuvenating
 *         0 FW is not rejuvenating
 */
int pld_is_fw_rejuvenate(struct device *dev);

/**
 * pld_have_platform_driver_support() - check if platform driver support
 * @dev: device
 *
 * Return: true if platform driver support.
 */
bool pld_have_platform_driver_support(struct device *dev);

/**
 * pld_idle_shutdown - request idle shutdown callback from platform driver
 * @dev: pointer to struct dev
 * @shutdown_cb: pointer to hdd psoc idle shutdown callback handler
 *
 * Return: 0 for success and non-zero negative error code for failure
 */
int pld_idle_shutdown(struct device *dev,
		      int (*shutdown_cb)(struct device *dev));

/**
 * pld_idle_restart - request idle restart callback from platform driver
 * @dev: pointer to struct dev
 * @restart_cb: pointer to hdd psoc idle restart callback handler
 *
 * Return: 0 for success and non-zero negative error code for failure
 */
int pld_idle_restart(struct device *dev,
		     int (*restart_cb)(struct device *dev));
/**
 * pld_srng_request_irq() - Register IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 * @handler: IRQ callback function
 * @irqflags: IRQ flags
 * @name: IRQ name
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_srng_request_irq(struct device *dev, int irq, irq_handler_t handler,
			 unsigned long irqflags,
			 const char *name,
			 void *ctx);
/**
 * pld_srng_free_irq() - Free IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_srng_free_irq(struct device *dev, int irq, void *ctx);

/**
 * pld_srng_enable_irq() - Enable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_enable_irq(struct device *dev, int irq);

/**
 * pld_srng_disable_irq() - Disable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_disable_irq(struct device *dev, int irq);

/**
 * pld_srng_disable_irq_sync() - Synchronouus disable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_disable_irq_sync(struct device *dev, int irq);

/**
 * pld_pci_read_config_word() - Read PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_read_config_word(struct pci_dev *pdev, int offset, uint16_t *val);

/**
 * pld_pci_write_config_word() - Write PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_write_config_word(struct pci_dev *pdev, int offset, uint16_t val);

/**
 * pld_pci_read_config_dword() - Read PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_read_config_dword(struct pci_dev *pdev, int offset, uint32_t *val);

/**
 * pld_pci_write_config_dword() - Write PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_write_config_dword(struct pci_dev *pdev, int offset, uint32_t val);

/**
 * pld_thermal_register() - Register the thermal device with the thermal system
 * @dev: The device structure
 * @state: The max state to be configured on registration
 * @mon_id: Thermal cooling device ID
 *
 * Return: Error code on error
 */
int pld_thermal_register(struct device *dev, unsigned long state, int mon_id);

/**
 * pld_thermal_unregister() - Unregister the device with the thermal system
 * @dev: The device structure
 * @mon_id: Thermal cooling device ID
 *
 * Return: None
 */
void pld_thermal_unregister(struct device *dev, int mon_id);

/**
 * pld_get_thermal_state() - Get the current thermal state from the PLD
 * @dev: The device structure
 * @thermal_state: param to store the current thermal state
 * @mon_id: Thermal cooling device ID
 *
 * Return: Non-zero code for error; zero for success
 */
int pld_get_thermal_state(struct device *dev, unsigned long *thermal_state,
			  int mon_id);

#if IS_ENABLED(CONFIG_WCNSS_MEM_PRE_ALLOC) && defined(FEATURE_SKB_PRE_ALLOC)

/**
 * pld_nbuf_pre_alloc() - get allocated nbuf from platform driver.
 * @size: Netbuf requested size
 *
 * Return: nbuf or NULL if no memory
 */
static inline struct sk_buff *pld_nbuf_pre_alloc(size_t size)
{
	struct sk_buff *skb = NULL;

	if (size >= WCNSS_PRE_SKB_ALLOC_GET_THRESHOLD)
		skb = wcnss_skb_prealloc_get(size);

	return skb;
}

/**
 * pld_nbuf_pre_alloc_free() - free the nbuf allocated in platform driver.
 * @skb: Pointer to network buffer
 *
 * Return: TRUE if the nbuf is freed
 */
static inline int pld_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return wcnss_skb_prealloc_put(skb);
}
#else
static inline struct sk_buff *pld_nbuf_pre_alloc(size_t size)
{
	return NULL;
}
static inline int pld_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return 0;
}
#endif
/**
 * pld_get_bus_type() - Bus type of the device
 * @dev: device
 *
 * Return: PLD bus type
 */
enum pld_bus_type pld_get_bus_type(struct device *dev);

static inline int pfrm_request_irq(struct device *dev, unsigned int ce_id,
				   irqreturn_t (*handler)(int, void *),
				   unsigned long flags, const char *name,
				   void *ctx)
{
	return pld_srng_request_irq(dev, ce_id, handler, flags, name, ctx);
}

static inline int pfrm_free_irq(struct device *dev, int irq, void *ctx)
{
	return pld_srng_free_irq(dev, irq, ctx);
}

static inline void pfrm_enable_irq(struct device *dev, int irq)
{
	pld_srng_enable_irq(dev, irq);
}

static inline void pfrm_disable_irq_nosync(struct device *dev, int irq)
{
	pld_srng_disable_irq(dev, irq);
}

static inline void pfrm_disable_irq(struct device *dev, int irq)
{
	pld_srng_disable_irq_sync(dev, irq);
}

static inline int pfrm_read_config_word(struct pci_dev *pdev, int offset,
					uint16_t *val)
{
	return pld_pci_read_config_word(pdev, offset, val);
}

static inline int pfrm_write_config_word(struct pci_dev *pdev, int offset,
					 uint16_t val)
{
	return pld_pci_write_config_word(pdev, offset, val);
}

static inline int pfrm_read_config_dword(struct pci_dev *pdev, int offset,
					 uint32_t *val)
{
	return pld_pci_read_config_dword(pdev, offset, val);
}

static inline int pfrm_write_config_dword(struct pci_dev *pdev, int offset,
					  uint32_t val)
{
	return pld_pci_write_config_dword(pdev, offset, val);
}

#endif
