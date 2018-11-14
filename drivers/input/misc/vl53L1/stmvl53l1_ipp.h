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
 * @file stmvl53l1_ipp.h
 *
 * helper to serialize de-serialize data in ipp exchange  between user/kernel
 *
 * @date Sep 2, 2016
 * @author imaging
 */

#ifndef _STMVL53L1_IPP_H_
#define _STMVL53L1_IPP_H_

#include "vl53l1_types.h"


/** @ingroup ipp_dev
 * @{
 */

/**
 * @defgroup ipp_serialize ST IPP serialization helper
 *
 * to use the dump help you lust define first a few extra "specific"
 *
 * @li IPP_PRINT(fmt,..) with typical printf/printk interface
 */
 /** @{ */
/**
 * generic data type to serialized scalar and offset for pointer
 *
 * serialized data can be seen as a first array of n ipp_art_t
 * where each input are arg value for scaler type
 * and an offset with respect to the ipp_arg base array where the data
 *
 * @note if all ipp argument can fit on 32 bit  then ipp_arg_t is best set as
 * a 32 bit base type '(i e uint32_t) to save space
 * on 64 bit cpu it can remain 64 bit type to get better alignment
 */
typedef uint64_t ipp_arg_t;

/**
 * set to the cpu structure alignment and packing constrain
 *
 * all serialized argument passed by pointer will be stored with
 * this memory align constrain
 *
 *
 * @note if target cpu is ok to access unaligned data or less constrain ie 64
 * bit data align 32 are ok then it can be set to 4 to save space in data
 * packing and copy\n
 * We may be over constraining in many cases as a struct is only required to
 * be aligned on i'st biggest item size
 *
 * @warning it must be a 2 power of 2 (& operation instead of mudulo used in
 * align calculation)
 * @warning using 8 byte constrain require that the @a ipp_arg_t is already
 * aligned on that constrain
 */
#define IPP_ALIGN_REQ   8

/**
 * set x to it's nearest aligned offset (not that 0 is fine as a valid offset
 * than will not be up aligned to next align chunk  )
 */
#define IPP_ALIGN_OFFSET(x)  (((x)+(IPP_ALIGN_REQ-1)) & (~(IPP_ALIGN_REQ-1)))

/**
 * declare variable needed for ipp serialization
 */
#define IPP_SERIALIZE_VAR	int ipp_offset
/**
 * Put in function start to init the serialization coding
 *
 * @param ipp_args buffer for data serialization
 * @param n_args is the total number of argument scalar and ptr to serialized
 *  dot not count out only arg that will be returned
 *
 * it define local var "ipp_offset" used to accumulate all input args that must
 * be serialize. It reserve header room in the buffer to place for scalar args
 * and offset for args ptr copy
 *
 * args pass by pointer must be serialized by @ref IPP_SET_ARG_PTR in order they
 * are declare\n
 * scalar args can be serialized in any order with @ref IPP_SET_ARG
 *
 * scalar args that can't be cast to basic @ref ipp_arg_t shall be serialized
 * Manually or using a macro similar to ptr that can be pass by copy
 *
 *@note @ref IPP_SERIALIZE_VAR must be use first
 * @code
 * int ipp_to _ser(int arg0, struct st_t *pdata1){
 * char *local var;
 * ipp_arg_t args[256];
 *  IPP_SERIALIZE_START(2);
 *  IPP_SET_ARG(args, 0, arg0);
 *  IPP_SET_ARG_PTR(args, 1, pdata);
 *  do_one_ipp(args);
 * }
 * @endcode
 *
 */
#define IPP_SERIALIZE_START(ipp_args, n_args)\
	(ipp_offset = IPP_ALIGN_OFFSET((char *)(&ipp_args[n_args]) - \
	(char *)ipp_args))

/**
 * @brief Serialize scalar argument
 *
 * serialized arg number n into ipp_args\n
 * can be used in any order
 *
 * @param ipp_args the args buffer
 * @param n 0 base arg number
 * @param v the scalar argument variable
 * @warning usable only for scalar type that can be copy on a ipp_arg_t
 */
#define IPP_SET_ARG(ipp_args, n, v) memcpy(&ipp_args[n], &v, sizeof(v))

