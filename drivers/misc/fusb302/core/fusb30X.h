#ifndef _FUSB30X_H_
#define _FUSB30X_H_

#include "platform.h"

// Convenience interfaces to I2C_WriteData() and I2C_ReadData()
BOOL DeviceWrite(UINT8 regAddr, UINT8 length, UINT8 * data);
BOOL DeviceRead(UINT8 regAddr, UINT8 length, UINT8 * data);

// FUSB300 I2C Configuration
#define FUSB300SlaveAddr   0x44
#define FUSB300AddrLength  1	// One byte address
#define FUSB300IncSize     1	// One byte increment

// FUSB300 Register Addresses
#define regDeviceID     0x01
#define regSwitches0    0x02
#define regSwitches1    0x03
#define regMeasure      0x04
#define regSlice        0x05
#define regControl0     0x06
#define regControl1     0x07
#define regControl2     0x08
#define regControl3     0x09
#define regMask         0x0A
#define regPower        0x0B
#define regReset        0x0C
#define regOCPreg       0x0D
#define regMaska        0x0E
#define regMaskb        0x0F
#define regControl4     0x10
#define regStatus0a     0x3C
#define regStatus1a     0x3D
#define regInterrupta   0x3E
#define regInterruptb   0x3F
#define regStatus0      0x40
#define regStatus1      0x41
#define regInterrupt    0x42
#define regFIFO         0x43

#ifdef FPGA_BOARD
#define SDAC_DEFAULT        0x2A
#define MDAC_0P2V           0x04
#define MDAC_0P4V           0x08
#define MDAC_0P8V           0x10
#define MDAC_1P6V           0x1F
#define MDAC_2P05V          0x28
#define MDAC_2P6V           0x32

#define VBUS_MDAC_0P8V      0x01
#else
#define SDAC_DEFAULT        0x20
#define MDAC_0P2V           0x05
#define MDAC_0P4V           0x0A
#define MDAC_0P8V           0x13
#define MDAC_1P6V           0x26
#define MDAC_2P05V          0x31
#define MDAC_2P6V           0x3E

#define VBUS_MDAC_0P8V      0x01
#define VBUS_MDAC_3p8       0x09
#define VBUS_MDAC_4p2       0x0A
#endif

typedef union {
	UINT8 byte;
	struct {
		unsigned REVISION:3;
		unsigned VERSION:5;
	};
} regDeviceID_t;

typedef union {
	UINT16 word;
	UINT8 byte[2] __PACKED;
	struct {
		// Switches0
		unsigned PDWN1:1;
		unsigned PDWN2:1;
		unsigned MEAS_CC1:1;
		unsigned MEAS_CC2:1;
		unsigned VCONN_CC1:1;
		unsigned VCONN_CC2:1;
		unsigned PU_EN1:1;
		unsigned PU_EN2:1;
		// Switches1
		unsigned TXCC1:1;
		unsigned TXCC2:1;
		unsigned AUTO_CRC:1;
		unsigned:1;
		unsigned DATAROLE:1;
		unsigned SPECREV:2;
		unsigned POWERROLE:1;
	};
} regSwitches_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned MDAC:6;
		unsigned MEAS_VBUS:1;
		unsigned:1;
	};
} regMeasure_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned SDAC:6;
		unsigned:2;
	};
} regSlice_t;

