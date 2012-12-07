/*
 *  linux/drivers/mmc/host/msm_sdcc_raw.c
 *
 *  Driver for Qualcomm MSM 8X55/8960 RAW SDCC Driver
 *
 *  Copyright (C) 2012 Motorola Mobility LLC.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Based on Code Aurora Forum: lk/platform/msm_shared/mmc.c
 *
 */

#include <linux/clk.h>
#include <linux/genhd.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mmc/card.h>
#include <linux/apanic_mmc.h>
#include <linux/init.h>

#include "../../../arch/arm/mach-msm/devices.h"

static void __iomem *raw_mmc_mci_base;

#define RAW_MMC_MCI_REG(offset)          ((raw_mmc_mci_base) + offset)

/*
 * Define Macros for SDCC Registers
 */
#define RAW_MMC_MCI_POWER                RAW_MMC_MCI_REG(0x000)  /* 8 bit */

/* MCICMD output control - 6th bit */
#define RAW_MMC_MCI_OPEN_DRAIN           (1 << 6)
#define RAW_MMC_MCI_PWR_OFF              0x00
#define RAW_MMC_MCI_PWR_UP               0x01
#define RAW_MMC_MCI_PWR_ON               0x01

#define RAW_MMC_MCI_CLK                  RAW_MMC_MCI_REG(0x004)  /* 16 bits */
/* Enable MCI bus clock - 0: clock disabled 1: enabled */
#define RAW_MMC_MCI_CLK_ENABLE           (1 << 8)
/* Disable clk o/p when bus idle- 0:always enabled 1:enabled when bus active */
#define RAW_MMC_MCI_CLK_PWRSAVE          (1 << 9)
/* Enable Widebus mode - 00: 1 bit mode 10:4 bit mode 01/11: 8 bit mode */
#define RAW_MMC_MCI_CLK_WIDEBUS_MODE     (3 << 10)
#define RAW_MMC_MCI_CLK_WIDEBUS_1_BIT    0
#define RAW_MMC_MCI_CLK_WIDEBUS_4_BIT    (2 << 10)
#define RAW_MMC_MCI_CLK_WIDEBUS_8_BIT    (1 << 10)
/* Enable flow control- 0: disable 1: enable */
#define RAW_MMC_MCI_CLK_ENA_FLOW         (1 << 12)
/* Set/clear to select rising/falling edge for data/cmd output */
#define RAW_MMC_MCI_CLK_INVERT_OUT       (1 << 13)
/* Select to lach data/cmd coming in falling/rising/feedbk/loopbk of MCIclk */
#define RAW_MMC_MCI_CLK_IN_FALLING       0x0
#define RAW_MMC_MCI_CLK_IN_RISING        (1 << 14)
#define RAW_MMC_MCI_CLK_IN_FEEDBACK      (2 << 14)
#define RAW_MMC_MCI_CLK_IN_LOOPBACK      (3 << 14)

/* Bus Width */
#define RAW_MMC_BUS_WIDTH_1_BIT          0
#define RAW_MMC_BUS_WIDTH_4_BIT          2
#define RAW_MMC_BUS_WIDTH_8_BIT          3

#define RAW_MMC_MCI_ARGUMENT             RAW_MMC_MCI_REG(0x008)  /* 32 bits */

#define RAW_MMC_MCI_CMD                  RAW_MMC_MCI_REG(0x00C)  /* 16 bits */
/* Command Index: 0 -5 */
/* Waits for response if set */
#define RAW_MMC_MCI_CMD_RESPONSE         (1 << 6)
/* Receives a 136-bit long response if set */
#define RAW_MMC_MCI_CMD_LONGRSP          (1 << 7)
/* If set, CPSM disables command timer and waits for interrupt */
#define RAW_MMC_MCI_CMD_INTERRUPT        (1 << 8)
/* If set waits for CmdPend before starting to send a command */
#define RAW_MMC_MCI_CMD_PENDING          (1 << 9)
/* CPSM is enabled if set */
#define RAW_MMC_MCI_CMD_ENABLE           (1 << 10)
/* If set PROG_DONE status bit asserted when busy is de-asserted */
#define RAW_MMC_MCI_CMD_PROG_ENA         (1 << 11)
/* To indicate that this is a Command with Data (for SDIO interrupts) */
#define RAW_MMC_MCI_CMD_DAT_CMD          (1 << 12)
/* Signals the next command to be an abort (stop) command. Always read 0 */
#define RAW_MMC_MCI_CMD_MCIABORT         (1 << 13)
/* Waits for Command Completion Signal if set */
#define RAW_MMC_MCI_CMD_CCS_ENABLE       (1 << 14)
/* If set sends CCS disable sequence */
#define RAW_MMC_MCI_CMD_CCS_DISABLE      (1 << 15)

#define RAW_MMC_MCI_RESP_CMD             RAW_MMC_MCI_REG(0x010)

#define RAW_MMC_MCI_RESP_0               RAW_MMC_MCI_REG(0x014)
#define RAW_MMC_MCI_RESP_1               RAW_MMC_MCI_REG(0x018)
#define RAW_MMC_MCI_RESP_2               RAW_MMC_MCI_REG(0x01C)
#define RAW_MMC_MCI_RESP_3               RAW_MMC_MCI_REG(0x020)

#define RAW_MMC_MCI_DATA_TIMER           RAW_MMC_MCI_REG(0x024)
#define RAW_MMC_MCI_DATA_LENGTH          RAW_MMC_MCI_REG(0x028)
#define RAW_MMC_MCI_DATA_CTL             RAW_MMC_MCI_REG(0x02C)  /* 16 bits */
/* Data transfer enabled */
#define RAW_MMC_MCI_DATA_ENABLE          (1 << 0)
/* Data transfer direction - 0: controller to card 1:card to controller */
#define RAW_MMC_MCI_DATA_DIR             (1 << 1)
/* Data transfer mode - 0: block data transfer 1: stream data transfer */
#define RAW_MMC_MCI_DATA_MODE            (1 << 2)
/* Enable DM interface - 0: DM disabled 1: DM enabled */
#define RAW_MMC_MCI_DATA_DM_ENABLE       (1 << 3)
/* Data block length in bytes (1-4096) */
#define RAW_MMC_MCI_BLKSIZE_POS          4
#define RAW_MMC_MCI_DATA_COUNT           RAW_MMC_MCI_REG(0x030)
#define RAW_MMC_MCI_STATUS               RAW_MMC_MCI_REG(0x034)
/* Command response received - CRC check failed */
#define RAW_MMC_MCI_STAT_CMD_CRC_FAIL    (1 << 0)
/* Data block sent/received - CRC check failed */
#define RAW_MMC_MCI_STAT_DATA_CRC_FAIL   (1 << 1)
/* Command resonse timeout */
#define RAW_MMC_MCI_STAT_CMD_TIMEOUT     (1 << 2)
/* Data timeout */
#define RAW_MMC_MCI_STAT_DATA_TIMEOUT    (1 << 3)
/* Transmit FIFO underrun error */
#define RAW_MMC_MCI_STAT_TX_UNDRUN       (1 << 4)
/* Receive FIFO overrun error */
#define RAW_MMC_MCI_STAT_RX_OVRRUN       (1 << 5)
/* Command response received - CRC check passed */
#define RAW_MMC_MCI_STAT_CMD_RESP_END    (1 << 6)
/* Command sent - no response required */
#define RAW_MMC_MCI_STAT_CMD_SENT        (1 << 7)
/* Data end - data counter zero */
#define RAW_MMC_MCI_STAT_DATA_END        (1 << 8)
/* Start bit not detected on all data signals in wide bus mode */
#define RAW_MMC_MCI_STAT_START_BIT_ERR   (1 << 9)
/* Data block sent/received - CRC check passed */
#define RAW_MMC_MCI_STAT_DATA_BLK_END    (1 << 10)
/* Command transfer in progress */
#define RAW_MMC_MCI_STAT_CMD_ACTIVE      (1 << 11)
/* Data transmit in progress */
#define RAW_MMC_MCI_STAT_TX_ACTIVE       (1 << 12)
/* Data receive in progress */
#define RAW_MMC_MCI_STAT_RX_ACTIVE       (1 << 13)
/* Transmit FIFO half full */
#define RAW_MMC_MCI_STAT_TX_FIFO_HFULL   (1 << 14)
/* Receive FIFO half full */
#define RAW_MMC_MCI_STAT_RX_FIFO_HFULL   (1 << 15)
/* Transmit FIFO full */
#define RAW_MMC_MCI_STAT_TX_FIFO_FULL    (1 << 16)
/* Receive FIFO full */
#define RAW_MMC_MCI_STAT_RX_FIFO_FULL    (1 << 17)
/* Transmit FIFO empty */
#define RAW_MMC_MCI_STAT_TX_FIFO_EMPTY   (1 << 18)
/* Receive FIFO empty */
#define RAW_MMC_MCI_STAT_RX_FIFO_EMPTY   (1 << 19)
/* Data available in transmit FIFO */
#define RAW_MMC_MCI_STAT_TX_DATA_AVLBL   (1 << 20)
/* Data available in receive FIFO */
#define RAW_MMC_MCI_STAT_RX_DATA_AVLBL   (1 << 21)
/* SDIO interrupt indicator for wake-up */
#define RAW_MMC_MCI_STAT_SDIO_INTR       (1 << 22)
/* Programming done */
#define RAW_MMC_MCI_STAT_PROG_DONE       (1 << 23)
/* CE-ATA command completion signal detected */
#define RAW_MMC_MCI_STAT_ATA_CMD_CMPL    (1 << 24)
/* SDIO interrupt indicator for normal operation */
#define RAW_MMC_MCI_STAT_SDIO_INTR_OP    (1 << 25)
/* Commpand completion signal timeout */
#define RAW_MMC_MCI_STAT_CCS_TIMEOUT     (1 << 26)

#define RAW_MMC_MCI_STATIC_STATUS        (RAW_MMC_MCI_STAT_CMD_CRC_FAIL| \
		RAW_MMC_MCI_STAT_DATA_CRC_FAIL| \
		RAW_MMC_MCI_STAT_CMD_TIMEOUT| \
		RAW_MMC_MCI_STAT_DATA_TIMEOUT| \
		RAW_MMC_MCI_STAT_TX_UNDRUN| \
		RAW_MMC_MCI_STAT_RX_OVRRUN| \
		RAW_MMC_MCI_STAT_CMD_RESP_END| \
		RAW_MMC_MCI_STAT_CMD_SENT| \
		RAW_MMC_MCI_STAT_DATA_END| \
		RAW_MMC_MCI_STAT_START_BIT_ERR| \
		RAW_MMC_MCI_STAT_DATA_BLK_END| \
		RAW_MMC_MCI_SDIO_INTR_CLR| \
		RAW_MMC_MCI_STAT_PROG_DONE| \
		RAW_MMC_MCI_STAT_ATA_CMD_CMPL |\
		RAW_MMC_MCI_STAT_CCS_TIMEOUT)

