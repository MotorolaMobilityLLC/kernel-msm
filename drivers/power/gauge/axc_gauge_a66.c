/* kernel/drivers/power/gauge/axc_gauge_a66.c
 *
 * Copyright (C) 2012 ASUS Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
 
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/delay.h>

#include "axc_gaugefactory.h"
#include "axc_gauge_a66.h"
//ASUS_BSP Eason_Chang add event log +++
#include <linux/asusdebug.h>
//ASUS_BSP Eason_Chang add event log ---
//ASUS_BSP Eason read battery ID+++
#include <linux/qpnp/qpnp-adc.h>
//ASUS_BSP Eason read battery ID---


/* -------------------------------------------------------------------------- */

#define BATTERY_RESISTOR_CALIBRATION_FILE_PATH    "/data/data/BattResicali.nvm"

#define OCV_PER_SPEEDUP_UPDATE		12
#define OCV_PER_SPEEDUP_UPDATE_14P	14//Hank: A86 change update interval when Cap<=14% 

#define TIMES_FOR_GENERAL_READ_ADC		15

#define DEFAULT_DEVICE_RESISTOR_VALUE_NVT   136
#define DEFAULT_DEVICE_RESISTOR_VALUE_TWS	146//ASUS_BSP Eason read battery ID
#define DEFAULT_DEVICE_RESISTOR_VALUE_ME175KG   190
#define DEFAULT_DEVICE_RESISTOR_VALUE_A11	100
#define DEFAULT_DEVICE_RESISTOR_VALUE_A68M	100

#define RESISTOR_MAX_VAL	200
#define RESISTOR_MIN_VAL		100

#define DEFAULT_DEVICE_RESISTOR_VALUE (150) 

/* UPDATE_INERVAL */
#define UPDATE_INERVAL_DEFAULT			180
#define UPDATE_INERVAL_BAT_LOW_IRQ		110
#define UPDATE_INERVAL_WHEN_CAP_LESS_14P	60//Hank: A86 change update interval when Cap<=14% 

/* TBL_SIZE - different table size */
#define OCV_TBL_SIZE		101

#ifdef CONFIG_PM_8226_CHARGER
extern int pm8226_is_full(void);
extern void pm8226_chg_enable_charging(bool enable);
extern void pm8226_chg_usb_suspend_enable(int enable);
#else
extern bool smb346_IsFull(void);
#endif

//ASUS_BSP Eason read battery ID+++
long long g_BatteryID_value=0; 
//ASUS_BSP Eason read battery ID--- 

#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern char g_CHG_mode;
#endif

/* -------------------------------------------------------------------------- */
/*
int NVT_SrcOCVtoPerTbl[OCV_TBL_SIZE]={
	3400, 3489, 3554, 3606, 3646, 3669, 3678, 3684, 3688, 3690,
	3693, 3695, 3698, 3700, 3706, 3717, 3725, 3732, 3742, 3748,
	3753, 3757, 3763, 3767, 3772, 3775, 3779, 3783, 3785, 3789,
	3792, 3795, 3799, 3801, 3804, 3805, 3808, 3810, 3813, 3815,
	3818, 3820, 3823, 3826, 3829, 3832, 3835, 3839, 3842, 3845,

	3850, 3853, 3858, 3863, 3867, 3872, 3877, 3883, 3889, 3897,
	3905, 3917, 3926, 3935, 3944, 3951, 3958, 3965, 3972, 3978,
	3986, 3992, 3999, 4007, 4014, 4020, 4031, 4039, 4046, 4055,
	4064, 4071, 4080, 4090, 4097, 4108, 4118, 4127, 4135, 4146,
	4155, 4165, 4175, 4185, 4196, 4207, 4217, 4228, 4238, 4248,
	4260
};
*/
int NVT_SrcOCVtoPerTbl[OCV_TBL_SIZE]={
	3400, 3465, 3530, 3582, 3600, 3623, 3633, 3640, 3644, 3646,
	3649, 3651, 3655, 3657, 3662, 3672, 3679, 3686, 3695, 3702,
	3709, 3714, 3721, 3726, 3732, 3736, 3741, 3745, 3747, 3751,
	3754, 3758, 3762, 3764, 3767, 3768, 3771, 3773, 3777, 3779,
	3782, 3784, 3787, 3790, 3794, 3797, 3800, 3804, 3807, 3811,

	3816, 3819, 3825, 3830, 3833, 3838, 3843, 3849, 3855, 3862,
	3871, 3884, 3894, 3904, 3914, 3921, 3928, 3936, 3943, 3949,
	3958, 3965, 3974, 3983, 3991, 3998, 4009, 4016, 4022, 4031,
	4039, 4046, 4055, 4065, 4072, 4083, 4093, 4102, 4110, 4122,
	4132, 4143, 4155, 4165, 4196, 4207, 4217, 4228, 4238, 4248,
	4260
};
/*
int TWS_SrcOCVtoPerTbl[OCV_TBL_SIZE]={
	3400, 3483, 3543, 3590, 3628, 3655, 3665, 3670, 3672, 3674, 
	3675, 3677, 3682, 3689, 3695, 3700, 3704, 3709, 3714, 3718,
	3722, 3725, 3728, 3731, 3733, 3737, 3741, 3745, 3748, 3753,
	3757, 3762, 3765, 3768, 3771, 3774, 3776, 3779, 3781, 3784,
	3787, 3790, 3793, 3796, 3799, 3803, 3806, 3810, 3813, 3817,

	3821, 3826, 3830, 3835, 3839, 3845, 3850, 3856, 3862, 3868,
	3876, 3885, 3895, 3906, 3915, 3924, 3932, 3940, 3948, 3956,
	3963, 3970, 3979, 3987, 3995, 4003, 4012, 4020, 4029, 4038,
	4047, 4057, 4066, 4075, 4085, 4095, 4105, 4115, 4125, 4135,
	4145, 4156, 4166, 4177, 4188, 4199, 4209, 4221, 4232, 4245,
	4260
};
*/

