/*
        TI bq27520-G3 Fuel Gauge Implementation

*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>

//#include "axc_TI_HWgauge_A80.h"//ASUS BSP Eason_Chang TIgauge
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#ifdef CONFIG_EEPROM_NUVOTON
#include <linux/microp_notify.h>
#include <linux/microp_api.h>
#endif /*CONFIG_EEPROM_NUVOTON */
#include <linux/platform_device.h>//ASUS_BSP Eason_Chang 1120 porting
#include <linux/i2c.h>//ASUS BSP Eason_Chang TIgauge
#include <linux/module.h>//ASUS BSP Eason_Chang A68101032 porting

//Hank TIgauge GPIO IRQ++
#include <linux/of_gpio.h>
//Hank TIgauge GPIO IRQ--

/*
#include <linux/of_gpio.h>//Hank TIgauge GPIO IRQ
*/
//ASUS BSP : Eason Chang check Df_version ++++++++
static struct delayed_work CheckDfVersionWorker;
bool DffsDown = false;
#define FAKE_TEMP (25)
//ASUS BSP : Eason Chang check Df_version --------

//Eason: TI HW GAUGE IRQ +++
#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE                NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)
//static int PMICgpio14; //Eason:A86 porting
//Eason: TI HW GAUGE IRQ ---

//Hank TIgauge update status++
#include "axc_TI_HWgauge.h"
char* UpdateStatusString; 
extern TIgauge_Up_Type g_TIgauge_update_status;
//Hank TIgauge update status--


//Hank TIgauge GPIO IRQ++
static int PMICgpio26;
//Hank TIgauge GPIO IRQ--

//Hank: update TIgauge global value+++ 
#include <linux/qpnp/qpnp-adc.h>
int  gBatteryTemp = 250;
int  gBatteryVol = 3500000;
int  gBatteryCurrent = 0;
int  gBatteryFCC = 0;
long long gPcbTemp = 0;
//Hank: update TIgauge global value---

//Hank; add i2c stress test support+++
#ifdef CONFIG_I2C_STRESS_TEST
#include <linux/i2c_testcase.h>
#define I2C_TEST_TIgauge_FAIL (-1)
#endif
//Hank; add i2c stress test support---

//ASUS BSP Eason_Chang get Temp from TIgauge+++
static int g_gauge_temp = 25;
#define TI_GAUGE_Error (-1)
//ASUS BSP Eason_Chang get Temp from TIgauge---

//check df version do Cap remapping+++
int g_gauge_df_version = 0;
//check df version do Cap remapping---

//Hank: show "?" when i2c error+++
#ifdef CONFIG_BAT_DEBUG
extern void asus_bat_status_change(void);
extern bool g_IsInRomMode;
#endif
//Hank: show "?" when i2c error---

//ASUS BSP Eason_Chang TIgauge+++
extern void msleep(unsigned int msecs);
static u32 g_TIgauge_slave_addr=0;
static int g_TIgauge_reg_value=0;
static int g_TIgauge_reg_address=0;
static struct TIgauge_info *g_TIgauge_info=NULL;
struct TIgauge_platform_data{
        int intr_gpio;
};

struct TIgauge_info {
       struct i2c_client *i2c_client;
       struct TIgauge_platform_data *pdata;
};
//ASUS BSP Eason_Chang TIgauge---