typedef union {
	UINT32 dword;
	UINT8 byte[4] __PACKED;
	struct {
		// Control0
		unsigned TX_START:1;
		unsigned AUTO_PRE:1;
		unsigned HOST_CUR:2;
		unsigned LOOPBACK:1;
		unsigned INT_MASK:1;
		unsigned TX_FLUSH:1;
		unsigned:1;
		// Control1
		unsigned ENSOP1:1;
		unsigned ENSOP2:1;
		unsigned RX_FLUSH:1;
		unsigned:1;
		unsigned BIST_MODE2:1;
		unsigned ENSOP1DP:1;
		unsigned ENSOP2DB:1;
		unsigned:1;
		// Control2
		unsigned TOGGLE:1;
		unsigned MODE:2;
		unsigned WAKE_EN:1;
		unsigned WAKE_SELF:1;
		unsigned TOG_RD_ONLY:1;
		unsigned:2;
		// Control3
		unsigned AUTO_RETRY:1;
		unsigned N_RETRIES:2;
		unsigned AUTO_SOFTRESET:1;
		unsigned AUTO_HARDRESET:1;
		unsigned:1;
		unsigned SEND_HARDRESET:1;
		unsigned:1;
	};
} regControl_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned M_BC_LVL:1;
		unsigned M_COLLISION:1;
		unsigned M_WAKE:1;
		unsigned M_ALERT:1;
		unsigned M_CRC_CHK:1;
		unsigned M_COMP_CHNG:1;
		unsigned M_ACTIVITY:1;
		unsigned M_VBUSOK:1;
	};
} regMask_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned PWR:4;
		unsigned:4;
	};
} regPower_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned SW_RES:1;
		unsigned:7;
	};
} regReset_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned OCP_CUR:3;
		unsigned OCP_RANGE:1;
		unsigned:4;
	};
} regOCPreg_t;

typedef union {
	UINT16 word;
	UINT8 byte[2] __PACKED;
	struct {
		// Maska
		unsigned M_HARDRST:1;
		unsigned M_SOFTRST:1;
		unsigned M_TXCRCSENT:1;
		unsigned M_HARDSENT:1;
		unsigned M_RETRYFAIL:1;
		unsigned M_SOFTFAIL:1;
		unsigned M_TOGDONE:1;
		unsigned M_OCP_TEMP:1;
		// Maskb
		unsigned M_GCRCSENT:1;
		unsigned:7;
	};
} regMaskAdv_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned TOG_USRC_EXIT:1;
		unsigned:7;
	};
} regControl4_t;

typedef union {
	UINT8 byte[7] __PACKED;
	struct {
		UINT16 StatusAdv;
		UINT16 InterruptAdv;
		UINT16 Status;
		UINT8 Interrupt1;
	};
	struct {
		// Status0a
		unsigned HARDRST:1;
		unsigned SOFTRST:1;
		unsigned POWER23:2;
		unsigned RETRYFAIL:1;
		unsigned SOFTFAIL:1;
		unsigned TOGDONE:1;
		unsigned M_OCP_TEMP:1;
		// Status1a
		unsigned RXSOP:1;
		unsigned RXSOP1DB:1;
		unsigned RXSOP2DB:1;
		unsigned TOGSS:3;
		unsigned:2;
		// Interrupta
		unsigned I_HARDRST:1;
		unsigned I_SOFTRST:1;
		unsigned I_TXSENT:1;
		unsigned I_HARDSENT:1;
		unsigned I_RETRYFAIL:1;
		unsigned I_SOFTFAIL:1;
		unsigned I_TOGDONE:1;
		unsigned I_OCP_TEMP:1;
		// Interruptb
		unsigned I_GCRCSENT:1;
		unsigned:7;
		// Status0
		unsigned BC_LVL:2;
		unsigned WAKE:1;
		unsigned ALERT:1;
		unsigned CRC_CHK:1;
		unsigned COMP:1;
		unsigned ACTIVITY:1;
		unsigned VBUSOK:1;
		// Status1
		unsigned OCP:1;
		unsigned OVRTEMP:1;
		unsigned TX_FULL:1;
		unsigned TX_EMPTY:1;
		unsigned RX_FULL:1;
		unsigned RX_EMPTY:1;
		unsigned RXSOP1:1;
		unsigned RXSOP2:1;
		// Interrupt
		unsigned I_BC_LVL:1;
		unsigned I_COLLISION:1;
		unsigned I_WAKE:1;
		unsigned I_ALERT:1;
		unsigned I_CRC_CHK:1;
		unsigned I_COMP_CHNG:1;
		unsigned I_ACTIVITY:1;
		unsigned I_VBUSOK:1;
	};
} regStatus_t;

typedef struct {
	regDeviceID_t DeviceID;
	regSwitches_t Switches;
	regMeasure_t Measure;
	regSlice_t Slice;
	regControl_t Control;
	regMask_t Mask;
	regPower_t Power;
	regReset_t Reset;
	regOCPreg_t OCPreg;
	regMaskAdv_t MaskAdv;
	regControl4_t Control4;
	regStatus_t Status;
} DeviceReg_t;

#endif // _FUSB30X_H_
