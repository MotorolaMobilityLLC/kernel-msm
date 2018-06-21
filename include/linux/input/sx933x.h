/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef SX933x_H
#define SX933x_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#if 0
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif
/*
 *  I2C Registers
 */
#define   SX933X_HOSTIRQSRC_REG           0x4000
#define   SX933X_HOSTIRQMSK_REG           0x4004
#define   SX933X_HOSTIRQCTRL_REG          0x4008
#define   SX933X_PAUSESTAT_REG            0x4010
#define   SX933X_AFECTRL_REG              0x4054
#define   SX933X_CLKGEN_REG               0x4200
#define   SX933X_I2CADDR_REG              0x41C4
#define   SX933X_RESET_REG                0x4240
#define   SX933X_CMD_REG                  0x4280
#define   SX933X_TOPSTAT0_REG             0x4284
#define   SX933X_PINCFG_REG               0x42C0
#define   SX933X_PINDOUT_REG              0x42C4
#define   SX933X_PINDIN_REG               0x42C8
#define   SX933X_INFO_REG                 0x42D8
#define   SX933X_STAT0_REG                0x8000
#define   SX933X_STAT1_REG                0x8004
#define   SX933X_STAT2_REG                0x8008
#define   SX933X_IRQCFG0_REG              0x800C
#define   SX933X_IRQCFG1_REG              0x8010
#define   SX933X_IRQCFG2_REG              0x8014
#define   SX933X_IRQCFG3_REG              0x8018
#define   SX933X_SCANPERIOD_REG           0x801C
#define   SX933X_GNRLCTRL2_REG            0x8020
#define   SX933X_AFEPARAMSPH0_REG         0x8024
#define   SX933X_AFEPHPH0_REG             0x8028
#define   SX933X_AFEPARAMSPH1_REG         0x802C
#define   SX933X_AFEPHPH1_REG             0x8030
#define   SX933X_AFEPARAMSPH2_REG         0x8034
#define   SX933X_AFEPHPH2_REG             0x8038
#define   SX933X_AFEPARAMSPH3_REG         0x803C
#define   SX933X_AFEPHPH3_REG             0x8040
#define   SX933X_AFEPARAMSPH4_REG         0x8044
#define   SX933X_AFEPHPH4_REG             0x8048
#define   SX933X_AFEPARAMSPH5_REG         0x804c
#define   SX933X_AFEPHPH5_REG             0x8050

