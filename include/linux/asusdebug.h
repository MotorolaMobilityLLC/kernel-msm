/////////////////////////////////////////////////////////////////////////////////////////////
////                  ASUS Debugging mechanism
/////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __ASUSDEBUG_H__
#define __ASUSDEBUG_H__
// #include <linux/asus_ver.h>

/////////////////////////////////////////////////////////////////////////////////////////////
////                  Debug mask mechanism
/////////////////////////////////////////////////////////////////////////////////////////////

extern unsigned int PRINTK_BUFFER;
extern unsigned int RTB_BUFFER;

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
extern unsigned int LAST_KMSG_BUFFER;
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

#define PRINTK_BUFFER_SIZE      (0x00200000)

#define PRINTK_PARSE_SIZE       (0x00080000)

#define PRINTK_BUFFER_MAGIC     (0xFEEDBEEF)
#define PRINTK_BUFFER_SLOT_SIZE (0x00040000)

#define PRINTK_BUFFER_SLOT1     (PRINTK_BUFFER)
#define PRINTK_BUFFER_SLOT2     ((int)PRINTK_BUFFER + (int)PRINTK_BUFFER_SLOT_SIZE)

#define PHONE_HANG_LOG_BUFFER   ((int)PRINTK_BUFFER + (int)PRINTK_BUFFER_SLOT_SIZE + (int)PRINTK_BUFFER_SLOT_SIZE)
#define PHONE_HANG_LOG_SIZE     (0x00080000)

#define ASUS_DBG_FILTER_PATH "/data/log/asus_debug_filter"

#define ASUS_MSK_GROUP  (64)
#define ASUS_MSK_MAGIC  (0xFC)

#define DEFAULT_MASK   {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                        0x00,0x00,0x00,0x00}
						
//ASUS_BSP +++ SamChen 20101001 add for audio debug mask
#define DBGMSK_SND_G0   "\xFC\x01\x01"	// normal
#define DBGMSK_SND_G1   "\xFC\x01\x02"	// tpa2018
#define DBGMSK_SND_G2   "\xFC\x01\x04"	// FM33 and I2C extend GPIO
#define DBGMSK_SND_G3   "\xFC\x01\x08"	// Headset and HOOK Key
#define DBGMSK_SND_G4   "\xFC\x01\x10"
#define DBGMSK_SND_G5   "\xFC\x01\x20"
#define DBGMSK_SND_G6   "\xFC\x01\x40"
#define DBGMSK_SND_G7   "\xFC\x01\x80"
//ASUS_BSP --- SamChen 20101001 add for audio debug mask

//ASUS_BSP +++ Mickey 20101001 add for LCD debug mask
#define DBGMSK_LCD_G0   "\xFC\x02\x01"
#define DBGMSK_LCD_G1   "\xFC\x02\x02"
#define DBGMSK_LCD_G2   "\xFC\x02\x04"
#define DBGMSK_LCD_G3   "\xFC\x02\x08"
#define DBGMSK_LCD_G4   "\xFC\x02\x10"
#define DBGMSK_LCD_G5   "\xFC\x02\x20"
#define DBGMSK_LCD_G6   "\xFC\x02\x40"
#define DBGMSK_LCD_G7   "\xFC\x02\x80"
//ASUS_BSP --- Mickey 20101001 add for LCD debug mask

// Add for Battery debug mask Bruno 20101001
#define DBGMSK_BAT_G0   "\xFC\x03\x01"
#define DBGMSK_BAT_G1   "\xFC\x03\x02"
#define DBGMSK_BAT_G2   "\xFC\x03\x04"
#define DBGMSK_BAT_G3   "\xFC\x03\x08"
#define DBGMSK_BAT_G4   "\xFC\x03\x10"
#define DBGMSK_BAT_G5   "\xFC\x03\x20"
#define DBGMSK_BAT_G6   "\xFC\x03\x40"
#define DBGMSK_BAT_G7   "\xFC\x03\x80"
// Add for Battery debug mask Bruno 20101001

// Add for KeyPad debug mask Bruno 20101001
#define DBGMSK_KPD_G0   "\xFC\x04\x01"
#define DBGMSK_KPD_G1   "\xFC\x04\x02"
#define DBGMSK_KPD_G2   "\xFC\x04\x04"
#define DBGMSK_KPD_G3   "\xFC\x04\x08"
#define DBGMSK_KPD_G4   "\xFC\x04\x10"
#define DBGMSK_KPD_G5   "\xFC\x04\x20"
#define DBGMSK_KPD_G6   "\xFC\x04\x40"
#define DBGMSK_KPD_G7   "\xFC\x04\x80"
// Add for KeyPad debug mask Bruno 20101001