struct TIgauge_command{
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

enum TIgauge_CMD_ID {
		TIgauge_SOC=0,
		TIgauge_VOLT,
		TIgauge_AI,
		TIgauge_TEMP,
		TIgauge_FCC,
		TIgauge_CC,
		TIgauge_RM,
		TIgauge_FLAG_H,
		TIgauge_CNTL,
};

struct TIgauge_command TIgauge_CMD_Table[]={
		{"SOC",			0x2C,  1,		E_READ},			//StateOfCharge   
		{"VOLT",			0x08,  2,   	E_READ},			//Votage
		{"AI",			0x14,  2,   	E_READ},			//AverageCurrent
		{"TEMP",			0x06,  2,		E_READ},			//Temprature
		{"FCC",			0x12,  2,		E_READ},		  	//FullChargeCapacity
		{"CC",			0x2A,  2,		E_READ},			//CycleCount
		{"RM",			0x10,  2,		E_READ},			//RemainingCapacity
		{"FLAG_H", 		0x0B,  1,		E_READ},			//Flags_HighByte
		{"CNTL",			0x00,  2,		E_READWRITE},	//Control
    	  
};


struct TIgauge_command TIgaugeG4_CMD_Table[]={
		{"SOC",			0x20,  1,		E_READ},			//StateOfCharge     
		{"VOLT",			0x08,  2,   	E_READ},			//Votage
		{"AI",			0x14,  2,   	E_READ},			//AverageCurrent
		{"TEMP",			0x06,  2,		E_READ},			//Temprature
		{"FCC",			0x12,  2,		E_READ},		  	//FullChargeCapacity
		{"CC",			0x1e,  2,		E_READ},			//CycleCount
		{"RM",			0x10,  2,		E_READ},			//RemainingCapacity
		{"FLAG_H", 		0x0B,  1,		E_READ},			//Flags_HighByte
		{"CNTL",			0x00,  2,		E_READWRITE},	//Control
    	  
};



static int TIgauge_i2c_read(u8 addr, int len, void *data)
{
        int i=0;
        int retries=6;
        int status=0;

	struct i2c_msg msg[] = {
		{
			.addr = g_TIgauge_slave_addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = g_TIgauge_slave_addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

    pr_debug("[BAT][GAU][TI][I2c]TIgauge_i2c_read+++\n");
    
        if(g_TIgauge_info){
            do{    
                pr_debug("[BAT][GAU][TI][I2c]TIgauge smb346_i2c_transfer\n");
        		status = i2c_transfer(g_TIgauge_info->i2c_client->adapter,
        			msg, ARRAY_SIZE(msg));
                
        		if ((status < 0) && (i < retries)){
        			    msleep(5);
                      
                                printk("[BAT][GAU][TI][I2c]%s retry %d\r\n", __FUNCTION__, i);                                
                                i++;
                     }
        	    } while ((status < 0) && (i < retries));
        }
        if(status < 0)
	{
            printk("[BAT][GAU][TI][I2c]TIgauge: i2c read error %d \n", status);
	     //Hank: show "?" when i2c error+++
	    #ifdef CONFIG_BAT_DEBUG
           g_IsInRomMode = true;
	    asus_bat_status_change();
	    printk("[BAT][GAU][TI][I2c]g_IsInRomMode: %d \n", (int)g_IsInRomMode);
	    #endif
	    //Hank: show "?" when i2c error---	
       }
    pr_debug("[BAT][GAU][TI][I2c]TIgauge_i2c_read---\n");
    

        return status;
        
}


static int TIgauge_read_reg(int cmd, void *data)
{
    int status=0;
    if(g_ASUS_hwID  < A90_EVB0)
    {
	    if(cmd>=0 && cmd < ARRAY_SIZE(TIgauge_CMD_Table)){
	        if(E_WRITE==TIgauge_CMD_Table[cmd].rw || E_NOUSE==TIgauge_CMD_Table[cmd].rw){ // skip read for these command
	            printk("[BAT][GAU][TI][I2c]TIgauge: read ignore cmd\r\n");      
	        }
	        else{   
	            pr_debug("[BAT][GAU][TI][I2c]TIgauge_read_reg\n");
	            status=TIgauge_i2c_read(TIgauge_CMD_Table[cmd].addr, TIgauge_CMD_Table[cmd].len, data);
	        }    
	    }
	    else
	        printk("[BAT][GAU][TI][I2c]TIgauge: unknown read cmd\r\n");
    	}
	else
	{
		if(cmd>=0 && cmd < ARRAY_SIZE(TIgaugeG4_CMD_Table))
		{
	        	if(E_WRITE==TIgaugeG4_CMD_Table[cmd].rw || E_NOUSE==TIgaugeG4_CMD_Table[cmd].rw)
			{ // skip read for these command
	            		printk("[BAT][GAU][TI][I2c]TIgauge: read ignore cmd\r\n");      
	        	}
		       else
			{   
		            pr_debug("[BAT][GAU][TI][I2c]TIgauge_read_reg\n");
		            status=TIgauge_i2c_read(TIgaugeG4_CMD_Table[cmd].addr, TIgaugeG4_CMD_Table[cmd].len, data);
		        }    
	    }
	    else
	        printk("[BAT][GAU][TI][I2c]TIgauge: unknown read cmd\r\n");
	}
            
    return status;
}

static void TIgauge_proc_read(void)
{    
       int status;
       int16_t reg_value=0;//for 2 byte I2c date should use int16_t instead int, let negative value can show correctly. ex:current
       printk("[BAT][GAU][TI][Proc]%s \r\n", __FUNCTION__);
       status=TIgauge_read_reg(g_TIgauge_reg_address,&reg_value);
       g_TIgauge_reg_value = reg_value;

       if(status > 0 && reg_value >= 0){
            printk("[BAT][GAU][TI][Proc] found! TIgauge=%d\r\n",reg_value);
       }
}
//ASUS BSP Eason_Chang TIgauge +++

static int TIgauge_i2c_write(u8 addr, int len, void *data)
{
    int i=0;
	int status=0;
	u8 buf[len + 1];
	struct i2c_msg msg[] = {
		{
			.addr = g_TIgauge_slave_addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};
	int retries = 6;

	buf[0] = addr;
	memcpy(buf + 1, data, len);

	do {
		status = i2c_transfer(g_TIgauge_info->i2c_client->adapter,
			msg, ARRAY_SIZE(msg));
        	if ((status < 0) && (i < retries)){
                    msleep(5);
           
                    printk("[BAT][GAU][TI][I2c]%s retry %d\r\n", __FUNCTION__, i);
                    i++;
              }
       } while ((status < 0) && (i < retries));

	if (status < 0) 
	{
             printk("[BAT][GAU][TI][I2c]TIgauge: i2c write error %d \n", status);
	      //Hank: show "?" when i2c error+++
	     #ifdef CONFIG_BAT_DEBUG
            g_IsInRomMode = true;
	     asus_bat_status_change();
	     printk("[BAT][GAU][TI][I2c]g_IsInRomMode: %d \n", (int)g_IsInRomMode);
	     #endif
	     //Hank: show "?" when i2c error---	
	}

	return status;
}

static int TIgauge_write_reg(int cmd, void *data)
{
    int status=0;
    if(g_ASUS_hwID  < A90_EVB0)
    {
	    if(cmd>=0 && cmd < ARRAY_SIZE(TIgauge_CMD_Table)){
	        if(E_READ==TIgauge_CMD_Table[cmd].rw  || E_NOUSE==TIgauge_CMD_Table[cmd].rw){ // skip read for these command               
	        }
	        else
	            status=TIgauge_i2c_write(TIgauge_CMD_Table[cmd].addr, TIgauge_CMD_Table[cmd].len, data);
	    }
	    else
	        printk("[BAT][GAU][TI][I2c]TIgauge: unknown write cmd\r\n");
    }
    else
    {
    		if(cmd>=0 && cmd < ARRAY_SIZE(TIgaugeG4_CMD_Table))
		{
	       	 if(E_READ==TIgaugeG4_CMD_Table[cmd].rw  || E_NOUSE==TIgaugeG4_CMD_Table[cmd].rw)
			 { // skip read for these command               
	       	 }
	        	else
	            		status=TIgauge_i2c_write(TIgaugeG4_CMD_Table[cmd].addr, TIgaugeG4_CMD_Table[cmd].len, data);
	    	}
	       else
	        	printk("[BAT][GAU][TI][I2c]TIgauge: unknown write cmd\r\n");
    }
            
    return status;
}

static void TIgauge_proc_write(void)
{    
       int status;
       uint16_t i2cdata[32]={0};

       i2cdata[0] = g_TIgauge_reg_value;
       printk("[BAT][GAU][TI][Proc]%s:%d \r\n", __FUNCTION__,g_TIgauge_reg_value);
       status=TIgauge_write_reg(g_TIgauge_reg_address,i2cdata);

       if(status > 0 ){
            printk("[BAT][GAU][TI][Proc] proc write\r\n");
       }
}

static int TIgauge_read_table(int tableNumber)
{    
       int status;
       int16_t reg_value=0;

       status=TIgauge_read_reg(tableNumber,&reg_value);

       if(status > 0)
	{
            pr_debug("[BAT][GAU][TI] table found!=%d\r\n",reg_value);
            return reg_value;
            //Hank get Cap from TIgauge : in Rom mode dont update Cap+++»   
       }else{
		   printk("[BAT][GAU][TI] table read error\n");
		   return -1;
	   }
	   //Hank get Cap from TIgauge : in Rom mode dont update Cap---
}
//ASUS BSP Eason_Chang TIgauge  ---

//Hank unlock gauge+++
void TIgauge_LockStep(void)
{
       int status;
       uint8_t i2cdata[32]={0};
	int i = 0;   
	int retry = 2;

       printk("[BAT][GAU][TI][Up]%s++++\n", __FUNCTION__);
	//Hank: TIgauge lock fail retry+++
	do
	{
	       i2cdata[0] = 0x20;
	       i2cdata[1] = 0x00;

	       status=TIgauge_i2c_write(0x00, 2, i2cdata);

	       if((status<0) && (i<retry) )
	       {
	           i++;	
		    printk("[BAT][GAU][TI][Up]TIgauge_LockStep Fail Retry\r\n");
		    msleep(20);	
	       }
	}while((status<0) && (i<retry));
	if(status<0)
		printk("[BAT][GAU][TI][Up]TIgauge_LockStep Fail\r\n");
	else
		printk("[BAT][GAU][TI][Up]TIgauge_LockStep Success\r\n");	
	//Hank: TIgauge lock fail retry---   
	printk("[BAT][GAU][TI][Up]%s----\n", __FUNCTION__);	
}

void TIgauge_UnlockStep1(void)
{
       int status;
       uint8_t i2cdata[32]={0};
       	
       printk("[BAT][GAU][TI][Up]%s++++\n", __FUNCTION__);	
       
       i2cdata[0] = 0x14;
       i2cdata[1] = 0x04;
       status=TIgauge_i2c_write(0x00, 2, i2cdata);
       if(status > 0 ){
            printk("[BAT][GAU][TI][Up]TIgauge_UnlockStep1_1\r\n");
       }

       i2cdata[0] = 0x72;
       i2cdata[1] = 0x16;
       status=TIgauge_i2c_write(0x00, 2, i2cdata);
       if(status > 0 ){
            printk("[BAT][GAU][TI][Up] TIgauge_UnlockStep1_2\r\n");
       }

       msleep(20);

       printk("[BAT][GAU][TI][Up]%s----\n", __FUNCTION__);	
}

void TIgauge_UnlockStep2(void)
{
       int status;
       uint8_t i2cdata[32]={0};
       	
       printk("[BAT][GAU][TI][Up]%s++++\n", __FUNCTION__);	
       
       i2cdata[0] = 0xFF;
       i2cdata[1] = 0xFF;
       status=TIgauge_i2c_write(0x00, 2, i2cdata);
       if(status > 0 ){
            printk("[BAT][GAU][TI][Up] TIgauge_UnlockStep2_1\r\n");
       }

       i2cdata[0] = 0xFF;
       i2cdata[1] = 0xFF;
       status=TIgauge_i2c_write(0x00, 2, i2cdata);
       if(status > 0 ){
            printk("[BAT][GAU][TI][Up] TIgauge_UnlockStep2_2\r\n");
       }
	
       msleep(20);
	
       printk("[BAT][GAU][TI][Up]%s----\n", __FUNCTION__);	
}
//Hank unlock gauge---

//Hank: enter TIgauge Rom mode+++
void TIgauge_EnterRomMode(void)
{
       int status;
       uint8_t i2cdata[32]={0};

       printk("[BAT][GAU][TI][Up]%s++++\n", __FUNCTION__);	
       i2cdata[0] = 0x00;
       i2cdata[1] = 0x0F;

       status=TIgauge_i2c_write(0x00, 2, i2cdata);

       if(status > 0 ){
            printk("[BAT][GAU][TI][Up] TIgauge_EnterRomMode proc write\r\n");
       }

	//Hank unlock gauge+++
	 msleep(20);  	
	//Hank unlock gauge---
	
	printk("[BAT][GAU][TI][Up]%s----\n", __FUNCTION__);	
}

bool  check_is_ActiveMode(void)
{
	uint8_t CheckData = 0;
	bool isActiveMode = false;
	 int status=0;

	status = TIgauge_i2c_read(0x00, 1, &CheckData);
	if(status > 0)
	{
			printk("[BAT][GAU][TI][Mode]IsActiveMode:true,0x%02X\n",CheckData);
			isActiveMode = true;
	}else{
			printk("[BAT][GAU][TI][Mode]IsActiveMode:false,0x%02X\n",CheckData);
			isActiveMode = false;
	}
	return isActiveMode;
}	
//Hank: enter TIgauge Rom mode---

//ASUS BSP : Eason Chang check Df_version ++++++++
static int TIgauge_DF_VERSION(void);
void Check_DF_Version(struct work_struct *work)
{
	g_gauge_df_version = TIgauge_DF_VERSION();

	//Hank: extend DF available version to 10+++
	if( (10 >= g_gauge_df_version)&&( 1<= g_gauge_df_version) )
	//Hank: extend DF available version to 10---
	{
		DffsDown = true;
		printk("[BAT][GAU][TI][DF]DffsDown:true\n");
	}else if(0==g_gauge_df_version)
	{
		DffsDown = false;
		printk("[BAT][GAU][TI][DF]DffsDown:false\n");
	}else
	{
		//Hank: ME771KL do not repeat check DF Version+++
		#ifdef ASUS_A86_PROJECT
		schedule_delayed_work(&CheckDfVersionWorker, 20*HZ);
		#endif
		//Hank: ME771KL do not repeat check DF Version---
		printk("[BAT][GAU][TI][DF]Df version unknown:\n");
	}	
}
//ASUS BSP : Eason Chang check Df_version --------
//Eason report capacity to TIgauge when gauge read error, don't update capacity+++
extern int ReportCapToTIGauge(void);
//Eason report capacity to TIgauge when gauge read error, don't update capacity---
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
bool get_FC_flage_from_TIgauge(void)
{
	int TIflag_H;
	int FCflag_H;

	TIflag_H=TIgauge_read_table(TIgauge_FLAG_H);

	printk("[BAT][GAU]Flags high Byte:0x%02X\n",TIflag_H);//0x0B High byte, 0x0A Low byte
	
	FCflag_H = TIflag_H & 0x02;
	
	if(0x02 == FCflag_H)
		return true;
	else
		return false;
}
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---
//ASUS BSP Eason_Chang get Cap from TIgauge+++
int get_Cap_from_TIgauge(void)
{
	int TIsoc;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return 77; 
	}
	//Eason_Chang:for A90 internal ChgGau---
	
	TIsoc=TIgauge_read_table(TIgauge_SOC);

	//Eason: when gauge read error, don't update capacity+++
	if(TI_GAUGE_Error==TIsoc || TIsoc > 100 || TIsoc < 0)
	{
		printk("[BAT][GAU][TI][Cap]%s():Error %d\n",__func__,TIsoc);
		return ReportCapToTIGauge();
	}else{
		pr_debug("[BAT][GAU][TI][Cap]:%d\n",TIsoc);
		return TIsoc;
	}
	//Eason: when gauge read error, don't update capacity---
}
//ASUS BSP Eason_Chang get Cap from TIgauge---
//ASUS BSP Eason_Chang get Temp from TIgauge+++
int get_Temp_from_TIgauge(void)
{
	int TItemp;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return FAKE_TEMP; 
	}
	//Eason_Chang:for A90 internal ChgGau---
	TItemp=TIgauge_read_table(TIgauge_TEMP);
	
	pr_debug("[BAT][GAU][TI][Temp]%s():%d\n",__func__,TItemp);

	if(TI_GAUGE_Error==TItemp || TItemp < 2330 || TItemp > 3580)//temp < -40, temp > 85
	{
		printk("[BAT][GAU][TI][Temp]%s(): Error %d\n",__func__,TItemp);
		return (gBatteryTemp/10);//Hank:return last temp
	}
	else
	{
		if(true==DffsDown)
		{
			gBatteryTemp = (((TItemp/10)-273)*10);
			return (gBatteryTemp/10);
		}
		else
		{
			return FAKE_TEMP;
		}
	}
}
//ASUS BSP Eason_Chang get Temp FCC from TIgauge---
 //ASUS BSP Eason_Chang dump TIgauge info +++
static int get_Cntl_from_TIgauge(int TIcntl)
{
	int TIvalue;

	TIvalue=TIgauge_read_table(TIcntl);
	
	//printk("[BAT][TIgauge][CNTL:%d]:%d\n",TIcntl,TIvalue);

	if(TI_GAUGE_Error==TIvalue)
	{
		printk("[BAT][GAU][TI][Cntl:%d]:%d error!!\n",TIcntl,TIvalue);
		return TI_GAUGE_Error;
	}
	else
		return TIvalue;
}
 //ASUS BSP Eason_Chang dump TIgauge info ---
 //Eason : read Volt Curr Temp from Gauge+++
int get_Volt_from_TIgauge(void)
{
	int ret;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return 4111; 
	}
	//Eason_Chang:for A90 internal ChgGau---
	ret = get_Cntl_from_TIgauge(TIgauge_VOLT);
	if(ret == -1 || ret > 6000 || ret < 2800)//volt > 6V, volt < 2.8V  
	{
		printk("[BAT][GAU][TI][Voltage]%s(): Error %d\n",__func__,ret );
		ret = gBatteryVol/1000;//use last voltage
	}
	else
		gBatteryVol = (ret*1000);//Whenever read voltage, update gBatteryVol;
	pr_debug("[BAT][GAU][TI][Volt]:%d\n",ret);
	return ret;
}

int get_Curr_from_TIgauge(void)
{
	int ret;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return 333; 
	}
	//Eason_Chang:for A90 internal ChgGau---
	ret = (-1)*get_Cntl_from_TIgauge(TIgauge_AI);
	if( ret < -50000 || ret == 1)//curr > 50A
	{
		printk("[BAT][GAU][TI][Curr]%s(): Error %d\n",__func__,ret );
		ret = gBatteryCurrent/1000;//use last current
	}
	else		
		gBatteryCurrent = (ret*1000);//Whenever read current, update gBatteryCurrent;
	pr_debug("[BAT][GAU][TI][Cur]:%d\n",(-1)*ret);
	return ret;
}