#define   SX933X_ADCFILTPH0_REG           0x8054
#define   SX933X_AVGBFILTPH0_REG          0x8058
#define   SX933X_AVGAFILTPH0_REG          0x805C
#define   SX933X_ADVDIG0PH0_REG           0x8060
#define   SX933X_ADVDIG1PH0_REG           0x8064
#define   SX933X_ADVDIG2PH0_REG           0x8068
#define   SX933X_ADVDIG3PH0_REG           0x806C
#define   SX933X_ADVDIG4PH0_REG           0x8070
#define   SX933X_ADCFILTPH1_REG           0x8074
#define   SX933X_AVGBFILTPH1_REG          0x8078
#define   SX933X_AVGAFILTPH1_REG          0x807C
#define   SX933X_ADVDIG0PH1_REG           0x8080
#define   SX933X_ADVDIG1PH1_REG           0x8084
#define   SX933X_ADVDIG2PH1_REG           0x8088
#define   SX933X_ADVDIG3PH1_REG           0x808C
#define   SX933X_ADVDIG4PH1_REG           0x8090
#define   SX933X_ADCFILTPH2_REG           0x8094
#define   SX933X_AVGBFILTPH2_REG          0x8098
#define   SX933X_AVGAFILTPH2_REG          0x809C
#define   SX933X_ADVDIG0PH2_REG           0x80A0
#define   SX933X_ADVDIG1PH2_REG           0x80A4
#define   SX933X_ADVDIG2PH2_REG           0x80A8
#define   SX933X_ADVDIG3PH2_REG           0x80AC
#define   SX933X_ADVDIG4PH2_REG           0x80B0
#define   SX933X_ADCFILTPH3_REG           0x80B4
#define   SX933X_AVGBFILTPH3_REG          0x80B8
#define   SX933X_AVGAFILTPH3_REG          0x80BC
#define   SX933X_ADVDIG0PH3_REG           0x80C0
#define   SX933X_ADVDIG1PH3_REG           0x80C4
#define   SX933X_ADVDIG2PH3_REG           0x80C8
#define   SX933X_ADVDIG3PH3_REG           0x80CC
#define   SX933X_ADVDIG4PH3_REG           0x80D0
#define   SX933X_ADCFILTPH4_REG           0x80D4
#define   SX933X_AVGBFILTPH4_REG          0x80D8
#define   SX933X_AVGAFILTPH4_REG          0x80DC
#define   SX933X_ADVDIG0PH4_REG           0x80E0
#define   SX933X_ADVDIG1PH4_REG           0x80E4
#define   SX933X_ADVDIG2PH4_REG           0x80E8
#define   SX933X_ADVDIG3PH4_REG           0x80EC
#define   SX933X_ADVDIG4PH4_REG           0x80F0
#define   SX933X_ADCFILTPH5_REG           0x80F4
#define   SX933X_AVGBFILTPH5_REG          0x80F8
#define   SX933X_AVGAFILTPH5_REG          0x80FC
#define   SX933X_ADVDIG0PH5_REG           0x8100
#define   SX933X_ADVDIG1PH5_REG           0x8104
#define   SX933X_ADVDIG2PH5_REG           0x8108
#define   SX933X_ADVDIG3PH5_REG           0x810C
#define   SX933X_ADVDIG4PH5_REG           0x8110

#define   SX933X_REFCORRA_REG             0x8124
#define   SX933X_REFCORRB_REG             0x8128

#define   SX933X_SMARTSAR0A_REG           0x812C
#define   SX933X_SMARTSAR1A_REG           0x8130
#define   SX933X_SMARTSAR2A_REG           0x8134

#define   SX933X_SMARTSAR0B_REG           0x8138
#define   SX933X_SMARTSAR1B_REG           0x813C
#define   SX933X_SMARTSAR2B_REG           0x8140

#define   SX933X_SMARTSAR0C_REG           0x8144
#define   SX933X_SMARTSAR1C_REG           0x8148
#define   SX933X_SMARTSAR2C_REG           0x814C




#define   SX933X_AUTOFREQ0_REG            0x8154
#define   SX933X_AUTOFREQ1_REG            0x8158

#define   SX933X_USEPH0_REG               0x815C
#define   SX933X_USEPH1_REG               0x8160
#define   SX933X_USEPH2_REG               0x8164
#define   SX933X_USEPH3_REG               0x8168
#define   SX933X_USEPH4_REG               0x816C
#define   SX933X_USEPH5_REG               0x8170

#define   SX933X_AVGPH0_REG               0x8174
#define   SX933X_AVGPH1_REG               0x8178
#define   SX933X_AVGPH2_REG               0x817C
#define   SX933X_AVGPH3_REG               0x8180
#define   SX933X_AVGPH4_REG               0x8184
#define   SX933X_AVGPH5_REG               0x8188

#define   SX933X_DIFFPH0_REG              0x818C
#define   SX933X_DIFFPH1_REG              0x8190
#define   SX933X_DIFFPH2_REG              0x8194
#define   SX933X_DIFFPH3_REG              0x8198
#define   SX933X_DIFFPH4_REG              0x819C
#define   SX933X_DIFFPH5_REG              0x81A0

#define   SX933X_OFFSETPH0_REG            SX933X_AFEPHPH0_REG//bit14:0
#define   SX933X_OFFSETPH1_REG            SX933X_AFEPHPH1_REG//bit14:0
#define   SX933X_OFFSETPH2_REG            SX933X_AFEPHPH2_REG//bit14:0
#define   SX933X_OFFSETPH3_REG            SX933X_AFEPHPH3_REG//bit14:0
#define   SX933X_OFFSETPH4_REG            SX933X_AFEPHPH4_REG//bit14:0
#define   SX933X_OFFSETPH5_REG            SX933X_AFEPHPH5_REG//bit14:0

