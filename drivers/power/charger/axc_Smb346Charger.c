/*
        Smb 346 Charger IC Implementation

*/
/*
	Definition on TD
	GPIO57	GPIO58	N/A
	PEN1	PEN2	USUS
	H	x	x	3000/Rpset
	L	H	L	475mA
	L	L	L	95mA
	L	x	H	0mA(UsbSuspend)
*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>

#include "axc_Smb346Charger.h"
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#ifdef CONFIG_EEPROM_NUVOTON
#include <linux/microp_notify.h>
#include <linux/microp_notifier_controller.h>	//ASUS_BSP Lenter+
#include <linux/microp_api.h>
#endif /*CONFIG_EEPROM_NUVOTON */
#include <linux/platform_device.h>//ASUS_BSP Eason_Chang 1120 porting
#include <linux/i2c.h>//ASUS BSP Eason_Chang smb346
#include <linux/module.h>//ASUS BSP Eason_Chang A68101032 porting
#include <linux/asus_bat.h>//Hank: move out of qpnp-charger

#ifdef CONFIG_CHARGER_MODE
extern int g_chg_present;
extern char g_CHG_mode;
#endif

//Eason_Chang:for A90 internal ChgGau+++
static bool   g_smb346_Psy_ready = false;
//Eason_Chang:for A90 internal ChgGau---

//Eason takeoff Battery shutdown +++
extern bool g_AcUsbOnline_Change0;
//Eason takeoff Battery shutdown ---
//Eason : when thermal too hot, limit charging current +++ 
//thermal limit level 0 : no limit
//thermal limit level 1: hot and low battery
//thermal limit level 2: hot but not very low battery
//thermal limit level 3: hot but no low battery
extern int g_thermal_limit;
extern bool g_audio_limit;
extern bool g_padMic_On;
//Eason : when thermal too hot, limit charging current ---

//Eason: AICL work around +++
extern int smb346_getCapacity(void);
//static struct delayed_work AICLWorker; //Hank: remove AICL worker+++
extern bool g_alreadyCalFirstCap;
#ifndef ASUS_FACTORY_BUILD
static bool g_chgTypeBeenSet = false;//Eason : prevent setChgDrawCurrent_smb346 before get chgType
static bool g_AICLlimit = false;
//Hank: remove 1A adapter protection+++
//static bool g_AICLSuccess = false;
//static unsigned long AICL_success_jiffies;
//Hank: remove 1A adapter protection---
#endif//#ifndef ASUS_FACTORY_BUILD
#include <linux/jiffies.h>
static AXE_Charger_Type lastTimeCableType = NO_CHARGER_TYPE;
#define ADAPTER_PROTECT_DELAY (7*HZ)

//Eason: AICL work around ---

//Eason: In phone OTG mode, skip Chg I2C can't write rule +++
/*          cause When enable OTG need using I2C set current500 & UVLO3.1V  
 *  
 *           - OtgCanWriteI2C : sync with bool switchOtg in UsbSetOtgSwitch()
 *                                      only used in smb346_i2c_write
 */
static bool OtgCanWriteI2C = false;
//Eason: In phone OTG mode, skip Chg I2C can't write rule ---

//ASUS BSP Eason_Chang smb346+++
extern void msleep(unsigned int msecs);
static u32 g_smb346_slave_addr=0;
static int g_smb346_reg_value=0;
static int g_smb346_reg_address=0;
static struct smb346_info *g_smb346_info=NULL;
struct smb346_platform_data{
        int intr_gpio;
};

struct smb346_info {
       struct i2c_client *i2c_client;
       struct smb346_platform_data *pdata;
};
//ASUS BSP Eason_Chang smb346---
//Eason: A80 OTG pin control +++
#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE                NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)
//static int PMICgpio_15; //Eason:A86 porting
//Eason: A80 OTG pin control ---

//Eason : read Volt Curr Temp from Gauge+++
extern int get_Temp_from_TIgauge(void);
extern int get_Curr_from_TIgauge(void);
extern int get_Volt_from_TIgauge(void);
//Eason : read Volt Curr Temp from Gauge---

//Eason: in Pad AC powered, judge AC powered true+++
extern int InP03JudgeACpowered(void);
//Eason: in Pad AC powered, judge AC powered true---

//Hank; add i2c stress test support+++
#ifdef CONFIG_I2C_STRESS_TEST
#include <linux/i2c_testcase.h>
#define I2C_TEST_SMB346_FAIL (-1)
#endif
//Hank; add i2c stress test support---
extern bool DffsDown;

//Hank: show "?" when i2c error+++
#ifdef CONFIG_BAT_DEBUG
extern void asus_bat_status_change(void);
extern bool g_IsInRomMode;
#endif
//Hank: show "?" when i2c error---

//Hank:SMB346_INOK global flag+++
bool SMB346_INOK_Online = 0;
//Hank:SMB346_INOK global flag---

#ifdef CONFIG_SMB_346_CHARGER
static AXC_SMB346Charger *gpCharger = NULL;

//ASUS BSP Eason_Chang smb346 +++
struct smb346_microp_command{
    char *name;;
    u8 addr;
    u8 len;
    enum readwrite{
		E_READ=0,
		E_WRITE=1,
		E_READWRITE=2,
		E_NOUSE=3,

    }rw;

};

struct smb346_microp_command smb346_CMD_Table[]={
        {"chg_cur",                   0x00,  1,   E_READWRITE},         //SMB346_CHG_CUR
        {"cur_lim",                   0x01,  1,   E_READWRITE},         //SMB346_INPUT_CUR_LIM
        {"var_fun",                   0x02,  1,   E_READWRITE},         //SMB346_VARIOUS_FUNCTIONS
        {"float_vol",                 0x03,  1,   E_READWRITE},         //SMB346_FLOAT_VOLTAGE
        {"chg_ctrl",                  0x04,  1,   E_READWRITE},         //SMB346_CHARGE_CONTROL
        {"stat_timers_ctrl",          0x05,  1,   E_READWRITE},         //SMB346_STAT_TIMER_CONTROL
        {"pin_enable_ctrl",           0x06,  1,   E_READWRITE},         //SMB346_PIN_ENABLE_CONTROL
        {"therm_system",              0x07,  1,   E_READWRITE},         //SMB346_THERM_SYSTEM_CONTROL
        {"sysok_usb3.0",              0x08,  1,   E_READWRITE},         //SMB346_SYSOK_USB3p0
        {"other_ctrl",                0x09,  1,   E_READWRITE},         //SMB346_OTHER_CONTROL_A
        {"otg_tlim_therm",            0x0A,  1,   E_READWRITE},         //SMB346_TLIM_THERM_CONTROL
        {"hw_sw_lim_cell_temp",       0x0B,  1,   E_READWRITE},         //SMB346_HW_SW_LIMIT_CELL_TEMP
        {"fault interrupt",           0x0C,  1,   E_READ},              //SMB346_FAULT_INTERRUPT
        {"status interrupt",          0x0D,  1,   E_READ},              //SMB346_STATUS_INTERRUPT
        {"i2c_bus_slave_add",         0x0E,  1,   E_READ},              //SMB346_I2C_BUS_SLAVE
        {"command_reg_a",             0x30,  1,   E_READWRITE},         //SMB346_COMMAND_REG_A
        {"command_reg_b",             0x31,  1,   E_READWRITE},         //SMB346_COMMAND_REG_B
        {"command_reg_c",             0x33,  1,   E_READ},              //SMB346_COMMAND_REG_C
        {"interrupt_reg_a",           0x35,  1,   E_READ},              //SMB346_INTERRUPT_REG_A
        {"interrupt_reg_b",           0x36,  1,   E_READ},              //SMB346_INTERRUPT_REG_B
        {"interrupt_reg_c",           0x37,  1,   E_READ},              //SMB346_INTERRUPT_REG_C
        {"interrupt_reg_d",           0x38,  1,   E_READ},              //SMB346_INTERRUPT_REG_D
        {"interrupt_reg_e",           0x39,  1,   E_READ},              //SMB346_INTERRUPT_REG_E
        {"interrupt_reg_f",           0x3A,  1,   E_READ},              //SMB346_INTERRUPT_REG_F
        {"status_reg_a",              0x3B,  1,   E_READ},              //SMB346_STATUS_REG_A
        {"status_reg_b",              0x3C,  1,   E_READ},              //SMB346_STATUS_REG_B
        {"status_reg_c",              0x3D,  1,   E_READ},              //SMB346_STATUS_REG_C
        {"status_reg_d",              0x3E,  1,   E_READ},              //SMB346_STATUS_REG_D
        {"status_reg_e",              0x3F,  1,   E_READ},              //SMB346_STATUS_REG_E                
};

//ASUS BSP Eason add A68 charge mode +++
#define SMB346_R03_defaultValue 232
#define SMB346_R03_noFloatVoltage 192
#define OCV_TBL_SIZE 101

int NVT_FloatV_Tbl[OCV_TBL_SIZE]={
	3500, 3500, 3560, 3620, 3660, 3680, 3680, 3700, 3700, 3700,
	3700, 3700, 3700, 3700, 3720, 3720, 3740, 3740, 3760, 3760,
	3760, 3760, 3780, 3780, 3780, 3780, 3780, 3800, 3800, 3800,
	3800, 3800, 3800, 3820, 3820, 3820, 3820, 3820, 3820, 3820,
	3820, 3820, 3840, 3840, 3840, 3840, 3840, 3840, 3860, 3860,

	3860, 3860, 3860, 3880, 3880, 3880, 3880, 3900, 3900, 3900,
	3920, 3920, 3940, 3940, 3960, 3960, 3960, 3980, 3980, 3980,
	4000, 4000, 4000, 4020, 4020, 4020, 4040, 4040, 4060, 4060,
	4080, 4080, 4080, 4100, 4100, 4120, 4120, 4140, 4140, 4160,
	4160, 4180, 4180, 4200, 4200, 4220, 4220, 4240, 4240, 4260,
	4260
};
//ASUS BSP Eason add A68 charge mode ---

static int smb346_i2c_read(u8 addr, int len, void *data)
{
        int i=0;
        int retries=6;
        int status=0;

	struct i2c_msg msg[] = {
		{
			.addr = g_smb346_slave_addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = g_smb346_slave_addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

    pr_debug("[BAT][CHG][SMB][I2c]smb346_i2c_read+++\n");

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return -EINVAL; 
	}
	//Eason_Chang:for A90 internal ChgGau---
    
        if(g_smb346_info){
            do{    
                pr_debug("[BAT][CHG][SMB][I2c]before smb346_i2c_transfer\n");
        		status = i2c_transfer(g_smb346_info->i2c_client->adapter,
        			msg, ARRAY_SIZE(msg));
                
        		if ((status < 0) && (i < retries)){
        			    msleep(5);
                      
                                printk("[BAT][CHG][SMB][I2c]%s retry %d\r\n", __FUNCTION__, i);                                
                                i++;
                     }
        	    } while ((status < 0) && (i < retries));
        }
        if(status < 0)
	{
            printk("[BAT][CHG][SMB][I2c]smb346: i2c read error %d \n", status);
	     //Hank: show "?" when i2c error+++
	    #ifdef CONFIG_BAT_DEBUG
           g_IsInRomMode = true;
	    asus_bat_status_change();
	    printk("[BAT][CHG][SMB][I2c]g_IsInRomMode: %d \n", (int)g_IsInRomMode);	
	    #endif
	    //Hank: show "?" when i2c error---		
        }
    pr_debug("[BAT][CHG][SMB][I2c]smb346_i2c_read---\n");
    

        return status;
        
}

int smb346_read_reg(int cmd, void *data)
{
    int status=0;
    if(cmd>=0 && cmd < ARRAY_SIZE(smb346_CMD_Table)){
        if(E_WRITE==smb346_CMD_Table[cmd].rw || E_NOUSE==smb346_CMD_Table[cmd].rw){ // skip read for these command
            printk("[BAT][CHG][SMB][I2c]smb346: read ignore cmd\r\n");      
        }
        else{   
            pr_debug("[BAT][CHG][SMB][I2c]smb346_read_reg\n");
            status=smb346_i2c_read(smb346_CMD_Table[cmd].addr, smb346_CMD_Table[cmd].len, data);
        }    
    }
    else
        printk("[BAT][CHG][SMB][I2c]smb346: unknown read cmd\r\n");
            
    return status;
}

void smb346_proc_read(void)
{    
       int status;
       int reg_value=0;
       printk("[BAT][CHG][SMB][Proc]%s \r\n", __FUNCTION__);
       status=smb346_read_reg(g_smb346_reg_address,&reg_value);
       g_smb346_reg_value = reg_value;

       if(status > 0 && reg_value >= 0){
            printk("[BAT][CHG][SMB][Proc]found! Charge Current=%d\r\n",reg_value);
       }
}

static int smb346_i2c_write(u8 addr, int len, void *data)
{
    int i=0;
	int status=0;
	u8 buf[len + 1];
	struct i2c_msg msg[] = {
		{
			.addr = g_smb346_slave_addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};
	int retries = 6;
	int nocable_retry = 2;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return -EINVAL; 
	}
	//Eason_Chang:for A90 internal ChgGau---

	buf[0] = addr;
	memcpy(buf + 1, data, len);
	while( (gpio_get_value(27) && i < nocable_retry) && (false == OtgCanWriteI2C) )
	{
		pr_debug("[BAT][CHG][SMB][I2c]%s(): No cable-in delay 10ms retry:%d\r\n", __FUNCTION__,i);
		i++;
		msleep(10);
	}
	if(i == nocable_retry)
	{
		printk("[BAT][CHG][SMB][I2c]%s(): No cable-in do not set charger\r\n", __FUNCTION__);
		return -1;
	}
	i = 0;
	
	do {
		status = i2c_transfer(g_smb346_info->i2c_client->adapter,
			msg, ARRAY_SIZE(msg));
        	if ((status < 0) && (i < retries)){
                    msleep(5);
           
                    printk("[BAT][CHG][SMB][I2c]%s retry %d\r\n", __FUNCTION__, i);
                    i++;
              }
       } while ((status < 0) && (i < retries));

	if (status < 0) 
	{
              printk("[BAT][CHG][SMB][I2c]smb346: i2c write error %d \n", status);
		//Hank: show "?" when i2c error+++
	      #ifdef CONFIG_BAT_DEBUG
             g_IsInRomMode = true;
	      asus_bat_status_change();
	      printk("[BAT][CHG][SMB][I2c]g_IsInRomMode: %d \n", (int)g_IsInRomMode);	  
	      #endif
	      //Hank: show "?" when i2c error---
	}

	return status;
}

static int smb346_write_reg(int cmd, void *data){
    int status=0;
    if(cmd>=0 && cmd < ARRAY_SIZE(smb346_CMD_Table)){
        if(E_READ==smb346_CMD_Table[cmd].rw  || E_NOUSE==smb346_CMD_Table[cmd].rw){ // skip read for these command               
        }
        else
            status=smb346_i2c_write(smb346_CMD_Table[cmd].addr, smb346_CMD_Table[cmd].len, data);
    }
    else
        printk("[BAT][CHG][SMB][I2c]smb346: unknown write cmd\r\n");
            
    return status;
}

void smb346_proc_write(void)
{    
       int status;
       uint8_t i2cdata[32]={0};

       i2cdata[0] = g_smb346_reg_value;
       printk("[BAT][CHG][SMB][Proc]%s:%d \r\n", __FUNCTION__,g_smb346_reg_value);
       status=smb346_write_reg(g_smb346_reg_address,i2cdata);

       if(status > 0 ){
            printk("[BAT][CHG][SMB][Proc] proc write\r\n");
       }
}

void SMB346_Down_FlotVolt(void)
{
	int status;
     	uint8_t i2cdata[32]={0};
	i2cdata[0] = 222;//Down Voltage to 4.1V
	status=smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);
	if(status > 0 )
		printk("[BAT][SER][SMB346] Down Float Voltage Success \n");
	else
		printk("[BAT][SER][SMB346] Down Float Voltage Fail \n");
}

