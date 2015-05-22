

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
#include <linux/kernel.h>
#include <linux/mutex.h>
#include "AXI_CapacityFilter.h"

/* BAT_LIFE - different battery life definition */
#define BAT_LIFE_TO_SHUTDOWN 0
#define BAT_LIFE_FIVE 5
#define BAT_LIFE_TWO  2
#define BAT_LIFE_ONE  1
#define BAT_LIFE_FULL 100
#define BAT_LIFE_BEFORE_FULL 99
#define BAT_LIFE_BEFORE_FULL_ONE_UNIT 83

#define SECOND_OF_HOUR	3600

/* g_bat_life_after_dot - remain battery life after dot, used for more accuracy */
static int g_bat_life_after_dot = 0;


/* eval_bat_life_when_discharging - Evaluate conservative battery life when discharging.
 * Use (maximum consuming current * update interval) as one factor.
 */
static int eval_bat_life_when_discharging(
	int nowCap, int lastCap, int maxMah, int interval)
{
	int bat_life = 0;
	int var_drop_base=0;
	int drop_val = lastCap - nowCap;

	printk( "[BAT][Fil]%s(), drop_val:%d\n",
		__func__,
		drop_val);

	if (drop_val > 0) {
		int max_predict_drop_val = 0;
		int finetune_max_predict_drop_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_predict_drop_val = (maxMah*100/BAT_CAP_MAH)*interval/SECOND_OF_HOUR;
			/* variable drop base for faster leverage true batter life */
			if ((drop_val/2) > 0) {
				var_drop_base = drop_val/2;
			} else {
				var_drop_base = 1;
			}

			finetune_max_predict_drop_val = max(var_drop_base, max_predict_drop_val);
			bat_life = lastCap - min(drop_val, finetune_max_predict_drop_val);
		} else {
			bat_life = lastCap - drop_val;
			pr_err( "[BAT][Fil] %s(), interval < 0\n",
					__func__);
		}

		printk( "[BAT][Fil] interval:%d, var_drop_base:%d, max_predict_drop_val:%d, finetune_max_predict_drop_val:%d \n",
			interval,
			var_drop_base,
			max_predict_drop_val,
			finetune_max_predict_drop_val);
	} else {
		bat_life = lastCap;
		if (drop_val < 0) {
			printk( "[BAT][Fil] %s(), drop val less than 0.\n", __func__);
		}
	}

	return bat_life;
}



/* eval_bat_life_when_charging - Evaluate conservative battery life when charging.
 * Use (maximum charging current * update interval) as one factor.
 */
static int eval_bat_life_when_charging(
	int nowCap, int lastCap, int maxMah, int interval)
{
	int bat_life = 0;
	int rise_val = nowCap - lastCap; 
	int tmp_val_after_dot = 0;

	if (rise_val > 0) {
		long max_bat_life_rise_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_bat_life_rise_val = (maxMah*100/BAT_CAP_MAH)*interval/SECOND_OF_HOUR;
			/* to calculate the first number after the decimal dot for more accuracy.*/
			tmp_val_after_dot = ((10*maxMah*100/BAT_CAP_MAH)*interval/SECOND_OF_HOUR)%10;
			pr_debug( "[BAT][Fil]%s(), tmp_val_after_dot:%d\n",
				__func__,
				tmp_val_after_dot);	
			g_bat_life_after_dot += tmp_val_after_dot;
			if ((g_bat_life_after_dot/10) >= 1) {
				pr_debug( "[BAT][Fil]%s(), g_bat_life_after_dot:%d\n",
					__func__,
					g_bat_life_after_dot);
				max_bat_life_rise_val += (g_bat_life_after_dot/10);
				g_bat_life_after_dot = g_bat_life_after_dot%10;
			}
			bat_life = lastCap + min(rise_val, (int)max_bat_life_rise_val);
            //TO DO ...if interval is too big...will get a negative value for capacity returned...
		} else {
			bat_life = lastCap + rise_val;
			pr_err("[BAT][Fil] %s(), interval < 0\n",
					__func__);
		}

		pr_debug( "[BAT][Fil]%s(), rise_val:%d, interval:%d, max_bat_life_rise_val:%d, bat_life:%d\n",
			__func__,
			rise_val,
			(int)interval,
			(int)max_bat_life_rise_val,
			bat_life);
	} else {
		bat_life = lastCap;
		pr_debug( "[BAT][Fil]%s(), keep the same bat_life:%d as before\n",
			__func__,
			bat_life);

		if (rise_val < 0) {
			pr_err("[BAT][Fil] %s(), rise val less than 0.\n", __func__);
		}
	}
	return bat_life;
}


/* update_bat_info_for_speical_case - Update battery info for some special cases.
 * Such as when to update battery life to 100.
 */
static int update_bat_info_for_speical_case(
	int bat_life,
	bool hasCable,
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
	} 


	if (bat_life >= BAT_LIFE_BEFORE_FULL_ONE_UNIT) {
		//Need to know if being charging or not	
		if (hasCable) {
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


int filterCapacity(int nowCap, int lastCap, bool hasCable, bool isCharing, bool isBatFull, bool isBatLow, int maxMah, int interval)
{
	int bat_life;

	// avoid to shutdown when consuming big current, so need to check battery life < 5 too.
	if ((isBatLow) && (nowCap < BAT_LIFE_FIVE))
		return BAT_LIFE_TO_SHUTDOWN;

	if (hasCable) {	// has cable
		bat_life = eval_bat_life_when_charging(nowCap, lastCap, maxMah, interval);
	} else {	// no cable
		bat_life = eval_bat_life_when_discharging(nowCap, lastCap, maxMah, interval);
	}

	bat_life = update_bat_info_for_speical_case(bat_life, hasCable, isCharing, isBatFull, isBatLow);		
	return bat_life;
}

//ASUS_BSP --- Josh_Liao "sw gauge v2"
