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
* without specific prior written permission.
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
 *  @file stmvl53l1_if.h  vl53l1 kernel driver user interface
 *
  * @note to use this header in a user space application it requires
 *  all st bare/ll driver platform wrapper files (for data struct def)
 *  this files (types etc ..) shall be same or compliant with bar driver version
 *  used in the kernel module
 */

#ifndef STMVL53L1_IF_H
#define STMVL53L1_IF_H


#include "vl53l1_def.h"
/**
 * misc device  name
 *
 * for mutli instance all device 2nd and next instance are basic name +"1"+"2"
 * @li stmvl53l1_ranging
 * @li stmvl53l1_ranging1
 * @li stmvl53l1_ranging2
 */

#define VL53L1_MISC_DEV_NAME		"laser"

/**
 * @addtogroup vl53l1_ioctl
 * @{
 */

/**
 * register data use for simple/single ranging data @ref VL53L1_IOCTL_GETDATAS
 *
 * @warning this definition is subject to change !
 */
typedef VL53L1_RangingMeasurementData_t  stmvl531_range_data_t;

/**
 * IOCTL register data structure use in @ref VL53L1_IOCTL_REGISTER
 *
 * for convenience an ease of use full data size capability is exposed here
 * but the @ref stmvl53l1_register_flexi can be used instead
 */
struct stmvl53l1_register {
	uint32_t is_read;	/*!< type of the access 1: read 0: write*/
	uint32_t index;		/*!< register index */
	uint32_t cnt;		/*!< register size shall be 1 to n */
	int32_t status;		/*!< operation status 0 ok else error */

	union reg_data_t {
		uint8_t b;	/*!< single data byte*/
		uint16_t w;	/*!< single data word (16 bits)*/
		uint32_t dw;	/*!< single data dword (32 bits)*/
		uint8_t bytes[256]; /*!< any size byte array
		* @note only effectively used array size is needed and will be
		* set/used another possible register definition is
		* @ref stmvl53l1_register_flexi
		*/
	} data; /*!< data only *@warning device is big endian and
	* no endianess adaptation is performed by @ref VL53L1_IOCTL_REGISTER
	*/
};

/**
 * flexible size data length register access structure
 * for use in @ref VL53L1_IOCTL_REGISTER
 */
struct stmvl53l1_register_flexi {
	uint32_t is_read;	/*!< [in] type of the access 1: read 0: write*/
	uint32_t index;		/*!< [in] register index */
	uint32_t cnt;		/*!< [în] register size shall be 1 to n */
	int32_t status;		/*!< [out] operation status 0 ok else error */
	uint8_t data[];		/*!< [in/out] flexible array size data */
	/*!< data only *@warning device is big endian and
	* no endianess adaptation is performed by @ref VL53L1_IOCTL_REGISTER
	*/
};


/**
 * parameter name in @ref stmvl53l1_parameter when using
 * @ref VL53L1_IOCTL_PARAMETER
 */
typedef enum {
	OFFSET_PAR = 0,
	/*!< OFFSET_PAR  @warning NOT supported yet */
	XTALKRATE_PAR = 1,
	/*!< XTALKRATE_PAR @warning NOT supported yet */
	XTALKENABLE_PAR = 2,
	/*!< XTALKENABLE_PAR @warning NOT supported yet*/
	GPIOFUNC_PAR = 3,
	/*!< GPIOFUNC_PAR @warning NOT supported yet */
	LOWTHRESH_PAR = 4,
	/*!< LOWTHRESH_PAR @warning NOT supported yet */
	HIGHTHRESH_PAR = 5,
	/*!< HIGHTHRESH_PAR @warning NOT supported yet */
	VL53L1_DEVICEMODE_PAR = 6,
	/*!< DEVICEMODE_PAR set ranging mode  \n
	 * valid mode value :
	 * @li 1 @a VL53L1_PRESETMODE_RANGING default ranging
	 * @li 2 @a VL53L1_PRESETMODE_MULTIZONES_SCANNING multiple zone
	 * @li 3 @a VL53L1_PRESETMODE_AUTONOMOUS autonomous mode
	 * @li 4 @a VL53L1_PRESETMODE_LITE_RANGING low mips ranging mode
	 *
	 * @warning mode can only be set while not ranging
	 */

	VL53L1_INTERMEASUREMENT_PAR = 7,
	/*!< INTERMEASUREMENT_PAR  NOT supported yet */

	REFERENCESPADS_PAR = 8,
	/*!< REFERENCESPADS_PAR @warning NOT supported yet */
	REFCALIBRATION_PAR = 9,
	/*!< REFCALIBRATION_PAR @warning NOT supported yet */

	VL53L1_POLLDELAY_PAR = 10,
	/*!< set the polling delay (msec)\n
	 *
	 * @note apply only when operates in polling mode  as no effect
	 * otherwise
	 */
	VL53L1_TIMINGBUDGET_PAR = 11,
	/*!< VL53L1_TIMINGBUDGET_PAR
	* @ref stmvl53l1_parameter.value field is timing budget in micro second
	*
	* @note the value cannot be set while ranging will set ebusy errno,
	* value set is absorbed at next range start @ref VL53L1_IOCTL_INIT
	*/


} stmv53l1_parameter_name_e;