#define RAW_MMC_MCI_CLEAR                RAW_MMC_MCI_REG(0x038)
#define RAW_MMC_MCI_CMD_CRC_FAIL_CLR     (1 << 0)
#define RAW_MMC_MCI_DATA_CRC_FAIL_CLR    (1 << 1)
#define RAW_MMC_MCI_CMD_TIMEOUT_CLR      (1 << 2)
#define RAW_MMC_MCI_DATA_TIMEOUT_CLR     (1 << 3)
#define RAW_MMC_MCI_TX_UNDERRUN_CLR      (1 << 4)
#define RAW_MMC_MCI_RX_OVERRUN_CLR       (1 << 5)
#define RAW_MMC_MCI_CMD_RESP_END_CLR     (1 << 6)
#define RAW_MMC_MCI_CMD_SENT_CLR         (1 << 7)
#define RAW_MMC_MCI_DATA_END_CLR         (1 << 8)
#define RAW_MMC_MCI_START_BIT_ERR_CLR    (1 << 9)
#define RAW_MMC_MCI_DATA_BLK_END_CLR     (1 << 10)
#define RAW_MMC_MCI_SDIO_INTR_CLR        (1 << 22)
#define RAW_MMC_MCI_PROG_DONE_CLR        (1 << 23)
#define RAW_MMC_MCI_ATA_CMD_COMPLR_CLR   (1 << 24)
#define RAW_MMC_MCI_CCS_TIMEOUT_CLR      (1 << 26)
#define RAW_MMC_MCI_INT_MASK0            RAW_MMC_MCI_REG(0x03C)
#define RAW_MMC_MCI_CMD_CRC_FAIL_MASK    (1 << 0)
#define RAW_MMC_MCI_DATA_CRC_FAIL_MASK   (1 << 1)
#define RAW_MMC_MCI_CMD_TIMEOUT_MASK     (1 << 2)
#define RAW_MMC_MCI_DATA_TIMEOUT_MASK    (1 << 3)
#define RAW_MMC_MCI_TX_OVERRUN_MASK      (1 << 4)
#define RAW_MMC_MCI_RX_OVERRUN_MASK      (1 << 5)
#define RAW_MMC_MCI_CMD_RESP_END_MASK    (1 << 6)
#define RAW_MMC_MCI_CMD_SENT_MASK        (1 << 7)
#define RAW_MMC_MCI_DATA_END_MASK        (1 << 8)
#define RAW_MMC_MCI_START_BIT_ERR_MASK   (1 << 9)
#define RAW_MMC_MCI_DATA_BLK_END_MASK    (1 << 10)
#define RAW_MMC_MCI_CMD_ACTIVE_MASK      (1 << 11)
#define RAW_MMC_MCI_TX_ACTIVE_MASK       (1 << 12)
#define RAW_MMC_MCI_RX_ACTIVE_MASK       (1 << 13)
#define RAW_MMC_MCI_TX_FIFO_HFULL_MASK   (1 << 14)
#define RAW_MMC_MCI_RX_FIFO_HFULL_MASK   (1 << 15)
#define RAW_MMC_MCI_TX_FIFO_FULL_MASK    (1 << 16)
#define RAW_MMC_MCI_RX_FIFO_FULL_MASK    (1 << 17)
#define RAW_MMC_MCI_TX_FIFO_EMPTY_MASK   (1 << 18)
#define RAW_MMC_MCI_RX_FIFO_EMPTY_MASK   (1 << 19)
#define RAW_MMC_MCI_TX_DATA_AVLBL_MASK   (1 << 20)
#define RAW_MMC_MCI_RX_DATA_AVLBL_MASK   (1 << 21)
#define RAW_MMC_MCI_SDIO_INT_MASK        (1 << 22)
#define RAW_MMC_MCI_PROG_DONE_MASK       (1 << 23)
#define RAW_MMC_MCI_ATA_CMD_COMPL_MASK   (1 << 24)
#define RAW_MMC_MCI_SDIO_INT_OPER_MASK   (1 << 25)
#define RAW_MMC_MCI_CCS_TIME_OUT_MASK    (1 << 26)

#define RAW_MMC_MCI_INT_MASK1            RAW_MMC_MCI_REG(0x040)

#define RAW_MMC_MCI_FIFO_COUNT           RAW_MMC_MCI_REG(0x044)

#define RAW_MMC_MCI_CCS_TIMER            RAW_MMC_MCI_REG(0x0058)

#define RAW_MMC_MCI_FIFO                 RAW_MMC_MCI_REG(0x080)

/* Card status */
#define RAW_MMC_CARD_STATUS(x)          ((x>>9) & 0x0F)
#define RAW_MMC_TRAN_STATE              4
#define RAW_MMC_PROG_STATE              7

/* SD Memory Card bus commands */
#define CMD0_GO_IDLE_STATE               0
#define CMD1_SEND_OP_COND                1
#define CMD2_ALL_SEND_CID                2
#define CMD3_SEND_RELATIVE_ADDR          3
#define CMD4_SET_DSR                     4
#define CMD6_SWITCH_FUNC                 6
#define ACMD6_SET_BUS_WIDTH              6    /* SD card */
#define CMD7_SELECT_DESELECT_CARD        7
#define CMD8_SEND_EXT_CSD                8
#define CMD8_SEND_IF_COND                8    /* SD card */
#define CMD9_SEND_CSD                    9
#define CMD10_SEND_CID                   10
#define CMD12_STOP_TRANSMISSION          12
#define CMD13_SEND_STATUS                13
#define CMD15_GO_INACTIVE_STATUS         15
#define CMD16_SET_BLOCKLEN               16
#define CMD17_READ_SINGLE_BLOCK          17
#define CMD18_READ_MULTIPLE_BLOCK        18
#define CMD24_WRITE_SINGLE_BLOCK         24
#define CMD25_WRITE_MULTIPLE_BLOCK       25
#define CMD28_SET_WRITE_PROTECT          28
#define CMD29_CLEAR_WRITE_PROTECT        29
#define CMD31_SEND_WRITE_PROT_TYPE       31
#define CMD32_ERASE_WR_BLK_START         32
#define CMD33_ERASE_WR_BLK_END           33
#define CMD38_ERASE                      38
#define ACMD41_SEND_OP_COND              41    /* SD card */
#define ACMD51_SEND_SCR                  51    /* SD card */
#define CMD55_APP_CMD                    55    /* SD card */

/* Switch Function Modes */
#define RAW_MMC_SWITCH_FUNC_CHECK        0
#define RAW_MMC_SWITCH_FUNC_SET          1

/* OCR Register */
#define RAW_MMC_OCR_17_19                (1 << 7)
#define RAW_MMC_OCR_27_36                (0x1FF << 15)
#define RAW_MMC_OCR_SEC_MODE             (2 << 29)
#define RAW_MMC_OCR_BUSY                 (1 << 31)

/* Commands type */
#define RAW_MMC_CMD_BCAST                (1 << 0)
#define RAW_MMC_CMD_BCAST_W_RESP         (1 << 1)
#define RAW_MMC_CMD_ADDRESS              (1 << 2)
#define RAW_MMC_CMD_ADDR_DATA_XFER       (1 << 3)

/* Response types */
#define RAW_MMC_RESP_NONE                0
#define RAW_MMC_RESP_R1                  (1 << 0)
#define RAW_MMC_RESP_R1B                 (1 << 1)
#define RAW_MMC_RESP_R2                  (1 << 2)
#define RAW_MMC_RESP_R3                  (1 << 3)
#define RAW_MMC_RESP_R6                  (1 << 6)
#define RAW_MMC_RESP_R7                  (1 << 7)

#define IS_RESP_136_BITS(x)              (x & RAW_MMC_RESP_R2)
#define CHECK_FOR_BUSY_AT_RESP(x)

/* Card Status bits (R1 register) */
#define RAW_MMC_R1_AKE_SEQ_ERROR         (1 << 3)
#define RAW_MMC_R1_APP_CMD               (1 << 5)
#define RAW_MMC_R1_RDY_FOR_DATA          (1 << 6)
#define RAW_MMC_R1_CURR_STATE_IDLE       (0 << 9)
#define RAW_MMC_R1_CURR_STATE_RDY        (1 << 9)
#define RAW_MMC_R1_CURR_STATE_IDENT      (2 << 9)
#define RAW_MMC_R1_CURR_STATE_STBY       (3 << 9)
#define RAW_MMC_R1_CURR_STATE_TRAN       (4 << 9)
#define RAW_MMC_R1_CURR_STATE_DATA       (5 << 9)
#define RAW_MMC_R1_CURR_STATE_RCV        (6 << 9)
#define RAW_MMC_R1_CURR_STATE_PRG        (7 << 9)
#define RAW_MMC_R1_CURR_STATE_DIS        (8 << 9)
#define RAW_MMC_R1_ERASE_RESET           (1 << 13)
#define RAW_MMC_R1_CARD_ECC_DISABLED     (1 << 14)
#define RAW_MMC_R1_WP_ERASE_SKIP         (1 << 15)
#define RAW_MMC_R1_ERROR                 (1 << 19)
#define RAW_MMC_R1_CC_ERROR              (1 << 20)
#define RAW_MMC_R1_CARD_ECC_FAILED       (1 << 21)
#define RAW_MMC_R1_ILLEGAL_CMD           (1 << 22)
#define RAW_MMC_R1_COM_CRC_ERR           (1 << 23)
#define RAW_MMC_R1_LOCK_UNLOCK_FAIL      (1 << 24)
#define RAW_MMC_R1_CARD_IS_LOCKED        (1 << 25)
#define RAW_MMC_R1_WP_VIOLATION          (1 << 26)
#define RAW_MMC_R1_ERASE_PARAM           (1 << 27)
#define RAW_MMC_R1_ERASE_SEQ_ERR         (1 << 28)
#define RAW_MMC_R1_BLOCK_LEN_ERR         (1 << 29)
#define RAW_MMC_R1_ADDR_ERR              (1 << 30)
#define RAW_MMC_R1_OUT_OF_RANGE          (1 << 31)

/* Macros for Common Errors */
#define RAW_MMC_E_SUCCESS                0
#define RAW_MMC_E_FAILURE                1
#define RAW_MMC_E_TIMEOUT                2
#define RAW_MMC_E_INVAL                  3
#define RAW_MMC_E_CRC_FAIL               4
#define RAW_MMC_E_INIT_FAIL              5
#define RAW_MMC_E_CMD_INDX_MISMATCH      6
#define RAW_MMC_E_RESP_VERIFY_FAIL       7
#define RAW_MMC_E_NOT_SUPPORTED          8
#define RAW_MMC_E_CARD_BUSY              9
#define RAW_MMC_E_MEM_ALLOC_FAIL         10
#define RAW_MMC_E_CLK_ENABLE_FAIL        11
#define RAW_MMC_E_CMMC_DECODE_FAIL       12
#define RAW_MMC_E_CID_DECODE_FAIL        13
#define RAW_MMC_E_BLOCKLEN_ERR           14
#define RAW_MMC_E_ADDRESS_ERR            15
#define RAW_MMC_E_DATA_CRC_FAIL          16
#define RAW_MMC_E_DATA_TIMEOUT           17
#define RAW_MMC_E_RX_OVRRUN              18
#define RAW_MMC_E_VREG_SET_FAILED        19
#define RAW_MMC_E_GPIO_CFG_FAIL          20

/* EXT_CSD */
#define RAW_MMC_ACCESS_WRITE             0x3
#define RAW_MMC_EXT_CMMC_HS_TIMING       185
#define RAW_MMC_EXT_CMMC_BUS_WIDTH       183

#define RAW_MMC_EXT_USER_WP              171
#define RAW_MMC_EXT_ERASE_GROUP_DEF      175
#define RAW_MMC_EXT_HC_WP_GRP_SIZE       221
#define RAW_MMC_EXT_HC_ERASE_GRP_SIZE    224

#define IS_BIT_SET_EXT_CSD(val, bit)      ((ext_csd_buf[val]) & (1<<(bit)))
#define IS_ADDR_OUT_OF_RANGE(resp)        ((resp >> 31) & 0x01)

#define RAW_MMC_US_PERM_WP_EN            2
#define RAW_MMC_US_PWR_WP_DIS            3

#define RAW_MMC_US_PERM_WP_DIS           (1<<4)
#define RAW_MMC_US_PWR_WP_EN             1

/* For SD */
#define RAW_MMC_SD_HC_VOLT_SUPPLIED      0x000001AA
#define RAW_MMC_SD_NEG_OCR               0x00FF8000
#define RAW_MMC_SD_HC_HCS                0x40000000
#define RAW_MMC_SD_DEV_READY             0x80000000
#define RAW_MMC_SD_SWITCH_HS             0x80FFFF01

