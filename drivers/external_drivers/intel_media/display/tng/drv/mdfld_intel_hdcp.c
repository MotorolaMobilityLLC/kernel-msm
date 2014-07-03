/*
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#include <drm/drmP.h>
#include <random.h>
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_hdmi_reg.h"
#include "psb_intel_hdmi.h"
#include "mdfld_hdcp_if.h"
#include "mdfld_hdcp_reg.h"
#include "mdfld_hdcp.h"

/*
 *
 */
static struct mid_intel_hdmi_priv *hdmi_priv;

void mdfld_hdcp_init(struct mid_intel_hdmi_priv *p_hdmi_priv)
{
	hdmi_priv = p_hdmi_priv;
}

/* 
 * IsValidBKSV:
 * Checks if the BKSV is valid or not.
 * A valid BKSV is one that contains 20 0's and 20 1's
 *
 */
int hdcp_is_valid_bksv(uint8_t * buffer, uint32_t size)
{
	uint8_t count = 0;
	int i = 0;
	uint8_t bksv = 0;
	uint8_t bit = 0;
	int ret = 0;

	if (buffer == NULL || size != CP_HDCP_KEY_SELECTION_VECTOR_SIZE)
		return ret;

	while (i < CP_HDCP_KEY_SELECTION_VECTOR_SIZE) {
		bksv = buffer[i];
		while (bksv != 0) {
			bit = (bksv) & 0x01;
			if (bit)
				count++;
			bksv = bksv >> 1;
		}
		i++;
	}

	if (count == 20)
		ret = 1;

	return ret;
}

/*
 * Checks if HDCP is supported
 *
 */
int hdcp_query()
{
	return hdmi_priv->is_hdcp_supported;
}

/* 
 * Gets the current status of HDCP 
 * 
 */
int hdcp_is_enabled(void)
{
	struct drm_device *dev = hdmi_priv->dev;
	mdfld_hdcp_status_t hdcp_status = { 0 };
	int ret = 0;

	if (hdmi_priv->is_hdcp_supported) {
		hdcp_status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
		ret = hdcp_status.cipher_hdcp_status;
	}

	return ret;
}

#define HDCP_PRIMARY_I2C_ADDR 0x74
/*
 *
 * Read HDCP device data from i2c link 
 *
 */
static int read_hdcp_port_data(uint8_t offset, uint8_t * buffer, int size)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = HDCP_PRIMARY_I2C_ADDR,
		 .flags = 0,
		 .len = 1,
		 .buf = &offset,
		 }, {
		     .addr = HDCP_PRIMARY_I2C_ADDR,
		     .flags = I2C_M_RD,
		     .len = size,
		     .buf = buffer,
		     }
	};

	if (i2c_transfer(hdmi_priv->hdmi_i2c_adapter, msgs, 2) == 2)
		return 1;

	return 0;
}

/* Read device status from i2c link */
int read_hdcp_port(uint32_t read_request_type, uint8_t * buffer, int size)
{
	int more_blocks_to_read = 0;
	uint32_t block_offset = 0;
	int ret = 1;
	uint8_t offset = 0;

	while (1) {
		switch (read_request_type) {
		case RX_TYPE_BSTATUS:
			offset = RX_BSTATUS_0;
			break;
		case RX_TYPE_RI_DATA:
			offset = RX_RI_HIGH;
			break;
		case RX_TYPE_BCAPS:
			offset = RX_BCAPS;
			break;
		case RX_TYPE_REPEATER_KSV_LIST:
			offset = RX_KSV_FIFO;
			break;
		case RX_TYPE_BKSV_DATA:
			offset = RX_BKSV_0;
			break;
		case RX_TYPE_REPEATER_PRIME_V:
			{
				offset = block_offset + RX_VPRIME_H0_0;
				buffer += block_offset;
				size = 4;
				if (offset < RX_VPRIME_H4_0) {
					more_blocks_to_read = 1;
					block_offset += 4;
				}
			}
			break;
		default:
			ret = 0;
			break;
		}

		if (ret) {
			if (!read_hdcp_port_data(offset, buffer, size)) {
				//I2C access failed
				ret = 0;
				break;
			}
			//Check whether more blocks are to be read
			if (!more_blocks_to_read) {
				break;
			} else {
				more_blocks_to_read = 0;
			}
		} else {
			break;
		}
	}

	return ret;
}

/* write to HDCP device through i2c link */
static int write_hdcp_port(uint8_t offset, uint8_t * buffer, int size)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = HDCP_PRIMARY_I2C_ADDR,
		 .flags = 0,
		 .len = 1,
		 .buf = &offset,
		 }, {
		     .addr = HDCP_PRIMARY_I2C_ADDR,
		     .flags = 0,
		     .len = size,
		     .buf = buffer,
		     }
	};

	if (i2c_transfer(hdmi_priv->hdmi_i2c_adapter, msgs, 2) == 2)
		return 1;

	return 0;
}

/*
 *
 * UpdateRepeaterState : Enables/Disables Repeater
 *
 */
static int hdcp_update_repeater_state(int enable)
{
	struct drm_device *dev = hdmi_priv->dev;
	mdfld_hdcp_rep_t hdcp_rep_ctrl_reg;
	hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
	hdcp_rep_ctrl_reg.repeater_present = enable;

	REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
	return 1;

}