/**
 * parameter structure use in @ref VL53L1_IOCTL_PARAMETER
 */
struct stmvl53l1_parameter {
	uint32_t is_read;	/*!< [in] 1: Get 0: Set*/
	stmv53l1_parameter_name_e name;	/*!< [in] parameter to set/get
	* see @ref stmv53l1_parameter_name_e
	*/

	int32_t value;		/*!< [in/out] value to set /get */
	int32_t value2;		/*!< [in/out] optional 2nd value */
	int32_t status;		/*!< [out] status of the operation */
};


/**
 * roi structure use as  @ref VL53L1_IOCTL_ROI arg
 *
 * see @ref stmvl53l1_roi_full_t for easy to use type variable declaration
 * required
 */
struct stmvl53l1_roi_t {
	int32_t		is_read;
	/*!<  specify roi transfer direction \n
	 * @li 0 to get roi
	 * @li !0 to set roi
	*/
	struct roi_cfg_t {
		uint8_t NumberOfRoi;
		/*!< Number of Rois to set/get defined\n
		 *  0 is set can be used to return to device default roi usage
		 *  @warning 0 in get will not return any data in UserRois!
		*/
		VL53L1_UserRoi_t    UserRois[1];
		/*!< roi data array length  definition is 1 but
		 * NumberOfRoi+ FirstRoiToScan in array are required
		 * and will be effectively copy to/from user space
		 */
	} roi_cfg;
};

/**
 * full roi struct use in @ref VL53L1_IOCTL_ROI arg
 *
 * @sa stmvl53l1_roi_t for detail this one
 * make it easier to declare a fully static variable with the max roi storage
 * capabilities.
 */
struct stmvl53l1_roi_full_t {
	int32_t		is_read;
	VL53L1_RoiConfig_t roi_cfg;
};


/*
 * IOCTL definitions
 */


/**
 * Start ranging (no argument)
 *
 * @note  sysfs and ioctl control are assumed mutual exclusive use
 * control from ioctl execute with no consideration of sysfs path.
 *
 * @return :
 *	@li 0 on success
 *	@li -EBUSY if already started
 *	@li other any possible over "st bare driver error code"
 *	    (shall be positive)
 *
 * example user land  :
 @code
 int smtvl53l1_start(int fd){error
	int rc;
	rc= ioctl(fd, VL53L1_IOCTL_INIT,NULL);
	if( rc ){
		if( errno == EBUSY){
			//the device is already started
			ioctl_warn("already started");
			return EBUSY;
		}
	}
	if( rc ){
		ioctl_error("%d %s", rc,strerror(errno));
	}
	return rc;
}
 @endcode
*/

#define VL53L1_IOCTL_INIT			_IO('p', 0x01)

