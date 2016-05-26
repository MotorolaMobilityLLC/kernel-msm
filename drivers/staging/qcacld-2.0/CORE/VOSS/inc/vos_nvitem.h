/*
 * Copyright (c) 2011-2016 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_NVITEM_H )
#define __VOS_NVITEM_H

/**=========================================================================

  \file  vos_nvitem.h

  \brief virtual Operating System Services (vOSS): Non-Volatile storage API

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_types.h"
#include "vos_status.h"
#include "wlan_nv.h"

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_COUNTRY_CODE_LEN  2
#define VOS_MAC_ADDRESS_LEN   6

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
// enum of regulatory doamains in WLAN
typedef enum
{
   REGDOMAIN_FCC,
   REGDOMAIN_ETSI,
   REGDOMAIN_JAPAN,
   REGDOMAIN_WORLD,
   REGDOMAIN_N_AMER_EXC_FCC,
   REGDOMAIN_APAC,
   REGDOMAIN_KOREA,
   REGDOMAIN_HI_5GHZ,
   REGDOMAIN_NO_5GHZ,
   // add new regulatory domain here
   REGDOMAIN_COUNT
}
v_REGDOMAIN_t;

typedef enum
{
   COUNTRY_INIT,
   COUNTRY_IE,
   COUNTRY_USER,
   COUNTRY_CELL_BASE,
   //add new sources here
   COUNTRY_QUERY,
   COUNTRY_MAX = COUNTRY_QUERY
}
v_CountryInfoSource_t;

typedef enum
{
   /* DFS Session refers to successful SAP session operating in DFS channel */
   DFS_CAC_NEVER_DONE,   /* CAC was never done for this current ap / ap-ap */
   DFS_CAC_IN_PROGRESS,  /* CAC is in progress for this DFS session */
   DFS_CAC_ALREADY_DONE, /* CAC already for SAP starting this DFS session */
}
eDFS_CAC_STATUS;

/**
 * struct chan_to_ht_40_index_map - information of secondary channel index
 *
 * @ht_40_minus_index: HT40- channel index
 * @ht_40_plus_index: HT40+ channel index
 */
struct chan_to_ht_40_index_map {
   uint16 ht_40_minus_index;
   uint16 ht_40_plus_index;
};

/**
 * enum phy_ch_width - physical layer channel width
 * @CH_WIDTH_20MHZ: channel width 20 mhz
 * @CH_WIDTH_40MHZ: channel width 40 mhz
 * @CH_WIDTH_80MHZ: channel width 80 mhz
 * @CH_WIDTH_5MHZ: channel width 5 mhz
 * @CH_WIDTH_10MHZ: channel width 10 mhz
 * @CH_WIDTH_INVALID: invalid channel width
 * @CH_WIDTH_MAX: maximum channel width
 */
enum phy_ch_width {
	CH_WIDTH_20MHZ = 0,
	CH_WIDTH_40MHZ,
	CH_WIDTH_80MHZ,
	CH_WIDTH_5MHZ,
	CH_WIDTH_10MHZ,
	CH_WIDTH_INVALID,
	CH_WIDTH_MAX
};

/**
 * struct ch_params_s
 * @ch_width: channel width
 * @sec_ch_offset: secondary channel offset
 * @center_freq_seg0: center freq for segment 0
 * @center_freq_seg1: center freq for segment 1
 */
struct ch_params_s {
	enum phy_ch_width ch_width;
	uint8_t sec_ch_offset;
	uint32_t center_freq_seg0;
	uint32_t center_freq_seg1;
};

// country code type
typedef v_U8_t v_COUNTRYCODE_t[VOS_COUNTRY_CODE_LEN];

void vos_set_channel_params(uint16_t oper_ch, uint16_t sec_ch_2g,
	struct ch_params_s *ch_params);