/* 
 * EnableHDCP : Enables/Disables HDCP
 *
 */
int hdcp_enable(int enable)
{
	struct drm_device *dev = hdmi_priv->dev;
	mdfld_hdcp_config_t config;
	mdfld_hdcp_receiver_ri_t receivers_ri;
	mdfld_hdcp_status_t status;
	mdfld_hdcp_rep_t hdcp_repeater;
	uint32_t max_retry = 0;
	sqword_tt hw_an;
	sqword_tt hw_aksv;
	sqword_tt hw_bksv;
	uint8_t bcaps = 0;
	uint32_t rx_ri = 0;
	int ret = 0;
	uint32_t hdcp_init_vec;

	if (enable == 0) {
		config.value = REG_READ(MDFLD_HDCP_CONFIG_REG);
		config.hdcp_config = HDCP_Off;
		REG_WRITE(MDFLD_HDCP_CONFIG_REG, config.value);

		//Check the status of cipher till it get's turned off
		// Bug #2808007-Delay required is one frame period.
		// waiting for 2 VBlanks provides this amount of delay
		max_retry = 0;
		status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
		while ((status.cipher_hdcp_status
			|| status.cipher_encrypting_status)
		       && max_retry < HDCP_MAX_RETRY_DISABLE) {
			psb_intel_wait_for_vblank(dev);
			status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
			max_retry++;
		}

		// Check for cipher time out
		if (status.cipher_hdcp_status
		    || status.cipher_encrypting_status) {
			ret = 0;
			return ret;
		}
		// clear the repeater specfic bits and set the repeater to idle
		hdcp_repeater.value = REG_READ(MDFLD_HDCP_REP_REG);
		hdcp_repeater.repeater_present = 0;
		hdcp_repeater.repeater_control = HDCP_REPEATER_CTRL_IDLE;
		REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_repeater.value);

		max_retry = HDCP_MAX_RETRY_STATUS;	//tbd: not yet finalized
		while (max_retry--) {
			hdcp_repeater.value = REG_READ(MDFLD_HDCP_REP_REG);

			if (hdcp_repeater.repeater_status ==
			    HDCP_REPEATER_STATUS_IDLE) {
				ret = 1;
				break;
			}
		}

		// Check for cipher time out
		if (max_retry == 0) {
			ret = 0;
			return 0;
		}
		// Clear the Ri' register
		// This is a HW issue because of which the Ri' status bit in HDCP_STATUS
		// register doesn't get cleared. 
		// refer ro https://vthsd.fm.intel.com/hsd/cantiga/sighting/default.aspx?sighting_id=304464
		// for details
		REG_WRITE(MDFLD_HDCP_RECEIVER_RI_REG, 0);

		//Disable the port on which HDCP is enabled
		REG_WRITE(hdmi_priv->hdmib_reg,
			  REG_READ(hdmi_priv->hdmib_reg) & ~HDMIB_HDCP_PORT);
	} else {
		//Generate An
		config.value = REG_READ(MDFLD_HDCP_CONFIG_REG);

		if (config.hdcp_config != HDCP_Off) {
			config.hdcp_config = HDCP_Off;
			REG_WRITE(MDFLD_HDCP_CONFIG_REG, config.value);
		}

		/*
		 * When generating An, hardware will use the two most recent values as a
		 * 64-bit source of entropy.
		 */
		get_random_bytes(&hdcp_init_vec, sizeof(hdcp_init_vec));
		REG_WRITE(MDFLD_HDCP_INIT_REG, hdcp_init_vec);
		get_random_bytes(&hdcp_init_vec, sizeof(hdcp_init_vec));
		REG_WRITE(MDFLD_HDCP_INIT_REG, hdcp_init_vec);
		udelay(10);

		config.hdcp_config = HDCP_CAPTURE_AN;
		REG_WRITE(MDFLD_HDCP_CONFIG_REG, config.value);

		//check the status of cipher before reading an
		max_retry = HDCP_MAX_RETRY_STATUS;	//tbd: not yet finalized
		while (max_retry--) {
			status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
			if (status.cipher_an_status) {
				ret = 1;
				break;
			}
		}

		config.hdcp_config = HDCP_Off;
		REG_WRITE(MDFLD_HDCP_CONFIG_REG, config.value);

		if (max_retry == 0)
			return 0;	//Cipher timeout, was not able to generate An :(

		//Read An
		hw_an.u.low_part = REG_READ(MDFLD_HDCP_AN_LOW_REG);
		hw_an.u.high_part = REG_READ(MDFLD_HDCP_AN_HI_REG);

		hw_aksv.u.low_part = REG_READ(MDFLD_HDCP_AKSV_LOW_REG);
		hw_aksv.u.high_part = REG_READ(MDFLD_HDCP_AKSV_HI_REG);
		//stHdcpParams.hwAksv.MajorPart_Low = 0x0361f714;//test data
		//stHdcpParams.hwAksv.MajorPart_High = 0xb7;

		//write An
		ret = write_hdcp_port(RX_AN_0, hw_an.byte, 8);
		if (!ret)
			return 0;

		//write Aksv
		ret = write_hdcp_port(RX_AKSV_0, hw_aksv.byte, 5);
		if (!ret)
			return 0;

		//Read the Bksv from receiver
		ret = read_hdcp_port(RX_TYPE_BKSV_DATA, &hw_bksv.byte[0], 5);
		if (ret) {
			// Validate BKSV
			ret = hdcp_is_valid_bksv(&hw_bksv.byte[0], 5);
		}

		if (!ret)
			return 0;

		//read the BCaps 
		ret = read_hdcp_port(RX_TYPE_BCAPS, &bcaps, 1);
		if (!ret)
			return 0;

		// set repeater bit if receiver connected is a repeater
		if (bcaps & BIT6) {
			hdcp_update_repeater_state(1);
		}
		//Write the BKsv into the encoder
		REG_WRITE(MDFLD_HDCP_BKSV_LOW_REG, hw_bksv.u.low_part);
		REG_WRITE(MDFLD_HDCP_BKSV_HI_REG, hw_bksv.u.high_part);

		//enable HDCP on this port
		REG_WRITE(hdmi_priv->hdmib_reg,
			  REG_READ(hdmi_priv->hdmib_reg) | HDMIB_HDCP_PORT);

		//TBD :Check the bStatus, for repeater and set HDCP_REP[1]
		//Set HDCP_CONFIG to 011 = Authenticate and encrypt
		config.hdcp_config = HDCP_AUTHENTICATE_AND_ENCRYPT;
		REG_WRITE(MDFLD_HDCP_CONFIG_REG, config.value);

		//At this point of time the Km is created

		//Wait for Ri ready
		max_retry = HDCP_MAX_RETRY_STATUS;	//TBD: Not yet finalized
		while (max_retry--) {
			status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
			if (status.cipher_ri_ready_status)
				break;
		}

		if (max_retry == 0)
			return 0;	//Cipher timeout, was not able to generate An :(

		//Compare the R0 and Ri
		//Read the Ri' of reciever
		ret = read_hdcp_port(RX_TYPE_RI_DATA, (uint8_t *) & rx_ri, 2);
		if (!ret)
			return 0;

		//TBD:Have some delay before reading the Ri'
		//Right now using 100 ms, as per the HDCP spec(Refer HDCP SAS for details)
		mdelay(HDCP_100MS_DELAY);

		//update the HDCP_Ri' register and read the status reg for confrmation
		receivers_ri.value = REG_READ(MDFLD_HDCP_RECEIVER_RI_REG);
		receivers_ri.ri = rx_ri;
		REG_WRITE(MDFLD_HDCP_RECEIVER_RI_REG, receivers_ri.value);

		status.value = REG_READ(MDFLD_HDCP_STATUS_REG);

		//SoftbiosDebugMessage(DBG_CRITICAL,"R Prime       = %x\n",dwRxRi);
		//SoftbiosDebugMessage(DBG_CRITICAL,"HDCP_STATUS = %x\n",stStatus.value);
		ret = status.cipher_ri_match_status;
		/*if(GEN4INTHDCPCONTROLLER_HasInterruptOccured(pThis,ePort) == TRUE)
		   {
		   bRet = 0;
		   } */
	}

	if (!ret) {
		//TODO: SoftbiosDebugMessage(DBG_CRITICAL," EnableHDCP failed \n");
	}

	return ret;
}

