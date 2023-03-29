/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_CHAINMASK_H
#define __CFG_CHAINMASK_H

/*
 * <ini>
 * gSetTxChainmask1x1 - Sets Transmit chain mask.
 * @Min: 1
 * @Max: 3
 * @Default: 0
 *
 * This ini Sets Transmit chain mask.
 *
 * If gEnable2x2 is disabled, gSetTxChainmask1x1 and gSetRxChainmask1x1 values
 * are taken into account. If chainmask value exceeds the maximum number of
 * chains supported by target, the max number of chains is used. By default,
 * chain0 is selected for both Tx and Rx.
 * gSetTxChainmask1x1=1 or gSetRxChainmask1x1=1 to select chain0.
 * gSetTxChainmask1x1=2 or gSetRxChainmask1x1=2 to select chain1.
 * gSetTxChainmask1x1=3 or gSetRxChainmask1x1=3 to select both chains.
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_1x1_TX_CHAINMASK CFG_INI_UINT( \
				"gSetTxChainmask1x1", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"1x1 VHT Tx Chainmask")

/*
 * <ini>
 * gSetRxChainmask1x1 - Sets Receive chain mask.
 * @Min: 1
 * @Max: 3
 * @Default: 0
 *
 * This ini is  used to set Receive chain mask.
 *
 * If gEnable2x2 is disabled, gSetTxChainmask1x1 and gSetRxChainmask1x1 values
 * are taken into account. If chainmask value exceeds the maximum number of
 * chains supported by target, the max number of chains is used. By default,
 * chain0 is selected for both Tx and Rx.
 * gSetTxChainmask1x1=1 or gSetRxChainmask1x1=1 to select chain0.
 * gSetTxChainmask1x1=2 or gSetRxChainmask1x1=2 to select chain1.
 * gSetTxChainmask1x1=3 or gSetRxChainmask1x1=3 to select both chains.
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_1x1_RX_CHAINMASK CFG_INI_UINT( \
				"gSetRxChainmask1x1", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"1x1 VHT Rx Chainmask")

/*
 * <ini>
 * gCckChainMaskEnable - Used to enable/disable Cck ChainMask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default Cck ChainMask
 * 0: disable the cck tx chain mask (default)
 * 1: enable the cck tx chain mask
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_CCK CFG_INI_BOOL( \
			"gCckChainMaskEnable", \
			0, \
			"Set default CCK Tx Chainmask")

/*
 * <ini>
 * gTxChainMask1ss - Enables/disables tx chain mask1ss, used by Rome
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini is used to set default tx chain mask for 1ss
 *
 * gTxChainMask1ss=0 : 1ss data tx chain mask set to 3 and self gen chain mask
 *    set to 3. This is default setting of fw side. For 1x1 case, WIFI will
 *    using chain0 to sent 1ss data and selfgen packets. 2x2 case, WIFI will
 *    using chain0 and chain1 to sent 1ss data and selfgen packets.
 *
 * gTxChainMask1ss=1 : 1ss data tx chain mask set to 2 and self gen chain mask
 *    set to 2. This setting can work only when 2x2 case, WIFI will use chain1
 *    to sent 1ss data packets and selfgen packets, this can improve BTC
 *    performance a little, but have side affect when chain0 and chain1 RSSI
 *    is unbalance or green AP is enabled. So we recommend not using it.
 *
 * gTxChainMask1ss=2 : 1ss data tx chain mask set to 3 and self gen chain mask
 * set to 2. This setting never used before.
 *
 * gTxChainMask1ss=3 : 1ss data tx chain mask set to 2 and self gen chain mask
 * set to 3. This setting never used before.
 *
 * Related: None
 *
 * Supported Feature: STA/SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_1SS CFG_INI_UINT( \
			"gTxChainMask1ss", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"1SS Tx Chainmask")

/*
 * <ini>
 * g11bNumTxChains - Number of Tx Chanins in 11b mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Number of Tx Chanins in 11b mode
 *
 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11B_NUM_TX_CHAIN CFG_INI_UINT( \
			"g11bNumTxChains", \
			0, \
			2, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"11b Num Tx chains")

/*
 * <ini>
 * g11agNumTxChains - Number of Tx Chanins in 11ag mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Number of Tx Chanins in 11ag mode
 *
 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11AG_NUM_TX_CHAIN CFG_INI_UINT( \
			"g11agNumTxChains", \
			0, \
			2, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"11ag Num Tx chains")

/*
 * <ini>
 * tx_chain_mask_2g - tx chain mask for 2g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set tx chain mask for 2g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * tx_chain_mask_2g=0 : don't care
 * tx_chain_mask_2g=1 : for 2g tx use chain 0
 * tx_chain_mask_2g=2 : for 2g tx use chain 1
 * tx_chain_mask_2g=3 : for 2g tx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_2G CFG_INI_UINT( \
			"tx_chain_mask_2g", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"2.4G Tx Chainmask")

/*
 * <ini>
 * rx_chain_mask_2g - rx chain mask for 2g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set rx chain mask for 2g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * rx_chain_mask_2g=0 : don't care
 * rx_chain_mask_2g=1 : for 2g rx use chain 0
 * rx_chain_mask_2g=2 : for 2g rx use chain 1
 * rx_chain_mask_2g=3 : for 2g rx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_CHAIN_MASK_2G CFG_INI_UINT( \
			"rx_chain_mask_2g", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"2.4G Rx Chainmask")

/*
 * <ini>
 * tx_chain_mask_5g - tx chain mask for 5g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set tx chain mask for 5g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * tx_chain_mask_5g=0 : don't care
 * tx_chain_mask_5g=1 : for 5g tx use chain 0
 * tx_chain_mask_5g=2 : for 5g tx use chain 1
 * tx_chain_mask_5g=3 : for 5g tx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_5G CFG_INI_UINT( \
			"tx_chain_mask_5g", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"5Ghz Tx Chainmask")

/*
 * <ini>
 * rx_chain_mask_5g - rx chain mask for 5g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set rx chain mask for 5g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * rx_chain_mask_5g=0 : don't care
 * rx_chain_mask_5g=1 : for 5g rx use chain 0
 * rx_chain_mask_5g=2 : for 5g rx use chain 1
 * rx_chain_mask_5g=3 : for 5g rx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_CHAIN_MASK_5G CFG_INI_UINT( \
			"rx_chain_mask_5g", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"5Ghz Tx Chainmask")

/*
 * <ini>
 * enable_bt_chain_separation - Enables/disables bt /wlan chainmask assignment
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini disables/enables chainmask setting on 2x2, mainly used for ROME
 * BT/WLAN chainmask assignment.
 *
 * 0, Disable
 * 1, Enable
 *
 * Related: NA
 *
 * Supported Feature: 11n/11ac
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BT_CHAIN_SEPARATION CFG_INI_BOOL( \
				"enableBTChainSeparation", \
				0, \
				"Enable/disable BT chainmask assignment")

#define CFG_CHAINMASK_ALL \
	CFG(CFG_VHT_ENABLE_1x1_TX_CHAINMASK) \
	CFG(CFG_VHT_ENABLE_1x1_RX_CHAINMASK) \
	CFG(CFG_TX_CHAIN_MASK_CCK) \
	CFG(CFG_TX_CHAIN_MASK_1SS) \
	CFG(CFG_11B_NUM_TX_CHAIN) \
	CFG(CFG_11AG_NUM_TX_CHAIN) \
	CFG(CFG_TX_CHAIN_MASK_2G) \
	CFG(CFG_RX_CHAIN_MASK_2G) \
	CFG(CFG_TX_CHAIN_MASK_5G) \
	CFG(CFG_RX_CHAIN_MASK_5G) \
	CFG(CFG_ENABLE_BT_CHAIN_SEPARATION)

#endif /* __CFG_CHAINMASK_H */