//i2c register bit mask
#define   MSK_IRQSTAT_RESET               0x00000080
#define   MSK_IRQSTAT_TOUCH               0x00000040
#define   MSK_IRQSTAT_RELEASE             0x00000020
#define   MSK_IRQSTAT_COMP                0x00000010
#define   MSK_IRQSTAT_CONV                0x00000008
#define   MSK_IRQSTAT_PROG2IRQ            0x00000004
#define   MSK_IRQSTAT_PROG1IRQ            0x00000002
#define   MSK_IRQSTAT_PROG0IRQ            0x00000001

#define   I2C_SOFTRESET_VALUE             0x000000DE

#define   I2C_REGCMD_PHEN                 0x0000000F
#define   I2C_REGCMD_COMPEN               0x0000000E
#define   I2C_REGCMD_EN_SLEEP             0x0000000D
#define   I2C_REGCMD_EX_SLEEP             0x0000000C

#define   I2C_REGGNRLCTRL2_PHEN_MSK       0x000000FF
#define   I2C_REGGNRLCTRL2_COMPEN_MSK     0x00ff0000

#define   MSK_REGSTAT0_STEADYSTAT5        0x20
#define   MSK_REGSTAT0_STEADYSTAT4        0x10
#define   MSK_REGSTAT0_STEADYSTAT3        0x08
#define   MSK_REGSTAT0_STEADYSTAT2        0x04
#define   MSK_REGSTAT0_STEADYSTAT1        0x02
#define   MSK_REGSTAT0_STEADYSTAT0        0x01

#define   MSK_REGSTAT0_BODYSTAT5          ((long int)0x20<<8)
#define   MSK_REGSTAT0_BODYSTAT4          ((long int)0x10<<8)
#define   MSK_REGSTAT0_BODYSTAT3          ((long int)0x08<<8)
#define   MSK_REGSTAT0_BODYSTAT2          ((long int)0x04<<8)
#define   MSK_REGSTAT0_BODYSTAT1          ((long int)0x02<<8)
#define   MSK_REGSTAT0_BODYSTAT0          ((long int)0x01<<8)

#define   MSK_REGSTAT0_TABLESTAT5         ((long int)0x20<<16)
#define   MSK_REGSTAT0_TABLESTAT4         ((long int)0x10<<16)
#define   MSK_REGSTAT0_TABLESTAT3         ((long int)0x08<<16)
#define   MSK_REGSTAT0_TABLESTAT2         ((long int)0x04<<16)
#define   MSK_REGSTAT0_TABLESTAT1         ((long int)0x02<<16)
#define   MSK_REGSTAT0_TABLESTAT0         ((long int)0x01<<16)

#define   MSK_REGSTAT0_PROXSTAT5          ((long int)0x20<<24)
#define   MSK_REGSTAT0_PROXSTAT4          ((long int)0x10<<24)
#define   MSK_REGSTAT0_PROXSTAT3          ((long int)0x08<<24)
#define   MSK_REGSTAT0_PROXSTAT2          ((long int)0x04<<24)
#define   MSK_REGSTAT0_PROXSTAT1          ((long int)0x02<<24)
#define   MSK_REGSTAT0_PROXSTAT0          ((long int)0x01<<24)

#define   MSK_REGSTAT1_FAILSTAT5          ((long int)0x20<<24)
#define   MSK_REGSTAT1_FAILSTAT4          ((long int)0x10<<24)
#define   MSK_REGSTAT1_FAILSTAT3          ((long int)0x08<<24)
#define   MSK_REGSTAT1_FAILSTAT2          ((long int)0x04<<24)
#define   MSK_REGSTAT1_FAILSTAT1          ((long int)0x02<<24)
#define   MSK_REGSTAT1_FAILSTAT0          ((long int)0x01<<24)