/*
To obtain receiver specific data. The request type
is from mdfld_hdcp_rx_data_type_en 
*/
static int hdcp_get_receiver_data(uint8_t * buffer, uint32_t size,
				  uint32_t rx_data_type)
{
	int ret = 0;

	if (buffer) {
		memset(buffer, 0, size);
		//Get the Data from reciever
		ret = read_hdcp_port(rx_data_type, buffer, size);
	}
	// Validate BKSV and Check if its Valid.
	if (RX_TYPE_BKSV_DATA == rx_data_type) {
		ret = hdcp_is_valid_bksv(buffer, size);
	}

	return ret;
}

/*
 *
 * WaitForNextDataReady : Function Waits for enryption ready
 *
 */
static int hdcp_wait_for_next_data_ready(void)
{
	struct drm_device *dev = hdmi_priv->dev;
	mdfld_hdcp_rep_t hdcp_rep_reg;
	uint32_t i = 0;
	int ret = 0;

	for (i = 0; i < HDCP_MAX_RETRY_STATUS; i++) {
		hdcp_rep_reg.value = REG_READ(MDFLD_HDCP_REP_REG);

		if (HDCP_REPEATER_STATUS_RDY_NEXT_DATA ==
		    hdcp_rep_reg.repeater_status) {
			ret = 1;
			break;
		}
	}

	return ret;
}

/*
 *
 * CompareVPrime : This routine compares the vprime
 * obtained from receiver with the one generated in
 * transmitter.
 *
 */