//Larry Lai#20101001 add i2c debug message mask
#define DBGMSK_I2C_G0   "\xFC\x05\x01"    // normal
#define DBGMSK_I2C_G1   "\xFC\x05\x02"    // buses, algos
#define DBGMSK_I2C_G2   "\xFC\x05\x04"    //
#define DBGMSK_I2C_G3   "\xFC\x05\x08"    // 
#define DBGMSK_I2C_G4   "\xFC\x05\x10"    //
#define DBGMSK_I2C_G5   "\xFC\x05\x20"    //
#define DBGMSK_I2C_G6   "\xFC\x05\x40"    //
#define DBGMSK_I2C_G7   "\xFC\x05\x80"    // stress

//Joe chang touch message mask
#define DBGMSK_TCH_G0   "\xFC\x06\x01"    
#define DBGMSK_TCH_G1   "\xFC\x06\x02"    
#define DBGMSK_TCH_G2   "\xFC\x06\x04"    
#define DBGMSK_TCH_G3   "\xFC\x06\x08"    
#define DBGMSK_TCH_G4   "\xFC\x06\x10"    
#define DBGMSK_TCH_G5   "\xFC\x06\x20"    
#define DBGMSK_TCH_G6   "\xFC\x06\x40"    
#define DBGMSK_TCH_G7   "\xFC\x06\x80"    

//Ledger ++#20101013 add power manger debug message mask
#define DBGMSK_PWR_G0   "\xFC\x07\x01"    //
#define DBGMSK_PWR_G1   "\xFC\x07\x02"    //
#define DBGMSK_PWR_G2   "\xFC\x07\x04"    //
#define DBGMSK_PWR_G3   "\xFC\x07\x08"    //
#define DBGMSK_PWR_G4   "\xFC\x07\x10"    //
#define DBGMSK_PWR_G5   "\xFC\x07\x20"    //
#define DBGMSK_PWR_G6   "\xFC\x07\x40"    //
#define DBGMSK_PWR_G7   "\xFC\x07\x80"    //
//Ledger --#20101013 add power manger debug message mask

//Rice ++#20101014 add camera debug message mask
#define DBGMSK_CAM_G0   "\xFC\x08\x01"    //
#define DBGMSK_CAM_G1   "\xFC\x08\x02"    //5M
#define DBGMSK_CAM_G2   "\xFC\x08\x04"    //VGA
#define DBGMSK_CAM_G3   "\xFC\x08\x08"    //
#define DBGMSK_CAM_G4   "\xFC\x08\x10"    //
#define DBGMSK_CAM_G5   "\xFC\x08\x20"    //
#define DBGMSK_CAM_G6   "\xFC\x08\x40"    //
#define DBGMSK_CAM_G7   "\xFC\x08\x80"    //
//Rice --#20101014 add camera debug message mask


//Ledger ++#20101101 add Vibrator debug message mask
#define DBGMSK_VIB_G0   "\xFC\x09\x01"    //
#define DBGMSK_VIB_G1   "\xFC\x09\x02"    //
#define DBGMSK_VIB_G2   "\xFC\x09\x04"    //
#define DBGMSK_VIB_G3   "\xFC\x09\x08"    //
#define DBGMSK_VIB_G4   "\xFC\x09\x10"    //
#define DBGMSK_VIB_G5   "\xFC\x09\x20"    //
#define DBGMSK_VIB_G6   "\xFC\x09\x40"    //
#define DBGMSK_VIB_G7   "\xFC\x09\x80"    //
//Ledger --#20101101 add Vibrator debug message mask

//Larry Lai#20110526 add i2c bus debug mask
#define DBGMSK_TWSI_G0   "\xFC\x0A\x01"    // gen1 i2c 
#define DBGMSK_TWSI_G1   "\xFC\x0A\x02"    // gen2 i2c
#define DBGMSK_TWSI_G2   "\xFC\x0A\x04"    // cam i2c
#define DBGMSK_TWSI_G3   "\xFC\x0A\x08"    // ddc i2c
#define DBGMSK_TWSI_G4   "\xFC\x0A\x10"    // power i2c
#define DBGMSK_TWSI_G5   "\xFC\x0A\x20"    //
#define DBGMSK_TWSI_G6   "\xFC\x0A\x40"    //
#define DBGMSK_TWSI_G7   "\xFC\x0A\x80"    // 

