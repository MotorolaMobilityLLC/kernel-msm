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
 * @addtogroup vl53l1_ioctl
 * @{
 */

/**
 * misc device name for ioctl device
 *
 * for mutli instance all device 2nd and next instance are basic name +"1"+"2"
 * @li stmvl53l1_ranging
 * @li stmvl53l1_ranging1
 * @li stmvl53l1_ranging2
 */
#define VL53L1_MISC_DEV_NAME		"laser"
/**
 * register data use for simple/single ranging data @ref VL53L1_IOCTL_GETDATAS
 *
 * @warning this definition is subject to change !
 */
#define stmvl531_range_data_t VL53L1_RangingMeasurementData_t

/**
 * parameter name in @ref stmvl53l1_parameter when using
 * @ref VL53L1_IOCTL_PARAMETER
 */
enum __stmv53l1_parameter_name_e {
	VL53L1_XTALKENABLE_PAR = 2,
	/*!< VL53L1_XTALKENABLE_PAR enable/disable crosstalk compensation\n
	 * valid value :
	 * @li 0 disable crosstalk compensation
	 * @li 1 enable crosstalk compensation
	 *
	 * @warning mode can only be set while not ranging
	 */

	VL53L1_DEVICEMODE_PAR = 6,
	/*!< DEVICEMODE_PAR set ranging mode  \n
	 * valid mode value :
	 * @li 1 @a VL53L1_PRESETMODE_RANGING default ranging
	 * @li 2 @a VL53L1_PRESETMODE_MULTIZONES_SCANNING multiple zone
	 * @li 3 @a VL53L1_PRESETMODE_AUTONOMOUS autonomous mode
	 * @li 4 @a VL53L1_PRESETMODE_LITE_RANGING low mips ranging mode
	 * @li 8 @a VL53L1_PRESETMODE_LOWPOWER_AUTONOMOUS low power autonomous
	 * mode
	 *
	 * @warning mode can only be set while not ranging
	 */

	VL53L1_POLLDELAY_PAR = 10,
	/*!< set the polling delay (msec)\n
	 *
	 * @note apply only when operates in polling mode  as no effect
	 * otherwise
	 */
	VL53L1_TIMINGBUDGET_PAR = 11,
	/*!< VL53L1_TIMINGBUDGET_PAR
	* @ref stmvl53l1_parameter.value field is timing budget in micro second
	*/

	VL53L1_DISTANCEMODE_PAR = 12,
	/*!< VL53L1_DISTANCEMODE_PAR
	 * valid distance mode value :
	 * @li 1 @a VL53L1_DISTANCEMODE_SHORT
	 * @li 2 @a VL53L1_DISTANCEMODE_MEDIUM
	 * @li 3 @a VL53L1_DISTANCEMODE_LONG
	 *
	 * @warning distance mode can only be set while not ranging
	 */

	VL53L1_OUTPUTMODE_PAR = 13,
	/*!< VL53L1_OUTPUTMODE_PAR
	 * valid output mode value :
	 * @li 1 @a VL53L1_OUTPUTMODE_NEAREST
	 * @li 2 @a VL53L1_OUTPUTMODE_STRONGEST
	 *
	 * @warning distance mode can only be set while not ranging
	 */

	VL53L1_FORCEDEVICEONEN_PAR = 14,
	/*!< VL53L1_FORCEDEVICEONEN_PAR
	 * This parameter will control if device is put under reset when
	 * stopped.
	 * valid force device on value :
	 * @li 0 feature is disable. Device is put under reset when stopped.
	 * @li 1 feature is enable. Device is not put under reset when stopped.
	 */

	VL53L1_LASTERROR_PAR = 15,
	/*!< VL53L1_LASTERROR_PAR
	 * This is a read only parameter. It will return last device internal
	 * error. It's valid only after an ioctl/sysfs return an -EIO error.
	 */

	VL53L1_OFFSETCORRECTIONMODE_PAR = 16,
	/*!< VL53L1_OFFSETCORRECTIONMODE_PAR
	 * This parameter will define which mode to use for the offset
	 * correction.
	 * valid force device on value :
	 * @li 1 @a VL53L1_OFFSETCORRECTIONMODE_STANDARD
	 * @li 2 @a VL53L1_OFFSETCORRECTIONMODE_PERZONE
	 *
	 * @warning offset correction mode can only be set while not ranging
	 */

	VL53L1_OPTICALCENTER_PAR = 17,
	/*!< VL53L1_OPTICALCENTER_PAR
	 * This is a read only parameter. It will return optical center issued
	 * from the nvm set at FTM stage. value will contain X position of
	 * center. value2 will contain Y position of center.
	 * Return values have FixPoint1616_t type.
	 */

	VL53L1_DMAXREFLECTANCE_PAR = 18,
	/*!< VL53L1_DMAXREFLECTANCE_PAR
	 * This parameter will define target reflectance @ 940nm used to
	 * calculate the ambient DMAX. Parameter is of type FixPoint1616_t.
	 *
	 * @warning dmax reflectance can only be be set while not ranging
	 */

	VL53L1_DMAXMODE_PAR = 19,
	/*!< VL53L1_DMAXMODE_PAR
	 * This parameter will select Dmax mode.
	 * valid Dmax mode value :
	 * @li 1 @a VL53L1_DMAXMODE_FMT_CAL_DATA
	 * @li 2 @a VL53L1_DMAXMODE_CUSTCAL_DATA
	 * @li 3 @a VL53L1_DMAXMODE_PER_ZONE_CAL_DATA
	 *
	 * @warning Dmax mode can only be set while not ranging
	 */

	VL53L1_TUNING_PAR = 20,
	/*!< VL53L1_DMAXMODE_PAR
	 * This parameter is a write only parameter. It will allow to provide
	 * low level layer with a configuration parameter.
	 * value will be use as a key parameter.
	 * value2 will be use as value parameter.
	 *
	 * @warning those configuration parameter settings are only allowed
	 * before device is start once.
	 */

	VL53L1_SMUDGECORRECTIONMODE_PAR = 21,
	/*!< VL53L1_SMUDGECORRECTIONMODE_PAR
	 * This parameter will control if smudge correction is enable and how
	 * crosstalk values are updated.
	 * @li 0 @a VL53L1_SMUDGE_CORRECTION_NONE
	 * @li 1 @a VL53L1_SMUDGE_CORRECTION_CONTINUOUS
	 * @li 2 @a VL53L1_SMUDGE_CORRECTION_SINGLE
	 * @li 3 @a VL53L1_SMUDGE_CORRECTION_DEBUG
	 */

	VL53L1_ISXTALKVALUECHANGED_PAR = 22,
	/*!< VL53L1_ISXTALKCHANGED_PAR
	 * This is a read only parameter. It will return if Xtalk value has
	 * been updated while ranging. This parameter is reset each time device
	 * start to range.
	 * @li 0 Xtalk values has not been changed.
	 * @li 1 Xtalk values has been changed.
	 */

	VL53L1_CAMERAMODE_PAR = 23,
	/*!< VL53L1_CAMERAMODE_PAR
	* valid camera mode value :
	* @li 0 device is out of camera mode.
	* @li 1 device is in camera mode.
	*/

	VL53L1_HWREV_PAR = 24,
	/*!< VL53L1_HWREV_PAR
	* This is a read only parameter. It will return HW revision number
	*/
};
#define stmv53l1_parameter_name_e enum __stmv53l1_parameter_name_e

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
	/*! roi data and count type use in @ VL53L1_IOCTL_ROI */
	struct roi_cfg_t {
		uint8_t NumberOfRoi;
		/*!< [in/out] Number of Rois to set/get
		 *
		 * on set :\n
		 * [in] number of roi to set
		 * @note 0 set can be used to return to device default roi usage
		 *
		 * on get :\n
		 * [in] max number provided\n
		 * [out] number of ROI  copied back to user\n
		 * @warning 0 will not return any roi datas!
		*/
		VL53L1_UserRoi_t    UserRois[1];
		/*!< roi data array length  definition is 1 but
		 * NumberOfRoi+ FirstRoiToScan in array are required
		 * and will be effectively copy to/from user space
		 *
		 * @sa stmvl53l1_roi_full_t
		 */
	} roi_cfg /*!  [in/out] roi data and count */;
};