static int hdcp_compare_v_prime(uint32_t * buffer_repeater_v_prime,
				uint8_t size_in_dword)
{
	struct drm_device *dev = hdmi_priv->dev;
	uint32_t value = 0;
	uint8_t *buffer = (uint8_t *) buffer_repeater_v_prime;
	int ret = 0;
	mdfld_hdcp_rep_t hdcp_rep_ctrl_reg;
	uint32_t i = 0;

	//TBD: To be implemented as part of repeater implementation
	//Set the repeater's vprime in GMCH

	if (size_in_dword == KSV_SIZE) {
		memcpy(&value, buffer, 4);
		REG_WRITE(MDFLD_HDCP_VPRIME_H0, value);

		buffer += 4;
		memcpy(&value, buffer, 4);
		REG_WRITE(MDFLD_HDCP_VPRIME_H1, value);

		buffer += 4;
		memcpy(&value, buffer, 4);
		REG_WRITE(MDFLD_HDCP_VPRIME_H2, value);

		buffer += 4;
		memcpy(&value, buffer, 4);
		REG_WRITE(MDFLD_HDCP_VPRIME_H3, value);

		buffer += 4;
		memcpy(&value, buffer, 4);
		REG_WRITE(MDFLD_HDCP_VPRIME_H4, value);

		if (!hdcp_wait_for_next_data_ready())
			return 0;

		// Set HDCP_REP to do the comparison
		// Start transmitter's V calculation
		hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
		hdcp_rep_ctrl_reg.repeater_control =
		    HDCP_REPEATER_COMPLETE_SHA1;
		REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);

		for (i = 0; i < HDCP_MAX_RETRY_STATUS; i++) {
			hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);

			switch (hdcp_rep_ctrl_reg.repeater_status) {
			case HDCP_REPEATER_STATUS_COMPLETE_MATCH:
				ret = 1;
				break;
			case HDCP_REPEATER_STATUS_COMPLETE_NO_MATCH:
				ret = 0;
				break;
			case HDCP_REPEATER_STATUS_IDLE:
				//Should not happen
				ret = 0;
				break;
				//default: Not needed
			}

			if (hdcp_rep_ctrl_reg.repeater_status !=
			    HDCP_REPEATER_STATUS_BUSY) {
				break;
			}
		}

	} else {
		ret = 0;
	}

	return ret;
}

/* 
 *
 * ComputeTransmitterV : This routine computes transmitter's V prime.
 * As per HDCP spec 1.3 for HDMI/DVI the BStatus register contains data specific to repeater 
 * sink topology in case sink is a repeater. The same interface is used by HDCP over display port
 * in which case BInfo contains the relevant data. The variable wBTopologyData represents Bstatus
 * for HDMI/DVI and BInfo for Display Port
 *
 */