int get_FCC_from_TIgauge(void)
{
	int ret;

	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return 2444; 
	}
	//Eason_Chang:for A90 internal ChgGau---
	ret = get_Cntl_from_TIgauge(TIgauge_FCC);
	if( ret > 5000 || ret == -1)//curr > 50A
	{
		printk("[BAT][GAU][TI][FCC]%s(): Error %d\n",__func__,ret );
		ret = gBatteryFCC;
	}
	else	
		gBatteryFCC = ret;
	pr_debug("[BAT][GAU][TI][FCC]:%d\n",ret);
	return ret;
}
//Eason : read Volt Curr Temp FCC from Gauge---
//ASUS BSP Eason_Chang get Temp from TIgauge+++
static int TIgaugeTemp_proc_show(struct seq_file *seq, void *v)
{
	g_gauge_temp = get_Temp_from_TIgauge();
	return seq_printf(seq, "%d\n", g_gauge_temp);
}

static ssize_t TIgaugeTemp_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
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

	g_gauge_temp = val;
     
	printk("[BAT][GAU][TI][Proc]mode:%d\n",val);

	return len;
}

static void *TIgaugeTemp_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *TIgaugeTemp_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void TIgaugeTemp_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations TIgaugeTemp_proc_seq = {
	.start		= TIgaugeTemp_proc_start,
	.show		= TIgaugeTemp_proc_show,
	.next		= TIgaugeTemp_proc_next,
	.stop		= TIgaugeTemp_proc_stop,
};