#define MMC_RCA 2

/* Data structure definitions */
struct raw_mmc_command {
	unsigned int cmd_index;
	unsigned int argument;
	unsigned int cmd_type;

	unsigned int resp[4];
	unsigned int resp_type;
	unsigned int prg_enabled;
	unsigned int xfer_mode;
};

#define RAW_MMC_XFER_MODE_BLOCK          0
#define RAW_MMC_XFER_MODE_STREAM         1

/* CSD Register.
 * Note: not all the fields have been defined here
 */
struct raw_mmc_csd {
	unsigned int cmmc_structure;
	unsigned int card_cmd_class;
	unsigned int write_blk_len;
	unsigned int read_blk_len;
	unsigned int r2w_factor;
	unsigned int sector_size;
	unsigned int c_size_mult;
	unsigned int c_size;
	unsigned int nsac_clk_cycle;
	unsigned int taac_ns;
	unsigned int tran_speed;
	unsigned int erase_grp_size;
	unsigned int erase_grp_mult;
	unsigned int wp_grp_size;
	unsigned int wp_grp_enable:1;
	unsigned int perm_wp:1;
	unsigned int temp_wp:1;
	unsigned int erase_blk_len:1;
	unsigned int read_blk_misalign:1;
	unsigned int write_blk_misalign:1;
	unsigned int read_blk_partial:1;
	unsigned int write_blk_partial:1;
};

/* CID Register */
struct raw_mmc_cid {
	unsigned int mid;    /* 8 bit manufacturer id*/
	unsigned int oid;    /* 16 bits 2 character ASCII - OEM ID*/
	unsigned char pnm[7];/* 6 character ASCII -  product name*/
	unsigned int prv;    /* 8 bits - product revision */
	unsigned int psn;    /* 32 bits - product serial number */
	unsigned int month;  /* 4 bits manufacturing month */
	unsigned int year;   /* 4 bits manufacturing year */
};

/* SCR Register */
struct raw_mmc_scr {
	unsigned int scr_structure;
	unsigned int mmc_spec;
#define RAW_MMC_SCR_MMC_SPEC_V1_01       0
#define RAW_MMC_SCR_MMC_SPEC_V1_10       1
#define RAW_MMC_SCR_MMC_SPEC_V2_00       2
	unsigned int data_stat_after_erase;
	unsigned int mmc_security;
#define RAW_MMC_SCR_NO_SECURITY         0
#define RAW_MMC_SCR_SECURITY_UNUSED     1
#define RAW_MMC_SCR_SECURITY_V1_01      2
#define RAW_MMC_SCR_SECURITY_V2_00      3
	unsigned int mmc_bus_width;
#define RAW_MMC_SCR_BUS_WIDTH_1_BIT     (1<<0)
#define RAW_MMC_SCR_BUS_WIDTH_4_BIT     (1<<2)
};

struct raw_mmc_card {
	unsigned int rca;
	unsigned int ocr;
	unsigned int capacity;
	unsigned int type;
#define RAW_MMC_TYPE_STD_SD             0
#define RAW_MMC_TYPE_SDHC               1
#define RAW_MMC_TYPE_SDIO               2
#define RAW_MMC_TYPE_MMCHC              3
#define RAW_MMC_TYPE_STD_MMC            4
	unsigned int status;
#define RAW_MMC_STATUS_INACTIVE         0
#define RAW_MMC_STATUS_ACTIVE           1
	unsigned int rd_timeout_ns;
	unsigned int wr_timeout_ns;
	unsigned int rd_block_len;
	unsigned int wr_block_len;
	struct raw_mmc_cid cid;
	struct raw_mmc_csd csd;
	struct raw_mmc_scr scr;
};

#define RAW_MMC_XFER_MULTI_BLOCK        0
#define RAW_MMC_XFER_SINGLE_BLOCK       1

struct raw_mmc_host {
	unsigned int mclk_rate;
	unsigned int pclk_rate;
	unsigned int ocr;
	unsigned int cmd_retry;
	unsigned int clk_enabled;
};


/* MACRO used to evoke regcomp */
#define REGCOMP_CKRTN(regx, str, errhandle)   \
	do {                            \
		if (regcomp(regx, str, REG_EXTENDED) != 0) {    \
			printf("Error building regex: %s\n", str);  \
			goto errhandle;  \
		} \
	} while (0)


#define GET_LWORD_FROM_BYTE(x)    ((unsigned)*(x) | \
		((unsigned)*(x+1) << 8) | \
		((unsigned)*(x+2) << 16) | \
		((unsigned)*(x+3) << 24))

#define PUT_LWORD_TO_BYTE(x, y)   do {*(x) = y & 0xff;     \
	*(x+1) = (y >> 8) & 0xff;     \
	*(x+2) = (y >> 16) & 0xff;     \
	*(x+3) = (y >> 24) & 0xff; } while (0)

#define GET_PAR_NUM_FROM_POS(x) (((x & 0x0000FF00) >> 8) + (x & 0x000000FF))


/* Can be used to unpack array of upto 32 bits data */
#define UNPACK_BITS(array, start, len, size_of)                               \
	({                                                             \
	 unsigned int indx = (start) / (size_of);                  \
	 unsigned int offset = (start) % (size_of);                \
	 unsigned int mask = (((len) < (size_of)) ? 1<<(len) : 0) - 1; \
	 unsigned int unpck = array[indx] >> offset;               \
	 unsigned int indx2 = ((start) + (len) - 1) / (size_of);       \
	 if (indx2 > indx)                                          \
		unpck |= array[indx2] << ((size_of) - offset);          \
	 unpck & mask;                                             \
	 })

#define RAW_MMC_MAX_COMMAND_RETRY    10
#define RAW_MMC_RD_BLOCK_LEN         512
#define RAW_MMC_WR_BLOCK_LEN         512

/* We have 16 32-bits FIFO registers */
#define MMC_MCI_FIFO_COUNT       16
#define RAW_MMC_MCI_HFIFO_COUNT      (MMC_MCI_FIFO_COUNT / 2)
#define RAW_MMC_MCI_FIFO_SIZE        (MMC_MCI_FIFO_COUNT * 4)

/*Need to put at proper place*/
#define SDC1_CLK    19  /* Secure Digital Card clocks */
#define SDC1_PCLK   20
#define SDC2_CLK    21
#define SDC2_PCLK   22
#define SDC3_CLK    23
#define SDC3_PCLK   24
#define SDC4_CLK    25
#define SDC4_PCLK   26

#define MMC_CLK_400KHZ                400000
#define MMC_CLK_144KHZ                144000
#define MMC_CLK_20MHZ                 20000000
#define MMC_CLK_25MHZ                 25000000
#define MMC_CLK_48MHZ                 48000000
#define MMC_CLK_50MHZ                 49152000

#define MMC_CLK_ENABLE      1
#define MMC_CLK_DISABLE     0

/* TODO: handle multiple versions of MSM */
#define MSM_SDC1_BASE	0x12400000
#define MSM_SDC2_BASE	0x12140000
#define MSM_SDC3_BASE	0x12180000
#define MSM_SDC4_BASE	0x121C0000
#define MSM_SDC5_BASE	0x12200000

/* data access time unit in ns */
static const unsigned int taac_unit[] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};
/* data access time value x 10 */
static const unsigned int taac_value[] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80
};

/* data transfer rate in kbit/s */
static const unsigned int xfer_rate_unit[] = {
	100, 1000, 10000, 100000, 0, 0, 0, 0
};
/* data transfer rate value x 10*/
static const unsigned int xfer_rate_value[] = {
	0, 10, 12, 13, 15, 20, 26, 30, 35, 40, 45, 52, 55, 60, 70, 80
};

static unsigned mmc_sdc_clk[] = { SDC1_CLK, SDC2_CLK, SDC3_CLK, SDC4_CLK };
static unsigned mmc_sdc_pclk[] = { SDC1_PCLK, SDC2_PCLK, SDC3_PCLK, SDC4_PCLK };

static unsigned char mmc_slot;

static unsigned char ext_csd_buf[512];

static int mmc_clock_enable_disable(unsigned id, unsigned enable)
{
	struct clk *sdc_clk = NULL;

	if (id == SDC1_CLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "core_clk");

		if (sdc_clk) {
			if (enable)
				clk_prepare_enable(sdc_clk);
			else
				clk_disable_unprepare(sdc_clk);
		}
		clk_put(sdc_clk);
	} else if (id == SDC1_PCLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "iface_clk");

		if (sdc_clk) {
			if (enable)
				clk_prepare_enable(sdc_clk);
			else
				clk_disable_unprepare(sdc_clk);
		}
		clk_put(sdc_clk);
	}

	else
		pr_err("msm_sdcc_raw: error clock id\n");
	return 0;
}

static int mmc_clock_get_rate(unsigned id)
{
	struct clk *sdc_clk = NULL;
	int clk_rate = -1;

	if (id == SDC1_CLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "core_clk");

		if (sdc_clk)
			clk_rate = clk_get_rate(sdc_clk);

		clk_put(sdc_clk);
	} else if (id == SDC1_PCLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "iface_clk");

		if (sdc_clk)
			clk_rate = clk_get_rate(sdc_clk);

		clk_put(sdc_clk);
	}

	else
		pr_err("msm_sdcc_raw: error clock id\n");

	return clk_rate;
}

static int mmc_clock_set_rate(unsigned id, unsigned rate)
{
	struct clk *sdc_clk = NULL;

	if (id == SDC1_CLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "core_clk");

		if (sdc_clk)
			clk_set_rate(sdc_clk, rate);

		clk_put(sdc_clk);
	} else if (id == SDC1_PCLK) {
		sdc_clk = clk_get(&(msm_device_sdc1.dev), "iface_clk");

		if (sdc_clk)
			clk_set_rate(sdc_clk, rate);

		clk_put(sdc_clk);
	}

	else
		pr_err("msm_sdcc_raw: error clock id\n");

	return 0;
}

static struct raw_mmc_host mmc_host;
static struct raw_mmc_card mmc_card;

static void mmc_display_ext_csd(void);
static void mmc_display_csd(void);

/*
 * Function to enable and set master and peripheral clock for
 * MMC card.
 */
static unsigned int raw_mmc_enable_clock(struct raw_mmc_host *host,
		unsigned int mclk)
{
	unsigned int mmc_clk = 0;
	int mmc_signed_ret = 0;
	unsigned SDC_CLK = mmc_sdc_clk[mmc_slot - 1];
	unsigned SDC_PCLK = mmc_sdc_pclk[mmc_slot - 1];

	if (host == NULL)
		return RAW_MMC_E_INVAL;

	if (!host->clk_enabled) {
		/* set clock */
		if (mmc_clock_enable_disable(SDC_PCLK, MMC_CLK_ENABLE) < 0) {
			pr_crit("msm_sdcc_raw: Failure enabling PCLK!\n");
			goto error_pclk;
		}

		if (mmc_clock_enable_disable(SDC_CLK, MMC_CLK_ENABLE) < 0) {
			pr_crit("msm_sdcc_raw: Failure enabling MMC Clock!\n");
			goto error;
		}
		host->clk_enabled = 1;
	}
	if (host->mclk_rate != mclk) {
		if (mmc_clock_set_rate(SDC_CLK, mclk) < 0) {
			pr_crit("msm_sdcc_raw: Failure setting clock rate for "
					"MCLK - clk_rate: %d\n!", mclk);
			goto error_mclk;
		}

		mmc_signed_ret = mmc_clock_get_rate(SDC_CLK);
		if (mmc_signed_ret < 0) {
			pr_crit("msm_sdcc_raw: Failure getting clock rate for "
					"MCLK - clk_rate: %d\n!",
					host->mclk_rate);
			goto error_mclk;
		}

		host->mclk_rate = (unsigned int) mmc_signed_ret;
	}

	mmc_signed_ret = mmc_clock_get_rate(SDC_PCLK);
	if (mmc_signed_ret < 0) {
		pr_crit("msm_sdcc_raw: Failure getting clock rate for PCLK - "
				"clk_rate: %d\n!", host->pclk_rate);
		goto error_pclk;
	}

	host->pclk_rate = (unsigned int) mmc_signed_ret;
	pr_debug("Clock rate - mclk: %dHz    pclk: %dHz\n",
			host->mclk_rate, host->pclk_rate);

	/* enable mci clock */
	mmc_clk |= RAW_MMC_MCI_CLK_ENABLE;
	/* enable flow control */
	mmc_clk |= RAW_MMC_MCI_CLK_ENA_FLOW;
	/* latch data and command using feedback clock */
	mmc_clk |= RAW_MMC_MCI_CLK_IN_FEEDBACK;
	writel(mmc_clk, RAW_MMC_MCI_CLK);
	return RAW_MMC_E_SUCCESS;

error_pclk:
	mmc_clock_enable_disable(SDC_PCLK, MMC_CLK_DISABLE);
error_mclk:
	mmc_clock_enable_disable(SDC_CLK, MMC_CLK_DISABLE);
error:
	return RAW_MMC_E_CLK_ENABLE_FAIL;
}


