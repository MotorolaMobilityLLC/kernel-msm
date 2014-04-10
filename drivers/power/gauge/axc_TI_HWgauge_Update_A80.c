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
//Eason: TIgauge update status+++
#include "axc_TI_HWgauge.h"
#include <linux/qpnp/qpnp-adc.h>//Hank: read voltage from PMIC
TIgauge_Up_Type g_TIgauge_update_status = UPDATE_NONE;
//Eason: TIgauge update status---
//Eason enterRomMode_test++++
extern int enterRomMode_test;//flag in AXC_BatteryService.c
//Eason enterRomMode_test---- 
//Eason unlock gauge+++
extern void TIgauge_LockStep(void);
//Eason unlock gauge---

//Hank; add i2c stress test support+++
/*
#ifdef CONFIG_I2C_STRESS_TEST
#include <linux/i2c_testcase.h>
#define I2C_TEST_TIgaugeUpdate_FAIL (-1)
#endif
*/
//Hank; add i2c stress test support---

//ASUS BSP Eason_Chang TIgauge+++
extern void msleep(unsigned int msecs);
static u32 g_TIgaugeUpdate_slave_addr=0;
static struct TIgaugeUpdate_info *g_TIgaugeUpdate_info=NULL;
struct TIgaugeUpdate_platform_data{
        int intr_gpio;
};

struct TIgaugeUpdate_info {
       struct i2c_client *i2c_client;
       struct TIgaugeUpdate_platform_data *pdata;
};
//ASUS BSP Eason_Chang TIgauge---
//Eason: TestVol3p7 +++
//static int TestVol3p7 = 3600;
//Eason: TestVol3p7 ---

//Hank: Enforce update gauge+++
static int EnforceUpdate = 0;
//Hank: Enforce update gauge---

