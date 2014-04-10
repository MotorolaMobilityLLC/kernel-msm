/*
	SW Gauge Implementation

*/
#include <linux/jiffies.h>
#include "axc_swgauge.h"
#include "../charger/axc_chargerfactory.h"
#include "sampledata/axc_sampledatafactory.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
///Eason_Chang///#include <linux/mfd/88pm860x.h>
///Eason_Chang///#include <linux/mfd/asus-88pm860x-power.h>
//#include "../asus_bat_dbg.h"
#include <linux/asus_bat.h>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/sched.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/leds.h>

#include "../asus_battery.h"

#include "axc_gaugefactory.h"

//ASUS_BSP  +++ Eason_Chang "add BAT info time"
#include <linux/rtc.h>
//ASUS_BSP  --- Eason_Chang "add BAT info time"

//Inline for AXC_SWGauge
#define AXC_SWGauge_CalculateRealVoltageFromADC(x)  (x*675/512)         //1.8/4096*1000(bias mv)*3
#define AXC_SWGauge_CalculateRealCurrentFromADC(x)  (x*375/512)         //1.8/4096*1000*10/6(mv)

#define AXC_SWGauge_CalBatIdFromADC(x)  (x*225/512)         //1.8/4096*1000(bias mv)

#define AXC_SWGauge_CalculateRealRntcFromADC(x)     (150*x/(4096-x))    //(1.8/4096)*x = 1.8*Rntc/(150K+Rntc)
#define INVERSE_OF_DEVICE_RESISTOR_VALUE (4)    //0.25, Avoid float problem of Linux, so use the inverse instead
#define DEFAULT_DEVICE_RESISTOR_VALUE (150) 
#define BATTERY_PERCENTAGE_IN_VALID (-1)
#define RESISTOR_MAX_VAL (250)
#define RESISTOR_MIN_VAL (150)


#define FAKE_VOLTAGE_CALIBRATION (-20)
#define FAKE_CURRENT_CALIBRATION (-40)
#define OMS_BATTERY_CALIBRATION_FILE_PATH    "/opl/telephony/Linux/Marvell/NVM/Battcali.nvm"
#define ANDROID_BATTERY_CALIBRATION_FILE_PATH    "/marvell/Linux/Marvell/NVM/Battcali.nvm"
#define BATTERY_CALIBRATION_FILE_PATH    "/data/.NVM/Battcali.nvm"

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
#define BATTERY_RESISTOR_CALIBRATION_FILE_PATH    "/data/data/BattResicali.nvm"
//ASUS_BSP --- Josh_Liao "sw gauge v2"

// joshtest

/* TBL_SIZE - different table size */
#define OCV_TBL_SIZE 101
#define VF_TBL_SIZE 101
#define FASTEST_CHARGING_TBL_SIZE 101
#define FIVE_PERCENT_TBL_SIZE 101

/* BAT_LIFE - different battery life definition */
#define BAT_LIFE_TO_SHUTDOWN 0
#define BAT_LIFE_UNIT 5
#define BAT_LIFE_TWO  2
#define BAT_LIFE_FULL 100
#define BAT_LIFE_BEFORE_FULL 99
#define BAT_LIFE_BEFORE_FULL_ONE_UNIT 83

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
/*  OCV_PER - ocv percent */
#define OCV_PER_SPEEDUP_UPDATE 12
//ASUS_BSP --- Josh_Liao "sw gauge v2"

/* UPDATE_INERVAL - different interval for different case */
#define UPDATE_INERVAL_NO_CHARGER_TYPE  120
#define UPDATE_INERVAL_LOW_CURRENT_CHARGER_TYPE      300
#define UPDATE_INERVAL_HIGH_CURRENT_CHARGER_TYPE       300 
#define UPDATE_INERVAL_DEFAULT          180
#define UPDATE_INERVAL_ACCELERATE       120
#define UPDATE_INERVAL_BAT_LOW_IRQ       60


/* Definition for usage of save last battery info */
#define LAST_BAT_INFO_FILE_PATH    "/data/.NVM/last_bat_info_file"
#define INT_TO_FILE_LENGTH 2
#define BAT_INFO_LEN 14

///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#define PM_8921_CHARGER
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

/* ACCELERATE_UPDATE_INERVAL_BAT_LIFE - Accelerate update interval to monitor
 * battery life for shutdown reporting in real time. 
 */
#define ACCELERATE_UPDATE_INERVAL_BAT_LIFE 5

/* Times to read adc(volt, curr) for each case. */
#define TIMES_FOR_FIRST_READ_ADC     8//80
#define TIMES_FOR_DISCHG_READ_ADC    5//50
#define TIMES_FOR_GENERAL_READ_ADC   5//50
#define TIMES_FOR_RESUME_READ_ADC    8//80

#define INTERVAL_TO_WAIT_VF  60*HZ

#define BAT_LOW_OCV			 3540
// joshtest

static void AXC_SWGauge_ChangeBatteryUpdatedInterval(AXC_SWGauge *this);
//eason//static void AXC_SWGauge_ReadAndUpdateBatteryInfo(AXC_SWGauge *this);
static void AXC_SWGauge_InitVoltageCurrnetCalibrationData(AXC_SWGauge *this);
static void AXC_SWGauge_InitResistorCalibrationData(AXC_SWGauge *this);

// joshtest
static void update_bat_info(struct AXC_SWGauge *this, enum swgauge_bat_update_state set_state);

/* asus_td_get_boot_mode - Defined in ttc_dkb.c , used to check bootup mode.
 */
extern int asus_td_get_boot_mode(void);

/* Defined in axc_Smb346Charger.c */
extern void chg_get_charger_update_time(struct timespec *charger_update_time);

/* Defined in asus_battery.c */
extern void asus_bat_enable_vibrator(void);

/* Defined in asus_battery.c */
extern bool asus_bat_meet_bat_life_interval_for_vibration(void);

/* Defined in asus_battery.c */
extern bool asus_bat_config_vibrator_debug(void);

// joshtest

///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#define EASON_CHANGE
extern int get_temp_for_ASUSswgauge(void);
extern int get_voltage_for_ASUSswgauge(void);
extern int get_current_for_ASUSswgauge(void);
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

typedef union mytemp mytemp;

union mytemp{
	s16 mnValue;
	char mnChar[2];
};

/* CurrOCVtoPerTbl - to determine which ocv to percent table used */
const int *CurrOCVtoPerTbl = NULL;

/**
 * Main Source OCV Table for TD2. Plz refer to "SBP-23 sample A.xls". T20 is the 
 * same with SBP.
 */
#ifdef CONFIG_PROJECT_TD2
const int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
/* 	X0,   X1,   X2,   X3,   X4,   X5,   X6,   X7,   X8,   X9,  */
  3400, 3548, 3608, 3653, 3670, 3675, 3678, 3680, 3681, 3682,  /*  0 to  9% */
  3683, 3684, 3686, 3694, 3705, 3712, 3720, 3727, 3733, 3738,  /* 10 to 19% */
  3741, 3745, 3749, 3753, 3758, 3760, 3762, 3763, 3764, 3765,  /* 20 to 29% */
  3766, 3767, 3768, 3770, 3771, 3773, 3774, 3776, 3778, 3780,  /* 30 to 39% */
  3782, 3785, 3787, 3789, 3792, 3794, 3797, 3800, 3803, 3806,  /* 40 to 49% */    
  3809, 3813, 3817, 3821, 3826, 3832, 3840, 3849, 3858, 3866,  /* 50 to 59% */
  3874, 3881, 3888, 3894, 3900, 3906, 3912, 3917, 3923, 3930,  /* 60 to 69% */
  3935, 3942, 3948, 3953, 3958, 3963, 3968, 3974, 3979, 3986,  /* 70 to 79% */
  3994, 4004, 4017, 4031, 4043, 4053, 4060, 4065, 4070, 4078,  /* 80 to 89% */
  4086, 4095, 4104, 4113, 4122, 4132, 4141, 4151, 4161, 4171,  /* 90 to 99% */
  4189                                                         /* 100% */ 
};

/**
 * Second Source OCV Table for TD2. Plz refer to "T20 pack 0.01C Discharge-1.xls"
 */
const int SecondSrcOCVtoPerTbl[OCV_TBL_SIZE]={
/* 	X0,   X1,   X2,   X3,   X4,   X5,   X6,   X7,   X8,   X9,  */
  3400, 3601, 3655, 3672, 3677, 3679, 3680, 3681, 3683, 3684,  /*  0 to  9% */
  3689, 3700, 3708, 3715, 3724, 3729, 3734, 3737, 3741, 3745,  /* 10 to 19% */
  3748, 3752, 3755, 3757, 3759, 3761, 3765, 3767, 3770, 3771,  /* 20 to 29% */
  3772, 3774, 3776, 3777, 3779, 3781, 3783, 3785, 3787, 3789,  /* 30 to 39% */
  3791, 3793, 3796, 3800, 3803, 3808, 3811, 3815, 3820, 3824,  /* 40 to 49% */    
  3828, 3832, 3837, 3841, 3845, 3850, 3854, 3859, 3863, 3867,  /* 50 to 59% */
  3871, 3876, 3881, 3888, 3895, 3902, 3911, 3918, 3925, 3933,  /* 60 to 69% */
  3941, 3949, 3957, 3965, 3973, 3981, 3987, 3993, 3999, 4005,  /* 70 to 79% */
  4011, 4019, 4028, 4036, 4044, 4051, 4059, 4067, 4075, 4083,  /* 80 to 89% */
  4092, 4101, 4111, 4121, 4131, 4141, 4148, 4153, 4161, 4173,  /* 90 to 99% */
  4185                                                         /* 100% */ 
};


/**
 * VF Table for TD2. Created by charging with ac (900mA).
 */
const int vf_to_percent_tbl[VF_TBL_SIZE]={
/* X0,  X1,  X2,  X3,  X4,  X5,  X6,  X7,  X8,  X9,  */
   74,  79, 236, 248, 264, 270, 271, 272, 275, 277,  /*  0 to  9% */
  280, 285, 287, 284, 277, 274, 270, 272, 269, 267,  /* 10 to 19% */
  267, 266, 266, 264, 261, 263, 263, 263, 264, 266,  /* 20 to 29% */
  266, 267, 268, 268, 268, 269, 269, 269, 270, 269,  /* 30 to 39% */
  270, 268, 268, 267, 266, 265, 264, 264, 262, 262,  /* 40 to 49% */    
  261, 260, 258, 257, 253, 251, 244, 239, 231, 227,  /* 50 to 59% */
  221, 217, 212, 209, 205, 203, 200, 197, 195, 189,  /* 60 to 69% */
  186, 182, 181, 178, 177, 175, 174, 170, 168, 165,  /* 70 to 79% */
  160, 152, 141, 128, 117, 110, 105, 103, 101,  96,  /* 80 to 89% */
   90,  83,  75,  68,  60,  51,  44,  35,   2,  18,  /* 90 to 99% */
    0                                                /* 100% */ 
};

/**
 * The Fastest Charging Time Table for TD2. Created by full charging with ac
 * (900mA). The table is the mapping of unit and minute. Ex. If charging 10 minutes, the
 * maxmium battery life is 9%.
 */
///Eason_Chang///const int fastest_charging_time_tbl[FASTEST_CHARGING_TBL_SIZE]={
int fastest_charging_time_tbl[FASTEST_CHARGING_TBL_SIZE]={
/* X0,  X1,  X2,  X3,  X4,  X5,  X6,  X7,  X8,  X9,  */
    0,   2,   3,   4,   5,   6,   7,   8,   9,  10,  /*  0 to  9% */
   12,  13,  14,  15,  16,  17,  18,  19,  20,  22,  /* 10 to 19% */
   23,  24,  25,  26,  27,  28,  29,  30,  32,  33,  /* 20 to 29% */
   34,  35,  36,  37,  38,  39,  40,  42,  43,  44,  /* 30 to 39% */
   45,  46,  47,  48,  49,  50,  52,  53,  54,  55,  /* 40 to 49% */    
   56,  57,  58,  59,  61,  62,  63,  64,  65,  67,  /* 50 to 59% */
   68,  69,  71,  72,  73,  75,  76,  77,  79,  80,  /* 60 to 69% */
   82,  83,  85,  87,  88,  90,  92,  94,  96,  98,  /* 70 to 79% */
  100, 102, 104, 106, 108, 111, 114, 117, 120, 124,  /* 80 to 89% */
  128, 132, 137, 142, 147, 153, 160, 168, 178, 190,  /* 90 to 99% */
  197                                                /* 100% */ 
};


/**
 * The five percent table for TD2.
 */
const int five_percent_tbl[FIVE_PERCENT_TBL_SIZE]={
/* X0,  X1,  X2,  X3,  X4,  X5,  X6,  X7,  X8,  X9,  */
    0,   1,   1,   1,   1,   5,   5,   5,   5,   5,  /*  0 to  9% */
   10,  10,  10,  10,  10,  15,  15,  15,  15,  15,  /* 10 to 19% */
   20,  20,  20,  20,  20,  25,  25,  25,  25,  25,  /* 20 to 29% */
   30,  30,  30,  30,  30,  35,  35,  35,  35,  35,  /* 30 to 39% */
   40,  40,  40,  40,  40,  45,  45,  45,  45,  45,  /* 40 to 49% */    
   50,  50,  50,  50,  50,  55,  55,  55,  55,  55,  /* 50 to 59% */
   60,  60,  60,  60,  60,  65,  65,  65,  65,  65,  /* 60 to 69% */
   70,  70,  70,  70,  70,  75,  75,  75,  75,  75,  /* 70 to 79% */
   80,  80,  80,  80,  80,  85,  85,  85,  85,  85,  /* 80 to 89% */
   90,  90,  90,  90,  95,  95,  95,  95,  95,  99,  /* 90 to 99% */
  100                                                /* 100% */ 
};

#elif defined CONFIG_PROJECT_TD3
int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
    3400, 3496, 3563, 3614, 3657, 3670, 3671, 3673, 3676, 3677,
    3678, 3682, 3691, 3699, 3707, 3715, 3721, 3727, 3732, 3736,
    3738, 3744, 3748, 3750, 3753, 3756, 3757, 3761, 3765, 3767,
    3768, 3770, 3772, 3773, 3775, 3778, 3780, 3781, 3785, 3787,
    3789, 3791, 3794, 3795, 3798, 3802, 3804, 3807, 3811, 3814,
    
	3816, 3820, 3828, 3831, 3837, 3844, 3851, 3860, 3873, 3886,
	3889, 3894, 3900, 3903, 3911, 3917, 3922, 3926, 3931, 3939,
	3944, 3951, 3957, 3964, 3971, 3980, 3986, 3993, 3999, 4011,
	4018, 4027, 4039, 4044, 4053, 4059, 4070, 4077, 4084, 4095,
	4101, 4111, 4120, 4127, 4137, 4145, 4156, 4164, 4174, 4185,
	4197
};
#elif defined PM_8921_CHARGER
///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
/*
int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
    3400, 3422, 3443, 3463, 3483, 3502, 3520, 3537, 3553, 3567,
    3581, 3593, 3605, 3615, 3626, 3636, 3643, 3655, 3666, 3673,
    3678, 3684, 3689, 3695, 3702, 3709, 3715, 3722, 3728, 3734,
    3739, 3744, 3750, 3755, 3761, 3767, 3772, 3777, 3783, 3788,
    3793, 3799, 3804, 3810, 3816, 3821, 3826, 3832, 3838, 3844,
   
    3850, 3857, 3864, 3872, 3881, 3888, 3894, 3901, 3907, 3913,
    3918, 3925, 3931, 3937, 3943, 3950, 3956, 3962, 3968, 3974,
    3979, 3985, 3991, 3999, 4005, 4013, 4020, 4029, 4037, 4044, 
    4050, 4057, 4063, 4069, 4074, 4081, 4087, 4094, 4101, 4107, 
    4115, 4121, 4128, 4135, 4142, 4151, 4158, 4166, 4173, 4181,
    4192 
};
*/
/*
int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
    3400, 3438, 3473, 3507, 3537, 3563, 3586, 3606, 3624, 3639,	
    3654, 3671, 3677, 3679, 3681, 3682, 3683, 3684, 3685, 3687,
    3695, 3704, 3711, 3718, 3726, 3732, 3736, 3740, 3743, 3747,
    3752, 3755, 3758, 3761, 3764, 3767, 3769, 3772, 3774, 3776,
    3778, 3780, 3782, 3785, 3787, 3790, 3792, 3795, 3798, 3801, 

    3804, 3807, 3811, 3814, 3818, 3821, 3825, 3830, 3834, 3839,
    3844, 3850, 3858, 3868, 3880, 3890, 3898, 3905, 3912, 3918, 
    3924, 3930, 3936, 3942, 3948, 3954, 3959, 3964, 3968, 3973,
    3979, 3985, 3992, 4000, 4010, 4022, 4034, 4045, 4055, 4062, 
    4067, 4073, 4080, 4088, 4097, 4105, 4114, 4123, 4132, 4141,
    4151	
};
*/
int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
    3400, 3423, 3458, 3492, 3522, 3548, 3571, 3591, 3609, 3624,
    3639, 3656, 3662, 3664, 3666, 3667, 3668, 3669, 3670, 3672,
    3680, 3689, 3696, 3703, 3711, 3717, 3721, 3725, 3728, 3732, 
    3737, 3740, 3743, 3746, 3749, 3752, 3754, 3757, 3759, 3761, 
    3763, 3765, 3767, 3770, 3772, 3775, 3777, 3780, 3783, 3786,
  
    3789, 3792, 3796, 3799, 3803, 3806, 3810, 3815, 3819, 3824, 
    3829, 3835, 3843, 3853, 3865, 3875, 3883, 3890, 3897, 3903,
    3909, 3915, 3921, 3927, 3933, 3939, 3944, 3949, 3953, 3958, 
    3964, 3970, 3977, 3985, 3995, 4007, 4019, 4030, 4040, 4047, 
    4052, 4058, 4065, 4073, 4082, 4090, 4099, 4108, 4117, 4126, 
    4136      
};
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

