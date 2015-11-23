#ifndef __VDM_BITFIELD_TRANSLATORS_H__
#define __VDM_BITFIELD_TRANSLATORS_H__

/*
 * Functions that convert bits into internal header representations...
 */
UnstructuredVdmHeader getUnstructuredVdmHeader(u32 in);	// converts 32 bits into an unstructured vdm header struct
StructuredVdmHeader getStructuredVdmHeader(u32 in);	// converts 32 bits into a structured vdm header struct
IdHeader getIdHeader(u32 in);	// converts 32 bits into an ID Header struct
VdmType getVdmTypeOf(u32 in);	// returns structured/unstructured vdm type

/*
 * Functions that convert internal header representations into bits...
 */
u32 getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in);	// converts unstructured vdm header struct into 32 bits
u32 getBitsForStructuredVdmHeader(StructuredVdmHeader in);	// converts structured vdm header struct into 32 bits
u32 getBitsForIdHeader(IdHeader in);	// converts ID Header struct into 32 bits

/*
 * Functions that convert bits into internal VDO representations...
 */
CertStatVdo getCertStatVdo(u32 in);
ProductVdo getProductVdo(u32 in);
CableVdo getCableVdo(u32 in);
AmaVdo getAmaVdo(u32 in);

/*
 * Functions that convert internal VDO representations into bits...
 */
u32 getBitsForProductVdo(ProductVdo in);	// converts Product VDO struct into 32 bits
u32 getBitsForCertStatVdo(CertStatVdo in);	// converts Cert Stat VDO struct into 32 bits
u32 getBitsForCableVdo(CableVdo in);	// converts Cable VDO struct into 32 bits
u32 getBitsForAmaVdo(AmaVdo in);	// converts AMA VDO struct into 32 bits

#endif // header guard