static int TIgaugeTemp_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &TIgaugeTemp_proc_seq);
}

static const struct file_operations TIgaugeTemp_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= TIgaugeTemp_proc_open,
	.read		= seq_read,
	.write		= TIgaugeTemp_write_proc,
};

void static create_TIgaugeTemp_proc_file(void)
{
	struct proc_dir_entry *TIgaugeTemp_proc_file = proc_create("driver/BatTemp", 0644, NULL, &TIgaugeTemp_proc_fops);

	if (!TIgaugeTemp_proc_file) {
		printk("[BAT][GAU][TI][Proc]TIgaugeTemp proc file create failed!\n");
	}

	return;
}
//ASUS BSP Eason_Chang get Temp from TIgauge---
//ASUS BSP Eason_Chang TIgauge +++
static ssize_t TIgauge_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	TIgauge_proc_read();
	return sprintf(page, "%d\n", g_TIgauge_reg_value);
}
static ssize_t TIgauge_write_proc(struct file *filp, const char __user *buff, 
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


	g_TIgauge_reg_value = val;

      TIgauge_proc_write();
    
    printk("[BAT][GAU][TI][Proc]mode:%d\n",g_TIgauge_reg_value);
	
	return len;
}

void static create_TIgauge_proc_file(void)
{
	struct proc_dir_entry *TIgauge_proc_file = create_proc_entry("driver/TIgauge", 0644, NULL);

	if (TIgauge_proc_file) {
		TIgauge_proc_file->read_proc = TIgauge_read_proc;
		TIgauge_proc_file->write_proc = TIgauge_write_proc;
	}
    else {
		printk("[BAT][GAU][TI][Proc]proc file create failed!\n");
    }

	return;
}