#else 
// do other job
int MainSrcOCVtoPerTbl[OCV_TBL_SIZE]={
//    3025, 3236, 3383, 3475, 3546, 3594, 3631, 3657, 3665, 3669,
    3400, 3440, 3480, 3520, 3560, 3600, 3631, 3657, 3665, 3669,
//	3671, 3673, 3674, 3675, 3675, 3676, 3679, 3689, 3699, 3704, 
//	3712, 3720, 3724, 3728, 3731, 3734, 3738, 3742, 3745, 3748, 
//	3750, 3753, 3755, 3759, 3762, 3764, 3766, 3767, 3769, 3771, 
//	3773, 3775, 3776, 3778, 3780, 3782, 3787, 3787, 3789, 3791, 
//Average 10%~50%
    3671, 3674, 3677, 3680, 3683, 3686, 3689, 3692, 3695, 3698,
    3701, 3704, 3707, 3710, 3713, 3716, 3719, 3722, 3725, 3728,
    3731, 3734, 3737, 3740, 3743, 3746, 3749, 3752, 3755, 3758,
    3761, 3764, 3767, 3770, 3773, 3776, 3779, 3782, 3785, 3791,
    
	3794, 3797, 3800, 3804, 3807, 3811, 3815, 3819, 3824, 3829, 
	3837, 3846, 3857, 3867, 3876, 3883, 3890, 3896, 3903, 3909, 
	3915, 3921, 3927, 3933, 3940, 3947, 3954, 3960, 3967, 3974, 
	3981, 3988, 3996, 4004, 4013, 4023, 4031, 4039, 4048, 4056, 
	4063, 4072, 4081, 4090, 4099, 4108, 4118, 4129, 4151, 4165,
	4181
};
#endif /* CONFIG_PROJECT_TD2 */

/* ASUS_BSP +++ Josh_Liao "Battery porting" */
static inline int asus_pmic_bulk_read(int reg, int count, unsigned char *buf)
{
	///Eason_Chang///return pm860x_codec_bulk_read(reg, count, buf);
	return 0;
}

static inline int asus_pmic_bulk_write(int reg, int count, unsigned char *buf)
{
	////Eason_Chang///return pm860x_codec_bulk_write(reg, count, buf);
	return 0;
}

/* asus_pmic_reg_read - basic function to read battery related info with 1byte
 */
static inline void asus_pmic_reg_read(int reg, int *pval)
{
	///Eason_Chang///*pval = pm860x_codec_reg_read(reg);
//  *pval = pm860x_reg_read(i2c, reg);
}

/* asus_pmic_reg_write - basic function to write battery related info with 1byte
 */
static inline void asus_pmic_reg_write(int reg, int val)
{
	///Eason_Chang///pm860x_codec_reg_write(reg, val);
//  pm860x_reg_write(i2c, reg, val);
}
/* ASUS_BSP --- Josh_Liao "Battery porting" */

//Private function for AXC_SWGauge
static int LinearSearch(int target,const int *array, int array_size)
{
	int i = 0;
	int target_index = -1;

	assert(NULL != array);

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
#if 0 ///Eason_Chang///
static int swg_search_percent_from_tbl(int val, int *tbl, int tbl_size)
{
    int target_index = 0;

    target_index = LinearSearch(val, tbl, tbl_size);
    if (target_index == -1) {
    	printk(DBGMSK_BAT_ERR "[BAT][Gau] something wrong of val:%d to percent\n", val);
    } else {
		pr_debug( "[BAT][Gau] ocv per index:%d\n", target_index);
	}

    return target_index;	
}
static void AXC_SWGauge_EnableBatIdGADC(bool abEnabled)
{
  int tmp;

  if(true == abEnabled) {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp | PM8607_MEAS_EN1_GPADC0);
    printk(DBGMSK_BAT_G2 "[BAT]tmp | PM8607_MEAS_EN1_GPADC0:%d\n", tmp | PM8607_MEAS_EN1_GPADC0);
  } else {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp & (~PM8607_MEAS_EN1_GPADC0));
    printk(DBGMSK_BAT_G2 "[BAT]tmp & (~PM8607_MEAS_EN1_GPADC0:%d\n", tmp & (~PM8607_MEAS_EN1_GPADC0));
  }
}
#endif  ///Eason_Chang///
#if 0 ///Eason_Chang///
static void AXC_SWGauge_EnableVoltageGADC(bool abEnabled)
{
  int tmp;

  if(true == abEnabled) {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp | PM8607_MEAS_EN1_GPADC3);
    printk(DBGMSK_BAT_G2 "[BAT]tmp | PM8607_MEAS_EN1_GPADC3:%d\n", tmp | PM8607_MEAS_EN1_GPADC3);
  } else {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp & (~PM8607_MEAS_EN1_GPADC3));
    printk(DBGMSK_BAT_G2 "[BAT]tmp & (~PM8607_MEAS_EN1_GPADC3:%d\n", tmp & (~PM8607_MEAS_EN1_GPADC3));
  }
}
#endif  ///Eason_Chang///
#if 0 ///Eason_Chang///
static void AXC_SWGauge_EnableCurrentGADC(bool abEnabled)
{
  int tmp;

  if(true == abEnabled) {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp | PM8607_MEAS_EN1_GPADC2);
    printk(DBGMSK_BAT_G2 "[BAT]tmp | PM8607_MEAS_EN1_GPADC2:%d\n", tmp | PM8607_MEAS_EN1_GPADC2);
  } else {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp & (~PM8607_MEAS_EN1_GPADC2));
    printk(DBGMSK_BAT_G2 "[BAT]tmp & (~PM8607_MEAS_EN1_GPADC2:%d\n", tmp & (~PM8607_MEAS_EN1_GPADC2));
  }
}
#endif  ///Eason_Chang///
#if 0 ///Eason_Chang///
static void AXC_SWGauge_EnableTemperatureGADC(bool abEnabled)
{
  int tmp;

  if(true == abEnabled) {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp | PM8607_MEAS_EN1_TBAT);
    printk(DBGMSK_BAT_G2 "[BAT]tmp | PM8607_MEAS_EN1_TBAT:%d\n", tmp | PM8607_MEAS_EN1_TBAT);
  } else {
    asus_pmic_reg_read(PM8607_MEAS_EN1, &tmp);
    asus_pmic_reg_write(PM8607_MEAS_EN1, tmp & (~PM8607_MEAS_EN1_TBAT));
    printk(DBGMSK_BAT_G2 "[BAT]tmp & (~PM8607_MEAS_EN1_TBAT:%d\n", tmp & (~PM8607_MEAS_EN1_TBAT));
  }
}
#endif  ///Eason_Chang///

static void AXC_SWGauge_InitGADC(void)
{
#if 0 ///Eason_Chang///
  printk(DBGMSK_BAT_G2 "[BAT]AXC_SWGauge_InitGADC++\n");
  AXC_SWGauge_EnableBatIdGADC(true);
  AXC_SWGauge_EnableVoltageGADC(true);
  AXC_SWGauge_EnableCurrentGADC(true);
  AXC_SWGauge_EnableTemperatureGADC(true);
  printk(DBGMSK_BAT_G2 "[BAT]AXC_SWGauge_InitGADC--\n");
  #endif  ///Eason_Chang///
}


#ifdef SAMPLE_THREAD_ENABLE
static void AXC_SWGauge_InitSampleData(AXC_SWGauge *this)
{
  if(NULL == this->mpVoltageSampleData) {
    AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE,
					&this->mpVoltageSampleData);
  }	

  if(NULL == this->mpCurrentSampleData) {
    AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE,
					&this->mpCurrentSampleData);
  }	

  if(NULL == this->mpTemperatureSampleData) {
    AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE,
                                        &this->mpTemperatureSampleData);
  }	
}

static void AXC_SWGauge_DeInitSampleData(AXC_SWGauge *this)
{
	if(NULL != this->mpVoltageSampleData){
		AXC_SampleDataFactory_FreeSampleData(this->mpVoltageSampleData);
	}	

	if(NULL != this->mpCurrentSampleData){
		AXC_SampleDataFactory_FreeSampleData(this->mpCurrentSampleData);
	}	

	if(NULL != this->mpTemperatureSampleData){
		AXC_SampleDataFactory_FreeSampleData(this->mpTemperatureSampleData);
	}	
}
#endif

//Return R*1000
#if 0 ///Eason_Chang///
static int AXC_SWGauge_LookupResisterByTemperature(int anTemperature)
{
  int lnResister;

  if( anTemperature > 34 )        lnResister = 250;
  else if( anTemperature > 23 )   lnResister = 250;
  else if( anTemperature > 12 )   lnResister = 250;
  else                            lnResister = 250;
  return lnResister;
}
#endif ///Eason_Chang///

// joshtest
static int g_prev_ocv = 0;

int AXC_SWGauge_DisableChgAndReadOCV(struct AXC_SWGauge *this)
{
	int real_ocv = 0;
	int lnTimesForGatherData = 50;

	pr_debug( "[BAT][Gau]AXC_SWGauge_DisableChgAndReadOCV\n");

	if(this->mnBatteryLifePercentage >= 90) {
		real_ocv = this->msParentGauge.ReadVoltage(&this->msParentGauge,lnTimesForGatherData);
		pr_debug( "[BAT][Gau]DisableChgReadOCV:%d\n", real_ocv);
	} else {
		real_ocv = this->msParentGauge.ReadVoltage(&this->msParentGauge,lnTimesForGatherData);
		if (g_prev_ocv > real_ocv ) {
			pr_debug( "[BAT][Gau]real ocv:%d, g_prev_ocv:%d, diff:%d\n",
				real_ocv, g_prev_ocv, g_prev_ocv-real_ocv );
			real_ocv = g_prev_ocv;
		}
		g_prev_ocv = real_ocv;
	}

	pr_debug( "[BAT][Gau]real ocv:%d in AXC_SWGauge_DisableChgAndReadOCV\n", real_ocv);
	return real_ocv ;
}

static int AXC_SWGauge_LookupPercentageByOCV(int anVocv)
{
    int target_index = 0;

    target_index = LinearSearch(anVocv, CurrOCVtoPerTbl, OCV_TBL_SIZE);
    if (target_index == -1) {
    	printk(DBGMSK_BAT_ERR "[BAT][Gau]There are something wrong in OCV(%d) to Percentage\n",anVocv);
        target_index = 0;
    } else {
		pr_debug( "[BAT][Gau] ocv per index:%d\n", target_index);
	}

    return target_index;	
}

#ifdef CFG_REF_LAST_BAT_INFO

static void read_int_from_file(struct file *cfile, int *int_val, loff_t *offset)
{
	mytemp int_val_from_file = {0};
	mm_segment_t orig_fs;
	char delta_t_str[INT_TO_FILE_LENGTH] = {0};
	int i;
	int r_size = 0;

	assert(NULL != cfile);
	orig_fs = get_fs();
	set_fs(KERNEL_DS);
	r_size = vfs_read(cfile, delta_t_str, INT_TO_FILE_LENGTH, &offset);
	for (i = 0; i < INT_TO_FILE_LENGTH; i++) {
		int_val_from_file.mnChar[i] = delta_t_str[i];
		pr_debug( "[BAT][Gau]%s(), delta_t_str[%d]: %d\n", __func__, i, delta_t_str[i]);
	}
	*int_val = (int)int_val_from_file.mnValue;
	set_fs(orig_fs);	

	pr_debug( "[BAT][Gau]%s(), r_size: %d\n", __func__, r_size);
	*offset = *offset + r_size;

	pr_debug( "[BAT][Gau]%s(), read int_val: %d, *offset:%d\n",
		__func__,
		*int_val,
		*offset);
	return;
}

static void write_int_to_file(struct file *cfile, int int_val, loff_t *offset)
{
	mytemp int_val_to_file = {0};
	mm_segment_t orig_fs;	
	char delta_t_str[4] = {0};
	int i;
	int w_size = 0;

	assert(NULL != cfile);
	pr_debug( "[BAT][Gau]%s(), write int_val: %d\n", __func__, int_val);

	int_val_to_file.mnValue = (s16)int_val;
	for (i = 0; i < INT_TO_FILE_LENGTH; i++) {
		delta_t_str[i] = int_val_to_file.mnChar[i];
		pr_debug( "[BAT][Gau]%s(), delta_t_str[%d]: %d\n", __func__, i, delta_t_str[i]);
	}

    orig_fs = get_fs();
    set_fs(KERNEL_DS);
	w_size = vfs_write(cfile, delta_t_str, 4, &offset);
    set_fs(orig_fs);

	pr_debug( "[BAT][Gau]%s(), w_size: %d\n", __func__, w_size);
	*offset = *offset + w_size;

	pr_debug( "[BAT][Gau]%s(), offset: %d\n", __func__, *offset);
	return;
}

/* swg_get_rtc_time - Used by SW gauge to get rtc time. */
static int swg_get_rtc_time(struct rtc_time *rtc_time)
{
    int err = 0;
    
    struct rtc_device *rtc_dev = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
    if (NULL == rtc_dev) {
        pr_debug( "[BAT][Gau]%s(), unable to open rtc device (%s)\n",
            __func__, CONFIG_RTC_HCTOSYS_DEVICE);
        return -ENODEV;
    }

    err = rtc_read_time(rtc_dev, rtc_time);
    if (0 == err) {
		pr_debug( "[BAT][Gau]%s(), tm_sec:%d, tm_min:%d, tm_hour:%d, tm_mday:%d, tm_mon:%d, tm_year:%d\n",
			__func__,
			rtc_time->tm_sec,
			rtc_time->tm_min,
			rtc_time->tm_hour,
			rtc_time->tm_mday,
			rtc_time->tm_mon,
			rtc_time->tm_year);
	} else {
        dev_err(rtc_dev->dev.parent,
            "[BAT][Gau]unable to read the hardware clock\n");
    }

    rtc_class_close(rtc_dev);
    return err;
}

/* interval_from_last_save_bat_info - Return the interval from last save battery
 * information. The unit of interval is second.
 */
static unsigned long interval_from_last_save_bat_info(struct rtc_time *last_rtc_time)
{
	struct rtc_time curr_rtc_time = {0};
	unsigned long last_time;
	unsigned long curr_time;
	int err = 0;

	assert(NULL != last_rtc_time);

	err = swg_get_rtc_time(&curr_rtc_time);
	if (0 != err) {
        pr_debug( "[BAT][Gau]Error!!! %s(): error when get rtc time\n", __func__);
	}	

	rtc_tm_to_time(last_rtc_time, &last_time);
	rtc_tm_to_time(&curr_rtc_time, &curr_time);

	pr_debug( "[BAT][Gau] last_time:%d, curr_time:%d, interval:%d\n",
		last_time,
		curr_time,
		curr_time - last_time);

	return (curr_time - last_time);
}

static struct file* last_bat_info_file_fd = NULL;

static struct file* create_last_bat_info_file(void)
{
	struct file *cfile = NULL;
    cfile = filp_open(LAST_BAT_INFO_FILE_PATH, O_CREAT | O_RDWR | O_SYNC, 0666);
    if (IS_ERR(cfile)){
		printk(DBGMSK_BAT_ERR "[BAT][Gau] Error!!! when creat .last_bat_info_file");
    }

	pr_debug( "[BAT][Gau]%s() \n", __func__);

	return cfile;
}

static void read_last_bat_info_file(struct file *cfile, int *bat_life, struct rtc_time *rtc_time)
{
	loff_t offset = 0;
	mytemp int_val_from_file = {0};
	mm_segment_t orig_fs;
	char delta_t_str[BAT_INFO_LEN] = {0};
	int cnt = 0;
	int r_size = 0;

	assert(NULL != cfile);
	assert(NULL != bat_life);
	assert(NULL != rtc_time);

	orig_fs = get_fs();
	set_fs(KERNEL_DS);
	r_size = vfs_read(cfile, delta_t_str, BAT_INFO_LEN, &offset);
	set_fs(orig_fs);

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	*bat_life = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_sec = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_min = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_hour = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_mday = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_mon = (int)int_val_from_file.mnValue;

	int_val_from_file.mnChar[0] = delta_t_str[cnt++];
	int_val_from_file.mnChar[1] = delta_t_str[cnt++];
	rtc_time->tm_year = (int)int_val_from_file.mnValue;

	assert(cnt == BAT_INFO_LEN);

	pr_debug( "[BAT][Gau]r_size:%d\n", r_size);

	pr_debug( "[BAT][Gau]%s(), bat_life: %d, tm_sec:%d, tm_min:%d, tm_hour:%d, tm_mday:%d, tm_mon:%d, tm_year:%d, count:%d\n",
		__func__,
		*bat_life,
		rtc_time->tm_sec,
		rtc_time->tm_min,
		rtc_time->tm_hour,
		rtc_time->tm_mday,
		rtc_time->tm_mon,
		rtc_time->tm_year,
		cnt);

	return;
}

static void write_last_bat_info_file(struct file *cfile, int bat_life, struct rtc_time *rtc_time)
{
	loff_t offset = 0;
	mytemp int_val_to_file = {0};
	mm_segment_t orig_fs;
	char delta_t_str[BAT_INFO_LEN] = {0};
	int cnt = 0;
	int w_size = 0;

	assert(NULL != cfile);
	assert(NULL != rtc_time);

	pr_debug( "[BAT][Gau]%s(), bat_life: %d, tm_sec:%d, tm_min:%d, tm_hour:%d, tm_mday:%d, tm_mon:%d, tm_year:%d\n",
		__func__,
		bat_life,
		rtc_time->tm_sec,
		rtc_time->tm_min,
		rtc_time->tm_hour,
		rtc_time->tm_mday,
		rtc_time->tm_mon,
		rtc_time->tm_year);

	int_val_to_file.mnValue = (s16)bat_life;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_sec;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_min;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_hour;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_mday;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_mon;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	int_val_to_file.mnValue = (s16)rtc_time->tm_year;
	delta_t_str[cnt++] = int_val_to_file.mnChar[0];
	delta_t_str[cnt++] = int_val_to_file.mnChar[1];

	orig_fs = get_fs();
	set_fs(KERNEL_DS);
	w_size = vfs_write(cfile, delta_t_str, 14, &offset);
	set_fs(orig_fs);	

	assert(cnt == BAT_INFO_LEN);

	pr_debug( "[BAT][Gau]delta_t_str count:%d, w_size:%d\n", cnt, w_size);

	return;
}

static int AXC_SWGauge_LoadLastBatInfo(
	struct AXI_Gauge *apGauge,
	int *last_bat_life,
	struct rtc_time *last_rtc_time)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

	assert(NULL != apGauge);
	assert(NULL != last_bat_life);
	assert(NULL != last_rtc_time);
	
	pr_debug( "[BAT][Gau]%s()\n", __func__);

	last_bat_info_file_fd = filp_open(LAST_BAT_INFO_FILE_PATH, O_RDONLY, 0);
	if (!IS_ERR(last_bat_info_file_fd)) {
		// for read file test
		read_last_bat_info_file(
			last_bat_info_file_fd,
			last_bat_life,
			last_rtc_time);
		filp_close(last_bat_info_file_fd, 0);		
		return 0;
	} else {
		printk(DBGMSK_BAT_ERR "[BAT][Gau]Error!!! %s(): error when load last bat info file\n", __func__);
		return -1;
	}
}

/* AXC_SWGauge_SaveLastBatInfo - Export interface to let others to save last battery
 * information. Such as battery life, time to shutdown.
 */