/**
 * full roi struct use in @ref VL53L1_IOCTL_ROI arg
 *
 * this definition make easier variable declaration with the max roi storage
 * capabilities.
 *
 * @sa stmvl53l1_roi_t for  field details
 */
struct stmvl53l1_roi_full_t {
	int32_t		is_read;
	/*!<  specify roi transfer direction \n
	 * @li 0 to get roi
	 * @li !0 to set roi
	*/
	VL53L1_RoiConfig_t roi_cfg;
	/*!< roi data array of max length but only requested copy to/from user
	 * space effectively used
	 * see @a stmvl53l1_roi_t::roi_cfg for  details
	 */
};

/**
 * parameter structure use in @ref VL53L1_IOCTL_CALIBRATION_DATA
 */
struct stmvl53l1_ioctl_calibration_data_t {
	int32_t is_read;	/*!< [in] 1: Get 0: Set*/
	VL53L1_CalibrationData_t data;
	/*!< [in/out] data to set /get. Caller
	 * should consider this structure as an opaque one
	 */
};

/**
 * Opaque structure use to hold content of zone offset calibration result.
 */
#define stmvl531_zone_calibration_data_t \
	struct _stmvl531_zone_calibration_data_t

struct _stmvl531_zone_calibration_data_t {
	uint32_t id;
	VL53L1_ZoneCalibrationData_t data;
};