static ssize_t TIgaugeAddress_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	return sprintf(page, "%d\n", g_TIgauge_reg_address);
}
static ssize_t TIgaugeAddress_write_proc(struct file *filp, const char __user *buff, 
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


	g_TIgauge_reg_address = val;  
    
    printk("[BAT][GAU][TI][Proc]TIgaugeAddress:%d\n",val);
	
	return len;
}

void static create_TIgaugeAddress_proc_file(void)
{
	struct proc_dir_entry *TIgaugeAddress_proc_file = create_proc_entry("driver/TIgaugeAddr", 0644, NULL);

	if (TIgaugeAddress_proc_file) {
		TIgaugeAddress_proc_file->read_proc = TIgaugeAddress_read_proc;
		TIgaugeAddress_proc_file->write_proc = TIgaugeAddress_write_proc;
	}
    else {
		printk("[BAT][GAU][TI][Proc]Addr proc file create failed!\n");
    }

	return;
}

//Hank: add TIgauge current_now proc file+++
static ssize_t TIgaugeCurrentNow_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	int rc = get_Cntl_from_TIgauge(TIgauge_AI);
	return sprintf(page, "%d\n", rc);
}
static ssize_t TIgaugeCurrentNow_write_proc(struct file *filp, const char __user *buff, 
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


    
    	printk("[BAT][GAU][TI][Proc]Do not support write\n");
	
	return len;
}

void static TIgaugeCurrentNow_create_proc_file(void)
{
	struct proc_dir_entry *TIgaugeCurrentNow_proc_file = create_proc_entry("driver/current_now", 0644, NULL);

	if (TIgaugeCurrentNow_proc_file) {
		TIgaugeCurrentNow_proc_file->read_proc = TIgaugeCurrentNow_read_proc;
		TIgaugeCurrentNow_proc_file->write_proc = TIgaugeCurrentNow_write_proc;
	}
    else {
		printk("[BAT][GAU][TI][Proc]proc file create failed!\n");
    }

	return;
}
//Hank: add TIgauge current_now proc file---

//ASUS BSP Eason_Chang TIgauge ---
//ASUS BSP Eason_Chang dump TIgauge info +++
static int TIgauge_CHEMID(void)
{
       int status;
       uint8_t i2cdata[32]={0};

       pr_debug("[BAT][GAU][TI][DF]%s++++\n", __FUNCTION__);	
       i2cdata[0] = 0x08;
       i2cdata[1] = 0x00;

       status=TIgauge_i2c_write(0x00, 2, i2cdata);//Before read CHEM_ID, need echo 0x0008 to address 0x01/0x00 first 

       if(status < 0 ){
            printk("[BAT][GAU][TI][DF]TIgauge_CHEMID() read error!!\r\n");
       }

	pr_debug("[BAT][GAU][TI][DF]%s----\n", __FUNCTION__);
	msleep(2);//Hank: add delay to avoid read error
	return get_Cntl_from_TIgauge(TIgauge_CNTL);
}

static int TIgauge_DF_VERSION(void)
{
	int status;
	uint8_t i2cdata[32]={0};

	pr_debug("[BAT][GAU][TI][DF]%s++++\n", __FUNCTION__);      
	i2cdata[0] = 0x1F;
	i2cdata[1] = 0x00;

	status=TIgauge_i2c_write(0x00, 2, i2cdata);//Before read DF_VERSION, need echo 0x001F to address 0x01/0x00 first

	if(status < 0 ){
		printk("[BAT][GAU][TI][DF]TIgauge_DF_VERSION() read error!!\r\n");
	}

	pr_debug("[BAT][GAU][TI][DF]%s----\n", __FUNCTION__);
	msleep(2);//Hank: add delay to avoid read error
	return get_Cntl_from_TIgauge(TIgauge_CNTL);
}

