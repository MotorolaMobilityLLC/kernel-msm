#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <mach/mt_pm_ldo.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <cust_eint.h>
#include <mach/eint.h>
#include <cust_eint.h>
#include <linux/kthread.h>
#include <mach/mt_gpio.h>

#include "vdm_types.h"
#include "vdm_bitfield_translators.h"

// determines, based on the 32-bit header, whether a VDM is structured or unstructured.
VdmType getVdmTypeOf(u32 in) {
	VdmType ret;
	
	UnstructuredVdmHeader vdm_header = getUnstructuredVdmHeader(in);
	return vdm_header.vdm_type;
}

// process a 32 bit field and parses it for VDM Header data
// returns Unstructured VDM Header struct
UnstructuredVdmHeader getUnstructuredVdmHeader(u32 in) {
	UnstructuredVdmHeader ret;
	
	ret.svid = 		(in >> 16) & 0x0000FFFF;
	ret.vdm_type = 	(in >> 15) & 0x00000001;
	ret.info = 		(in >>  0) & 0x00007FFF;
	
	return ret;
}

// Turn the internal Unstructured VDM Header struct representation into a 32 bit field
u32 	getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in) {
	u32 ret;
	u32 tmp;	// cast each member to a 32 bit type first so it can be easily ORd in
	
	ret = 0;
	
	tmp = in.svid;
	ret |= (tmp << 16);
	
	tmp = in.vdm_type;
	ret |= (tmp << 15);
	
	tmp = (in.info << 0);
	ret |= tmp;
	
	return ret;
}

// process a 32 bit field and return a parsed StructuredVdmHeader
// assumes that the 32 bits are actually structured and not unstructured.
// returns parsed Structured VDM Header Struct
StructuredVdmHeader getStructuredVdmHeader(u32 in) {
	StructuredVdmHeader ret;
	
	ret.svid = 			(in >> 16) & 0x0000FFFF;
	ret.vdm_type = 		(in >> 15) & 0x00000001;
	ret.svdm_version = 	(in >> 13) & 0x00000003;
	ret.obj_pos = 		(in >>  8) & 0x00000007;
	ret.cmd_type = 		(in >>  6) & 0x00000003;
	ret.command =		(in >>  0) & 0x0000001F;
	
	return ret;
}

// Turn the internal Structured VDM Header struct representation into a 32 bit field
u32 	getBitsForStructuredVdmHeader(StructuredVdmHeader in) {
	u32 ret;
	u32 tmp;	// cast each member to a 32 bit type first so it can be easily ORd in
	
	ret = 0;
	
	tmp = in.svid;
	ret |= (tmp << 16);
	
	tmp = in.vdm_type;
	ret |= (tmp << 15);
	
	tmp = in.svdm_version;
	ret |= (tmp << 13);
	
	tmp = in.obj_pos;
	ret |= (tmp <<  8);
	
	tmp = in.cmd_type;
	ret |= (tmp <<  6);
	
	tmp = in.command;
	ret |= (tmp <<  0);
	
	return ret;
}


// process a 32 bit field and parses it for ID Header data
// returns parsed ID Header struct
IdHeader getIdHeader(u32 in) {
	IdHeader ret;
	
	ret.usb_host_data_capable = 		(in >> 31) & 0x00000001;
	ret.usb_device_data_capable = 		(in >> 30) & 0x00000001;
	ret.product_type = 					(in >> 27) & 0x00000007;
	ret.modal_op_supported = 			(in >> 26) & 0x00000001;
	ret.usb_vid =						(in >>  0) & 0x0000FFFF;
	
	return ret;
}

// Turn the internal Structured VDM Header struct representation into a 32 bit field
u32 	getBitsForIdHeader(IdHeader in) {
	u32 ret;
	u32 tmp;	// cast each member to a 32 bit type first so it can be easily ORd in
	
	ret = 0;
	
	tmp = in.usb_host_data_capable;
	ret |= (tmp << 31);
	
	tmp = in.usb_device_data_capable;
	ret |= (tmp << 30);
	
	tmp = in.product_type;
	ret |= (tmp << 27);
	
	tmp = in.modal_op_supported;
	ret |= (tmp << 26);
	
	tmp = in.usb_vid;
	ret |= (tmp <<  0);
	
	return ret;
}

// process a 32 bit field and parses it for Product VDO data
// returns parsed Product VDO struct
ProductVdo getProductVdo(u32 in) {
	ProductVdo ret;
	
	ret.usb_product_id = 	(in >> 16) & 0x0000FFFF;
	ret.bcd_device = 		(in >>  0) & 0x0000FFFF;

	return ret;
}

// Turn the internal Product VDO struct representation into a 32 bit field
u32 	getBitsForProductVdo(ProductVdo in) {
	u32 ret;
	u32 tmp;
	
	ret = 0;

	tmp = in.usb_product_id;
	ret |= (tmp << 16);
	
	tmp = in.bcd_device;
	ret |= (tmp <<  0);
	
	return ret;
}

// process a 32 bit field and parses it for Cert Stat VDO data
// returns parsed Cert Stat VDO struct
CertStatVdo getCertStatVdo(u32 in) {
	CertStatVdo ret;
	
	ret.test_id = (in >>  0) & 0x000FFFFF;
	
	return ret;
}

