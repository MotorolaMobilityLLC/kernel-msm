#ifndef __FSC_HOSTCOMM_H_
#define __FSC_HOSTCOMM_H_

// ##### Packet Structure ##### //
#define PKTOUT_REQUEST                  0x00
#define PKTOUT_VERSION                  0x01
#define PKTIN_REQUEST                   0x00
#define PKTIN_STATUS                    0x01
#define PKTIN_ERRORCODE                 0x03

// ##### Command definitions ##### //
#define CMD_GETDEVICEINFO               0x00
#define CMD_USBPD_BUFFER_READ           0xA0
#define CMD_USBPD_STATUS                0xA2
#define CMD_USBPD_CONTROL               0xA3
#define CMD_GET_SRC_CAPS                0xA4
#define CMD_GET_SINK_CAPS               0xA5
#define CMD_GET_SINK_REQ                0xA6
#define CMD_ENABLE_PD                   0xA7
#define CMD_DISABLE_PD                  0xA8
#define CMD_GET_ALT_MODES               0xA9
#define CMD_GET_MANUAL_RETRIES          0xAA
#define CMD_SET_STATE_UNATTACHED        0xAB
#define CMD_ENABLE_TYPEC_SM             0xAC
#define CMD_DISABLE_TYPEC_SM            0xAD
#define CMD_SEND_HARD_RESET             0xAE

#define CMD_DEVICE_LOCAL_REGISTER_READ  0xB0	// xBX designation used for automated testing
#define CMD_SET_STATE                   0xB1
#define CMD_READ_STATE_LOG              0xB2
#define CMD_READ_PD_STATE_LOG           0xB3

#define CMD_READ_I2C                    0xC0
#define CMD_WRITE_I2C                   0xC1
#define CMD_GET_VBUS5V                  0xC4
#define CMD_SET_VBUS5V                  0xC5
#define CMD_GET_INTN                    0xC6

#ifdef DEBUG
#define CMD_GET_TIMER_TICKS             0xF0
#define CMD_GET_SM_TICKS                0xF1
#define CMD_GET_GPIO_SM_TOGGLE          0xF2
#define CMD_SET_GPIO_SM_TOGGLE          0xF3
#endif // DEBUG

#define TEST_FIRMWARE                   0X01	// For GUI identification of firmware

/* Device Info */

/* MCU Identification */
typedef enum {
	mcuUnknown = 0,
	mcuPIC18F14K50 = 1,
	mcuPIC32MX795F512L = 2,
	mcuPIC32MX250F128B = 3,
	mcuGENERIC_LINUX = 4,	// Linux driver
} mcu_t;

/* Device Type Identification */
typedef enum {
	dtUnknown = -1,
	dtUSBI2CStandard = 0,
	dtUSBI2CPDTypeC = 1
} dt_t;

/* Board Configuration */
typedef enum {
	bcUnknown = -1,
	bcStandardI2CConfig = 0,
	bcFUSB300Eval = 0x100,
	bcFUSB302FPGA = 0x200,
	bcFM14014 = 0x300
} bc_t;

#define MY_MCU          mcuGENERIC_LINUX
#define MY_DEV_TYPE     dtUSBI2CPDTypeC
#define MY_BC           bcStandardI2CConfig