//Hank TIgauge Update++
static void showUpdateStatusString(TIgauge_Up_Type updateStatus)
{


	switch(updateStatus)
	{
		case UPDATE_NONE:
			UpdateStatusString = "UPDATE_NONE";
			return ;
		case UPDATE_OK:
			UpdateStatusString = "UPDATE_OK";
			return ;
		case UPDATE_FROM_ROM_MODE:
			UpdateStatusString = "UPDATE_FROM_ROM_MODE";
			return ;
		case UPDATE_VOLT_NOT_ENOUGH:
			UpdateStatusString = "UPDATE_VOLT_NOT_ENOUGH";
			return ;
		case UPDATE_CHECK_MODE_FAIL:
			UpdateStatusString = "UPDATE_CHECK_MODE_FAIL";
			return ;
		case UPDATE_ERR_MATCH_OP_BUF:
			UpdateStatusString = "UPDATE_ERR_MATCH_OP_BUF";
			return ;
		case UPDATE_PROCESS_FAIL:
			UpdateStatusString = "UPDATE_PROCESS_FAIL";
			return ;
		default:
			UpdateStatusString = "ERROR";
			return ;
	}
}
//Hank TIgauge Update--

//For PMIC voltage info+++
//extern int get_voltage_for_ASUSswgauge(void);
//For PMIC voltage info---

//Hank: add dffs update node+++
extern int check_is_RomMode(void);
static ssize_t UpdateDffs_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	int chemId;
	
	chemId = TIgauge_CHEMID();
	g_gauge_df_version = TIgauge_DF_VERSION();
	showUpdateStatusString(g_TIgauge_update_status);
	
	return sprintf(page, "IsRomMode:%d\n"
					     "CHEM_ID: 0x%04X\n"
					     "DF_VERSION: 0x%04X\n"
					     "update_status:%s\n"
					     ,check_is_RomMode()
					     ,chemId
					     ,g_gauge_df_version
					     ,UpdateStatusString);
	
}
static ssize_t UpdateDffs_write_proc(struct file *filp, const char __user *buff, 
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

     
        printk("[BAT][UpdateDffs]Do no support write mode:%d\n",val);

	return len;
}

void static create_UpdateDffs_proc_file(void)
{
	struct proc_dir_entry *UpdateDffs_proc_file = create_proc_entry("driver/UpdateDffs", 0666, NULL);

	if (UpdateDffs_proc_file) {
		UpdateDffs_proc_file->read_proc = UpdateDffs_read_proc;
		UpdateDffs_proc_file->write_proc = UpdateDffs_write_proc;
	}
	else {
		printk("[BAT]TestVol3p7 proc file create failed!\n");
	}

	return;
}
//Hank: add dffs update node---


static ssize_t TIgaugeDump_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	int chemId;
	int df_version;
	
	chemId = TIgauge_CHEMID();
	df_version = TIgauge_DF_VERSION();
	showUpdateStatusString(g_TIgauge_update_status);

	return sprintf(page, "FCC(mAh): %d\n"
						"RM(mAh): %d\n"
						"SOC: %d\n"
						"VOLT(mV): %d\n"
						"AI(mA): %d\n"
						"TEMP(degC): %d\n"
						"CC: %d\n"
						"CHEM_ID: 0x%04X\n"
						"DF_VERSION: 0x%04X\n"
						"update_status:%s\n"
						"FC_status:%d\n"
						,get_Cntl_from_TIgauge(TIgauge_FCC)
						,get_Cntl_from_TIgauge(TIgauge_RM)
						,get_Cap_from_TIgauge()
						,get_Cntl_from_TIgauge(TIgauge_VOLT)
						,get_Cntl_from_TIgauge(TIgauge_AI)
						,get_Temp_from_TIgauge()
						,get_Cntl_from_TIgauge(TIgauge_CC)
						,chemId
						,df_version
						,UpdateStatusString
						,get_FC_flage_from_TIgauge());
	
}

void static create_TIgaugeDump_proc_file(void)
{
	struct proc_dir_entry *TIgaugeDump_proc_file = create_proc_entry("driver/bq27520_test_info_dump", 0444, NULL);

	if (TIgaugeDump_proc_file) {
		TIgaugeDump_proc_file->read_proc = TIgaugeDump_read_proc;
	}
	else {
		printk("[BAT][GAU][TI][Proc]TIgaugeDump proc file create failed!\n");
	}

	return;
}

 //ASUS BSP Eason_Chang dump TIgauge info ---
//Eason: TI HW GAUGE IRQ +++
#if 0  //Eason:A86 porting
#define HW_GAUGE_IRQ_GPIO	14
static int hw_gauge_irq; 

static irqreturn_t  asus_hw_gauge_irq_handler(int irq, void *dev_id)
{
	printk("[BAT]%s() triggered \r\n", __FUNCTION__);

	//schedule_delayed_work(&asus_bat->phone_b.bat_low_work, 0); 
	printk("[BAT][HWgauge]irq:%d\n",gpio_get_value(PMICgpio14));

	return IRQ_HANDLED;
}