/**
 * @brief  Serialize an arg passed by pointer
 *
 * @param  ipp_args the arg ptr array
 * @param  n the arg number to serialize
 * @param  pdata  argument it must be a type like struct x_t *  int [n] etc ...
 *  that size is given by size of and and be copied by a single memcpy
 */
#define IPP_SET_ARG_PTR(ipp_args, n, pdata)\
	do {\
		ipp_args[n] = ipp_offset;\
		memcpy(((char *)ipp_args) + ipp_offset, pdata, sizeof(*pdata));\
		ipp_offset = IPP_ALIGN_OFFSET(ipp_offset + sizeof(*pdata));\
	} while (0)

/**
 * serialize out get ptr for pdata
 *
 * @note it does not cpy data just set ptr and update the offset
 * @warning  to be use in order
 *
 * @param  ipp_args the arg ptr array
 * @param  n the arg number to serialize (unused)
 * @param  pdata init to ptr in ipp_args type is used for offset computation
 * @warning to use sequential in order agr are serialized
 */
#define IPP_OUT_ARG_PTR(ipp_args, n, pdata)\
	do {\
		pdata = (void *)(((char *)ipp_args) + ipp_offset);\
		ipp_offset = IPP_ALIGN_OFFSET(ipp_offset + \
		(int)sizeof(*pdata));\
	} while (0)


/**
 * @brief ipp get payload
 *
 * @return paylaod at time used \n
 * when all done it's overall out payload
 *
 * require @ref IPP_SERIALIZE_VAR and @ref IPP_SERIALIZE_START used first\n
 * best use after all @ref IPP_OUT_ARG_PTR or @ref IPP_SET_ARG_PTR done to get
 * full payload
 **/
#define IPP_SERIALIZE_PAYLAOD() (ipp_offset + IPP_WORK_HDR_SIZE)

/**
 * de-serialize and argument that was passed by value
 * @param  ipp_args the args array
 * @param  n  the 0 base argument number
 * @param  v argument it must be exact type
 *
 *
 * @code
 * f_deserialize(ipp_arg_t args[])
 * {
 *	// f_ser is like (uint16_t arg0, struct s_arg_t * arg1, int arg2)
 *	uint16_t arg0;
 *	void * parg1;
 *	int arg2;
 *
 *	IPP_GET_ARG( args, 0, arg0)
 *	IPP_GET_ARG( args, 2, arg2)
 * }
 * @endcode
 */
#define IPP_GET_ARG(ipp_args, n, v) memcpy(&v, &ipp_args[n], sizeof(v))


/**
 * de-serialize an argument passed by pointer
 *
 * @param ipp_args the serialized argument array
 * @param n 0 base argument number
 * @param p  ptr to arg to be set
 *
 * @note unlike serializing de-serializing pointer args data can be done in any
 * order
 *
 * @code
 * struct some_struct_t *parg2;
 * IPP_GET_ARG_PTR(args,2,parg2);
 * @endcode
 */
#define IPP_GET_ARG_PTR(ipp_args, n, p) (p = (void *)((char *)ipp_args + \
	ipp_args[n]))




/**
 * debug macro to pint out all serialized args
 *
 * Implementation shall define IPP_PRINT_FUNC function to use
 */
#define IPP_PRINT_ARGS(ipp_args, n) \
	do {\
		int i;\
		for (i = 0; i < n; i++)\
			IPP_PRINT("arg#%d/%d is %8d 0x%08x\n", i, n,\
				ipp_args[i], ipp_args[i]);\
		IPP_PRINT("used data size %d\n", ipp_offset);\
	} while (0)


/**
 * processing code type of proccesing
 *
 * used in @a ipp_work_t::process_no
 */
enum stmvl53l1_ipp_proccesing_e {
	stmvl53l1_ipp_ping = 0,
	/*!< stmvl53l1_ipp_ping
	 * @li can be sent by driver to check client is alive
	 * @li  daemon sent it to identify and register himself to the driver
	 */
	stmvl53l1_ipp_cal_hist = 1,
	/*!< stmvl53l1_ipp_cal_hist process cal hist*/

	stmvl53l1_ipp_xtalk_calibration = 2,
	/*!< stmvl53l1_ipp_xtalk_calibration process crosstalk calibration data
	 */

	stmvl53l1_ipp_hist_ambient_dmax = 3,
	/*!< stmvl53l1_ipp_hist_ambient_dmax process ambient dmac calculation
	 * from histogram
	 */

