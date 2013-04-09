/*
 * Linux roam cache
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: wl_roam.c 334946 2012-05-24 20:38:00Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include <wl_cfg80211.h>
#include <wldev_common.h>

#define MAX_ROAM_CACHE		100
#define MAX_CHANNEL_LIST	20
#define MAX_SSID_BUFSIZE	36

#define ROAMSCAN_MODE_NORMAL	0
#define ROAMSCAN_MODE_WES		1

typedef struct {
	chanspec_t chanspec;
	int ssid_len;
	char ssid[MAX_SSID_BUFSIZE];
} roam_channel_cache;

typedef struct {
	int n;
	chanspec_t channels[MAX_CHANNEL_LIST];
} channel_list_t;

static int n_roam_cache = 0;
static int roam_band = WLC_BAND_AUTO;
static roam_channel_cache roam_cache[MAX_ROAM_CACHE];
#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
static int roamscan_mode = ROAMSCAN_MODE_NORMAL;
#endif

#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
int get_roamscan_mode(struct net_device *dev, int *mode)
{
	*mode = roamscan_mode;

	return 0;
}

int set_roamscan_mode(struct net_device *dev, int mode)
{
	int error = 0;
	roamscan_mode = mode;
	n_roam_cache = 0;

	error = wldev_iovar_setint(dev, "roamscan_mode", mode);
	if (error) {
		WL_ERR(("Failed to set roamscan mode to %d, error = %d\n", mode, error));
	}

	return error;
}

int get_roamscan_channel_list(struct net_device *dev, unsigned char channels[])
{
	int n = 0;

	if (roamscan_mode == ROAMSCAN_MODE_WES) {
		for (n = 0; n < n_roam_cache; n++) {
			channels[n] = roam_cache[n].chanspec & WL_CHANSPEC_CHAN_MASK;

			WL_DBG(("channel[%d] - [%02d] \n", n, channels[n]));
		}
	}

	return n;
}

int set_roamscan_channel_list(struct net_device *dev,
	unsigned char n, unsigned char channels[], int ioctl_ver)
{
	int i;
	int error;
	channel_list_t channel_list;
	char iobuf[WLC_IOCTL_SMLEN];
	uint band2G, band5G, bw;
	roamscan_mode = ROAMSCAN_MODE_WES;

	if (n > MAX_CHANNEL_LIST)
		n = MAX_CHANNEL_LIST;

#ifdef D11AC_IOTYPES
	if (ioctl_ver == 1) {
		/* legacy chanspec */
		band2G = WL_LCHANSPEC_BAND_2G;
		band5G = WL_LCHANSPEC_BAND_5G;
		bw = WL_LCHANSPEC_BW_20 | WL_LCHANSPEC_CTL_SB_NONE;
	} else {
		band2G = WL_CHANSPEC_BAND_2G;
		band5G = WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
	}
#else
	band2G = WL_CHANSPEC_BAND_2G;
	band5G = WL_CHANSPEC_BAND_5G;
	bw = WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE;
#endif /* D11AC_IOTYPES */

	for (i = 0; i < n; i++) {
		chanspec_t chanspec;

		if (channels[i] <= CH_MAX_2G_CHANNEL) {
			chanspec = band2G | bw | channels[i];
		} else {
			chanspec = band5G | bw | channels[i];
		}
		roam_cache[i].chanspec = chanspec;
		channel_list.channels[i] = chanspec;

		WL_DBG(("channel[%d] - [%02d] \n", i, channels[i]));
	}

	n_roam_cache = n;
	channel_list.n = n;

	/* need to set ROAMSCAN_MODE_NORMAL to update roamscan_channels,
	   otherwise, it won't be updated
	*/
	wldev_iovar_setint(dev, "roamscan_mode", ROAMSCAN_MODE_NORMAL);
	error = wldev_iovar_setbuf(dev, "roamscan_channels", &channel_list,
		sizeof(channel_list), iobuf, sizeof(iobuf), NULL);
	if (error) {
		WL_DBG(("Failed to set roamscan channels, error = %d\n", error));
	}
	wldev_iovar_setint(dev, "roamscan_mode", ROAMSCAN_MODE_WES);

	return error;
}
#endif /* WES_SUPPORT */