void SMB346_Default_FlotVolt(void)
{
	int status;
     	uint8_t i2cdata[32]={0};
	i2cdata[0] = 233;//Default Voltage to 4.32V
	status=smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);
	if(status > 0 )
		printk("[BAT][SER][SMB346] Default Float Voltage Success \n");
	else
		printk("[BAT][SER][SMB346] Default Float Voltage Fail \n");
}

void SMB346_Down_CCcurr(void)
{
	int status;
     	uint8_t i2cdata[32]={0};
	i2cdata[0] = 18;
	status=smb346_write_reg(SMB346_CHG_CUR,i2cdata);//Down current
	if(status > 0 )
		printk("[BAT][SER][SMB346] Down Charging Current Success \n");
	else
		printk("[BAT][SER][SMB346] Down Charging Current Fail \n");
}

void SMB346_Default_CCcurr(void)
{
	int status;
     	uint8_t i2cdata[32]={0};
	i2cdata[0] = 145;
	status=smb346_write_reg(SMB346_CHG_CUR,i2cdata);//Default Current
	if(status > 0 )
		printk("[BAT][SER][SMB346] Default Fast Charging Current \n");
	else
		printk("[BAT][SER][SMB346] Default Fast Charging Current Fail \n");
}

void smb346_write_enable(void)
{    
       int status;
       uint8_t i2cdata[32]={0};

	//ASUS BSP Eason_Chang OTG +++
	status=smb346_read_reg(SMB346_COMMAND_REG_A,i2cdata);
	i2cdata[0] |= 0x80;
	//ASUS BSP Eason_Chang OTG ---

	pr_debug("[BAT][CHG][SMB]%s \r\n", __FUNCTION__);
	status=smb346_write_reg(SMB346_COMMAND_REG_A,i2cdata);

	if(status > 0 ){
		pr_debug("[BAT][CHG][SMB] write enable\r\n");
	}
}

//ASUS BSP Eason_Chang OTG +++
/*
static void smb346OtgEnableBit(bool switchOtgBit)
{
     int status;
     uint8_t i2cdata[32]={0};

     if(true == switchOtgBit)
     {
		i2cdata[0] = 0x90;//cause smb346_write_enable SMB346_COMMAND_REG_A(R30h) will be 0x80
		status=smb346_write_reg(SMB346_COMMAND_REG_A,i2cdata);
		if(status > 0 ){
			printk("[BAT][CHG][SMB]  otg enableBit true\r\n");
		}
     }else{
		i2cdata[0] = 0x80;//cause smb346_write_enable SMB346_COMMAND_REG_A(R30h) will be 0x80
		status=smb346_write_reg(SMB346_COMMAND_REG_A,i2cdata);
		if(status > 0 ){
			printk("[BAT][CHG][SMB] otg enableBit false\r\n");
		}
     }
}
*/

//Hank: ME771KL OTG setting+++
#ifdef ASUS_ME771KL_PROJECT
static void smb345Otg750Ma(bool switchOtg750)
{
     int status;
     uint8_t i2cdata[32]={0};

     if(true == switchOtg750)
     {
		i2cdata[0] = 0xBC;//set otg 750 mA & OTG_uvlo_2p7V-
		status=smb346_write_reg(SMB346_TLIM_THERM_CONTROL,i2cdata);
		if(status <= 0 ){
			printk("[BAT][CHG][SMB]set otg 750Ma true: fail!!\r\n");
		}
		else{
			pr_debug("[BAT][CHG][SMB]set otg 750Ma true: success!!\r\n");
		}
     }else{
		i2cdata[0] = 0xB4;//default  OTG Output Current Limit 250 mA & OTG_uvlo_2p7V-
		status=smb346_write_reg(SMB346_TLIM_THERM_CONTROL,i2cdata);
		if(status <= 0 ){
				printk("[BAT][CHG][SMB]set otg 750Ma false: fail!!\r\n");
		}
		else{
			pr_debug("[BAT][CHG][SMB]set otg 750Ma false: success!!\r\n");
		}
     }
}
#endif
//Hank: ME771KL OTG setting---
//#ifdef ASUS_A91_PROJECT
static void smb346Otg500Ma(bool switchOtg500)
{
     int status;
     uint8_t i2cdata[32]={0};

     if(true == switchOtg500)
     {
     		//Hank: charger 1895 porting+++
		i2cdata[0] = 0x4A;//set otg 500 mA & OTG_uvlo_3p1V
		//Hank: charger 1895 porting---
		status=smb346_write_reg(SMB346_TLIM_THERM_CONTROL,i2cdata);
		if(status > 0 ){
			pr_debug("[BAT][CHG][SMB] otg 500Ma true\r\n");
		}
		else{
			printk("[BAT][CHG][SMB] otg 500Ma true:fail!!\r\n");
		}
     }else{
     		//Hank: charger 1895 porting+++
		i2cdata[0] = 0x44;//default Charge Current Compensation 350mA , OTG Output Current Limit 250 mA
		//Hank: charger 1895 porting---
		status=smb346_write_reg(SMB346_TLIM_THERM_CONTROL,i2cdata);
		if(status > 0 ){
				pr_debug("[BAT][CHG][SMB] otg 500Ma false\r\n");
		}
		else{
			printk("[BAT][CHG][SMB] otg 500Ma fals:fail!!\r\n");
		}
     }
}
//#endif




//Eason: for A80 use OTG need change INOK to active high +++
extern enum DEVICE_HWID g_ASUS_hwID;
#define GPIO_CHG_OTG 52
//Eason:A90_EVB porting+++
/*
static void smb346_INOK_active_High(bool IsAcitveHigh)
{
     int status;
     uint8_t i2cdata[32]={0};
	 
     status=smb346_read_reg(SMB346_SYSOK_USB3p0,i2cdata);
	 
     if(true == IsAcitveHigh)
     {
		i2cdata[0] |= 0x01;//INOK active high
		status=smb346_write_reg(SMB346_SYSOK_USB3p0,i2cdata);
		if(status > 0 ){
			pr_debug("[BAT][CHG][SMB]%s(): inok active high\r\n",__FUNCTION__);
		}
		else{
			printk("[BAT][CHG][SMB]%s(): inok active high:fail!!\r\n",__FUNCTION__);
		}
     }else{
		i2cdata[0] &= 0xfe;//INOK active low
		status=smb346_write_reg(SMB346_SYSOK_USB3p0,i2cdata);
		if(status > 0 ){
			pr_debug("[BAT][CHG][SMB]%s(): inok active low\r\n",__FUNCTION__);
		}
		else{
			printk("[BAT][CHG][SMB]%s():  inok active high:fail!!\r\n",__FUNCTION__);
		}
     }	
	
}
//Eason: for A80 use OTG need change INOK to active high ---
*/
//Eason:A90_EVB porting---

static void set_otg_curr(bool switchOtg)
{
	//#ifdef ASUS_A91_PROJECT
		smb346Otg500Ma(switchOtg);
	//#endif
	#ifdef ASUS_ME771KL_PROJECT
		smb345Otg750Ma(switchOtg);
	#endif
}

//Eason  "smb346_otg_enable"
/*
**   Enable OTG, need from default current 250mA type. Prevent CMOS burn out. 
*
*   1. enable/disable OTG by 
*   (1)I2C   :smb346OtgEnableBit(switchOtg)
*   (2)GPIO :gpio_direction_output(GPIO_CHG_OTG, switchOtg)
*
*   2. set OTG current by set_otg_curr
*   (1)default set OTG output curr 250mA, no matter enable/disable OTG 
*   (2)change output current while enable OTG 
*/
static void smb346_otg_enable(bool switchOtg)
{
		smb346_write_enable();//set current need do, ex:smb346Otg500Ma

		set_otg_curr(false);//default set OTG output curr 250mA, no matter enable/disable OTG

		//ASUS_BSP+++ BennyCheng "add vbus output support by charger IC for usb OTG"
		/*
		 * enable/disable vbus output by CHG_OTG gpio
		 */
		//ASUS_BSP+++ BennyCheng "add phone mode usb OTG support"
		if (g_ASUS_hwID == A90_EVB0) 
		{
			gpio_direction_output(GPIO_CHG_OTG, switchOtg);
		}
		//ASUS_BSP--- BennyCheng "add phone mode usb OTG support"
		//ASUS_BSP--- BennyCheng "add vbus output support by charger IC for usb OTG"
	
		if(true == switchOtg)
			set_otg_curr(switchOtg);//change output current while enable OTG 
		
		printk("[BAT][CHG][SMB]%s(): otg switch:%d\r\n",__FUNCTION__,switchOtg);
}

//Eason: "UsbSetOtgSwitch"
/*
**In phone OTG mode, skip Chg I2C can't write rule 
**Cause in smb346_i2c_write if (false == OtgCanWriteI2C)  can't write I2C
*
*(ture == switchOtg)
*         need do I2C step after  OtgCanWriteI2C=true be set, prevent can't set OTG enabled value when plug in OTG
*
*(false == switchOtg)
*          need do I2C step before  OtgCanWriteI2C=false be set, prevent can't set OTG default value when plug out OTG
*/
void UsbSetOtgSwitch_smb346(bool switchOtg)
{
	if(true == switchOtg)
		OtgCanWriteI2C = switchOtg;
	
	smb346_otg_enable(switchOtg);

	if(false == switchOtg)
		OtgCanWriteI2C = switchOtg;
}

//ASUS BSP Eason_Chang OTG ---
//ASUS BSP Eason_Chang smb346 ---

//ASUS BSP Eason add A68 charge mode +++
static void setSmb346FloatVoltage(int reg_value)
{
     uint8_t i2cdata[32]={0};
     i2cdata[0] = reg_value;

     smb346_write_enable();
     smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);	
}

void setFloatVoltage(int StopPercent)
{
	int reg_add_value = 0;
        int reg_value = SMB346_R03_defaultValue;//R03 default value

	reg_add_value = (NVT_FloatV_Tbl[StopPercent]-3500)/20;
        reg_value = SMB346_R03_noFloatVoltage + reg_add_value; 
	printk("[BAT][CHG][SMB]StopVoltage:%d,R03reg_value: %d\n",NVT_FloatV_Tbl[StopPercent],reg_value);

        setSmb346FloatVoltage(reg_value);
}
//ASUS BSP Eason add A68 charge mode ---


// For checking initializing or not
static bool AXC_Smb346_Charger_GetGPIO(int anPin)
{
	return gpio_get_value(anPin);
}

static void AXC_Smb346_Charger_SetGPIO(int anPin, bool abHigh)
{
	gpio_set_value(anPin,abHigh);
	printk( "[BAT][CHG]SetGPIO pin%d,%d:%d\n",anPin,abHigh,AXC_Smb346_Charger_GetGPIO(anPin));
}