/* Sets a timeout for read operation.
*/
static unsigned int raw_mmc_set_read_timeout(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	unsigned int timeout_ns = 0;

	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	if ((card->type == RAW_MMC_TYPE_MMCHC)
			|| (card->type == RAW_MMC_TYPE_SDHC))
		card->rd_timeout_ns = 100000000;
	else if ((card->type == RAW_MMC_TYPE_STD_SD)
			|| (card->type == RAW_MMC_TYPE_STD_MMC)) {
		timeout_ns =
			10 * ((card->csd.taac_ns) +
					(card->csd.nsac_clk_cycle /
					 (host->mclk_rate / 1000000000)));
		card->rd_timeout_ns = timeout_ns;
	} else
		return RAW_MMC_E_NOT_SUPPORTED;

	pr_debug("Read timeout set: %d ns\n", card->rd_timeout_ns);

	return RAW_MMC_E_SUCCESS;
}

/*
 * Sets a timeout for write operation.
 */
static unsigned int raw_mmc_set_write_timeout(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	unsigned int timeout_ns = 0;

	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	if ((card->type == RAW_MMC_TYPE_MMCHC)
			|| (card->type == RAW_MMC_TYPE_SDHC))
		card->wr_timeout_ns = 100000000;
	else if (card->type == RAW_MMC_TYPE_STD_SD
			|| (card->type == RAW_MMC_TYPE_STD_MMC)) {
		timeout_ns =
			10 * ((card->csd.taac_ns) +
					(card->csd.nsac_clk_cycle /
					 (host->mclk_rate / 1000000000)));
		timeout_ns = timeout_ns << card->csd.r2w_factor;
		card->wr_timeout_ns = timeout_ns;
	} else
		return RAW_MMC_E_NOT_SUPPORTED;

	pr_debug("Write timeout set: %d ns\n", card->wr_timeout_ns);

	return RAW_MMC_E_SUCCESS;
}


/*
 * Decodes CSD response received from the card. Note that we have defined only
 * few of the CSD elements in csd structure. We'll only decode those values.
 */
static unsigned int raw_mmc_decode_and_save_csd(struct raw_mmc_card *card,
		unsigned int *raw_csd)
{
	unsigned int mmc_sizeof = 0;
	unsigned int mmc_unit = 0;
	unsigned int mmc_value = 0;
	unsigned int mmc_temp = 0;

	struct raw_mmc_csd mmc_csd;

	if ((card == NULL) || (raw_csd == NULL))
		return RAW_MMC_E_INVAL;

	/*
	 * CSD register is little bit differnet for CSD version 2.0 High
	 * Capacity and CSD version 1.0/2.0 Standard memory cards. In Version
	 * 2.0 some of * the fields have fixed values and it's not necessary
	 * for host to refer these fields in CSD sent by card
	 */

	mmc_sizeof = sizeof(unsigned int) * 8;
	memset(&mmc_csd, 0x0, sizeof(mmc_csd));
	mmc_csd.cmmc_structure = UNPACK_BITS(raw_csd, 126, 2, mmc_sizeof);

	/* cmmc_structure- 0: Version 1.0     1: Version 2.0 */
	if (mmc_csd.cmmc_structure) {
		mmc_csd.card_cmd_class =
			UNPACK_BITS(raw_csd, 84, 12, mmc_sizeof);
		mmc_csd.write_blk_len = 512; /* Fixed value is 9 = 2^9 = 512 */
		mmc_csd.read_blk_len = 512;  /* Fixed value is 9 = 512 */
		/* Fixed value: 010b */
		mmc_csd.r2w_factor = UNPACK_BITS(raw_csd, 26, 3, mmc_sizeof);
		mmc_csd.c_size_mult = 0;     /* not there in version 2.0 */
		mmc_csd.c_size = UNPACK_BITS(raw_csd, 62, 12, mmc_sizeof);
		mmc_csd.nsac_clk_cycle =
			UNPACK_BITS(raw_csd, 104, 8, mmc_sizeof) * 100;

		mmc_unit = UNPACK_BITS(raw_csd, 112, 3, mmc_sizeof);
		mmc_value = UNPACK_BITS(raw_csd, 115, 4, mmc_sizeof);
		mmc_csd.taac_ns =
			(taac_value[mmc_value] * taac_unit[mmc_unit]) / 10;

		mmc_csd.erase_blk_len = 1;
		mmc_csd.read_blk_misalign = 0;
		mmc_csd.write_blk_misalign = 0;
		mmc_csd.read_blk_partial = 0;
		mmc_csd.write_blk_partial = 0;

		mmc_unit = UNPACK_BITS(raw_csd, 96, 3, mmc_sizeof);
		mmc_value = UNPACK_BITS(raw_csd, 99, 4, mmc_sizeof);
		mmc_csd.tran_speed =
			(xfer_rate_value[mmc_value] *
			 xfer_rate_unit[mmc_unit]) / 10;

		/* Calculate card capcity now itself */
		card->capacity = (1 + mmc_csd.c_size) * 512000;
	} else {
		mmc_csd.card_cmd_class =
			UNPACK_BITS(raw_csd, 84, 12, mmc_sizeof);

		mmc_temp = UNPACK_BITS(raw_csd, 22, 4, mmc_sizeof);
		mmc_csd.write_blk_len = (mmc_temp > 8
				&& mmc_temp <
				12) ? (1 << mmc_temp) : 512;

		mmc_temp = UNPACK_BITS(raw_csd, 80, 4, mmc_sizeof);
		mmc_csd.read_blk_len = (mmc_temp > 8
				&& mmc_temp <
				12) ? (1 << mmc_temp) : 512;

		mmc_unit = UNPACK_BITS(raw_csd, 112, 3, mmc_sizeof);
		mmc_value = UNPACK_BITS(raw_csd, 115, 4, mmc_sizeof);
		mmc_csd.taac_ns =
			(taac_value[mmc_value] * taac_unit[mmc_unit]) / 10;

		mmc_unit = UNPACK_BITS(raw_csd, 96, 3, mmc_sizeof);
		mmc_value = UNPACK_BITS(raw_csd, 99, 4, mmc_sizeof);
		mmc_csd.tran_speed =
			(xfer_rate_value[mmc_value] *
			 xfer_rate_unit[mmc_unit]) / 10;

		mmc_csd.nsac_clk_cycle =
			UNPACK_BITS(raw_csd, 104, 8, mmc_sizeof) * 100;

		mmc_csd.r2w_factor =
			UNPACK_BITS(raw_csd, 26, 3, mmc_sizeof);
		mmc_csd.sector_size =
			UNPACK_BITS(raw_csd, 39, 7, mmc_sizeof) + 1;

		mmc_csd.erase_blk_len =
			UNPACK_BITS(raw_csd, 46, 1, mmc_sizeof);
		mmc_csd.read_blk_misalign =
			UNPACK_BITS(raw_csd, 77, 1, mmc_sizeof);
		mmc_csd.write_blk_misalign =
			UNPACK_BITS(raw_csd, 78, 1, mmc_sizeof);
		mmc_csd.read_blk_partial =
			UNPACK_BITS(raw_csd, 79, 1, mmc_sizeof);
		mmc_csd.write_blk_partial =
			UNPACK_BITS(raw_csd, 21, 1, mmc_sizeof);

		mmc_csd.c_size_mult =
			UNPACK_BITS(raw_csd, 47, 3, mmc_sizeof);
		mmc_csd.c_size = UNPACK_BITS(raw_csd, 62, 12, mmc_sizeof);
		mmc_temp =
			(1 << (mmc_csd.c_size_mult + 2)) * (mmc_csd.c_size +
					1);
		card->capacity = mmc_temp * mmc_csd.read_blk_len;
	}

	mmc_csd.erase_grp_size = UNPACK_BITS(raw_csd, 42, 5, mmc_sizeof);
	mmc_csd.erase_grp_mult = UNPACK_BITS(raw_csd, 37, 5, mmc_sizeof);
	mmc_csd.wp_grp_size = UNPACK_BITS(raw_csd, 32, 5, mmc_sizeof);
	mmc_csd.wp_grp_enable = UNPACK_BITS(raw_csd, 31, 1, mmc_sizeof);
	mmc_csd.perm_wp = UNPACK_BITS(raw_csd, 13, 1, mmc_sizeof);
	mmc_csd.temp_wp = UNPACK_BITS(raw_csd, 12, 1, mmc_sizeof);

	/* save the information in card structure */
	memcpy((struct raw_mmc_csd *) &card->csd,
			(struct raw_mmc_csd *) &mmc_csd,
			sizeof(struct raw_mmc_csd));

	pr_debug("Decoded CSD fields:\n");
	pr_debug("cmmc_structure: %d\n", mmc_csd.cmmc_structure);
	pr_debug("card_cmd_class: %x\n", mmc_csd.card_cmd_class);
	pr_debug("write_blk_len: %d\n", mmc_csd.write_blk_len);
	pr_debug("read_blk_len: %d\n", mmc_csd.read_blk_len);
	pr_debug("r2w_factor: %d\n", mmc_csd.r2w_factor);
	pr_debug("sector_size: %d\n", mmc_csd.sector_size);
	pr_debug("c_size_mult:%d\n", mmc_csd.c_size_mult);
	pr_debug("c_size: %d\n", mmc_csd.c_size);
	pr_debug("nsac_clk_cycle: %d\n", mmc_csd.nsac_clk_cycle);
	pr_debug("taac_ns: %d\n", mmc_csd.taac_ns);
	pr_debug("tran_speed: %d kbps\n", mmc_csd.tran_speed);
	pr_debug("erase_blk_len: %d\n", mmc_csd.erase_blk_len);
	pr_debug("read_blk_misalign: %d\n", mmc_csd.read_blk_misalign);
	pr_debug("write_blk_misalign: %d\n", mmc_csd.write_blk_misalign);
	pr_debug("read_blk_partial: %d\n", mmc_csd.read_blk_partial);
	pr_debug("write_blk_partial: %d\n", mmc_csd.write_blk_partial);
	pr_debug("Card Capacity: %d Bytes\n", card->capacity);

	return RAW_MMC_E_SUCCESS;

}

/*
 * Decode CID sent by the card.
 */