// Turn the internal Cert Stat VDO struct representation into a 32 bit field
u32 	getBitsForCertStatVdo(CertStatVdo in) {
	u32 ret;
	u32 tmp;
	
	ret = 0;

	tmp = in.test_id;
	ret |= (tmp <<  0);
	
	return ret;
}

// process a 32 bit field and parses it for Cable VDO data
// returns parsed Cable VDO struct
CableVdo getCableVdo(u32 in) {
	CableVdo ret;
	
	// TODO: Ought to enumerate things like bit offset and size. - Gabe
	ret.cable_hw_version =			(in >> 28) & 0x0000000F;
	ret.cable_fw_version =			(in >> 24) & 0x0000000F;
	ret.cable_to_type =				(in >> 18) & 0x00000003;
	ret.cable_to_pr =				(in >> 17) & 0x00000001;
	ret.cable_latency =				(in >> 13) & 0x0000000F;
	ret.cable_term = 				(in >> 11) & 0x00000003;
	ret.sstx1_dir_supp =			(in >> 10) & 0x00000001;
	ret.sstx2_dir_supp =			(in >>  9) & 0x00000001;
	ret.ssrx1_dir_supp =			(in >>  8) & 0x00000001;
	ret.ssrx2_dir_supp =			(in >>  7) & 0x00000001;
	ret.vbus_current_handling_cap =	(in >>  5) & 0x00000003;
	ret.vbus_thru_cable =			(in >>  4) & 0x00000001;
	ret.sop2_presence =				(in >>  3) & 0x00000001;
	ret.usb_ss_supp = 				(in >>  0) & 0x00000007;

	return ret;
}

// turn the internal Cable VDO representation into a 32 bit field
u32 getBitsForCableVdo(CableVdo in) {
	u32 ret;
	u32 tmp;
	
	ret = 0;
	
	tmp = in.cable_hw_version;
	ret |= (tmp << 28);
	
	tmp = in.cable_fw_version;
	ret |= (tmp << 24);
	
	tmp = in.cable_to_type;
	ret |= (tmp << 18);
	
	tmp = in.cable_to_pr;
	ret |= (tmp << 17);
	
	tmp = in.cable_latency;
	ret |= (tmp << 13);
	
	tmp = in.cable_term;
	ret |= (tmp << 11);
	
	tmp = in.sstx1_dir_supp;
	ret |= (tmp << 10);
	
	tmp = in.sstx2_dir_supp;
	ret |= (tmp <<  9);
	
	tmp = in.ssrx1_dir_supp;
	ret |= (tmp <<  8);
	
	tmp = in.ssrx2_dir_supp;
	ret |= (tmp <<  7);
	
	tmp = in.vbus_current_handling_cap;
	ret |= (tmp <<  5);
	
	tmp = in.vbus_thru_cable;
	ret |= (tmp <<  4);
	
	tmp = in.sop2_presence;
	ret |= (tmp <<  3);
	
	tmp = in.usb_ss_supp;
	ret |= (tmp <<  0);
	
	return ret;
}

// process a 32 bit field and parses it for AMA VDO data
// returns parsed AMA VDO struct
AmaVdo getAmaVdo(u32 in) {
	AmaVdo ret;
	
	ret.cable_hw_version =			(in >> 28) & 0x0000000F;
	ret.cable_fw_version =			(in >> 24) & 0x0000000F;
	ret.sstx1_dir_supp =			(in >> 11) & 0x00000001;
	ret.sstx2_dir_supp =			(in >> 10) & 0x00000001;
	ret.ssrx1_dir_supp =			(in >>  9) & 0x00000001;
	ret.ssrx2_dir_supp =			(in >>  8) & 0x00000001;
	ret.vconn_full_power =			(in >>  5) & 0x00000007;
	ret.vconn_requirement =			(in >>  4) & 0x00000001;
	ret.vbus_requirement =			(in >>  3) & 0x00000001;
	ret.usb_ss_supp =				(in >>  0) & 0x00000007;
	
	return ret;
}

// turn the internal AMA VDO representation into a 32 bit field
u32	getBitsForAmaVdo(AmaVdo in) {
	u32 ret;
	u32 tmp;
	
	ret = 0;
	
	tmp = in.cable_hw_version;
	ret |= (tmp << 28);
	
	tmp = in.cable_fw_version;
	ret |= (tmp << 24);
	
	tmp = in.sstx1_dir_supp;
	ret |= (tmp << 11);
	
	tmp = in.sstx2_dir_supp;
	ret |= (tmp << 10);
	
	tmp = in.ssrx1_dir_supp;
	ret |= (tmp <<  9);
	
	tmp = in.ssrx2_dir_supp;
	ret |= (tmp <<  8);
	
	tmp = in.vconn_full_power;
	ret |= (tmp <<  5);
	
	tmp = in.vconn_requirement;
	ret |= (tmp <<  4);
	
	tmp = in.vbus_requirement;
	ret |= (tmp <<  3);
	
	tmp = in.usb_ss_supp;
	ret |= (tmp <<  0);
	
	return ret;
}