#define   MSK_REGSTAT1_COMPSTAT5          ((long int)0x20<<16)
#define   MSK_REGSTAT1_COMPSTAT4          ((long int)0x10<<16)
#define   MSK_REGSTAT1_COMPSTAT3          ((long int)0x08<<16)
#define   MSK_REGSTAT1_COMPSTAT2          ((long int)0x04<<16)
#define   MSK_REGSTAT1_COMPSTAT1          ((long int)0x02<<16)
#define   MSK_REGSTAT1_COMPSTAT0          ((long int)0x01<<16)
#define   MSK_REGSTAT1_SATSTAT5           ((long int)0x20<<8)
#define   MSK_REGSTAT1_SATSTAT4           ((long int)0x10<<8)
#define   MSK_REGSTAT1_SATSTAT3           ((long int)0x08<<8)
#define   MSK_REGSTAT1_SATSTAT2           ((long int)0x04<<8)
#define   MSK_REGSTAT1_SATSTAT1           ((long int)0x02<<8)
#define   MSK_REGSTAT1_SATSTAT0           ((long int)0x01<<8)



#define   MSK_REGSTAT2_STARTUPSTAT5       ((long int)0x20)
#define   MSK_REGSTAT2_STARTUPSTAT4       ((long int)0x10)
#define   MSK_REGSTAT2_STARTUPSTAT3       ((long int)0x08)
#define   MSK_REGSTAT2_STARTUPSTAT2       ((long int)0x04)
#define   MSK_REGSTAT2_STARTUPSTAT1       ((long int)0x02)
#define   MSK_REGSTAT2_STARTUPSTAT0       ((long int)0x01)

#define   MSK_REGSTAT1_STEADYSTATALL      0x02


#define   SX933X_STAT0_PROXSTAT_PH5_FLAG      0x20000000
#define   SX933X_STAT0_PROXSTAT_PH4_FLAG      0x10000000
#define   SX933X_STAT0_PROXSTAT_PH3_FLAG      0x08000000
#define   SX933X_STAT0_PROXSTAT_PH2_FLAG      0x04000000
#define   SX933X_STAT0_PROXSTAT_PH1_FLAG      0x02000000
#define   SX933X_STAT0_PROXSTAT_PH0_FLAG      0x01000000

/*      Chip ID 	*/
#define SX933X_WHOAMI_VALUE                   0x00003113
/*command*/
#define SX933X_PHASE_CONTROL                  0x0000000F
#define SX933X_COMPENSATION_CONTROL           0x0000000E
#define SX933X_ENTER_CONTROL                  0x0000000D
#define SX933X_EXIT_CONTROL                   0x0000000C




/**************************************
 *   define platform data
 *
 **************************************/
struct smtc_reg_data
{
	unsigned int reg;
	unsigned int val;
};

typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;


struct _buttonInfo
{
	/*! The Key to send to the input */
	// int keycode; //not use
	/*! Mask to look for on Touch Status */
	int mask;
	/*! Current state of button. */
	int state;
	struct input_dev *input_dev;
	char *name;
	struct sensors_classdev sensors_capsensor_cdev;
	bool enabled;
	//may different project use different buttons
	bool used;
};

struct totalButtonInformation
{
	struct _buttonInfo *buttons;
	int buttonSize;
	//struct input_dev *input; //not used
};

typedef struct totalButtonInformation buttonInformation_t;
typedef struct totalButtonInformation *pbuttonInformation_t;


/* Define Registers that need to be initialized to values different than
 * default
 */