//TWS discharge  vf  OCVtable version20120904 +++
int TWS_SrcOCVtoPerTbl[OCV_TBL_SIZE]={
	3400, 3480, 3543, 3592, 3608, 3636, 3648, 3653, 3655, 3657, 
	3659, 3660, 3666, 3673, 3679, 3682, 3685, 3691, 3694, 3699,
	3706, 3710, 3713, 3717, 3721, 3726, 3731, 3734, 3738, 3742,
	3745, 3749, 3752, 3756, 3760, 3761, 3765, 3767, 3770, 3774,
	3777, 3779, 3782, 3786, 3790, 3793, 3796, 3799, 3803, 3808,

	3812, 3815, 3821, 3826, 3830, 3836, 3839, 3846, 3853, 3858,
	3863, 3872, 3880, 3888, 3894, 3899, 3906, 3915, 3923, 3930,
	3939, 3948, 3958, 3966, 3975, 3984, 3990, 3999, 4007, 4017,
	4025, 4033, 4043, 4052, 4062, 4071, 4080, 4091, 4101, 4113,
	4123, 4135, 4148, 4158, 4188, 4199, 4210, 4221, 4232, 4246,
	4260
};
//TWS discharge  vf  OCVtable version20120904 ---

#ifdef ASUS_ME175KG_PROJECT
int ME175KG_SrcOCVtoPerTbl[OCV_TBL_SIZE]={
	3501, 3531, 3576, 3618, 3639, 3655, 3665, 3673, 3679, 3684,
	3688, 3692, 3695, 3698, 3701, 3704, 3707, 3711, 3714, 3717,
	3720, 3724, 3727, 3731, 3734, 3738, 3742, 3746, 3750, 3754,
	3758, 3762, 3766, 3771, 3775, 3780, 3785, 3790, 3794, 3799,
	3805, 3810, 3815, 3820, 3826, 3832, 3837, 3843, 3849, 3855, 

	3861, 3867, 3874, 3880, 3886, 3893, 3900, 3907, 3913, 3920,
	3927, 3934, 3941, 3948, 3955, 3962, 3969, 3977, 3984, 3992, 
	4000, 4009, 4017, 4026, 4035, 4044, 4054, 4063, 4072, 4082, 
	4092, 4102, 4112, 4122, 4133, 4143, 4154, 4165, 4176, 4187, 
	4198, 4210, 4221, 4232, 4244, 4255, 4267, 4279, 4292, 4305, 
	4310
};
#endif



/* CurrOCVtoPerTbl - to determine which ocv to percent table used */
const int *CurrOCVtoPerTbl = NULL;

typedef union mytemp mytemp;

union mytemp{
	s16 mnValue;
	char mnChar[2];
};


/* -------------------------------------------------------------------------- */

extern int get_temp_for_ASUSswgauge(void);
extern int get_voltage_for_ASUSswgauge(void);
extern int get_current_for_ASUSswgauge(void);

/* -------------------------------------------------------------------------- */
//ASUS BSP Eason filter adc read current <= 15mA when doesn't has cable +++
extern bool report_BatteryService_If_HasCable(void);
//ASUS BSP Eason filter adc read current <= 15mA when doesn't has cable ---
//Eason: Do VF with Cable when boot up+++
static bool WhenBootUpDoVf = true;
extern int getIfonline(void);
//Eason: Do VF with Cable when boot up---
//Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status +++ 
bool g_adc_get_correct_VolCurr = false;
//Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status ---
//Eason repoert capacity to gauge for judge interval+++
int ReportCapToGaugeForJudgeInterval(void);
//Eason repoert capacity to gauge for judge interval---

static void AXC_Gauge_A66_ReadVoltageCurrentFromADC(AXI_Gauge *apGauge, int *volt, int *curr)
{
	*curr = get_current_for_ASUSswgauge();
	*volt = get_voltage_for_ASUSswgauge();
	return;
}

static int find_tbl_index(int target,const int *array, int array_size)
{
	int i = 0;
	int target_index = -1;

	BUG_ON(NULL == array);

	if (target <= array[0]) { 
		target_index = 0;
	} else if(target >= array[array_size-1]) {
		target_index = array_size-1;
	} else {
		for (i = 1; i < array_size; i++)
		{
			if(target < array[i])
				break;
		}
		target_index = i-1;
    }
	return target_index;
}
//Hank read BatteryID+++
extern int64_t pm8226_read_battery_id(void);
int64_t read_battery_id(void)
{
	g_BatteryID_value = pm8226_read_battery_id();
	return g_BatteryID_value;
}
//Hank read BatteryID---
static int init_resistor_cali(AXC_Gauge_A66 *this)
{
	struct file *fd;
	mm_segment_t mmseg_fs;
	int FileLength = 0;
	mytemp lnResistorCalibrationData;

	mmseg_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = filp_open( BATTERY_RESISTOR_CALIBRATION_FILE_PATH, O_RDONLY,0);
	if (IS_ERR(fd))
	{
		pr_err( "[BAT][Gau][A66]error! can not open battery Resistor cali file, use default value.\n");
//ASUS_BSP Eason read battery ID+++  
#ifdef ASUS_ME175KG_PROJECT
	this->resistorCali = DEFAULT_DEVICE_RESISTOR_VALUE_ME175KG;
#else
	if(g_BatteryID_value<1100000){
		this->resistorCali = DEFAULT_DEVICE_RESISTOR_VALUE_NVT;
	}
	else{
		this->resistorCali = DEFAULT_DEVICE_RESISTOR_VALUE_TWS;
	}
#endif
//ASUS_BSP Eason read battery ID---            
		set_fs(mmseg_fs);
		return -1;
	}

	FileLength = fd->f_op->read(fd, lnResistorCalibrationData.mnChar, 2, &fd->f_pos);
	if (2 <= FileLength) {
		this->resistorCali = lnResistorCalibrationData.mnValue;
		pr_info( "[BAT][Gau][A66] init R Cali:%d\n", this->resistorCali);
	} else {
		pr_err( "[BAT][Gau][A66] cannot read R Cali\n");
	}

	if (filp_close(fd, NULL))
       		pr_err("[BAT][Gau][A66] fail to close resistor cali file in %s\n", __func__);
	
	set_fs(mmseg_fs);
  	return 0;
}

//ASUS_BSP  +++ Eason_Chang "add BAT info time"
static void ReportTime(void)
{
	struct timespec ts;
	struct rtc_time tm;
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	pr_debug ("[BAT][Gau][A66] %d-%02d-%02d %02d:%02d:%02d,T=%d\n"
		,tm.tm_year + 1900
		,tm.tm_mon + 1
		,tm.tm_mday
		,tm.tm_hour
		,tm.tm_min
		,tm.tm_sec
		,get_temp_for_ASUSswgauge());
}
//ASUS_BSP  --- Eason_Chang "add BAT info time"