#ifdef CONFIG_ENABLE_PIN1  
static void AXC_Smb346_Charger_SetPIN1(AXC_SMB346Charger *this,bool abHigh)
{ 
	AXC_Smb346_Charger_SetGPIO(this->mpGpio_pin[Smb346_PIN1].gpio,abHigh);
}
#endif
#ifdef CONFIG_ENABLE_PIN2
static void AXC_Smb346_Charger_SetPIN2(AXC_SMB346Charger *this,bool abHigh)
{
	AXC_Smb346_Charger_SetGPIO(this->mpGpio_pin[Smb346_PIN2].gpio,abHigh);
}
#endif
extern bool DisChg;
static void AXC_Smb346_Charger_SetChargrDisbalePin(AXC_SMB346Charger *this,bool abDisabled)
{
	AXC_Smb346_Charger_SetGPIO(this->mpGpio_pin[Smb346_CHARGING_DISABLE].gpio,abDisabled);
	//Hank: disable charging by write register+++	
	/*
	int status = 0;
       uint8_t i2cdata[32]={0}; 
	
	if(abDisabled)
	{
		i2cdata[0] = 17;
		status=smb346_write_reg(SMB346_PIN_ENABLE_CONTROL,i2cdata);
		if(status > 0 )
		      printk("[BAT][CHG][SMB] Disable Charging Success \n");
		else
		      printk("[BAT][CHG][SMB] Disable Charging Fail \n");
	}
	else
	{
		i2cdata[0] = 113;
		status=smb346_write_reg(SMB346_PIN_ENABLE_CONTROL,i2cdata);
		if(status > 0 )
		      printk("[BAT][CHG][SMB] Enable Charging Success \n");
		else
		      printk("[BAT][CHG][SMB] Enable Charging Fail \n");
	}
	*/
	//Hank: disable charging by write register---
}
static void EnableCharging(struct AXI_Charger *apCharger, bool enabled)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

    AXC_Smb346_Charger_SetChargrDisbalePin(this,!enabled);
}

//Eason: Do VF with Cable when boot up+++
int getIfonline_smb346(void)
{
    return !gpio_get_value(gpCharger->mpGpio_pin[Smb346_DC_IN].gpio);
}
//Eason: Do VF with Cable when boot up---

static int AXC_Smb346_Charger_InitGPIO(AXC_SMB346Charger *this)
{
    Smb346_PIN_DEF i;

    int err = 0;
printk("[BAT][CHG][SMB]AXC_Smb346_Charger_InitGPIO+++\n");
    for(i = 0; i<Smb346_PIN_COUNT;i++){
        //request
        err  = gpio_request(this->mpGpio_pin[i].gpio, this->mpGpio_pin[i].name);
        if (err < 0) {
            printk( "[BAT][CHG][SMB]gpio_request %s failed, err = %d\n",this->mpGpio_pin[i].name, err);
            goto err_exit;
        }

        //input
        if(this->mpGpio_pin[i].in_out_flag == 0){

            err = gpio_direction_input(this->mpGpio_pin[i].gpio);
            
            if (err  < 0) {
                printk( "[BAT][CHG][SMB]gpio_direction_input %s failed, err = %d\n", this->mpGpio_pin[i].name,err);
                goto err_exit;
            }

            if(this->mpGpio_pin[i].handler != NULL){

                this->mpGpio_pin[i].irq = gpio_to_irq(this->mpGpio_pin[i].gpio);

			printk("[BAT][CHG][SMB]:GPIO:%d,IRQ:%d\n",this->mpGpio_pin[i].gpio,  this->mpGpio_pin[i].irq);
                if(true == this->mpGpio_pin[i].irq_enabled){
                    
                    enable_irq_wake(this->mpGpio_pin[i].irq);

                }

                err = request_irq(this->mpGpio_pin[i].irq , 
                    this->mpGpio_pin[i].handler, 
                    this->mpGpio_pin[i].trigger_flag,
                    this->mpGpio_pin[i].name, 
                    this);

                if (err  < 0) {
                    printk( "[BAT][CHG][SMB]request_irq %s failed, err = %d\n", this->mpGpio_pin[i].name,err);
                    goto err_exit;
                }

            }

        }else{//output

            gpio_direction_output(this->mpGpio_pin[i].gpio, 
                this->mpGpio_pin[i].init_value);            
        }
    }
printk("[BAT][CHG][SMB]AXC_Smb346_Charger_InitGPIO---\n");
    return 0;
    
err_exit:

    for(i = 0; i<Smb346_PIN_COUNT;i++){
        
        gpio_free(this->mpGpio_pin[i].gpio);
printk("[Eason]AXC_Smb346_Charger_InitGPIO_err_exit\n");        
    }
    
    return err;
}

static void AXC_Smb346_Charger_DeInitGPIO(AXC_SMB346Charger *this)
{
    Smb346_PIN_DEF i;
    
    for(i = 0; i<Smb346_PIN_COUNT;i++){
        
        gpio_free(this->mpGpio_pin[i].gpio);
        
    }
}

/*
static void AXC_Smb346_Charger_NotifyClientForStablePlugIn(struct work_struct *dat)
{
	AXC_SMB346Charger *this = container_of(dat,
                                                AXC_SMB346Charger,
                                                msNotifierWorker.work);

	this->msParentCharger.SetCharger(&this->msParentCharger, STABLE_CHARGER);
}
*/
//ASUS_BSP +++ Peter_lu "For fastboot mode"
#ifdef CONFIG_FASTBOOT
#include <linux/fastboot.h>
#endif //#ifdef CONFIG_FASTBOOT
//ASUS_BSP ---

static void (*notify_charger_in_out_func_ptr)(int) = NULL;
//Eason : support OTG mode+++
static void (*notify_charger_i2c_ready_func_ptr)(void) = NULL;
//Eason : support OTG mode---
static DEFINE_SPINLOCK(charger_in_out_debounce_lock);
static void charger_in_out_debounce_time_expired(unsigned long _data)
{
    unsigned long flags;
    struct AXC_SMB346Charger *this = (struct AXC_SMB346Charger *)_data;
    bool online;
    
    spin_lock_irqsave(&charger_in_out_debounce_lock, flags);

    online = !gpio_get_value(this->mpGpio_pin[Smb346_DC_IN].gpio);
	
    if(SMB346_INOK_Online != online)
    {
    	    SMB346_INOK_Online = online; 	
	    printk("[BAT][CHG][SMB]%s,%d\n",__FUNCTION__,SMB346_INOK_Online);

	    //Eason: set Chg Limit In Pad When Charger Reset +++
	    #ifndef ASUS_FACTORY_BUILD
	    if( (NORMAL_CURRENT_CHARGER_TYPE==this->type)&&(1==SMB346_INOK_Online) )
	    {
			printk("[BAT][CHG][PAD] Reset work\n");
			schedule_delayed_work(&this->setChgLimitInPadWhenChgResetWorker, msecs_to_jiffies(20));
	    }
	    #endif
	    //Eason: set Chg Limit In Pad When Charger Reset ---
	    //Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
	    if((1==SMB346_INOK_Online) )
	    {
	    		schedule_delayed_work(&this->checkGaugeFCNeedDisableChgWorker, msecs_to_jiffies(20));
	    }
	    //Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

	    wake_lock_timeout(&this->cable_in_out_wakelock, 3 * HZ);

	    //ASUS_BSP +++ Peter_lu "For fastboot mode wake up because cable in"
#ifdef CONFIG_FASTBOOT
	    if(is_fastboot_enable() && !AX_MicroP_IsP01Connected())
	    {
	        if(SMB346_INOK_Online)
		 {
	            printk("[FastBoot]Detect cable in\n");
	            ready_to_wake_up_and_send_power_key_press_event_in_fastboot(true);
	        }
	     }
#endif //#ifdef CONFIG_FASTBOOT
	    //ASUS_BSP ---

	    #ifdef CONFIG_CHARGER_MODE
	    g_chg_present = SMB346_INOK_Online;
	    #endif

	    if(NULL != notify_charger_in_out_func_ptr)
	    {
	    
	         (*notify_charger_in_out_func_ptr) (SMB346_INOK_Online);//asus_set_vbus_state()
	    
	    }
	    else
	    {
	    
	         printk("[BAT][CHG][SMB] No usb callback registed..\n");
	    }
    }
    else
		printk("[BAT][CHG][SMB] Same INOK state ignore INOK IRQ\n");
    
    enable_irq_wake(this->mpGpio_pin[Smb346_DC_IN].irq);

    spin_unlock_irqrestore(&charger_in_out_debounce_lock, flags);

}

static irqreturn_t charger_in_out_handler(int irq, void *dev_id)
{
    unsigned long flags;
    
    AXC_SMB346Charger *this = (AXC_SMB346Charger *)dev_id;

	printk("[BAT][CHG][SMB]charger_in_out_handler+++\n");

    if(!timer_pending(&this->charger_in_out_timer)){

        spin_lock_irqsave(&charger_in_out_debounce_lock, flags);

        disable_irq_wake(irq);

        mod_timer(&this->charger_in_out_timer, jiffies + msecs_to_jiffies(20));

        spin_unlock_irqrestore(&charger_in_out_debounce_lock, flags);

    }
	printk("[BAT][CHG][SMB]charger_in_out_handler---\n");
        
    return IRQ_HANDLED;

}
#ifdef ENABLE_WATCHING_STATUS_PIN_IN_IRQ
static void status_handle_work(struct work_struct *work)
{

    if(NULL == gpCharger){
        return;
    }

    printk("[BAT][CHG][SMB]%s,%d\n",__FUNCTION__,gpCharger->msParentCharger.IsCharging(&gpCharger->msParentCharger));

   if(true == gpCharger->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled){
        disable_irq_wake(gpCharger->mpGpio_pin[Smb346_CHARGING_STATUS].irq);
        gpCharger->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled = false;
    }    

    if(NULL != gpCharger->mpNotifier){

        gpCharger->mpNotifier->onChargingStart(&gpCharger->msParentCharger, gpCharger->msParentCharger.IsCharging(&gpCharger->msParentCharger));
    }    

}

static irqreturn_t charging_status_changed_handler(int irq, void *dev_id)
{
    
    AXC_SMB346Charger *this = (AXC_SMB346Charger *)dev_id;

    schedule_delayed_work(&this->asus_chg_work, (2000 * HZ/1000));
    
    return IRQ_HANDLED;

}
#endif

static int smb346_read_table(int tableNumber)
{    
       int status;
       int reg_value=0;

       status=smb346_read_reg(tableNumber,&reg_value);

       if(status > 0 && reg_value >= 0){
            pr_debug("[BAT][CHG][SMB][I2c] found! Charge Current=%d\r\n",reg_value);
       }
       return reg_value;
}

//Eason takeoff smb346 IRQ handler+++   
/*
static void smb346_readIRQSignal(void)
{

    int HardLimitIrq = 0;
    int AICLDoneIrq = 0;
    int BatOvpIrq = 0;
    int ReChgIrq = 0;
    int reg35_value = 0;
    int reg38_value = 0;
    int reg36_value = 0;
    int reg37_value = 0;

    printk("[BAT][smb346][IRQ]:read signal\n");

	reg35_value = smb346_read_table(18);//S35[6] Hot/Cold Hard Limit
        HardLimitIrq = reg35_value & 64;
        if(64==HardLimitIrq)
        {
		printk("[BAT][smb346][IRQ]:Hard Limit IRQ\n");
	}

	reg38_value = smb346_read_table(21);//S38[4] AICL Done
	AICLDoneIrq = reg38_value & 16;
	if(16==AICLDoneIrq)
	{
		printk("[BAT][smb346][IRQ]: AICL Done IRQ\n");		
	}

	reg36_value = smb346_read_table(19);//S36[6] Battery OVP
	BatOvpIrq = reg36_value & 64;
	if(64==AICLDoneIrq)
	{
		printk("[BAT][smb346][IRQ]: Bat OVP IRQ\n");		
	}

	reg37_value = smb346_read_table(20);//S37[4] Recharge
	ReChgIrq = reg37_value & 16;
	if(16==ReChgIrq)
	{
		printk("[BAT][smb346][IRQ]: Recharge IRQ\n");		
	}
}

static void IrqHandleWork(struct work_struct *work)
{
         smb346_readIRQSignal();
}

static irqreturn_t smb346_irq_handler(int irq, void *dev_id)
{ 
    AXC_SMB346Charger *this = (AXC_SMB346Charger *)dev_id;

	IsChgIRQ = gpio_get_value(this->mpGpio_pin[Smb346_CHARGING_IRQ].gpio);
    pr_debug("[BAT][smb346][IRQ]:%d\n",IsChgIRQ);
    schedule_delayed_work(&this->smb346_irq_worker, 0*HZ);
    return IRQ_HANDLED;  
}
*/
// Eason takeoff smb346 IRQ handler---  

//static void create_charger_proc_file(void); //ASUS BSP Eason_Chang smb346
static Smb346_PIN gGpio_pin[]=
{
    {//346_DC_IN
        .gpio = 27,
        .name = "346_DCIN",
        .in_out_flag = 0,
        .handler = charger_in_out_handler,
        .trigger_flag= IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, 
        .irq_enabled = false,        
    },
#ifdef CONFIG_ENABLE_PIN1       
    {//346_PIN1
        .gpio = 97,
        .name = "346_Pin1",
        .in_out_flag = 1,
        .init_value = 0,
    },
#endif
//#ifdef ASUS_A91_PROJECT
    {//346_CHARGING_DISABLE
        .gpio = 85,
        .name = "346_Disable",
        .in_out_flag = 1,
        .init_value = 0,
    },
//#endif
//Hank: not use+++
/*
    {//346_CHARGING_STATUS
        .gpio = 86,
        .name = "346_IRQ",
        .in_out_flag = 0,
        //.handler = smb346_irq_handler, // Eason takeoff smb346 IRQ handler   
        .trigger_flag= IRQF_TRIGGER_FALLING,
        .irq_enabled = false,
    },
*/
//Hank: not use---
};

//ASUS BSP Eason_Chang smb346 +++
static ssize_t smb346charger_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	smb346_proc_read();
	return sprintf(page, "%d\n", g_smb346_reg_value);
}
static ssize_t smb346charger_write_proc(struct file *filp, const char __user *buff, 
	            unsigned long len, void *data)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);


	g_smb346_reg_value = val;

        smb346_write_enable();
        smb346_proc_write();
    
    printk("[BAT][CHG][SMB][Proc]mode:%d\n",g_smb346_reg_value);
	
	return len;
}