static int config_hwGauge_irq_gpio_pin(void)
{
	int ret;

	struct pm_gpio PMICgpio14_param = {
		.direction = PM_GPIO_DIR_IN ,
     	   	.output_buffer = PM_GPIO_OUT_BUF_CMOS,
     	   	.output_value = 0,
       	.pull = PM_GPIO_PULL_UP_30,
   	 	.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};


        PMICgpio14 = PM8921_GPIO_PM_TO_SYS(HW_GAUGE_IRQ_GPIO);
 
	ret = gpio_request(PMICgpio14, "TI_HWGAUGE_IRQ");
	if (ret) {
		pr_err("%s: unable to request pmic's gpio 14 (BAT_HWGAUGE)\n",__func__);
		return ret;
	}

	ret = pm8xxx_gpio_config(PMICgpio14, &PMICgpio14_param);
	if (ret)
		pr_err("%s: Failed to configure gpio\n", __func__);

	//enable TI HW GAUGE IRQ +++
	hw_gauge_irq = gpio_to_irq(PMICgpio14);
	
	enable_irq_wake(hw_gauge_irq);

	ret = request_irq(hw_gauge_irq , asus_hw_gauge_irq_handler, \
			IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "TI_HWGAUGE_IRQ", NULL);

    	if (ret) {
		printk("request_hw_gauge_irq  failed, ret = %d\n", ret);
    	} 
	//enable TI HW GAUGE IRQ ---
	
	return 0;
}
#endif  //Eason:A86 porting
//Eason: TI HW GAUGE IRQ ---


//Hank TIgauge GPIO IRQ ++
#define HW_GAUGE_IRQ_GPIO	26
static int hw_gauge_irq; 

static irqreturn_t  asus_hw_gauge_irq_handler(int irq, void *dev_id)
{
	printk("[BAT][GAU][TI]%s() triggered \r\n", __FUNCTION__);

	//schedule_delayed_work(&asus_bat->phone_b.bat_low_work, 0); 
	printk("[BAT][GAU][TI][irq:%d\n",gpio_get_value(PMICgpio26));

	return IRQ_HANDLED;
}

static int config_hwGauge_irq_gpio_pin(struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	
    PMICgpio26 = of_get_named_gpio(np,"gauge-gpio",0);
   
    
    if(!gpio_is_valid(PMICgpio26))
		 printk("[BAT][GAU][TI][%s: PMICgpio26 is not valid = %d",__func__,PMICgpio26);
 
	ret = gpio_request(PMICgpio26, "TI_HWGAUGE_IRQ");
	if (ret) {
		pr_err("[BAT][GAU][TI][%s: unable to request pmic's gpio 26 (BAT_HWGAUGE)\n",__func__);
		return ret;
	}

	
	ret = gpio_direction_input(PMICgpio26);
	if(ret<0){
		pr_err("[BAT][GAU][TI][%s: not able to set TI_HWGAUGE_IRQ as input\n",__func__);
		gpio_free(PMICgpio26);
	}
	

	//enable TI HW GAUGE IRQ +++
	hw_gauge_irq = gpio_to_irq(PMICgpio26);
	if(hw_gauge_irq<0)
		printk("[BAT][GAU][TI][Unable to get irq number for TIgauge GPIO\n");
	
	enable_irq_wake(hw_gauge_irq);

	ret = request_irq(hw_gauge_irq , asus_hw_gauge_irq_handler, \
			IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "TI_HWGAUGE_IRQ", NULL);

    	if (ret) {
		printk("[BAT][GAU][TI][request_hw_gauge_irq  failed, ret = %d\n", ret);
    	} 
	//enable TI HW GAUGE IRQ ---
	
	return 0;
}
//Hank TIgauge GPIO IRQ--

//Hank: add i2c stress test support+++
#ifdef CONFIG_I2C_STRESS_TEST
static int TestTIgaugeReadWrite(struct i2c_client *client)
{

	
	int status;
	int16_t reg_value=0;
	i2c_log_in_test_case("[BAT][GAU][TI][Test]Test TIgauge +++\n");
	 
     	status=TIgauge_read_reg(7,&reg_value);
	if (status< 0) {
		i2c_log_in_test_case("[BAT][GAU][TI][Test]Test TIgauge Read ---\n");
		return I2C_TEST_TIgauge_FAIL;
	}
	
	status = TIgauge_write_reg(7, &reg_value);
	if (status < 0) {
		i2c_log_in_test_case("[BAT][GAU][TI][Test]Test TIgauge Write ---\n");
		return I2C_TEST_TIgauge_FAIL;
	}

	
	i2c_log_in_test_case("[BAT][GAU][TI][Test]Test TIgauge  ---\n");
	return I2C_TEST_PASS;
};

static struct i2c_test_case_info gTIgaugeTestCaseInfo[] =
{
	__I2C_STRESS_TEST_CASE_ATTR(TestTIgaugeReadWrite),
};
#endif
//Hank: add i2c stress test support---


#define UPDATE_BATGLOBAL_PERIOD      120      // 2min 
#define ERROR_UPDATE_BATGLOBAL_PERIOD   5      // 5sec 
#define UPDATE_FAIL -1

//Hank: update TIgauge global value+++ 
static struct delayed_work update_TIgaugeGlobalValue_worker;
static int  gUpdateFlag = 0;
extern int get_charge_type(void);

//Eason A90_EVB porting +++
extern struct qpnp_vadc_chip *asus_vadc;
//Eason A90_EVB porting ---
static void updatePcbThermal(void)
{

	struct qpnp_vadc_result result;
	int rc = -1;

	rc = qpnp_vadc_read(asus_vadc, LR_MUX4_PU2_AMUX_THM1, &result);

	if (rc) {
		printk("[BAT][GAU][TI]VADC read error with %d\n", rc);
		return;
	}
	gPcbTemp  = result.physical;
	//printk("[BAT][GAU][TI]gPcbTemp: %lld\n", gPcbTemp);


}
void UpdateTIgaugeGlobalValue(void)
{
	
   pr_debug("[BAT][CHG][SMB][Prop]%s()+++\n",__FUNCTION__);   
   if(get_Volt_from_TIgauge() == -1)
   {
	gUpdateFlag = UPDATE_FAIL;
	printk("[BAT][CHG][SMB][Prop]%s:Voltage update fail\n",__FUNCTION__);
   }
   else
   	gBatteryVol = (get_Volt_from_TIgauge()*1000);
   
   gBatteryCurrent = (get_Curr_from_TIgauge()*1000);
   gBatteryTemp = (get_Temp_from_TIgauge()*10);
   updatePcbThermal();
   get_charge_type(); 
   pr_debug("[BAT][CHG][SMB][Prop]%s()---\n",__FUNCTION__);
}

