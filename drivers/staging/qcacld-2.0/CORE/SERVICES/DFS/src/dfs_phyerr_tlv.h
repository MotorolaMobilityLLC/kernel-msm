/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================

                     dfs_phyerr_tlv.h

  OVERVIEW:

  Source code borrowed from QCA_MAIN DFS module

  DEPENDENCIES:

  Are listed for each API below.

===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when        who     what, where, why
----------    ---    --------------------------------------------------------

===========================================================================*/
#ifndef	__DFS_PHYERR_PEREGRINE_H__
#define	__DFS_PHYERR_PEREGRINE_H__

/*
 * Register manipulation macros that expect bit field defines
 * to follow the convention that an _S suffix is appended for
 * a shift count, while the field mask has no suffix.
 */
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)

/*
 * The TLV dword is at the beginning of each TLV section.
 */
#define	TLV_REG		0x00

#define	TLV_LEN		0x0000FFFF
#define	TLV_LEN_S	0

#define	TLV_SIG		0x00FF0000
#define	TLV_SIG_S	16

#define	TLV_TAG		0xFF000000
#define	TLV_TAG_S	24

#define TAG_ID_SEARCH_FFT_REPORT        0xFB
#define TAG_ID_RADAR_PULSE_SUMMARY      0xF8
/*
 * Radar pulse summary
 *
 * + TYPE=0xF8 (Radar pulse summary reprot)
 * + SIG=0xBB (baseband PHY generated TLV components)
 */

#define	RADAR_REPORT_PULSE_REG_1	0x00

#define		RADAR_REPORT_PULSE_IS_CHIRP		0x80000000
#define		RADAR_REPORT_PULSE_IS_CHIRP_S		31

#define		RADAR_REPORT_PULSE_IS_MAX_WIDTH		0x40000000
#define		RADAR_REPORT_PULSE_IS_MAX_WIDTH_S	30

#define		RADAR_REPORT_AGC_TOTAL_GAIN		0x3FF00000
#define		RADAR_REPORT_AGC_TOTAL_GAIN_S		20

#define		RADAR_REPORT_PULSE_DELTA_DIFF		0x000F0000
#define		RADAR_REPORT_PULSE_DELTA_DIFF_S		16

#define		RADAR_REPORT_PULSE_DELTA_PEAK		0x0000FC00
#define		RADAR_REPORT_PULSE_DELTA_PEAK_S		10

#define		RADAR_REPORT_PULSE_SIDX			0x000003FF
#define		RADAR_REPORT_PULSE_SIDX_S		0x0

#define	RADAR_REPORT_PULSE_REG_2	0x01

#define		RADAR_REPORT_PULSE_SRCH_FFT_A_VALID	0x80000000
#define		RADAR_REPORT_PULSE_SRCH_FFT_A_VALID_S	31

#define		RADAR_REPORT_PULSE_AGC_MB_GAIN		0x7F000000
#define		RADAR_REPORT_PULSE_AGC_MB_GAIN_S	24

#define		RADAR_REPORT_PULSE_SUBCHAN_MASK		0x00FF0000
#define		RADAR_REPORT_PULSE_SUBCHAN_MASK_S	16

#define		RADAR_REPORT_PULSE_TSF_OFFSET		0x0000FF00
#define		RADAR_REPORT_PULSE_TSF_OFFSET_S		8

#define		RADAR_REPORT_PULSE_DUR			0x000000FF
#define		RADAR_REPORT_PULSE_DUR_S		0

#define SEARCH_FFT_REPORT_REG_1     0x00


#define         SEARCH_FFT_REPORT_TOTAL_GAIN_DB     0xFF800000
#define         SEARCH_FFT_REPORT_TOTAL_GAIN_DB_S   23

#define         SEARCH_FFT_REPORT_BASE_PWR_DB       0x007FC000
#define         SEARCH_FFT_REPORT_BASE_PWR_DB_S     14

#define         SEARCH_FFT_REPORT_FFT_CHN_IDX       0x00003000
#define         SEARCH_FFT_REPORT_FFT_CHN_IDX_S     12

#define         SEARCH_FFT_REPORT_PEAK_SIDX         0x00000FFF
#define         SEARCH_FFT_REPORT_PEAK_SIDX_S       0

#define SEARCH_FFT_REPORT_REG_2     0x01

#define         SEARCH_FFT_REPORT_RELPWR_DB         0xFC000000
#define         SEARCH_FFT_REPORT_RELPWR_DB_S       26

#define         SEARCH_FFT_REPORT_AVGPWR_DB         0x03FC0000
#define         SEARCH_FFT_REPORT_AVGPWR_DB_S       18

#define         SEARCH_FFT_REPORT_PEAK_MAG          0x0003FF00
#define         SEARCH_FFT_REPORT_PEAK_MAG_S        8

#define         SEARCH_FFT_REPORT_NUM_STR_BINS_IB   0x000000FF
#define         SEARCH_FFT_REPORT_NUM_STR_BINS_IB_S 0



/*
 * Although this code is now not parsing the whole frame (descriptor
 * and all), the relevant fields are still useful information
 * for anyone who is working on the PHY error part of DFS pattern
 * matching.
 *
 * However, to understand _where_ these descriptors start, you
 * should do some digging into the peregrine descriptor format.
 * The 30 second version: each RX ring has a bitmap listing which
 * descriptors are to be included, and then a set of offsets
 * into the RX buffer for where each descriptor will be written.
 * It's not like the 802.11n generation hardware which has
 * a fixed descriptor format.
 */

/*
 * RX_PPDU_START
 */
#define	RX_PPDU_START_LEN		(10*4)

#define	RX_PPDU_START_REG_4		0x0004
#define		RX_PPDU_START_RSSI_COMB			0x000000FF
#define		RX_PPDU_START_RSSI_COMB_S		0

/*
 * RX_PPDU_END
 */
#define	RX_PPDU_END_LEN			(21*4)

#define	RX_PPDU_END_REG_16		16
#define		RX_PPDU_END_TSF_TIMESTAMP		0xFFFFFFFF
#define		RX_PPDU_END_TSF_TIMESTAMP_S		0

#define	RX_PPDU_END_REG_18		18
#define		RX_PPDU_END_PHY_ERR_CODE		0x0000FF00
#define		RX_PPDU_END_PHY_ERR_CODE_S		8
#define		RX_PPDU_END_PHY_ERR			0x00010000
#define		RX_PPDU_END_PHY_ERR_S			16

/*
 * The RSSI values can have "special meanings".
 *
 * If rssi=50, it means that the peak detector triggered.
 */
#define	RSSI_PEAK_DETECTOR_SAT		50

/*
 *
 * If rssi=25, it means that the ADC was saturated, but that only is
 * valid when there is one ADC gain change.  For short pulses this
 * is true - you won't have time to do a gain change before the pulse
 * goes away.  But for longer pulses, ADC gain changes can occur, so
 * you'll get a more accurate RSSI figure.
 *
 * For short pulses (and the definition of "short" still isn't clear
 * at the time of writing) there isn't any real time to do a gain change
 * (or two, or three..) in order to get an accurate estimation of signal
 * sizing.  Thus, RSSI will not be very accurate for short duration pulses.
 * All you can really say for certain is that yes, there's a pulse that
 * met the requirements of the pulse detector.
 *
 * For more information, see the 802.11ac Microarchitecture guide.
 * (TODO: add a twiki reference.)
 */

#endif	/* __DFS_PHYERR_PEREGRINE_H__ */