static unsigned int raw_mmc_decode_and_save_cid(struct raw_mmc_card *card,
		unsigned int *raw_cid)
{
	struct raw_mmc_cid mmc_cid;
	unsigned int mmc_sizeof = 0;
	int i = 0;

	if ((card == NULL) || (raw_cid == NULL))
		return RAW_MMC_E_INVAL;

	memset(&mmc_cid, 0x0, sizeof(mmc_cid));
	mmc_sizeof = sizeof(unsigned int) * 8;
	mmc_cid.mid = UNPACK_BITS(raw_cid, 120, 8, mmc_sizeof);
	mmc_cid.oid = UNPACK_BITS(raw_cid, 104, 16, mmc_sizeof);

	for (i = 0; i < 6; i++) {
		mmc_cid.pnm[i] = (unsigned char) UNPACK_BITS(raw_cid,
				(104 - 8 * (i + 1)), 8, mmc_sizeof);
	}
	mmc_cid.pnm[6] = 0;

	mmc_cid.prv = UNPACK_BITS(raw_cid, 48, 8, mmc_sizeof);
	mmc_cid.month = UNPACK_BITS(raw_cid, 12, 4, mmc_sizeof);
	mmc_cid.year = UNPACK_BITS(raw_cid, 8, 4, mmc_sizeof);

	/* save it in card database */
	memcpy((struct raw_mmc_cid *) &card->cid,
			(struct raw_mmc_cid *) &mmc_cid,
			sizeof(struct raw_mmc_cid));

	pr_debug("Decoded CID fields:\n");
	pr_debug("Manufacturer ID: %x\n", mmc_cid.mid);
	pr_debug("OEM ID: 0x%x\n", mmc_cid.oid);
	pr_debug("Product Name: %s\n", mmc_cid.pnm);
	pr_debug("Product revision: %d.%d\n", (mmc_cid.prv >> 4),
			(mmc_cid.prv & 0xF));
	pr_debug("Manufacturing date: %d %d\n", mmc_cid.month,
			mmc_cid.year + 1997);

	return RAW_MMC_E_SUCCESS;
}

/*
 * Sends specified command to a card and waits for a response.
 */
static unsigned int raw_mmc_send_command(struct raw_mmc_command *cmd)
{
	unsigned int mmc_cmd = 0;
	unsigned int mmc_status = 0;
	unsigned int mmc_resp = 0;
	unsigned int mmc_return = RAW_MMC_E_SUCCESS;
	unsigned int cmd_index = 0;
	int i = 0;

	/* basic check */
	if (cmd == NULL)
		return RAW_MMC_E_INVAL;

	/* 1. Write command argument to RAW_MMC_MCI_ARGUMENT register */
	writel(cmd->argument, RAW_MMC_MCI_ARGUMENT);

	/* 2. Set appropriate fields and write RAW_MMC_MCI_CMD */
	/* 2a. Write command index in CMD_INDEX field */
	cmd_index = cmd->cmd_index;
	mmc_cmd |= cmd->cmd_index;
	/* 2b. Set RESPONSE bit to 1 for all cmds except CMD0 */
	if (cmd_index != CMD0_GO_IDLE_STATE)
		mmc_cmd |= RAW_MMC_MCI_CMD_RESPONSE;

	/* 2c. Set LONGRESP bit to 1 for CMD2, CMD9 and CMD10 */
	if (IS_RESP_136_BITS(cmd->resp_type))
		mmc_cmd |= RAW_MMC_MCI_CMD_LONGRSP;

	/* 2d. Set INTERRUPT bit to 1 to disable command timeout */

	/* 2e. Set PENDING bit to 1 for CMD12 in the beginning of stream
	   mode data transfer */
	if (cmd->xfer_mode == RAW_MMC_XFER_MODE_STREAM)
		mmc_cmd |= RAW_MMC_MCI_CMD_PENDING;

	/* 2f. Set ENABLE bit to 1 */
	mmc_cmd |= RAW_MMC_MCI_CMD_ENABLE;

	/* 2g. Set PROG_ENA bit to 1 for CMD12, CMD13 issued at the end of
	   write data transfer */
	if ((cmd_index == CMD12_STOP_TRANSMISSION ||
				cmd_index == CMD13_SEND_STATUS)
			&& cmd->prg_enabled)
		mmc_cmd |= RAW_MMC_MCI_CMD_PROG_ENA;

	/* 2h. Set MCIABORT bit to 1 for CMD12 when working with SDIO card */
	/* 2i. Set CCS_ENABLE bit to 1 for CMD61 when Command Completion Signal
	   of CE-ATA device is enabled */

	/* 2j. clear all static status bits */
	writel(RAW_MMC_MCI_STATIC_STATUS, RAW_MMC_MCI_CLEAR);

	/* 2k. Write to RAW_MMC_MCI_CMD register */
	writel(mmc_cmd, RAW_MMC_MCI_CMD);

	pr_debug("Command sent: CMD%d MCI_CMD_REG:%x MCI_ARG:%x\n",
			cmd_index, mmc_cmd, cmd->argument);

	/* 3. Wait for interrupt or poll on the following bits of MCI_STATUS
	   register */
	do {
		/* 3a. Read MCI_STATUS register */
		while (readl(RAW_MMC_MCI_STATUS) & RAW_MMC_MCI_STAT_CMD_ACTIVE)
			;

		mmc_status = readl(RAW_MMC_MCI_STATUS);

		/* 3b. CMD_SENT bit supposed to be set to 1 only after CMD0 is sent -
		   no response required. */
		if ((cmd->resp_type == RAW_MMC_RESP_NONE) &&
				(mmc_status & RAW_MMC_MCI_STAT_CMD_SENT))
			break;

		/* 3c. If CMD_TIMEOUT bit is set then no response was received */
		else if (mmc_status & RAW_MMC_MCI_STAT_CMD_TIMEOUT) {
			mmc_return = RAW_MMC_E_TIMEOUT;
			break;
		}

		/* 3d. If CMD_RESPONSE_END bit is set to 1 then command's
		 * response was received and CRC check passed Spcial case for
		 * ACMD41: it seems to always fail CRC even if the response is
		 * valid
		 */
		else if ((mmc_status & RAW_MMC_MCI_STAT_CMD_RESP_END)
				|| (cmd_index == CMD1_SEND_OP_COND)
				|| (cmd_index == CMD8_SEND_IF_COND)) {
			/* 3i. Read MCI_RESP_CMD register to verify that
			 * response index is equal to command index */
			mmc_resp = readl(RAW_MMC_MCI_RESP_CMD) & 0x3F;

			/* However, long response does not contain the command
			 * index field. In that case, response index field
			 * must be set to 111111b (0x3F) */
			if ((mmc_resp == cmd_index) ||
					(cmd->resp_type == RAW_MMC_RESP_R2 ||
					 cmd->resp_type == RAW_MMC_RESP_R3 ||
					 cmd->resp_type == RAW_MMC_RESP_R6 ||
					 cmd->resp_type == RAW_MMC_RESP_R7)) {
				/* 3j. If resp index is equal to cmd index,
				 * read command resp from MCI_RESPn registers
				 * - MCI_RESP0/1/2/3 for CMD2/9/10
				 * - MCI_RESP0 for all other registers */
				if (IS_RESP_136_BITS(cmd->resp_type))
					for (i = 0; i < 4; i++) {
						cmd->resp[3 - i] =
							readl
							(RAW_MMC_MCI_RESP_0 +
							 (i * 4));

					}
				else
					cmd->resp[0] =
						readl(RAW_MMC_MCI_RESP_0);
			} else
				/* command index mis-match */
				mmc_return = RAW_MMC_E_CMD_INDX_MISMATCH;

			pr_debug("Command response received: %X\n",
					cmd->resp[0]);
			break;
		}

		/* 3e. If CMD_CRC_FAIL bit is set to 1 then cmd's response was
		 * recvd, but CRC check failed. */
		else if ((mmc_status & RAW_MMC_MCI_STAT_CMD_CRC_FAIL)) {
			if (cmd_index == ACMD41_SEND_OP_COND)
				cmd->resp[0] = readl(RAW_MMC_MCI_RESP_0);
			else
				mmc_return = RAW_MMC_E_CRC_FAIL;
			break;
		}

	} while (1);

	return mmc_return;
}

/*
 * Reset all the cards to idle condition (CMD 0)
 */
static unsigned int raw_mmc_reset_cards(void)
{
	struct raw_mmc_command cmd;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	cmd.cmd_index = CMD0_GO_IDLE_STATE;
	cmd.argument = 0;	/* stuff bits - ignored */
	cmd.cmd_type = RAW_MMC_CMD_BCAST;
	cmd.resp_type = RAW_MMC_RESP_NONE;

	/* send command */
	return raw_mmc_send_command(&cmd);
}

/*
 * Send CMD1 to know whether the card supports host VDD profile or not.
 */
static unsigned int raw_mmc_send_op_cond(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_resp = 0;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD1 format:
	 * [31] Busy bit
	 * [30:29] Access mode
	 * [28:24] reserved
	 * [23:15] 2.7-3.6
	 * [14:8]  2.0-2.6
	 * [7]     1.7-1.95
	 * [6:0]   reserved
	 */

	cmd.cmd_index = CMD1_SEND_OP_COND;
	cmd.argument = host->ocr;
	cmd.cmd_type = RAW_MMC_CMD_BCAST_W_RESP;
	cmd.resp_type = RAW_MMC_RESP_R3;

	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* Now it's time to examine response */
	mmc_resp = cmd.resp[0];

	/* Response contains card's ocr. Update card's information */
	card->ocr = mmc_resp;

	/* Check the response for busy status */
	if (!(mmc_resp & RAW_MMC_OCR_BUSY))
		return RAW_MMC_E_CARD_BUSY;

	if (mmc_resp & RAW_MMC_OCR_SEC_MODE)
		card->type = RAW_MMC_TYPE_MMCHC;
	else
		card->type = RAW_MMC_TYPE_STD_MMC;
	return RAW_MMC_E_SUCCESS;
}

/*
 * Request any card to send its uniquie card identification (CID) number (CMD2).
 */
static unsigned int raw_mmc_all_send_cid(struct raw_mmc_card *card)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD2 Format:
	 * [31:0] stuff bits
	 */
	cmd.cmd_index = CMD2_ALL_SEND_CID;
	cmd.argument = 0;
	cmd.cmd_type = RAW_MMC_CMD_BCAST_W_RESP;
	cmd.resp_type = RAW_MMC_RESP_R2;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* Response contains card's 128 bits CID register */
	mmc_ret = raw_mmc_decode_and_save_cid(card, cmd.resp);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;
	return RAW_MMC_E_SUCCESS;
}

/*
 * Ask any card to send it's relative card address (RCA).This RCA number is
 * shorter than CID and is used by the host to address the card in future (CMD3)
 */
static unsigned int raw_mmc_send_relative_address(struct raw_mmc_card
		*card)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD3 Format:
	 * [31:0] stuff bits
	 */
	if (card->type == RAW_MMC_TYPE_SDHC
			|| card->type == RAW_MMC_TYPE_STD_SD) {
		cmd.cmd_index = CMD3_SEND_RELATIVE_ADDR;
		cmd.argument = 0;
		cmd.cmd_type = RAW_MMC_CMD_BCAST_W_RESP;
		cmd.resp_type = RAW_MMC_RESP_R6;

		/* send command */
		mmc_ret = raw_mmc_send_command(&cmd);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;
		/* For sD, card will send RCA. Store it */
		card->rca = (cmd.resp[0] >> 16);
	} else {
		cmd.cmd_index = CMD3_SEND_RELATIVE_ADDR;
		cmd.argument = (MMC_RCA << 16);
		card->rca = (cmd.argument >> 16);
		cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
		cmd.resp_type = RAW_MMC_RESP_R1;

		/* send command */
		mmc_ret = raw_mmc_send_command(&cmd);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;
	}

	return RAW_MMC_E_SUCCESS;
}

/*
 * Requests card to send it's CSD register's contents. (CMD9)
 */
static unsigned int raw_mmc_send_csd(struct raw_mmc_card *card)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_arg = 0;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD9 Format:
	 * [31:16] RCA
	 * [15:0] stuff bits
	 */
	mmc_arg |= card->rca << 16;

	cmd.cmd_index = CMD9_SEND_CSD;
	cmd.argument = mmc_arg;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R2;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* Response contains card's 128 bits CSD register */
	/* Decode and save the register */
	mmc_ret = raw_mmc_decode_and_save_csd(card, cmd.resp);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	return RAW_MMC_E_SUCCESS;
}

