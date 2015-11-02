/* platform.h
 *
 * THIS FILE DEFINES EVERY PLATFORM-DEPENDENT ELEMENT THAT THE CORE REQUIRES.
 * 
 * INSTRUCTIONS FOR THIS FILE:
 * 1. Modify this file with a definition of Generic Type Definitions
 * (INT8, UINT32, etc) Either by include or putting directly in this file.
 * 2. Include this as a header file for your platform.c and implement the 
 * function headers as defined below.
 * 
 * It is the driver-writer's responsibility to implement each function
 * stub and to allocate/initialize/reserve sufficient system resources.
 * 
 */
#ifndef _FSC_PLATFORM_H_
#define _FSC_PLATFORM_H_

/* PLATFORM_NONE
 * 
 * This is a set of stubs for no platform in particular.
 */
#ifdef PLATFORM_NONE
#include "../Platform_None/GenericTypeDefs.h"
#endif // PLATFORM_NONE

/* PLATFORM_PIC32
 * 
 * This platform is for the Microchip PIC32 microcontroller.
 */
#ifdef PLATFORM_PIC32
#include "../Platform_PIC32/GenericTypeDefs.h"
#endif // PLATFORM_PIC32

/* PLATFORM_ARM
 *
 * This platform is for the ARM M0.
 */
#ifdef PLATFORM_ARM
#include "../Platform_ARM/app/GenericTypeDefs.h"
#endif // PLATFORM_ARM

/* PLATFORM_LINUX
 * 
 * This platform is for the Linux kernel driver.
 */
#ifdef PLATFORM_LINUX
#include "../Platform_Linux/GenericTypeDefs.h"
#endif // PLATFORM_LINUX

/*******************************************************************************
 * Function:        platform_set/get_vbus_5v_enable
 * Input:           Boolean
 * Return:          Boolean
 * Description:     Provide access to the 5V VBUS control pin.
 ******************************************************************************/
void platform_set_vbus_5v_enable(BOOL blnEnable);
BOOL platform_get_vbus_5v_enable(void);

/*******************************************************************************
 * Function:        platform_set/get_vbus_12v_enable
 * Input:           Boolean
 * Return:          Boolean
 * Description:     Provide access to the 12V VBUS control pin.
 ******************************************************************************/
void platform_set_vbus_lvl1_enable(BOOL blnEnable);
BOOL platform_get_vbus_lvl1_enable(void);

/*******************************************************************************
 * Function:        platform_get_device_irq_state
 * Input:           None
 * Return:          Boolean.  TRUE = Interrupt Active
 * Description:     Get the state of the INT_N pin.  INT_N is active low.  This
 *                  function handles that by returning TRUE if the pin is
 *                  pulled low indicating an active interrupt signal.
 ******************************************************************************/
BOOL platform_get_device_irq_state(void);

/*******************************************************************************
 * Function:        platform_i2c_write
 * Input:           SlaveAddress - Slave device bus address
 *                  RegAddrLength - Register Address Byte Length
 *                  DataLength - Length of data to transmit
 *                  PacketSize - Maximum size of each transmitted packet
 *                  IncSize - Number of bytes to send before incrementing addr
 *                  RegisterAddress - Internal register address
 *                  Data - Buffer of char data to transmit
 * Return:          Error state
 * Description:     Write a char buffer to the I2C peripheral.
 ******************************************************************************/
BOOL platform_i2c_write(unsigned char SlaveAddress,
			unsigned char RegAddrLength,
			unsigned char DataLength,
			unsigned char PacketSize,
			unsigned char IncSize,
			unsigned long RegisterAddress, unsigned char *Data);

/*******************************************************************************
 * Function:        platform_i2c_read
 * Input:           SlaveAddress - Slave device bus address
 *                  RegAddrLength - Register Address Byte Length
 *                  DataLength - Length of data to attempt to read
 *                  PacketSize - Maximum size of each received packet
 *                  IncSize - Number of bytes to recv before incrementing addr
 *                  RegisterAddress - Internal register address
 *                  Data - Buffer for received char data
 * Return:          Error state.
 * Description:     Read char data from the I2C peripheral.
 ******************************************************************************/
BOOL platform_i2c_read(unsigned char SlaveAddress,
		       unsigned char RegAddrLength,
		       unsigned char DataLength,
		       unsigned char PacketSize,
		       unsigned char IncSize,
		       unsigned long RegisterAddress, unsigned char *Data);

/*****************************************************************************
* Function:        platform_enable_timer
* Input:           enable - TRUE to enable platform timer, FALSE to disable
* Return:          None
* Description:     Enables or disables platform timer
******************************************************************************/
void platform_enable_timer(BOOL enable);

/******************************************************************************
 * Function:        platform_delay_10us
 * Input:           delayCount - Number of 10us delays to wait
 * Return:          None
 * Description:     Perform a blocking software delay in intervals of 10us
 ******************************************************************************/
void platform_delay_10us(UINT32 delayCount);
void platform_disableSuperspeedUSB(void);
void platform_enableSuperspeedUSB(int CC1, int CC2);
void platform_toggleAudioSwitch(bool enable);

#endif // _FSC_PLATFORM_H_
