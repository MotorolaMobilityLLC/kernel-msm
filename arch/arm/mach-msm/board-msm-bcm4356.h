#ifdef CONFIG_BCM4356
extern int brcm_wlan_power(int onoff);
#else
inline int brcm_wlan_power(int onoff)
{
	return -ENODEV;
}
#endif