//ASUS_BSP+++ Wenli "smd debug mask"
#define DBGMSK_SMD_G0   "\xFC\x0B\x01"    // SMD Error
#define DBGMSK_SMD_G1   "\xFC\x0B\x02"    // SMD Warm
#define DBGMSK_SMD_G2   "\xFC\x0B\x04"    // SMD Info
#define DBGMSK_SMD_G3   "\xFC\x0B\x08"    // SMD Debug
#define DBGMSK_SMD_G4   "\xFC\x0B\x10"    // smd_xxx Error
#define DBGMSK_SMD_G5   "\xFC\x0B\x20"    // smd_xxx Warm
#define DBGMSK_SMD_G6   "\xFC\x0B\x40"    // smd_xxx Info
#define DBGMSK_SMD_G7   "\xFC\x0B\x80"    // smd_xxx Debug
//ASUS_BSP--- Wenli "smd debug mask"

//[cm3623] for light and proximity sensor debug mask
#define DBGMSK_PRX_G0   "\xFC\x0C\x01"    // CM3623 & AL3010 Error
#define DBGMSK_PRX_G1   "\xFC\x0C\x02"    // CM3623 & AL3010 Warm
#define DBGMSK_PRX_G2   "\xFC\x0C\x04"    // CM3623 & AL3010 Info
#define DBGMSK_PRX_G3   "\xFC\x0C\x08"    // Light sensor Info
#define DBGMSK_PRX_G4   "\xFC\x0C\x10"    // Proximity Info
#define DBGMSK_PRX_G5   "\xFC\x0C\x20"    // ATD Info
#define DBGMSK_PRX_G6   "\xFC\x0C\x40"    // AL3010 Info
#define DBGMSK_PRX_G7   "\xFC\x0C\x80"    // CM3623 Debug Trace

//backlight driver debug mask
#define DBGMSK_BL_G0   "\xFC\x0d\x01"    // 
#define DBGMSK_BL_G1   "\xFC\x0d\x02"    // 
#define DBGMSK_BL_G2   "\xFC\x0d\x04"    // 
#define DBGMSK_BL_G3   "\xFC\x0d\x08"    //
#define DBGMSK_BL_G4   "\xFC\x0d\x10"    // 
#define DBGMSK_BL_G5   "\xFC\x0d\x20"    // 
#define DBGMSK_BL_G6   "\xFC\x0d\x40"    // 
#define DBGMSK_BL_G7   "\xFC\x0d\x80"    // 

//Ledger ++#20120311 add gpio debug message mask
#define DBGMSK_GIO_G0   "\xFC\x0e\x01"    //
#define DBGMSK_GIO_G1   "\xFC\x0e\x02"    //
#define DBGMSK_GIO_G2   "\xFC\x0e\x04"    //
#define DBGMSK_GIO_G3   "\xFC\x0e\x08"    //
#define DBGMSK_GIO_G4   "\xFC\x0e\x10"    //
#define DBGMSK_GIO_G5   "\xFC\x0e\x20"    //
#define DBGMSK_GIO_G6   "\xFC\x0e\x40"    //
#define DBGMSK_GIO_G7   "\xFC\x0e\x80"    //
//Ledger --#20120311 add gpio debug message mask

/////////////////////////////////////////////////////////////////////////////////////////////
////                  Eventlog mask mechanism
/////////////////////////////////////////////////////////////////////////////////////////////
#define ASUS_EVTLOG_PATH "/asdf/ASUSEvtlog"
#define ASUS_EVTLOG_STR_MAXLEN (256)
#define ASUS_EVTLOG_MAX_ITEM (20)

void save_all_thread_info(void);
void delta_all_thread_info(void);
void save_phone_hang_log(void);
void save_last_shutdown_log(char *filename);
void printk_lcd(const char *fmt, ...);
void printk_lcd_xy(int xx, int yy, unsigned int color, const char *fmt, ...);
void ASUSEvtlog(const char *fmt, ...);
//20101202_Bruno: added to get debug mask value
bool isASUS_MSK_set(const char *fmt);

#endif