/*define the value without Phase enable settings for easy changes in driver*/
static const struct smtc_reg_data sx933x_i2c_reg_setup[] =
{
	//
	{
		.reg = SX933X_AFECTRL_REG,
		.val = 0x00000400,
	},
	//
	{
		.reg = SX933X_CLKGEN_REG,
		.val = 0x00000008,
	},
	//
	{
		.reg = SX933X_PINCFG_REG,
		.val = 0x08000000,
	},
	{
		.reg = SX933X_PINDOUT_REG,
		.val = 0x0000003F,
	},
	//
	{
		.reg = SX933X_IRQCFG0_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_IRQCFG1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_IRQCFG2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_IRQCFG3_REG,
		.val = 0x00000000,
	},
	//
	{
		.reg = SX933X_SCANPERIOD_REG,
		.val = 0x00000032,
	},
	{
		.reg = SX933X_GNRLCTRL2_REG,
		.val = 0x3F001F, //enable phase0~phase4  0x00FF0001,
	},
	{
		.reg = SX933X_AFEPARAMSPH0_REG,
		.val = 0x00010001,
	},
	{
		.reg = SX933X_AFEPHPH0_REG,
		.val = 0x00A00BE3,
	},
	{
		.reg = SX933X_AFEPARAMSPH1_REG,
		.val = 0x300004D4,
	},
	{
		.reg = SX933X_AFEPHPH1_REG,
		.val = 0x00,
	},
	{
		.reg = SX933X_AFEPARAMSPH2_REG,
		.val = 0x300004D4,//0x44F,//AFE setting for phase2  3.85pf
	},
	{
		.reg = SX933X_AFEPHPH2_REG,
		.val = 0x00,//Configure for phase2
	},
	{
		.reg = SX933X_AFEPARAMSPH3_REG,
		.val = 0x300004D4,//0x44F,//AFE setting for phase3 3.3pf
	},
	{
		.reg = SX933X_AFEPHPH3_REG,
		.val = 0x00,//Configure for phase3
	},
	{
		.reg = SX933X_AFEPARAMSPH4_REG,
		.val = 0x300004D4,//0x44F,//AFE setting for phase4 9.9pf
	},
	{
		.reg = SX933X_AFEPHPH4_REG,
		.val = 0x00,//Configure for phase4
	},
	{
		.reg = SX933X_AFEPARAMSPH5_REG,
		.val = 0x300004D4,
	},
	{
		.reg = SX933X_AFEPHPH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADCFILTPH0_REG,
		.val = 0x10162000,
	},
	{
		.reg = SX933X_AVGBFILTPH0_REG,
		.val = 0x20400017,
	},
	{
		.reg = SX933X_AVGAFILTPH0_REG,
		.val = 0x31CF9110,
	},
	{
		.reg = SX933X_ADVDIG0PH0_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH0_REG,
		.val = 0x00,
	},
	{
		.reg = SX933X_ADVDIG2PH0_REG,
		.val = 0x00,
	},
	{
		.reg = SX933X_ADVDIG3PH0_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH0_REG,
		.val = 0x10000,
	},
	{
		.reg = SX933X_ADCFILTPH1_REG,
		.val = 0x100000,
	},
	{
		.reg = SX933X_AVGBFILTPH1_REG,
		.val = 0x20600C00,
	},
	{
		.reg = SX933X_AVGAFILTPH1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG0PH1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG2PH1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG3PH1_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH1_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADCFILTPH2_REG,
		.val = 0x100000,
	},
	{
		.reg = SX933X_AVGBFILTPH2_REG,
		.val = 0x20600C00,
	},
	{
		.reg = SX933X_AVGAFILTPH2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG0PH2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG2PH2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG3PH2_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH2_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADCFILTPH3_REG,
		.val = 0x100000,
	},
	{
		.reg = SX933X_AVGBFILTPH3_REG,
		.val = 0x20600C00,
	},
	{
		.reg = SX933X_AVGAFILTPH3_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG0PH3_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH3_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG2PH3_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG3PH3_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH3_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADCFILTPH4_REG,
		.val = 0x100000,
	},
	{
		.reg = SX933X_AVGBFILTPH4_REG,
		.val = 0x20600C00,
	},
	{
		.reg = SX933X_AVGAFILTPH4_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG0PH4_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH4_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG2PH4_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG3PH4_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH4_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADCFILTPH5_REG,
		.val = 0x100000,
	},
	{
		.reg = SX933X_AVGBFILTPH5_REG,
		.val = 0x20600C00,
	},
	{
		.reg = SX933X_AVGAFILTPH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG0PH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG1PH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG2PH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG3PH5_REG ,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_ADVDIG4PH5_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_REFCORRA_REG,
		.val = 0x7B13B10,
	},
	{
		.reg = SX933X_REFCORRB_REG,
		.val = 0x4000000,
	},
	{
		.reg = SX933X_SMARTSAR0A_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_SMARTSAR1A_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_SMARTSAR2A_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_SMARTSAR0B_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_SMARTSAR1B_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_SMARTSAR2B_REG,
		.val = 0x00000000,
	},
	//
	{
		.reg = SX933X_AUTOFREQ0_REG,
		.val = 0x00000000,
	},
	{
		.reg = SX933X_AUTOFREQ1_REG,
		.val = 0x00000000,
	},
	//
	{
		.reg = SX933X_HOSTIRQMSK_REG,
		.val = 0x00000078,
	},
	{
		.reg = SX933X_HOSTIRQCTRL_REG,
		.val = 0x00,
	}
};