static int hdcp_compute_transmitter_v(ksv_t * ksv_list,
				      uint32_t ksv_list_entries,
				      uint16_t b_topology_data)
{
	struct drm_device *dev = hdmi_priv->dev;
	uint32_t num_devices = ksv_list_entries;
//    uint32_t lower_num_bytes_for_sha = 0, upper_num_bytes_for_sha = 0; // This has to be in mutiple of 512 bits
	uint32_t lower_num_bytes_for_sha = 0;
	uint32_t num_pad_bytes = 0;
	uint8_t *buffer = NULL;
	uint8_t *temp_buffer = NULL;
	mdfld_hdcp_rep_t hdcp_rep_ctrl_reg;
	uint32_t value = 0;
	int ret = 1;
	uint32_t i = 0;
	uint32_t rem_text_data = 0, num_mo_bytes_left = 8;
	uint8_t *temp_data_ptr = NULL;
	sqword_tt buffer_len;
	uint32_t temp_data = 0;

	//Clear SHA hash generator for new V calculation and set the repeater to idle state
	REG_WRITE(MDFLD_HDCP_SHA1_IN, 0);
	hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
	hdcp_rep_ctrl_reg.repeater_control = HDCP_REPEATER_CTRL_IDLE;
	REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
	for (i = 0; i < HDCP_MAX_RETRY_STATUS; i++) {
		hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
		if (HDCP_REPEATER_CTRL_IDLE ==
		    hdcp_rep_ctrl_reg.repeater_status) {
			ret = 1;
			break;
		}
	}
	if (i == HDCP_MAX_RETRY_STATUS) {
		return 0;
	}
	// Start the SHA buffer creation
	//To find the number of pad bytes
	num_pad_bytes = (64 - (ksv_list_entries * KSV_SIZE + 18) % 64);

	// Get the number of bytes for SHA
	lower_num_bytes_for_sha = KSV_SIZE * num_devices + 18 + num_pad_bytes;	//multiple of 64 bytes

	buffer = (uint8_t *) kzalloc(lower_num_bytes_for_sha, GFP_KERNEL);

	if (buffer) {
		//1. Copy the KSV buffer
		//Note:The data is in little endian format
		temp_buffer = buffer;
		memcpy((void *)temp_buffer, (void *)ksv_list,
		       num_devices * KSV_SIZE);
		temp_buffer += num_devices * KSV_SIZE;

		//2. Copy the b_topology_data
		memcpy((void *)temp_buffer, (void *)&b_topology_data, 2);
		//The bstatus is copied in little endian format
		temp_buffer += 2;

		//3. Offset the pointer buffer by 8 bytes
		// These 8 bytes are zeroed and are place holdes for Mo
		temp_buffer += 8;

		//4. Pad the buffer with extra bytes
		// No need to pad the begining of padding bytes by adding
		// 0x80. HW automatically appends the same while creating 
		// the buffer.
		//*temp_buffer = (BYTE)0x80;
		//temp_buffer++;
		for (i = 0; i < num_pad_bytes; i++) {
			*temp_buffer = (uint8_t) 0x00;
			temp_buffer++;
		}

		//5. Construct the length byte
		buffer_len.quad_part =
		    (unsigned long long)(ksv_list_entries * KSV_SIZE + 2 +
					 8) * 8;
		temp_data_ptr = (uint8_t *) & buffer_len.quad_part;
		// Store it in big endian form
		for (i = 1; i <= 8; i++) {
			*temp_buffer = *(temp_data_ptr + 8 - i);
			temp_buffer++;
		}

		//5.Write a random 64 bit value to the buffer
		//memcpy(temp_buffer,&upper_num_bytes_for_sha,4);
		//temp_buffer += 4;
		//memcpy(temp_buffer,&lower_num_bytes_for_sha,4);

		//Now write the data into the SHA
		temp_buffer = buffer;
		for (i = 0; i < (KSV_SIZE * num_devices + 2) / 4; i++) {
			hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
			hdcp_rep_ctrl_reg.repeater_control =
			    HDCP_REPEATER_32BIT_TEXT_IP;
			REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);

			//As per HDCP spec sample SHA is in little endian format. But the
			//data fed to the cipher needs to be in big endian format for it
			//to compute it correctly
			memcpy(&value, temp_buffer, 4);
			value = HDCP_CONVERT_BIG_ENDIAN(value);
			REG_WRITE(MDFLD_HDCP_SHA1_IN, value);
			temp_buffer += 4;

			if (!hdcp_wait_for_next_data_ready())
				return 0;
		}

		//Write the remaining text data with M0 
		//BUN#: 07ww44#1: text input must be aligned to LSB of the SHA1 
		//in register when inputting partial text and partial M0
		rem_text_data = (KSV_SIZE * num_devices + 2) % 4;
		if (rem_text_data) {
			// Update the no of Mo bytes
			num_mo_bytes_left =
			    num_mo_bytes_left - (4 - rem_text_data);

			if (!hdcp_wait_for_next_data_ready())
				return 0;

			hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
			switch (rem_text_data) {
			case 1:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP;
				break;
			case 2:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP;
				break;
			case 3:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP;
				break;
			default:
				ret = 0;
			}

			if (!ret)
				return ret;

			REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
			memcpy(&value, temp_buffer, 4);

			// swap the text data in big endian format leaving the Mo data as it is.
			// As per the bun the LSB should contain the data in big endian format.
			// since the M0 specfic data is all zeros while it's fed to the cipher.
			// Those bit don't need to be modified
			temp_data = 0;
			for (i = 0; i < rem_text_data; i++) {
				temp_data |=
				    ((value & 0xff << (i * 8)) >> (i * 8)) <<
				    ((rem_text_data - i - 1) * 8);
			}
			REG_WRITE(MDFLD_HDCP_SHA1_IN, temp_data);
			temp_buffer += 4;
		}
		//Write 4 bytes of Mo
		if (!hdcp_wait_for_next_data_ready())
			return 0;

		hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
		hdcp_rep_ctrl_reg.repeater_control = HDCP_REPEATER_32BIT_MO_IP;
		REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
		memcpy(&value, temp_buffer, 4);
		REG_WRITE(MDFLD_HDCP_SHA1_IN, value);
		temp_buffer += 4;
		num_mo_bytes_left -= 4;

		if (num_mo_bytes_left) {
			// The remaining Mo + padding bytes need to be added 
			num_pad_bytes = num_pad_bytes - (4 - num_mo_bytes_left);

			//Write 4 bytes of Mo
			if (!hdcp_wait_for_next_data_ready())
				return 0;

			hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
			switch (num_mo_bytes_left) {
			case 1:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP;
				break;
			case 2:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP;
				break;
			case 3:
				hdcp_rep_ctrl_reg.repeater_control =
				    HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP;
				break;
			default:
				// should never happen
				ret = 0;
			}

			if (!ret)
				return ret;

			REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
			memcpy(&value, temp_buffer, 4);
			//BUN#:07ww44#1
			temp_data = 0;
			for (i = 0; i < rem_text_data; i++) {
				temp_data |=
				    ((value & 0xff << (i * 8)) >> (i * 8)) <<
				    ((rem_text_data - i - 1) * 8);
			}
			REG_WRITE(MDFLD_HDCP_SHA1_IN, value);
			temp_buffer += 4;
			num_mo_bytes_left = 0;
		}
		//Write the remaining no of bytes
		// Remaining data = remaining padding data + 64 bits of length data
		rem_text_data = num_pad_bytes + 8;

		if (rem_text_data % 4) {
			//Should not happen
			return 0;
		}

		for (i = 0; i < rem_text_data / 4; i++) {
			REG_WRITE(MDFLD_HDCP_SHA1_IN, temp_data);
			if (!hdcp_wait_for_next_data_ready())
				return 0;

			hdcp_rep_ctrl_reg.value = REG_READ(MDFLD_HDCP_REP_REG);
			hdcp_rep_ctrl_reg.repeater_control =
			    HDCP_REPEATER_32BIT_TEXT_IP;
			REG_WRITE(MDFLD_HDCP_REP_REG, hdcp_rep_ctrl_reg.value);
			memcpy(&value, temp_buffer, 4);
			// do the Big endian conversion
			value = HDCP_CONVERT_BIG_ENDIAN(value);
			REG_WRITE(MDFLD_HDCP_SHA1_IN, value);
			temp_buffer += 4;
		}
		kfree(buffer);

		ret = 1;
	} else {
		return 0;
	}

	return ret;
}