static int AXC_SWGauge_SaveLastBatInfo(struct AXI_Gauge *apGauge)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	struct rtc_time rtc_time_before_shutdown = {0};
	int bat_life_from_file;
	struct rtc_time rtc_time_from_file = {0};
	int err = 0;
	mm_segment_t orgfs;
	
	pr_debug( "[BAT][Gau]%s(), mnBatteryLifePercentage:%d\n",
		__func__,
		this->mnBatteryLifePercentage);

	update_bat_info(this, SWGAUGE_BAT_UPDATE_CYCLE);

	err = swg_get_rtc_time(&rtc_time_before_shutdown);
	if (0 != err) {
        printk(DBGMSK_BAT_ERR "[BAT][Gau] Error!!! %s(), when get rtc time\n", __func__);
	}

    last_bat_info_file_fd = filp_open(LAST_BAT_INFO_FILE_PATH, O_CREAT | O_RDWR | O_SYNC, 0666);
    if (IS_ERR(last_bat_info_file_fd)){
		printk(DBGMSK_BAT_ERR "[BAT][Gau] Error!!! when creat .last_bat_info_file");
		return -1;
    }

	write_last_bat_info_file(
		last_bat_info_file_fd,
		this->mnBatteryLifePercentage,
		&rtc_time_before_shutdown);

	filp_close(last_bat_info_file_fd, 0);

	return 0;
}

#endif /* CFG_REF_LAST_BAT_INFO */


#ifdef CFG_VF_TUNING
int get_finetune_ocv_for_vf(struct AXC_SWGauge *this, int ocv_val)
{
	int ocv_percent;
	int fix_vf;
	int finetune_ocv;

	pr_debug( "[BAT][Gau]original ocv:%d\n", ocv_val);

	ocv_percent	= AXC_SWGauge_LookupPercentageByOCV(ocv_val);
	fix_vf = vf_to_percent_tbl[ocv_percent]*(this->m_vf_tuning_ratio)/10;
	finetune_ocv = ocv_val - fix_vf;

	pr_debug( "[BAT][Gau]fix_vf:%d, finetune ocv:%d\n",fix_vf ,finetune_ocv);
	return finetune_ocv;
}


int finetune_ocv_for_vf(AXC_SWGauge *this, int ocv, AXE_Charger_Type charger_mode)
{
	int finetune_ocv = 0;
	pr_debug( "[BAT] %s()", __func__);
	switch(charger_mode) {
	case LOW_CURRENT_CHARGER_TYPE:
		finetune_ocv = get_finetune_ocv_for_vf(ocv);
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		finetune_ocv = get_finetune_ocv_for_vf(ocv);
		break;
	default:
		pr_debug( "[BAT]Error!!! Wrong charger mode: %d in this state!", charger_mode);
		break;
	}
	return finetune_ocv;
}
#endif /* CFG_VF_TUNING */


/* AXC_SWGauge_CalBatPercentWhenChargerIn - calculate battery percentage when adpater in */
int AXC_SWGauge_CalBatPercentWhenChargerIn(struct AXC_SWGauge *this)
{
	int lnVocv = 0;
	int lnPercentage = 0;
	pr_debug( "[BAT] %s()", __func__);
	lnVocv = AXC_SWGauge_DisableChgAndReadOCV(this) ;

#ifdef CFG_VF_TUNING
	lnVocv = finetune_ocv_for_vf(this, lnVocv);
#endif /* CFG_VF_TUNING */

	lnPercentage = AXC_SWGauge_LookupPercentageByOCV(lnVocv);

	return lnPercentage;
}

static int get_max_rise_mah(AXE_Charger_Type charger_mode)
{
	int max_rise_mah = 0;
	pr_debug( "[BAT] %s()", __func__);
	switch(charger_mode) {
	case LOW_CURRENT_CHARGER_TYPE:
		max_rise_mah = MAX_RAISE_USB_MAH;
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		max_rise_mah = MAX_RAISE_AC_MAH;
		break;
	default:
		pr_debug( "[BAT]Error!!! Wrong charger mode: %d in this state!", charger_mode);
		break;
	}

	return max_rise_mah;
}

// joshtest


/* AXC_SWGauge_CalProperBatLifeWhenChargerIn - */
static int AXC_SWGauge_CalProperBatLifeWhenChargerIn(
	struct AXC_SWGauge *this,
	int anPercentage,
	AXE_Charger_Type charger_mode)
{
	int lnPraise = 0;	

	lnPraise = anPercentage -this->mnBatteryLifePercentage;

	if (lnPraise >0) {
		int max_raise_mah;
		struct timespec mtNow;
		int lnIntervalFromLastUpdate;
		int lnDropBetweenInterval;
		int max_raise_percent;
		max_raise_mah = get_max_rise_mah(charger_mode);
		
		//int lnIntervalFromLastUpdate = jiffies_to_msecs(jiffies - this->mnjiffiesFromLastUpdateBattery);
		mtNow = current_kernel_time();
		lnIntervalFromLastUpdate = mtNow.tv_sec - this->mtLastUpdatedTime.tv_sec;
		lnDropBetweenInterval = (max_raise_mah*100/BATT_CAP_MAH)*lnIntervalFromLastUpdate/3600;

		max_raise_percent = max(1,lnDropBetweenInterval);

		anPercentage = this->mnBatteryLifePercentage + min(lnPraise , max_raise_percent);

		pr_debug( "[BAT][Gau]anPercentage=%d, lnIntervalFromLastUpdate=%d, lnDropBetweenInterval=%d, max_raise_percent=%d, max_raise_mah=%d\n",
			anPercentage,
			lnIntervalFromLastUpdate,
			lnDropBetweenInterval,
			max_raise_percent,
			max_raise_mah);	
	}else{
		anPercentage = this->mnBatteryLifePercentage;
	}

	if(anPercentage < this->mnBatteryLifePercentage){

		//Should not happen....monitoring...
		printk(DBGMSK_BAT_ERR "[BAT][ERR]lnPercentage < this->mnBatteryLifePercentage!!\n");

		anPercentage = this->mnBatteryLifePercentage;

	}

	return anPercentage;

}

//joshtest
int AXC_SWGauge_CalculateBatteryLifeWhenDCInWithCorrection(struct AXC_SWGauge *this)
{
	//int lnVocv = 0;
	int lnPercentage = 0;
	lnPercentage = AXC_SWGauge_CalBatPercentWhenChargerIn(this);
	pr_debug( "[BAT][Gau] battery life: %d, without correction when dc in\n", lnPercentage);

	lnPercentage = AXC_SWGauge_CalProperBatLifeWhenChargerIn(
		this,
		lnPercentage,
		this->mpCharger->GetChargerStatus(this->mpCharger));
	return lnPercentage;
}


int AXC_SWGauge_CalculateBatteryLifeOnlyBattery(struct AXC_SWGauge *this, int anVoltage, int anCurrent, int anTemperature)
{
	int lnVocv = 0;
	int lnPercentage = 0;
	int resistor;

//TODO: need to use new function
	if (E_SW_GAUGE_TYPE == this->mnType) {
		resistor = this->mnResistorCalibration;	//old gauge 
	} else {
		resistor = this->resistorCali;	// new gauge
	}
	
	lnVocv = anVoltage + anCurrent * resistor/1000 ;

	lnPercentage = AXC_SWGauge_LookupPercentageByOCV(lnVocv);

	// Essential information for OCV table
	printk(KERN_INFO "[BAT][Gau]%s(), ocv per=%d,ocv=%d,v=%d,c=%d,r=%d\n",
		__func__,
		lnPercentage,
		lnVocv,
		anVoltage,
		anCurrent,
		resistor);
	
	return lnPercentage;
}

static int AXC_SWGauge_GetProperBatteryLifeForOnlyBattery(struct AXC_SWGauge *this,int anPercentage)
{
	int lnDrop = 0;
	
	lnDrop = this->mnBatteryLifePercentage - anPercentage;

	if(lnDrop > 0){

		//int lnIntervalFromLastUpdate = jiffies_to_msecs(jiffies - this->mnjiffiesFromLastUpdateBattery);
		struct timespec mtNow = current_kernel_time();

		int lnIntervalFromLastUpdate = mtNow.tv_sec - this->mtLastUpdatedTime.tv_sec;

		int lnDropBetweenInterval = (MAX_DROP_MAH*100/BATT_CAP_MAH)*lnIntervalFromLastUpdate/3600;

		int max_prec_drop = max(1,lnDropBetweenInterval);

		//printk("lnPercentage=%d,lnIntervalFromLastUpdate=%d,lnDropBetweenInterval=%d,max_prec_drop=%d,",
		//	lnPercentage,
		//	lnIntervalFromLastUpdate,
		//	lnDropBetweenInterval,
		//	max_prec_drop);
		

		anPercentage = this->mnBatteryLifePercentage - min(lnDrop , max_prec_drop);	
	
	}else{

		anPercentage = this->mnBatteryLifePercentage;
	}
	
	return anPercentage;

}
int AXC_SWGauge_CalculateBatteryLifeOnlyBatteryWithCorrection(struct AXC_SWGauge *this, int anVoltage, int anCurrent, int anTemperature)
{
	int lnPercentage = 0;

	lnPercentage = AXC_SWGauge_CalculateBatteryLifeOnlyBattery(this,
								anVoltage,
								anCurrent,
								anTemperature);
	lnPercentage = AXC_SWGauge_GetProperBatteryLifeForOnlyBattery(this,lnPercentage);
	return lnPercentage;
}

#ifdef SAMPLE_THREAD_ENABLE
static void AXC_SWGauge_ClearSampleData(struct AXC_SWGauge *this)
{
	this->mpVoltageSampleData->Reset(this->mpVoltageSampleData);
	this->mpCurrentSampleData->Reset(this->mpCurrentSampleData);
	this->mpTemperatureSampleData->Reset(this->mpTemperatureSampleData);
}

#endif /* SAMPLE_THREAD_ENABLE */

#define THE_TIME_TO_KEEP_100_PERCENTAGE (150)//SEC
#if 0 ///Eason_Chang///
static void AXC_SWGauge_CalculateBatteryLife(struct AXC_SWGauge *this, int anVoltage, int anCurrent, int anTemperature)
{
	int lnPreviousBatteryLifePercentage = this->mnBatteryLifePercentage;

	pr_debug( "[BAT]this->mbKeep100Percentage:%d, this->mnBatteryLifePercentage:%d\n", this->mbKeep100Percentage, this->mnBatteryLifePercentage);

	if(true == this->mbKeep100Percentage) {
		if(this->mnBatteryLifePercentage != 100) {
			printk(DBGMSK_BAT_ERR "[BAT][ERR]Should not happen!Previous percentage should be 100!!\n");
		} else {
			struct timespec mtNow = current_kernel_time();
			if(THE_TIME_TO_KEEP_100_PERCENTAGE > mtNow.tv_sec - this->mtTimeWhenCablePluggedOutAnd100Percentage.tv_sec)
			goto _CLEAR_DATA;
		}
	}

	this->mbKeep100Percentage = false;
	if(true == this->mpCharger->IsCharegrPlugin(this->mpCharger)) {
		pr_debug( "[BAT]IsCharegrPlugin\n");
		//the first time to calculate battery life
		if ((BATTERY_LIFE_PERCENTAGE_IN_VALID == this->mnBatteryLifePercentage) ||
			(true == this->mbISChargerStatusChangedAfterResume))
		{
			int lnBatteryLife;
			lnBatteryLife = AXC_SWGauge_CalBatPercentWhenChargerIn(this);
			if(true == this->mbISChargerStatusChangedAfterResume){
				//Before Resume:Cable not plugged in; After Resume:Cable plugged in 
				lnBatteryLife = AXC_SWGauge_GetProperBatteryLifeForOnlyBattery(this,lnBatteryLife);
				this->mbISChargerStatusChangedAfterResume = false;
			}
			this->mnBatteryLifePercentage =  lnBatteryLife;
		} else {
			pr_debug( "[BAT]AXC_SWGauge_CalculateBatteryLifeWhenDCInWithCorrection");
			this->mnBatteryLifePercentage =  AXC_SWGauge_CalculateBatteryLifeWhenDCInWithCorrection(this);
		}
	} else {
        pr_debug( "[BAT]NotCharegrPlugin\n");
		//the first time to calculate battery life
		if ((BATTERY_LIFE_PERCENTAGE_IN_VALID == this->mnBatteryLifePercentage) ||
			(true == this->mbISChargerStatusChangedAfterResume))
		{
			int lnBatteryLife;
			lnBatteryLife = AXC_SWGauge_CalculateBatteryLifeOnlyBattery(this, 
						anVoltage, 
						anCurrent,
						anTemperature);
			if(true == this->mbISChargerStatusChangedAfterResume){
				/* Before Resume:Cable plugged in; After Resume:Cable not plugged in
				 * So calculate bat life as charger plug in
				 */
				lnBatteryLife = AXC_SWGauge_CalProperBatLifeWhenChargerIn(
					this,
					lnBatteryLife,
					this->m_charger_mode_before_suspend);
				this->mbISChargerStatusChangedAfterResume = false;
			}
			this->mnBatteryLifePercentage =  lnBatteryLife;
		}else{
			this->mnBatteryLifePercentage =  AXC_SWGauge_CalculateBatteryLifeOnlyBatteryWithCorrection(this, 
						anVoltage, 
						anCurrent,
						anTemperature);
		}
	}

    pr_debug( "[BAT]this->mnBatteryLifePercentage:%d\n", this->mnBatteryLifePercentage);

	//this->mnjiffiesFromLastUpdateBattery = jiffies;
	msleep(100);

	pr_debug( "[BAT]Update Jiffies(msec):%d\n",jiffies_to_msecs(this->mnjiffiesFromLastUpdateBattery));
	if(this->mnBatteryLifePercentage >= 100)
    {
		if(true == this->mpCharger->IsCharging(this->mpCharger))
        {
			pr_debug( "[BAT]Although 100,but still in charging, update 99\n");
			this->mnBatteryLifePercentage = 99;
		}
	} else if(this->mnBatteryLifePercentage >= 95) {
        //Need to know if being charging or not	
		if(true == this->mpCharger->IsCharegrPlugin(this->mpCharger)) {
			if(false == this->mpCharger->IsCharging(this->mpCharger)) {
				pr_debug( "[BAT]keep percentage=100\n");
				this->mnBatteryLifePercentage = 100;
			} else {
				pr_debug( "[BAT]IsCharging%d\n",this->mpCharger->IsCharging(this->mpCharger));
			}
		}
        else
        {
			pr_debug( "[BAT]xxx\n");
		}
	}

	if(100 != lnPreviousBatteryLifePercentage && 100 == this->mnBatteryLifePercentage)
    {
		this->mn100PercentageOCV = this->msParentGauge.ReadVoltage(&this->msParentGauge,50);
		pr_debug( "[BAT]100-Per OCV=%d\n",this->mn100PercentageOCV);
	}

_CLEAR_DATA:
	this->mtLastUpdatedTime = current_kernel_time();
#ifdef SAMPLE_THREAD_ENABLE
	AXC_SWGauge_ClearSampleData(this);
#endif	
}
#endif  ///Eason_Chang///
static void NotifyBatteryObersever(struct AXC_SWGauge *this)
{
	assert(NULL != this);
	assert(NULL != this->mpNotifier);

	if(this->msParentGauge.IsBatteryRemoved(&this->msParentGauge)){
		pr_debug( "[BAT][Gau] %s(), bat removed and prepare to shutdown.", __func__);
		this->mpNotifier->OnBatteryRemoved(&this->msParentGauge);
	} else {
		this->mpNotifier->UpdateBatteryLifePercentage(&this->msParentGauge, this->mnBatteryLifePercentage);
	}

	return;
}

//joshtest


static enum swgauge_bat_update_state g_bat_update_state = SWGAUGE_BAT_UPDATE_LIMIT;
///Eason_Chang///static enum swgauge_bat_update_state g_prev_bat_update_state = SWGAUGE_BAT_UPDATE_LIMIT;

/* g_bat_update_state_lock - Spinlock to update battery state. */
spinlock_t g_bat_update_state_lock;

/* swg_set_bat_update_state - 
 */
static void swg_set_bat_update_state(
	AXC_SWGauge *this,
	enum swgauge_bat_update_state set_state)
{
	unsigned long flags;

#ifdef CFG_VF_TUNING
	/* If return in the following, it means calculating vf ongoing. We shall wait
	 * until SWGAUGE_BAT_UPDATE_CAL_VF_DONE.
	 */
	if ((SWGAUGE_BAT_UPDATE_CAL_VF == g_bat_update_state) &&
		this->mpCharger->IsCharging(this->mpCharger) &&
		(SWGAUGE_BAT_UPDATE_CAL_VF_DONE != set_state)) {
		pr_debug( "[BAT][Gau] ignore set_state:%d for calculate VF\n", set_state);
		return;
	}
#endif /* CFG_VF_TUNING */

	spin_lock_irqsave(&g_bat_update_state_lock, flags);
	g_bat_update_state = set_state;
	spin_unlock_irqrestore(&g_bat_update_state_lock, flags);
	return;
}

/* swg_get_bat_update_state - 
 */
static enum swgauge_bat_update_state swg_get_bat_update_state(void)
{
	return g_bat_update_state;
}


/* cal_ocv_percent_when_discharging - Calculate battery life when discharging.
 * Use formula, OCV = V + I*R, to get ocv persent. 
 */
static int cal_ocv_percent_when_discharging(
	struct AXC_SWGauge *this,
	int volt,
	int curr)
{
	int ocv = 0;
	int percent = 0;

	int resistor;

	assert(NULL != this);

// TODO: need to use new function 
	if (E_SW_GAUGE_TYPE == this->mnType) {
		resistor = this->mnResistorCalibration;	//old gauge 
	} else {
		resistor = this->resistorCali;	// new gauge
	}
	resistor =150 ;
	ocv = volt + curr*resistor/1000 ;

#ifdef CFG_CHECK_BAT_LOW_DETECTOR
	/* Do not has bat low irq and ocv less than bat low ocv. It meet the battery low shutdown criteria*/
	if ((!this->m_has_bat_low_detector)&&(ocv <= BAT_LOW_OCV)) {
		this->m_bat_low = true;
		pr_debug( "[BAT][Gau] no bat low irq and has bat low ocv\n");
	}
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */
	
	percent = AXC_SWGauge_LookupPercentageByOCV(ocv);
	// Essential information for OCV table
	pr_debug( "[BAT][Gau]%s(), ocv per=%d, ocv=%d, v=%d, c=%d, r=%d\n",
		__func__, percent, ocv, volt, curr, resistor);

	return percent;
}