/*
 * Selects a card by sending CMD7 to the card with its RCA.
 * If RCA field is set as 0 ( or any other address ),
 * the card will be de-selected. (CMD7)
 */
static unsigned int raw_mmc_select_card(struct raw_mmc_card *card,
		unsigned int rca)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_arg = 0;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD7 Format:
	 * [31:16] RCA
	 * [15:0] stuff bits
	 */
	mmc_arg |= rca << 16;

	cmd.cmd_index = CMD7_SELECT_DESELECT_CARD;
	cmd.argument = mmc_arg;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	/* If we are deselecting card, we do not get response */
	if (rca == card->rca && rca) {
		if (card->type == RAW_MMC_TYPE_SDHC
				|| card->type == RAW_MMC_TYPE_STD_SD)
			cmd.resp_type = RAW_MMC_RESP_R1B;
		else
			cmd.resp_type = RAW_MMC_RESP_R1;
	} else
		cmd.resp_type = RAW_MMC_RESP_NONE;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* As of now no need to look into a response. If it's required
	 * we'll explore later on */

	return RAW_MMC_E_SUCCESS;
}

/*
 * Send command to set block length.
 */
static unsigned int raw_mmc_set_block_len(struct raw_mmc_card *card,
		unsigned int block_len)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD16 Format:
	 * [31:0] block length
	 */

	cmd.cmd_index = CMD16_SET_BLOCKLEN;
	cmd.argument = block_len;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* If blocklength is larger than 512 bytes,
	 * the card sets BLOCK_LEN_ERROR bit. */
	if (cmd.resp[0] & RAW_MMC_R1_BLOCK_LEN_ERR)
		return RAW_MMC_E_BLOCKLEN_ERR;
	return RAW_MMC_E_SUCCESS;
}

/*
 * Requests the card to stop transmission of data.
 */
static unsigned int raw_mmc_send_stop_transmission(struct raw_mmc_card
		*card,
		unsigned int
		prg_enabled)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD12 Format:
	 * [31:0] stuff bits
	 */

	cmd.cmd_index = CMD12_STOP_TRANSMISSION;
	cmd.argument = 0;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1B;
	cmd.xfer_mode = RAW_MMC_XFER_MODE_BLOCK;
	cmd.prg_enabled = prg_enabled;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;
	return RAW_MMC_E_SUCCESS;
}

/*
 * Get the card's current status
 */
static unsigned int raw_mmc_get_card_status(struct raw_mmc_card *card,
		unsigned int prg_enabled,
		unsigned int *status)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD13 Format:
	 * [31:16] RCA
	 * [15:0] stuff bits
	 */
	cmd.cmd_index = CMD13_SEND_STATUS;
	cmd.argument = card->rca << 16;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;
	cmd.prg_enabled = prg_enabled;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* Checking ADDR_OUT_OF_RANGE error in CMD13 response */
	if (IS_ADDR_OUT_OF_RANGE(cmd.resp[0]))
		return RAW_MMC_E_FAILURE;

	*status = cmd.resp[0];
	return RAW_MMC_E_SUCCESS;
}

/*
 * Decode type of error caused during read and write
 */
static unsigned int raw_mmc_status_error(unsigned mmc_status)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* If DATA_CRC_FAIL bit is set to 1 then CRC error was detected by
	   card/device during the data transfer */
	if (mmc_status & RAW_MMC_MCI_STAT_DATA_CRC_FAIL)
		mmc_ret = RAW_MMC_E_DATA_CRC_FAIL;
	/* If DATA_TIMEOUT bit is set to 1 then the data transfer time exceeded
	   the data timeout period without completing the transfer */
	else if (mmc_status & RAW_MMC_MCI_STAT_DATA_TIMEOUT)
		mmc_ret = RAW_MMC_E_DATA_TIMEOUT;
	/* If RX_OVERRUN bit is set to 1 then SDCC2 tried to receive data from
	   the card before empty storage for new received data was available.
	   Verify that bit FLOW_ENA in MCI_CLK is set to 1 during the data xfer.
	   */
	else if (mmc_status & RAW_MMC_MCI_STAT_RX_OVRRUN)
		/* Note: We've set FLOW_ENA bit in MCI_CLK to 1. so no need to
		 * verify for now */
		mmc_ret = RAW_MMC_E_RX_OVRRUN;
	/* If TX_UNDERRUN bit is set to 1 then SDCC2 tried to send data to
	   the card before new data for sending was available. Verify that bit
	   FLOW_ENA in MCI_CLK is set to 1 during the data xfer. */
	else if (mmc_status & RAW_MMC_MCI_STAT_TX_UNDRUN)
		/* Note: We've set FLOW_ENA bit in MCI_CLK to 1.so skipping it
		 * now */
		mmc_ret = RAW_MMC_E_RX_OVRRUN;
	return mmc_ret;
}

/*
 * Switch command
 */
static unsigned int raw_mmc_switch_cmd(struct raw_mmc_card *card,
		unsigned access,
		unsigned index, unsigned value)
{

	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD6 Format:
	 * [31:26] set to 0
	 * [25:24] access
	 * [23:16] index
	 * [15:8] value
	 * [7:3] set to 0
	 * [2:0] cmd set
	 */
	cmd.cmd_index = CMD6_SWITCH_FUNC;
	cmd.argument |= (access << 24);
	cmd.argument |= (index << 16);
	cmd.argument |= (value << 8);
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1B;

	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	return RAW_MMC_E_SUCCESS;
}

/*
 * A command to set the data bus width for card. Set width to either
 */
static unsigned int raw_mmc_set_bus_width(struct raw_mmc_card *card,
		unsigned int width)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;
	unsigned int mmc_reg = 0;
	unsigned int mmc_width = 0;
	unsigned int status;

	if (width != RAW_MMC_BUS_WIDTH_1_BIT)
		mmc_width = width - 1;


	do {
		mmc_ret = raw_mmc_get_card_status(card, 1, &status);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;
	} while (RAW_MMC_CARD_STATUS(status) == RAW_MMC_PROG_STATE);

	if (RAW_MMC_CARD_STATUS(status) != RAW_MMC_TRAN_STATE)
		return RAW_MMC_E_FAILURE;

	mmc_ret =
		raw_mmc_switch_cmd(card, RAW_MMC_ACCESS_WRITE,
				RAW_MMC_EXT_CMMC_BUS_WIDTH, mmc_width);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* set MCI_CLK accordingly */
	mmc_reg = readl(RAW_MMC_MCI_CLK);
	mmc_reg &= ~RAW_MMC_MCI_CLK_WIDEBUS_MODE;
	if (width == RAW_MMC_BUS_WIDTH_1_BIT)
		mmc_reg |= RAW_MMC_MCI_CLK_WIDEBUS_1_BIT;
	else if (width == RAW_MMC_BUS_WIDTH_4_BIT)
		mmc_reg |= RAW_MMC_MCI_CLK_WIDEBUS_4_BIT;
	else if (width == RAW_MMC_BUS_WIDTH_8_BIT)
		mmc_reg |= RAW_MMC_MCI_CLK_WIDEBUS_8_BIT;
	writel(mmc_reg, RAW_MMC_MCI_CLK);

	mdelay(10);		/* Giving some time to card to stabilize. */

	return RAW_MMC_E_SUCCESS;
}


/*
 * A command to start data write to card. Either a single block or
 * multiple blocks can be written. Multiple block write will continuously
 * transfer data from host to card unless requested to stop by issuing
 * CMD12 - STOP_TRANSMISSION.
 */
static unsigned int raw_mmc_send_write_command(struct raw_mmc_card *card,
		unsigned int xfer_type,
		unsigned int data_addr)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	/* basic check */
	if (card == NULL)
		return RAW_MMC_E_INVAL;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* CMD24/25 Format:
	 * [31:0] Data Address
	 */
	if (xfer_type == RAW_MMC_XFER_MULTI_BLOCK)
		cmd.cmd_index = CMD25_WRITE_MULTIPLE_BLOCK;
	else
		cmd.cmd_index = CMD24_WRITE_SINGLE_BLOCK;

	cmd.argument = data_addr;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;

	/* send command */
	mmc_ret = raw_mmc_send_command(&cmd);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* Response contains 32 bit Card status. Here we'll check
	   BLOCK_LEN_ERROR and ADDRESS_ERROR */
	if (cmd.resp[0] & RAW_MMC_R1_BLOCK_LEN_ERR)
		return RAW_MMC_E_BLOCKLEN_ERR;
	/* Misaligned address not matching block length */
	if (cmd.resp[0] & RAW_MMC_R1_ADDR_ERR)
		return RAW_MMC_E_ADDRESS_ERR;

	return RAW_MMC_E_SUCCESS;
}


/*
 * Write data_len data to address specified by data_addr. data_len is
 * multiple of blocks for block data transfer.
 */