void static create_smb346_proc_file(void)
{
	struct proc_dir_entry *smb346_proc_file = create_proc_entry("driver/smb346chg", 0644, NULL);

	if (smb346_proc_file) {
		smb346_proc_file->read_proc = smb346charger_read_proc;
		smb346_proc_file->write_proc = smb346charger_write_proc;
	}
    else {
		printk("[BAT][CHG][SMB][Proc]proc file create failed!\n");
    }

	return;
}

static ssize_t smb346ChgAddress_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	return sprintf(page, "%d\n", g_smb346_reg_address);
}
static ssize_t smb346ChgAddress_write_proc(struct file *filp, const char __user *buff, 
	            unsigned long len, void *data)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);


	g_smb346_reg_address = val;  
    
    printk("[BAT][CHG][SMB][Proc]ChgAddress:%d\n",val);
	
	return len;
}

void static create_smb346ChgAddress_proc_file(void)
{
	struct proc_dir_entry *smb346ChgAddress_proc_file = create_proc_entry("driver/smb346ChgAddr", 0644, NULL);

	if (smb346ChgAddress_proc_file) {
		smb346ChgAddress_proc_file->read_proc = smb346ChgAddress_read_proc;
		smb346ChgAddress_proc_file->write_proc = smb346ChgAddress_write_proc;
	}
    else {
		printk("[BAT][CHG][SMB][Proc]proc file create failed!\n");
    }

	return;
}
//ASUS BSP Eason_Chang smb346 ---

//Hank: Add chargerIC_status to check charger IC I2C state+++
static ssize_t chargerIC_status_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	int status;
       int reg_value=0;
       status=smb346_read_reg(0,&reg_value);
	return sprintf(page, "%d\n",status > 0 ? 1 : 0);
}
static ssize_t chargerIC_status_write_proc(struct file *filp, const char __user *buff, 
	            unsigned long len, void *data)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);


    
    	printk("[BAT][CHG][SMB][Proc]ChargerIC Proc File: %d\n",val);
	
	return len;
}

void static create_chargerIC_status_proc_file(void)
{
	struct proc_dir_entry *chargerIC_status_proc_file = create_proc_entry("driver/chargerIC_status", 0644, NULL);

	if (chargerIC_status_proc_file) {
		chargerIC_status_proc_file->read_proc = chargerIC_status_read_proc;
		chargerIC_status_proc_file->write_proc = chargerIC_status_write_proc;
	}
    else {
		printk("[BAT][CHG][SMB][Proc]proc file create failed!\n");
    }

	return;
}
//Hank: Add chargerIC_status to check charger IC I2C state---

//Eason: set Chg Limit In Pad When Charger Reset +++
#ifndef ASUS_FACTORY_BUILD
void setChgLimitInPadWhenChgReset_smb346(void);
static void setChgLimitInPadWhenResetWork(struct work_struct *dat)
{
		setChgLimitInPadWhenChgReset_smb346();
}
#endif
//Eason: set Chg Limit In Pad When Charger Reset ---

//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
extern void checkGaugeFCSetChg(void);
static void checkGaugeFCNeedDisableChgWork(struct work_struct *dat)
{
		checkGaugeFCSetChg();
}
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

//Function implementation for AXC_Smb346_Charger
static void AXC_Smb346_Charger_Init(AXI_Charger *apCharger)
{
    	AXC_SMB346Charger *this = container_of(apCharger,AXC_SMB346Charger,msParentCharger);
	printk("[BAT][CHG][SMB]AXC_Smb346_Charger_Init+++\n");
    	if(false == this->mbInited)
    	{
		printk( "[BAT][CHG][SMB]Init++\n");
		this->type = NO_CHARGER_TYPE;
		this->mpNotifier = NULL;
		//INIT_DELAYED_WORK(&this->msNotifierWorker, AXC_Smb346_Charger_NotifyClientForStablePlugIn) ;
		#ifdef ENABLE_WATCHING_STATUS_PIN_IN_IRQ
        	INIT_DELAYED_WORK(&this->asus_chg_work, status_handle_work);
		#endif

		//Eason: set Chg Limit In Pad When Charger Reset +++
		#ifndef ASUS_FACTORY_BUILD
	 	INIT_DELAYED_WORK(&this->setChgLimitInPadWhenChgResetWorker,setChgLimitInPadWhenResetWork);
		#endif
	 	//Eason: set Chg Limit In Pad When Charger Reset ---

		//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
		// can block in pad mode, then plug padAC
	 	INIT_DELAYED_WORK(&this->checkGaugeFCNeedDisableChgWorker,checkGaugeFCNeedDisableChgWork);
	 	//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

	 	this->mpGpio_pin = gGpio_pin;

        	wake_lock_init(&this->cable_in_out_wakelock, WAKE_LOCK_SUSPEND, "cable in out");
			
        	setup_timer(&this->charger_in_out_timer, charger_in_out_debounce_time_expired,(unsigned long)this);

		if (0 == AXC_Smb346_Charger_InitGPIO(this)) 
		{
			this->mbInited = true;
		} 
		else 
		{
			printk( "[BAT][CHG][SMB]Charger can't init\n");
        	}
        
        	charger_in_out_debounce_time_expired((unsigned long)this);
        
       	 //charger_in_out_handler(this->mpGpio_pin[Smb346_DC_IN].irq, this);

    		//ASUS BSP Eason_Chang smb346 +++
   		 //	create_charger_proc_file();
    		create_smb346_proc_file();
    		create_smb346ChgAddress_proc_file(); 

		//Hank: Add chargerIC_status to check charger IC I2C state+++
		create_chargerIC_status_proc_file();
		//Hank: Add chargerIC_status to check charger IC I2C state---
    		//INIT_DELAYED_WORK(&this->smb346_irq_worker,IrqHandleWork);//Eason takeoff smb346 IRQ handler
    		//ASUS BSP Eason_Chang smb346 ---
              gpCharger = this;
		printk( "[BAT][CHG][SMB]Init--\n");
    	}
	printk("[BAT][CHG][SMB]AXC_Smb346_Charger_Init---\n");
}

static void AXC_Smb346_Charger_DeInit(AXI_Charger *apCharger)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

    if(true == this->mbInited) {
	AXC_Smb346_Charger_DeInitGPIO(this);
        this->mbInited = false;
    }
}

int AXC_Smb346_Charger_GetType(AXI_Charger *apCharger)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

    return this->mnType;
}

void AXC_Smb346_Charger_SetType(AXI_Charger *apCharger ,int anType)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

    this->mnType = anType;
}

static AXE_Charger_Type AXC_Smb346_Charger_GetChargerStatus(AXI_Charger *apCharger)
{
	AXC_SMB346Charger *this = container_of(apCharger,
                                                AXC_SMB346Charger,
                                                msParentCharger);

	return this->type;
}


// joshtest
/*

static struct timespec g_charger_update_time = {0};

static void chg_set_charger_update_time(void)
{
	g_charger_update_time = current_kernel_time();
	printk( "[BAT][Chg] %s(), tv_sec:%ld\n",
		__func__,
		g_charger_update_time.tv_sec);
	return;
}


void chg_get_charger_update_time(struct timespec *charger_update_time)
{
	BUG_ON(NULL != charger_update_time);
	*charger_update_time = g_charger_update_time;
	printk( "[BAT][Chg] %s(), tv_sec:%ld\n",
		__func__,
		charger_update_time->tv_sec);
	return;
}
*/

// joshtest
static char *pm_power_supplied_to[] = {
	"battery",
};

static enum power_supply_property pm_power_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_ONLINE,
};
	
static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
        AXE_Charger_Type type;


       if(NULL == gpCharger){

            val->intval = 0;
            return 0;
       }

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
             val->intval =  0;

            type =gpCharger->msParentCharger.GetChargerStatus(&gpCharger->msParentCharger);

        
             if(NO_CHARGER_TYPE < type){

		   //Eason: in Pad AC powered, judge AC powered true+++
		   #if 0
                if(psy->type ==POWER_SUPPLY_TYPE_MAINS && 
                    (type == HIGH_CURRENT_CHARGER_TYPE || 
                    type == ILLEGAL_CHARGER_TYPE|| 
                    type == NORMAL_CURRENT_CHARGER_TYPE)){            

                    val->intval =  1;

                }
		   #endif
		   
               if(psy->type ==POWER_SUPPLY_TYPE_MAINS && 
                    (type == HIGH_CURRENT_CHARGER_TYPE || 
                    type == ILLEGAL_CHARGER_TYPE))
                {            

                    val->intval =  1;

                }else if((psy->type ==POWER_SUPPLY_TYPE_MAINS) && (type == NORMAL_CURRENT_CHARGER_TYPE))
                {
                	if(1==InP03JudgeACpowered())
				val->intval =  1;
                
		   }
	         //Eason: in Pad AC powered, judge AC powered true---	

		   

                 if(psy->type ==POWER_SUPPLY_TYPE_USB && type == LOW_CURRENT_CHARGER_TYPE){            
                 
                     val->intval =  1;
                 
                 }

             }

             if(true == g_AcUsbOnline_Change0)
             {
                     val->intval = 0;
                     printk("[BAT][CHG][SMB]: set online 0 to shutdown device\n");   
             }
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

//Hank : move batt_psy out of qpnp-charger+++ 

static enum power_supply_property pm_batt_power_props[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
};


static int  gChargeStatus=0;
static bool gChargeEnable = false;
static int gBatteryPresent = 0;
static int gTempLevel = 0;
extern int  gBatteryTemp;
extern int  gBatteryVol;
extern int  gBatteryCurrent;

//Hank: check battery missing+++
static bool AXC_Smb346_Charger_IsBattMissing(void)
{
	int status;
	 uint8_t i2cdata[32]={0};
	status=smb346_read_reg(SMB346_INTERRUPT_REG_B,i2cdata);

	if(status < 0 ){
            printk("[BAT][CHG][SMB][I2c]%s: I2c read error\n",__FUNCTION__);
	     gBatteryPresent = true;
	     return false;//i2c error return false common case;	
       }
	
	if(i2cdata[0] & 0x10)
	{
		printk("[BAT][CHG][SMB][Prop]%s(): Battery Missing!!\n",__FUNCTION__);
		gBatteryPresent = false;
		return true;
	}
	else
	{
		gBatteryPresent = true;
		return false;
	}
}
//Hank: check battery missing---

int get_charge_type(void)
{
	int status;
	uint8_t i2cdata[32]={0};
	
	if(AXC_Smb346_Charger_IsBattMissing())
	{
		gChargeStatus = POWER_SUPPLY_CHARGE_TYPE_NONE;
		gChargeEnable = false;
		printk("[BAT][CHG][SMB][Prop]%s(): Battery is missing!! \n",__FUNCTION__);   
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}
	status=smb346_read_reg(SMB346_STATUS_REG_C,i2cdata);

	if(status < 0 ){
            printk("[BAT][CHG][SMB][I2c]%s(): I2c read error\n",__FUNCTION__);
       }
	
	if(i2cdata[0] & 0x04)
	{
		gChargeStatus = POWER_SUPPLY_CHARGE_TYPE_FAST;
		gChargeEnable = true;
		pr_debug("[BAT][CHG][SMB][Prop]: Fast charging!! \n");   
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}
	else
	{
		if(i2cdata[0] & 0x02)
		{
			gChargeStatus = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			gChargeEnable = true;
			pr_debug("[BAT][CHG][SMB][Prop]: Trickle charging!! \n");   
			return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		}
		else
		{
			gChargeStatus = POWER_SUPPLY_CHARGE_TYPE_NONE;
			gChargeEnable = true;
			pr_debug("[BAT][CHG][SMB][Prop]: Non charging!! \n");   
			return POWER_SUPPLY_CHARGE_TYPE_NONE;
		}
	}
	pr_debug("[BAT][CHG][SMB][Prop]: Default Non charging!! \n");   
	return POWER_SUPPLY_CHARGE_TYPE_NONE;
	
}
#ifdef CONFIG_BATTERY_ASUS
static int get_current_now(void)
{
	return (get_Curr_from_TIgauge()*1000);
}
static int get_bat_status(void)
{
	return asus_bat_report_phone_status(0);
}
#endif


extern int asus_bat_report_phone_status(int);
extern int asus_bat_report_phone_capacity(int);

static int
batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	//struct qpnp_chg_chip *chip = container_of(psy, struct qpnp_chg_chip,batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = asus_bat_report_phone_status(0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = gChargeStatus;	
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		 //Hank: always return health good+++
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		 //Hank: always return health good---
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = gBatteryPresent;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 4350 * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 3400* 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = gBatteryVol;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = gBatteryTemp;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = asus_bat_report_phone_capacity(100);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = gBatteryCurrent;		
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 2500;			
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = gChargeEnable;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = gTempLevel;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void asus_bat_status_change(void);
extern void notifyThermalLimit(int thermalnotify);
static int
batt_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		AXC_Smb346_Charger_SetChargrDisbalePin(NULL,val->intval ? 0 : 1);
		DisChg = val->intval;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		gTempLevel =  val->intval;
		if(A90_EVB0==g_ASUS_hwID )
		{
			notifyThermalLimit(gTempLevel);
			ASUSEvtlog("[BAT]set SYSTEM_TEMP_LEVEL:%d \n",val->intval);
		}
		break;
	default:
		return -EINVAL;
	}

	asus_bat_status_change();
	return 0;
}
static int
batt_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply batt_psy = {
	.name		= "battery",
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	//.supplied_to = pm_power_supplied_to,
       //.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	.properties	= pm_batt_power_props,
	.num_properties	= ARRAY_SIZE(pm_batt_power_props),
	.get_property	= batt_power_get_property,
	.set_property	= batt_power_set_property,
	.property_is_writeable =batt_property_is_writeable,
};