static int map_per_with_ocv(int anVocv)
{
    int target_index = 0;

    target_index = find_tbl_index(anVocv, CurrOCVtoPerTbl, OCV_TBL_SIZE);
    if (target_index == -1) {
    	pr_err("[BAT][Gau]There are something wrong in OCV(%d) to Percentage\n",anVocv);
        target_index = 0;
    } else {
		pr_debug( "[BAT][Gau] ocv per index:%d\n", target_index);
	}

    return target_index;	
}

/* cal_ocv_percent_when_discharging - Calculate battery life when discharging.
 * Use formula, OCV = V + I*R, to get ocv persent. 
 */
static int cal_ocv_percent_when_discharging(
	AXC_Gauge_A66 *this,
	int volt,
	int curr)
{
	int ocv = 0;
	int percent = 0;

	int resistor;

	resistor = this->resistorCali;

//ASUS_BSP Eason read battery ID+++    
#ifdef ASUS_ME175KG_PROJECT
	resistor = DEFAULT_DEVICE_RESISTOR_VALUE_ME175KG;
#else
	if(g_BatteryID_value<1100000){
		resistor = DEFAULT_DEVICE_RESISTOR_VALUE_NVT;
	}
	else{
		resistor = DEFAULT_DEVICE_RESISTOR_VALUE_TWS;
	}
#endif
//ASUS_BSP Eason read battery ID---    

	if(curr < 0){
		ocv = volt;
	}
	else{
		ocv = volt + curr*resistor/1000;
	}

	percent = map_per_with_ocv(ocv);
	/*printk( "[BAT][Gau]ocv per=%d, ocv=%d, v=%d, c=%d, r=%d\n",
		 percent, ocv, volt, curr, resistor);*/
    printk( "[BAT][Gau]ocv per=%d, ocv=%d, r=%d\n",
		 percent, ocv, resistor);
    
    //ASUS_BSP Eason_Chang add event log +++
//    ASUSEvtlog( "[BAT][Gau]ocv per=%d, ocv=%d, r=%d\n", percent, ocv, resistor);
    //ASUS_BSP Eason_Chang add event log ---

	return percent;
}

/* cal_ocv_percent_by_volt_directly - Calculate ocv percent by voltage directly.
 * Usually used by the case of discharging to read volt.
 */
//Eason always use V+IR  ++++++++// 
/*
static int cal_ocv_percent_by_volt_directly(
	int volt)
{
	int percent = 0;

	percent = map_per_with_ocv(volt);
	// Essential information for OCV table
	printk( "[BAT][Gau]ocv per=%d, ocv=v=%d\n",
		percent,
		volt);
	return percent;
}
*/
//Eason always use V+IR  --------//
static int decideVoltSec(int volt)
{
	int i=0;
	int j;
	int voltTableSec[20];

	for(j=0;j<20;j++)
	{ 
#ifdef ASUS_ME175KG_PROJECT
		voltTableSec[j]=3447+47*j;
#else
		voltTableSec[j]=3440+40*j;
#endif
	}

	while(voltTableSec[i]<=volt && i<20)
	{
		i++;
	}

	return i;
}

enum currentSec{
    currSectionI = 0,
    currSectionII,    
    currSectionIII,
};    

static int decideCurrSec(int curr)
{
    if(curr <= 300){
        return currSectionI;
    }else if(curr >= 900){
        return currSectionIII;
    }else{
        return currSectionII;
    }    
}

static int decideVf300(int voltSec)
{
#ifdef ASUS_ME175KG_PROJECT
	int Vf300TableSec[20] = {65,63,60,58,56,50,43,35,36,28,38,40,36,36,36,36,37,38,37,37};
							//3447 to 4340	interval:47 mV, total:20
#else
	int Vf300TableSec[20] = {29,27,27,29,29,35,38,43,41,28,28,28,29,25,24,25,25,24,15,15};
							//3440 to 4200  interval:40 mV , total:20
#endif
	if(voltSec > 19){                      
		return Vf300TableSec[19];
	}else{
		return Vf300TableSec[voltSec];                      
	}
}

static int decideVf900(int voltSec)
{
#ifdef ASUS_ME175KG_PROJECT
	int Vf900TableSec[20] = {161,150,135,124,120,115,110,107,104,103,103,108,114,103,102,103,105,107,108,108};
							//3447 to 4340	interval:47 mV, total:20
#else
	//int Vf900TableSec[20] = {15,15,15,9,19,22,29,31,42,45,48,52,64,65,67,65,60,61,33,33};
	int Vf900TableSec[20] = {15,15,15,9,19,22,29,21,32,35,28,27,39,40,42,40,35,36,8,8};	
							//3440 to 4200  interval:40 mV , total:20
#endif
	if(voltSec > 19){                      
		return Vf900TableSec[19];
	}else{
		return Vf900TableSec[voltSec];                      
	}
}

static int decideVolSecSpec(int voltSec)
{
	if(0==voltSec){
#ifdef ASUS_ME175KG_PROJECT
		return 3447;
#else
		return 3440;
#endif
	}else if(20==voltSec){
#ifdef ASUS_ME175KG_PROJECT
		return 4340;
#else
		return 4200;
#endif
    }else{
#ifdef ASUS_ME175KG_PROJECT
		return (3447+47*(voltSec-1));
#else
		return (3440+40*(voltSec-1));
#endif
	}
}

