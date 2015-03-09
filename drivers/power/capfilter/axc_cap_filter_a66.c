#include <linux/kernel.h>
#include <linux/mutex.h>
#include "axi_cap_filter.h"
#include "axc_cap_filter_a66.h"
#include <linux/asusdebug.h>

/* BAT_LIFE - different battery life definition */
#define BAT_LIFE_TO_SHUTDOWN 0
#define BAT_LIFE_ONE 1
#define BAT_LIFE_FIVE 5
#define BAT_LIFE_TWO  2
#define BAT_LIFE_FULL 100
#define BAT_LIFE_BEFORE_FULL 99
#define BAT_LIFE_BEFORE_FULL_ONE_UNIT 94

#define SECOND_OF_HOUR	3600
//Eason:fix Cap drop too slowly in unattended mode+++
#define DIFF_EARLYSUSPEND_SUSPEND_CUR 100
#define MIN_SUSPEND_CUR 5
#define WLAN_HOT_SPOT_CUR 350
#define DEFAULT_SUSPEND_FAST 30
#define DEFAULT_SUSPEND_PHONE_FAST 200
#define MAX_SUSPEND_FAST 500
extern int filRealSusT;   
//Eason:fix Cap drop too slowly in unattended mode---
//Eason: more accuracy for discharge after dot+++
static int g_discharge_after_dot = 0;
//Eason: more accuracy for discharge after dot---
//Eason: if BatLow keep 15 min, shutdown devices+++
extern bool g_batLowLongTimeShut;
#define BATLOW_LONGTIME_SHUT_CAP 5
//Eason: if BatLow keep 15 min, shutdown devices---
//Eason add fasterLeverage judge+++ 
static int g_do_fasterLeverage_count = 0; 
//Eason add fasterLeverage judge---  
//Eason: choose Capacity type SWGauge/BMS +++
extern int g_CapType;
//Eason: choose Capacity type SWGauge/BMS ---
//Hank: A86 bo use+++
static int gDiff_BMS;
//Hank: A86 bo use---
//Hank:A86 slowly drop+++
extern int g_current_now;
#define OCV_PER_SPEEDUP_UPDATE_14P	14//Hank: A86 change update interval when Cap<=14% 
//Hank:A86 slowly drop---
//ASUS_BSP Eason_Chang:BatFilter know past time in phone call+++
int g_BatFil_InPhoneCall_PastTime = 0;
//ASUS_BSP Eason_Chang:BatFilter know past time in phone call---

/* eval_bat_life_when_discharging - Evaluate conservative battery life when discharging.
 * Use (maximum consuming current * update interval) as one factor.
 */


#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern char g_CHG_mode;
#endif

extern enum DEVICE_HWID g_ASUS_hwID;

//Eason:fix Cap drop too slowly in unattended mode+++
 /*
 *- interval           
 *- filRealSusTime  : real suspend time during interval 
 *- resCurr		: resume time current 
 */
static int cntPastAvgCurr(int interval, int filRealSusTime, int resCurr)
{
	int resumeP;
	int suspendP;
	int susCurr;

	resumeP = (interval-filRealSusTime)*100/interval; //resume percent
	suspendP = (filRealSusTime*100)/interval;  //suspend percent
	
	susCurr = resCurr-DIFF_EARLYSUSPEND_SUSPEND_CUR; //suspend current
	if(susCurr < MIN_SUSPEND_CUR)
		susCurr = MIN_SUSPEND_CUR;

	if(resCurr < 0)
		return 0;

	printk("[BAT][Fil]resumeP:%d, suspendP:%d\n",resumeP,suspendP);
	return ( (resumeP*resCurr/100) + (suspendP*susCurr/100) );
}
//Eason:fix Cap drop too slowly in unattended mode---
//Eason: more accuracy for discharge after dot+++
int formula_of_discharge(int maxMah,int batCapMah, int interval)
{
	return  ( (maxMah*100*100/batCapMah)*interval/SECOND_OF_HOUR/100 );
}

int formula_of_discharge_dot(int maxMah,int batCapMah, int interval)
{
	return  ( 10*(maxMah*100*100/batCapMah)*interval/SECOND_OF_HOUR/100 )%10;
}

int discharge_dot_need_plus(void)
{
	int total_can_be_plus = 0;
	
	if( (g_discharge_after_dot/10)>=1)
	{
		total_can_be_plus += (g_discharge_after_dot/10);
		g_discharge_after_dot = g_discharge_after_dot%10;
	}

	return total_can_be_plus;
}
//Eason: more accuracy for discharge after dot---

