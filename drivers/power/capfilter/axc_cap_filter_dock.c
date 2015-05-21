#include <linux/kernel.h>
#include <linux/mutex.h>
#include "axi_cap_filter.h"
#include "axc_cap_filter_dock.h"

/* BAT_LIFE - different battery life definition */
#define BAT_LIFE_TO_SHUTDOWN 0
#define BAT_LIFE_FIVE 5
#define BAT_LIFE_TWO  2
#define BAT_LIFE_FULL 100
#define BAT_LIFE_KEEP_FULL 95
#define BAT_LIFE_BEFORE_FULL 99
#define BAT_LIFE_BEFORE_FULL_ONE_UNIT 95

#define SECOND_OF_HOUR	3600

/* g_bat_life_after_dot - remain battery life after dot, used for more accuracy */
static int g_bat_life_after_dot = 0;


/* eval_dock_bat_life_when_discharging - Evaluate conservative battery life when discharging.
 * Use (maximum consuming current * update interval) as one factor.
 */
static int eval_dock_bat_life_when_discharging(
	int nowCap, int lastCap, int maxMah, int interval, int batCapMah)
{
	int bat_life = 0;
	int var_drop_base=0;
	int drop_val = lastCap - nowCap;

	pr_info( "[BAT][Fil]%s(), drop_val:%d\n",
		__func__,
		drop_val);

	if (drop_val > 0) {
		int max_predict_drop_val = 0;
		int finetune_max_predict_drop_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_predict_drop_val = (maxMah*100/batCapMah)*interval/SECOND_OF_HOUR;
			/* variable drop base for faster leverage true batter life */
			var_drop_base = 1;

			finetune_max_predict_drop_val = max(var_drop_base, max_predict_drop_val);
			bat_life = lastCap - min(drop_val, finetune_max_predict_drop_val);
		} else {
			bat_life = lastCap - drop_val;
			pr_err( "[BAT][Fil]Error!!! %s(), interval < 0\n",
					__func__);
		}

		pr_info( "[BAT][Fil] interval:%d, var_drop_base:%d, max_predict_drop_val:%d, finetune_max_predict_drop_val:%d \n",
			interval,
			var_drop_base,
			max_predict_drop_val,
			finetune_max_predict_drop_val);
	} else {
		bat_life = lastCap;
		if (drop_val < 0) {
			pr_info( "[BAT][Fil] Error!!! %s(), drop val less than 0.\n", __func__);
		}
	}

	return bat_life;
}



/* eval_dock_bat_life_when_charging - Evaluate conservative battery life when charging.
 * Use (maximum charging current * update interval) as one factor.
 */
static int eval_dock_bat_life_when_charging(
	int nowCap, int lastCap, int maxMah, int interval, int batCapMah)
{
	int bat_life = 0;
	int rise_val = nowCap - lastCap; 
	int tmp_val_after_dot = 0;

	if (rise_val > 0) {
		long max_bat_life_rise_val = 0;

		if (interval > 0) {
			/* if interval is more than 108sec, max_predict_drop_val will be more than 1 */
			max_bat_life_rise_val = (maxMah*100/batCapMah)*interval/SECOND_OF_HOUR;
			/* to calculate the first number after the decimal dot for more accuracy.*/
			tmp_val_after_dot = ((10*maxMah*100/batCapMah)*interval/SECOND_OF_HOUR)%10;
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
			pr_err("[BAT][Fil]Error!!! %s(), interval < 0\n",
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
			pr_err("[BAT][Fil] Error!!! %s(), rise val less than 0.\n", __func__);
		}
	}
	return bat_life;
}

int AXC_Cap_Filter_Dock_GetType(struct AXI_Cap_Filter *apCapFilter)
{
	AXC_Cap_Filter_Dock *this = container_of(apCapFilter, AXC_Cap_Filter_Dock, parentCapFilter);
	return this->filterType;
}

#define BAT_LOW_LIFE_MAP_START 	15
	
static int bat_low_life_map_tbl[BAT_LOW_LIFE_MAP_START + 1] = {0, 0, 0, 0, 0, 0, 1, 2, 4, 6, 7, 9, 10, 11, 13, 14};

int AXC_Cap_Filter_Dock_FilterCapacity(struct AXI_Cap_Filter *apCapFilter, int nowCap, int lastCap, bool hasCable, bool isCharing, bool isBatFull, bool isBatLow, int maxMah, int interval)
{
	int bat_life;
	AXC_Cap_Filter_Dock *this = container_of(apCapFilter, AXC_Cap_Filter_Dock, parentCapFilter);

	/* the criteria to set dock bat full*/
	if ((nowCap >= BAT_LIFE_BEFORE_FULL_ONE_UNIT) && (isBatFull)){
		bat_life = BAT_LIFE_FULL;
		pr_info( "[BAT][Fil] %s(), set bat life 100\n", __func__);
		return bat_life;
	}

	/* battery low life remap */
	if (nowCap <= BAT_LOW_LIFE_MAP_START) {
		bat_life = bat_low_life_map_tbl[nowCap];
		pr_info("[BAT][Fil]%s(), remap cap from %d to %d \n", __func__, nowCap, bat_life);
		return bat_life;
	}
		

	/* Only when lastCap more than 88, use slow rise algo and use slow down algo*/
	if (lastCap >= 88) {		
		if (hasCable) {	// has cable, use slow rise algo
			bat_life = eval_dock_bat_life_when_charging(nowCap, lastCap, maxMah, interval, this->capMah);
		} else {	// no cable, use slow down algo
			bat_life = eval_dock_bat_life_when_discharging(nowCap, lastCap, maxMah, interval, this->capMah);
		}
	} else {
		bat_life = nowCap;
	}

	pr_debug("[BAT][Fil]%s(), filter cap:%d \n", __func__, bat_life);
		
	return bat_life;

}

extern void AXC_Cap_Filter_Dock_Constructor(AXI_Cap_Filter *apCapFilter, int filterType, int capMah)
{

	AXC_Cap_Filter_Dock *this = container_of(apCapFilter, AXC_Cap_Filter_Dock, parentCapFilter);
	this->capMah = capMah;
	this->filterType = filterType;
	this->parentCapFilter.getType = AXC_Cap_Filter_Dock_GetType;
	this->parentCapFilter.filterCapacity = AXC_Cap_Filter_Dock_FilterCapacity;

}