static void  UpdateTIgaugeGlobalValue_work(struct work_struct *work)
{	
    //struct delayed_work *dwork = to_delayed_work(work);

    UpdateTIgaugeGlobalValue();

    if(gUpdateFlag == UPDATE_FAIL)
    {
    	printk("[BAT][CHG][SMB][Prop]%s(): fail!\n ",__func__);
	gUpdateFlag = 0;
	schedule_delayed_work(&update_TIgaugeGlobalValue_worker,ERROR_UPDATE_BATGLOBAL_PERIOD*HZ);
    }
    else
    {
    	gUpdateFlag = 0;
    	schedule_delayed_work(&update_TIgaugeGlobalValue_worker,UPDATE_BATGLOBAL_PERIOD*HZ);
    }
    
}
//Hank: update TIgauge global value---

static int TIgauge_i2c_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
	
    struct TIgauge_info *info;
    g_TIgauge_info=info = kzalloc(sizeof(struct TIgauge_info), GFP_KERNEL);
    g_TIgauge_slave_addr=client->addr;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);

    printk("[BAT][GAU][TI]%s+++\n",__FUNCTION__);
    //ASUS BSP Eason_Chang TIgauge +++
    create_TIgauge_proc_file();
    create_TIgaugeAddress_proc_file(); 
    //ASUS BSP Eason_Chang TIgauge ---
    //ASUS BSP Eason_Chang get Temp from TIgauge+++
    create_TIgaugeTemp_proc_file();
    //ASUS BSP Eason_Chang get Temp from TIgauge---
    //ASUS BSP Eason_Chang dump TIgauge info +++
    create_TIgaugeDump_proc_file();
    //ASUS BSP Eason_Chang dump TIgauge info ---
      //Hank: add TIgauge current_now proc file+++
    TIgaugeCurrentNow_create_proc_file();	
     //Hank: add TIgauge current_now proc file---
     //Hank: add dffs update node+++
     create_UpdateDffs_proc_file();
     //Hank: add dffs update node---
    //Hank: TI HW GAUGE IRQ +++
    config_hwGauge_irq_gpio_pin(&client->dev);//Eason:A86 porting
    //Hank: TI HW GAUGE IRQ ---
    //ASUS BSP : Eason Chang check Df_version ++++++++
    INIT_DELAYED_WORK(&CheckDfVersionWorker,Check_DF_Version);
    schedule_delayed_work(&CheckDfVersionWorker, 0*HZ);
   //ASUS BSP : Eason Chang check Df_version --------

   //Hank: update global variable right now+++
   INIT_DELAYED_WORK(&update_TIgaugeGlobalValue_worker,UpdateTIgaugeGlobalValue_work);
   pr_debug("[BAT][CHG][SMB][Prop]:Update global value right now!\n");
   schedule_delayed_work(&update_TIgaugeGlobalValue_worker,round_jiffies_relative(0));
   //Hank: update global variable right now---

     //Hank: add i2c stress test support +++
    #ifdef CONFIG_I2C_STRESS_TEST
	i2c_add_test_case(client, "TIgauge",ARRAY_AND_SIZE(gTIgaugeTestCaseInfo));
    #endif
    //Hank: add i2c stress test support ---
    printk("[BAT][GAU][TI]%s---\n",__FUNCTION__);
    return 0;
}

static int TIgauge_i2c_remove(struct i2c_client *client)
{
	return 0;
}

//ASUS BSP Eason_Chang +++
const struct i2c_device_id TIgauge_id[] = {
    {"TI_bq27520_Gauge_i2c", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, TIgauge_id);

//Eason_Chang : A86 porting +++
static struct of_device_id bq27520_match_table[] = {
	{ .compatible = "TI,TI_bq27520_Gauge",},
	{ },
};
//Eason_Chang : A86 porting ---


static struct i2c_driver TIgauge_i2c_driver = {
    .driver = {
        .name = "TI_bq27520_Gauge_i2c",
        .owner  = THIS_MODULE,
        .of_match_table = bq27520_match_table,
     },
    .probe = TIgauge_i2c_probe,
    .remove = TIgauge_i2c_remove,
    //.suspend = smb346_suspend,
    //.resume = smb346_resume,
    .id_table = TIgauge_id,
};    
//ASUS BSP Eason_Chang ---
//Hank another way to register i2c driver---
/*
static int TIgauge_probe( struct platform_device *pdev )
{
	i2c_add_driver(&TIgauge_i2c_driver);
	//Hank TIgauge GPIO IRQ ++
	//config_hwGauge_irq_gpio_pin(&pdev->dev);
	//Hank TIgauge GPIO IRQ --
	return 0;
}

static int __devexit TIgauge_remove( struct platform_device *pdev )
{
	return 0;
}

static struct platform_driver  TIgauge_driver = {
	.probe 	=  TIgauge_probe,
     	.remove   =  TIgauge_remove,

	.driver = {
		.name = "TI_bq27520_Gauge",
		.owner = THIS_MODULE,
	},
};
*/
//Hank another way to register i2c driver+++

static int __init TIgauge_init(void)
{
    //ASUS BSP Eason_Chang +++
    printk("[BAT][GAU][TI]init\n");
	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return -EINVAL; 
	}
	//Eason_Chang:for A90 internal ChgGau---
    return i2c_add_driver(&TIgauge_i2c_driver);
    //return platform_driver_register(&TIgauge_driver);
    //ASUS BSP Eason_Chang ---
}

static void __exit TIgauge_exit(void)
{
    //ASUS BSP Eason_Chang +++
    printk("[BAT][GAU][TI][exit\n");
    i2c_del_driver(&TIgauge_i2c_driver);
    //ASUS BSP Eason_Chang ---

}
// be be later after usb...
module_init(TIgauge_init);
module_exit(TIgauge_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS TIgauge virtual driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Eason Chang <eason1_chang@asus.com>");

//ASUS_BSP --- Josh_Liao "add asus battery driver"