/* swg_discharging_to_read_volt - Used to read correct voltage when charging. */
static int swg_discharging_to_read_volt(
	struct AXC_SWGauge *this,
	AXE_Charger_Type charger_mode,
	int times_to_read_adc)
{
	int volt;
	int curr;

	assert(NULL != this);
	assert(NULL != this->mpCharger);

	if (this->mpCharger->IsCharging(this->mpCharger)) {
		pr_debug( "[BAT][Gau]%s(), charging now\n", __func__);
		//this->mpCharger->SetCharger(this->mpCharger, TEMP_NO_CHARGER_TYPE);
		this->msParentGauge.ReadVoltCurr(
		&this->msParentGauge,
		times_to_read_adc,
		&volt,
		&curr);
		//this->mpCharger->SetCharger(
			//this->mpCharger,
			//this->mpCharger->GetChargerStatus(this->mpCharger));
	} else {
		pr_debug( "[BAT][Gau]%s(), discharging now\n", __func__);
		this->msParentGauge.ReadVoltCurr(
		&this->msParentGauge,
		times_to_read_adc,
		&volt,
		&curr);
	}

	pr_debug( "[BAT][Gau]%s(), volt=%d\n",
		__func__, volt);

	return volt;
}

/* swg_get_charger_mode - Used by SW gauge to get charger mode. */
static AXE_Charger_Type swg_get_charger_mode(
	AXC_SWGauge *this)
{
	assert(NULL != this);
	assert(NULL != this->mpCharger);
	return this->mpCharger->GetChargerStatus(this->mpCharger);
}

/* get_correct_charger_mode_for_update_bat - get correct charger mode for update
 * bat info. Because charger plug-in or plug-out will resume system, we shall use
 * the charger state before suspend as correct charger state.
 */
#if 0 ///Eason_Chang///
static AXE_Charger_Type get_correct_charger_mode_for_update_bat(
	AXC_SWGauge *this,
	enum swgauge_bat_update_state update_state)
{
	AXE_Charger_Type correct_charger_mode = ERROR_CHARGER;
	pr_debug( "[BAT] %s()\n", __func__);

	assert(NULL != this);

	switch(update_state) {
	case SWGAUGE_BAT_UPDATE_FIRST:
	case SWGAUGE_BAT_UPDATE_CYCLE:
		correct_charger_mode = this->mpCharger->GetChargerStatus(this->mpCharger);
		break;
	case SWGAUGE_BAT_UPDATE_RESUME:
		//correct_charger_mode = this->m_charger_mode_before_suspend;
		correct_charger_mode = this->mpCharger->GetChargerStatus(this->mpCharger);
		break;
	default:
		pr_debug( "[BAT][Gau]Error!!! Wrong state: %d in this state!", update_state);
		break;	
	}

	pr_debug( "[BAT][Gau]correct charger mode: %d\n", correct_charger_mode);
	return correct_charger_mode;
}
#endif  ///Eason_Chang///

/* cal_ocv_percent_by_volt_directly - Calculate ocv percent by voltage directly.
 * Usually used by the case of discharging to read volt.
 */
static int cal_ocv_percent_by_volt_directly(
	struct AXC_SWGauge *this,
	int volt)
{
	int percent = 0;

	assert(NULL != this);

	percent = AXC_SWGauge_LookupPercentageByOCV(volt);
	// Essential information for OCV table
	pr_debug( "[BAT][Gau]%s, ocv per=%d, ocv=v=%d\n",
		__func__,
		percent,
		volt);
	return percent;
}

#define BL_MIN_UI_BRIGHTNESS 30
#define BL_MAX_UI_BRIGHTNESS 255

#define CURR_WITH_BL_MIN_UI_BRIGHTNESS 100
#define CURR_WITH_BL_MAX_UI_BRIGHTNESS 220

/* eval_backlight_consuming_current - */
#if 0 ///Eason_Chang///
static int eval_backlight_consuming_current(void)
{
	int brightness = asus_td_get_bl_brightness();
	int curr = 0;
	if ((brightness < BL_MIN_UI_BRIGHTNESS) || (brightness > BL_MAX_UI_BRIGHTNESS)) {
		return 0;
	}

	curr = CURR_WITH_BL_MIN_UI_BRIGHTNESS +
		(brightness - BL_MIN_UI_BRIGHTNESS)*
		(CURR_WITH_BL_MAX_UI_BRIGHTNESS - CURR_WITH_BL_MIN_UI_BRIGHTNESS)/
		(BL_MAX_UI_BRIGHTNESS - BL_MIN_UI_BRIGHTNESS);

	pr_debug( "[BAT][Gau]%s(), evaluated bl current:%d \n",
		__func__,
		curr);

	return curr;
}
#endif  ///Eason_Chang///

/* eval_bat_life_when_discharging - */
static int eval_max_charging_current(
	AXE_Charger_Type charger_mode)
{
	int max_charging_curr = 0;
	switch(charger_mode) {
	case LOW_CURRENT_CHARGER_TYPE:
		//max_charging_curr = MAX_RAISE_USB_MAH - eval_backlight_consuming_current();
		max_charging_curr = MAX_RAISE_USB_MAH;
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		//max_charging_curr = MAX_RAISE_AC_MAH - eval_backlight_consuming_current();
		max_charging_curr = MAX_RAISE_AC_MAH;
		break;
	default:
		printk(DBGMSK_BAT_ERR "[BAT][Gau]Error!!! Wrong charger mode in %s()\n",
			__func__);
		max_charging_curr = -1;
		break;
	}

	pr_debug( "[BAT][Gau]%s(), max_charging_current:%d\n",
			__func__, max_charging_curr);
	return max_charging_curr;
}


/* eval_bat_life_when_discharging - Evaluate conservative battery life when discharging.
 * Use (maximum consuming current * update interval) as one factor.
 */
static int eval_bat_life_when_discharging(
	struct AXC_SWGauge *this,
	int ocv_percent)
{
	int bat_life = 0;
	int var_drop_base=0;
	//+++ASUS BSP Eason_Chang return with last showed capacity
	int drop_val = this->mnBatteryLifePercentage- ocv_percent;
	//int drop_val = this->m_truely_bat_life_percent - ocv_percent;
    //---ASUS BSP Eason_Chang return with last showed capacity

	pr_debug( "[BAT][Gau]%s(), drop_val:%d\n",
		__func__,
		drop_val);

	assert(NULL != this);

	if (drop_val > 0) {
		struct timespec now = current_kernel_time();
		int interval = now.tv_sec - this->mtLastUpdatedTime.tv_sec; // interval from last update
		int max_predict_drop_val = 0;
		int finetune_max_predict_drop_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_predict_drop_val = (MAX_DROP_MAH*100/BATT_CAP_MAH)*interval/3600;
			/* variable drop base for faster leverage true batter life */
			if ((drop_val/2) > 0) {
				var_drop_base = drop_val/2;
			} else {
				var_drop_base = 1;
			}

			finetune_max_predict_drop_val = max(var_drop_base, max_predict_drop_val);
			bat_life = this->mnBatteryLifePercentage - min(drop_val, finetune_max_predict_drop_val);
		} else {
			bat_life = this->mnBatteryLifePercentage - drop_val;
			printk(DBGMSK_BAT_ERR "[BAT][Gau]Error!!! %s(), interval < 0, current time:%d\n",
					__func__, (int)now.tv_sec);
		}

		pr_debug( "[BAT][Gau] interval:%d, var_drop_base:%d, max_predict_drop_val:%d, finetune_max_predict_drop_val:%d \n",
			interval,
			var_drop_base,
			max_predict_drop_val,
			finetune_max_predict_drop_val);
	} else {
	    //+++ASUS BSP Eason_Chang return with last showed capacity
        bat_life = this->mnBatteryLifePercentage;
		//bat_life = this->m_truely_bat_life_percent;
        //---ASUS BSP Eason_Chang return with last showed capacity
		if (drop_val < 0) {
			pr_debug( "[BAT][Gau] Error!!! %s(), drop val less than 0.\n", __func__);
		}
	}

	return bat_life;
}

// No used
/* eval_bat_life_with_fastest_charging_time_tbl - */
#if 0 ///Eason_Chang///
int eval_bat_life_with_fastest_charging_time_tbl(
	struct AXC_SWGauge *this,
	int bat_life,
	int minute_interval)
{
	int finetune_bat_life;
	int charging_time;

	if (0 == minute_interval) {
		finetune_bat_life = bat_life;
		return finetune_bat_life;
	}

	charging_time = fastest_charging_time_tbl[bat_life] + minute_interval;
	finetune_bat_life = swg_search_percent_from_tbl(
		charging_time,
		fastest_charging_time_tbl,
		FASTEST_CHARGING_TBL_SIZE);
	if (finetune_bat_life < 0) {
		printk(DBGMSK_BAT_ERR "[BAT][Gau]%s(), cann't find target in table\n",
			__func__);
	}

	printk(DBGMSK_BAT_ERR "[BAT][Gau]%s(), finetune_bat_life:%d\n",
		__func__,
		finetune_bat_life);

	return finetune_bat_life;
}
#endif  ///Eason_Chang///

/* g_bat_life_after_dot - remain battery life after dot, used for more accuracy */
static int g_bat_life_after_dot = 0;

/* eval_bat_life_when_charging - Evaluate conservative battery life when charging.
 * Use (maximum charging current * update interval) as one factor.
 */
static int eval_bat_life_when_charging(
	struct AXC_SWGauge *this,
	int ocv_percent,
	int max_rise_mah)
{
	int bat_life = 0;
	//+++ASUS BSP Eason_Chang return with last showed capacity
    int rise_val = ocv_percent - this->mnBatteryLifePercentage;
	//int rise_val = ocv_percent - this->m_truely_bat_life_percent;
	//---ASUS BSP Eason_Chang return with last showed capacity 
	int tmp_val_after_dot = 0;

	if (rise_val > 0) {
		struct timespec now = current_kernel_time();
		long interval = now.tv_sec - this->mtLastUpdatedTime.tv_sec; // interval from last update
		long max_bat_life_rise_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_bat_life_rise_val = (max_rise_mah*100/BATT_CAP_MAH)*interval/3600;
			/* to calculate the first number after the decimal dot for more accuracy.*/
			tmp_val_after_dot = ((10*max_rise_mah*100/BATT_CAP_MAH)*interval/3600)%10;
			pr_debug( "[BAT][Gau]%s(), tmp_val_after_dot:%d\n",
				__func__,
				tmp_val_after_dot);	
			g_bat_life_after_dot += tmp_val_after_dot;
			if ((g_bat_life_after_dot/10) >= 1) {
				pr_debug( "[BAT][Gau]%s(), g_bat_life_after_dot:%d\n",
					__func__,
					g_bat_life_after_dot);
				max_bat_life_rise_val += (g_bat_life_after_dot/10);
				g_bat_life_after_dot = g_bat_life_after_dot%10;
			}
			bat_life = this->mnBatteryLifePercentage + min(rise_val, (int)max_bat_life_rise_val);
		} else {
			bat_life = this->mnBatteryLifePercentage + rise_val;
			printk(DBGMSK_BAT_ERR "[BAT][Gau]Error!!! %s(), interval < 0, current time:%d\n",
					__func__, (int)now.tv_sec);
		}

		pr_debug( "[BAT][Gau]%s(), rise_val:%d, interval:%d, max_bat_life_rise_val:%d, bat_life:%d\n",
			__func__,
			rise_val,
			(int)interval,
		    (int)max_bat_life_rise_val,
			bat_life);
	} else {
	    //+++ASUS BSP Eason_Chang return with last showed capacity
	    bat_life = this->mnBatteryLifePercentage;
		//bat_life = this->m_truely_bat_life_percent;
		//---ASUS BSP Eason_Chang return with last showed capacity
		pr_debug( "[BAT][Gau]%s(), keep the same bat_life:%d as before\n",
			__func__,
			bat_life);

		if (rise_val < 0) {
			printk(DBGMSK_BAT_ERR "[BAT][Gau] Error!!! %s(), rise val less than 0.\n", __func__);
		}
	}
	return bat_life;
}

/* eval_bat_life_by_charger_mode_before_suspend - Evaluate battery life by charger
 * mode before suspend. Charger in and out will wake up device.
 */
static int eval_bat_life_by_charger_mode_before_suspend(
	struct AXC_SWGauge *this,
	int ocv_percent)
{
	int finetune_bat_life=0;

	assert(NULL != this);

	pr_debug( "[BAT][Gau]%s(), charger_mode_before_suspend:%d\n",
		__func__,
		this->m_charger_mode_before_suspend);

	switch(this->m_charger_mode_before_suspend) {
	case NO_CHARGER_TYPE:
		finetune_bat_life = eval_bat_life_when_discharging(this, ocv_percent);
		break;
	case LOW_CURRENT_CHARGER_TYPE:
		finetune_bat_life = eval_bat_life_when_charging(this, ocv_percent, MAX_RAISE_USB_MAH);
		break;
	case HIGH_CURRENT_CHARGER_TYPE:	
		finetune_bat_life = eval_bat_life_when_charging(this, ocv_percent, MAX_RAISE_AC_MAH);
		break;
	default:
		printk(DBGMSK_BAT_ERR "[BAT][Gau]%s(), charger_mode_before_suspend:%d\n",
			__func__,
			this->m_charger_mode_before_suspend);
		break;
	}

	return finetune_bat_life;
}

#ifdef CFG_REF_LAST_BAT_INFO
/* eval_bat_life_by_last_bat_info - Evaluate battery life by last battery info from
 * last shutdown. Even some battey info from bootup is different from last in limited
 * condition. We still trade them as the same.  
 */
static int eval_bat_life_by_last_bat_info(
	struct AXC_SWGauge *this,
	int curr_bat_life)
{
	int last_bat_life;
	int err;
	struct rtc_time last_rtc_time = {0};
	int diff_ocv_val;
	unsigned long interval_from_last_shutdown;

	err = AXC_SWGauge_LoadLastBatInfo(this, &last_bat_life, &last_rtc_time);
	if (0 != err) {
		pr_debug( "[BAT][Gau]Error!!! when load last bat info in %s()\n",
			__func__);
	}

	assert(NULL != CurrOCVtoPerTbl);

	diff_ocv_val = CurrOCVtoPerTbl[last_bat_life] - CurrOCVtoPerTbl[curr_bat_life];
	interval_from_last_shutdown = interval_from_last_save_bat_info(&last_rtc_time);
	pr_debug( "[BAT][Gau]%s(), last_bat_life:%d, curr_bat_life:%d, diff_ocv_val:%d, interval_from_last_shutdown:%d \n",
		__func__,
		last_bat_life,
		curr_bat_life,
		diff_ocv_val,
		interval_from_last_shutdown);
		
	if ((interval_from_last_shutdown > 0) &&
		(interval_from_last_shutdown < 300) &&
		(diff_ocv_val > 0) &&
		(diff_ocv_val < 25))
	{
		return last_bat_life;
	} else {
		return curr_bat_life;
	}
}
#endif /* CFG_REF_LAST_BAT_INFO */

//Todo using interface to acces charge done status....ugly code....
extern bool asus_get_charge_done(void);
/* update_bat_info_for_speical_case - Update battery info for some special cases.
 * Such as when to update battery life to 100.
 */
void update_bat_info_for_speical_case(
	struct AXC_SWGauge *this)
{

	if(this->m_truely_bat_life_percent >= BAT_LIFE_FULL)
    {
    	if(false == asus_get_charge_done())
		//if(this->mpCharger->IsCharging(this->mpCharger))
        {
			pr_debug( "[BAT][Gau]%s(), Still in charging status, so update bat life to 99\n", __func__);
			this->mnBatteryLifePercentage = BAT_LIFE_BEFORE_FULL;
		}
	} else if(this->m_truely_bat_life_percent >= BAT_LIFE_BEFORE_FULL_ONE_UNIT) {
        //Need to know if being charging or not	
		if(this->mpCharger->IsCharegrPlugin(this->mpCharger)) {
			if(true == asus_get_charge_done()){
			//if(this->mpCharger->IsCharging(this->mpCharger)) {
				pr_debug( "[BAT][Gau]%s(), keep bat life to 100\n", __func__);
				this->mnBatteryLifePercentage = BAT_LIFE_FULL;
			} else {
				pr_debug( "[BAT][Gau]%s(), still in charging\n", __func__);
			}
		}
	}

	if ((BAT_LIFE_FULL != this->m_prev_bat_life_percent) &&
		(BAT_LIFE_FULL == this->mnBatteryLifePercentage))
    {
		this->mn100PercentageOCV = this->msParentGauge.ReadVoltage(&this->msParentGauge, TIMES_FOR_GENERAL_READ_ADC);
		pr_debug( "[BAT][Gau] 100 percent ocv=%d\n",this->mn100PercentageOCV);
	}


	return;
}

/* cal_bat_life_for_first - Calculate battery life for first update(i.e. in 
 * SWGAUGE_BAT_UPDATE_FIRST) */
int cal_bat_life_for_first(
	struct AXC_SWGauge *this)
{
	int volt;
	int curr;
	int ocv_percent=0;
	int bat_life=0;
	AXE_Charger_Type correct_charger_mode;

	assert(NULL != this);

	pr_debug( "[BAT][Gau]%s()\n", __func__);

	AXC_SWGauge_InitVoltageCurrnetCalibrationData(this);
	AXC_SWGauge_InitResistorCalibrationData(this);
	pr_debug( "[BAT][Gau]%s(), init calibration data done\n", __func__);

	correct_charger_mode = swg_get_charger_mode(this);
	switch (correct_charger_mode) {
	case NO_CHARGER_TYPE:
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_FIRST_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		break;
	case LOW_CURRENT_CHARGER_TYPE:
	case HIGH_CURRENT_CHARGER_TYPE:	
		volt = swg_discharging_to_read_volt(this, correct_charger_mode, TIMES_FOR_FIRST_READ_ADC);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		break;
       default:
            break;
/*
	case TEMP_NO_CHARGER_TYPE:	///Eason_Chang///	
	    break; ///Eason_Chang//
    case ERROR_CHARGER:        ///Eason_Chang///
	    break;///Eason_Chang///
*/
	}

	pr_debug( "[BAT][Gau]%s(), ocv_per=%d, v=%d, c=%d\n",
		__func__, ocv_percent, volt, curr);

#ifdef CFG_REF_LAST_BAT_INFO
	bat_life = eval_bat_life_by_last_bat_info(this, ocv_percent);
#else
	bat_life = ocv_percent;
#endif /* CFG_REF_LAST_BAT_INFO */

	return bat_life;
}

/* cal_bat_life_for_resume - Calculate battery life for resume update (i.e. in 
 * SWGAUGE_BAT_UPDATE_RESUME) */
int cal_bat_life_for_resume(
	struct AXC_SWGauge *this)
{
	int volt;
	int curr;
	int ocv_percent=0;
	int bat_life=0;
	AXE_Charger_Type correct_charger_mode;

	assert(NULL != this);

	pr_debug( "[BAT][Gau]%s()\n", __func__);