static void doAdcVfModify(int* volt,int* curr)
{   
	int voltSec;
	int currSec;
	long Vfcurr_seci;
	long Vf300_seci;
	long Vf900_seci;
	long Vfcurr_sec_iplusOne;
	long Vf300_sec_iplusOne;
	long Vf900_sec_iplusOne;
	long Vminus;
	long voltSpecSeci;

	*curr = (*curr)*(-1);//for calculate convinient

	voltSec = decideVoltSec(*volt);
	currSec = decideCurrSec(*curr);

	pr_debug("[BAT][vf]Vsec:%d,Csec:%d\n",voltSec,currSec);


	if(0==voltSec){//voltSec:0~20
		Vf300_seci = decideVf300(voltSec);
		Vf300_sec_iplusOne = decideVf300(voltSec);
	}else if(20==voltSec){
		Vf300_seci = decideVf300(voltSec-1);
		Vf300_sec_iplusOne = decideVf300(voltSec-1);
	}else{
		Vf300_seci = decideVf300(voltSec-1);
		Vf300_sec_iplusOne = decideVf300(voltSec);
	}

	if(0==voltSec){//voltSec:0~20
		Vf900_seci = decideVf900(voltSec);
		Vf900_sec_iplusOne = decideVf900(voltSec);
	}else if(20==voltSec){
		Vf900_seci = decideVf900(voltSec-1);
		Vf900_sec_iplusOne = decideVf900(voltSec-1);
	}else{
		Vf900_seci = decideVf900(voltSec-1);
		Vf900_sec_iplusOne = decideVf900(voltSec);
	}


	if(currSectionII==currSec){
		Vfcurr_seci = Vf300_seci + (*curr-300)*(Vf900_seci-Vf300_seci)/(900-300);
		Vfcurr_sec_iplusOne = Vf300_sec_iplusOne 
							+ (*curr-300)*(Vf900_sec_iplusOne-Vf300_sec_iplusOne)/(900-300);
	}else if(currSectionI==currSec){
		Vfcurr_seci = Vf300_seci;
		Vfcurr_sec_iplusOne = Vf300_sec_iplusOne;
	}else{//currSectionIII==currSec
		Vfcurr_seci = Vf900_seci;
		Vfcurr_sec_iplusOne = Vf900_sec_iplusOne;
	}


	voltSpecSeci =  decideVolSecSpec(voltSec);

#ifdef ASUS_ME175KG_PROJECT
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/47;
#elif defined (ASUS_A11_PROJECT)
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/47;
#elif defined (ASUS_A68M_PROJECT)
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/47;
#else
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/40;
#endif

	pr_debug("[BAT][vf]Vf300i:%ld,Vf300i+1:%ld,Vf900i:%ld,Vf900i+1:%ld,Vfcurri:%ld,Vfcurri+1:%ld,Vminus:%ld\n"
					,Vf300_seci,Vf300_sec_iplusOne,Vf900_seci,Vf900_sec_iplusOne
					,Vfcurr_seci,Vfcurr_sec_iplusOne,Vminus);

	*volt = (*volt)-(int)Vminus;
	*curr = (*curr)*(-1);//for calculate convinient

}


//Eason: A91 use 200 & 1100mA VF table +++
static int decideVf200(int voltSec)
{
	int Vf300TableSec[20] = {47,47,47,47,47,45,45,48,48,51,52,42,53,53,51,58,58,57,56,25};
							//3447 to 4340	interval:47 mV, total:20

	if(voltSec > 19){                      
		return Vf300TableSec[19];
	}else{
		return Vf300TableSec[voltSec];                      
	}
}

static int decideVf1100(int voltSec)
{
	int Vf900TableSec[20] = {200,200,200,200,200,200,200,200,200,201,207,204,205,214,207,188,191,188,200,156};
							//3447 to 4340	interval:47 mV, total:20

	if(voltSec > 19){                      
		return Vf900TableSec[19];
	}else{
		return Vf900TableSec[voltSec];                      
	}
}
static void doAdcVfModify_A91(int* volt,int* curr)
{   
    int voltSec;
    int currSec;
    long Vfcurr_seci;
    long Vf200_seci;
    long Vf1100_seci;
    long Vfcurr_sec_iplusOne;
    long Vf200_sec_iplusOne;
    long Vf1100_sec_iplusOne;
    long Vminus;
    long voltSpecSeci;
    

    *curr = (*curr)*(-1);//for calculate convinient
    
    voltSec = decideVoltSec(*volt);
    currSec = decideCurrSec(*curr);

    pr_debug("[BAT][vf]Vsec:%d,Csec:%d\n",voltSec,currSec);

   
    if(0==voltSec){//voltSec:0~20
        Vf200_seci = decideVf200(voltSec);
        Vf200_sec_iplusOne = decideVf200(voltSec);
    }else if(20==voltSec){
        Vf200_seci = decideVf200(voltSec-1);
        Vf200_sec_iplusOne = decideVf200(voltSec-1);
    }else{
        Vf200_seci = decideVf200(voltSec-1);
        Vf200_sec_iplusOne = decideVf200(voltSec);
    }

    if(0==voltSec){//voltSec:0~20
        Vf1100_seci = decideVf1100(voltSec);
        Vf1100_sec_iplusOne = decideVf1100(voltSec);
    }else if(20==voltSec){
        Vf1100_seci = decideVf1100(voltSec-1);
        Vf1100_sec_iplusOne = decideVf1100(voltSec-1);
    }else{
        Vf1100_seci = decideVf1100(voltSec-1);
        Vf1100_sec_iplusOne = decideVf1100(voltSec);
    }
    

    if(currSectionII==currSec){
        Vfcurr_seci = Vf200_seci + (*curr-200)*(Vf1100_seci-Vf200_seci)/(1100-200);
        Vfcurr_sec_iplusOne = Vf200_sec_iplusOne 
                            + (*curr-200)*(Vf1100_sec_iplusOne-Vf200_sec_iplusOne)/(1100-200);
    }else if(currSectionI==currSec){
        Vfcurr_seci = Vf200_seci;
        Vfcurr_sec_iplusOne = Vf200_sec_iplusOne;
    }else{//currSectionIII==currSec
        Vfcurr_seci = Vf1100_seci;
        Vfcurr_sec_iplusOne = Vf1100_sec_iplusOne;
    }


    voltSpecSeci =  decideVolSecSpec(voltSec);

#ifdef CONFIG_PM_8941_CHARGER
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/47;
#else
	Vminus = Vfcurr_seci + (*volt-voltSpecSeci)*(Vfcurr_sec_iplusOne-Vfcurr_seci)/40;
#endif

    pr_debug("[BAT][vf]Vf200i:%ld,Vf200i+1:%ld,Vf1100i:%ld,Vf1100i+1:%ld,Vfcurri:%ld,Vfcurri+1:%ld,Vminus:%ld\n"
                    ,Vf200_seci,Vf200_sec_iplusOne,Vf1100_seci,Vf1100_sec_iplusOne
                    ,Vfcurr_seci,Vfcurr_sec_iplusOne,Vminus);

    *volt = (*volt)-(int)Vminus;
    *curr = (*curr)*(-1);//for calculate convinient
    
}
//Eason: A91 use 200 & 1100mA VF table ---

