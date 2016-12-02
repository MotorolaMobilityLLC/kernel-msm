/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permissivl53l1_mod_dbgon.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 *  @file stmvl53l1_dox.h - STM VL53L1 doxygen documentation
 *
 *  @warning file use for doxygen doc generation do not use in source !
 */

/**
 * @defgroup vl53l1_config  Static configuration
 * @{
 *
 * see also @a vl53l1_mod_dbg
 */

/**
 * select i2c/cci and h/w control  module interface used in driver
 *
 * CAMERA_CCI defined select msm cci interface MODULE\n
 * when not defined linux native i2c interface is used
 *
 * module interface see @a stmvl53l1_module_fn_t
 */
#define CAMERA_CCI


/** enable poll timing to be logged via dbgmsg
  * default to off when not defined set to 1 to enable
  */
#define STMVL53L1_LOG_POLL_TIMING	0

/** enable or not cci access timing to be logged dbgmsg
 * default to off when not defined set to 1 to enable
 */
#define STMVL53L1_LOG_CCI_TIMING	0

/**
 * configure usage of regulator to enable/disable sensor power
 *
 * @note the regulator information come from device tree
 * see stmvl53l1_module-i2c or stmvl53l1_module-i2c file
 */
/*#define CFG_STMVL53L1_HAVE_REGULATOR*/


/**
 * @defgroup vl53l1_mod_dbg  debugging
 *
 * empty group
 */
/** @} */

/**
 * @defgroup vl53l1_ioctl  IOCTL interface commands
 *
 * Ioctl commands for vl53L1
 */


/**
 *@defgroup sysfs_attrib sysfs attribute

 * stmvl53l1 can be controle wia a set of exposed sysf attribute
 * under sys/class/inputx/attribe_name where x is the input device
 * associated with the vl53l1 (dynamic)
 */

/**@defgroup drv_port vl53l1 interface module porting and customization
 *
 * this i2c + gpio interface module is use  when @ref CAMERA_CCI isn't set
 */

/**@defgroup ipp_dev  IPP information
 *
 * @section ipp_dev_info IPP stand for Ip Protected Processing ST proprietary and patented that cannot
 * be included as kernel GPL code.
 *
 * A couple IPP call form the STbare driver comes to the kernel driver
 * that will pass this to user space daemon.
 *
 * this is done by serializing function arguments and passing them
 * to and from the user space.
 *
 * Serialization is done ipp/ipp_linux.c mainly using macro from stmvl53l1_ipp.h
 * (shared with user space).\n
 * The data is then pass to ipp netlink layer in stmvl53l1_ipp_nl.c
 * @note  mmap/file io and or ioctl could be used to implement a different
 * ipp data channel.
 */


#error "This file must not be included in source used for doc purpose !!!"