	correct_charger_mode = swg_get_charger_mode(this);
	switch (correct_charger_mode) {
	case NO_CHARGER_TYPE:
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_RESUME_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		bat_life = eval_bat_life_by_charger_mode_before_suspend(this, ocv_percent);
		//bat_life = eval_bat_life_when_discharging(this, ocv_percent);
		break;
	case LOW_CURRENT_CHARGER_TYPE:
		volt = swg_discharging_to_read_volt(this, correct_charger_mode, TIMES_FOR_RESUME_READ_ADC);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		//bat_life = ocv_percent;
		bat_life = eval_bat_life_by_charger_mode_before_suspend(this, ocv_percent);
		break;
	case HIGH_CURRENT_CHARGER_TYPE:	
		volt = swg_discharging_to_read_volt(this, correct_charger_mode, TIMES_FOR_RESUME_READ_ADC);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		//bat_life = ocv_percent;
		bat_life = eval_bat_life_by_charger_mode_before_suspend(this, ocv_percent);
		break;
        default:
            break;
/*
	case TEMP_NO_CHARGER_TYPE:	///Eason_Chang///	
	    break; ///Eason_Chang///

    case ERROR_CHARGER:        ///Eason_Chang///
	    break;///Eason_Chang///	
*/

	}

	pr_debug( "[BAT][Gau]%s(), v=%d, c=%d, ocv_percent:%d\n",
		__func__, volt, curr, ocv_percent);

	return bat_life;
}

/* check_prev_volt_when_charging - Sometimes the voltage will drop when charging.
 * Ignore the unreasonable case. */
void check_prev_volt_when_charging(
	struct AXC_SWGauge *this,
	int *volt)
{
	///Eason_Chang///if (this->m_prev_volt_when_charging > volt ) {
	if (this->m_prev_volt_when_charging > *volt ) {  ///Eason_Chang///  
		pr_debug( "[BAT][Gau]Unreasonable!! previous volt more than current volt when charging.\n");
		*volt = this->m_prev_volt_when_charging;
	} else {
		this->m_prev_volt_when_charging = *volt;
	}

	return;
}

/* cal_bat_life_for_cycle - Calculate battery life for cycle update (i.e. in 
 * SWGAUGE_BAT_UPDATE_CYCLE) */
int cal_bat_life_for_cycle(
	struct AXC_SWGauge *this)
{
	int volt;
	int curr;
	///Eason_Chang///int finetune_ocv;
	int ocv_percent;
	int bat_life =0;
	AXE_Charger_Type correct_charger_mode;

	assert(NULL != this);

	correct_charger_mode = swg_get_charger_mode(this);
	switch(correct_charger_mode) {
	case NO_CHARGER_TYPE:
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		bat_life = eval_bat_life_when_discharging(this, ocv_percent);
#ifdef CFG_VF_TUNING
		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		bat_life = ocv_percent;
		bat_life = eval_bat_life_when_discharging(this, ocv_percent);
#endif /* CFG_VF_TUNING */
		break;
	case LOW_CURRENT_CHARGER_TYPE:
		volt = swg_discharging_to_read_volt(this, LOW_CURRENT_CHARGER_TYPE, TIMES_FOR_GENERAL_READ_ADC);
		check_prev_volt_when_charging(this, &volt);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		bat_life = eval_bat_life_when_charging(this, ocv_percent, eval_max_charging_current(LOW_CURRENT_CHARGER_TYPE));
// For test
		pr_debug( "[BAT][Gau]%s(), USB charging, direct volt ocv:%d, max_bat_life ocv:%d, diff vf:%d\n",
			__func__,
			volt,
			CurrOCVtoPerTbl[bat_life],
			volt - CurrOCVtoPerTbl[bat_life]);

#ifdef CFG_VF_TUNING
		finetune_ocv = get_finetune_ocv_for_vf(this, volt);
		finetune_ocv = volt;
		ocv_percent = AXC_SWGauge_LookupPercentageByOCV(finetune_ocv);
		bat_life = ocv_percent;
		bat_life = eval_bat_life_when_charging(this, ocv_percent, MAX_RAISE_USB_MAH);
#endif /* CFG_VF_TUNING */
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		volt = swg_discharging_to_read_volt(this, HIGH_CURRENT_CHARGER_TYPE, TIMES_FOR_GENERAL_READ_ADC);
		check_prev_volt_when_charging(this, &volt);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		bat_life = eval_bat_life_when_charging(this, ocv_percent, eval_max_charging_current(HIGH_CURRENT_CHARGER_TYPE));
// For test
		pr_debug( "[BAT][Gau]%s(), AC charging, direct volt ocv:%d, max_bat_life ocv:%d, diff vf:%d\n",
			__func__,
			volt,
			CurrOCVtoPerTbl[bat_life],
			volt - CurrOCVtoPerTbl[bat_life]);

#ifdef CFG_VF_TUNING
		finetune_ocv = get_finetune_ocv_for_vf(this, volt);
		finetune_ocv = volt;
		ocv_percent = AXC_SWGauge_LookupPercentageByOCV(finetune_ocv);
		bat_life = ocv_percent;
		bat_life = eval_bat_life_when_charging(this, ocv_percent, MAX_RAISE_AC_MAH);
#endif /* CFG_VF_TUNING */
		break;
	default:
		pr_debug( "[BAT][Gau]Error!!! Wrong charger state: %d in this state!", correct_charger_mode);
		break;	
	}


	return bat_life;
}

#ifdef CFG_VF_TUNING
/* cal_vf_tuning_ratio - Calculate VF tuning ratio. Made by the real voltage
 * difference and vf_to_percent_tbl[].
 */
void cal_vf_tuning_ratio(
	struct AXC_SWGauge *this)
{
	int orig_vf;
	int curr_vf;
	int percent;
	int read_average_times = 50;
	int volt;
	int curr;

	this->msParentGauge.ReadVoltCurr(
	&this->msParentGauge,
	read_average_times,
	&volt,
	&curr);
			
	percent = AXC_SWGauge_LookupPercentageByOCV(volt);
	curr_vf = volt - this->m_vf_ocv_base;
	orig_vf = vf_to_percent_tbl[percent];
	this->m_vf_tuning_ratio = curr_vf*10/orig_vf;

	pr_debug( "[BAT][Gau]%s(), volt:%d, ocv per:%d, vf_ocv_base:%d, curr_vf:%d, orig_vf:%d, vf_tuning_ratio:%d \n",
		__func__,
		volt,
		percent,
		this->m_vf_ocv_base,
		curr_vf,
		orig_vf,
		this->m_vf_tuning_ratio);

	return;
}

/* check_bat_update_state_for_vf - Check battery update state for VF tuning */
static void check_bat_update_state_for_vf(
	struct AXC_SWGauge *this)
{
	assert(NULL != this);

	if ((SWGAUGE_BAT_UPDATE_FIRST == swg_get_bat_update_state()) ||
		(SWGAUGE_BAT_UPDATE_CAL_VF == swg_get_bat_update_state())) {
		return;
	}

	if ((this->m_prev_update_charger_mode != this->mpCharger->GetChargerStatus(this->mpCharger))&&
		(NO_CHARGER_TYPE == this->m_prev_update_charger_mode)) {
		swg_set_bat_update_state(this, SWGAUGE_BAT_UPDATE_CAL_OCV);
	}

	return;
}
#endif /* CFG_VF_TUNING */

/* determine_keep_full_bat_life - determin if to keep full battery life by interval. */
static void determine_keep_full_bat_life(
	struct AXC_SWGauge *this)
{
	struct timespec mtNow;

	int interval;

	assert(NULL != this);

	mtNow = current_kernel_time();
	interval = mtNow.tv_sec - this->mtTimeWhenCablePluggedOutAnd100Percentage.tv_sec;
	pr_debug( "[BAT][Gau]%s(), Keep 100 Percentage, interval:%d\n",
		__func__,
		interval);
	if (interval < THE_TIME_TO_KEEP_100_PERCENTAGE) {
		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		NotifyBatteryObersever(this);
		queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , (THE_TIME_TO_KEEP_100_PERCENTAGE - interval)*HZ);
		this->mtLastUpdatedTime = current_kernel_time();
	} else {
		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		NotifyBatteryObersever(this);
		queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval)); 
		this->mtLastUpdatedTime = current_kernel_time();
	}
	return;
}

/* update_bat_info - the main function to update battery info, such as battery life.
 */
static void update_bat_info(
	struct AXC_SWGauge *this,
	enum swgauge_bat_update_state set_state)
{
	//int volt;
	//int curr;
	//int finetune_ocv;
	//int ocv_percent;
	int bat_life;
	//AXE_Charger_Type correct_charger_mode = ERROR_CHARGER;
	enum swgauge_bat_update_state bat_update_state;

	assert(NULL != this);

	pr_debug( "[BAT][Gau]%s()+++\n", __func__);

	mutex_lock(&this->swg_mutex_lock);

	wake_lock(&this->msReadingBatteryInfoWakeLock);

	if (this->mbKeep100Percentage) {
		determine_keep_full_bat_life(this);
		wake_unlock(&this->msReadingBatteryInfoWakeLock);
		mutex_unlock(&this->swg_mutex_lock);
		this->mbKeep100Percentage = false;
		return;
	}

	swg_set_bat_update_state(this, set_state);

	this->m_prev_bat_life_percent = this->mnBatteryLifePercentage;


#ifdef CFG_VF_TUNING
	check_bat_update_state_for_vf(this);
#endif /* CFG_VF_TUNING */

	bat_update_state = swg_get_bat_update_state();
	pr_debug( "[BAT][Gau]%s(), bat_update_state:%d, prev_update_charger_mode: %d\n",
		__func__,
		bat_update_state,
		this->m_prev_update_charger_mode);	

	switch(swg_get_bat_update_state()) {
	case SWGAUGE_BAT_UPDATE_FIRST:
		bat_life = cal_bat_life_for_first(this);
		this->m_truely_bat_life_percent = bat_life;
		this->mnBatteryLifePercentage = bat_life;
		update_bat_info_for_speical_case(this);
    	pr_debug( "[BAT][Gau]prev_bat_life_percent:%d, truely bat life:%d, update to OMS bat life:%d\n",
			this->m_prev_bat_life_percent,
			this->m_truely_bat_life_percent,
			this->mnBatteryLifePercentage);

		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		NotifyBatteryObersever(this);
		queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval));
		this->mtLastUpdatedTime = current_kernel_time();
		break;
	case SWGAUGE_BAT_UPDATE_CYCLE:
		bat_life = cal_bat_life_for_cycle(this);
		this->m_truely_bat_life_percent = bat_life;
		this->mnBatteryLifePercentage = bat_life;
		update_bat_info_for_speical_case(this);
    	pr_debug( "[BAT][Gau]prev_bat_life_percent:%d, truely bat life:%d, update to OMS bat life:%d\n",
			this->m_prev_bat_life_percent,
			this->m_truely_bat_life_percent,
			this->mnBatteryLifePercentage);
		
		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		NotifyBatteryObersever(this);
		queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval)); 
		this->mtLastUpdatedTime = current_kernel_time();
		break;
	case SWGAUGE_BAT_UPDATE_RESUME:
		bat_life = cal_bat_life_for_resume(this);
		this->m_truely_bat_life_percent = bat_life;
		this->mnBatteryLifePercentage = bat_life;
		update_bat_info_for_speical_case(this);
    	pr_debug( "[BAT][Gau]prev_bat_life_percent:%d, truely bat life:%d, update to OMS bat life:%d\n",
			this->m_prev_bat_life_percent,
			this->m_truely_bat_life_percent,
			this->mnBatteryLifePercentage);
		
		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		NotifyBatteryObersever(this);
		//queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval)); 
		this->mtLastUpdatedTime = current_kernel_time();
		break;
#ifdef CFG_VF_TUNING
	case SWGAUGE_BAT_UPDATE_CAL_OCV:
		volt = swg_discharging_to_read_volt(this, this->mpCharger->GetChargerStatus(this->mpCharger), TIMES_FOR_GENERAL_READ_ADC);
		this->m_vf_ocv_base = volt;
		queue_delayed_work(this->mpBatteryUpdateQueue, &this->msBatteryUpdateWorker, INTERVAL_TO_WAIT_VF);
		swg_set_bat_update_state(this, SWGAUGE_BAT_UPDATE_CAL_VF);
		break;
	case SWGAUGE_BAT_UPDATE_CAL_VF:
		if (this->mpCharger->IsCharging(this->mpCharger)) {
			cal_vf_tuning_ratio(this);
		}
		swg_set_bat_update_state(this, SWGAUGE_BAT_UPDATE_CAL_VF_DONE);
		queue_delayed_work(this->mpBatteryUpdateQueue, &this->msBatteryUpdateWorker, 0);
		break;
	case SWGAUGE_BAT_UPDATE_CAL_VF_DONE:
		printk(DBGMSK_BAT_ERR "[BAT][Gau] Should not go SWGAUGE_BAT_UPDATE_CAL_VF_DONE\n");
		break;
#endif /* CFG_VF_TUNING */
	default:
		printk(DBGMSK_BAT_ERR "[BAT][Gau]Error!!! Wrong update state: %d!!!\n", swg_get_bat_update_state());
		break;	
	}
 
	this->m_prev_update_charger_mode = this->mpCharger->GetChargerStatus(this->mpCharger);

	wake_unlock(&this->msReadingBatteryInfoWakeLock);

	mutex_unlock(&this->swg_mutex_lock);

// use vibrator for debug
/*
 	if (this->m_prev_bat_life_percent != this->mnBatteryLifePercentage) {
		pr_debug( "[BAT][Gau] Battery life displayed on UI changed\n");
		if (asus_bat_config_vibrator_debug()){
			if (asus_bat_meet_bat_life_interval_for_vibration()) {
				asus_bat_enable_vibrator();
			}
		}
	}
*/
	pr_debug( "[BAT][Gau]%s()---\n", __func__);
	return;
}
//joshtest


//Sampling Vbat twice with rate=1 sample/sec，get their mean
#if 0 //eason//
static void AXC_SWGauge_ReadAndUpdateBatteryInfo(AXC_SWGauge *this)
{

	static bool lbHasReadCalibrationData = false;

	static struct semaphore sem;

	int lnVBAT = 0;

	int lnCurrent = 0;

	int lnTemperature = 0;

	int lnTimesForGatherData = 50;

	wake_lock(&this->msReadingBatteryInfoWakeLock);

	if(false == lbHasReadCalibrationData){

		AXC_SWGauge_InitVoltageCurrnetCalibrationData(this);
	
		AXC_SWGauge_InitResistorCalibrationData(this);

		//init_MUTEX(&sem);

		sema_init(&sem, 1);

		lbHasReadCalibrationData = true;
	}

	down(&sem);

/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
#if 0
	lnVBAT = this->msParentGauge.ReadVoltage(&this->msParentGauge,lnTimesForGatherData);
	lnCurrent = this->msParentGauge.ReadCurrent(&this->msParentGauge,lnTimesForGatherData);
#endif
	this->msParentGauge.ReadVoltCurr(&this->msParentGauge, lnTimesForGatherData,
		&lnVBAT, &lnCurrent);
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */

	lnTemperature = this->msParentGauge.ReadTemperature(&this->msParentGauge);

	//printk("AXC_SWGauge_ReadAndUpdateBatteryInfoxx\n");

	AXC_SWGauge_CalculateBatteryLife(this, lnVBAT, lnCurrent, lnTemperature);

	NotifyBatteryObersever(this);

	AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
	queue_delayed_work(this->mpBatteryUpdateQueue,&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval)); 

	up(&sem);

	wake_unlock(&this->msReadingBatteryInfoWakeLock);
	//printk("AXC_SWGauge_ReadAndUpdateBatteryInfo--\n");

}
#endif

static void AXC_SWGauge_ChangeBatteryUpdatedInterval(AXC_SWGauge *this)
{
	AXE_Charger_Type charger_mode = NOTDEFINE_TYPE; 

	if (true == this->mbForceFixedUpdateInterval) {
		pr_debug( "[BAT][Gau]%s(), already set fixed update interval:%d\n", __func__, this->mnBatteryUpdateInterval/HZ);
		return;
	}

	if(this->m_bat_low) {
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_BAT_LOW_IRQ*HZ;
		pr_debug( "[BAT][Gau]%s(), bat low irq, update interval to: %d\n", __func__, UPDATE_INERVAL_BAT_LOW_IRQ);
		return;
	}

	charger_mode = this->mpCharger->GetChargerStatus(this->mpCharger);

	if ((ACCELERATE_UPDATE_INERVAL_BAT_LIFE >= this->m_truely_bat_life_percent) && 
		(NO_CHARGER_TYPE == charger_mode)) {
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_ACCELERATE*HZ;
		pr_debug( "[BAT][Gau]%s(), Accelerate update interval to: %d\n", __func__, UPDATE_INERVAL_ACCELERATE);
		return;
	}


	switch(charger_mode){
	case NO_CHARGER_TYPE:
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_NO_CHARGER_TYPE*HZ;
		pr_debug( "[BAT][Gau]%s(), mnBatteryUpdateInterval: %d\n", __func__, UPDATE_INERVAL_NO_CHARGER_TYPE);
		break;
	case LOW_CURRENT_CHARGER_TYPE:
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_LOW_CURRENT_CHARGER_TYPE*HZ;
		pr_debug( "[BAT][Gau]%s(), mnBatteryUpdateInterval: %d\n", __func__, UPDATE_INERVAL_LOW_CURRENT_CHARGER_TYPE);
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_HIGH_CURRENT_CHARGER_TYPE*HZ;
		pr_debug( "[BAT][Gau]%s(), mnBatteryUpdateInterval: %d\n", __func__, UPDATE_INERVAL_HIGH_CURRENT_CHARGER_TYPE);
		break;
	default:
		pr_debug( "[BAT]Error!!! Wrong charger mode in this state!");
		this->mnBatteryUpdateInterval = UPDATE_INERVAL_DEFAULT*HZ;
		break;
	}
	return;
/*
	if(false == this->mpCharger->IsCharegrPlugin(this->mpCharger))
    {
		if( anTemperature > 34 )		this->mnBatteryUpdateInterval = 95*HZ;
		else if( anTemperature > 23 )	this->mnBatteryUpdateInterval = 100*HZ;
		else if( anTemperature > 12 )   this->mnBatteryUpdateInterval = 105*HZ;
		else					        this->mnBatteryUpdateInterval = 110*HZ;
	}
    else
    {
		this->mnBatteryUpdateInterval = 120*HZ;
		if( anTemperature > 34 )        this->mnBatteryUpdateInterval = 45*HZ;
		else if( anTemperature > 23 )   this->mnBatteryUpdateInterval = 55*HZ;
		else if( anTemperature > 12 )   this->mnBatteryUpdateInterval = 60*HZ;
		else                            this->mnBatteryUpdateInterval = 65*HZ;
	}
*/
/*add 5 ms?? 
    if( anCurrent < 50 )       this->mnBatteryUpdateInterval += 5;
    else if( anCurrent < 200 ) this->mnBatteryUpdateInterval += 3;
    else if( anCurrent < 350 ) this->mnBatteryUpdateInterval += 0;
    else if( anCurrent < 500 ) this->mnBatteryUpdateInterval -= 1;	
*/
}