/*
 *
 * SetHDCPEncryptionLevel:
 *
 */
static uint32_t hdcp_set_encryption_level(cp_parameters_t * cp)
{
	uint32_t ret = STATUS_UNSUCCESSFUL;
	int hdcp_enabled = 0;
	uint32_t ksv_length = 0;
	ksv_t bksv;

	//Get the hdcp configuration of the port
	if (hdmi_priv->is_hdcp_supported) {
		hdcp_enabled = hdcp_is_enabled();
		if (((cp->level == CP_PROTECTION_LEVEL_HDCP_OFF)
		     && (!hdcp_enabled))
		    || ((cp->level == CP_PROTECTION_LEVEL_HDCP_ON)
			&& hdcp_enabled)) {
			ret = STATUS_SUCCESS;
		}

		if (ret == STATUS_UNSUCCESSFUL) {
			//Turn off HDCP
			if (hdcp_enable(0))
				ret = STATUS_SUCCESS;

			if ((cp->level != CP_PROTECTION_LEVEL_HDCP_OFF)
			    && (ret == STATUS_SUCCESS)) {
				// Check if a Revoked device is attached
				if (cp->hdcp.ksv_list_length) {
					//Get the current set of BKSV's from the 
					if (hdcp_get_receiver_data
					    ((uint8_t *) bksv.ab_ksv,
					     CP_HDCP_KEY_SELECTION_VECTOR_SIZE,
					     RX_TYPE_BKSV_DATA)) {
						for (ksv_length = 0;
						     ksv_length <
						     cp->hdcp.ksv_list_length;
						     ksv_length++) {
							if (!memcmp
							    (&bksv,
							     &cp->hdcp.ksv_list
							     [ksv_length],
							     CP_HDCP_KEY_SELECTION_VECTOR_SIZE))
							{
								ret =
								    STATUS_REVOKED_HDCP_DEVICE_ATTACHED;
								break;

							}
						}
					}
				}

				if (ret == STATUS_SUCCESS) {
					//Activate the link layer
					ret = hdcp_enable(1);
				}

			}
		}
	}
	return ret;
}

/*
 *
 * GetMaxSupportedAttachedDevices: Returns the
 * max no attached devices supported on repeater 
 *
 */
static uint16_t hdcp_get_max_supported_attached_devices(void)
{
	//currently return 128 as specified by the HDCP spec
	return MAX_HDCP_DEVICES;
}

/*
 *
 * ActivateRepeater: Activates reciver mode
 *
 */
static uint32_t hdcp_activate_repeater(cp_parameters_t * cp)
{
	uint32_t ret = STATUS_UNSUCCESSFUL;
	uint16_t device_count = 0;
	uint16_t get_max_device_supported = 0;
	uint8_t *ksv_list = NULL;	//[MAX_HDCP_DEVICES] = {0};// * KSV_SIZE] = { 0 };
	uint16_t i = 0, j = 0, k = 0;
	uint32_t repeater_prime_v[5];
	hdcp_rx_bcaps_t b_caps;
	hdcp_rx_bstatus_t b_status;
	//TBD: TO be enabled for OPM - Vista

	// Init bcaps
	b_caps.value = 0;
	b_status.value = 0;

	for (i = 0; i < 5; i++)
		repeater_prime_v[i] = 0;

	for (i = 0; i < 1; i++) {
		ksv_list =
		    (uint8_t *) kzalloc(MAX_HDCP_DEVICES * KSV_SIZE,
					GFP_KERNEL);

		if (!ksv_list) {
			ret = STATUS_UNSUCCESSFUL;
			break;
		}
		//get the receiver bcaps 
		hdcp_get_receiver_data(&b_caps.value, 1, RX_TYPE_BCAPS);

		// Check for repeater caps
		if (!(b_caps.is_reapeater)) {
			ret = STATUS_INVALID_PARAMETER;
			break;
		}
		// Check if the KSV FIFO is ready
		if (!(b_caps.ksv_fifo_ready)) {
			// The HDCP repeater is not yet ready to return a KSV list.
			// Per HDCP spec, the repeater has 5 seconds from when KSVs are exchanged
			// in the first part of the authentication protocol (HDCPActivateLink)
			// to be ready to report out downstream KSVs.
			ret = STATUS_PENDING;
			break;
		}
		//Read repeater's Bstatus
		hdcp_get_receiver_data((uint8_t *) & b_status.value, 2,
				       RX_TYPE_BSTATUS);

		// check if max dev limit is exceeded
		if (b_status.max_devs_exceeded) {
			ret = STATUS_INVALID_PARAMETER;
			break;
		}
		// Check for topology error. This happens when
		// more then seven levels of video repeater have been cascaded.
		if (b_status.max_cascade_exceeded) {
			ret = STATUS_INVALID_PARAMETER;
			break;
		}

		device_count = b_status.device_count;
		if (device_count == 0) {
			ret = STATUS_SUCCESS;
			break;
		}

		get_max_device_supported =
		    hdcp_get_max_supported_attached_devices();

		if (device_count > get_max_device_supported) {
			ret = STATUS_INVALID_PARAMETER;
			break;
		}
		// Update the cipher saying sink supports repeater capabilities
		if (!hdcp_update_repeater_state(1)) {
			ret = STATUS_UNSUCCESSFUL;
			break;
		}
		// Read the KSV list from the repeater
		if (!hdcp_get_receiver_data
		    (ksv_list, device_count * KSV_SIZE,
		     RX_TYPE_REPEATER_KSV_LIST)) {
			ret = STATUS_UNSUCCESSFUL;
			break;
		}

		for (j = 0; j < device_count; j++) {
			for (k = 0; k < cp->hdcp.ksv_list_length; k++) {
				if (0 ==
				    memcmp(&ksv_list[j * KSV_SIZE],
					   &cp->hdcp.ksv_list[k],
					   CP_HDCP_KEY_SELECTION_VECTOR_SIZE)) {
					ret =
					    STATUS_REVOKED_HDCP_DEVICE_ATTACHED;
					break;
				}
			}
		}

		if (!hdcp_compute_transmitter_v
		    ((ksv_t *) ksv_list, device_count, b_status.value)) {
			ret = STATUS_UNSUCCESSFUL;
			break;
		}
		//Get the HDCP receiver's V' value (20 bytes in size)
		if (!hdcp_get_receiver_data
		    ((uint8_t *) repeater_prime_v, KSV_SIZE * 4,
		     RX_TYPE_REPEATER_PRIME_V)) {
			ret = STATUS_UNSUCCESSFUL;
			break;
		}

		if (!hdcp_compare_v_prime(repeater_prime_v, KSV_SIZE)) {
			//set hdcp encryption level to 0
			hdcp_update_repeater_state(0);
			hdcp_enable(0);
			ret = STATUS_UNSUCCESSFUL;
		} else {
			ret = STATUS_SUCCESS;
		}
	}

	if (ksv_list) {
		kfree(ksv_list);
		ksv_list = NULL;
	}

	return ret;
}