/**
 * parameter structure use in @ref VL53L1_IOCTL_ZONE_CALIBRATION_DATA
 */
struct stmvl53l1_ioctl_zone_calibration_data_t {
	int32_t is_read;	/*!< [in] 1: Get 0: Set*/
	stmvl531_zone_calibration_data_t data;
	/*!< [in/out] data to set /get. Caller
	 * should consider this structure as an opaque one
	 */
};

/** Select reference spad calibration in @ref VL53L1_IOCTL_PERFORM_CALIBRATION.
 *
 * param1, param2 and param3 not use
 */
#define VL53L1_CALIBRATION_REF_SPAD		0

/** Select crosstalk calibration in @ref VL53L1_IOCTL_PERFORM_CALIBRATION.
 *
 * param1 is calibration method. param2 and param3 not use.
 */
#define VL53L1_CALIBRATION_CROSSTALK		1

/** Select offset calibration  @ref VL53L1_IOCTL_PERFORM_CALIBRATION.
 * param1 is offset calibration mode. Parameter is either:
 *        - VL53L1_OFFSETCALIBRATIONMODE_STANDARD
 *        - VL53L1_OFFSETCALIBRATIONMODE_PRERANGE_ONLY
 *        - VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE (deprecated)
 * param2 is target distance in mm.
 * param3 is target reflectance in percent. Parameter is of type FixPoint1616_t.
 *
 * Note that VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE usage is deprecated. Per
 * zone offset calibration should use VL53L1_CALIBRATION_OFFSET_PER_ZONE
 * instead.
 */
#define VL53L1_CALIBRATION_OFFSET		2

/** Select offset calibration per zone @ref VL53L1_IOCTL_PERFORM_CALIBRATION.
 * param1 is offset calibration mode. Parameter is:
 *        - VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE
 * param2 is target distance in mm.
 * param3 is target reflectance in percent. Parameter is of type FixPoint1616_t.
 *
 * Note that region of interest should be defined by a prior call to
 * VL53L1_IOCTL_ROI before calling VL53L1_IOCTL_PERFORM_CALIBRATION /
 * VL53L1_CALIBRATION_OFFSET combinaison.
 */
#define VL53L1_CALIBRATION_OFFSET_PER_ZONE	3

/** Select simple offset calibration @ref VL53L1_IOCTL_PERFORM_CALIBRATION.
 * param1 is target distance in mm.
 * param2 and param3 are not used
 */
