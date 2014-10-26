/*                                                                                                                                                       
        include file

*/
#ifndef __AXC_TI_GAUGE_H__
#define __AXC_TI_GAUGE_H__  


//Eason: TIgauge update status+++
typedef enum  
{
	UPDATE_NONE=0,
	UPDATE_OK,
	UPDATE_FROM_ROM_MODE,
	UPDATE_VOLT_NOT_ENOUGH,
	UPDATE_CHECK_MODE_FAIL,
	UPDATE_ERR_MATCH_OP_BUF,
	UPDATE_PROCESS_FAIL,
}TIgauge_Up_Type;


//Eason: TIgauge update status---


#endif //__AXC_TI_GAUGE_H__