static int eval_bat_life_when_discharging(
	int nowCap, int lastCap, int maxMah, int interval, int batCapMah)
{
	int bat_life = 0;
	int drop_val = lastCap - nowCap;
	int pastTimeAvgCurr = 0;//Eason:fix Cap drop too slowly in unattended mode
	//Eason: more accuracy for discharge after dot+++
	int pred_discharge_after_dot = 0;  //predict discharge cap after dot 
	int fast_discharge_after_dot = 0;   //fastleverage discharger cap after dot
	//Eason: more accuracy for discharge after dot---

	printk( "[BAT][Fil]%s(), drop_val:%d\n",__func__,drop_val);

	//Hank:A86 slowly drop+++
	//if(g_ASUS_hwID >= A90_EVB0)
	if(1)
		gDiff_BMS = 100;//set gDiffBMS a large value, let cap judge only compare (1)gauge drop value & (2)predict drop value.
	//Hank:A86 slowly drop---

	if (drop_val > 0) { //capacity drop
		int max_predict_drop_val = 0;
		int finetune_max_predict_drop_val = 0;
		int fasterLeverage_drop_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			//Eason :when  low bat Cap draw large current  +++
			//max_predict_drop_val = (maxMah*100/batCapMah)*interval/SECOND_OF_HOUR;
			max_predict_drop_val = formula_of_discharge(maxMah, batCapMah, interval);
			pred_discharge_after_dot = formula_of_discharge_dot(maxMah, batCapMah, interval);

			if(10 != maxMah){
				if(nowCap<=OCV_PER_SPEEDUP_UPDATE_14P){
					if(g_current_now>600){
						fasterLeverage_drop_val = formula_of_discharge(g_current_now, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(g_current_now, batCapMah, interval);
					}
					else{
						fasterLeverage_drop_val = formula_of_discharge(600, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(600, batCapMah, interval);
					}
				}
				else{
					if(g_current_now>300){
						fasterLeverage_drop_val = formula_of_discharge(g_current_now, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(g_current_now, batCapMah, interval);
					}
					else{
						fasterLeverage_drop_val = formula_of_discharge(300, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(300, batCapMah, interval);
					}
				}
			}
			//Eason :when  low bat Cap draw large current  ---
			//Eason:prevent in unattend mode mass drop+++
			if(10==maxMah){
				//Eason:In  phone call suspend, use 200mA do fasterLeverage+++
				/*
				*	extern int g_flag_csvoice_fe_connected, 
				*     - 0: not in phone call
				*     - 1: in phone call
				*
				*	g_BatFil_InPhoneCall_PastTime,
				*	- 0: Past time not in phone call
				*	- 1: Past time in phone call
				*/
				pastTimeAvgCurr = cntPastAvgCurr(interval,filRealSusT,g_current_now);
				
				if(0 == g_BatFil_InPhoneCall_PastTime)
				{
					//Eason:fix Cap drop too slowly in unattended mode+++
					if(pastTimeAvgCurr > WLAN_HOT_SPOT_CUR)
					{
						fasterLeverage_drop_val = formula_of_discharge(WLAN_HOT_SPOT_CUR, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(WLAN_HOT_SPOT_CUR, batCapMah, interval);
					}
					else if(pastTimeAvgCurr < DEFAULT_SUSPEND_FAST)
					{
						fasterLeverage_drop_val = formula_of_discharge(DEFAULT_SUSPEND_FAST, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(DEFAULT_SUSPEND_FAST, batCapMah, interval);
					}else{
						fasterLeverage_drop_val = formula_of_discharge(pastTimeAvgCurr, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(pastTimeAvgCurr, batCapMah, interval);
					}
					//Eason:fix Cap drop too slowly in unattended mode---
				}else{
					if(pastTimeAvgCurr > MAX_SUSPEND_FAST)
					{
						fasterLeverage_drop_val = formula_of_discharge(MAX_SUSPEND_FAST, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(MAX_SUSPEND_FAST, batCapMah, interval);
					}else if(pastTimeAvgCurr > DEFAULT_SUSPEND_PHONE_FAST)
					{
						fasterLeverage_drop_val = formula_of_discharge(pastTimeAvgCurr, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(pastTimeAvgCurr, batCapMah, interval);
					}else{
						fasterLeverage_drop_val = formula_of_discharge(DEFAULT_SUSPEND_PHONE_FAST, batCapMah, interval);
						fast_discharge_after_dot = formula_of_discharge_dot(DEFAULT_SUSPEND_PHONE_FAST, batCapMah, interval);
					}
				}
				//Eason:In  phone call suspend, use 200mA do fasterLeverage---
			}
			//Eason:prevent in unattend mode mass drop---

			//Eason add fasterLeverage judge+++  
			if((drop_val > max_predict_drop_val) && (g_do_fasterLeverage_count < 3)){
				g_do_fasterLeverage_count++; 
			}
			else if((drop_val <= max_predict_drop_val) && (g_do_fasterLeverage_count > 0)){
				g_do_fasterLeverage_count--;
			}

			if( nowCap<=OCV_PER_SPEEDUP_UPDATE_14P ){
				finetune_max_predict_drop_val = fasterLeverage_drop_val;
				//Eason: more accuracy for discharge after dot+++
				g_discharge_after_dot += fast_discharge_after_dot;
				printk("[BAT][Fil]formula:%d.%d\n",fasterLeverage_drop_val,fast_discharge_after_dot);
				finetune_max_predict_drop_val += discharge_dot_need_plus();
				//Eason: more accuracy for discharge after dot---
			}
			else if( (2<=g_do_fasterLeverage_count)&&(nowCap<10) ){
				finetune_max_predict_drop_val = fasterLeverage_drop_val;
				//Eason: more accuracy for discharge after dot+++
				g_discharge_after_dot += fast_discharge_after_dot;
				printk("[BAT][Fil]formula:%d.%d\n",fasterLeverage_drop_val,fast_discharge_after_dot);
				finetune_max_predict_drop_val += discharge_dot_need_plus();
				//Eason: more accuracy for discharge after dot---
			}
			else if(3 == g_do_fasterLeverage_count){
				finetune_max_predict_drop_val = fasterLeverage_drop_val;
				//Eason: more accuracy for discharge after dot+++
				g_discharge_after_dot += fast_discharge_after_dot;
				printk("[BAT][Fil]formula:%d.%d\n",fasterLeverage_drop_val,fast_discharge_after_dot);
				finetune_max_predict_drop_val += discharge_dot_need_plus();
				//Eason: more accuracy for discharge after dot---
			}
			else{
				//Eason: more accuracy for discharge after dot+++
				g_discharge_after_dot += pred_discharge_after_dot;
				printk("[BAT][Fil]formula:%d.%d\n",max_predict_drop_val,pred_discharge_after_dot);
				max_predict_drop_val += discharge_dot_need_plus();
				//Eason: more accuracy for discharge after dot---
				
				//Eason: remember last BMS Cap to filter+++
				if(gDiff_BMS >=0)//when discharge BMS drop value >=0
				{
						finetune_max_predict_drop_val = min(abs(gDiff_BMS),abs(max_predict_drop_val));		
				 }
				else
				 {
				 		//Eason: add BMS absolute value judge value+++
				 		 int MinOfBmsPd = 0; 
						 MinOfBmsPd = min(abs(gDiff_BMS),abs(max_predict_drop_val));	
						if( abs(gDiff_BMS) == min(abs(MinOfBmsPd),abs(drop_val)) )
						{
							finetune_max_predict_drop_val = 0;   
						}else{
							finetune_max_predict_drop_val = abs(max_predict_drop_val);  
						}
						//Eason: add BMS absolute value judge value---
				}
				//Eason: remember last BMS Cap to filter---
			}

			if(finetune_max_predict_drop_val<0)
			{
				finetune_max_predict_drop_val = -finetune_max_predict_drop_val;
				printk("[BAT][Fil]Error: finetune_max_predict_drop_val over float\n");
			}
			//Eason add fasterLeverage judge---
            
			bat_life = lastCap - min(drop_val, finetune_max_predict_drop_val);
		}
		else {
			//bat_life = lastCap - drop_val;
			bat_life = lastCap;
			printk( "[BAT][Fil]Error!!! %s(), interval < 0\n",
					__func__);
		}

		ASUSEvtlog( "[BAT][Fil] int:%d, sus:%d, dp:%d, pre:%d, fast:%d, BMS:%d, fine:%d, cnt:%d, aCur:%d, nCur:%d, dot:%d\n",
			interval,
			filRealSusT,
			drop_val,
			max_predict_drop_val,
			fasterLeverage_drop_val,
			gDiff_BMS,
			finetune_max_predict_drop_val,
			g_do_fasterLeverage_count,
			pastTimeAvgCurr,
			g_current_now,
			g_discharge_after_dot);

	}
	else {
		bat_life = lastCap; // without cablein capacity rise, do not  change capacity

		if(g_do_fasterLeverage_count > 0){
			g_do_fasterLeverage_count--;
		}

		if (drop_val < 0) {
			pr_info( "[BAT][Fil] Error!!! %s(), drop val less than 0. count:%d\n", __func__,g_do_fasterLeverage_count);
		}
	}

	return bat_life;
}



/* eval_bat_life_when_charging - Evaluate conservative battery life when charging.
 * Use (maximum charging current * update interval) as one factor.
 */
static int eval_bat_life_when_charging(
	int nowCap, int lastCap, int maxMah, int interval, int batCapMah)
{
	int bat_life = 0;
	int rise_val = nowCap - lastCap; 
	int tmp_val_after_dot = 0;
	/* g_bat_life_after_dot - remain battery life after dot, used for more accuracy */
	static int max_bat_life_rise_after_dot = 0;

	pr_debug( "[BAT][Fil]%s(), rise_val:%d\n",__func__,rise_val);

	if (rise_val > 0) {
		unsigned long max_bat_life_rise_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_bat_life_rise_val = ((maxMah*100/batCapMah)*interval/SECOND_OF_HOUR)+1;

			/* to calculate the first number after the decimal dot for more accuracy.*/
			tmp_val_after_dot = ((100*maxMah*100/batCapMah)*interval/SECOND_OF_HOUR)%100;	
			max_bat_life_rise_after_dot += tmp_val_after_dot;
			pr_debug( "[BAT][Fil]%s(), tmp_val_after_dot:%d, max_bat_life_rise_after_dot:%d\n",__func__,tmp_val_after_dot,max_bat_life_rise_after_dot);
			
			if ((max_bat_life_rise_after_dot/100) >= 1) {
				pr_debug( "[BAT][Fil]%s(), max_bat_life_rise_after_dot:%d\n",__func__,max_bat_life_rise_after_dot);
				max_bat_life_rise_val += (max_bat_life_rise_after_dot/100);
				max_bat_life_rise_after_dot = max_bat_life_rise_after_dot%100;
			}
			bat_life = lastCap + min(rise_val, (int)max_bat_life_rise_val);
            //TO DO ...if interval is too big...will get a negative value for capacity returned...
		} 
		else {
			bat_life = lastCap + rise_val;
			pr_err("[BAT][Fil]Error!!! %s(), interval < 0\n",
					__func__);
		}

		pr_debug( "[BAT][Fil]%s(), rise_val:%d, interval:%d, max_bat_life_rise_val:%d, bat_life:%d\n",
			__func__,
			rise_val,
			(int)interval,
			(int)max_bat_life_rise_val,
			bat_life);
	} 
	else {
		bat_life = lastCap;
		pr_debug( "[BAT][Fil]%s(), keep the same bat_life:%d as before\n",__func__,bat_life);

		if (rise_val < 0) {
			pr_err("[BAT][Fil] Error!!! %s(), rise val less than 0.\n", __func__);
		}
	}
	return bat_life;
}


/* update_bat_info_for_speical_case - Update battery info for some special cases.
 * Such as when to update battery life to 100.
 */
static int update_bat_info_for_speical_case(
	int bat_life,
	bool EnableBATLifeRise,
	bool isCharing,	
	bool isBatFull,
	bool isBatLow)
{
	int final_bat_life;

	if ((!isBatLow) && (bat_life <= 0)) {
		pr_info( "[BAT][Fil]%s(), bat not low, but get cap as 0, return 1\n", __func__);
		final_bat_life = BAT_LIFE_ONE;
		return final_bat_life;
	}

	if (bat_life >= BAT_LIFE_FULL) {
#if 0
		if(false == isBatFull)
		//if(this->mpCharger->IsCharging(this->mpCharger))
		{
			pr_debug( "[BAT][Fil]%s(), Still in charging status, so update bat life to 99\n", __func__);
			final_bat_life = BAT_LIFE_BEFORE_FULL;
			return final_bat_life;
		} else {
			final_bat_life = BAT_LIFE_FULL;
			return final_bat_life;
		}
#else
		final_bat_life = BAT_LIFE_FULL;
		return final_bat_life;
#endif
	} 


	if (bat_life >= BAT_LIFE_BEFORE_FULL_ONE_UNIT) {
		//Need to know if being charging or not	
		if (EnableBATLifeRise) {
			if(true == isBatFull){
				//if(this->mpCharger->IsCharging(this->mpCharger)) {
				pr_debug( "[BAT][Fil]%s(), keep bat life to 100\n", __func__);
				final_bat_life = BAT_LIFE_FULL;
				return final_bat_life;
			} else {
				pr_debug( "[BAT][Fil]%s(), still in charging\n", __func__);
				final_bat_life = bat_life;
				return final_bat_life;
			}
		}
	}

	final_bat_life = bat_life;

	return final_bat_life;
}

//#define BAT_LOW_LIFE_MAP_START 	10

//static int bat_low_life_map_tbl[BAT_LOW_LIFE_MAP_START + 1] = {1, 1, 1, 1, 2, 3, 4, 6, 7, 8, 10};

int AXC_Cap_Filter_A66_GetType(struct AXI_Cap_Filter *apCapFilter)
{
	AXC_Cap_Filter_A66 *this = container_of(apCapFilter, AXC_Cap_Filter_A66, parentCapFilter);
	return this->filterType;
}


int AXC_Cap_Filter_A66_FilterCapacity(struct AXI_Cap_Filter *apCapFilter, int nowCap, int lastCap, bool EnableBATLifeRise, bool isCharing, bool isBatFull, bool isBatLow, int maxMah, int interval)
{
	int bat_life;
	AXC_Cap_Filter_A66 *this = container_of(apCapFilter, AXC_Cap_Filter_A66, parentCapFilter);

	//Eason: choose Capacity type SWGauge/BMS +++
	if ((1==g_CapType)&&isBatLow && (nowCap <= 0) && (lastCap <= 3)) {
		printk("[BAT][Fil][BMS]%s(), bat low and cap <= 2, shutdown!! \n", __func__);
		return BAT_LIFE_TO_SHUTDOWN;
	}
	//Eason: choose Capacity type SWGauge/BMS ---

	/* the criteria to set bat life as 0 to shutdown */	
	if (isBatLow && (nowCap <= 0) && (lastCap <= 3)) {
		pr_info("[BAT][Fil]%s(), bat low and cap <= 3, shutdown!! \n", __func__);
		return BAT_LIFE_TO_SHUTDOWN;
	}

#ifndef ASUS_FACTORY_BUILD
	if ((nowCap <= 0) && (lastCap <= 3)
#ifdef ASUS_CHARGING_MODE
	 && (g_CHG_mode != 1)
#endif
	 ) {
		pr_info("[BAT][Fil]%s(), bat low and cap <= 3, shutdown!! \n", __func__);
		return BAT_LIFE_TO_SHUTDOWN;
	}
#endif
	
	//Eason: if BatLow keep 15 min, shutdown devices+++
	if (isBatLow && g_batLowLongTimeShut && (nowCap <= BATLOW_LONGTIME_SHUT_CAP) && (lastCap <= BATLOW_LONGTIME_SHUT_CAP)){
		ASUSEvtlog("[BAT][Fil][BatLow]Long tme => shutdown\n");
		return BAT_LIFE_TO_SHUTDOWN;
	}
	//Eason: if BatLow keep 15 min, shutdown devices---

	if (EnableBATLifeRise) // enable capacity rise
	{
		bat_life = eval_bat_life_when_charging(nowCap, lastCap, maxMah, interval, this->capMah);
	} 
	else // disable capacity rise 
	{
		bat_life = eval_bat_life_when_discharging(nowCap, lastCap, maxMah, interval, this->capMah);
	}

	bat_life = update_bat_info_for_speical_case(bat_life, EnableBATLifeRise, isCharing, isBatFull, isBatLow);

	pr_debug("[BAT][Fil]%s(), filter cap:%d \n", __func__, bat_life);
	return bat_life;

}

extern void AXC_Cap_Filter_A66_Constructor(AXI_Cap_Filter *apCapFilter, int filterType, int capMah)
{

	AXC_Cap_Filter_A66 *this = container_of(apCapFilter, AXC_Cap_Filter_A66, parentCapFilter);
	this->capMah = capMah;
	this->filterType = filterType;
	this->parentCapFilter.getType = AXC_Cap_Filter_A66_GetType;
	this->parentCapFilter.filterCapacity = AXC_Cap_Filter_A66_FilterCapacity;


}