	stmvl53l1_ipp_generate_dual_reflectance_xtalk_samples = 4,
	/*!< stmvl53l1_ipp_generate_dual_reflectance_xtalk_samples process
	 * Xtalk data from dual reflectance histogram data
	 */

	/** keep last*/
	stmvl53l1_ipp_max /*!< stmvl53l1_ipp_max */
};

/**
 * status use on @a ipp_work_t::status
 */
enum stmvl53l1_ipp_status_e {
	stmvl53l1_ipp_status_ok = 0,	/*!< ok work done */
	stmvl53l1_ipp_status_inv_id,	/*!< dev id  not supported or invalid */
	stmvl53l1_ipp_status_inv_proc,
	/*!< process_no asked  not supported or not implemented */
	stmvl53l1_ipp_status_inv_payload,
	/*!< data payload for asked processing incorrect*/

	stmvl53l1_ipp_status_proc_code = 0x100,
	/*!< the lowest 8 bit is the error code form the processing */
};

/**
 * Ipp work (job) struct
 *
 * containing header with sequenc control information plus serialized data
 */
struct ipp_work_t {
	int8_t dev_id;		/*!< [in]/[out] device id */
	/*!< Identify the work do be run see @a stmvl53l1_ipp_proccesing_e */
	uint8_t process_no;
	/*!< [out] status from daemon */
	int16_t status;
	/*!< [in/out] unique xfer id */
	uint32_t xfer_id;
	/*!< [in/out] effective data length including header*/
	uint32_t payload;

/** max IPP data payload (not including header)
 *
 * we substract size of of item above
 * must be lesss than one netlink packet
*/
#define MAX_IPP_DATA	((4096-4*3)/8)
	ipp_arg_t data[MAX_IPP_DATA];	/*!< [in][out] */
};

/**
 * size of header only message ( no payload)
 * require \#include <stddef.h> in user land
*/
#define IPP_WORK_HDR_SIZE	(offsetof(struct ipp_work_t, data[0]))
/**
 * max payload per ipp transfer
 */
#define IPP_WORK_MAX_PAYLOAD	sizeof(struct ipp_work_t)

/** copy ipp header from src to dest
 *
 * used to prepare return work using incoming work header
 * @param dest  dest work ptr
 * @param src   src work ptr
 */
#define IPP_CPY_HEADER(dest, src) memcpy(dest, src, IPP_WORK_HDR_SIZE)

/**
 * dump in human readble way ipp struct
 *
 * @note require IPP_PRINT macro
 *
 * @param pw	ipp_work struct to dump
 * @param max_data	max amount of data to be dump
 * @param max_dev	max number of dev (for check)
 */
static inline void ipp_dump_work(struct ipp_work_t *pw, uint32_t max_data,
		int max_dev)
{
	uint32_t data_cnt;
	uint32_t i;
	uint8_t *pbdata;

	(void)max_dev; /* avoid warning when not used */
	(void)pbdata; /*avoid warning in case no print no use*/

	IPP_PRINT("dev #%d (%s)\n", pw->dev_id, pw->dev_id < max_dev ?
			"ok" : "bad");
	IPP_PRINT("process #%d (%s)\n", pw->process_no,
			pw->process_no < stmvl53l1_ipp_max ? "ok" : "bad");
	IPP_PRINT("status %d\n",  pw->status);
	IPP_PRINT("Xfer id 0x%08X payload  %d bytes (%s)\n", pw->xfer_id,
			pw->payload,
			pw->payload > IPP_WORK_MAX_PAYLOAD ? "invalid" : "ok");
	data_cnt = pw->payload > IPP_WORK_MAX_PAYLOAD ?
			IPP_WORK_MAX_PAYLOAD : pw->payload;
	data_cnt  = data_cnt > max_data ? max_data : data_cnt;
	for (i = 0, pbdata = (uint8_t *)pw->data; i < data_cnt; i++) {
		if (i%16 == 0)
			IPP_PRINT("\n%4X\t", i);
		IPP_PRINT("%02X ", pbdata[i]);
	}
	IPP_PRINT("\n");
}

/** @} */ /* ingroup helper */

/** @} */ /* ingroup ipp_dev */
#endif /* _STMVL53L1_IPP_H_ */