static unsigned int raw_mmc_write_to_card(struct raw_mmc_host *host,
		struct raw_mmc_card *card,
		unsigned long long data_addr,
		unsigned int data_len,
		unsigned int *in)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;
	unsigned int mmc_status = 0;
	unsigned int *mmc_ptr = in;
	unsigned int mmc_count = 0;
	unsigned int mmc_reg = 0;
	unsigned int addr;
	unsigned int xfer_type;
	unsigned int write_error;
	unsigned int status;
	int i;

	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	/* Set block length. High Capacity MMC/SD card uses fixed 512 bytes block
	   length. So no need to send CMD16. */
	if ((card->type != RAW_MMC_TYPE_MMCHC)
			&& (card->type != RAW_MMC_TYPE_SDHC)) {
		mmc_ret = raw_mmc_set_block_len(card, card->wr_block_len);
		if (mmc_ret != RAW_MMC_E_SUCCESS) {
			pr_crit("Error No.%d: Failure setting block length for "
					"Card (RCA:%s)\n", mmc_ret,
					(char *)(card->rca));
			return mmc_ret;
		}
	}

	/* use multi-block mode to transfer for data larger than a block */
	xfer_type =
		(data_len >
		 card->rd_block_len) ? RAW_MMC_XFER_MULTI_BLOCK :
		RAW_MMC_XFER_SINGLE_BLOCK;

	/* For MMCHC/SDHC data address is specified in unit of 512B */
	addr = ((card->type != RAW_MMC_TYPE_MMCHC)
			&& (card->type != RAW_MMC_TYPE_SDHC))
		? (unsigned int) data_addr : (unsigned int) (data_addr / 512);

	/* Set the FLOW_ENA bit of MCI_CLK register to 1 */
	mmc_reg = readl(RAW_MMC_MCI_CLK);
	mmc_reg |= RAW_MMC_MCI_CLK_ENA_FLOW;
	writel(mmc_reg, RAW_MMC_MCI_CLK);

	/* Write data timeout period to MCI_DATA_TIMER register */
	/* Data timeout period should be in card bus clock periods */
	/*TODO: Fix timeout value */
	mmc_reg = 0xFFFFFFFF;
	writel(mmc_reg, RAW_MMC_MCI_DATA_TIMER);

	/* Write the total size of the transfer data to MCI_DATA_LENGTH
	 * register */
	writel(data_len, RAW_MMC_MCI_DATA_LENGTH);

	/* Send command to the card/device in order to start the write data
	 * xfer. The possible commands are CMD24/25/53/60/61 */
	mmc_ret = raw_mmc_send_write_command(card, xfer_type, addr);
	if (mmc_ret != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure sending write command to the "
				"Card(RCA:%x)\n", mmc_ret, card->rca);
		return mmc_ret;
	}

	/* Set appropriate fields and write the MCI_DATA_CTL register */
	/* Set ENABLE bit to 1 to enable the data transfer. */
	mmc_reg = 0;
	mmc_reg |= RAW_MMC_MCI_DATA_ENABLE;
	/* Clear DIRECTION bit to 0 to enable transfer from host to card */
	/* Clear MODE bit to 0 to enable block oriented data transfer. For
	   MMC cards only, if stream data transfer mode is desired, set
	   MODE bit to 1. */
	/* Set DM_ENABLE bit to 1 in order to enable DMA, otherwise set 0 */
	/* Write size of block to be used during the data transfer to
	   BLOCKSIZE field */
	mmc_reg |= card->wr_block_len << RAW_MMC_MCI_BLKSIZE_POS;
	writel(mmc_reg, RAW_MMC_MCI_DATA_CTL);

	write_error = RAW_MMC_MCI_STAT_DATA_CRC_FAIL |
		RAW_MMC_MCI_STAT_DATA_TIMEOUT | RAW_MMC_MCI_STAT_TX_UNDRUN;

	/* Write the transfer data to SDCC3 FIFO */
	/* If Data Mover is used for data transfer, prepare a command list entry
	   and enable the Data Mover to work with SDCC2 */
	/* If Data Mover is NOT used for data xfer: */
	do {
		mmc_ret = RAW_MMC_E_SUCCESS;
		mmc_status = readl(RAW_MMC_MCI_STATUS);

		if (mmc_status & write_error) {
			mmc_ret = raw_mmc_status_error(mmc_status);
			break;
		}

		/* Write the data in MCI_FIFO register as long as TXFIFO_FULL
		 * bit of
		   MCI_STATUS register is 0. Continue the writes until the whole
		   transfer data is written. */
		if (((data_len - mmc_count) >= RAW_MMC_MCI_FIFO_SIZE / 2)
				&& (mmc_status &
					RAW_MMC_MCI_STAT_TX_FIFO_HFULL)) {
			for (i = 0; i < RAW_MMC_MCI_HFIFO_COUNT; i++) {
				/* FIFO contains 16 32-bit data buffer on 16
				 * sequential addresses */
				writel(*mmc_ptr, RAW_MMC_MCI_FIFO +
						(mmc_count %
						 RAW_MMC_MCI_FIFO_SIZE));
				mmc_ptr++;
				/* increase mmc_count by word size */
				mmc_count += sizeof(unsigned int);
			}

		} else if (!(mmc_status & RAW_MMC_MCI_STAT_TX_FIFO_FULL)
				&& (mmc_count != data_len)) {
			/* FIFO contains 16 32-bit data buffer on 16
			 * sequential addresses */
			writel(*mmc_ptr, RAW_MMC_MCI_FIFO +
					(mmc_count % RAW_MMC_MCI_FIFO_SIZE));
			mmc_ptr++;
			/* increase mmc_count by word size */
			mmc_count += sizeof(unsigned int);
		} else if ((mmc_status & RAW_MMC_MCI_STAT_DATA_END))
			break;	/* success */

	} while (1);

	if (mmc_ret != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure on data transfer from the "
				"Card(RCA:%x)\n", mmc_ret, card->rca);
		/* In case of any failure happening for multi block transfer */
		if (xfer_type == RAW_MMC_XFER_MULTI_BLOCK)
			raw_mmc_send_stop_transmission(card, 1);
		return mmc_ret;
	}

	/* Send command to the card/device in order to poll the de-assertion of
	   card/device BUSY condition. It is important to set PROG_ENA bit in
	   MCI_CLK register before sending the command. Possible commands are
	   CMD12/13. */
	if (xfer_type == RAW_MMC_XFER_MULTI_BLOCK) {
		mmc_ret = raw_mmc_send_stop_transmission(card, 1);
		if (mmc_ret != RAW_MMC_E_SUCCESS) {
			pr_crit("Error No.%d: Failure sending Stop "
					"Transmission command to the "
					"Card(RCA:%x)\n", mmc_ret, card->rca);
			return mmc_ret;
		}
	} else {
		mmc_ret = raw_mmc_get_card_status(card, 1, &status);
		if (mmc_ret != RAW_MMC_E_SUCCESS) {
			pr_crit("Error No.%d: Failure getting card status of "
					"Card(RCA:%x)\n", mmc_ret, card->rca);
			return mmc_ret;
		}
	}

	/* Wait for interrupt or poll on PROG_DONE bit of MCI_STATUS register.
	 * If PROG_DONE bit is set to 1 it means that the card finished it
	 * programming and stopped driving DAT0 line to 0 */
	do {
		mmc_status = readl(RAW_MMC_MCI_STATUS);
		if (mmc_status & RAW_MMC_MCI_STAT_PROG_DONE)
			break;
	} while (1);

	return RAW_MMC_E_SUCCESS;
}


/*
 * Adjust the interface speed to optimal speed
 */
static unsigned int raw_mmc_adjust_interface_speed(struct raw_mmc_host
		*host, struct raw_mmc_card
		*card)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;
	unsigned int status;


	do {
		mmc_ret = raw_mmc_get_card_status(card, 1, &status);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;
	} while (RAW_MMC_CARD_STATUS(status) == RAW_MMC_PROG_STATE);

	if (RAW_MMC_CARD_STATUS(status) != RAW_MMC_TRAN_STATE)
		return RAW_MMC_E_FAILURE;

	/* Setting HS_TIMING in EXT_CSD (CMD6) */
	mmc_ret =
		raw_mmc_switch_cmd(card, RAW_MMC_ACCESS_WRITE,
				RAW_MMC_EXT_CMMC_HS_TIMING, 1);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;
	mmc_ret = raw_mmc_enable_clock(host, MMC_CLK_50MHZ);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return RAW_MMC_E_CLK_ENABLE_FAIL;
	return RAW_MMC_E_SUCCESS;
}

/*
 * Initialize host structure, set and enable clock-rate and power mode.
 */
static unsigned int raw_mmc_init(struct raw_mmc_host *host)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;
	unsigned int mmc_pwr = 0;


	host->ocr = RAW_MMC_OCR_27_36 | RAW_MMC_OCR_SEC_MODE;
	host->cmd_retry = RAW_MMC_MAX_COMMAND_RETRY;
	host->clk_enabled = 0;

	/* clock frequency should be less than 400KHz in identification mode */
	mmc_ret = raw_mmc_enable_clock(host, MMC_CLK_400KHZ);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return RAW_MMC_E_CLK_ENABLE_FAIL;

	/* set power mode */
	/* give some time to reach minimum voltate */
	mdelay(2);
	mmc_pwr &= ~RAW_MMC_MCI_PWR_UP;
	mmc_pwr |= RAW_MMC_MCI_PWR_ON;
	mmc_pwr |= RAW_MMC_MCI_PWR_UP;
	writel(mmc_pwr, RAW_MMC_MCI_POWER);
	/* some more time to stabilize voltage */
	mdelay(2);

	return RAW_MMC_E_SUCCESS;
}

/*
 * Performs card identification process by getting card's unique identification
 * number (CID) and relative card address (RCA). After that card will be in
 * stand-by state.
 */
static unsigned int raw_mmc_identify_card(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	unsigned int mmc_return = RAW_MMC_E_SUCCESS;

	/* basic check */
	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	/* Ask card to send its unique card identification
	 * (CID) number (CMD2) */
	mmc_return = raw_mmc_all_send_cid(card);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No. %d: Failure getting card's CID number!\n",
				mmc_return);
		return mmc_return;
	}

	/* Ask card to send a relative card address (RCA) (CMD3) */
	mmc_return = raw_mmc_send_relative_address(card);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No. %d: Failure getting card's RCA!\n",
				mmc_return);
		return mmc_return;
	}

	/* Set the card status as active */
	card->status = RAW_MMC_STATUS_ACTIVE;

	/* Get card's CSD register (CMD9) */
	mmc_return = raw_mmc_send_csd(card);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure getting card's CSD "
				"information!\n", mmc_return);
		return mmc_return;
	}

	/* Once CSD is received, set read and write timeout value now itself */
	mmc_return = raw_mmc_set_read_timeout(host, card);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure setting Read Timeout value!\n",
				mmc_return);
		return mmc_return;
	}

	mmc_return = raw_mmc_set_write_timeout(host, card);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure setting Write Timeout value!\n",
				mmc_return);
		return mmc_return;
	}

	return RAW_MMC_E_SUCCESS;
}

static unsigned int raw_mmc_send_app_cmd(unsigned int rca)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	cmd.cmd_index = CMD55_APP_CMD;
	cmd.argument = (rca << 16);
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;

	mmc_ret = raw_mmc_send_command(&cmd);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	return RAW_MMC_E_SUCCESS;
}

static unsigned int raw_mmc_sd_init_card(struct raw_mmc_card *card)
{
	unsigned int i, mmc_ret;
	unsigned int ocr_cmd_arg;
	struct raw_mmc_command cmd;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));
	ocr_cmd_arg = 0;

	/* Send CMD8 to set interface condition */
	for (i = 0; i < 3; i++) {
		cmd.cmd_index = CMD8_SEND_IF_COND;
		cmd.argument = RAW_MMC_SD_HC_VOLT_SUPPLIED;
		cmd.cmd_type = RAW_MMC_CMD_BCAST_W_RESP;
		cmd.resp_type = RAW_MMC_RESP_R7;

		mmc_ret = raw_mmc_send_command(&cmd);
		if (mmc_ret == RAW_MMC_E_SUCCESS) {
			if (cmd.resp[0] != RAW_MMC_SD_HC_VOLT_SUPPLIED)
				return RAW_MMC_E_FAILURE;
			/* Set argument for ACMD41 */
			ocr_cmd_arg =
				RAW_MMC_SD_NEG_OCR | RAW_MMC_SD_HC_HCS;
			break;
		}
		mdelay(1);
	}

	/* Send ACMD41 to set operating condition */
	/* Try for a max of 1 sec as per spec */
	for (i = 0; i < 20; i++) {
		mmc_ret = raw_mmc_send_app_cmd(0);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;

		cmd.cmd_index = ACMD41_SEND_OP_COND;
		cmd.argument = ocr_cmd_arg;
		cmd.cmd_type = RAW_MMC_CMD_BCAST_W_RESP;
		cmd.resp_type = RAW_MMC_RESP_R3;

		mmc_ret = raw_mmc_send_command(&cmd);
		if (mmc_ret != RAW_MMC_E_SUCCESS)
			return mmc_ret;
		else if (cmd.resp[0] & RAW_MMC_SD_DEV_READY) {
			/* Check for HC */
			if (cmd.resp[0] & (1 << 30))
				card->type = RAW_MMC_TYPE_SDHC;
			else
				card->type = RAW_MMC_TYPE_STD_SD;
			break;
		}
		mdelay(50);
	}
	return RAW_MMC_E_SUCCESS;
}

/*
 * Routine to initialize MMC card. It resets a card to idle state, verify
 * operating voltage and set the card inready state.
 */
static unsigned int raw_mmc_init_card(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	unsigned int mmc_retry = 0;
	unsigned int mmc_return = RAW_MMC_E_SUCCESS;

	/* basic check */
	if ((host == NULL) || (card == NULL))
		return RAW_MMC_E_INVAL;

	/* 1. Card Reset - CMD0 */
	mmc_return = raw_mmc_reset_cards();
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.:%d: Failure resetting MMC cards!\n",
				mmc_return);
		return mmc_return;
	}

	/* 2. Card Initialization process */

	/* Send CMD1 to identify and reject cards that do not match host's
	 * VDD range profile. Cards sends its OCR register in response.
	 */
	mmc_retry = 0;
	do {
		mmc_return = raw_mmc_send_op_cond(host, card);
		/* Card returns busy status. We'll retry again! */
		if (mmc_return == RAW_MMC_E_CARD_BUSY) {
			mmc_retry++;
			mdelay(200);
			continue;
		} else if (mmc_return == RAW_MMC_E_SUCCESS)
			break;
		else {
			pr_crit("Error No. %d: Failure Initializing MMC "
					"Card!\n", mmc_return);

			/* Check for sD card */
			mmc_return = raw_mmc_sd_init_card(card);
			return mmc_return;
		}
	} while (mmc_retry < host->cmd_retry);

	/* If card still returned busy status we are out of luck.
	 * Card cannot be initialized */
	if (mmc_return == RAW_MMC_E_CARD_BUSY) {
		pr_crit("Error No. %d: Card has busy status set. "
				"Initialization not completed\n", mmc_return);
		return RAW_MMC_E_CARD_BUSY;
	}
	return RAW_MMC_E_SUCCESS;
}


