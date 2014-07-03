/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#include <linux/types.h>
#include "hdcp_rx_defs.h"
#include "ips_hdcp_api.h"
#include "ipil_hdcp_api.h"

/**
 * Description: check whether hdcp hardware is ready
 *
 * Returns:	true if ready else false
 */
bool ipil_hdcp_is_ready(void)
{
	return ips_hdcp_is_ready();
}

/**
 * Description: read an from hdcp tx
 *
 * @an		buffer to return an
 * @size	size of an buffer passed
 *
 * Returns:	true on succesful read else false
 */
bool ipil_hdcp_get_an(uint8_t *an, uint32_t size)
{
	bool ret = false;
	if (an != NULL && size == HDCP_AN_SIZE) {
		ips_hdcp_get_an(an);
		ret = true;
	}
	return ret;
}

/**
 * Description: read aksv from hdcp tx
 *
 * @aksv	buffer to return aksv
 * @size	size of an buffer passed
 *
 * Returns:	true on succesful read else false
 */
bool ipil_hdcp_get_aksv(uint8_t *aksv, uint32_t size)
{
	bool ret = false;
	if (aksv != NULL  && size == HDCP_KSV_SIZE) {
		ips_hdcp_get_aksv(aksv);
		ret = true;
	}
	return ret;
}

/**
 * Description: set repeater bit in hdcp tx if downstream is a repeater else
 *		reset the bit
 *
 * @present	indicates whether downstream is repeater or not
 *
 * Returns:	true on succesful write else false
 */
bool ipil_hdcp_set_repeater(bool present)
{
	return ips_hdcp_set_repeater(present);
}

/**
 * Description: set downstream bksv in hdcp tx
 *
 * @bksv	bksv from downstream device
 *
 * Returns:	true on succesful write else false
 */
bool ipil_hdcp_set_bksv(uint8_t *bksv)
{
	return ips_hdcp_set_bksv(bksv);
}

/**
 * Description: start first stage of authentication by writing an aksv
 *
 * Returns:	true on succesfully starting authentication else false
 */
bool ipil_hdcp_start_authentication(void)
{
	return ips_hdcp_start_authentication();
}

/**
 * Description: check if hdcp tx R0 is ready after starting authentication
 *
 * Returns:	true if r0 is ready else false
 */
bool ipil_hdcp_is_r0_ready(void)
{
	return ips_hdcp_is_r0_ready();
}

/**
 * Description: check if hdcp tx & rx ri matches
 *
 * @rx_ri	ri of downstream device
 *
 * Returns:	true if ri matches else false
 */
bool ipil_hdcp_does_ri_match(uint16_t rx_ri)
{
	return ips_hdcp_does_ri_match(rx_ri);
}

/**
 * Description: Enable encryption once r0 matches
 *
 * Returns:	true on enabling encryption else false
 */
bool ipil_hdcp_enable_encryption(void)
{
	return ips_hdcp_enable_encryption();
}

/**
 * Description: compute hdcp tx's v(sha1) for repeater authentication
 *
 * @rep_ksv_list	 ksv list from downstream repeater
 * @rep_ksv_list_entries number of entries in the ksv list
 * @topology_data	 bstatus value
 *
 * Returns:	true on successfully computing v else false
 */
bool ipil_hdcp_compute_tx_v(uint8_t *rep_ksv_list,
				    uint32_t rep_ksv_list_entries,
				    uint16_t topology_data)
{
	return ips_hdcp_compute_tx_v(rep_ksv_list,
				     rep_ksv_list_entries,
				     topology_data);
}

/**
 * Description: compare hdcp tx & hdcp rx sha1 results
 *
 * @rep_prime_v	sha1 value from downstream repeater
 *
 * Returns:	true if same else false
 */
bool ipil_hdcp_compare_v(uint32_t *rep_prime_v)
{
	return ips_hdcp_compare_v(rep_prime_v);
}

/**
 * Description: disable hdcp
 *
 * Returns:	true on successfully disabling hdcp else false
 */
bool ipil_hdcp_disable(void)
{
	return ips_hdcp_disable();
}

/**
 * Description: check whether hdcp tx can authenticate
 *
 * Returns:	true if device can authenticate else false
 */
bool ipil_hdcp_device_can_authenticate(void)
{
	return ips_hdcp_device_can_authenticate();
}

/**
 * Description: initialize hdcp tx for authentication
 *
 * Returns:	true success else false
 */
bool ipil_hdcp_init(void)
{
	return ips_hdcp_init();
}

/**
 * Description: get hardware frame count for cipher Ri update
 *
 * @count   frame count for cipher Ri update
 *
 * Returns: true if successful else false
 */
bool ipil_hdcp_get_ri_frame_count(uint8_t *count)
{
	return ips_hdcp_get_ri_frame_count(count);
}