//Eason: A91 get SWgauge percent now+++
/*get SWgauge percent by thermal tool, this result is not work after filter
*/
AXC_Gauge_A66 * SW_this;
int cal_SWgauge_capacity(void)
{
	int volt;
	int curr;
	int ocv_percent=0;

	SW_this->msParentGauge.ReadVoltCurr(
			&SW_this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);

#ifdef CONFIG_PM_8226_CHARGER
	if( (curr < -100) && (1==getIfonline()) && (false==pm8226_is_full()) )
#else
      if( (curr < -100) && (1==getIfonline()) && (false==smb346_IsFull()) )
#endif
	{
            printk("[BAT][vf][SWgauge]beforeVF: V:%d,C:%d\n",volt,curr);

#ifdef CONFIG_PM_8226_CHARGER
		 doAdcVfModify_A91(&volt,&curr);
#else
            doAdcVfModify(&volt,&curr);
#endif
            //ASUS_BSP Eason_Chang add event log +++
            printk("[BAT][vf][SWgauge]afterVF: V:%d,C:%d\n",volt,curr);
            //ASUS_BSP Eason_Chang add event log ---
	}

	ocv_percent = cal_ocv_percent_when_discharging(SW_this, volt, curr);

	return ocv_percent;
	
}
//Eason: A91 get SWgauge percent now---

/* cal_bat_capacity_work -
 */
 int RSOC=0;
static void cal_bat_capacity_work(struct work_struct *work)
{
	//int one_time_volt;
	//int one_time_curr;
	int volt;
	int curr;
	int ocv_percent=0;
    int online; ////Eason: Do VF with Cable when boot up

	AXC_Gauge_A66 *this = container_of(work, AXC_Gauge_A66, calBatCapacityWorker.work);

	// Read resistor when first time to cal battery capacity 
	if (!this->firstCalBatCapacity) {
		if (init_resistor_cali(this) < 0) {
			pr_info("[BAT][Gau][A66] cannot read bat resistor from file, maybe file system not ready, try again!\n");
			queue_delayed_work(this->gaugeQueue, &this->readBatResistorWorker , 8*HZ);
		}
			
		this->firstCalBatCapacity = true;
	}

	wake_lock(&this->calBatCapWlock);

    /*
	this->msParentGauge.ReadVoltCurr(
		&this->msParentGauge,
		1,
		&one_time_volt,
		&one_time_curr);

	printk("[BAT][Gau][A66]judge curr:%d\n",one_time_curr);
	*/

    //Eason always use V+IR  +++++++++//
	//if (one_time_curr > 0) {  // on discharging
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);

	printk("[BAT][Gau]:volt = %d curr = %d \n",volt,curr);
#ifdef CONFIG_PM_8226_CHARGER
		if( (curr < -100) && (1==getIfonline()) && (false==pm8226_is_full()) )
#else
        if( (curr < -100) && (1==getIfonline()) && (false==smb346_IsFull()) )
#endif
        {
            pr_debug("[BAT][vf]beforeVF: V:%d,C:%d\n",volt,curr);
#ifdef CONFIG_PM_8941_CHARGER
		doAdcVfModify_A91(&volt,&curr);
#else
            doAdcVfModify(&volt,&curr);
#endif
            pr_debug("[BAT][vf]afterVF: V:%d,C:%d\n",volt,curr);
            //ASUS_BSP Eason_Chang add event log +++
//            ASUSEvtlog("[BAT][vf]afterVF: V:%d,C:%d\n",volt,curr);
            //ASUS_BSP Eason_Chang add event log ---
        }
#ifdef CONFIG_PM_8226_CHARGER        
        else if((true == WhenBootUpDoVf) && (curr < -100) && (false==pm8226_is_full()))//Eason: Do VF with Cable when boot up+++
#else
		else if((true == WhenBootUpDoVf) && (curr < -100) && (false==smb346_IsFull()))//Eason: Do VF with Cable when boot up+++
#endif
        {
        	online = getIfonline();

        	if( 1==online )
            {
                printk("[BAT][vf]:Do VF with Cable when boot up\n");
                printk("[BAT][vf]beforeVF: V:%d,C:%d\n",volt,curr);
#ifdef CONFIG_PM_8941_CHARGER  				
		   doAdcVfModify_A91(&volt,&curr);
#else
		   doAdcVfModify(&volt,&curr);
#endif
                printk("[BAT][vf]afterVF: V:%d,C:%d\n",volt,curr);
            } 
        }//Eason: Do VF with Cable when boot up---

        WhenBootUpDoVf = false;//Eason: Do VF with Cable when boot up
    
        if (volt > 0){
    		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		RSOC=ocv_percent;
		pr_debug("[BAT][Gau][A66] %s(), discharging, ocv = %d\n", __func__, ocv_percent);
        }else{
            pr_err("[BAT][Gau][A66]:Error: volt not >0\n");
        }
	/*} else { // on charging
		this->gaugeCb->askSuspendCharging(this->gaugeCb);
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_by_volt_directly(volt);
		this->gaugeCb->askResumeCharging(this->gaugeCb);
		pr_debug("[BAT][Gau][A66] %s(), charging, ocv = %d\n", __func__, ocv_percent);		
	}
    */
    //Eason always use V+IR  --------//
	wake_unlock(&this->calBatCapWlock);

       //ASUS_BSP  +++ Eason_Chang "add BAT info time"
        ReportTime();
       //ASUS_BSP  --- Eason_Chang "add BAT info time"
	
    if (volt >0 ){
        this->ocvPercent = ocv_percent;
    	this->gaugeCb->onCapacityReply(&this->msParentGauge, this->gaugeCb, ocv_percent, BAT_CAP_REPLY_OK);
    }else{
        this->gaugeCb->onCapacityReply(&this->msParentGauge, this->gaugeCb, ocv_percent, BAT_CAP_REPLY_ERR);
    }
// TODO: need to add BAT_CAP_REPLY_ERR case

	return;
}

/* cal_bat_100_per_ocv_work -
 */
static void cal_bat_100_per_ocv_work(struct work_struct *work)
{
	int volt;
	int curr;

	AXC_Gauge_A66 *this = container_of(work, AXC_Gauge_A66, calBat100PerOCVWorker.work);

	wake_lock(&this->calBat100PerOCVWlock);

	this->msParentGauge.ReadVoltCurr(
		&this->msParentGauge,
		TIMES_FOR_GENERAL_READ_ADC,
		&volt,
		&curr);

	wake_unlock(&this->calBat100PerOCVWlock);

	if ( 0 == this->Per100OCV) {
		this->Per100OCV = volt;
		pr_info("[BAT][Gau][A66] first time update 100 per OCV: %d\n", this->Per100OCV);
	} else {
//TODO: average 100 percent OCV
		this->Per100OCV = (this->Per100OCV + volt)/2;
		pr_info("[BAT][Gau][A66] cycle update 100 per OCV: %d\n", this->Per100OCV);
	}

	return;
}