/*******************************************************************************
* Function:        fusb_ProcessMsg
* Input:           inMsgBuffer: Input array to process
*                  outMsgBuffer: Result is stored here
* Return:          none
* Description:     Processes the input array as a command with optional data.
*                  Provides a debugging interface to the caller.
********************************************************************************/
void fusb_ProcessMsg(u8 * inMsgBuffer, u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_Handle_I2CRead
* Input:           inBuf: Input buffer to parse. inBuf[0]: Addr, other bytes ignored
*                  outBuf: I2C read result stored in outBuf[0]
* Return:          none
* Description:     Read an unsigned byte from the I2C peripheral
********************************************************************************/
void fusb_hc_Handle_I2CRead(u8 * inBuf, u8 * outBuf);

/*******************************************************************************
* Function:        fusb_hc_Handle_I2CWrite
* Input:           inBuf: Input buffer to parse. 
*                       inBuf[1]: Start of address range to write to
*                       inBuf[2]: Num bytes to write
*                       inBuf[3.. inBuf[1]]: Data to write
*                  outBuf: Output buffer populated with result:
*                       Success: outBuf[0] = 1
*                       Failure: outBuf[0] = 0
* Return:          none
* Description:     Write an unsigned byte to the I2C peripheral
********************************************************************************/
void fusb_hc_Handle_I2CWrite(u8 * inBuf, u8 * outBuf);

/*******************************************************************************
* Function:        fusb_hc_GetVBus5V
* Input:           none
*                  outBuf: Output buffer populated with result:
*                       Success:    outBuf[0] = 1
*                                   outBuf[1] = value
*                       Failure:    outBuf[0] = 0
* Return:          none
* Description:     Get the value of VBus 5V
********************************************************************************/
void fusb_hc_GetVBus5V(u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_SetVBus5V
* Input:           inMsgBuffer:     inMsgBuffer[1] = [1 || 0], value to write
*                  outBuf: Output buffer populated with result:
*                       Success:    outBuf[0] = 1
*                                   outBuf[1] = value that was set
*                       Failure:    outBuf[0] = 0
* Return:          none
* Description:     Set the value of VBus 5V
********************************************************************************/
void fusb_hc_SetVBus5V(u8 * inMsgBuffer, u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_GetIntN
* Input:           none
*                  outBuf: Output buffer populated with result:
*                       Success:    outBuf[0] = 1
*                                   outBuf[1] = value
*                       Failure:    outBuf[0] = 0
* Return:          none
* Description:     Get the value of Int_N
********************************************************************************/
void fusb_hc_GetIntN(u8 * outMsgBuffer);

#ifdef DEBUG
/*******************************************************************************
* Function:        fusb_hc_GetTimerTicks
* Input:           none
*                  outBuf: Output buffer populated with result:
*                       outBuf[0]: 1 on success, 0 on failure
*                       outBuf[1]: Number of timer ticks passed
*                       outBuf[2]: Number of times the timer tick ticker rolled over
* Return:          none
* Description:     DEBUG: Get the number of timer ticks that have passed
********************************************************************************/
void fusb_hc_GetTimerTicks(u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_GetSMTicks
* Input:           none
*                  outBuf: Output buffer populated with result:
*                       outBuf[0]: 1 on success, 0 on failure
*                       outBuf[1]: Number of SM ticks passed
*                       outBuf[2]: Number of times the SM tick ticker rolled over
* Return:          none
* Description:     DEBUG: Get the number of SM ticks that have passed
********************************************************************************/
void fusb_hc_GetSMTicks(u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_GetGPIO_SM_Toggle
* Input:           none
*                  outBuf: Output buffer populated with result:
*                       Success:    outBuf[0] = 1
*                                   outBuf[1] = value
*                       Failure:    outBuf[0] = 0
* Return:          none
* Description:     Get the value of the SM toggle GPIO
********************************************************************************/
void fusb_hc_GetGPIO_SM_Toggle(u8 * outMsgBuffer);

/*******************************************************************************
* Function:        fusb_hc_SetGPIO_SM_Toggle
* Input:           inMsgBuffer:     inMsgBuffer[1] = [1 || 0], value to write
*                  outBuf: Output buffer populated with result:
*                       Success:    outBuf[0] = 1
*                                   outBuf[1] = value that was set
*                       Failure:    outBuf[0] = 0
* Return:          none
* Description:     Set the value of the SM toggle GPIO
********************************************************************************/
void fusb_hc_SetGPIO_SM_Toggle(u8 * inMsgBuffer, u8 * outMsgBuffer);
#endif // DEBUG

#endif // __FSC_HOSTCOMM_H_