//Eason_Chang:for A90 internal ChgGau+++
/*
void asus_bat_status_change(void)
{
	power_supply_changed(&batt_psy);
}
*/
//Eason_Chang:for A90 internal ChgGau---

//Hank : move batt_psy out of qpnp-charger---

static struct power_supply usb_psy = {
	.name		= "usb",
	.type		= POWER_SUPPLY_TYPE_USB,
	.supplied_to = pm_power_supplied_to,
       .num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	.properties	= pm_power_props,
	.num_properties	= ARRAY_SIZE(pm_power_props),
	.get_property	= pm_power_get_property,
};

static struct power_supply main_psy = {
	.name		= "ac",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = pm_power_supplied_to,
       .num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	.properties	= pm_power_props,
	.num_properties	= ARRAY_SIZE(pm_power_props),
	.get_property	= pm_power_get_property,
};

//Hank: set ME771KL EVB float voltage 4.2V+++
#ifdef ASUS_ME771KL_PROJECT
static void setME771KLFloatVoltage4p2(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 227;
	smb346_write_enable();
	smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);	
	printk("[BAT][CHG][SMB]Default Float Voltage 4.2V\n");	 
	 
}
#endif
//Hank: set ME771KL EVB float voltage 4.2V---

//Eason: set default Float Voltage  +++
#ifdef ASUS_A86_PROJECT
static void setSmb346FloatVoltageDefault(void)
{
     	uint8_t i2cdata[32]={0};	 
	i2cdata[0] = 234;	
	smb346_write_enable();
	smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);	
	printk("[BAT][CHG][SMB]Default Float Voltage\n");	 
}
#endif
//Eason: set default Float Voltage ---

//Eason: A68 charging in P03 limit 900mA, when recording limit 500mA ++++

static void enableAICL(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 151;

     smb346_write_enable();
     smb346_write_reg(SMB346_VARIOUS_FUNCTIONS,i2cdata);
}

static void disableAICL(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 135;

     smb346_write_enable();
     smb346_write_reg(SMB346_VARIOUS_FUNCTIONS,i2cdata);
}

//Hank: ME771KL USB Max Current 2000mA+++
/*
#ifdef ASUS_ME771KL_PROJECT 
static void limitSmb346chg2000(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 71;// 2000mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}

#endif
*/
//Hank: ME771KL USB Max Current 2000mA---
//#ifdef ASUS_A91_PROJECT 
static void limitSmb346chg1200(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 68;// 1200mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
//#endif


#ifdef ASUS_ME771KL_PROJECT 
static void limitSmb346chg500(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 17;// 500mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
#endif

//#ifdef ASUS_A91_PROJECT 
static void limitSmb346chg700(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 0x22;// 700mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
void limitSmb346chg900(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 51;// 900mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
#ifndef ASUS_FACTORY_BUILD
static void limitSmb346chg500(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 17;// 500mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
//Eason: Pad draw rule compare thermal  +++
static void limitSmb346chg300(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 0x00;// 300mA

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
//Eason: Pad draw rule compare thermal ---
#endif//#ifndef ASUS_FACTORY_BUILD
//#endif//#ifdef ASUS_A91_PROJECT 

#ifndef ASUS_FACTORY_BUILD   
static void defaultSmb346chgSetting(void)
{
     uint8_t i2cdata[32]={0};

     //Hank: charger 1895 porting+++
     i2cdata[0] = 0x51;// default:1500mA,500mA
     //Hank: charger 1895 porting---

     smb346_write_enable();
     smb346_write_reg(SMB346_INPUT_CUR_LIM,i2cdata);	
}
#endif

//Eason: A68 charging in P03 limit 900mA, when recording limit 500mA ---
//Eason:  Pad draw rule compare thermal +++
void setSmb346HigherHotTempLimit(void);
#ifndef ASUS_FACTORY_BUILD
static PadDrawLimitCurrent_Type JudgePadThermalDrawLimitCurrent(void)
{
	if( (3==g_thermal_limit)||(true==g_audio_limit) )
	{
		return PadDraw500;
   	}else if(true==g_padMic_On){

		return PadDraw500;
	}else if( 2==g_thermal_limit )
	{
		return PadDraw700;
	}else if(1 == g_thermal_limit)
	{
		return PadDraw900;
	}else
		return PadDraw900;
}
//Eason: set Chg Limit In Pad When Charger Reset +++ 

static PadDrawLimitCurrent_Type g_InPadChgNoResetDraw_limit = PadDraw700;
void setChgLimitInPadWhenChgReset_smb346(void)
{
	disableAICL();
	
	switch(g_InPadChgNoResetDraw_limit)
	{
		case PadDraw300:
				limitSmb346chg300();
				printk("[BAT][CHG][PAD][Reset]:draw 300\n");
				break;
		case PadDraw500:
				limitSmb346chg500();
				printk("[BAT][CHG][PAD][Reset]:draw 500\n");
				break;
		case PadDraw700:
				limitSmb346chg700();
				printk("[BAT][CHG][PAD][Reset]:draw 700\n");
				break;
		case PadDraw900:
				limitSmb346chg900();
				printk("[BAT][CHG][PAD][Reset]:draw 900\n");
				break;				
		default:
				limitSmb346chg700();
				printk("[BAT][CHG][PAD][Reset]:draw 700\n");
				break;
	}
	if(DffsDown == true)
		setSmb346HigherHotTempLimit();
}

//Eason: set Chg Limit In Pad When Charger Reset --- 

void setChgLimitThermalRuleDrawCurrent_smb346(bool isSuspendCharge)
{
	PadDrawLimitCurrent_Type PadThermalDraw_limit;
	PadDrawLimitCurrent_Type PadRuleDraw_limit;
	PadDrawLimitCurrent_Type MinThermalRule_limit;

	PadThermalDraw_limit=JudgePadThermalDrawLimitCurrent();
	PadRuleDraw_limit=JudgePadRuleDrawLimitCurrent(isSuspendCharge);
	MinThermalRule_limit = min(PadThermalDraw_limit,PadRuleDraw_limit);
	printk("[BAT][CHG][PAD][Cur]:Thermal:%d,Rule:%d,Min:%d\n",PadThermalDraw_limit,PadRuleDraw_limit,MinThermalRule_limit);

	disableAICL();
	
	//ForcePowerBankMode draw 900, but  still compare thermal result unless PhoneCap<=8
	if( (PadDraw900==PadRuleDraw_limit)&&(smb346_getCapacity()<= 8))
	{
		limitSmb346chg900();
		g_InPadChgNoResetDraw_limit = PadDraw900;
		printk("[BAT][CHG][PAD][Cur]:draw 900\n");
	}else{
		switch(MinThermalRule_limit)
		{
			case PadDraw300:
				limitSmb346chg300();
				g_InPadChgNoResetDraw_limit = PadDraw300;
				printk("[BAT][CHG][PAD][Cur]:draw 300\n");
				break;
			case PadDraw500:
				limitSmb346chg500();
				g_InPadChgNoResetDraw_limit = PadDraw500;
				printk("[BAT][CHG][PAD][Cur]:draw 500\n");
				break;
			case PadDraw700:
				limitSmb346chg700();
				g_InPadChgNoResetDraw_limit = PadDraw700;
				printk("[BAT][CHG][PAD][Cur]:draw 700\n");
				break;
			case PadDraw900:
				limitSmb346chg900();
				if(!isSuspendCharge)//Hank: if suspend set 900mA do not remeber the current
					g_InPadChgNoResetDraw_limit = PadDraw900;
				printk("[BAT][CHG][P03]:draw 900\n");
				break;
			default:
				limitSmb346chg700();
				g_InPadChgNoResetDraw_limit = PadDraw700;
				printk("[BAT][CHG][PAD][Cur]:draw 700\n");
				break;
		}
	}
	
	//enableAICL();

}
#endif//#ifndef ASUS_FACTORY_BUILD
//Eason:  Pad draw rule compare thermal ---
//Eason : when thermal too hot, limit charging current +++ 
void setChgDrawCurrent_smb346(void)
{
#ifndef ASUS_FACTORY_BUILD   
  if(true == g_chgTypeBeenSet)//Eason : prevent setChgDrawCurrent_smb346 before get chgType
  {
	if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type)
	{
		setChgLimitThermalRuleDrawCurrent_smb346(false);
	}else if(NOTDEFINE_TYPE==gpCharger->type
			||NO_CHARGER_TYPE==gpCharger->type)
	{
		disableAICL();
		defaultSmb346chgSetting();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:default 700mA\n");
	}else if( (3==g_thermal_limit)||(true==g_audio_limit) )
	{
		disableAICL();
		limitSmb346chg500();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 500mA\n");
	}else if(true==g_padMic_On){
		disableAICL();
		limitSmb346chg500();
		enableAICL();	
		printk("[BAT][CHG][SMB][Cur]:InPad onCall limit cur500mA\n");

	}else if( 2==g_thermal_limit )
	{
		disableAICL();
		limitSmb346chg700();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 700mA\n");

	}else if(1 == g_thermal_limit){
		disableAICL();
		limitSmb346chg900();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 900mA\n");
	}else{

		if(LOW_CURRENT_CHARGER_TYPE==gpCharger->type
				||ILLEGAL_CHARGER_TYPE==gpCharger->type)
		{   
			disableAICL();
			limitSmb346chg900();
			enableAICL();
			printk("[BAT][CHG][SMB][Cur]LowIllegal: limit chgCur,  darw 900\n");

		}else if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type)
		{
			//Eason: AICL work around +++
			if(true==g_AICLlimit)
			{
				disableAICL();
				limitSmb346chg900();
				enableAICL();
				printk("[BAT][CHG][SMB][AICL]: g_AICLlimit = true\n");
			}
			//Eason: AICL work around ---
			else{
				disableAICL();
				limitSmb346chg1200();
				enableAICL();
				printk("[BAT][CHG][SMB][Cur]AC: dont limit chgCur, use default setting\n");
			}
		}
	}
  }
#else
   printk("[BAT][CHG][SMB][Cur]Factory build use default 500mA\n");
#endif//#ifndef ASUS_FACTORY_BUILD    
}
//Eason : when thermal too hot, limit charging current ---

//Eason: AICL work around +++
void setChgDrawACTypeCurrent_withCheckAICL_smb346(void)
{
#ifndef ASUS_FACTORY_BUILD   

	if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type)
	{
		setChgLimitThermalRuleDrawCurrent_smb346(false);
	}else if( (3==g_thermal_limit)||(true==g_audio_limit) )
	{
		disableAICL();
		limitSmb346chg500();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 500mA\n");
	}else if(true==g_padMic_On){
		disableAICL();
		limitSmb346chg500();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:InPad onCall limit cur500mA\n");

	}else if( 2 == g_thermal_limit){
		disableAICL();
		limitSmb346chg700();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 700mA\n");

	}else if( 1 == g_thermal_limit){
		disableAICL();
		limitSmb346chg900();
		enableAICL();
		printk("[BAT][CHG][SMB][Cur]:limit charging current 900mA\n");

	}else{
		if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type)
		{
			//Hank: remove 1A adapter protection+++
			#if 0
			//Hank: Prevent 1A adapter cause current on/off issue+++
			if( (time_after(AICL_success_jiffies+ADAPTER_PROTECT_DELAY , jiffies))
				&& (HIGH_CURRENT_CHARGER_TYPE==lastTimeCableType) 
				&& (true == g_AICLSuccess) )
			//Hank: Prevent 1A adapter cause current on/off issue---
			{
				disableAICL();
				limitSmb346chg900();
				enableAICL();
				g_AICLlimit = true;
				g_AICLSuccess = false;
				printk("[BAT][CHG][SMB][AICL]:AICL fail, always limit 900mA charge \n");
				
			}else if( (smb346_getCapacity()>5) &&(true== g_alreadyCalFirstCap))
			{

				disableAICL();
				limitSmb346chg1200();
				enableAICL();
				//Hank: ME771KL USB Max Current 2000mA---
				g_AICLlimit =false;
				g_AICLSuccess = true;
				//Eason: AICL work around +++
				AICL_success_jiffies = jiffies;
				//Eason: AICL work around ---
				printk("[BAT][CHG][SMB][AICL]:Set current 1200mA and check AICL in 7sec \n");

				//Hank: remove AICL worker+++
				/*
				disableAICL();
				limitSmb346chg900();
				enableAICL();
				g_AICLlimit = true;
				schedule_delayed_work(&AICLWorker, 60*HZ);
				printk("[BAT][CHG][SMB][AICL]: check AICL\n");
				*/
				//Hank: remove AICL worker---
			}else{
				disableAICL();
				limitSmb346chg1200();
				enableAICL();
				printk("[BAT][CHG][SMB][AICL]: dont limit chgCur, use default setting\n");
			}
			#endif
			disableAICL();
			limitSmb346chg1200();
			enableAICL();
			//Hank: remove 1A adapter protection---

		}
	}
#endif//#ifndef ASUS_FACTORY_BUILD    
}
//Eason: AICL work around ---

//Eason: A68 charging in P03 limit 900mA, when recording limit 500mA ++++
void setChgDrawPadCurrent(bool audioOn)
{
   g_padMic_On = audioOn;
   printk("[BAT][CHG][Pad]g_padMic_On:%d\n",g_padMic_On);
   
#ifndef ASUS_FACTORY_BUILD
    setChgDrawCurrent_smb346();
/*
   if( (false == audioOn)&&(false == g_thermal_limit)&&(false == g_audio_limit) )
   {
     disableAICL();
     limitSmb346chg900(); 
	enableAICL();	 
     printk("[BAT][CHG]:audio off : draw P03 900mA\n");
   }else{
     disableAICL();
     limitSmb346chg500();
	 enableAICL();
     printk("[BAT][CHG]:audio on : draw P03 500mA\n");
   }
   */
#endif//#ifndef ASUS_FACTORY_BUILD
}
//Eason: A68 charging in P03 limit 900mA, when recording limit 500mA ---

//Hank: remove AICL worker+++
/*
//Eason: AICL work around +++
void checkIfAICLSuccess(void)
{
#ifndef ASUS_FACTORY_BUILD  
	if ( 0==!gpio_get_value(gpCharger->mpGpio_pin[Smb346_DC_IN].gpio)){
		disableAICL();
		limitSmb346chg900();
		enableAICL();
		g_AICLlimit = true;
		g_AICLSuccess = false;
		printk("[BAT][CHG][SMB][AICL]:AICL fail, limit 900mA charge \n");
	}else{
		disableAICL();
		limitSmb346chg1200();
		enableAICL();
		//Hank: ME771KL USB Max Current 2000mA---
		g_AICLlimit =false;
		g_AICLSuccess = true;
		//Eason: AICL work around +++
		AICL_success_jiffies = jiffies;
		//Eason: AICL work around ---
		printk("[BAT][CHG][SMB][AICL]:AICL success, use default charge \n");
	}
#endif//#ifndef ASUS_FACTORY_BUILD	
}

static void checkAICL(struct work_struct *dat)
{
        checkIfAICLSuccess();
}
//Eason: AICL work around ---
*/
//Hank: remove AICL worker---

//#ifdef ASUS_A91_PROJECT
extern void nv_touch_mode(int); //ASUS_BSP HANS: temporary way to notify usb state ++
//#endif
static void AXC_Smb346_Charger_SetCharger(AXI_Charger *apCharger , AXE_Charger_Type aeChargerType)
{
	//static bool first_call = true;
    	AXC_SMB346Charger *this = container_of(apCharger,AXC_SMB346Charger,msParentCharger);

//#ifdef ASUS_A91_PROJECT
	//ASUS_BSP HANS: temporary way to notify usb state +++
	        nv_touch_mode(aeChargerType);//1032 porting remove
	//ASUS_BSP HANS: temporary way to notify usb state ---
//#endif
    
	if(false == this->mbInited)
	{
		printk("[BAT][CHG][SMB]%s():AXC_SMB346Charger not init return \n",__func__);
		return;
	}
	/*
	if (!first_call && !this->m_is_bat_valid) {
		printk(KERN_INFO "[BAT][Chg] %s(), battery is invalid and cannot charging\n", __func__);
		aeChargerType = NO_CHARGER_TYPE;
	}
	*/
	printk( "[BAT][CHG][SMB]%s():CharegeModeSet:%d\n",__func__,aeChargerType);

	//ASUS BSP Eason_Chang prevent P02 be set as illegal charger +++ 
      if(NORMAL_CURRENT_CHARGER_TYPE==this->type && ILLEGAL_CHARGER_TYPE==aeChargerType)
      {
        	printk("[BAT][CHG][SMB]prevent P02 be set as illegal charger\n");
        	return;
      }
      //ASUS BSP Eason_Chang prevent P02 be set as illegal charger ---

	//A68 set smb346 default charging setting+++

	//Eason: when AC dont set default current. When phone Cap low can always draw 1200mA from boot to kernel+++
	#if 0
       if(HIGH_CURRENT_CHARGER_TYPE!=aeChargerType)
       {
	       disableAICL();
	       defaultSmb346chgSetting();
	 	enableAICL();
		printk("[BAT][CHG][SMB]default setting\n");
       }
	#endif   
	//Eason: when AC dont set default current. When phone Cap low can always draw 1200mA from boot to kernel---

	#ifndef ASUS_FACTORY_BUILD		
	g_AICLlimit = false;
	g_chgTypeBeenSet = true;//Eason : prevent setChgDrawCurrent_smb346 before get chgType
	#endif//#ifndef ASUS_FACTORY_BUILD
	//A68 set smb346 default charging setting---            

	switch(aeChargerType)
    	{
		case NO_CHARGER_TYPE:
			#ifdef CONFIG_ENABLE_PIN1 
			AXC_Smb346_Charger_SetPIN1(this,0);
			#endif

			#ifdef CONFIG_ENABLE_PIN2
			AXC_Smb346_Charger_SetPIN2(this,0);
			#endif
			//AXC_Smb346_Charger_SetChargrDisbalePin(this,1); //Hank: keep charger always enable

			this->type = aeChargerType;

			#ifdef ENABLE_WATCHING_STATUS_PIN_IN_IRQ          
            		if(true == this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled)
			{
                		disable_irq_wake(this->mpGpio_pin[Smb346_CHARGING_STATUS].irq);
                		this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled = false;
            		}    
			#endif

			//cancel_delayed_work(&this->msNotifierWorker);
			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);//callback to axc_BatteryServuce.c NotifyForChargerStateChanged()
			break;

        	case ILLEGAL_CHARGER_TYPE:
		case LOW_CURRENT_CHARGER_TYPE:			
			this->type = aeChargerType;
			//Eason: AICL work around +++
			lastTimeCableType = aeChargerType;
			//Eason: AICL work around ---
			//#ifdef ASUS_A91_PROJECT
				setChgDrawCurrent_smb346();
				//Hank: Factory build set usb charging current 700mA+++
				#ifdef ASUS_FACTORY_BUILD
					disableAICL();
					limitSmb346chg700();
					enableAICL();
				#endif //#ifdef ASUS_FACTORY_BUILD
				//Hank: Factory build set usb charging current 700mA---
			//#endif //#ifdef ASUS_A91_PROJECT

			//Hank: ME771KL usb 500mA+++
			#ifdef ASUS_ME771KL_PROJECT
				disableAICL();
				limitSmb346chg500();
				enableAICL();
			#endif
			//Hank: ME771KL usb 500mA---

			//Hank: set higher hot temperature limit when CPU on+++
			if(DffsDown == true)
			{
				setSmb346HigherHotTempLimit();
			}
			//Hank: set higher hot temperature limit when CPU on---

			//Hank: ME771KL usb 500mA---			

			AXC_Smb346_Charger_SetChargrDisbalePin(this,0);
			DisChg = false;
			#ifdef ENABLE_WATCHING_STATUS_PIN_IN_IRQ
		              if(false== this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled)
				{
		                	enable_irq_wake(this->mpGpio_pin[Smb346_CHARGING_STATUS].irq);
		                	this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled = true;
		              }    
			#endif
			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);//callback to axc_BatteryServuce.c NotifyForChargerStateChanged()
			//cancel_delayed_work(&this->msNotifierWorker);
			//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
			break;
			
              case NORMAL_CURRENT_CHARGER_TYPE:
		case HIGH_CURRENT_CHARGER_TYPE:
           		//Eason : when thermal too hot, limit charging current +++ 
            		this->type = aeChargerType;
			//Eason: AICL work around +++
			lastTimeCableType = aeChargerType;

			//Hank: Factory build set AC current directly+++
			#ifdef ASUS_FACTORY_BUILD  	
			if(HIGH_CURRENT_CHARGER_TYPE==aeChargerType)
			{
				disableAICL();
				limitSmb346chg1200();
				enableAICL();
			}
			#endif//ASUS_FACTORY_BUILD  		
			//Hank: Factory build set AC current directly+++
			
			setChgDrawACTypeCurrent_withCheckAICL_smb346();
			//Eason: AICL work around ---
           		//Eason : when thermal too hot, limit charging current --- 
			//Hank: set higher hot temperature limit when CPU on+++
			if(DffsDown == true)
			{
				setSmb346HigherHotTempLimit();
			}
			//Hank: set higher hot temperature limit when CPU on---
			AXC_Smb346_Charger_SetChargrDisbalePin(this,0);
			DisChg = false;
			#ifdef ENABLE_WATCHING_STATUS_PIN_IN_IRQ   
            		if(false== this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled)
			{
                		enable_irq_wake(this->mpGpio_pin[Smb346_CHARGING_STATUS].irq);
                		this->mpGpio_pin[Smb346_CHARGING_STATUS].irq_enabled = true;
            		}    
			#endif

			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);//callback to axc_BatteryServuce.c NotifyForChargerStateChanged()
			//cancel_delayed_work(&this->msNotifierWorker);
			//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
			break;
			
		default:
			printk( "[BAT][CHG][SMB]Wrong ChargerMode:%d\n",aeChargerType);
			break;
	}

	/*
	if (first_call) 
	{
		first_call = false;
	}
	*/

	if(true==g_smb346_Psy_ready )
	{
    		power_supply_changed(&usb_psy);
	    	power_supply_changed(&main_psy);
	}		

}