static unsigned int raw_mmc_set_sd_bus_width(struct raw_mmc_card *card,
		unsigned int width)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;
	unsigned int sd_reg;

	mmc_ret = raw_mmc_send_app_cmd(card->rca);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* Send ACMD6 to set bus width */
	cmd.cmd_index = ACMD6_SET_BUS_WIDTH;
	/* 10 => 4 bit wide */
	cmd.argument = (1 << 1);
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;

	mmc_ret = raw_mmc_send_command(&cmd);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	/* set MCI_CLK accordingly */
	sd_reg = readl(RAW_MMC_MCI_CLK);
	sd_reg &= ~RAW_MMC_MCI_CLK_WIDEBUS_MODE;
	if (width == RAW_MMC_BUS_WIDTH_1_BIT)
		sd_reg |= RAW_MMC_MCI_CLK_WIDEBUS_1_BIT;
	else if (width == RAW_MMC_BUS_WIDTH_4_BIT)
		sd_reg |= RAW_MMC_MCI_CLK_WIDEBUS_4_BIT;
	else if (width == RAW_MMC_BUS_WIDTH_8_BIT)
		sd_reg |= RAW_MMC_MCI_CLK_WIDEBUS_8_BIT;
	writel(sd_reg, RAW_MMC_MCI_CLK);

	mdelay(10);		/* Giving some time to card to stabilize. */

	return RAW_MMC_E_SUCCESS;
}

static unsigned int raw_mmc_set_sd_hs(struct raw_mmc_host *host,
		struct raw_mmc_card *card)
{
	struct raw_mmc_command cmd;
	unsigned int mmc_ret;

	memset((struct raw_mmc_command *) &cmd, 0,
			sizeof(struct raw_mmc_command));

	/* Send CMD6 function mode 1 to set high speed */
	/* Not using mode 0 to read current consumption */
	cmd.cmd_index = CMD6_SWITCH_FUNC;
	cmd.argument = RAW_MMC_SD_SWITCH_HS;
	cmd.cmd_type = RAW_MMC_CMD_ADDRESS;
	cmd.resp_type = RAW_MMC_RESP_R1;

	mmc_ret = raw_mmc_send_command(&cmd);

	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return mmc_ret;

	mdelay(1);

	mmc_ret = raw_mmc_enable_clock(host, MMC_CLK_50MHZ);
	if (mmc_ret != RAW_MMC_E_SUCCESS)
		return RAW_MMC_E_CLK_ENABLE_FAIL;

	return RAW_MMC_E_SUCCESS;
}

/*
 * Performs initialization and identification of all the MMC cards connected
 * to the host.
 */

static unsigned int raw_mmc_init_and_identify_cards(struct raw_mmc_host
		*host, struct raw_mmc_card
		*card)
{
	unsigned int mmc_return = RAW_MMC_E_SUCCESS;
	unsigned int status;

	/* Basic check */
	if (host == NULL)
		return RAW_MMC_E_INVAL;

	/* Initialize MMC card structure */
	card->status = RAW_MMC_STATUS_INACTIVE;
	card->rd_block_len = RAW_MMC_RD_BLOCK_LEN;
	card->wr_block_len = RAW_MMC_WR_BLOCK_LEN;

	/* Start initialization process (CMD0 & CMD1) */
	mmc_return = raw_mmc_init_card(host, card);
	if (mmc_return != RAW_MMC_E_SUCCESS)
		return mmc_return;

	/* Start card identification process (CMD2, CMD3 & CMD9) */
	mmc_return = raw_mmc_identify_card(host, card);
	if (mmc_return != RAW_MMC_E_SUCCESS)
		return mmc_return;

	/* Select the card (CMD7) */
	mmc_return = raw_mmc_select_card(card, card->rca);
	if (mmc_return != RAW_MMC_E_SUCCESS) {
		pr_crit("Error No.%d: Failure selecting the Card with "
				"RCA: %x\n", mmc_return, card->rca);
		return mmc_return;
	}

	if (card->type == RAW_MMC_TYPE_SDHC
			|| card->type == RAW_MMC_TYPE_STD_SD) {
		mmc_return = raw_mmc_set_sd_hs(host, card);
		if (mmc_return != RAW_MMC_E_SUCCESS)
			return mmc_return;

		mmc_return =
			raw_mmc_set_sd_bus_width(card,
					RAW_MMC_BUS_WIDTH_4_BIT);
		if (mmc_return != RAW_MMC_E_SUCCESS)
			return mmc_return;
	} else {
		/* set interface speed */
		mmc_return = raw_mmc_adjust_interface_speed(host, card);
		if (mmc_return != RAW_MMC_E_SUCCESS) {
			pr_crit("Error No.%d: Error adjusting interface "
					"speed!\n", mmc_return);
			return mmc_return;
		}

		/* enable wide bus */
		mmc_return =
			raw_mmc_set_bus_width(card, RAW_MMC_BUS_WIDTH_4_BIT);
		if (mmc_return != RAW_MMC_E_SUCCESS) {
			pr_crit("Error No.%d: Failure to set wide bus for "
					"Card(RCA:%x)\n", mmc_return,
					card->rca);
			return mmc_return;
		}
	}

	/* Just checking whether we're in TRAN state after changing speed and
	 * bus width */
	mmc_return = raw_mmc_get_card_status(card, 1, &status);
	if (mmc_return != RAW_MMC_E_SUCCESS)
		return mmc_return;

	if (RAW_MMC_CARD_STATUS(status) != RAW_MMC_TRAN_STATE)
		return RAW_MMC_E_FAILURE;

	return RAW_MMC_E_SUCCESS;
}

static void mmc_controller_reset(void)
{
	struct clk *sdc_clk;

	sdc_clk = clk_get(&(msm_device_sdc1.dev), "core_clk");
	if (sdc_clk) {
		clk_reset(sdc_clk, 1);
		clk_reset(sdc_clk, 0);
	}
}

/*
 * Entry point to MMC boot process
 */
static unsigned int raw_mmc_main(unsigned char slot, unsigned int base)
{
	unsigned int mmc_ret = RAW_MMC_E_SUCCESS;

	memset((struct raw_mmc_host *) &mmc_host, 0,
			sizeof(struct raw_mmc_host));
	memset((struct raw_mmc_card *) &mmc_card, 0,
			sizeof(struct raw_mmc_card));

	mmc_slot = slot;

	/* Initialize necessary data structure and enable/set clock and power */
	pr_debug(" Initializing MMC host data structure and clock!\n");
	mmc_controller_reset();
	mmc_ret = raw_mmc_init(&mmc_host);
	if (mmc_ret != RAW_MMC_E_SUCCESS) {
		pr_crit("MMC Boot: Error Initializing MMC Card!!!\n");
		return RAW_MMC_E_FAILURE;
	}

	/* Initialize and identify cards connected to host */
	mmc_ret = raw_mmc_init_and_identify_cards(&mmc_host, &mmc_card);
	if (mmc_ret != RAW_MMC_E_SUCCESS) {
		pr_crit("MMC Boot: Failure detecting MMC card!!!\n");
		return RAW_MMC_E_FAILURE;
	}

	mmc_display_csd();
	mmc_display_ext_csd();

	return RAW_MMC_E_SUCCESS;
}

/*
 * MMC write function
 */
static unsigned int mmc_write(unsigned long long data_addr,
		unsigned int data_len, unsigned int *in)
{
	int val = 0;
	unsigned int write_size = ((unsigned) (0xFFFFFF / 512)) * 512;
	unsigned offset = 0;
	unsigned int *sptr = in;

	if (data_len % 512)
		data_len = ALIGN(data_len, 512);

	while (data_len > write_size) {
		val = raw_mmc_write_to_card(&mmc_host, &mmc_card,
				data_addr + offset,
				write_size, sptr);
		if (val)
			return val;

		sptr += (write_size / sizeof(unsigned));
		offset += write_size;
		data_len -= write_size;
	}
	if (data_len) {
		val = raw_mmc_write_to_card(&mmc_host, &mmc_card,
				data_addr + offset,
				data_len, sptr);
		offset += data_len;
	}
	if (val == RAW_MMC_E_SUCCESS)
		val = offset;
	return val;
}

static void mmc_display_ext_csd(void)
{
	pr_debug("part_config: %x\n", ext_csd_buf[179]);
	pr_debug("erase_group_def: %x\n", ext_csd_buf[175]);
	pr_debug("user_wp: %x\n", ext_csd_buf[171]);
}

static void mmc_display_csd(void)
{
	pr_debug("erase_grpsize: %d\n", mmc_card.csd.erase_grp_size);
	pr_debug("erase_grpmult: %d\n", mmc_card.csd.erase_grp_mult);
	pr_debug("wp_grpsize: %d\n", mmc_card.csd.wp_grp_size);
	pr_debug("wp_grpen: %d\n", mmc_card.csd.wp_grp_enable);
	pr_debug("perm_wp: %d\n", mmc_card.csd.perm_wp);
	pr_debug("temp_wp: %d\n", mmc_card.csd.temp_wp);
}

static int raw_mmc_probe_emmc(struct hd_struct *rhd)
{
	pr_debug("start sec is: %ul\n", (unsigned int) rhd->start_sect);
	pr_debug("number of secs are: %ul\n", (unsigned int) rhd->nr_sects);
	return raw_mmc_main(1, MSM_SDC1_BASE);
}

static int raw_mmc_write_mmc(char *buf, sector_t start_sect,
		sector_t nr_sects, unsigned int offset,
		unsigned int len)
{
	if (offset + len > nr_sects * 512) {
		pr_err("try to wirte addr out of range\n");
		return 0;
	}

	return mmc_write((unsigned long long) (start_sect * 512 + offset),
			len, (unsigned int *) buf);
}

static int raw_mmc_panic_probe(struct hd_struct *rhd, int type)
{
	if (type == MMC_TYPE_MMC) {
		pr_debug("into probe func\n");
		pr_debug("mmc type : %d\n", type);
		return raw_mmc_probe_emmc(rhd);
	}
	return -EIO;
}

static int raw_mmc_panic_write(struct hd_struct *rhd, char *buf,
		unsigned int offset, unsigned int len)
{
	return raw_mmc_write_mmc(buf, rhd->start_sect, rhd->nr_sects,
			offset, len);
}

static int raw_mmc_panic_erase(struct hd_struct *rhd, unsigned int offset,
		unsigned int len)
{
	/*
	 * apanic_mmc erase mmc by means of bio,
	 * no need to impl raw erase here
	 */
	return -EIO;
}

static struct raw_mmc_panic_ops msm_sdcc_raw_panic_ops = {
	.panic_probe   = raw_mmc_panic_probe,
	.panic_write   = raw_mmc_panic_write,
	.panic_erase   = raw_mmc_panic_erase,
};

static int  __init msm_init_apanic(void)
{
	raw_mmc_mci_base = ioremap(MSM_SDC1_BASE, PAGE_SIZE);
	if (!raw_mmc_mci_base) {
		pr_crit("raw mmc mci base register remap failed, no kernel "
				"log will be saved if panic happens under "
				"irq context\n");
		return 0;
	}

	apanic_mmc_init(&msm_sdcc_raw_panic_ops);

	return 0;
}
postcore_initcall(msm_init_apanic);