/* write_resistor_cali -
 */
static void write_resistor_cali(AXC_Gauge_A66 *this ,int lnResisterValue)
{
	struct file *cfile;
	mm_segment_t orgfs;
	char delta_t_str[4];

	mytemp lnResisterCalibrationData;
	lnResisterCalibrationData.mnValue = (s16)lnResisterValue;
	pr_info( "[BAT]Save R-Value:%d\n", lnResisterValue);

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	cfile = filp_open(BATTERY_RESISTOR_CALIBRATION_FILE_PATH, O_CREAT | O_RDWR | O_SYNC, 0666);
	if (!IS_ERR(cfile))
	{	
		delta_t_str[0]=lnResisterCalibrationData.mnChar[0];
		delta_t_str[1]=lnResisterCalibrationData.mnChar[1];
		cfile->f_op->write(cfile, delta_t_str, 2, &cfile->f_pos);
		if(filp_close(cfile, NULL))
       			pr_err("[BAT][Gau] fail to close resistor cali file in %s()\n", __func__);	
	}

	set_fs (orgfs);
	return;
}

/* cal_bat_resistor_work -
 */
static void cal_bat_resistor_work(struct work_struct *work)
{
	int volt, curr;
	int cnt = 0;

	AXC_Gauge_A66 *this = container_of(work, AXC_Gauge_A66, calBatResistorWorker.work);

	wake_lock(&this->calBatResistorWlock);
	// sometimes read current as 0 at the time of charger plug-out, so retry more times.
	do {
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge, 
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		cnt++;
		pr_info( "[BAT][Gau][A66]cal bat resistor, V=%d, C=%d, CNT=%d\n",volt, curr, cnt);
		msleep(1);
	} while ((curr <= 10) && (cnt < 15));

	wake_unlock(&this->calBatResistorWlock);

	if (this->Per100OCV < volt) {
		pr_err("[BAT][Gau][A66] error!! 100 per OCV less than volt \n");
		return;
	}

	if (curr <= 0) {
		pr_err("[BAT][Gau][A66] error!! unknown current \n");
		return;
	}

	this->resistorCali = (this->Per100OCV - volt)*1000/curr;
	pr_info( "[BAT][Gau][A66] cal resistor=%d\n", this->resistorCali);

	if (this->resistorCali > RESISTOR_MAX_VAL) {
		this->resistorCali = RESISTOR_MAX_VAL;
	} else if (this->resistorCali < RESISTOR_MIN_VAL) {
		this->resistorCali = RESISTOR_MIN_VAL;
	}

	pr_info("[BAT][Gau][A66] reset resistor cali:%d \n", this->resistorCali);

	write_resistor_cali(this, this->resistorCali);
	//TODO: Redesign calculate cali way, use average
	return;
}

/* read_bat_resistor_work - the work to read battery resistor from file */
static void read_bat_resistor_work(struct work_struct *work)
{
	AXC_Gauge_A66 *this = container_of(work, AXC_Gauge_A66, readBatResistorWorker.work);

	init_resistor_cali(this);

	return;		
}

/* AXC_Gauge_A66_AskCapacity -
 */
int AXC_Gauge_A66_AskCapacity(struct AXI_Gauge *gauge)
{
	AXC_Gauge_A66 *this = container_of(gauge, AXC_Gauge_A66, msParentGauge);
	BUG_ON(NULL == gauge);

	queue_delayed_work(this->calBatCapacityQueue, &this->calBatCapacityWorker , 0*HZ);

	return 0;
}

/* AXC_Gauge_A66_GetNextPollingInterval -
 */
int AXC_Gauge_A66_GetNextPollingInterval(struct AXI_Gauge *gauge)
{
	int interval;
	//AXC_Gauge_A66 *this = container_of(gauge, AXC_Gauge_A66, msParentGauge);//Eason repoert capacity to gauge for judge interval

	BUG_ON(NULL == gauge);

	if (ReportCapToGaugeForJudgeInterval() <= OCV_PER_SPEEDUP_UPDATE_14P)
		interval = UPDATE_INERVAL_WHEN_CAP_LESS_14P;
	else
		interval = UPDATE_INERVAL_DEFAULT;

	pr_debug("[BAT][Gau][A66] next polling interval:%d\n", interval);

	return interval;
}

/* AXC_Gauge_A66_NotifyCableInOut -
 */
int AXC_Gauge_A66_NotifyCableInOut(struct AXI_Gauge *gauge, bool hasCable)
{

	AXC_Gauge_A66 *this = container_of(gauge, AXC_Gauge_A66, msParentGauge);
	this->hasCable = hasCable;

	BUG_ON(NULL == gauge);

	pr_debug("[BAT][Gau][A66] notify calbe: %s \n", hasCable?"in":"out");
	pr_debug("[BAT][Gau][A66] isBatFull: %s \n", this->isBatFull?"yes":"no");

	if ((!this->hasCable) && (this->isBatFull)) {	// cable out
		pr_debug("[BAT][Gau][A66] calBatResistorWorker \n");
		queue_delayed_work(this->gaugeQueue, &this->calBatResistorWorker , 1.5*HZ);
	}

	return 0;
}

/* AXC_Gauge_A66_NotifyBatFullChanged -
 */
int AXC_Gauge_A66_NotifyBatFullChanged(struct AXI_Gauge *gauge, bool isFull)
{
	AXC_Gauge_A66 *this = container_of(gauge, AXC_Gauge_A66, msParentGauge);

	BUG_ON(NULL == gauge);

	if (isFull == this->isBatFull) {
		pr_info("[BAT][Gau][A66] %s(), same, need not to be changed \n", __FUNCTION__);
		return -1;
	}

	this->isBatFull = isFull;

	if (this->isBatFull) {
		queue_delayed_work(this->gaugeQueue, &this->calBat100PerOCVWorker , 0*HZ);		
		pr_debug("[BAT][Gau][A66] notify bat full\n");
	} else {
		pr_debug("[BAT][Gau][A66] notify bat changed to not full \n");
	}

	return 0;
}