/**
 * stop ranging (no argument)

 * @note  sysfs and ioctl control are assumed mutual exclusive use
 * control from ioctl execute action with no consideration of sysfs path.
 *
 * @return
 * @li 0 on success
 * @li -EBUSY if it was already
 * @li other specific error code shall be >0
 *
 * c example userland :
 @code
int smtvl53l1_stop(int fd){
	int rc;
	rc= ioctl(fd, VL53L1_IOCTL_STOP,NULL);
	if( rc ){
		if( errno == EBUSY ){
			ioctl_warn("already stopped");
			return errno;
		}
		ioctl_error("%d %s", rc,strerror(errno));
	}
	return rc;
}
@endcode
 */
 #define VL53L1_IOCTL_STOP			_IO('p', 0x05)

/**
 * set the offset calibration
 *  ioctl arg [IN] one byte offset
 *  @param offset type int8_t is the offset in ??  to be set
 */
#define VL53L1_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)

/**
 * get single ranging data @sa for multi zone/objet
 *
 * retrieve the last range data available form the device
 *
 * @param in/out data struct ptr of type @ref stmvl531_range_data_t
 * it may come in but is out as of now
 *
 * this function succeed whatever device state (off, stopped )
 *
 * @warning this ioctl will not wait for a new range sample acquisition
 * it will return what available at time it get called . Hence same data maybe
 * returned many time when doing fast polling.\n
 * End user must inspect the data structure (time stamp etc )to find about it\n
 * Despite it's non "waiting" nature this ioctl may still block/sleep shortly
 * to ensure race free usage acquiring mutex and/or locks.
 */
#define VL53L1_IOCTL_GETDATAS \
			_IOWR('p', 0x0b, stmvl531_range_data_t)

/**
 * low level register access IOCTL
 * ioctl arg [IN]/[OUT] pointer to register struct @ref stmvl53l1_register
 * or @ref stmvl53l1_register_flexi
 *
 * ioctl arg [out] set back with status and data when applicable
 */
#define VL53L1_IOCTL_REGISTER	_IOWR('p', 0x0c, struct stmvl53l1_register)

/**
 * set or get parameter
 *
 * @param parameter in/out  @ref stmvl53l1_parameter
 * @sa stmv53l1_parameter_name_e
 *
 * for get if ioctl fail do not check for out params it is not valid
 * for set theirs not copy back only see ioctl status, errno to get error case
 *
 * @note a set parameter may not be absorbed straight aways !
 */
#define VL53L1_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l1_parameter)


/**
 * set/get roi
 *
 * shall only be use while device is stopped (EBUSY error otherwise)
 * setting 0 rois stand for "disable  user define roi usage, use device default"
 *
 * @param roi_cfg [in/out] type @ref stmvl53l1_roi_t and
 * @ref stmvl53l1_roi_full_t
 * @note when getting roi the returned roi cnt is set to available number
 * of roi in driver but  at most requested number or available one
 * will be set in returned structure
 * @warning the coordinate system is not usual image x,y (y down)but traditional
 * ecludian x,y (y up)
 *
 * @warning once defined the user roi is kept alive until unset by user .
 * User shall update roi when required (mode change etc ..)\n
 * To return to default unset roi by setting none, device will return to default
 * at next start
 *
 * @note roi validity is only checked at start ranging , as such invalid roi set
 * can make start to fail
 *
 * @return 0 on success , see errno for error detail
 *  @li EBUSY when trying to set roi while ranging
 *  @li ENODEV never device get started and trying to get more rois than set
 *  @li other errno code could be ll driver specific
 */
#define VL53L1_IOCTL_ROI\
			_IOWR('p', 0x0e, struct stmvl53l1_roi_t)

/**
 * Get multi object/zone ranging data
 *
 * this call is non blocking and will return what available internally
 * in all case (veen error)
 *
 * @param [out] multi zone range @ref VL53L1_MultiRangingData_t always update
 * but -EFAULT error case
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copyed
 * @li -ENOEXEC active mode is not mutli-zone
 * @li -ENODEV device is not ranging
 * as in that case MZ data may not be fully valid
 */
#define VL53L1_IOCTL_MZ_DATA\
			_IOR('p', 0x0f, VL53L1_MultiRangingData_t)
/** @} */ /* ioctl group */
#endif /* STMVL53L1_IF_H */