static struct _buttonInfo psmtcButtons[] =
{
	{
		.mask = SX933X_STAT0_PROXSTAT_PH0_FLAG,
		.name = "Moto CapSense Ch0",
		.enabled = false,
		.used = false,
	},
	{
		.mask = SX933X_STAT0_PROXSTAT_PH1_FLAG,
		.name = "Moto CapSense Ch1",
		.enabled = false,
		.used = false,
	},
	{
		.mask = SX933X_STAT0_PROXSTAT_PH2_FLAG,
		.name = "Moto CapSense Ch2",
		.enabled = false,
		.used = false,
	},
	{
		.mask = SX933X_STAT0_PROXSTAT_PH3_FLAG,
		.name = "Moto CapSense Ch3",
		.enabled = false,
		.used = false,
	},
	{
		.mask = SX933X_STAT0_PROXSTAT_PH4_FLAG,
		.name = "Moto CapSense Ch4",
		.enabled = false,
		.used = false,
	},
};

struct sx933x_platform_data
{
	int i2c_reg_num;
	u32 button_used_flag;
	struct regulator *cap_vdd;
	bool cap_vdd_en;
	struct smtc_reg_data *pi2c_reg;
	int irq_gpio;
	pbuttonInformation_t pbuttonInformation;

	int (*get_is_nirq_low)(void);

	int     (*init_platform_hw)(struct i2c_client *client);
	void    (*exit_platform_hw)(struct i2c_client *client);
};
typedef struct sx933x_platform_data sx933x_platform_data_t;
typedef struct sx933x_platform_data *psx933x_platform_data_t;

/***************************************
 *  define data struct/interrupt
 *
 ***************************************/

#define MAX_NUM_STATUS_BITS (8)

typedef struct sx93XX sx93XX_t, *psx93XX_t;
struct sx93XX
{
	void * bus; /* either i2c_client or spi_client */

	struct device *pdev; /* common device struction for linux */

	void *pDevice; /* device specific struct pointer */

	/* Function Pointers */
	int (*init)(psx93XX_t this); /* (re)initialize device */

	/* since we are trying to avoid knowing registers, create a pointer to a
	 * common read register which would be to read what the interrupt source
	 * is from
	 */
	int (*refreshStatus)(psx93XX_t this); /* read register status */

	int (*get_nirq_low)(void); /* get whether nirq is low (platform data) */

	/* array of functions to call for corresponding status bit */
	void (*statusFunc[MAX_NUM_STATUS_BITS])(psx93XX_t this);

	/* Global variable */
	u8 		failStatusCode;	/*Fail status code*/
	bool	reg_in_dts;

	spinlock_t       lock; /* Spin Lock used for nirq worker function */
	int irq; /* irq number used */

	/* whether irq should be ignored.. cases if enable/disable irq is not used
	 * or does not work properly */
	char irq_disabled;

	u8 useIrqTimer; /* older models need irq timer for pen up cases */

	int irqTimeout; /* msecs only set if useIrqTimer is true */

	/* struct workqueue_struct *ts_workq;  */  /* if want to use non default */
	struct delayed_work dworker; /* work struct for worker function */
	u8 phaseselect;
};

int sx93XX_IRQ_init(psx93XX_t this);

#endif