static int TIgaugeUpdate_i2c_read(u8 addr, int len, void *data)
{
        int i=0;
        int retries=6;
        int status=0;

	struct i2c_msg msg[] = {
		{
			.addr = g_TIgaugeUpdate_slave_addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = g_TIgaugeUpdate_slave_addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

    pr_debug("[BAT]TIgaugeUpdate_i2c_read+++\n");
    
        if(g_TIgaugeUpdate_info){
            do{    
                pr_debug("[BAT]TIgauge smb346_i2c_transfer\n");
        		status = i2c_transfer(g_TIgaugeUpdate_info->i2c_client->adapter,
        			msg, ARRAY_SIZE(msg));
                
        		if ((status < 0) && (i < retries)){
        			    msleep(5);
                      
                                printk("%s retry %d\r\n", __FUNCTION__, i);                                
                                i++;
                     }
        	    } while ((status < 0) && (i < retries));
        }
        if(status < 0){
            printk("TIgauge: i2c read error %d \n", status);
        }
    pr_debug("[BAT]TIgaugeUpdate_i2c_read---\n");
    

        return status;
        
}

static int TIgaugeUpdate_i2c_write(u8 addr, int len, void *data)
{
    int i=0;
	int status=0;
	u8 buf[len + 1];
	struct i2c_msg msg[] = {
		{
			.addr = g_TIgaugeUpdate_slave_addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};
	int retries = 6;

	buf[0] = addr;
	memcpy(buf + 1, data, len);

	do {
		status = i2c_transfer(g_TIgaugeUpdate_info->i2c_client->adapter,
			msg, ARRAY_SIZE(msg));
        	if ((status < 0) && (i < retries)){
                    msleep(5);
           
                    printk("%s retry %d\r\n", __FUNCTION__, i);
                    i++;
              }
       } while ((status < 0) && (i < retries));

	if (status < 0) {
                    printk("TIgauge: i2c write error %d \n", status);
	}

	return status;
}

//Eason: when update gauge need to disable charge+++
extern void UpGaugeSetChg(bool enabled);
//Eason: when update gauge need to disable charge---
//If voltage < 3.7, don't do update process+++
extern int get_Volt_from_TIgauge(void);
//If voltage < 3.7, don't do update process---
bool  check_is_RomMode(void);
//Hank: check gauge dffs version+++
extern int g_gauge_df_version;
//Hank: check gauge dffs version---
#define error_base 10
#define error_status 10
extern void Check_DF_Version(void);
//Eason A90_EVB porting +++
extern struct qpnp_vadc_chip *asus_vadc;
//Eason A90_EVB porting ---
static ssize_t explain_dffs_string(char *OriginalMessages, unsigned long len)
{
	char ConvertMessages[293];

	char *delimit=" ";
	char *p;
	u8 MessageCount = 0;//Eason: Test_WXC_cmd
	uint8_t MessageValue[96]={0};//Eason: Test_WXC_cmd
	uint8_t Ccmd_CheckData = 0;//Eason: Test_WXC_cmd
	int status;//Eason: Test_WXC_cmd
	
	char *MessageTokenTemp;
	char *MessageToken;
	char *nextToken;
	int i;
	char cmd;//Todo[2]
	int slaveAddrValue;
	u8 regAddrValue;//Eason: Test_WXC_cmd
	u8 Ccmd_addr;//Eason: Test_WXC_cmd
	u8 Ccmd_ReadCount;//Eason: Test_WXC_cmd
	int waitmsec;
	//int I2Cerror = 0; //Todo[8]//Eason: Test_WXC_cmd

	//Hank:read voltage from PMIC+++
	struct qpnp_vadc_result result;
	int rc = -1;
	//int volt = 3500;
	//Hank:read voltage from PMIC---
	
	cmd = OriginalMessages[0];
	printk("cmd:%c\n",cmd);

	for(i=0 ; i<293 ;i++)
	ConvertMessages[i] = OriginalMessages[i+3];
 
	//Todo[2]: check command type, if X need use decimal type to read msleep 
	//Todo[3]: if command is C or W, slave address(string[3] string[4]) should be 0x16

	//Eason: Test_WXC_cmd+++
	pr_debug("command handle +++\n");
	//Eason: Test_WXC_cmd---
	//Eason: TIgauge update status+++
	if('F' == cmd)
	{
			g_TIgauge_update_status = UPDATE_PROCESS_FAIL;
			printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
			return 0;
	}else if('U' == cmd)
	{					
			g_TIgauge_update_status = UPDATE_FROM_ROM_MODE;
			printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
			return 0;
	}else if('O' == cmd)
	{
			g_TIgauge_update_status = UPDATE_OK;
			printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
			return 0;
	}else
	//Eason: TIgauge update status---
	//when update gauge need to disable charge+++
	if('D' == cmd)
	{
		UpGaugeSetChg(false);
		msleep(20);
		return 0;
		
	}
	else if('H' == cmd)
	{
			if(EnforceUpdate == 1 || EnforceUpdate == 4)
			{
				printk("[BAT][gaugeUp][status]skip check gauge HW version\n");
				return -ESRCH;
			}
			if(g_ASUS_hwID >= A90_EVB0)
			{
				printk("[BAT][gaugeUp][status]G4 gauge\n");
				return -EPERM;
			}
			else
			{
				printk("[BAT][gaugeUp][status]G3 gauge\n");
				return 0;
			}
	}
	else if('S' == cmd)
	{
			if(EnforceUpdate == 2 || EnforceUpdate == 4)
			{
				printk("[BAT][gaugeUp][status]skip check gauge dffs version\n");
				return -ESRCH;
			}

			printk("[BAT][gaugeUp][status]Gauge Dffs:%d before update\n",g_gauge_df_version);
			if(g_TIgauge_update_status != 1 &&  g_TIgauge_update_status != 2)//Wrong update status
				return -(error_base+error_status);
			else
				return -(error_base+g_gauge_df_version);

	}
	else if('E' == cmd)
	{
		UpGaugeSetChg(true);
		return 0;
	}else
	//when update gauge need to disable charge---
	//Eason: check if Voltage < 3.7V+++
	if('V' == cmd)
	{

		if(EnforceUpdate == 3 || EnforceUpdate == 4)
		{
			printk("[BAT][gaugeUp][status]skip check voltage\n");
			return -ESRCH;
		}
		rc = qpnp_vadc_read(asus_vadc, VBAT_SNS, &result);
		
		if (rc) {
			printk("[BAT][gaugeUp][TI]VADC read error with %d\n", rc);
			return -EPERM;
		}
		//volt  = result.physical/1000;
		printk("[BAT][gaugeUp][voltage]VADC read voltage %lld\n", result.physical);
		//Eason:after ER2 use PMIC voltage to judge+++
		if( (3700000 > result.physical )/*&&(g_A68_hwID>=A80_SR2)*/ ) 
 		{
			//Eason: TIgauge update status+++
			g_TIgauge_update_status = UPDATE_VOLT_NOT_ENOUGH;
			printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
			//Eason: TIgauge update status---
			return -EPERM;				
		}

		//Eason:after ER2 use PMIC voltage to judge---
		#if 0
		else	 if( (3700 > TestVol3p7)/*&&(g_A68_hwID<=A80_SR1)*/ )
		{
			//Eason: TIgauge update status+++
			g_TIgauge_update_status = UPDATE_VOLT_NOT_ENOUGH;
			printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
			//Eason: TIgauge update status---
			return -EPERM;
		}
		#endif
		else
			return 0;
	}else
	//Eason: check if Voltage < 3.7V---
	//Eason: check if in Rom mode+++
	if('R' == cmd)
	{
		if(true==check_is_RomMode())
			return -EPERM;
		else
			return 0;
		
	}else
	//Eason: check if in Rom mode---
	if('X' == cmd)
	{
		MessageTokenTemp = ConvertMessages;	
		MessageToken = strsep(&MessageTokenTemp, delimit);
		pr_debug("wait command:%s\n",MessageToken);
		waitmsec = (int)simple_strtol(MessageToken, NULL, 10);
		printk("[BAT][gaugeUp][X_cmd]:%d\n",waitmsec);
		//Eason: Test_WXC_cmd+++
		msleep(waitmsec);
			//Eason enterRomMode_test++++
			if(4000==waitmsec)
			{
				enterRomMode_test = 0;//exit RomMode
				TIgauge_LockStep();
				//update dffs fail, stay in rom mode show "?"+++
				printk("[BAT][gaugeUp]:Is_RomMode:%d\n",check_is_RomMode());
				//update dffs fail, stay in rom mode show "?"---

				Check_DF_Version();//Update down check df_version
			}	
			//Eason enterRomMode_test----	
		return -EPIPE;
		//Eason: Test_WXC_cmd---
	}else if( ('W' == cmd)||('C' == cmd) ){
#if 0//Eason:take off dummy code++	
		//Todo[3]++
		if( '1' != OriginalMessages[3])
			printk("error slave addr3\n");
		if( '6' != OriginalMessages[4])
			printk("error slave addr4\n");
		//Todo[3]--
		printk("slave addr: %c,%c\n",OriginalMessages[3],OriginalMessages[4]);
#endif //Eason:take off dummy code--
	
		MessageTokenTemp = ConvertMessages;	
		MessageToken = strsep(&MessageTokenTemp, delimit);
		nextToken = MessageTokenTemp;
		pr_debug("slave addr string:%s\n",MessageToken);
		slaveAddrValue = (int)simple_strtol(MessageToken, NULL, 16);
		pr_debug("slave addr string:0X%X\n",slaveAddrValue);

		p=strsep(&nextToken,delimit);
		pr_debug("reg addr sting:%s\n",p);
		regAddrValue=(u8)simple_strtol(p, NULL, 16);//Eason: Test_WXC_cmd(int)=>(u8)
		printk("reg addr string:0X%02X\n",regAddrValue);	

		//Todo[4]:MessageValue[0];I2C slave address will be 16 
		//Todo[5]:MessageValue[1];reg  address 
		//Todo[6]:depend on command type(W/C) do different process. 
		//Todo[7]:do real I2C command(W/C)
		while((p=strsep(&nextToken,delimit)))
		{
			pr_debug("%s\n",p);
			MessageValue[MessageCount]=(uint8_t)simple_strtol(p, NULL, 16);//Eason: Test_WXC_cmd(int)=>(uint8_t)
			pr_debug("[%d]0X%02X\n",MessageCount,MessageValue[MessageCount]);		
			MessageCount++;
		}
		//Eason: Test_WXC_cmd+++
		if('W' ==cmd)
		{
			status = TIgaugeUpdate_i2c_write(regAddrValue, MessageCount, MessageValue);
			       
			if(status > 0)
		   	{
				pr_debug("[BAT][gaugeUp][W_cmd][I2C]:success\n");
				return len;
			}else{
				printk("[BAT][gaugeUp][W_cmd][I2C]:fail\n");
				return -EAGAIN;
			}
		}else if('C' == cmd)
		{
			for(Ccmd_ReadCount=0; Ccmd_ReadCount < MessageCount ;Ccmd_ReadCount++)
			{
				Ccmd_addr = regAddrValue + Ccmd_ReadCount; 
				status = TIgaugeUpdate_i2c_read(Ccmd_addr, 1, &Ccmd_CheckData);
				if(status > 0)
		   		{
					pr_debug("[BAT][gaugeUp][C_cmd][I2C]:success\n");
				}else{
					printk("[BAT][gaugeUp][C_cmd][I2C]:fail\n");
					return -EAGAIN;
				}

				if( Ccmd_CheckData != MessageValue[Ccmd_ReadCount])
				{
					printk("[BAT][gaugeUp][C_cmd][Cmp]:fail\n");
					return -EAGAIN;	
				}else{
					printk("[BAT][gaugeUp][C_cmd][Cmp]:success\n");
					return -EFBIG;
				}
			} 
		}	
		//Eason: Test_WXC_cmd---
	}else{
		printk("[BAT][gaugeUp][Error_cmd]\n");
	}
	//Eason: Test_WXC_cmd+++
	pr_debug("command handle ---\n");
	//Eason: Test_WXC_cmd---

#if 0//Eason: Test_WXC_cmd+++
	//Todo[8]: return different error number let /system/bin/tigauge can do retry
	//Todo[8]+++
	if('C' == cmd)
		return -EFBIG;
	else if('W' == cmd)
		return len;
	else if('X' ==cmd)
		return -EPIPE;
	else if( 0!= I2Cerror )
		return -EAGAIN;
	else
#endif //Eason: Test_WXC_cmd+++
		return -EIO;
	//Todo[8]---
	//return len;//Todo[8]
}

void exitRomMode(void)
{
	long rt;
	/*
	char OutRomMode_1[296];
	char OutRomMode_2[296];
	char OutRomMode_3[296];
	char OutRomMode_4[296];
	char OutRomMode_5[296];
	char OutRomMode_6[296];
	char OutRomMode_7[296];
	*/
	char OutRomMode_1[] = "W: 16 00 05";
	char OutRomMode_2[] = "W: 16 64 05 00";
	char OutRomMode_3[] = "X: 170";
	char OutRomMode_4[] = "C: 16 04 9D 2E CF A2";
	char OutRomMode_5[] = "W: 16 00 0F";
	char OutRomMode_6[] = "W: 16 64 0F 00";
	char OutRomMode_7[] = "X: 4000";

	rt = explain_dffs_string(OutRomMode_1, strlen(OutRomMode_1));
	printk("return_1:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_2, strlen(OutRomMode_2));
	printk("return_2:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_3, strlen(OutRomMode_3));
	printk("return_3:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_4, strlen(OutRomMode_4));
	printk("return_4:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_5, strlen(OutRomMode_5));
	printk("return_5:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_6, strlen(OutRomMode_6));
	printk("return_6:%ld\n",rt);
	rt = explain_dffs_string(OutRomMode_7, strlen(OutRomMode_7));
	printk("return_7:%ld\n",rt);

}
//in rom mode show "?"+++
extern void asus_bat_status_change(void);//in Pm8921-charger.c
extern int g_IsInRomMode;
//in rom mode show "?"---
//Eason: TIgauge update status+++
extern  bool check_is_ActiveMode(void);
//Eason: TIgauge update status---
bool  check_is_RomMode(void)
{
	uint8_t CheckData = 0;
	bool isRomMode = false;
	int status=0;

	//Hank: Check is active mode first avoid i2c error+++
	if(false == check_is_ActiveMode())
	{
		status = TIgaugeUpdate_i2c_read(0x00, 1, &CheckData);
		if(status > 0)
		{
				printk("[BAT][gaugeUp][IsRomMode]:true,0x%02X\n",CheckData);
				isRomMode = true;
				//in rom mode show "?"+++
				g_IsInRomMode = true;
				//in rom mode show "?"---
		}else{
				//Eason: TIgauge update status+++
				if(false == check_is_ActiveMode())//Neither Rom mode nor Acitve mode, status UPDATE_CHECK_MODE_FAIL
				{
					g_TIgauge_update_status = UPDATE_CHECK_MODE_FAIL;
					printk("[BAT][gaugeUp][status]:%d\n",g_TIgauge_update_status);
				}
				//Eason: TIgauge update status---	
		
				printk("[BAT][gaugeUp][IsRomMode]:false,0x%02X\n",CheckData);
				isRomMode = false;
				//in rom mode show "?"+++
				g_IsInRomMode = false;
				//in rom mode show "?"---
		}
	}
	else
	{
		printk("[BAT][gaugeUp][IsRomMode]:false\n");
		//not in rom mode show charge icon+++
		g_IsInRomMode = false;
		//not in rom mode show charge icon---
	}
	//not in rom mode show charge icon in rom mode show "?"+++
	asus_bat_status_change();		
	//not in rom mode show charge icon in rom mode show "?"--- 
	//Hank: Check is active mode first avoid i2c error---
	return isRomMode;
}

//Eason UpdateTIgauge +++
static int UpdateTIgauge_proc_show(struct seq_file *seq, void *v)
{
	return 0;
}

static ssize_t UpdateTIgauge_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	char OriginalMessages[296];
	long rt;

	if (len > 296) {
		len = 296;
	}

	if (copy_from_user(OriginalMessages, buff, len)) {
		return -EFAULT;
	}

	pr_debug("UpdateTIgauge_write_proc+++\n");

	rt = explain_dffs_string(OriginalMessages, len);

	pr_debug("UpdateTIgauge_write_proc---\n");

	return rt;//Eason proc Array	
}

static void *UpdateTIgauge_proc_start(struct seq_file *seq, loff_t *pos)
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

static void *UpdateTIgauge_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void UpdateTIgauge_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations UpdateTIgauge_proc_seq = {
	.start		= UpdateTIgauge_proc_start,
	.show		= UpdateTIgauge_proc_show,
	.next		= UpdateTIgauge_proc_next,
	.stop		= UpdateTIgauge_proc_stop,
};

static int UpdateTIgauge_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &UpdateTIgauge_proc_seq);
}

static const struct file_operations UpdateTIgauge_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= UpdateTIgauge_proc_open,
	.read		= seq_read,
	.write		= UpdateTIgauge_write_proc,
};

void static create_UpdateTIgauge_proc_file(void)
{
	struct proc_dir_entry *UpdateTIgauge_proc_file = proc_create("driver/UpdateTIgauge", 0666, NULL, &UpdateTIgauge_proc_fops);

	if (!UpdateTIgauge_proc_file) {
		printk("[BAT][Bal]UpdateTIgauge create failed!\n");
    }

	return;
}
//Eason UpdateTIgauge ---
//Eason: TestVol3p7 +++
static ssize_t EnforceUpdate_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	return sprintf(page, "%d\n", EnforceUpdate);
}
static ssize_t EnforceUpdate_write_proc(struct file *filp, const char __user *buff, 
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

	EnforceUpdate = val;
     
        printk("[BAT][Bal]mode:%d\n",val);

	return len;
}

void static create_EnforceUpdate_proc_file(void)
{
	struct proc_dir_entry *EnforceUpdate_proc_file = create_proc_entry("driver/EnforceUpdate", 0666, NULL);

	if (EnforceUpdate_proc_file) {
		EnforceUpdate_proc_file->read_proc = EnforceUpdate_read_proc;
		EnforceUpdate_proc_file->write_proc = EnforceUpdate_write_proc;
	}
	else {
		printk("[BAT]EnforceUpdate proc file create failed!\n");
	}

	return;
}
//Eason: TestVol3p7 ---


//Hank; add i2c stress test support+++
/*
#ifdef CONFIG_I2C_STRESS_TEST
extern void TIgauge_UnlockStep1(void);
extern void TIgauge_UnlockStep2(void);
extern void TIgauge_EnterRomMode(void);
extern bool check_is_ActiveMode(void);
static int TestTIgaugeUpdateReadWrite(struct i2c_client *client)
{

	uint8_t CheckData = 0;
	//bool isRomMode = false;
	int status=0;

	i2c_log_in_test_case("Test TIgaugeUpdate +++\n");


	TIgauge_UnlockStep1();
	TIgauge_UnlockStep2();
	TIgauge_EnterRomMode();

	 printk("[BAT][enterRomMode_test]:Rom:%d,Active:%d\n",check_is_RomMode(),check_is_ActiveMode());//check Rom mode first
	 
     	status=TIgaugeUpdate_i2c_read(0x00, 1, &CheckData);
	if (status< 0) {
		exitRomMode();
		 printk("[BAT][enterRomMode_test]:Rom:%d,Active:%d\n",check_is_RomMode(),check_is_ActiveMode());//check Rom mode first
		i2c_log_in_test_case("Test TIgaugeUpdate ---\n");
		return I2C_TEST_TIgaugeUpdate_FAIL;
	}
	
	status = TIgaugeUpdate_i2c_write(0x00, 1, &CheckData);
	if (status < 0) {
		exitRomMode();
		 printk("[BAT][enterRomMode_test]:Rom:%d,Active:%d\n",check_is_RomMode(),check_is_ActiveMode());//check Rom mode first
		i2c_log_in_test_case("Test TIgaugeUpdate ---\n");
		return I2C_TEST_TIgaugeUpdate_FAIL;
	}

	exitRomMode();
	 printk("[BAT][enterRomMode_test]:Rom:%d,Active:%d\n",check_is_RomMode(),check_is_ActiveMode());//check Rom mode first
	i2c_log_in_test_case("Test TIgaugeUpdate ---\n");
	return I2C_TEST_PASS;
};

static struct i2c_test_case_info gTIgaugeUpdateTestCaseInfo[] =
{
	__I2C_STRESS_TEST_CASE_ATTR(TestTIgaugeUpdateReadWrite),
};
#endif
*/
//Hank; add i2c stress test support---

static int TIgauge_Update_i2c_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{

    struct TIgaugeUpdate_info *info;
    printk("%s+++\n",__FUNCTION__);

    g_TIgaugeUpdate_info=info = kzalloc(sizeof(struct TIgaugeUpdate_info), GFP_KERNEL);
    g_TIgaugeUpdate_slave_addr=client->addr;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
    	//Eason UpdateTIgauge test+++
	create_UpdateTIgauge_proc_file();
	//Eason UpdateTIgauge test---
	//Eason: TestVol3p7 +++
	create_EnforceUpdate_proc_file();
	//Eason: TestVol3p7 ---
	
	 //Hank: add i2c stress test support +++
	 /*
    	#ifdef CONFIG_I2C_STRESS_TEST

		i2c_add_test_case(client, "TIgaugeUpdate",ARRAY_AND_SIZE(gTIgaugeUpdateTestCaseInfo));

    	#endif
    	*/
    	//Hank: add i2c stress test support ---
    printk("%s---\n",__FUNCTION__);
    return 0;
}

static int TIgauge_Update_i2c_remove(struct i2c_client *client)
{
	return 0;
}

//Hank TIgauge Update++
const struct i2c_device_id TIgauge_Update_i2c_id[] = {
    {"TI_Gauge_Update_i2c", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, TIgauge_Update_i2c_id);

//Hank: A86 porting +++
static struct of_device_id bq27520_update_match_table[] = {
	{ .compatible = "TI,TI_Gauge_Update",},
	{ },
};
//Hank: A86 porting ---

static struct i2c_driver TIgauge_Update_i2c_driver = {
    .driver = {
        .name = "TI_Gauge_Update_i2c",
        .owner  = THIS_MODULE,
        .of_match_table = bq27520_update_match_table,
     },
    .probe = TIgauge_Update_i2c_probe,
    .remove = TIgauge_Update_i2c_remove,
    .id_table = TIgauge_Update_i2c_id,
};    

/*
static int TIgauge_probe( struct platform_device *pdev )
{
	i2c_add_driver(&TIgauge_Update_i2c_driver);
	//Hank TIgauge GPIO IRQ ++
	//config_hwGauge_irq_gpio_pin(&pdev->dev);
	//Hank TIgauge GPIO IRQ --
	return 0;
}

static int __devexit TIgauge_remove( struct platform_device *pdev )
{
	return 0;
}

static struct platform_driver  TIgauge_Update_driver = {
	.probe 	=  TIgauge_probe,
     	.remove   =  TIgauge_remove,

	.driver = {
		.name = "TI_Gauge_Update",
		.owner = THIS_MODULE,
	},
};
//Hank TIgauge Update---
*/

static int __init TIgaugeUpdate_init(void)
{
    //Hank +++
    printk("[BAT][TIgaugeUpdate]init\n");
	//Eason_Chang:for A90 internal ChgGau+++
	if(g_ASUS_hwID != A90_EVB0)
	{
		printk("[BAT]%s FAIL\n",__FUNCTION__);
		return -EINVAL; 
	}
	//Eason_Chang:for A90 internal ChgGau---	
    return i2c_add_driver(&TIgauge_Update_i2c_driver);
    //Hank ---
}

static void __exit TIgaugeUpdate_exit(void)
{
    //Hank +++
    printk("[BAT][TIgaugeUpdate]exit\n");
    i2c_del_driver(&TIgauge_Update_i2c_driver);
    //Hank ---

}
// be be later after usb...
module_init(TIgaugeUpdate_init);
module_exit(TIgaugeUpdate_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS TIgauge Update virtual driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Eason Chang <eason1_chang@asus.com>");
