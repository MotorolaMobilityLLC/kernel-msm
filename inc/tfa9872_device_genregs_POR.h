/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9872_DEVICE_GENREGS_H
#define _TFA9872_DEVICE_GENREGS_H


#define TFA9872_SYS_CONTROL0              0x00
#define TFA9872_SYS_CONTROL1              0x01
#define TFA9872_SYS_CONTROL2              0x02
#define TFA9872_DEVICE_REVISION           0x03
#define TFA9872_CLOCK_CONTROL             0x04
#define TFA9872_CLOCK_GATING_CONTROL      0x05
#define TFA9872_SIDE_TONE_CONFIG          0x0d
#define TFA9872_STATUS_FLAGS0             0x10
#define TFA9872_STATUS_FLAGS1             0x11
#define TFA9872_STATUS_FLAGS3             0x13
#define TFA9872_STATUS_FLAGS4             0x14
#define TFA9872_BATTERY_VOLTAGE           0x15
#define TFA9872_TEMPERATURE               0x16
#define TFA9872_VDDP_VOLTAGE              0x17
#define TFA9872_TDM_CONFIG0               0x20
#define TFA9872_TDM_CONFIG1               0x21
#define TFA9872_TDM_CONFIG2               0x22
#define TFA9872_TDM_CONFIG3               0x23
#define TFA9872_TDM_CONFIG6               0x26
#define TFA9872_TDM_CONFIG7               0x27
#define TFA9872_PDM_CONFIG0               0x31
#define TFA9872_INTERRUPT_OUT_REG1        0x40
#define TFA9872_INTERRUPT_OUT_REG2        0x41
#define TFA9872_INTERRUPT_OUT_REG3        0x42
#define TFA9872_INTERRUPT_IN_REG1         0x44
#define TFA9872_INTERRUPT_IN_REG2         0x45
#define TFA9872_INTERRUPT_IN_REG3         0x46
#define TFA9872_INTERRUPT_ENABLE_REG1     0x48
#define TFA9872_INTERRUPT_ENABLE_REG2     0x49
#define TFA9872_INTERRUPT_ENABLE_REG3     0x4a
#define TFA9872_STATUS_POLARITY_REG1      0x4c
#define TFA9872_STATUS_POLARITY_REG2      0x4d
#define TFA9872_STATUS_POLARITY_REG3      0x4e
#define TFA9872_BAT_PROT_CONFIG           0x50
#define TFA9872_AUDIO_CONTROL             0x51
#define TFA9872_AMPLIFIER_CONFIG          0x52
#define TFA9872_PGA_CONTROL0              0x60
#define TFA9872_GAIN_ATT                  0x61
#define TFA9872_TDM_SOURCE_CTRL           0x68
#define TFA9872_SAM_CTRL                  0x69
#define TFA9872_STATUS_FLAGS5             0x6e
#define TFA9872_CURSENSE_COMP             0x6f
#define TFA9872_DCDC_CONTROL0             0x70
#define TFA9872_DCDC_CONTROL4             0x74
#define TFA9872_DCDC_CONTROL5             0x75
#define TFA9872_MTPKEY2_REG               0xa1
#define TFA9872_MTP_STATUS                0xa2
#define TFA9872_KEY_PROTECTED_MTP_CONTROL 0xa3
#define TFA9872_MTP_DATA_OUT_MSB          0xa5
#define TFA9872_MTP_DATA_OUT_LSB          0xa6
#define TFA9872_TEMP_SENSOR_CONFIG        0xb1
#define TFA9872_SOFTWARE_PROFILE          0xee
#define TFA9872_SOFTWARE_VSTEP            0xef
#define TFA9872_KEY2_PROTECTED_MTP0       0xf0
#define TFA9872_KEY2_PROTECTED_MTP5       0xf5
#define TFA9872_SYS_CONTROL0_POR                                0x0001
#define TFA9872_SYS_CONTROL1_POR                                0x0000
#define TFA9872_SYS_CONTROL2_POR                                0x2828
#define TFA9872_DEVICE_REVISION_POR                             0x3b72
#define TFA9872_CLOCK_CONTROL_POR                               0x0000
#define TFA9872_CLOCK_GATING_CONTROL_POR                        0x1f6a
#define TFA9872_SIDE_TONE_CONFIG_POR                            0x0ebe
#define TFA9872_STATUS_FLAGS0_POR                               0x001d
#define TFA9872_STATUS_FLAGS1_POR                               0x0004
#define TFA9872_STATUS_FLAGS3_POR                               0x000f
#define TFA9872_STATUS_FLAGS4_POR                               0x0000
#define TFA9872_BATTERY_VOLTAGE_POR                             0x03ff
#define TFA9872_TEMPERATURE_POR                                 0x0100
#define TFA9872_VDDP_VOLTAGE_POR                                0x0000
#define TFA9872_TDM_CONFIG0_POR                                 0x2890
#define TFA9872_TDM_CONFIG1_POR                                 0xc1f1
#define TFA9872_TDM_CONFIG2_POR                                 0x045c
#define TFA9872_TDM_CONFIG3_POR                                 0x0003
#define TFA9872_TDM_CONFIG6_POR                                 0x0010
#define TFA9872_TDM_CONFIG7_POR                                 0x0001
#define TFA9872_PDM_CONFIG0_POR                                 0x0000
#define TFA9872_INTERRUPT_OUT_REG1_POR                          0x0081
#define TFA9872_INTERRUPT_OUT_REG2_POR                          0x0000
#define TFA9872_INTERRUPT_OUT_REG3_POR                          0x0000
#define TFA9872_INTERRUPT_IN_REG1_POR                           0x0000
#define TFA9872_INTERRUPT_IN_REG2_POR                           0x0000
#define TFA9872_INTERRUPT_IN_REG3_POR                           0x0000
#define TFA9872_INTERRUPT_ENABLE_REG1_POR                       0x0001
#define TFA9872_INTERRUPT_ENABLE_REG2_POR                       0x0000
#define TFA9872_INTERRUPT_ENABLE_REG3_POR                       0x0000
#define TFA9872_STATUS_POLARITY_REG1_POR                        0x74e3
#define TFA9872_STATUS_POLARITY_REG2_POR                        0x967b
#define TFA9872_STATUS_POLARITY_REG3_POR                        0x0085
#define TFA9872_BAT_PROT_CONFIG_POR                             0x8091
#define TFA9872_AUDIO_CONTROL_POR                               0x0080
#define TFA9872_AMPLIFIER_CONFIG_POR                            0x7a08
#define TFA9872_PGA_CONTROL0_POR                                0x0000
#define TFA9872_GAIN_ATT_POR                                    0x0000
#define TFA9872_TDM_SOURCE_CTRL_POR                             0x0400
#define TFA9872_SAM_CTRL_POR                                    0x0000
#define TFA9872_STATUS_FLAGS5_POR                               0x0007
#define TFA9872_CURSENSE_COMP_POR                               0x02e4
#define TFA9872_DCDC_CONTROL0_POR                               0x06e6
#define TFA9872_DCDC_CONTROL4_POR                               0xd913
#define TFA9872_DCDC_CONTROL5_POR                               0x118a
#define TFA9872_MTPKEY2_REG_POR                                 0x0000
#define TFA9872_MTP_STATUS_POR                                  0x0003
#define TFA9872_KEY_PROTECTED_MTP_CONTROL_POR                   0x0000
#define TFA9872_MTP_DATA_OUT_MSB_POR                            0x0000
#define TFA9872_MTP_DATA_OUT_LSB_POR                            0x0000
#define TFA9872_TEMP_SENSOR_CONFIG_POR                          0x0000
#define TFA9872_SOFTWARE_PROFILE_POR                            0x0000
#define TFA9872_SOFTWARE_VSTEP_POR                              0x0000
#define TFA9872_KEY2_PROTECTED_MTP0_POR                         0x0000
#define TFA9872_KEY2_PROTECTED_MTP5_POR                         0x0000

#endif /* _TFA9872_DEVICE_GENREGS_H */
