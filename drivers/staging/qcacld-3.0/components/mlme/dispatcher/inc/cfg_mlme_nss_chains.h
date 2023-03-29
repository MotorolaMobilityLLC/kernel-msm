/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_NSS_CHAINS
#define __CFG_MLME_NSS_CHAINS

/*
 * <ini>
 * num_tx_chains_2g - Config Param to change number of tx
 * chains per vdev for 2.4ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num tx chains for 2.4ghz connection to 1 each
 * 0x02492492 - change all vdev's num tx chains for 2.4ghz connection to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_2G CFG_INI_UINT( \
				"num_tx_chains_2g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num tx chains 2g")

/*
 * <ini>
 * num_tx_chains_5g - Config Param to change number of tx
 * chains per vdev for 5 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249- change all vdev's tx num chains for 5ghz connection to 1 each
 * 0x02492492 - change all vdev's tx num chains for 5ghz connection to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_5G CFG_INI_UINT( \
				"num_tx_chains_5g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num tx chains 5g")

/*
 * <ini>
 * num_rx_chains_2g - Config Param to change number of rx
 * chains per vdev for 2.4 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's rx num chains for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's rx num chains for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_RX_CHAINS_2G CFG_INI_UINT( \
				"num_rx_chains_2g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num rx chains 2g")

/*
 * <ini>
 * num_rx_chains_5g - Config Param to change number of rx
 * chains per vdev for 5 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's rx num chains for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's rx num chains for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_RX_CHAINS_5G CFG_INI_UINT( \
				"num_rx_chains_5g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num rx chains 5g")

/*
 * <ini>
 * tx_nss_2g - Config Param to change tx nss
 * per vdev for 2.4ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of tx spatial streams for eg:-
 * 0x01249249 - change all vdev's tx nss for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's tx nss for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_NSS_2G CFG_INI_UINT( \
				"tx_nss_2g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"tx nss 2.4ghz")

/*
 * <ini>
 * tx_nss_5g - Config Param to change tx nss
 * per vdev for 5ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of tx spatial streams for eg:-
 * 0x01249249 - change all vdev's tx nss for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's tx nss for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_NSS_5G CFG_INI_UINT( \
				"tx_nss_5g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"tx nss 5ghz")

/*
 * <ini>
 * rx_nss_2g - Config Param to change rx nss
 * per vdev for 2.4ghz frequency connections
 *
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of rx spatial streams for eg:-
 * 0x01249249 - change all vdev's rx nss for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's rx nss for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_NSS_2G CFG_INI_UINT( \
				"rx_nss_2g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"rx nss 2.4ghz")

/*
 * <ini>
 * rx_nss_5g - Config Param to change rx nss
 * per vdev for 5ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of rx spatial streams for eg:-
 * 0x01249249 - change all vdev's rx nss for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's rx nss for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_NSS_5G CFG_INI_UINT( \
				"rx_nss_5g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"rx nss 5ghz")

/*
 * <ini>
 * num_tx_chains_11b - Config Param to change number of tx
 * chains per vdev for 2.4ghz 11b mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11b connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11b connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11b CFG_INI_UINT( \
				"num_tx_chains_11b", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num tx chains 11b")

/*
 * <ini>
 * num_tx_chains_11g - Config Param to change number of tx
 * chains per vdev for 2.4ghz 11g mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11g connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11g connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11g CFG_INI_UINT( \
				"num_tx_chains_11g", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num tx chains 11g")

/*
 * <ini>
 * num_tx_chains_11a - Config Param to change number of tx
 * chains per vdev for 5ghz 11a mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11a connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11a connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11a CFG_INI_UINT( \
				"num_tx_chains_11a", \
				0x01249249, \
				0x02492492, \
				0x02492492, \
				CFG_VALUE_OR_DEFAULT, \
				"num tx chains 11a")

/*
 * <ini>
 * disable_tx_mrc_2g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_TX_MRC_2G CFG_INI_BOOL( \
				"disable_tx_mrc_2g", \
				0, \
				"disable diversity gain tx 2g")

/*
 * <ini>
 * disable_rx_mrc_2g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_RX_MRC_2G CFG_INI_BOOL( \
				"disable_rx_mrc_2g", \
				0, \
				"disable diversity gain rx 2g")

/*
 * <ini>
 * disable_tx_mrc_5g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_TX_MRC_5G CFG_INI_BOOL( \
				"disable_tx_mrc_5g", \
				0, \
				"disable diversity gain tx 5g")

/*
 * <ini>
 * disable_rx_mrc_5g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_RX_MRC_5G CFG_INI_BOOL( \
				"disable_rx_mrc_5g", \
				0, \
				"disable diversity gain rx 5g")

/*
 * <ini>
 * enable_dynamic_nss_chain_config - Enable/Disable dynamic nss and chain config
 * to FW.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: STA/SAP/P2P/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DYNAMIC_NSS_CHAIN_CONFIG CFG_INI_BOOL( \
					"enable_dynamic_nss_chain_config", \
					1, \
					"enable dynamic nss chain config")

#define CFG_NSS_CHAINS_ALL \
	CFG(CFG_NUM_TX_CHAINS_2G) \
	CFG(CFG_NUM_TX_CHAINS_5G) \
	CFG(CFG_NUM_RX_CHAINS_2G) \
	CFG(CFG_NUM_RX_CHAINS_5G) \
	CFG(CFG_TX_NSS_5G) \
	CFG(CFG_TX_NSS_2G) \
	CFG(CFG_RX_NSS_5G) \
	CFG(CFG_RX_NSS_2G) \
	CFG(CFG_NUM_TX_CHAINS_11b) \
	CFG(CFG_NUM_TX_CHAINS_11g) \
	CFG(CFG_NUM_TX_CHAINS_11a) \
	CFG(CFG_DISABLE_TX_MRC_2G) \
	CFG(CFG_DISABLE_RX_MRC_2G) \
	CFG(CFG_DISABLE_TX_MRC_5G) \
	CFG(CFG_DISABLE_RX_MRC_5G) \
	CFG(CFG_ENABLE_DYNAMIC_NSS_CHAIN_CONFIG)

#endif /* __CFG_MLME_NSS_CHAINS */