/*
 * IsHDCPRepeater : Reads the caps register and informs
 * whether the received is a repeater
 *
 */
static int hdcp_is_repeater(int *is_repeater)
{
	int ret = 0;
	hdcp_rx_bcaps_t b_caps;

	//Init
	b_caps.value = 0;

	ret = hdcp_get_receiver_data(&b_caps.value, 1, RX_TYPE_BCAPS);
	if (ret) {
		*is_repeater = b_caps.is_reapeater;
	}

	return ret;
}

/* Get's the current link status */
static int hdcp_get_link_status(void)
{
	struct drm_device *dev = hdmi_priv->dev;
	int ret = 0;
	uint32_t rx_ri = 0;
	mdfld_hdcp_receiver_ri_t receivers_ri;
	mdfld_hdcp_status_t status;
	uint32_t max_count = 0;

	max_count = HDCP_MAX_RI_QUERY_COUNT;
	while (max_count) {
		max_count--;

		//Read the Ri' of reciever
		ret = read_hdcp_port(RX_TYPE_RI_DATA, (uint8_t *) & rx_ri, 2);
		if (!ret)
			break;	// I2C access failed

		//update the HDCP_Ri' register and read the status reg for cofrmation
		receivers_ri.value = REG_READ(MDFLD_HDCP_RECEIVER_RI_REG);
		receivers_ri.ri = rx_ri;
		REG_WRITE(MDFLD_HDCP_RECEIVER_RI_REG, receivers_ri.value);

		status.value = REG_READ(MDFLD_HDCP_STATUS_REG);

		ret = status.cipher_ri_match_status;
		if (ret) {
			//Ri and Ri' matches, hence reciver is a authentic one :)
			break;
		} else {
			//The Ri changes every 128th frame.Hence if the Ri check fails
			//that means the sink has updated the Ri value and that can happen
			//every 128th frame. In that case we wait for the next frame count.
			//Wait should be around 16 ms.
			while ((status.frame_count & HDCP_NEXT_RI_FRAME) ==
			       HDCP_NEXT_RI_FRAME) {
				status.value = REG_READ(MDFLD_HDCP_STATUS_REG);
			}
		}
	}

	return ret;
}

/*
 *
 * GetHDCPEncryptionLevel: 
 *
 */
static void hdcp_get_encryption_level(cp_parameters_t * cp)
{

	if (hdcp_is_enabled()) {
		cp->level = CP_PROTECTION_LEVEL_HDCP_ON;
	} else {
		cp->level = CP_PROTECTION_LEVEL_HDCP_OFF;
	}

	return;
}

/*
 *
 * GetCPData: Get Content protection Data
 * based upon the request from CP
 *
 */