void set_roam_band(int band)
{
	roam_band = band;
}

void reset_roam_cache(void)
{
#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
	if (roamscan_mode == ROAMSCAN_MODE_WES)
		return;
#endif

	n_roam_cache = 0;
}

void add_roam_cache(wl_bss_info_t *bi)
{
	int i;
	uint8 channel;

#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
	if (roamscan_mode == ROAMSCAN_MODE_WES)
		return;
#endif

	if (n_roam_cache >= MAX_ROAM_CACHE)
		return;

	for (i = 0; i < n_roam_cache; i++) {
		if ((roam_cache[i].ssid_len == bi->SSID_len) &&
			(roam_cache[i].chanspec == bi->chanspec) &&
			(memcmp(roam_cache[i].ssid, bi->SSID, bi->SSID_len) == 0)) {
			/* identical one found, just return */
			return;
		}
	}

	roam_cache[n_roam_cache].ssid_len = bi->SSID_len;
	channel = (bi->ctl_ch == 0) ? CHSPEC_CHANNEL(bi->chanspec) : bi->ctl_ch;
	WL_DBG(("CHSPEC 0x%X %d, CTL %d\n",
		bi->chanspec, CHSPEC_CHANNEL(bi->chanspec), bi->ctl_ch));
	roam_cache[n_roam_cache].chanspec =
		WL_CHANSPEC_BW_20 |
		(channel <= CH_MAX_2G_CHANNEL ? WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G) |
		channel;
	memcpy(roam_cache[n_roam_cache].ssid, bi->SSID, bi->SSID_len);

	n_roam_cache++;
}

int get_roam_channel_list(int target_chan,
	chanspec_t *channels, const wlc_ssid_t *ssid, int ioctl_ver)
{
	int i, n = 1;
	uint band, band2G, band5G, bw;

#ifdef D11AC_IOTYPES
	if (ioctl_ver == 1) {
		/* legacy chanspec */
		band2G = WL_LCHANSPEC_BAND_2G;
		band5G = WL_LCHANSPEC_BAND_5G;
		bw = WL_LCHANSPEC_BW_20 | WL_LCHANSPEC_CTL_SB_NONE;
	} else {
		band2G = WL_CHANSPEC_BAND_2G;
		band5G = WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
	}
#else
	band2G = WL_CHANSPEC_BAND_2G;
	band5G = WL_CHANSPEC_BAND_5G;
	bw = WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE;
#endif /* D11AC_IOTYPES */

	if (target_chan <= CH_MAX_2G_CHANNEL)
		band = band2G;
	else
		band = band5G;
	*channels = (target_chan & WL_CHANSPEC_CHAN_MASK) | band | bw;
	WL_DBG(("%02d 0x%04X\n", target_chan, *channels));
	++channels;

#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
	if (roamscan_mode == ROAMSCAN_MODE_WES) {
		for (i = 0; i < n_roam_cache; i++) {
			if ((roam_cache[i].chanspec & WL_CHANSPEC_CHAN_MASK) != target_chan) {
				*channels = roam_cache[i].chanspec & WL_CHANSPEC_CHAN_MASK;
				WL_DBG(("%02d\n", *channels));
				if (*channels <= CH_MAX_2G_CHANNEL)
					*channels |= band2G | bw;
				else
					*channels |= band5G | bw;

				channels++;
				n++;
			}
		}

		return n;
	}
#endif /* WES_SUPPORT */

	for (i = 0; i < n_roam_cache; i++) {
		chanspec_t ch = roam_cache[i].chanspec;
		if ((roam_cache[i].ssid_len == ssid->SSID_len) &&
			((ch & WL_CHANSPEC_CHAN_MASK) != target_chan) &&
			((roam_band == WLC_BAND_AUTO) ||
			((roam_band == WLC_BAND_2G) && CHSPEC_IS2G(ch)) ||
			((roam_band == WLC_BAND_5G) && CHSPEC_IS5G(ch))) &&
			(memcmp(roam_cache[i].ssid, ssid->SSID, ssid->SSID_len) == 0)) {
			/* match found, add it */
			*channels = ch & WL_CHANSPEC_CHAN_MASK;
			if (*channels <= CH_MAX_2G_CHANNEL)
				*channels |= band2G | bw;
			else
				*channels |= band5G | bw;

			WL_DBG(("%02d 0x%04X\n", ch & WL_CHANSPEC_CHAN_MASK, *channels));

			channels++; n++;
		}
	}

	return n;
}