//ASUS BSP Eason_Chang+++  SWgauge resume worker
static void calculate_resume_capacity(struct work_struct *dat)
{
    AXC_SWGauge *this = container_of(dat, AXC_SWGauge, resumeBatteryUpdateWorker.work);
	
   	update_bat_info(this, SWGAUGE_BAT_UPDATE_RESUME);
}
//ASUS BSP Eason_Chang---  SWgauge resume worker

#ifndef SAMPLE_THREAD_ENABLE
static void AXC_SWGauge_Sample(AXC_SWGauge *this)
{
// joshtest
//	AXC_SWGauge_ReadAndUpdateBatteryInfo(this);
	update_bat_info(this, SWGAUGE_BAT_UPDATE_CYCLE);
// joshtest
}
#else
static void AXC_SWGauge_Sample(struct work_struct *dat)
{
    AXC_SWGauge *this = container_of(dat, AXC_SWGauge, msSampleWorker.work);

	int lnVoltage = 0;
	int lnCurrent = 0;
	int lnTemperature = 0;
	int lnTimesForGatherData=5;
	int i;

	for(i = 0; i < lnTimesForGatherData ; i++)
    {
/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
#if 0
		lnVoltage = this->msParentGauge.ReadVoltage(&this->msParentGauge,1);
		lnCurrent = this->msParentGauge.ReadCurrent(&this->msParentGauge,1);
#endif
		this->msParentGauge.ReadVoltCurr(&this->msParentGauge, 1,
			&lnVoltage, &lnCurrent);		
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */
		lnTemperature = this->msParentGauge.ReadTemperature(&this->msParentGauge);
		this->mpVoltageSampleData->AddSample(this->mpVoltageSampleData, (void *)&lnVoltage);
		this->mpCurrentSampleData->AddSample(this->mpCurrentSampleData, (void *)&lnCurrent);
		this->mpTemperatureSampleData->AddSample(this->mpTemperatureSampleData, (void *)&lnTemperature);
    
        pr_debug( "[BAT]lnVoltage:%d, lnCurrent:%d, lnTemperature:%d\n", lnVoltage, lnCurrent, lnTemperature);
	}

	if(true == this->mbNeedtoUpdateBatteryLifePercentage)
    {
		this->mbNeedtoUpdateBatteryLifePercentage = false;
		AXC_SWGauge_CalculateBatteryLife(this,
						this->mpVoltageSampleData->GetAverage(this->mpVoltageSampleData),
						this->mpCurrentSampleData->GetAverage(this->mpCurrentSampleData),
						this->mpTemperatureSampleData->GetAverage(this->mpTemperatureSampleData));

		NotifyBatteryObersever(this);

		AXC_SWGauge_ChangeBatteryUpdatedInterval(this);
		schedule_delayed_work(&this->msBatteryUpdateWorker , round_jiffies_relative(this->mnBatteryUpdateInterval));
	}

	schedule_delayed_work(&this->msSampleWorker, round_jiffies_relative(this->mnSampleInterval));
}
#endif

static void AXC_SWGauge_NotifyForBatteryLifePercentageChanged(struct work_struct *dat)
{
    AXC_SWGauge *this = container_of(dat, AXC_SWGauge, msBatteryUpdateWorker.work);
#ifdef SAMPLE_THREAD_ENABLE
	this->mbNeedtoUpdateBatteryLifePercentage = true;	
#else
	AXC_SWGauge_Sample(this);
#endif
}

#if 0 ///Eason_Chang///
static void AXC_SWGauge_AutoCreateCalibrationData(AXC_SWGauge *this)
{
	struct file *cfile;
	mm_segment_t orgfs;
	char delta_t_str[4];

	loff_t offset = 0;
	mytemp lnFakeVoltageCalibrationData,lnFakeCurrentCalibrationData;
	lnFakeVoltageCalibrationData.mnValue = FAKE_VOLTAGE_CALIBRATION;
	lnFakeCurrentCalibrationData.mnValue = FAKE_CURRENT_CALIBRATION;
	pr_debug( "[BAT]AXC_SWGauge_AutoCreateCalibrationData");
	pr_debug( "[BAT]Voltage:0x%x,0x%x,0x%x\n",
		lnFakeVoltageCalibrationData.mnValue,
		lnFakeVoltageCalibrationData.mnChar[0],
		lnFakeVoltageCalibrationData.mnChar[1]);
	pr_debug( "[BAT]Current:0x%x,0x%x,0x%x\n",
		lnFakeCurrentCalibrationData.mnValue,
		lnFakeCurrentCalibrationData.mnChar[0],
		lnFakeCurrentCalibrationData.mnChar[1]);

#if 1
    orgfs=get_fs();
    set_fs(KERNEL_DS);
    cfile = filp_open(ANDROID_BATTERY_CALIBRATION_FILE_PATH, O_CREAT | O_RDWR | O_LARGEFILE, 0600);
    if (!IS_ERR(cfile))
    {
        delta_t_str[0]=lnFakeVoltageCalibrationData.mnChar[0];
        delta_t_str[1]=lnFakeVoltageCalibrationData.mnChar[1];
		delta_t_str[2]=lnFakeCurrentCalibrationData.mnChar[0];
		delta_t_str[3]=lnFakeCurrentCalibrationData.mnChar[1];
    	vfs_write(cfile, delta_t_str, 4, &offset);
    }
	//filp_close(cfile,NULL);
    set_fs(orgfs);
#endif
}
#endif  ///Eason_Chang///
static void AXC_SWGauge_InitVoltageCurrnetCalibrationData(AXC_SWGauge *this)
{
	struct file *fd;
	mm_segment_t mmseg_fs;
	int FileLength = 0;

	mytemp lnVoltageCalibrationData,lnCurrentCalibrationData;

#if 1
	mmseg_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = filp_open( BATTERY_CALIBRATION_FILE_PATH, O_RDONLY,0);
	if (IS_ERR(fd))
    {
		pr_debug( "[BAT][Gau]%s: did not find battery cali file, use default value.\n",__FUNCTION__);
		return;
	}

	FileLength =    fd->f_op->read(fd, lnVoltageCalibrationData.mnChar, 2, &fd->f_pos);
	FileLength +=   fd->f_op->read(fd, lnCurrentCalibrationData.mnChar, 2, &fd->f_pos);

	assert(4 == FileLength);
	if(4 <= FileLength)
    {
		this->mnVoltageCalibration = lnVoltageCalibrationData.mnValue;
		this->mnCurrentCalibration = lnCurrentCalibrationData.mnValue;

		pr_debug( "[BAT][Gau]Voltage Calibration:%d,Current Calibration:%d\n", this->mnVoltageCalibration, this->mnCurrentCalibration);
	}

	if(500 < this->mnVoltageCalibration || -500 > this->mnVoltageCalibration)
    {
		pr_debug( "[BAT][Gau]Calibration data out of range, use no calibration\n");
		this->mnVoltageCalibration = 0;
		this->mnCurrentCalibration = 0;
	}

	set_fs(mmseg_fs);
#endif
}

static void AXC_SWGauge_CreateResistorCalibrationData(AXC_SWGauge *this ,int lnResisterValue)
{
	struct file *cfile;
	mm_segment_t orgfs;
	char delta_t_str[4];

//	loff_t offset = 0;
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

static void AXC_SWGauge_InitResistorCalibrationData(AXC_SWGauge *this)
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
		printk(DBGMSK_BAT_ERR "[BAT][Gau]%s: did not find battery Resistor cali file, use default value.\n",__FUNCTION__);
		this->mnResistorCalibration = DEFAULT_DEVICE_RESISTOR_VALUE;
		return;
	}

	FileLength = fd->f_op->read(fd, lnResistorCalibrationData.mnChar, 2, &fd->f_pos);
	assert(2 == FileLength);
	if(2 <= FileLength)
	{
		this->mnResistorCalibration = lnResistorCalibrationData.mnValue;
		pr_debug( "[BAT][Gau]R-Cali:%d\n", this->mnResistorCalibration);
	}
	
	set_fs(mmseg_fs);

      filp_close(fd, NULL);
  	return;
}
#if 0 ///Eason_Chang///
static int AXC_SWGauge_StopMonitorBatteryInfo(AXC_SWGauge *this )
{
#ifdef SAMPLE_THREAD_ENABLE
	cancel_delayed_work_sync(&this->msSampleWorker);
#endif	
	cancel_delayed_work_sync(&this->msBatteryUpdateWorker);
	cancel_delayed_work_sync(&this->msDelayAfterBootWorker);
#ifdef SAMPLE_THREAD_ENABLE
	AXC_SWGauge_ClearSampleData(this);
#endif	
	return 0;
}
#endif  ///Eason_Chang///
#if 0 ///Eason_Chang///
static int AXC_SWGauge_FlushAndReportCurrnetBatteryInfo(AXC_SWGauge *this)
{


	return 0;
}
#endif  ///Eason_Chang///
static bool gbChargerStatusBeforeSuspend = false;//no plugged in

static int AXC_SWGauge_Resume(struct AXI_Gauge *apGauge)
{

	AXC_SWGauge *this = container_of(apGauge,
											AXC_SWGauge,
											msParentGauge);
    pr_debug("[BAT][Gau]SWresume+++\n");
	if(gbChargerStatusBeforeSuspend != this->mpCharger->IsCharegrPlugin(this->mpCharger)){


		this->mbISChargerStatusChangedAfterResume = true;
	}
//ASUS BSP Eason_Chang+++  SWgauge resume worker
/*
// joshtest
//    AXC_SWGauge_ReadAndUpdateBatteryInfo(this);
	update_bat_info(this, SWGAUGE_BAT_UPDATE_RESUME);
// joshtest
*/

    queue_delayed_work(this->resBatteryUpdateQueue,&this->resumeBatteryUpdateWorker , 0);
//ASUS BSP Eason_Chang+++  SWgauge resume worker

	//AXC_SWGauge_SetupStartWorking(this,DALAY_SAMPLING_AFTER_RESUME);
    pr_debug("[BAT][Gau]SWresume---\n");
	return 0;

	//return AXC_SWGauge_FlushAndReportCurrnetBatteryInfo(this);

}

static int AXC_SWGauge_Suspend(struct AXI_Gauge *apGauge)
{
	AXC_SWGauge *this = container_of(apGauge,
											AXC_SWGauge,
											msParentGauge);
	gbChargerStatusBeforeSuspend = this->mpCharger->IsCharegrPlugin(this->mpCharger);

	this->m_charger_mode_before_suspend = this->mpCharger->GetChargerStatus(this->mpCharger);

	this->m_jiffies_of_suspend = jiffies;

	pr_debug( "[BAT][Gau]charger_mode_before_suspend: %d\n", this->m_charger_mode_before_suspend);

	//return AXC_SWGauge_StopMonitorBatteryInfo(this);
	return 0;
}

static int AXC_SWGauge_ReCalculateResistorAfter100PercentageAndCablePlugged(struct AXC_SWGauge *this)
{
	int lnVoltageWhenCablePlugged, lnCurrentWhenCablePlugged;
	int lnTimesForGatherData = TIMES_FOR_GENERAL_READ_ADC;

	pr_debug( "[BAT][Gau] UpdateBattery, %s(), 100 percent ocv=%d\n",
		__func__,
		this->mn100PercentageOCV);

	if(0 != this->mn100PercentageOCV)
    {
/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
#if 0
		lnVoltageWhenCablePlugged = this->msParentGauge.ReadVoltage(&this->msParentGauge,lnTimesForGatherData);
		lnCurrentWhenCablePlugged = this->msParentGauge.ReadCurrent(&this->msParentGauge,lnTimesForGatherData);
#endif
		this->msParentGauge.ReadVoltCurr(&this->msParentGauge, lnTimesForGatherData,
			&lnVoltageWhenCablePlugged, &lnCurrentWhenCablePlugged);
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */
		pr_debug( "[BAT][Gau]V When Cable out=%d\n",lnVoltageWhenCablePlugged);
		pr_debug( "[BAT][Gau]C When Cable out=%d\n",lnCurrentWhenCablePlugged);

		assert(this->mn100PercentageOCV > lnVoltageWhenCablePlugged);
		assert(lnCurrentWhenCablePlugged != 0);
		if(this->mn100PercentageOCV > lnVoltageWhenCablePlugged && lnCurrentWhenCablePlugged != 0)
        {
			this->mnResistorCalibration = (this->mn100PercentageOCV - lnVoltageWhenCablePlugged)*1000/lnCurrentWhenCablePlugged;
			pr_debug( "[BAT][Gau]Reset Orignal Resistor: %d\n",this->mnResistorCalibration);

			if (this->mnResistorCalibration > RESISTOR_MAX_VAL) {
				this->mnResistorCalibration = RESISTOR_MAX_VAL;
			} else if (this->mnResistorCalibration < RESISTOR_MIN_VAL) {
				this->mnResistorCalibration = RESISTOR_MIN_VAL;
			}

			pr_debug( "[BAT][Gau]Reset Tuning Resistor: %d\n",this->mnResistorCalibration);
			AXC_SWGauge_CreateResistorCalibrationData(this,this->mnResistorCalibration);
			this->mn100PercentageOCV = 0;//Never need it...
		}
	}
    else
    {
		//Just happens fast plug-in before Next  this->mn100PercentageOCV has been updated..
		pr_debug( "[BAT]Not Need To Update Resistor\n");
	}

	return 0;
}

static int AXC_SWGauge_OnCableStatueChanged(struct AXI_Gauge *apGauge)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

    pr_debug( "[BAT][Gau] truely bat life percent:%d, charegr status:%d (in or out)\n",
		this->m_truely_bat_life_percent,
		this->mpCharger->IsCharegrPlugin(this->mpCharger));

	if(100 == this->mnBatteryLifePercentage && false == this->mpCharger->IsCharegrPlugin(this->mpCharger))
    {
		this->mbKeep100Percentage = true;
		AXC_SWGauge_ReCalculateResistorAfter100PercentageAndCablePlugged(this);
		this->mtTimeWhenCablePluggedOutAnd100Percentage = current_kernel_time();
	}

	return 0;
}

static int AXC_SWGauge_SetFixedUpdateInterval(struct AXI_Gauge *apGauge,unsigned int anInterval)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

	if(0 == anInterval) {
		this->mbForceFixedUpdateInterval = false;
	} else {   
		this->mbForceFixedUpdateInterval = true;
		this->mnBatteryUpdateInterval = anInterval*HZ;
	}

	return 0;
}
static int AXC_SWGauge_IsBatteryRemoved(struct AXI_Gauge *apGauge )
{
	//AXC_SWGauge *this = container_of(apGauge,
	//										AXC_SWGauge,
	//										msParentGauge);

	if(2500 > apGauge->ReadVoltage(apGauge,1)){

		return 1;
	}
	
	return 0;

}

/* For querying calibration data */
static int AXC_SWGauge_GetVoltageCalibrationVal(struct AXI_Gauge *apGauge )
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->mnVoltageCalibration;
}
static int AXC_SWGauge_GetCurrentCalibrationVal(struct AXI_Gauge *apGauge )
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->mnCurrentCalibration;
}
static int AXC_SWGauge_GetResistorCalibrationVal(struct AXI_Gauge *apGauge )
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->mnResistorCalibration;
}


static void AXC_SWGauge_FirstTimeToReadBatteryInfo(struct work_struct *dat)
{
        AXC_SWGauge *this = container_of(dat,
                                                AXC_SWGauge,
                                                msDelayAfterBootWorker.work);
// joshtest
//	AXC_SWGauge_ReadAndUpdateBatteryInfo(this);
	update_bat_info(this, SWGAUGE_BAT_UPDATE_FIRST);
// joshtest
}
static void AXC_SWGauge_DelayFirstTimeToReadBatteryInfoAfterFileSystemInited(AXC_SWGauge *this,int anDelay)
{
	
	this->mnDelayAfterBoot = anDelay;

	//schedule_delayed_work(&this->msDelayAfterBootWorker, round_jiffies_relative(this->mnDelayAfterBoot)); 

}

static void AXC_SWGauge_InitWorker(AXC_SWGauge *this)
{

	//Init Dalay Worker
	static bool lbWorkerInited = false;

	if(false == lbWorkerInited){

		INIT_DELAYED_WORK(&this->msDelayAfterBootWorker, AXC_SWGauge_FirstTimeToReadBatteryInfo) ;

#ifdef SAMPLE_THREAD_ENABLE
		
			//Init sampling worker
		INIT_DELAYED_WORK(&this->msSampleWorker, AXC_SWGauge_Sample) ;
						
#endif		

		this->mpBatteryUpdateQueue = create_singlethread_workqueue("BatteryUpdateWorker");
			//Init Updating worker

		pr_debug( "[BAT][Gau]INIT_DELAYED_WORK, AXC_SWGauge_NotifyForBatteryLifePercentageChanged\n");
		INIT_DELAYED_WORK(&this->msBatteryUpdateWorker, AXC_SWGauge_NotifyForBatteryLifePercentageChanged) ;

        
//ASUS BSP Eason_Chang+++  SWgauge resume worker
        this->resBatteryUpdateQueue = create_singlethread_workqueue("resumeBatteryUpdateWorker");
        INIT_DELAYED_WORK(&this->resumeBatteryUpdateWorker, calculate_resume_capacity) ;
//ASUS BSP Eason_Chang---  SWgauge resume worker

		lbWorkerInited = true;
	}

}

//Function implementation for AXC_SWGauge
static void AXC_SWGauge_Init(AXI_Gauge *apGauge ,AXI_Charger *apCharger,AXI_GaugeResultNotifier *apNotifier)
{
        AXC_SWGauge *this = container_of(apGauge,
                                                AXC_SWGauge,
                                                msParentGauge);

	//printk("AXC_SWGauge_Init\n");

	if(false == this->mbInited){
		
		AXC_SWGauge_InitGADC();

		if(NULL != apCharger){

			this->mpCharger = apCharger;

			//gnBatteryPercentage = AXC_SWGauge_GetBatteryLife(apGauge, 
			//						this->mpCharger->IsCharegrPlugin());		
			//this->mpCharger->Init(this->mpCharger);
		}

		AXC_SWGauge_InitWorker(this);

		AXC_SWGauge_DelayFirstTimeToReadBatteryInfoAfterFileSystemInited(this,DALAY_SAMPLING_AFTER_BOOT_UP);

// joshtest
		spin_lock_init(&g_bat_update_state_lock);
		mutex_init(&this->swg_mutex_lock);
// joshtest

		this->mbNeedtoUpdateBatteryLifePercentage = false;

		this->mpNotifier = apNotifier;

		wake_lock_init(&this->msReadingBatteryInfoWakeLock, WAKE_LOCK_SUSPEND, "SWGauge");

		this->mbInited = true;
	}

}