void AcUsbPowerSupplyChange_smb346(void)
{
	if(true==g_smb346_Psy_ready )
	{
		power_supply_changed(&usb_psy);
		power_supply_changed(&main_psy);
		printk("[BAT][CHG][SMB]:Ac Usb PowerSupply Change\n");
	}	
}

static void AXC_Smb346_Charger_SetBatConfig(AXI_Charger *apCharger , bool is_bat_valid)
{
//	AXC_SMB346Charger *this = container_of(apCharger,
//                                                AXC_SMB346Charger,
//                                                msParentCharger);
	if (is_bat_valid) {
		printk(KERN_INFO "[BAT][CHG][SMB]%s, bat is valid\n", __func__);
	}

	//this->m_is_bat_valid = is_bat_valid;

	return;
}

static bool AXC_Smb346_Charger_IsCharegrPlugin(AXI_Charger *apCharger)
{
	AXC_SMB346Charger *this = container_of(apCharger,
                                                AXC_SMB346Charger,
                                                msParentCharger);

#ifdef STAND_ALONE_WITHOUT_USB_DRIVER
	//Should be configured by usb driver...
	return (!AXC_Smb346_Charger_GetGPIO(this->mnChargePlugInPin));
#else
	return (this->type != NO_CHARGER_TYPE);
#endif
}

//Eason show AICL setting+++
//#define InputCurrentLimit__smb346_CMD_Table  1  //Input Current Limit : 0x01h
int showSmb346AICL_Setting(void)
{
	int reg01_value;
	int AICL_Setting_500;	
	int AICL_Setting_700;
	int AICL_Setting_900;
	int AICL_Setting_1200;
	int AICLsetting = 3;

	reg01_value = smb346_read_table(SMB346_INPUT_CUR_LIM);
	printk("[BAT][CHG][SMB][AICL]AICL_Setting:0x%02X\n",reg01_value);

	AICL_Setting_500 =  reg01_value & 0x11;
	AICL_Setting_700 =  reg01_value & 0x22;
	AICL_Setting_900 =  reg01_value & 0x33;
	AICL_Setting_1200 =  reg01_value & 0x44;

	if(0x44 == AICL_Setting_1200)
	{
		AICLsetting = 12;
	}else if(0x33 == AICL_Setting_900)
	{
		AICLsetting = 9;
	}else if(0x22 == AICL_Setting_700)
	{
		AICLsetting = 7;
	}else if(0x11 == AICL_Setting_500)
	{
		AICLsetting = 5;
	}

	return AICLsetting;
}
//Eason show AICL setting---
//Eason show AICL result+++
//#define StatusRegisterE__smb346_CMD_Table  28  //Input Current Limit : 0x3Fh
int showSmb346AICL_Result(void)
{
	int reg3F_value;
	int AICL_Result_500;	
	int AICL_Result_700;
	int AICL_Result_900;
	int AICL_Result_1200;
	int AICLresult = 3;

	reg3F_value = smb346_read_table(SMB346_STATUS_REG_E);
	printk("[BAT][CHG][SMB][AICL]AICL_Result:0x%02X\n",reg3F_value);

	AICL_Result_500 =  reg3F_value & 0x01;
	AICL_Result_700 =  reg3F_value & 0x02;
	AICL_Result_900 =  reg3F_value & 0x03;
	AICL_Result_1200 =  reg3F_value & 0x04;

	if(0x04 == AICL_Result_1200)
	{
		AICLresult = 12;
	}else if(0x03 == AICL_Result_900)
	{
		AICLresult = 9;	
	}else if(0x02 == AICL_Result_700)
	{
		AICLresult = 7;
	}else if(0x01 == AICL_Result_500)
	{
		AICLresult = 5;
	}

	return AICLresult;
}
//Eason show AICL result---

//Eason show temp limit +++
//#define InterruptRegisterA__smb346_CMD_Table  18  //Interrupt Register A : 0x35h
int showSmb346TempLimitReason(void)
{
	int reg35_value;
	int HotTempHardLimitStatus;//S35[6] 
	int ColdTempHardLimitStatus;//S35[4] 
	int HotTempSoftLimitStatus;//S35[2] 
	int ColdTempSoftLimitStatus;//S35[0] 
	int limitStatus = 0;

	reg35_value = smb346_read_table(SMB346_INTERRUPT_REG_A);
	printk("[BAT][CHG][SMB]TempLimitReason:0x%02X\n",reg35_value);
	
	HotTempHardLimitStatus = reg35_value & 64;
	ColdTempHardLimitStatus = reg35_value & 16;
	HotTempSoftLimitStatus = reg35_value &  4;
	ColdTempSoftLimitStatus = reg35_value & 1;

	if( 16 == ColdTempHardLimitStatus)
	{
		limitStatus = 1;
	}else if(1 == ColdTempSoftLimitStatus)
	{
		limitStatus = 2;
	}else if( 64 == HotTempHardLimitStatus)	
	{
		limitStatus = 3;
	}else if( 4 == HotTempSoftLimitStatus)
	{
		limitStatus = 4;
	}

	return limitStatus;
}
//Eason show temp limit ---