/* AXC_Gauge_A66_IsBatExisted -
 */
bool AXC_Gauge_A66_IsBatExisted(struct AXI_Gauge *gauge)
{
	return true;
}

//Eason takeoff ADC read max & min +++
int findMax(int* Array)
{
   int Max;
   int i;
   int MaxIndex = 0;
  
   Max = Array[0];

   for(i=1;i<TIMES_FOR_GENERAL_READ_ADC;i++)
   {
	if(Array[i] > Max)
	{
		Max = Array[i];
                MaxIndex = i;
	}
   }
   
   return MaxIndex;
}

int findMin(int* Array)
{
   int Min;
   int i;
   int MinIndex = TIMES_FOR_GENERAL_READ_ADC-1;
  
   Min = Array[TIMES_FOR_GENERAL_READ_ADC-1];

   for(i=TIMES_FOR_GENERAL_READ_ADC-2; i>=0 ;i--)
   {
	if(Array[i] < Min)
	{
		Min = Array[i];
                MinIndex = i;
	}
   }
   
   return MinIndex;
}
//Eason takeoff ADC read max & min ---

//extern void asus_pet_watchdog(void);
/* AXC_Gauge_A66_ReadVoltCurrWithoutCali - To read voltage and current without
 * calibration for several times.
 */
static void AXC_Gauge_A66_ReadVoltCurrWithoutCali(
	AXI_Gauge *apGauge,
	unsigned int anTimes,
	int *volt,
	int *curr)
{
	int i = 0;
    int j = 0;
	int tmp_volt;
	int tmp_curr;
	int volt_sum = 0;
	int curr_sum = 0;
	bool curr_nega = false;
	int volArray[anTimes]; //Eason print VC
	int currArray[anTimes]; //Eason print VC
	//Eason takeoff ADC read max & min +++
	int volMax = 0;
    int volMin = TIMES_FOR_GENERAL_READ_ADC-1;
    int currMax = 0;
    int currMin = TIMES_FOR_GENERAL_READ_ADC-1;
	//Eason takeoff ADC read max & min ---
	//Eason ADC read 15times+++
	int adcPrint ;
	//Eason ADC read 15times---

	BUG_ON(anTimes <= 0);
	BUG_ON(NULL == volt);
	BUG_ON(NULL == curr);

	while (i < anTimes)
	{
		AXC_Gauge_A66_ReadVoltageCurrentFromADC(apGauge, &tmp_volt, &tmp_curr);
		if ((tmp_volt < 2700)&&(j < 3)) {
			pr_err("[BAT][Gau][A66] read error volt: %d, try again \n", tmp_volt);
			j++;

//			asus_pet_watchdog();

			msleep(2);            
			continue;
		}

        if (j >= 3) {
            *volt = -1;
            *curr = -1;
            pr_err("[BAT][Gau][A66] read error over 3, return v,c as -1\n");
            return;
        }

		volt_sum += tmp_volt;
		curr_sum += tmp_curr;
		volArray[i] = tmp_volt; //Eason print VC
		currArray[i] = tmp_curr; //Eason print VC
		++i;
		msleep(2);
	}

    //Eason takeoff ADC read max & min +++
    volMax = findMax(volArray);
    volMin = findMin(volArray);
    currMax = findMax(currArray);
    currMin = findMin(currArray);
    
    volt_sum = volt_sum - volArray[volMax] - volArray[volMin];   
    curr_sum = curr_sum - currArray[currMax] - currArray[currMin]; 
    //Eason takeoff ADC read max & min ---

	if(curr_sum < 0){
		curr_nega = true ;
		curr_sum = curr_sum * (-1);
	}else{
		curr_nega = false;
	}

	*volt = volt_sum/(anTimes-2);//Eason takeoff ADC read max & min
	*curr = curr_sum/(anTimes-2);//Eason takeoff ADC read max & min

	if(true == curr_nega){
		*curr = (*curr)*(-1);
	}    

    //ASUS BSP Eason filter adc read current <= 15mA when doesn't has cable +++
    if( (*curr >= 0)&&(*curr <= 15)&&(false == report_BatteryService_If_HasCable()) )
    {
            printk("[BAT][Gau][A66] read error current<=15mA without cable:%d\n",*curr);
    	    *volt = -1;
            *curr = -1;
            pr_err("[BAT][Gau][A66] read error current<=15mA without cable, return v,c as -1\n");
            return;
    }
    //ASUS BSP Eason filter adc read current <= 15mA when doesn't has cable ---
    

	//Eason print VC +++
	/*for(i = 0; i < anTimes; i++){ 
		printk("%d,%d",volArray[i],currArray[i]);
	}*/
    //Eason takeoff ADC read max & min+++    
	if(15==anTimes){
		//Eason ADC read 15times+++
		
		/*printk("[BAT][Gau][A66]:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",volArray[0],volArray[1],volArray[2],volArray[3],volArray[anTimes-1],
			currArray[0],currArray[1],currArray[2],currArray[3],currArray[anTimes-1]);*/
		pr_debug("[BAT][Gau][A66]:");
		for(adcPrint=0;adcPrint<15;adcPrint++)
		{
			pr_debug("%d,",volArray[adcPrint]);
		}
		for(adcPrint=0;adcPrint<15;adcPrint++)
		{
			pr_debug("%d,",currArray[adcPrint]);
		}
		pr_debug("\n");
		//Eason ADC read 15times---
		
		printk("[BAT][Gau][A66]:Vmax:%d,Vmin:%d,avgV:%d,Cmax:%d,Cmin:%d,avgC:%d\n",volArray[volMax],volArray[volMin],*volt,
            currArray[currMax],currArray[currMin],*curr);
        //ASUS_BSP Eason_Chang add event log +++
//        ASUSEvtlog("[BAT][Gau][A66]:Vmax:%d,Vmin:%d,avgV:%d,Cmax:%d,Cmin:%d,avgC:%d\n",volArray[volMax],volArray[volMin],*volt,
//            currArray[currMax],currArray[currMin],*curr);
        //ASUS_BSP Eason_Chang add event log ---
	}
    //Eason takeoff ADC read max & min---
	//Eason print VC ---

   //Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status +++ 
   g_adc_get_correct_VolCurr = true;
   //Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status ---    

	return;
}


/* AXC_Gauge_A66_ReadVoltCurr - To read voltage and current with
 * calibration for several times.
 */