static void AXC_SWGauge_DeInit(AXI_Gauge *apGauge)
{
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
    if(true == this->mbInited)
    {
#ifdef SAMPLE_THREAD_ENABLE
		AXC_SWGauge_DeInitSampleData(this);
#endif
        this->mbInited = false;
    }
}

int AXC_SWGauge_GetType(AXI_Gauge *apGauge)
{
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
    return this->mnType;
}

void AXC_SWGauge_SetType(AXI_Gauge *apGauge ,int anType)
{
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
    this->mnType = anType;
}

#define TIMES_FOR_READ_BATID	(10)

#if 0 ///Eason_Chang///	
static int AXC_SWGauge_ReadBatIdFromADC(AXI_Gauge *apGauge)
{
	//int i, tmp_val, total_val, ave_val;
	int tmp_val;
	u8 adc_val[2];

	tmp_val = 0;
	///Eason_Chang///asus_pmic_bulk_read(PM8607_GPADC0_MEAS1, 2, adc_val);
	tmp_val = (adc_val[0] << 4) + (adc_val[1] & 0xf);

	printk(KERN_INFO "[BAT][Gau] %s(), ADC val:%d, ADC calc val:%d\n",
		__func__,
		tmp_val,
		AXC_SWGauge_CalBatIdFromADC(tmp_val));

	return (AXC_SWGauge_CalBatIdFromADC(tmp_val));	
}
#endif  ///Eason_Chang///

#define BAT_MAIN_SOURCE_VOLT (1200)
#define BAT_SECOND_SOURCE_VOLT (980)

/* AXC_SWGauge_SetConfig - set battery config. determine which ocv tble used by
 * read battery id. The battery id is voltage. And it will also set flag to
 * indicate if battery is valid.
 */
static int AXC_SWGauge_SetConfig(AXI_Gauge *apGauge)
{
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	
	CurrOCVtoPerTbl = MainSrcOCVtoPerTbl;
	
	this->m_is_bat_valid = true;
	
/*
	//int bat_id, diff_volt;
	int bat_id;

	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	
	bat_id = AXC_SWGauge_ReadBatIdFromADC(apGauge);
	if ((-50 < (bat_id - BAT_MAIN_SOURCE_VOLT)) &&
		((bat_id - BAT_MAIN_SOURCE_VOLT) < 50)) {
		CurrOCVtoPerTbl = MainSrcOCVtoPerTbl;
		this->m_is_bat_valid = true;
		printk(KERN_INFO "[BAT][Gau] %s(), set main source battery ocv tbl, bat_id:%d\n",
			__func__,
			bat_id);
	} else if ((-50 < (bat_id - BAT_SECOND_SOURCE_VOLT)) &&
				((bat_id - BAT_SECOND_SOURCE_VOLT) < 50)){
		CurrOCVtoPerTbl = SecondSrcOCVtoPerTbl;
		this->m_is_bat_valid = true;
		printk(KERN_INFO "[BAT][Gau] %s(), set second source battery ocv tbl, bat_id:%d\n",
			__func__,
			bat_id);
	} else {
		CurrOCVtoPerTbl = MainSrcOCVtoPerTbl;
		this->m_is_bat_valid = false;
		printk(KERN_INFO "[BAT][Gau] %s(), the battery is invalid and set main source battery ocv tbl, bat_id:%d\n",
			__func__,
			bat_id);
	}
*/
	return 0;	
}

static bool AXC_SWGauge_IsBatValid(AXI_Gauge *apGauge)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	printk(KERN_INFO "[BAT] %s()\n", __func__);
	return this->m_is_bat_valid;	
}

///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#ifdef EASON_CHANGE
static int AXC_SWGauge_ReadOneTimeVoltageFromADC(AXI_Gauge *apGauge)
{
    return get_voltage_for_ASUSswgauge();
}
#else
static int AXC_SWGauge_ReadOneTimeVoltageFromADC(AXI_Gauge *apGauge)
{
	int tmp, value;
    asus_pmic_reg_read(PM8607_GPADC3_MEAS2, &tmp);
	value = tmp & 0xf;
    asus_pmic_reg_read(PM8607_GPADC3_MEAS1, &tmp);
	value += tmp << 4;
    return (AXC_SWGauge_CalculateRealVoltageFromADC(value));
}
#endif
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */

#define TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC (1)

/* AXC_SWGauge_ReadVoltageCurrentFromADC - To get voltage and current from data
 * simultaneously for precision. 0x79, 0x7a are current value address, and 0x7b,
 * 0x7c are voltage value address. And SANREMO_GPADC2_MEAS1 is 0x79.
 */
 
///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#ifdef EASON_CHANGE
static void AXC_SWGauge_ReadVoltageCurrentFromADC(AXI_Gauge *apGauge, int *volt, int *curr)
{
printk("++++enter AXC_SWGauge_ReadVoltageCurrentFromADC/n");
	*curr = get_current_for_ASUSswgauge();
 	*volt = get_voltage_for_ASUSswgauge();
 	return;
}
#else
static void AXC_SWGauge_ReadVoltageCurrentFromADC(AXI_Gauge *apGauge, int *volt, int *curr)
{
	u8 adc_val[4];
	int tmp;
	asus_pmic_bulk_read(PM8607_GPADC2_MEAS1, 4, adc_val);
	tmp = (adc_val[0] << 4) + (adc_val[1] & 0xf);
	*curr = AXC_SWGauge_CalculateRealCurrentFromADC(tmp);
	tmp = (adc_val[2] << 4) + (adc_val[3] & 0xf);
 	*volt = AXC_SWGauge_CalculateRealVoltageFromADC(tmp);
 	return;
}
#endif
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

/* AXC_SWGauge_ReadVoltCurrWithoutCali - To read voltage and current without
 * calibration for several times.
 */
static void AXC_SWGauge_ReadVoltCurrWithoutCali(
	AXI_Gauge *apGauge,
	unsigned int anTimes,
	int *volt,
	int *curr)
{
	int i;
	int tmp_volt;
	int tmp_curr;
	static struct semaphore sem;
	static AXI_SampleData *lpVoltageSampleData = NULL;
	static AXI_SampleData *lpCurrentSampleData = NULL;
    int volArray[anTimes]; //Eason print VC
    int currArray[anTimes]; //Eason print VC

	assert(anTimes > 0);
	assert(NULL != volt);
	assert(NULL != curr);

//	printk(KERN_INFO "[BAT] %s()+++\n", __func__);

	if(NULL == lpCurrentSampleData){
		AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE,
						&lpVoltageSampleData);
		lpVoltageSampleData->Init(lpVoltageSampleData);

		AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE,
						&lpCurrentSampleData);
		lpCurrentSampleData->Init(lpCurrentSampleData);
		sema_init(&sem,1);
	}

	down(&sem);

	for(i = 0; i < anTimes; i++)
	{
		AXC_SWGauge_ReadVoltageCurrentFromADC(apGauge, &tmp_volt, &tmp_curr);
		lpVoltageSampleData->AddSample(lpVoltageSampleData, (void *)&tmp_volt);
		lpCurrentSampleData->AddSample(lpCurrentSampleData, (void *)&tmp_curr);
        volArray[i]=tmp_volt; //Eason print VC
        currArray[i]=tmp_curr; //Eason print VC
		if((TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC -1) == i % TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC){
			if (!in_atomic() || !irqs_disabled()) {
//				if (SWGAUGE_BAT_UPDATE_RESUME == swg_get_bat_update_state()) {
//					msleep(0);
//				} else {
					msleep(2);
//				}
			} else {
				printk(KERN_INFO "[BAT][Gau] not do msleep");
			}
		}
	}
    //Eason print VC +++
    printk("[BAT][Gau]:");
        for(i = 0; i < anTimes; i++){ 
        	printk("%d,%d",volArray[i],currArray[i]);
    }
    printk("\n");
    //Eason print VC ---

	*volt = lpVoltageSampleData->GetAverage(lpVoltageSampleData);
	lpVoltageSampleData->Reset(lpVoltageSampleData);

	*curr = lpCurrentSampleData->GetAverage(lpCurrentSampleData);
	lpCurrentSampleData->Reset(lpCurrentSampleData);

	up(&sem);

//	printk(KERN_INFO "[BAT] %s()---\n", __func__);

	return;
}

// joshtest
/* AXC_SWGauge_UpdateBatInfo - To update battery info.
 */
static void AXC_SWGauge_UpdateBatInfo(
	AXI_Gauge *apGauge,
	enum swgauge_bat_update_state set_state)
{
	//assert(NULL != apGauge);

	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

	update_bat_info(this, set_state);

	return;
}

/* AXC_SWGauge_SetBatLow - To set bat low status.
 */
static void AXC_SWGauge_SetBatLow(
	AXI_Gauge *apGauge,
	bool is_bat_low)
{
	//assert(NULL != apGauge);

	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

	this->m_bat_low = is_bat_low;

	printk(KERN_INFO "[BAT][Gau] %s(), set as:%d\n",
		__func__,
		is_bat_low);
	return;
}

/* AXC_SWGauge_GetBatLow - To get bat low status.
 */
static bool AXC_SWGauge_GetBatLow(
	AXI_Gauge *apGauge)
{
	//assert(NULL != apGauge);

	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

	pr_debug( "[BAT][Gau] %s(), get as:%d\n",
		__func__,
		this->m_bat_low);
	return this->m_bat_low;
}

#ifdef CFG_CHECK_BAT_LOW_DETECTOR

/* AXC_SWGauge_SetBatLowDetector - To set bat low detector flag as true or false.
 */
static void AXC_SWGauge_SetBatLowDetector(
	AXI_Gauge *apGauge,
	bool has_bat_low_detector)
{
	//assert(NULL != apGauge);

	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

	pr_debug( "[BAT][Gau] %s()\n", __func__);

	this->m_has_bat_low_detector = has_bat_low_detector;
	return;
}


/* AXC_SWGauge_GetBatLowDetector - To get bat low detector flag.
 */
static bool AXC_SWGauge_GetBatLowDetector(
	AXI_Gauge *apGauge)
{
	//assert(NULL != apGauge);

	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

	pr_debug( "[BAT][Gau] %s()\n", __func__);

	return this->m_has_bat_low_detector;
}
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */

/* AXC_SWGauge_ReadVoltCurr - To read voltage and current with
 * calibration for several times.
 */
static void AXC_SWGauge_ReadVoltCurr(
	AXI_Gauge *apGauge,
	unsigned int anTimes,
	int *volt,
	int *curr)
{
	AXC_SWGauge *this = container_of(
		apGauge,
		AXC_SWGauge,
		msParentGauge);

//	printk(KERN_INFO "[BAT] %s()+++\n", __func__);

	AXC_SWGauge_ReadVoltCurrWithoutCali(apGauge, anTimes, volt, curr);
/* ASUS_BPS +++ Josh_Liao "add get prop func" */
	this->m_prop_volt_without_cali = *volt;
	this->m_prop_curr_without_cali = *curr;
/* ASUS_BPS --- Josh_Liao "add get prop func" */

	*volt += this->mnVoltageCalibration;
	*curr += this->mnCurrentCalibration;

/* ASUS_BPS +++ Josh_Liao "add get prop func" */
	this->m_prop_volt = *volt;
	this->m_prop_curr = *curr;
/* ASUS_BPS --- Josh_Liao "add get prop func" */

//	printk(KERN_INFO "[BAT] %s()---\n", __func__);
	return;
}

/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */

static int AXC_SWGauge_ReadVoltageWithoutCalibration(AXI_Gauge *apGauge, unsigned int anTimes)
{
	int i, lnValue;

	static struct semaphore sem;
	static AXI_SampleData *lpVoltageSampleData = NULL;

	assert(anTimes > 0);
	if(anTimes == 0) {
		anTimes = 1;
  }

	if(NULL == lpVoltageSampleData)
    {
		AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE, &lpVoltageSampleData);
		lpVoltageSampleData->Init(lpVoltageSampleData);
		sema_init(&sem,1);
	}
	down(&sem);

	for(i = 0; i < anTimes; i++) {
		lnValue = AXC_SWGauge_ReadOneTimeVoltageFromADC(apGauge);
		lpVoltageSampleData->AddSample(lpVoltageSampleData, (void *)&lnValue);
		if((TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC -1) == i % TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC) {
			if (!in_atomic() || !irqs_disabled()) {
//				if (SWGAUGE_BAT_UPDATE_RESUME == swg_get_bat_update_state()) {
//					msleep(0);
//				} else {
					msleep(2);
//				}
            }
	    }
    }
	lnValue = lpVoltageSampleData->GetAverage(lpVoltageSampleData);
	lpVoltageSampleData->Reset(lpVoltageSampleData);
	up(&sem);

	return (lnValue);	
}

static int AXC_SWGauge_ReadVoltage(AXI_Gauge *apGauge,unsigned int anTimes)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	int lnVoltageWithoutCalibration = AXC_SWGauge_ReadVoltageWithoutCalibration(apGauge,anTimes);

	lnVoltageWithoutCalibration += this->mnVoltageCalibration;
	return lnVoltageWithoutCalibration;
}

///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#ifdef EASON_CHANGE
static int AXC_SWGauge_ReadOneTimeCurrentFromADC(AXI_Gauge *apGauge)
{
printk("++++enter AXC_SWGauge_ReadOneTimeCurrentFromADC/n");
	return get_current_for_ASUSswgauge();
}
#else
static int AXC_SWGauge_ReadOneTimeCurrentFromADC(AXI_Gauge *apGauge)
{
    int tmp, value;

    asus_pmic_reg_read(PM8607_GPADC2_MEAS2, &tmp);
	value = tmp & 0xf;

	asus_pmic_reg_read(PM8607_GPADC2_MEAS1, &tmp);
	value += tmp << 4;

	return (AXC_SWGauge_CalculateRealCurrentFromADC(value));
}
#endif
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

static int AXC_SWGauge_ReadCurrentWithoutCalibration(AXI_Gauge *apGauge,unsigned int anTimes)
{
	int i, lnValue;
	static struct semaphore sem;
	static AXI_SampleData *lpCurrentSampleData = NULL;

	assert(anTimes > 0);
	if(anTimes == 0)
		anTimes = 1;

	if(NULL == lpCurrentSampleData)
    {
		AXC_SampleDataFactory_GetSampleData(E_INT_SAMPLE_DATA_TYPE, &lpCurrentSampleData);
		lpCurrentSampleData->Init(lpCurrentSampleData);
		sema_init(&sem,1);
	}

	down(&sem);
	for(i = 0; i < anTimes; i++) {
		lnValue = AXC_SWGauge_ReadOneTimeCurrentFromADC(apGauge);
		lpCurrentSampleData->AddSample(lpCurrentSampleData, (void *)&lnValue);        
		if((TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC -1) == i % TIMES_FOR_TAKING_A_BREAK_FOR_READ_ADC) {
			if (!in_atomic() || !irqs_disabled()) {
//				if (SWGAUGE_BAT_UPDATE_RESUME == swg_get_bat_update_state()) {
//					msleep(0);
//				} else {
					msleep(2);
//				}
			}
        }
	}
	lnValue = lpCurrentSampleData->GetAverage(lpCurrentSampleData);
	lpCurrentSampleData->Reset(lpCurrentSampleData);
	up(&sem);

	return (lnValue);	
}

static int AXC_SWGauge_ReloadFactoryCalibration(AXI_Gauge *apGauge)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

	AXC_SWGauge_InitVoltageCurrnetCalibrationData(this);
	return 0;
}

static int AXC_SWGauge_ReadCurrent(AXI_Gauge *apGauge,unsigned int anTimes)
{
    int lnCurrentWithoutCalibration = AXC_SWGauge_ReadCurrentWithoutCalibration(apGauge,anTimes);
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
    pr_debug( "[BAT]AXC_SWGauge_ReadCurrent++\n");
	//int lnCurrentWithoutCalibration = AXC_SWGauge_ReadCurrentWithoutCalibration(apGauge,anTimes);

	lnCurrentWithoutCalibration += this->mnCurrentCalibration;
    pr_debug( "[BAT]AXC_SWGauge_ReadCurrent:%d--\n", lnCurrentWithoutCalibration);
	return lnCurrentWithoutCalibration;
}
///+++ASUS_BSP+++ Eason_Chang  ASUS SW gauge
#ifdef EASON_CHANGE
static int AXC_SWGauge_ReadTemperature(AXI_Gauge *apGauge)
{ 
	return get_temp_for_ASUSswgauge();
}
#else
static int AXC_SWGauge_ReadTemperature(AXI_Gauge *apGauge)
{
    ///Eason_Chang///asus_pmic_reg_read(PM8607_TBAT_MEAS2, &tmp);
	value = tmp & 0xf;
	int tmp=0, value, Rntc, Temperature;

    pr_debug( "[BAT]AXC_SWGauge_ReadTemperature++\n");
	asus_pmic_reg_read(PM8607_TBAT_MEAS2, &tmp);
	value = tmp & 0xf;
	asus_pmic_reg_read(PM8607_TBAT_MEAS1, &tmp);
	value += tmp << 4;
	Rntc = AXC_SWGauge_CalculateRealRntcFromADC(value);

	//only handle > 34, > 23, > 12...
	if(Rntc <12)        Temperature = 70;
	else if(Rntc <17)   Temperature = 60;
	else if(Rntc <25)	Temperature = 50;
	else if(Rntc <30)	Temperature = 45;
	else if(Rntc <36)	Temperature = 40;
	else if(Rntc <45)	Temperature = 34;
	else if(Rntc <74)	Temperature = 23;
	else if(Rntc <126)	Temperature = 12;
	else if(Rntc <235)	Temperature = 0;
	else if(Rntc <743)	Temperature = -20;
	else        		Temperature = -40;

	pr_debug( "[BAT][ReadTemperature]Value:%d,Rntc:%d,Temp:%d\n", value, Rntc, Temperature);
	return Temperature;
}
#endif
///---ASUS_BSP--- Eason_Chang  ASUS SW gauge

int AXC_SWGauge_GetBatteryLife(AXI_Gauge *apGauge)
{    
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
    pr_debug( "[BAT]AXC_SWGauge_GetBatteryLife:%d\n", this->mnBatteryLifePercentage);
	return this->mnBatteryLifePercentage;
}


/* ASUS_BPS +++ Josh_Liao "add get prop func" */
int AXC_SWGauge_GetPropVolt(AXI_Gauge *apGauge)
{    
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->m_prop_volt;
}

int AXC_SWGauge_GetPropVoltWithoutCali(AXI_Gauge *apGauge)
{    
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->m_prop_volt_without_cali;
}

int AXC_SWGauge_GetPropCurr(AXI_Gauge *apGauge)
{    
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->m_prop_curr;
}

int AXC_SWGauge_GetPropCurrWithoutCali(AXI_Gauge *apGauge)
{    
    AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);
	return this->m_prop_curr_without_cali;
}
/* ASUS_BPS --- Josh_Liao "add get prop func" */

static bool AXC_SWGauge_Test(void *apTestObject)
{
	return true;	
}