/**------------------------------------------------------------------------

  \brief vos_nv_getRegDomainFromCountryCode() - get the regulatory domain of
  a country given its country code

  The \a vos_nv_getRegDomainFromCountryCode() returns the regulatory domain of
  a country given its country code.  This is done from reading a cached
  copy of the binary file.

  \param pRegDomain  - pointer to regulatory domain

  \param countryCode - country code

  \param source      - source of country code

  \return VOS_STATUS_SUCCESS - regulatory domain is found for the given country
          VOS_STATUS_E_FAULT - invalid pointer error
          VOS_STATUS_E_EMPTY - country code table is empty
          VOS_STATUS_E_EXISTS - given country code does not exist in table

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getRegDomainFromCountryCode( v_REGDOMAIN_t *pRegDomain,
      const v_COUNTRYCODE_t countryCode, v_CountryInfoSource_t source);

/**------------------------------------------------------------------------

  \brief vos_nv_getSupportedCountryCode() - get the list of supported
  country codes

  The \a vos_nv_getSupportedCountryCode() encodes the list of supported
  country codes with paddings in the provided buffer

  \param pBuffer     - pointer to buffer where supported country codes
                       and paddings are encoded; this may be set to NULL
                       if user wishes to query the required buffer size to
                       get the country code list

  \param pBufferSize - this is the provided buffer size on input;
                       this is the required or consumed buffer size on output

  \return VOS_STATUS_SUCCESS - country codes are successfully encoded
          VOS_STATUS_E_NOMEM - country codes are not encoded because either
                               the buffer is NULL or buffer size is
                               sufficient
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedCountryCode( v_BYTE_t *pBuffer, v_SIZE_t *pBufferSize,
      v_SIZE_t paddingSize );

/**------------------------------------------------------------------------
  \brief vos_nv_getDefaultRegDomain() - return the default regulatory domain
  \return default regulatory domain
  \sa
  -------------------------------------------------------------------------*/
v_REGDOMAIN_t vos_nv_getDefaultRegDomain( void );

/**------------------------------------------------------------------------
  \brief vos_nv_getSupportedChannels() - function to return the list of
          supported channels
  \param p20MhzChannels - list of 20 Mhz channels
  \param pNum20MhzChannels - number of 20 Mhz channels
  \param p40MhzChannels - list of 20 Mhz channels
  \param pNum40MhzChannels - number of 20 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedChannels( v_U8_t *p20MhzChannels, int *pNum20MhzChannels,
                                        v_U8_t *p40MhzChannels, int *pNum40MhzChannels);

/**------------------------------------------------------------------------
  \brief vos_nv_readDefaultCountryTable() - return the default Country table
  \param table data - a union to return the default country table data in.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readDefaultCountryTable( uNvTables *tableData );

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelListWithPower() - function to return the list of
          supported channels with the power limit info too.
  \param pChannels20MHz - list of 20 Mhz channels
  \param pNum20MHzChannelsFound - number of 20 Mhz channels
  \param pChannels40MHz - list of 20 Mhz channels
  \param pNum40MHzChannelsFound - number of 20 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getChannelListWithPower(tChannelListWithPower *pChannels20MHz /*[NUM_LEGIT_RF_CHANNELS] */,
                                          tANI_U8 *pNum20MHzChannelsFound,
                                          tChannelListWithPower *pChannels40MHz /*[NUM_CHAN_BOND_CHANNELS] */,
                                          tANI_U8 *pNum40MHzChannelsFound
                                          );

/**------------------------------------------------------------------------

  \brief vos_nv_open() - initialize the NV module

  The \a vos_nv_open() initializes the NV module.  This function read the binary
  file qcom_nv.bin for macaddress,country code,regulatory domain information and etc.

  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_open(void);

/**------------------------------------------------------------------------

  \brief vos_nv_close() - uninitialize the NV module

  The \a vos_nv_init() uninitializes the NV module.  This function release the binary
  file qcom_nv.bin data buffer.

  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa

  -------------------------------------------------------------------------*/

VOS_STATUS vos_nv_close(void);

/**------------------------------------------------------------------------
  \brief vos_nv_setRegDomain -
  \param clientCtxt  - Client Context, Not used for PRIMA
              regId  - Regulatory Domain ID
              sendRegHint - send hint to cfg80211
  \return status set REG domain operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setRegDomain(void * clientCtxt, v_REGDOMAIN_t regId,
                                                  v_BOOL_t sendRegHint);

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelEnabledState -
  \param rfChannel  - input channel number to know enabled state
  \return eNVChannelEnabledType enabled state for channel
             * enabled
             * disabled
             * DFS
  \sa
  -------------------------------------------------------------------------*/
eNVChannelEnabledType vos_nv_getChannelEnabledState(v_U32_t rfChannel);

uint8_t vos_nv_skip_dsrc_dfs_2g(uint32_t rf_channel, int32_t skip_group);
VOS_STATUS vos_nv_get_dfs_region(uint8_t *dfs_region);
VOS_STATUS vos_nv_set_dfs_region(uint8_t dfs_region);

#define VOS_IS_DFS_CH(channel) (vos_nv_getChannelEnabledState((channel)) == \
                                   NV_CHANNEL_DFS)

VOS_STATUS vos_init_wiphy_from_nv_bin(void);
const char * voss_DomainIdtoString(const v_U8_t domainIdCurrent);
VOS_STATUS vos_init_wiphy_from_eeprom(void);
bool vos_is_dsrc_channel(uint16_t);
uint32_t vos_nv_get_channel_flags(uint32_t rf_channel);
int vos_update_band(uint8_t  band);
#endif // __VOS_NVITEM_H
