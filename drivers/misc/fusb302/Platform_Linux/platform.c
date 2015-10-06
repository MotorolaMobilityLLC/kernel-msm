#include <linux/kernel.h>	// printk
#include "fusb30x_global.h"	// Chip structure
#include "platform_helpers.h"	// Implementation details
#include "../core/platform.h"

/*******************************************************************************
* Function:        platform_set/get_vbus_5v_enable
* Input:           Boolean
* Return:          Boolean
* Description:     Provide access to the 5V VBUS control pin.
******************************************************************************/
void platform_set_vbus_5v_enable(BOOL blnEnable)
{
	fusb_GPIO_Set_VBus5v(blnEnable == TRUE ? true : false);
	return;
}

BOOL platform_get_vbus_5v_enable(void)
{
	return fusb_GPIO_Get_VBus5v()? TRUE : FALSE;
}

/*******************************************************************************
* Function:        platform_set/get_vbus_12v_enable
* Input:           Boolean
* Return:          Boolean
* Description:     Provide access to the 12V VBUS control pin.
******************************************************************************/
void platform_set_vbus_lvl1_enable(BOOL blnEnable)
{
	fusb_GPIO_Set_VBusOther(blnEnable);
	return;
}

BOOL platform_get_vbus_lvl1_enable(void)
{
	return fusb_GPIO_Get_VBusOther()? TRUE : FALSE;
}

/*******************************************************************************
* Function:        platform_get_device_irq_state
* Input:           None
* Return:          Boolean.  TRUE = Interrupt Active
* Description:     Get the state of the INT_N pin.  INT_N is active low.  This
*                  function handles that by returning TRUE if the pin is
*                  pulled low indicating an active interrupt signal.
******************************************************************************/
BOOL platform_get_device_irq_state(void)
{
	return fusb_GPIO_Get_IntN()? TRUE : FALSE;
}

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
			unsigned long RegisterAddress, unsigned char *Data)
{
	BOOL ret = FALSE;
	if (Data == NULL) {
		printk(KERN_ERR "%s - Error: Write data buffer is NULL!\n",
		       __func__);
		ret = FALSE;
	} else
	    if (fusb_I2C_WriteData
		((unsigned char)RegisterAddress, DataLength, Data)) {
		ret = TRUE;
	} else			// I2C Write failure
	{
		ret = FALSE;	// Write data block to the device
		printk(KERN_ERR
		       "%s - I2C write failed! RegisterAddress: 0x%02x\n",
		       __func__, (unsigned int)RegisterAddress);
	}
	return ret;
}

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
		       unsigned long RegisterAddress, unsigned char *Data)
{
	BOOL ret = FALSE;
	int i = 0;
	unsigned char temp = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return FALSE;
	}

	if (Data == NULL) {
		printk(KERN_ERR "%s - Error: Read data buffer is NULL!\n",
		       __func__);
		ret = FALSE;
	} else if (chip->use_i2c_blocks) {
		if (!fusb_I2C_ReadBlockData(RegisterAddress, DataLength, Data)) {
			printk(KERN_ERR
			       "%s - I2C block read failed! RegisterAddress: 0x%02x, Length: %u\n",
			       __func__, (unsigned int)RegisterAddress,
			       (u8) DataLength);
			ret = FALSE;
		} else {
			ret = TRUE;
		}
	} else {
		for (i = 0; i < DataLength; i++) {
			if (fusb_I2C_ReadData
			    ((UINT8) RegisterAddress + i, &temp)) {
				Data[i] = temp;
				ret = TRUE;
			} else {
				printk(KERN_ERR
				       "%s - I2C read failed! RegisterAddress: 0x%02x\n",
				       __func__, (unsigned int)RegisterAddress);
				ret = FALSE;
				break;
			}
		}
	}

	return ret;
}

/*****************************************************************************
* Function:        platform_enable_timer
* Input:           enable - TRUE to enable platform timer, FALSE to disable
* Return:          None
* Description:     Enables or disables platform timer
******************************************************************************/
void platform_enable_timer(BOOL enable)
{
	if (enable == TRUE) {
		fusb_StartTimers();
	} else {
		fusb_StopTimers();
	}
}

/*****************************************************************************
* Function:        platform_delay_10us
* Input:           delayCount - Number of 10us delays to wait
* Return:          None
* Description:     Perform a blocking software delay in intervals of 10us
******************************************************************************/
void platform_delay_10us(UINT32 delayCount)
{
	fusb_Delay10us(delayCount);
}