void print_roam_cache(void)
{
	int i;

	WL_DBG(("%d cache\n", n_roam_cache));

	for (i = 0; i < n_roam_cache; i++) {
		roam_cache[i].ssid[roam_cache[i].ssid_len] = 0;
		WL_DBG(("0x%02X %02d %s\n", roam_cache[i].chanspec,
			roam_cache[i].ssid_len, roam_cache[i].ssid));
	}
}

static void add_roamcache_channel(channel_list_t *channels, chanspec_t ch)
{
	int i;

	if (channels->n >= MAX_CHANNEL_LIST) /* buffer full */
		return;

	for (i = 0; i < channels->n; i++) {
		if (channels->channels[i] == ch) /* already in the list */
			return;
	}

	channels->channels[i] = ch;
	channels->n++;

	WL_DBG((" RCC: %02d 0x%04X\n",
		ch & WL_CHANSPEC_CHAN_MASK, ch));
}

void update_roam_cache(struct wl_priv *wl, int ioctl_ver)
{
	int error, i, prev_channels;
	channel_list_t channel_list;
	char iobuf[WLC_IOCTL_SMLEN];
	struct net_device *dev = wl_to_prmry_ndev(wl);
	wlc_ssid_t ssid;
	uint band2G, band5G, bw;

#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
	if (roamscan_mode == ROAMSCAN_MODE_WES) {
		/* no update when ROAMSCAN_MODE_WES */
		return;
	}
#endif

	if (!wl_get_drv_status(wl, CONNECTED, dev)) {
		WL_DBG(("Not associated\n"));
		return;
	}

	/* need to read out the current cache list
	   as the firmware may change dynamically
	*/
	error = wldev_iovar_getbuf(dev, "roamscan_channels", 0, 0,
		(void *)&channel_list, sizeof(channel_list), NULL);

	WL_DBG(("%d AP, %d cache item(s), err=%d\n", n_roam_cache, channel_list.n, error));

	error = wldev_get_ssid(dev, &ssid);
	if (error) {
		WL_ERR(("Failed to get SSID, err=%d\n", error));
		return;
	}

#ifdef D11AC_IOTYPES
	if (ioctl_ver == 1) {
		/* legacy chanspec */
		band2G = WL_LCHANSPEC_BAND_2G;
		band5G = WL_LCHANSPEC_BAND_5G;
		bw = WL_LCHANSPEC_BW_20 | WL_LCHANSPEC_CTL_SB_NONE;
	} else {
		band2G = WL_CHANSPEC_BAND_2G;
		band5G = WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
	}
#else
	band2G = WL_CHANSPEC_BAND_2G;
	band5G = WL_CHANSPEC_BAND_5G;
	bw = WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE;
#endif /* D11AC_IOTYPES */

	prev_channels = channel_list.n;
	for (i = 0; i < n_roam_cache; i++) {
		chanspec_t ch = roam_cache[i].chanspec;
		if ((roam_cache[i].ssid_len == ssid.SSID_len) &&
			((roam_band == WLC_BAND_AUTO) ||
			((roam_band == WLC_BAND_2G) && CHSPEC_IS2G(ch)) ||
			((roam_band == WLC_BAND_5G) && CHSPEC_IS5G(ch))) &&
			(memcmp(roam_cache[i].ssid, ssid.SSID, ssid.SSID_len) == 0)) {
			/* match found, add it */
			ch &= WL_CHANSPEC_CHAN_MASK;
			if (ch <= CH_MAX_2G_CHANNEL)
				ch |= band2G | bw;
			else
				ch |= band5G | bw;

			add_roamcache_channel(&channel_list, ch);
		}
	}
	if (prev_channels != channel_list.n) {
		/* channel list updated */
		error = wldev_iovar_setbuf(dev, "roamscan_channels", &channel_list,
			sizeof(channel_list), iobuf, sizeof(iobuf), NULL);
		if (error) {
			WL_ERR(("Failed to update roamscan channels, error = %d\n", error));
		}
	}
}
