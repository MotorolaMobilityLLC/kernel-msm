//ASUS_BSP +++ Josh_Liao "sw gauge v2"
/*
*    Capacity filter interface include File
*
*/
#ifndef __AXI_CAPACITYFILTER_H__
#define __AXI_CAPACITYFILTER_H__

/* filterCapacity - filter capacity with specific algorithm. The main purpose is to make capacity changed smoothly. It is
*     synchronization.
* @nowCap: capacity for now 
* @lastCap: capacity from last updated
* @hasCable: hasCable?
* @maxMah: maximum current(MA) per hour. It shall be max charging current when charing, max discharging current when discharging.
*     Alwasy be positive value. Be care of the value when suspend, it shall be a small value.
* @interval: interval since last updated. The unit is second.
* RETURNS: estimated battery capacity.
*/
extern int filterCapacity(int nowCap, int lastCap, bool hasCable, bool isCharing, bool isBatFull, bool isBatLow, int maxMah, int interval);

/* BAT_CAP_MAH - battery capacity, which unit is maH. It shall be modified by different battery.
  */
#define BAT_CAP_MAH      1500

#endif // __AXI_CAPACITYFILTER_H__

//ASUS_BSP --- Josh_Liao "sw gauge v2"