//ASUS_BSP +++ Josh_Liao "sw gauge v2"

static int init_resistor_cali(AXC_SWGauge *this)
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
		pr_err( "[BAT][Gau]error! can not open battery Resistor cali file, use default value.\n");
		this->resistorCali = DEFAULT_DEVICE_RESISTOR_VALUE;
		set_fs(mmseg_fs);
		return -1;
	}

	FileLength = fd->f_op->read(fd, lnResistorCalibrationData.mnChar, 2, &fd->f_pos);
	if (2 <= FileLength) {
		this->resistorCali = lnResistorCalibrationData.mnValue;
		pr_info( "[BAT][Gau] init R Cali:%d\n", this->resistorCali);
	} else {
		pr_err( "[BAT][Gau] cannot read R Cali\n");
	}

	if (filp_close(fd, NULL))
       		pr_err("[BAT][Gau] fail to close resistor cali file in %s\n", __func__);
	
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


    printk(KERN_INFO "[BAT] %d-%02d-%02d %02d:%02d:%02d\n"
        ,tm.tm_year + 1900
        ,tm.tm_mon + 1
        ,tm.tm_mday
        ,tm.tm_hour
        ,tm.tm_min
        ,tm.tm_sec);
}
//ASUS_BSP  --- Eason_Chang "add BAT info time"

static void cal_bat_capacity_work(struct work_struct *work)
{
	int volt;
	int curr;
	int ocv_percent;

	AXC_SWGauge *this = container_of(work, AXC_SWGauge, calBatCapacityWorker.work);

	// Read resistor when first time to cal battery capacity 
	if (!this->firstCalBatCapacity) {
		if (init_resistor_cali(this) < 0) {
			pr_info("[BAT][Gau] cannot read bat resistor from file, maybe file system not ready, try again!\n");
			queue_delayed_work(this->gaugeQueue, &this->readBatResistorWorker , 8*HZ);
		}
			
		this->firstCalBatCapacity = true;
	}
	
	if (!this->hasCable) {  // on discharging
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_when_discharging(this, volt, curr);
		pr_debug("[BAT][Gau] %s(), discharging, ocv = %d\n", __func__, ocv_percent);
	} else { // on charging
		this->gaugeCb->askSuspendCharging(this->gaugeCb);
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge,
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		ocv_percent = cal_ocv_percent_by_volt_directly(this, volt);
		this->gaugeCb->askResumeCharging(this->gaugeCb);
		pr_debug("[BAT][Gau] %s(), charging, ocv = %d\n", __func__, ocv_percent);		
	}
       //printk("[BAT]Temperature Now=%d\n",get_temp_for_ASUSswgauge());
       printk("[BAT]V=%d,C=%d,T=%d\n",\
                    get_voltage_for_ASUSswgauge(),\
                    get_current_for_ASUSswgauge(),\
                    get_temp_for_ASUSswgauge());
       //ASUS_BSP  +++ Eason_Chang "add BAT info time"
        ReportTime();
       //ASUS_BSP  --- Eason_Chang "add BAT info time"
	this->ocvPercent = ocv_percent;
	this->gaugeCb->onCapacityReply(&this->msParentGauge, this->gaugeCb, ocv_percent, BAT_CAP_REPLY_OK);
// TODO: need to add BAT_CAP_REPLY_ERR case

	return;
}


static void cal_bat_100_per_ocv_work(struct work_struct *work)
{
	int volt;
	int curr;

	AXC_SWGauge *this = container_of(work, AXC_SWGauge, calBat100PerOCVWorker.work);

	this->msParentGauge.ReadVoltCurr(
		&this->msParentGauge,
		TIMES_FOR_GENERAL_READ_ADC,
		&volt,
		&curr);

	if ( 0 == this->Per100OCV) {
		this->Per100OCV = volt;
		pr_info("[BAT][Gau] first time update 100 per OCV: %d\n", this->Per100OCV);
	} else {
//TODO: average 100 percent OCV
		this->Per100OCV = (this->Per100OCV + volt)/2;
		pr_info("[BAT][Gau] cycle update 100 per OCV: %d\n", this->Per100OCV);
	}

	return;
}

static void cal_bat_resistor_work(struct work_struct *work)
{
	int volt, curr;
	int cnt = 0;

	AXC_SWGauge *this = container_of(work, AXC_SWGauge, calBatResistorWorker.work);

	// sometimes read current as 0 at the time of charger plug-out, so retry more times.
	do {
		this->msParentGauge.ReadVoltCurr(
			&this->msParentGauge, 
			TIMES_FOR_GENERAL_READ_ADC,
			&volt,
			&curr);
		cnt++;
		pr_info( "[BAT][Gau]cal bat resistor, V=%d, C=%d, CNT=%d\n",volt, curr, cnt);
		msleep(1);
	} while ((curr <= 10) && (cnt < 15));

	if (this->Per100OCV < volt) {
		pr_err("[BAT][Gau] error!! 100 per OCV less than volt \n");
		return;
	}

	if (curr <= 0) {
		pr_err("[BAT][Gau] error!! unknown current \n");
		return;
	}

	this->resistorCali = (this->Per100OCV - volt)*1000/curr;
	pr_info( "[BAT][Gau]cal resistor=%d\n", this->resistorCali);

	if (this->resistorCali > RESISTOR_MAX_VAL) {
		this->resistorCali = RESISTOR_MAX_VAL;
	} else if (this->resistorCali < RESISTOR_MIN_VAL) {
		this->resistorCali = RESISTOR_MIN_VAL;
	}

	pr_info("[BAT][Gau] reset resistor cali:%d \n", this->resistorCali);

	AXC_SWGauge_CreateResistorCalibrationData(this, this->resistorCali);
	//TODO: Redesign calculate cali way, use average
	return;
}

/* read_bat_resistor_work - the work to read battery resistor from file */
static void read_bat_resistor_work(struct work_struct *work)
{
	AXC_SWGauge *this = container_of(work, AXC_SWGauge, readBatResistorWorker.work);

	init_resistor_cali(this);

	return;		
}

int AXC_SWGauge_AskCapacity(struct AXI_Gauge *gauge)
{
	AXC_SWGauge *this = container_of(gauge, AXC_SWGauge, msParentGauge);
	BUG_ON(NULL == gauge);

	queue_delayed_work(this->calBatCapacityQueue, &this->calBatCapacityWorker , 0*HZ);

	return 0;
}

int AXC_SWGauge_GetNextPollingInterval(struct AXI_Gauge *gauge)
{
	int interval;
	AXC_SWGauge *this = container_of(gauge, AXC_SWGauge, msParentGauge);

	BUG_ON(NULL == gauge);

	if (this->ocvPercent < OCV_PER_SPEEDUP_UPDATE)
		interval = UPDATE_INERVAL_BAT_LOW_IRQ;
	else
		interval = UPDATE_INERVAL_DEFAULT;

	pr_debug("[BAT][Gau]next polling interval:%d\n", interval);

	return interval;
}


int AXC_SWGauge_NotifyCableInOut(struct AXI_Gauge *gauge, bool hasCable)
{

	AXC_SWGauge *this = container_of(gauge, AXC_SWGauge, msParentGauge);
	this->hasCable = hasCable;

	BUG_ON(NULL == gauge);

	pr_debug("[BAT][Gau] notify calbe: %s \n", hasCable?"in":"out");
	pr_debug("[BAT][Gau] isBatFull: %s \n", this->isBatFull?"yes":"no");

	if ((!this->hasCable) && (this->isBatFull)) {	// cable out
		pr_debug("[BAT][Gau] calBatResistorWorker \n");
		queue_delayed_work(this->gaugeQueue, &this->calBatResistorWorker , 0*HZ);
	}

	return 0;
}

int AXC_SWGauge_NotifyBatFullChanged(struct AXI_Gauge *gauge, bool isFull)
{
	AXC_SWGauge *this = container_of(gauge, AXC_SWGauge, msParentGauge);

	BUG_ON(NULL == gauge);

	if (isFull == this->isBatFull) {
		pr_info("[BAT][Gau] %s(), same, need not to be changed \n", __FUNCTION__);
		return -1;
	}

	this->isBatFull = isFull;

	if (this->isBatFull) {
		queue_delayed_work(this->gaugeQueue, &this->calBat100PerOCVWorker , 0*HZ);		
		pr_debug("[BAT][Gau] notify bat full\n");
	} else {
		pr_debug("[BAT][Gau] notify bat changed to not full \n");
	}

	return 0;
}

bool AXC_SWGauge_IsBatExisted(struct AXI_Gauge *gauge)
{
//	AXC_SWGauge *this = container_of(gauge, AXC_SWGauge, msParentGauge);

	return true;
}
//ASUS_BSP --- Josh_Liao "sw gauge v2"

//Data Member
void AXC_SWGauge_Binding(AXI_Gauge *apGauge,int anType)
{
        AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

		this->mnBatteryLifePercentage = BATTERY_LIFE_PERCENTAGE_IN_VALID;

        this->msParentGauge.Init = AXC_SWGauge_Init;
        this->msParentGauge.DeInit = AXC_SWGauge_DeInit;
        this->msParentGauge.GetType = AXC_SWGauge_GetType;
        this->msParentGauge.SetType = AXC_SWGauge_SetType;
        this->msParentGauge.ReadVoltage = AXC_SWGauge_ReadVoltage;
        this->msParentGauge.ReadCurrent = AXC_SWGauge_ReadCurrent;
		this->msParentGauge.SetConfig = AXC_SWGauge_SetConfig;
		this->msParentGauge.IsBatValid = AXC_SWGauge_IsBatValid;
/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
		this->msParentGauge.ReadVoltCurr = AXC_SWGauge_ReadVoltCurr;
		this->msParentGauge.ReadVoltCurrWithoutCali = AXC_SWGauge_ReadVoltCurrWithoutCali;
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */
        this->msParentGauge.ReadVoltageWithoutCalibration = AXC_SWGauge_ReadVoltageWithoutCalibration;
        this->msParentGauge.ReadCurrentWithoutCalibration = AXC_SWGauge_ReadCurrentWithoutCalibration;
		this->msParentGauge.ReloadFactoryCalibration = AXC_SWGauge_ReloadFactoryCalibration;

/* ASUS_BPS +++ Josh_Liao "add get prop func" */
		this->msParentGauge.GetPropVolt = AXC_SWGauge_GetPropVolt;
		this->msParentGauge.GetPropVoltWithoutCali = AXC_SWGauge_GetPropVoltWithoutCali;
		this->msParentGauge.GetPropCurr = AXC_SWGauge_GetPropCurr;
		this->msParentGauge.GetPropCurrWithoutCali = AXC_SWGauge_GetPropCurrWithoutCali;
/* ASUS_BPS --- Josh_Liao "add get prop func" */

// joshtest
#ifdef CFG_REF_LAST_BAT_INFO
		this->msParentGauge.LoadLastBatInfo = AXC_SWGauge_LoadLastBatInfo;
		this->msParentGauge.SaveLastBatInfo = AXC_SWGauge_SaveLastBatInfo;
#endif /* CFG_REF_LAST_BAT_INFO */
		this->msParentGauge.UpdateBatInfo = AXC_SWGauge_UpdateBatInfo;
		this->msParentGauge.SetBatLow = AXC_SWGauge_SetBatLow;
		this->msParentGauge.GetBatLow = AXC_SWGauge_GetBatLow;
#ifdef CFG_CHECK_BAT_LOW_DETECTOR
		this->msParentGauge.SetBatLowDetector = AXC_SWGauge_SetBatLowDetector;
		this->msParentGauge.GetBatLowDetector = AXC_SWGauge_GetBatLowDetector;
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */
// joshtest

		this->msParentGauge.ReadTemperature = AXC_SWGauge_ReadTemperature;
		this->msParentGauge.GetBatteryLife = AXC_SWGauge_GetBatteryLife;
		this->msParentGauge.Resume =  AXC_SWGauge_Resume;
		this->msParentGauge.Suspend =  AXC_SWGauge_Suspend;
		this->msParentGauge.OnCableStatueChanged =  AXC_SWGauge_OnCableStatueChanged;
		this->msParentGauge.SetFixedUpdateInterval = AXC_SWGauge_SetFixedUpdateInterval;
        this->msParentGauge.IsBatteryRemoved = AXC_SWGauge_IsBatteryRemoved;

		this->msParentGauge.GetVoltageCalibrationVal = AXC_SWGauge_GetVoltageCalibrationVal;
		this->msParentGauge.GetCurrentCalibrationVal = AXC_SWGauge_GetCurrentCalibrationVal;
		this->msParentGauge.GetResistorCalibrationVal = AXC_SWGauge_GetResistorCalibrationVal;

        this->msParentGauge.SetType(apGauge,anType);

		this->mnVoltageCalibration = 0;
		this->mnCurrentCalibration = 0;		
        //this->mbInited = false;
#ifdef SW_GAUGE_SELF_TEST_ENABLE
        this->msParentSelfTest.Test = AXC_SWGauge_Test;
#endif
}



//ASUS_BSP +++ Josh_Liao "sw gauge v2"
void AXC_SWGauge_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb)
{
	AXC_SWGauge *this = container_of(apGauge, AXC_SWGauge, msParentGauge);

// josh_liao_gauge +++
	BUG_ON((NULL == gaugeCb) || (NULL == apGauge));

	this->gaugeCb = gaugeCb;
	INIT_DELAYED_WORK(&this->calBatCapacityWorker, cal_bat_capacity_work) ;
	INIT_DELAYED_WORK(&this->calBat100PerOCVWorker, cal_bat_100_per_ocv_work) ;
	INIT_DELAYED_WORK(&this->calBatResistorWorker, cal_bat_resistor_work) ;
	INIT_DELAYED_WORK(&this->readBatResistorWorker, read_bat_resistor_work) ;

	this->calBatCapacityQueue = create_singlethread_workqueue("CalBatCapacityWorker");
	if (!this->calBatCapacityQueue)
		pr_err("[BAT][Gau]error!! when create calBatCapacityQueue\n");

	this->gaugeQueue = create_singlethread_workqueue("GaugeWorker");	
	if (!this->gaugeQueue)
		pr_err("[BAT][Gau]error!! when create gaugeQueue\n");

	this->msParentGauge.askCapacity = AXC_SWGauge_AskCapacity;
	this->msParentGauge.getNextPollingInterval = AXC_SWGauge_GetNextPollingInterval;
	this->msParentGauge.notifyCableInOut = AXC_SWGauge_NotifyCableInOut;
	this->msParentGauge.notifyBatFullChanged = AXC_SWGauge_NotifyBatFullChanged;
	this->msParentGauge.isBatExisted = AXC_SWGauge_IsBatExisted;
	this->msParentGauge.SetType = AXC_SWGauge_SetType;
	this->msParentGauge.GetType = AXC_SWGauge_GetType;
// ---

	this->mnBatteryLifePercentage = BATTERY_LIFE_PERCENTAGE_IN_VALID;

	this->msParentGauge.Init = AXC_SWGauge_Init;
	this->msParentGauge.DeInit = AXC_SWGauge_DeInit;
	this->msParentGauge.ReadVoltage = AXC_SWGauge_ReadVoltage;
	this->msParentGauge.ReadCurrent = AXC_SWGauge_ReadCurrent;
	this->msParentGauge.SetConfig = AXC_SWGauge_SetConfig;
	this->msParentGauge.IsBatValid = AXC_SWGauge_IsBatValid;
/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
	this->msParentGauge.ReadVoltCurr = AXC_SWGauge_ReadVoltCurr;
	this->msParentGauge.ReadVoltCurrWithoutCali = AXC_SWGauge_ReadVoltCurrWithoutCali;
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */
	this->msParentGauge.ReadVoltageWithoutCalibration = AXC_SWGauge_ReadVoltageWithoutCalibration;
        this->msParentGauge.ReadCurrentWithoutCalibration = AXC_SWGauge_ReadCurrentWithoutCalibration;
	this->msParentGauge.ReloadFactoryCalibration = AXC_SWGauge_ReloadFactoryCalibration;

/* ASUS_BPS +++ Josh_Liao "add get prop func" */
	this->msParentGauge.GetPropVolt = AXC_SWGauge_GetPropVolt;
	this->msParentGauge.GetPropVoltWithoutCali = AXC_SWGauge_GetPropVoltWithoutCali;
	this->msParentGauge.GetPropCurr = AXC_SWGauge_GetPropCurr;
	this->msParentGauge.GetPropCurrWithoutCali = AXC_SWGauge_GetPropCurrWithoutCali;
/* ASUS_BPS --- Josh_Liao "add get prop func" */

// joshtest
#ifdef CFG_REF_LAST_BAT_INFO
	this->msParentGauge.LoadLastBatInfo = AXC_SWGauge_LoadLastBatInfo;
	this->msParentGauge.SaveLastBatInfo = AXC_SWGauge_SaveLastBatInfo;
#endif /* CFG_REF_LAST_BAT_INFO */
	this->msParentGauge.UpdateBatInfo = AXC_SWGauge_UpdateBatInfo;
	this->msParentGauge.SetBatLow = AXC_SWGauge_SetBatLow;
	this->msParentGauge.GetBatLow = AXC_SWGauge_GetBatLow;
#ifdef CFG_CHECK_BAT_LOW_DETECTOR
	this->msParentGauge.SetBatLowDetector = AXC_SWGauge_SetBatLowDetector;
	this->msParentGauge.GetBatLowDetector = AXC_SWGauge_GetBatLowDetector;
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */
// joshtest

	this->msParentGauge.ReadTemperature = AXC_SWGauge_ReadTemperature;
	this->msParentGauge.GetBatteryLife = AXC_SWGauge_GetBatteryLife;
	this->msParentGauge.Resume =  AXC_SWGauge_Resume;
	this->msParentGauge.Suspend =  AXC_SWGauge_Suspend;
	this->msParentGauge.OnCableStatueChanged =  AXC_SWGauge_OnCableStatueChanged;
	this->msParentGauge.SetFixedUpdateInterval = AXC_SWGauge_SetFixedUpdateInterval;
	this->msParentGauge.IsBatteryRemoved = AXC_SWGauge_IsBatteryRemoved;

	this->msParentGauge.GetVoltageCalibrationVal = AXC_SWGauge_GetVoltageCalibrationVal;
	this->msParentGauge.GetCurrentCalibrationVal = AXC_SWGauge_GetCurrentCalibrationVal;
	this->msParentGauge.GetResistorCalibrationVal = AXC_SWGauge_GetResistorCalibrationVal;

	this->msParentGauge.SetType(apGauge,anType);

	this->mnVoltageCalibration = 0;
	this->mnCurrentCalibration = 0;		
	//this->mbInited = false;
#ifdef SW_GAUGE_SELF_TEST_ENABLE
	this->msParentSelfTest.Test = AXC_SWGauge_Test;
#endif
}

//ASUS_BSP --- Josh_Liao "sw gauge v2"