uint32_t hdcp_get_cp_data(cp_parameters_t * cp)
{
	uint32_t ret = STATUS_SUCCESS;
	int is_repeater = 0;

	if ((cp->protect_type_mask & CP_PROTECTION_TYPE_HDCP)) {
		//Check whether HDCP is on
		hdcp_get_encryption_level(cp);

		if (cp->level != CP_PROTECTION_LEVEL_HDCP_OFF) {
			// see if the link is valid, do it by authenticating
			if (!hdcp_get_link_status()) {
				// Encryption setting failed; swtich off the encryption 
				cp->level = CP_PROTECTION_LEVEL_HDCP_OFF;
				hdcp_set_encryption_level(cp);
				ret = STATUS_UNSUCCESSFUL;
			}
		}
		//else
		//{
		//HDCP is off
		//}
#if 0				//Don't need this for client
		//Get the BKSv and repeater status. This has to be returned irrespective of
		//HDCP is ON or Not
		if (!hdcp_get_receiver_data
		    ((uint8_t *) (cp->hdcp.bksv.ab_ksv),
		     CP_HDCP_KEY_SELECTION_VECTOR_SIZE, RX_TYPE_BKSV_DATA)) {
			cp->hdcp.is_repeater = 0;
			memset(&(cp->hdcp.bksv), 0,
			       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
			ret = STATUS_DATA_ERROR;
		} else {
			// This is via opregion. This will return all zeros in production mode
			// Get the AKSV
			if (hdcp_get_aksv(&aksv)) {
				memcpy(cp->hdcp.aksv.ab_ksv, aksv.uc_aksv,
				       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
			} else	// if failed return all zeros
			{
				memset(&cp->hdcp.aksv, 0,
				       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
			}
		}
#endif
		if (ret != STATUS_DATA_ERROR) {
			if (hdcp_is_repeater(&is_repeater)) {
				cp->hdcp.is_repeater = is_repeater;
			} else {
				cp->hdcp.is_repeater = 0;
				ret = STATUS_DATA_ERROR;
			}
		}
	}
#if 0				/* support repeater later */
	else if (cp->protect_type_mask == CP_PROTECTION_TYPE_NONE)	// report repeater capability+BKSV for this mask
	{
		if (!hdcp_get_receiver_data
		    ((uint8_t *) (cp->hdcp.bksv.ab_ksv),
		     CP_HDCP_KEY_SELECTION_VECTOR_SIZE, RX_TYPE_BKSV_DATA)) {
			cp->hdcp.is_repeater = 0;
			memset(&(cp->hdcp.bksv), 0,
			       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
		} else if (hdcp_is_repeater(&is_repeater)) {
			cp->hdcp.is_repeater = is_repeater;
		} else {
			cp->hdcp.is_repeater = 0;
		}

		// Get the AKSV
		if (hdcp_get_aksv(&aksv)) {
			memcpy(cp->hdcp.aksv.ab_ksv, aksv.uc_aksv,
			       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
		} else		// if failed return all zeros
		{
			memset(&cp->hdcp.aksv, 0,
			       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
		}
	}
#endif
	else			//Invalid mask
	{
		//assert(0);

		cp->hdcp.is_repeater = 0;
		//memset(&(cp->hdcp.bksv), 0, CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
	}

	//Note this data needs to be sent irrespective of any unsupported mask
	if (ret == STATUS_SUCCESS) {
		cp->protect_type_mask |= CP_PROTECTION_TYPE_HDCP;
	}

	return ret;
}

/*
 *
 * SetCPData: Enables/Disables Content protection
 * based upon the request from CP
 *
 */
uint32_t hdcp_set_cp_data(cp_parameters_t * cp)
{
	uint32_t ret = STATUS_UNSUCCESSFUL;
	int is_repeater = 0;

	if (cp->protect_type_mask & CP_PROTECTION_TYPE_HDCP) {
		// Get Receiver's repeater status
		// Note:- Reporting back Repeater status in SetCP Data.
		// This is because the analyzer for CTS, acts as repeater only 
		// when the test is running, so notifying this back to opm in 
		// SetProtectionLevel.
		if (hdcp_is_repeater(&is_repeater)) {
			cp->hdcp.is_repeater = is_repeater;
		}
		// Second step flag is if Repeater support needs to be enabled
		if (cp->hdcp.perform_second_step) {
			ret = hdcp_activate_repeater(cp);
			if ((ret != STATUS_SUCCESS) && (ret != STATUS_PENDING)) {
				// Encryption setting failed; switch off the encryption 
				cp->level = CP_PROTECTION_LEVEL_HDCP_OFF;
				hdcp_set_encryption_level(cp);
			}
		} else {
			ret = hdcp_set_encryption_level(cp);

			if (ret != STATUS_SUCCESS) {
				// Encryption setting failed; swtich off the encryption 
				cp->level = CP_PROTECTION_LEVEL_HDCP_OFF;
				hdcp_set_encryption_level(cp);
			}
		}

		if (ret == STATUS_SUCCESS) {
#if 0				//Do need this for client
			// read the bksv
			if (!hdcp_get_receiver_data
			    ((uint8_t *) (cp->hdcp.bksv.ab_ksv),
			     CP_HDCP_KEY_SELECTION_VECTOR_SIZE,
			     RX_TYPE_BKSV_DATA)) {
				cp->hdcp.is_repeater = 0;
				memset(&(cp->hdcp.bksv), 0,
				       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
			} else {
				// read aksv
				if (hdcp_get_aksv(&aksv)) {
					memcpy(cp->hdcp.aksv.ab_ksv,
					       aksv.uc_aksv,
					       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
				} else	// if failed return all zeros
				{
					memset(&cp->hdcp.aksv, 0,
					       CP_HDCP_KEY_SELECTION_VECTOR_SIZE);
				}
			}
#endif
			cp->protect_type_mask |= CP_PROTECTION_TYPE_HDCP;
		}
	} else {
		// No other calls are handled
		return STATUS_SUCCESS;
	}

	return ret;
}