#define VL53L1_CALIBRATION_OFFSET_SIMPLE	4

/**
 * parameter structure use in @ref VL53L1_IOCTL_PERFORM_CALIBRATION
 */
struct stmvl53l1_ioctl_perform_calibration_t {
	uint32_t calibration_type;
	/*!< [in] select which calibration to do :
	 * @li @ref VL53L1_CALIBRATION_REF_SPAD
	 * @li @ref VL53L1_CALIBRATION_CROSSTALK
	 * @li @ref VL53L1_CALIBRATION_OFFSET
	 * @li @ref VL53L1_CALIBRATION_OFFSET_SIMPLE
	 * @li @ref VL53L1_CALIBRATION_OFFSET_PER_ZONE
	 */
	uint32_t param1;
	/*!< [in] first param. Usage depends on calibration_type */
	uint32_t param2;
	/*!< [in] second param. Usage depends on calibration_type */
	uint32_t param3;
	/*!< [in] third param. Usage depends on calibration_type */
};

/**
 * parameter structure use in @ref VL53L1_IOCTL_AUTONOMOUS_CONFIG
 */
struct stmvl53l1_autonomous_config_t {
	int32_t is_read;
	/*!< [in] 1: Get 0: Set*/
	uint32_t pollingTimeInMs;
	/*!< [in/out] interval between two measure in ms */
	VL53L1_DetectionConfig_t config;
	/*!< [int/out] autonomous mode configuration structure */
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
 *	@li -ENXIO failed to change i2c address change after reset release
 *	@li -EIO. Read last_error to get device error code
 *	@li -ENODEV. Device has been removed.
 *
 * example user land  :
 @code
 int smtvl53l1_start(int fd){error
	int rc;
	rc= ioctl(fd, VL53L1_IOCTL_START,NULL);
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

#define VL53L1_IOCTL_START			_IO('p', 0x01)

/**
 * stop ranging (no argument)

 * @note  sysfs and ioctl control are assumed mutual exclusive use
 * control from ioctl execute action with no consideration of sysfs path.
 *
 * @return
 * @li 0 on success
 * @li -EBUSY if it was already
 * @li -EIO. Read last_error to get device error code
 * @li -ENODEV. Device has been removed.
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
 * get single ranging data @sa for multi zone/objet
 *
 * retrieve the last range data available form the device
 *
 * @param in/out data struct ptr of type @ref stmvl531_range_data_t
 * it may come in but is out as of now
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copied
 * @li -ENODEV. Device has been removed.
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
 * set or get parameter
 *
 * @param parameter in/out  @ref stmvl53l1_parameter
 * @sa stmv53l1_parameter_name_e
 *
 * for get if ioctl fail do not check for out params it is not valid
 * for set theirs not copy back only see ioctl status, errno to get error case
 *
 * @return 0 on success else o, error check errno
 * @li -ENODEV. Device has been removed.
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
 * @li -ENODEV device is not ranging or device has been removed.
 * as in that case MZ data may not be fully valid
 */
#define VL53L1_IOCTL_MZ_DATA\
			_IOR('p', 0x0f, VL53L1_MultiRangingData_t)

/**
 * get single ranging data @sa for multi zone/objet
 *
 * this call is equivalent to VL53L1_IOCTL_GETDATAS but will block until
 * new data are available since previous call.
 *
 * @param in/out data struct ptr of type @ref stmvl531_range_data_t
 * it may come in but is out as of now
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copied
 * @li -ENODEV device is not ranging or device has been removed.
 * @li -ERESTARTSYS interrupt while sleeping.
 */
#define VL53L1_IOCTL_GETDATAS_BLOCKING\
			_IOWR('p', 0x10, stmvl531_range_data_t)

/**
 * Get multi object/zone ranging data
 *
 * this call is equivalent to VL53L1_IOCTL_MZ_DATA but will block until
 * new data are available since previous call.
 *
 * @param [out] multi zone range @ref VL53L1_MultiRangingData_t always update
 * but -EFAULT error case
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copyed
 * @li -ENOEXEC active mode is not mutli-zone
 * @li -ENODEV device is not ranging or device has been removed.
 * @li -ERESTARTSYS interrupt while sleeping.
 * as in that case MZ data may not be fully valid
 */
#define VL53L1_IOCTL_MZ_DATA_BLOCKING\
			_IOR('p', 0x11, VL53L1_MultiRangingData_t)

/**
 * Get / set calibration data
 *
 * this call allow client to either read calibration data after calibration
 * has been performed to store them in the host filesystem or push calibration
 * data before ranging at each start-up.
 *
 * @param [in/out] data struct ptr of type
 * @ref stmvl53l1_ioctl_calibration_data_t. Caller should consider it as an
 * opaque structure.
 *
 * use this after either VL53L1_CALIBRATION_REF_SPAD,
 * VL53L1_CALIBRATION_CROSSTALK or VL53L1_CALIBRATION_OFFSET.
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copied
 * @li -EBUSY when trying to set calibration data while ranging
 * @li -EIO. Read last_error to get device error code
 * @li -ENODEV. Device has been removed.
 */
#define VL53L1_IOCTL_CALIBRATION_DATA\
	_IOWR('p', 0x12, struct stmvl53l1_ioctl_calibration_data_t)

/**
 * Get / set zone calibration data
 *
 * this call allow client to either read zone calibration data after calibration
 * has been performed to store them in the host filesystem or push zone
 * calibration data before ranging at each start-up.
 *
 * use this after VL53L1_CALIBRATION_OFFSET_PER_ZONE calibration.
 *
 * @param [in/out] data struct ptr of type
 * @ref stmvl53l1_ioctl_zone_calibration_data_t. Caller should consider it as an
 * opaque structure.
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copied
 * @li -EBUSY when trying to set calibration data while ranging
 * @li -EIO. Read last_error to get device error code
 * @li -ENODEV. Device has been removed.
 */
#define VL53L1_IOCTL_ZONE_CALIBRATION_DATA\
	_IOWR('p', 0x12, struct stmvl53l1_ioctl_zone_calibration_data_t)

/**
 * perform calibration squence according to calibration_type
 *
 * this call is attended to be used during factory calibration. You select
 * calibration to issue using calibration_type.
 *
 * @param [in] data struct ptr of type
 * @ref stmvl53l1_ioctl_perform_calibration_t.
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copied
 * @li -EBUSY when trying to perform calibration data while ranging
 * @li -EIO. Read last_error to get device error code
 * @li -ENODEV. Device has been removed.
 */
#define VL53L1_IOCTL_PERFORM_CALIBRATION\
	_IOW('p', 0x13, struct stmvl53l1_ioctl_perform_calibration_t)

/**
 * set/get configure autonomous mode parameters
 *
 * Allow to get or set autonomous configuration. Change it only when device
 * is stopped otherwise you will receive an EBUSY error.
 *
 * @param stmvl53l1_autonomous_config_t [in/out]
 *
 * @note autonomous config validity is only checked at start ranging , as such
 * invalid autonomous config set can make start to fail.
 *
 * @return 0 on success , see errno for error detail
 * @li -EFAULT failed to copy from/to configuration.
 * @li -EBUSY when trying to change configuration while ranging.
 * @li -ENODEV. Device has been removed.
 */
#define VL53L1_IOCTL_AUTONOMOUS_CONFIG\
	_IOWR('p', 0x14, struct stmvl53l1_autonomous_config_t)

/**
 * suspend ranging (no argument)

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
	rc= ioctl(fd, VL53L1_IOCTL_SUSPEND,NULL);
	if( rc ){
		if( errno == EBUSY ){
			ioctl_warn("already suspended");
			return errno;
		}
		ioctl_error("%d %s", rc,strerror(errno));
	}
	return rc;
}
@endcode
 */
#define VL53L1_IOCTL_SUSPEND		   _IO('p', 0x15)

/** @} */ /* ioctl group */
#endif /* STMVL53L1_IF_H */