bool IsFirstReadVoltCurr =true;
extern void Smb346_Enable_Charging(bool enabled);
extern void pm8226_chg_enable_charging(bool enable);
static void AXC_Gauge_A66_ReadVoltCurr(
	AXI_Gauge *apGauge,
	unsigned int anTimes,
	int *volt,
	int *curr)
{
	/*AXC_Gauge_A66 *this = container_of(
		apGauge,
		AXC_Gauge_A66,
		msParentGauge);
    */
	int i=0;
	int j=0;
	int voltage_test[10][2];
	int current_test[10][2];
	int voltage_temp=0;
	int current_temp=0;
	if( IsFirstReadVoltCurr ==true)
	{
		printk("[BAT][Gau] enter AXC_Gauge_A66_ReadVoltCurr IsFirstReadVoltCurr\n");
		IsFirstReadVoltCurr =false;

#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
		if(!g_CHG_mode){
#ifdef CONFIG_PM_8226_CHARGER
			pm8226_chg_enable_charging(false);
			pm8226_chg_usb_suspend_enable(true);
#else
			Smb346_Enable_Charging(false);
			smb346_suspend_mode(true);
#endif
			msleep(500);
		}
#endif

		for(i=0;i<10;i++)
		{
			AXC_Gauge_A66_ReadVoltCurrWithoutCali(apGauge, anTimes, volt, curr);
			voltage_test[i][0]=*volt;
			current_test[i][0]=*curr;
			AXC_Gauge_A66_ReadVoltCurrWithoutCali(apGauge, anTimes, volt, curr);
			voltage_test[i][1]=*volt;
			current_test[i][1]=*curr;
			if (abs(voltage_test[i][0]-voltage_test[i][1])<=5)
				{
					break;
				}
		}
		if(i==10)
		{
			printk("[BAT][Gau] i =10\n");
			for(j=0;j<10;j++)
				{
				voltage_temp +=voltage_test[j][0]+voltage_test[j][1];
				current_temp +=current_test[j][0]+current_test[j][1];
				}
			*volt = voltage_temp/20;
			*curr = current_temp/20;
		}
		printk("[BAT][Gau] AXC_Gauge_A66_ReadVoltCurr volt = %d , curr = %d \n",*volt,*curr);

#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
		if(!g_CHG_mode){
#ifdef CONFIG_PM_8226_CHARGER
			pm8226_chg_enable_charging(true);
			pm8226_chg_usb_suspend_enable(false);
#else
			Smb346_Enable_Charging(true);
			smb346_suspend_mode(false);
#endif
		}
#endif

	}
	else
	{
		AXC_Gauge_A66_ReadVoltCurrWithoutCali(apGauge, anTimes, volt, curr);
	}
	//*volt += this->mnVoltageCalibration;
	//*curr += this->mnCurrentCalibration;

	return;
}


int AXC_Gauge_A66_GetType(AXI_Gauge *apGauge)
{
    AXC_Gauge_A66 *this = container_of(apGauge, AXC_Gauge_A66, msParentGauge);
    return this->mnType;
}

void AXC_Gauge_A66_SetType(AXI_Gauge *apGauge ,int anType)
{
    AXC_Gauge_A66 *this = container_of(apGauge, AXC_Gauge_A66, msParentGauge);
    this->mnType = anType;
}

void AXC_Gauge_A66_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb)
{
	AXC_Gauge_A66 *this = container_of(apGauge, AXC_Gauge_A66, msParentGauge);

	//Eason: A91 get SWgauge percent now+++
	SW_this = this;
	//Eason: A91 get SWgauge percent now---

	BUG_ON((NULL == gaugeCb) || (NULL == apGauge));

//ASUS_BSP Eason read battery ID+++
#ifdef ASUS_ME175KG_PROJECT
	CurrOCVtoPerTbl = ME175KG_SrcOCVtoPerTbl;
#else
	if(g_BatteryID_value<1100000){
		CurrOCVtoPerTbl = NVT_SrcOCVtoPerTbl;
	}
	else{
		CurrOCVtoPerTbl = TWS_SrcOCVtoPerTbl;
	}
#endif
//ASUS_BSP Eason read battery ID---    

	this->gaugeCb = gaugeCb;

	wake_lock_init(&this->calBatCapWlock, WAKE_LOCK_SUSPEND, "calBatCapWlock");
	wake_lock_init(&this->calBat100PerOCVWlock, WAKE_LOCK_SUSPEND, "calBat100PerOCVWlock");
	wake_lock_init(&this->calBatResistorWlock, WAKE_LOCK_SUSPEND, "calBatResistorWlock");

	INIT_DELAYED_WORK(&this->calBatCapacityWorker, cal_bat_capacity_work) ;
	INIT_DELAYED_WORK(&this->calBat100PerOCVWorker, cal_bat_100_per_ocv_work) ;
	INIT_DELAYED_WORK(&this->calBatResistorWorker, cal_bat_resistor_work) ;
	INIT_DELAYED_WORK(&this->readBatResistorWorker, read_bat_resistor_work) ;

	this->calBatCapacityQueue = create_singlethread_workqueue("CalBatCapacityWorker");
	if (!this->calBatCapacityQueue)
		pr_err("[BAT][Gau][A66]error!! when create calBatCapacityQueue\n");

	this->gaugeQueue = create_singlethread_workqueue("GaugeWorker");	
	if (!this->gaugeQueue)
		pr_err("[BAT][Gau][A66]error!! when create gaugeQueue\n");

	this->msParentGauge.askCapacity = AXC_Gauge_A66_AskCapacity;
	this->msParentGauge.getNextPollingInterval = AXC_Gauge_A66_GetNextPollingInterval;
	this->msParentGauge.notifyCableInOut = AXC_Gauge_A66_NotifyCableInOut;
	this->msParentGauge.notifyBatFullChanged = AXC_Gauge_A66_NotifyBatFullChanged;
	this->msParentGauge.isBatExisted = AXC_Gauge_A66_IsBatExisted;
	this->msParentGauge.SetType = AXC_Gauge_A66_SetType;
	this->msParentGauge.GetType = AXC_Gauge_A66_GetType;
	this->msParentGauge.ReadVoltCurr = AXC_Gauge_A66_ReadVoltCurr;
	this->msParentGauge.ReadVoltCurrWithoutCali = AXC_Gauge_A66_ReadVoltCurrWithoutCali;

	this->mbInited = true;
}


