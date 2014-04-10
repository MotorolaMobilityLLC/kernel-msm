/* asus_bat.h */
//ASUS_BSP +++ Josh_Liao "add asus battery driver"
#ifndef _ASUS_BAT_H
#define _ASUS_BAT_H

//#include <linux/asusdebug.h>

struct asus_bat_platform_data{
        int bat_low_irq;
};

#define DBGMSK_BAT_DEBUG		KERN_DEBUG
#define DBGMSK_BAT_INFO		KERN_INFO	//DBGMSK_BAT_G1
#define DBGMSK_BAT_TRACE		KERN_DEBUG
#define DBGMSK_BAT_WARNING	KERN_DEBUG
#define DBGMSK_BAT_ERR		KERN_INFO	//DBGMSK_BAT_G0

struct asus_bat_phone_bat_struct {
	void (*phone_bat_power_supply_changed)(void);
	int 	(*get_prop_bat_present_byhw)(void);
	int 	(*get_prop_bat_capacity_byhw)(void);
	int 	(*get_prop_bat_status_byhw)(void);
	int 	(*get_prop_bat_chgin_byhw)(void);
	int	(*get_prop_charge_type_byhw)(void);
	int	(*get_prop_batt_health_byhw)(void);
	int	(*get_prop_batt_volt_byhw)(void);
	int	(*get_prop_batt_curr_byhw)(void);
};


enum asus_bat_charger_cable {
	ASUS_BAT_CHARGER_NONE = 0,
	ASUS_BAT_CHARGER_USB,
	ASUS_BAT_CHARGER_AC,
	ASUS_BAT_CHARGER_OTHER_BAT,
	ASUS_BAT_CHARGER_UNKNOWN
};

// Asus BSP Eason_Chang +++ function for AXI_BatteryServiceFacade
enum exChargingStop_Reason{   
    exCHGDOWN = 0,
    exBATT_REMOVED_ERROR,
    exBATTEMP_HOT_ERROR,
    exBATTEMP_COLD_ERROR,
    exTEMP_OK_ERROR,
    exDCIN_OV,
    exDCIN_UV,
    exUNREG_extDC,
    ex8921FSM_change,
};

#ifdef CONFIG_BATTERY_ASUS

extern void asus_bat_set_phone_bat(struct asus_bat_phone_bat_struct *pbs);

extern void asus_bat_report_phone_charger(enum asus_bat_charger_cable charger);

extern void asus_bat_write_phone_bat_capacity_tofile(void);

extern int asus_bat_report_phone_capacity(int phone_bat_capacity);

extern int asus_bat_report_phone_status(int phone_bat_status);

extern int asus_bat_report_phone_present(int phone_bat_present);

extern bool asus_bat_has_done_first_periodic_update(void);

// extern void asus_bat_set_pad_bat(struct power_supply *psy);
// extern void asus_bat_set_dock_bat(struct power_supply *psy);

#else

static inline void asus_bat_set_phone_bat(struct asus_bat_phone_bat_struct *pbs){}

static inline void asus_bat_report_phone_charger(enum asus_bat_charger_cable charger){}

static inline void asus_bat_write_phone_bat_capacity_tofile(void){}

static inline int asus_bat_report_phone_capacity(int phone_bat_capacity)
{ return 0; }

static inline int asus_bat_report_phone_status(int phone_bat_status)
{ return 0; }

static inline int asus_bat_report_phone_present(int phone_bat_present)
{ return 0; }

static inline bool asus_bat_has_done_first_periodic_update(void)
{ return false;}

#endif /* CONFIG_BATTERY_ASUS */

#endif /* _ASUS_BAT_H */
//ASUS_BSP --- Josh_Liao "add asus battery driver"