//Eason judge smb346 full +++
bool smb346_IsFull(void)
{
    int reg3D_value;
    int reg37_value;	
    int ChgCycleTerminated;
    //At least one charging cycle has terminated since Charging first enabled
    int TermChgCycleHitStatus;//Termination Charging Current Hit status


    reg3D_value = smb346_read_table(SMB346_STATUS_REG_C);//S3D[5] 
    ChgCycleTerminated = reg3D_value & 32;
    
    reg37_value = smb346_read_table(SMB346_INTERRUPT_REG_C);//S37[0]
    TermChgCycleHitStatus = reg37_value & 1;


    if(32==ChgCycleTerminated && 1==TermChgCycleHitStatus)
    {	
        pr_debug("[BAT][CHG][SMB]:IsFull:true\n");
    	return true;
    }
    else
    {
        pr_debug("[BAT][CHG][SMB]:IsFull:false\n");
    	return false;
    }
}
//Eason judge smb346 full ---

//Hank: add charger state check +++
//Hank: TempLimit Status+++
int ChargerTempLimit = 0;
//Hank: TempLimit Status---
#ifdef CONFIG_BAT_DEBUG
void smb346_state_check(void)
{

	if(ChargerTempLimit)
	{
		printk("[BAT][CHG][SMB]SMB346_CHG_CUR\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_CHG_CUR));
		printk("[BAT][CHG][SMB]SMB346_INPUT_CUR_LIM\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INPUT_CUR_LIM));
		printk("[BAT][CHG][SMB]SMB346_FLOAT_VOLTAGE\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_FLOAT_VOLTAGE));
		printk("[BAT][CHG][SMB]SMB346_PIN_ENABLE_CONTROL\t\t= 0x%2X \r\n", smb346_read_table(SMB346_PIN_ENABLE_CONTROL));
		printk("[BAT][CHG][SMB]SMB346_HW_SW_LIMIT_CELL_TEMP\t\t= 0x%2X \r\n", smb346_read_table(SMB346_HW_SW_LIMIT_CELL_TEMP));
		printk("[BAT][CHG][SMB]SMB346_COMMAND_REG_A\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_COMMAND_REG_A));
		printk("[BAT][CHG][SMB]SMB346_COMMAND_REG_B\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_COMMAND_REG_B));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_A\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_A));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_B\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_B));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_C\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_C));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_D\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_D));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_E\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_E));
		printk("[BAT][CHG][SMB]SMB346_INTERRUPT_REG_F\t\t= 0x%2X \r\n", smb346_read_table(SMB346_INTERRUPT_REG_F));
		printk("[BAT][CHG][SMB]SMB346_STATUS_REG_A\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_STATUS_REG_A));
		printk("[BAT][CHG][SMB]SMB346_STATUS_REG_B\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_STATUS_REG_B));
		printk("[BAT][CHG][SMB]SMB346_STATUS_REG_C\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_STATUS_REG_C));
		printk("[BAT][CHG][SMB]SMB346_STATUS_REG_D\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_STATUS_REG_D));
		printk("[BAT][CHG][SMB]SMB346_STATUS_REG_E\t\t\t= 0x%2X \r\n", smb346_read_table(SMB346_STATUS_REG_E));
	}
		

}
#endif
//Hank: add charger state check ---
static bool AXC_Smb346_Charger_IsCharging(AXI_Charger *apCharger)
{
    int reg3D_value;
    int PreChargingStatus;
    int FastChargingStatus;
    int TaperChargingStatus;

    reg3D_value = smb346_read_table(SMB346_STATUS_REG_C);//S3D[2:1] 
    PreChargingStatus = reg3D_value & 2;
    FastChargingStatus = reg3D_value & 4;
    TaperChargingStatus = reg3D_value & 6;

    if(2==PreChargingStatus)
    {
    	pr_debug("[BAT][CHG][SMB]:Pre charging\n");
    	return true;
    }else if(4==FastChargingStatus){
    	pr_debug("[BAT][CHG][SMB]:Fast charging\n");
    	return true;
    }else if(6==TaperChargingStatus){
    	pr_debug("[BAT][CHG][SMB]:Taper charging\n");
    	return true;
    }else{
    	return false;
    }
}
/*
static bool AXC_Smb346_Charger_Test(void *apTestObject)
{
	return true;
}
*/

#ifdef CONFIG_CHARGER_MODE //ASUS_BSP Eason_Chang 1120 porting +++
static void ChgModeIfCableInSetAsUsbDefault(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                ,AXE_Charger_Type chargerType)
{
        AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);
        int online;

        if( 1==g_CHG_mode ){

            online = !gpio_get_value(this->mpGpio_pin[Smb346_DC_IN].gpio);
            printk("[BAT][CHG][SMB]If cableIn:%d,%d\n",chargerType,online);
 
            if( (1==online) && (NOTDEFINE_TYPE== chargerType) )
            {
                this->type = NORMAL_CURRENT_CHARGER_TYPE;
                printk("[BAT][CHG][SMB]If cableIn set Pad default\n");
            
                if(NULL != notify_charger_in_out_func_ptr){
    
                     (*notify_charger_in_out_func_ptr) (online);
                     printk("[BAT][CHG][SMB]If cableIn notify online\n");
    
                }else{
    
                     printk("[BAT][CHG][SMB]Nobody registed..\n");
                }
                
            }
        }
 
}
#endif//CONFIG_CHARGER_MODE//ASUS_BSP Eason_Chang 1120 porting ---

static void AXC_SMB346Charger_RegisterChargerStateChanged(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                            ,AXE_Charger_Type chargerType)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);
#ifdef CONFIG_CHARGER_MODE //ASUS_BSP Eason_Chang 1120 porting +++    
    if( 1==g_CHG_mode ){
            ChgModeIfCableInSetAsUsbDefault(apCharger, apNotifier, chargerType);
    }
#endif//CONFIG_CHARGER_MODE//ASUS_BSP Eason_Chang 1120 porting ---    

    this->mpNotifier = apNotifier;
}

static void AXC_SMB346Charger_DeregisterChargerStateChanged(struct AXI_Charger *apCharger,AXI_ChargerStateChangeNotifier *apNotifier)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

	if(this->mpNotifier == apNotifier)
		this->mpNotifier = NULL;
}

void AXC_SMB346Charger_Binding(AXI_Charger *apCharger,int anType)
{
    AXC_SMB346Charger *this = container_of(apCharger,
                                            AXC_SMB346Charger,
                                            msParentCharger);

    this->msParentCharger.Init = AXC_Smb346_Charger_Init;
    this->msParentCharger.DeInit = AXC_Smb346_Charger_DeInit;
    this->msParentCharger.GetType = AXC_Smb346_Charger_GetType;
    this->msParentCharger.SetType = AXC_Smb346_Charger_SetType;
    this->msParentCharger.GetChargerStatus = AXC_Smb346_Charger_GetChargerStatus;
    this->msParentCharger.SetCharger = AXC_Smb346_Charger_SetCharger;
    this->msParentCharger.EnableCharging = EnableCharging;
    this->msParentCharger.SetBatConfig = AXC_Smb346_Charger_SetBatConfig;
    this->msParentCharger.IsCharegrPlugin = AXC_Smb346_Charger_IsCharegrPlugin;
    this->msParentCharger.IsCharging = AXC_Smb346_Charger_IsCharging;
    this->msParentCharger.RegisterChargerStateChanged= AXC_SMB346Charger_RegisterChargerStateChanged;	
    this->msParentCharger.DeregisterChargerStateChanged= AXC_SMB346Charger_DeregisterChargerStateChanged;	
    this->msParentCharger.SetType(apCharger,anType);

    //this->mbInited = false;
#ifdef CHARGER_SELF_TEST_ENABLE
    this->msParentSelfTest.Test = AXC_Smb346_Charger_Test;
#endif
}

/*
Implement Interface for USB Driver
*/
#include "axc_chargerfactory.h"
/*
static unsigned GetChargerStatus(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	return (unsigned) lpCharger->GetChargerStatus(lpCharger);
}

static void SetCharegerUSBMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk( "[BAT][Chg]SetCharegerUSBMode\n");
	lpCharger->SetCharger(lpCharger,LOW_CURRENT_CHARGER_TYPE);
}

static void SetCharegerACMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk( "[BAT][Chg]SetCharegerACMode\n");
	lpCharger->SetCharger(lpCharger,HIGH_CURRENT_CHARGER_TYPE);
}

static void SetCharegerNoPluginMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

    printk( "[BAT][Chg]SetCharegerNoPluginMode\n");
	lpCharger->SetCharger(lpCharger,NO_CHARGER_TYPE);
}
*/

//ASUS BSP Eason_Chang smb346 +++
/*
static ssize_t charger_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	return sprintf(page, "%d\n", lpCharger->GetChargerStatus(lpCharger));
}

static ssize_t charger_write_proc(struct file *filp, const char __user *buff, 
	unsigned long len, void *data)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);

    {
	    static AXI_Charger *lpCharger = NULL;

	    if(NULL == lpCharger)
        {
		    AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE ,&lpCharger);
		    lpCharger->Init(lpCharger);
	    }

	    lpCharger->SetCharger(lpCharger,val);
    }
	//UpdateBatteryLifePercentage(gpGauge, val);
	
	return len;
}

static void create_charger_proc_file(void)
{
	struct proc_dir_entry *charger_proc_file = create_proc_entry("driver/asus_chg", 0644, NULL);

	if (charger_proc_file) {
		charger_proc_file->read_proc = charger_read_proc;
		charger_proc_file->write_proc = charger_write_proc;
	}
    else {
		printk( "[BAT][CHG]proc file create failed!\n");
    }

	return;
}
*/
//ASUS BSP Eason_Chang smb346 ---

int registerChargerInOutNotificaition_smb346(void (*callback)(int))
{
    printk("[BAT][CHG][SMB]%s\n",__FUNCTION__);
    
//ASUS_BSP Eason_Chang 1120 porting +++
#if 0//ASUS_BSP Eason_Chang 1120 porting
    if(g_A60K_hwID >=A66_HW_ID_ER2){

        notify_charger_in_out_func_ptr = callback;   
        
    }else{
    
       pm8921_charger_register_vbus_sn(callback); 
       
    }
#endif//ASUS_BSP Eason_Chang 1120 porting
       notify_charger_in_out_func_ptr = callback;  
//ASUS_BSP Eason_Chang 1120 porting ---    

    return 0;
}
//Eason : support OTG mode+++
int registerChargerI2CReadyNotificaition(void (*callback)(void))
{
	printk("[BAT][CHG][SMB]%s\n",__FUNCTION__);
	notify_charger_i2c_ready_func_ptr = callback;
	return 0;
}
//Eason : support OTG mode---
#ifdef CONFIG_EEPROM_NUVOTON
static int smb346_microp_event_handler(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
    if(gpCharger == NULL){

        return NOTIFY_DONE;
    }

	switch (event) {
	case P01_ADD:
            //gpCharger->msParentCharger.SetCharger(&gpCharger->msParentCharger,NORMAL_CURRENT_CHARGER_TYPE);//Hank: remove unnecessary setcharger        
		//ASUS_BSP +++ Peter_lu "For fastboot mode plug in Pad issue"
#ifdef CONFIG_FASTBOOT
		if(is_fastboot_enable())
		{
			printk("[FastBoot]Detect PAD add\n");
			ready_to_wake_up_and_send_power_key_press_event_in_fastboot(true);
		}
#endif //#ifdef CONFIG_FASTBOOT
		break;	
	case P01_REMOVE: // means P01 removed
        //gpCharger->msParentCharger.SetCharger(&gpCharger->msParentCharger, NO_CHARGER_TYPE);////Hank: remove unnecessary setcharger 
		break;
	case P01_BATTERY_POWER_BAD: // P01 battery low
		break;
	case P01_AC_USB_IN:
		//ASUS_BSP +++ Peter_lu "For fastboot mode Pad plug in cable issue"
#ifdef CONFIG_FASTBOOT
		if(is_fastboot_enable())
		{
			printk("[FastBoot]Detect PAD cable in\n");
			ready_to_wake_up_and_send_power_key_press_event_in_fastboot(true);
		}
#endif //#ifdef CONFIG_FASTBOOT
	    //ASUS_BSP ---
		break;
	case P01_AC_USB_OUT:
		break;
	default:
             break;
	}

	return NOTIFY_DONE;
}
#endif /* CONFIG_EEPROM_NUVOTON */


#ifdef CONFIG_EEPROM_NUVOTON
static struct notifier_block smb346_microp_notifier = {
        .notifier_call = smb346_microp_event_handler,
};
#endif /* CONFIG_EEPROM_NUVOTON */

//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
void setSmb346CC_Curr900mA_Iterm50(void);
void setSmb346FloatVol4p32(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] = 0xE9;

     smb346_write_enable();
     smb346_write_reg(SMB346_FLOAT_VOLTAGE,i2cdata);	
}
void setSmb346OVDoNtEndChg(void)
{
     int status;	
     uint8_t i2cdata[32]={0};

     status=smb346_read_reg(SMB346_VARIOUS_FUNCTIONS,i2cdata);
     i2cdata[0] &= 0xFD;

     smb346_write_enable();
     smb346_write_reg(SMB346_VARIOUS_FUNCTIONS,i2cdata);	
}

void setSmb346PreventOverFCC(void)
{
	setSmb346OVDoNtEndChg();
	setSmb346CC_Curr900mA_Iterm50();
	setSmb346FloatVol4p32(); 
}
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

//Eason: A80 OTG pin control +++
#ifdef ASUS_A86_PROJECT
static void setSmb346OtgPinControl(void)
{
     uint8_t i2cdata[32]={0};

     i2cdata[0] |= 0x60;

     smb346_write_enable();
     smb346_write_reg(SMB346_OTHER_CONTROL_A,i2cdata);
	
}
#endif
#if 0 //Eason:A86 porting
#define OTG_GPIO	15
static int config_OTG_PMICpin_control(void)
{
	int ret;

	struct pm_gpio PMICgpio15_param = {
		.direction = PM_GPIO_DIR_OUT ,
     	   	.output_buffer = PM_GPIO_OUT_BUF_CMOS,
     	   	.output_value = 0,
       	.pull = PM_GPIO_PULL_NO,
   	 	.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};


        PMICgpio_15= PM8921_GPIO_PM_TO_SYS(OTG_GPIO);
 
	ret = gpio_request(PMICgpio_15, "OTG_config");
	if (ret) {
		pr_err("%s: unable to request pmic's gpio 15 (APSD_INOK)\n",__func__);
		return ret;
	}

	ret = pm8xxx_gpio_config(PMICgpio_15, &PMICgpio15_param);
	if (ret)
		pr_err("%s: Failed to configure PNICgpio15\n", __func__);
	
	return 0;
}
#endif  //Eason:A86 porting
//Eason: A80 OTG pin control ---

//Hank; add i2c stress test support+++
#ifdef CONFIG_I2C_STRESS_TEST
static int TestSmb346ChargerReadWrite(struct i2c_client *client)
{

	int status;
	uint8_t i2cdata[32]={0};
	i2c_log_in_test_case("[BAT][CHG][SMB][Test]Test Smb346Charger +++\n");
	 
     	status=smb346_read_reg(SMB346_CHG_CUR,i2cdata);
	if (status< 0) {
		i2c_log_in_test_case("[BAT][CHG][SMB][Test]Test Smb346Charger ---\n");
		return I2C_TEST_SMB346_FAIL;
	}
	
	status = smb346_write_reg(SMB346_CHG_CUR, i2cdata);
	if (status < 0) {
		i2c_log_in_test_case("[BAT][CHG][SMB][Test]Test Smb346Charger ---\n");
		return I2C_TEST_SMB346_FAIL;
	}

	i2c_log_in_test_case("[BAT][CHG][SMB][Test]Test Smb346Charger ---\n");
	return I2C_TEST_PASS;
};

static struct i2c_test_case_info gChargerTestCaseInfo[] =
{
	__I2C_STRESS_TEST_CASE_ATTR(TestSmb346ChargerReadWrite),
};
#endif
//Hank; add i2c stress test support---

//Hank: set higher hot temperature limit when CPU on+++
void setSmb346HigherHotTempLimit(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 51;//HLCT:10; HLHT:65; SLCT:15; SLHT:55;
	smb346_write_enable();
	smb346_write_reg(SMB346_HW_SW_LIMIT_CELL_TEMP,i2cdata);	
	i2cdata[0] = 100;//disable HTSL response
	smb346_write_reg(SMB346_THERM_SYSTEM_CONTROL,i2cdata);	
	printk("[BAT][CHG][SMB]%s():Set Higher Hot Temperature Limit & Disable HTSL Response\n",__func__);	
	
}
//Hank: set higher hot temperature limit when CPU on---

//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
//Iterm50  for robust float voltage
void setSmb346CC_Curr1250mA_Iterm50(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 241;//CC-Cur:1250mA
	smb346_write_enable();
	smb346_write_reg(SMB346_CHG_CUR,i2cdata);	
	pr_debug("[BAT][CHG][SMB]%s()Set CC current 1250mA Iterm50\n",__func__);	
}

void setSmb346CC_Curr900mA_Iterm50(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 145;
	smb346_write_enable();
	smb346_write_reg(SMB346_CHG_CUR,i2cdata);	
	pr_debug("[BAT][CHG][SMB]%s()Set CC current 1250mA Iterm50\n",__func__);	
}
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

//Hank: disable charger APSD+++
//#ifdef ASUS_A91_PROJECT
static void DisableSmb346APSD(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 57;//HLCT:10; HLHT:65; SLCT:15; SLHT:55;
	smb346_write_enable();
	smb346_write_reg(SMB346_CHARGE_CONTROL,i2cdata);	
	pr_debug("[BAT][CHG][SMB]Disable APSD\n");	
}
//#endif
//Hank: disable charger APSD---


//Hank: set default hot temperature limit when CPU off+++
//#ifdef ASUS_A91_PROJECT
void setSmb346DefaultHotTempLimit(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 17;//HLCT:10; HLHT:55; SLCT:15; SLHT:45;
	smb346_write_enable();
	smb346_write_reg(SMB346_HW_SW_LIMIT_CELL_TEMP,i2cdata);	
	i2cdata[0] = 102;//enable HTSL response
	smb346_write_reg(SMB346_THERM_SYSTEM_CONTROL,i2cdata);	
	printk("[BAT][CHG][SMB]%s():Set Default Hot Temperature Limit & Enable HTSL Response\n",__func__);	
	
}
//#endif
#ifdef ASUS_ME771KL_PROJECT
void setSmb345DefaultHotTempLimit(void)
{
	uint8_t i2cdata[32]={0};
	i2cdata[0] = 82;//HLCT:10; HLHT:55; SLCT:15; SLHT:45;
	smb346_write_enable();
	smb346_write_reg(SMB346_HW_SW_LIMIT_CELL_TEMP,i2cdata);	
	i2cdata[0] = 102;//enable HTSL response
	smb346_write_reg(SMB346_THERM_SYSTEM_CONTROL,i2cdata);	
	printk("[BAT][CHG][SMB]%s():Set Default Hot Temperature Limit & Enable HTSL Response\n",__func__);	
	
}
#endif
//Hank: set default hot temperature limit when CPU off---

static int smb346_i2c_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
	int err;
	//struct resource *res;

    	struct smb346_info *info;

	//Hank: move phone_bat out of qpnp-charger+++
	#ifdef CONFIG_BATTERY_ASUS
	struct asus_bat_phone_bat_struct *phone_bat;
	#endif
	//Hank: move phone_bat out of qpnp-charger---

    	g_smb346_info=info = kzalloc(sizeof(struct smb346_info), GFP_KERNEL);
    	g_smb346_slave_addr=client->addr;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
	

       printk("[BAT][CHG][SMB]%s+++\n",__FUNCTION__);

	#ifdef CONFIG_EEPROM_NUVOTON
        err = register_microp_notifier(&smb346_microp_notifier);
        notify_register_microp_notifier(&smb346_microp_notifier, "axc_smb346charger"); //ASUS_BSP Lenter+
	#endif /* CONFIG_EEPROM_NUVOTON */

     err = power_supply_register(&client->dev, &batt_psy);
    if (err < 0) {
        printk("[BAT][CHG][SMB]power_supply_register batt failed rc = %d\n", err);
    }

    err = power_supply_register(&client->dev, &usb_psy);
    if (err < 0) {
        printk("[BAT][CHG][SMB]power_supply_register usb failed rc = %d\n", err);
    }
    
    err= power_supply_register(&client->dev, &main_psy);
    if (err < 0) {
        printk("[BAT][CHG][SMB]power_supply_register ac failed rc = %d\n", err);
    }

   //Eason_Chang:for A90 internal ChgGau+++
   g_smb346_Psy_ready = true;
   //Eason_Chang:for A90 internal ChgGau---

   //Hank: remove AICL worker+++ 	
   //Eason: AICL work around +++
   //INIT_DELAYED_WORK(&AICLWorker,checkAICL); 
  //Eason: AICL work around ---
  //Hank: remove AICL worker---

   //Eason : support OTG mode+++
    if(NULL != notify_charger_i2c_ready_func_ptr){

         (*notify_charger_i2c_ready_func_ptr) ();

    }else{

         printk("[BAT][CHG][SMB]Charger i2c ready, Nobody registed..\n");
    }
   //Eason : support OTG mode---

	//Eason: A80 OTG pin control +++
	//Eason:A86 porting+++==============
	/*
	if(g_ASUS_hwID <= A86_SR1)
	{
		 //Eason: set default Float Voltage +++
    		setSmb346FloatVoltageDefault();
    		//Eason: set default Float Voltage ---
		setSmb346OtgPinControl();
		smb346Otg500Ma(false);
		//config_OTG_PMICpin_control();//Eason:A86 porting
	}
	*/
	//Eason:A90_EVB porting---==============
	//#ifdef ASUS_A91_PROJECT
	//Hank: disable charger APSD+++
	DisableSmb346APSD();
	//Hank: disable charger APSD---
	//#endif

	//Eason: A80 OTG pin control ---

	//Hank: add i2c stress test support +++
	#ifdef CONFIG_I2C_STRESS_TEST
		i2c_add_test_case(client, "Smb346Charger",ARRAY_AND_SIZE(gChargerTestCaseInfo));
	#endif
	//Hank: add i2c stress test support ---

	//ASUS_BSP+++ BennyCheng "add phone mode usb OTG support"
	if (A90_EVB0==g_ASUS_hwID) {
		gpio_request(GPIO_CHG_OTG, "CHG_OTG");
	}
	//ASUS_BSP--- BennyCheng "add phone mode usb OTG support"


	//Hank: move phone_bat out of qpnp-charger+++
	#ifdef CONFIG_BATTERY_ASUS
	phone_bat = kzalloc(sizeof(struct asus_bat_phone_bat_struct), GFP_KERNEL);
	if (!phone_bat) 
	{
		printk("[BAT][CHG][SMB][Prop] cannot allocate asus_bat_phone_bat_struct\n");
		return -ENOMEM;
	}
	phone_bat->phone_bat_power_supply_changed = asus_bat_status_change;
	phone_bat->get_prop_bat_status_byhw = get_bat_status;
	phone_bat->get_prop_batt_curr_byhw = get_current_now;

	asus_bat_set_phone_bat(phone_bat);
	#endif
	//Hank: move phone_bat out of qpnp-charger----
	
    printk("[BAT][CHG][SMB]%s---\n",__FUNCTION__);
    return err;
}

static int smb346_i2c_remove(struct i2c_client *client)
{
	//ASUS_BSP+++ BennyCheng "add phone mode usb OTG support"
	if (A90_EVB0==g_ASUS_hwID){
		gpio_free(GPIO_CHG_OTG);
	}
	//ASUS_BSP--- BennyCheng "add phone mode usb OTG support"
	power_supply_unregister(&batt_psy);
	power_supply_unregister(&usb_psy);
	power_supply_unregister(&main_psy);
	return 0;
}

//ASUS BSP Eason_Chang smb346 +++
/*
static struct platform_driver smb346_driver = {
	.probe	= smb346_probe,
	.remove	= smb346_remove,
	.driver	= {
		.name	= "asus_chg",
		.owner	= THIS_MODULE,
	},
};
*/
//ASUS BSP Eason_Chang smb346 ---

//ASUS BSP Eason_Chang +++
const struct i2c_device_id smb346_id[] = {
    {"smb346_i2c", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, smb346_id);

//Eason_Chang : A86 porting +++
static struct of_device_id smb346_match_table[] = {
	{ .compatible = "summit,smb346",},
	{ },
};
//Eason_Chang : A86 porting ---

static struct i2c_driver smb346_i2c_driver = {
    .driver = {
        .name = "smb346_i2c",
        .owner  = THIS_MODULE,
        .of_match_table = smb346_match_table,
     },
    .probe = smb346_i2c_probe,
    .remove = smb346_i2c_remove,
    //.suspend = smb346_suspend,
    //.resume = smb346_resume,
    .id_table = smb346_id,
};    
//ASUS BSP Eason_Chang ---

//Hank: another way to register i2c driver+++
/*
static int smb346_probe( struct platform_device *pdev )
{
	i2c_add_driver(&smb346_i2c_driver);
	//ASUS_BSP+++ BennyCheng "add vbus output support by charger IC for usb OTG"
	gpio_request(GPIO_CHG_OTG, "CHG_OTG");
	//ASUS_BSP--- BennyCheng "add vbus output support by charger IC for usb OTG"
	return 0;
}

static int __devexit smb346_remove( struct platform_device *pdev )
{
	//ASUS_BSP+++ BennyCheng "add vbus output support by charger IC for usb OTG"
	gpio_free(GPIO_CHG_OTG);
	//ASUS_BSP--- BennyCheng "add vbus output support by charger IC for usb OTG"
	return 0;
}

static struct platform_driver  smb346_driver = {
	.probe 	=  smb346_probe,
     	.remove   =  smb346_remove,

	.driver = {
		.name = "smb346",
		.owner = THIS_MODULE,
	},
};
*/
//Hank: another way to register i2c driver---
static int __init smb346_init(void)
{
    //ASUS BSP Eason_Chang +++
    printk("[BAT][CHG][SMB]init\n");
    //Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return -EINVAL; 
	}
    //Eason_Chang:for A90 internal ChgGau---	
    return i2c_add_driver(&smb346_i2c_driver);
    //ASUS BSP Eason_Chang ---
}

static void __exit smb346_exit(void)
{
    //ASUS BSP Eason_Chang +++
    printk("[BAT][CHG][SMB]exit\n");
    i2c_del_driver(&smb346_i2c_driver);
    //ASUS BSP Eason_Chang ---

}
// be be later after usb...
module_init(smb346_init);
module_exit(smb346_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS battery virtual driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Josh Liao <josh_liao@asus.com>");

//ASUS_BSP --- Josh_Liao "add asus battery driver"



#endif //#ifdef CONFIG_SMB_346_CHARGER

