/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9872_TFAFIELDNAMES_H
#define _TFA9872_TFAFIELDNAMES_H

#define TFA9872_I2CVERSION_N1A    26
#define TFA9872_I2CVERSION_N1B    29
#define TFA9872_I2CVERSION_N1B2   21

typedef enum nxpTfa9872BfEnumList {
    TFA9872_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9872_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9872_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
    TFA9872_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9872_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9872_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9872_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA9872_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9872_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA9872_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA9872_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
    TFA9872_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA9872_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9872_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA9872_BF_BYPHVBF= 0x02b0,    /*!< Bypass HVBAT filter                                */
    TFA9872_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9872_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9872_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA9872_BF_SSE   = 0x0510,    /*!< Enable speaker path                                */
    TFA9872_BF_VSE   = 0x0530,    /*!< Voltage sense                                      */
    TFA9872_BF_CSE   = 0x0550,    /*!< Current sense                                      */
    TFA9872_BF_SSPDME= 0x0560,    /*!< Sub-system PDM                                     */
    TFA9872_BF_PGAE  = 0x0580,    /*!< Enable PGA chop clock                              */
    TFA9872_BF_SSTDME= 0x0590,    /*!< Sub-system TDM                                     */
    TFA9872_BF_SSPBSTE= 0x05a0,    /*!< Sub-system boost                                   */
    TFA9872_BF_SSADCE= 0x05b0,    /*!< Sub-system ADC                                     */
    TFA9872_BF_SSFAIME= 0x05c0,    /*!< Sub-system FAIM                                    */
    TFA9872_BF_STGAIN= 0x0d18,    /*!< Side tone gain                                     */
    TFA9872_BF_STSMUTE= 0x0da0,    /*!< Side tone soft mute                                */
    TFA9872_BF_ST1C  = 0x0db0,    /*!< side tone one s complement                         */
    TFA9872_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9872_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
    TFA9872_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA9872_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA9872_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA9872_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
    TFA9872_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
    TFA9872_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
    TFA9872_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
    TFA9872_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA9872_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA9872_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA9872_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9872_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
    TFA9872_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos                                      */
    TFA9872_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA9872_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA9872_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA9872_BF_STMUTEB= 0x1170,    /*!< side tone (un)mute busy                            */
    TFA9872_BF_STMUTE= 0x1180,    /*!< side tone mute state                               */
    TFA9872_BF_TDMLUTER= 0x1190,    /*!< TDM LUT error                                      */
    TFA9872_BF_TDMSTAT= 0x11a2,    /*!< TDM status bits                                    */
    TFA9872_BF_TDMERR= 0x11d0,    /*!< TDM error                                          */
    TFA9872_BF_OCPOAP= 0x1300,    /*!< OCPOK pmos A                                       */
    TFA9872_BF_OCPOAN= 0x1310,    /*!< OCPOK nmos A                                       */
    TFA9872_BF_OCPOBP= 0x1320,    /*!< OCPOK pmos B                                       */
    TFA9872_BF_OCPOBN= 0x1330,    /*!< OCPOK nmos B                                       */
    TFA9872_BF_CLIPAH= 0x1340,    /*!< Clipping A to Vddp                                 */
    TFA9872_BF_CLIPAL= 0x1350,    /*!< Clipping A to gnd                                  */
    TFA9872_BF_CLIPBH= 0x1360,    /*!< Clipping B to Vddp                                 */
    TFA9872_BF_CLIPBL= 0x1370,    /*!< Clipping B to gnd                                  */
    TFA9872_BF_OCDS  = 0x1380,    /*!< OCP  amplifier                                     */
    TFA9872_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
    TFA9872_BF_OCPOKMC= 0x13a0,    /*!< OCPOK MICVDD                                       */
    TFA9872_BF_MANALARM= 0x13b0,    /*!< Alarm state                                        */
    TFA9872_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA9872_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA9872_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA9872_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA9872_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA9872_BF_DCMODE= 0x1471,    /*!< DCDC mode status bits                              */
    TFA9872_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9872_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9872_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/9.5 V)                 */
    TFA9872_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
    TFA9872_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA9872_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA9872_BF_TDMFSLN= 0x2073,    /*!< FS length (master mode only)                       */
    TFA9872_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA9872_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9872_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA9872_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA9872_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA9872_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA9872_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA9872_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA9872_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA9872_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits                                 */
    TFA9872_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots DATAO                          */
    TFA9872_BF_TDMSPKE= 0x2300,    /*!< Control audio tdm channel in 0 (spkr + dcdc)       */
    TFA9872_BF_TDMDCE= 0x2310,    /*!< Control audio  tdm channel in 1  (dcdc)            */
    TFA9872_BF_TDMCSE= 0x2330,    /*!< current sense vbat temperature and vddp feedback   */
    TFA9872_BF_TDMVSE= 0x2340,    /*!< Voltage sense vbat temperature and vddp feedback   */
    TFA9872_BF_TDMSPKS= 0x2603,    /*!< tdm slot for sink 0 (speaker + dcdc)               */
    TFA9872_BF_TDMDCS= 0x2643,    /*!< tdm slot for  sink 1  (dcdc)                       */
    TFA9872_BF_TDMCSS= 0x26c3,    /*!< Slot Position of current sense vbat temperature and vddp feedback */
    TFA9872_BF_TDMVSS= 0x2703,    /*!< Slot Position of Voltage sense vbat temperature and vddp feedback */
    TFA9872_BF_PDMSTSEL= 0x3111,    /*!< Side tone input                                    */
    TFA9872_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9872_BF_ISTPLLS= 0x4010,    /*!< Status PLL lock                                    */
    TFA9872_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9872_BF_ISTOVDS= 0x4030,    /*!< Status OVP alarm                                   */
    TFA9872_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9872_BF_ISTCLKS= 0x4050,    /*!< Status clocks stable                               */
    TFA9872_BF_ISTMTPB= 0x4060,    /*!< Status MTP busy                                    */
    TFA9872_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9872_BF_ISTSWS= 0x40a0,    /*!< Status amplifier engage                            */
    TFA9872_BF_ISTAMPS= 0x40c0,    /*!< Status amplifier enable                            */
    TFA9872_BF_ISTAREFS= 0x40d0,    /*!< Status Ref enable                                  */
    TFA9872_BF_ISTADCCR= 0x40e0,    /*!< Status Control ADC                                 */
    TFA9872_BF_ISTBSTCU= 0x4100,    /*!< Status DCDC current limiting                       */
    TFA9872_BF_ISTBSTHI= 0x4110,    /*!< Status DCDC active                                 */
    TFA9872_BF_ISTBSTOC= 0x4120,    /*!< Status DCDC OCP                                    */
    TFA9872_BF_ISTBSTPKCUR= 0x4130,    /*!< Status bst peakcur                                 */
    TFA9872_BF_ISTBSTVC= 0x4140,    /*!< Status DCDC level 1x                               */
    TFA9872_BF_ISTBST86= 0x4150,    /*!< Status DCDC level 1.14x                            */
    TFA9872_BF_ISTBST93= 0x4160,    /*!< Status DCDC level 1.07x                            */
    TFA9872_BF_ISTOCPR= 0x4190,    /*!< Status ocp alarm                                   */
    TFA9872_BF_ISTMWSRC= 0x41a0,    /*!< Status Waits HW I2C settings                       */
    TFA9872_BF_ISTMWSMU= 0x41c0,    /*!< Status Audio mute sequence                         */
    TFA9872_BF_ISTCLKOOR= 0x41f0,    /*!< Status flag_clk_out_of_range                       */
    TFA9872_BF_ISTTDMER= 0x4200,    /*!< Status tdm error                                   */
    TFA9872_BF_ISTCLPR= 0x4220,    /*!< Status clip                                        */
    TFA9872_BF_ISTLP0= 0x4240,    /*!< Status low power mode0                             */
    TFA9872_BF_ISTLP1= 0x4250,    /*!< Status low power mode1                             */
    TFA9872_BF_ISTLA = 0x4260,    /*!< Status low noise detection                         */
    TFA9872_BF_ISTVDDPH= 0x4270,    /*!< Status VDDP greater than VBAT                      */
    TFA9872_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9872_BF_ICLPLLS= 0x4410,    /*!< Clear PLL lock                                     */
    TFA9872_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9872_BF_ICLOVDS= 0x4430,    /*!< Clear OVP alarm                                    */
    TFA9872_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9872_BF_ICLCLKS= 0x4450,    /*!< Clear clocks stable                                */
    TFA9872_BF_ICLMTPB= 0x4460,    /*!< Clear mtp busy                                     */
    TFA9872_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9872_BF_ICLSWS= 0x44a0,    /*!< Clear amplifier engage                             */
    TFA9872_BF_ICLAMPS= 0x44c0,    /*!< Clear enbl amp                                     */
    TFA9872_BF_ICLAREFS= 0x44d0,    /*!< Clear ref enable                                   */
    TFA9872_BF_ICLADCCR= 0x44e0,    /*!< Clear control ADC                                  */
    TFA9872_BF_ICLBSTCU= 0x4500,    /*!< Clear DCDC current limiting                        */
    TFA9872_BF_ICLBSTHI= 0x4510,    /*!< Clear DCDC active                                  */
    TFA9872_BF_ICLBSTOC= 0x4520,    /*!< Clear DCDC OCP                                     */
    TFA9872_BF_ICLBSTPC= 0x4530,    /*!< Clear bst peakcur                                  */
    TFA9872_BF_ICLBSTVC= 0x4540,    /*!< Clear DCDC level 1x                                */
    TFA9872_BF_ICLBST86= 0x4550,    /*!< Clear DCDC level 1.14x                             */
    TFA9872_BF_ICLBST93= 0x4560,    /*!< Clear DCDC level 1.07x                             */
    TFA9872_BF_ICLOCPR= 0x4590,    /*!< Clear ocp alarm                                    */
    TFA9872_BF_ICLMWSRC= 0x45a0,    /*!< Clear wait HW I2C settings                         */
    TFA9872_BF_ICLMWSMU= 0x45c0,    /*!< Clear audio mute sequence                          */
    TFA9872_BF_ICLCLKOOR= 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
    TFA9872_BF_ICLTDMER= 0x4600,    /*!< Clear tdm error                                    */
    TFA9872_BF_ICLCLPR= 0x4620,    /*!< Clear clip                                         */
    TFA9872_BF_ICLLP0= 0x4640,    /*!< Clear low power mode0                              */
    TFA9872_BF_ICLLP1= 0x4650,    /*!< Clear low power mode1                              */
    TFA9872_BF_ICLLA = 0x4660,    /*!< Clear low noise detection                          */
    TFA9872_BF_ICLVDDPH= 0x4670,    /*!< Clear VDDP greater then VBAT                       */
    TFA9872_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA9872_BF_IEPLLS= 0x4810,    /*!< Enable pll lock                                    */
    TFA9872_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9872_BF_IEOVDS= 0x4830,    /*!< Enable OVP alarm                                   */
    TFA9872_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9872_BF_IECLKS= 0x4850,    /*!< Enable clocks stable                               */
    TFA9872_BF_IEMTPB= 0x4860,    /*!< Enable mtp busy                                    */
    TFA9872_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9872_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
    TFA9872_BF_IEAMPS= 0x48c0,    /*!< Enable enbl amp                                    */
    TFA9872_BF_IEAREFS= 0x48d0,    /*!< Enable ref enable                                  */
    TFA9872_BF_IEADCCR= 0x48e0,    /*!< Enable Control ADC                                 */
    TFA9872_BF_IEBSTCU= 0x4900,    /*!< Enable DCDC current limiting                       */
    TFA9872_BF_IEBSTHI= 0x4910,    /*!< Enable DCDC active                                 */
    TFA9872_BF_IEBSTOC= 0x4920,    /*!< Enable DCDC OCP                                    */
    TFA9872_BF_IEBSTPC= 0x4930,    /*!< Enable bst peakcur                                 */
    TFA9872_BF_IEBSTVC= 0x4940,    /*!< Enable DCDC level 1x                               */
    TFA9872_BF_IEBST86= 0x4950,    /*!< Enable DCDC level 1.14x                            */
    TFA9872_BF_IEBST93= 0x4960,    /*!< Enable DCDC level 1.07x                            */
    TFA9872_BF_IEOCPR= 0x4990,    /*!< Enable ocp alarm                                   */
    TFA9872_BF_IEMWSRC= 0x49a0,    /*!< Enable waits HW I2C settings                       */
    TFA9872_BF_IEMWSMU= 0x49c0,    /*!< Enable man Audio mute sequence                     */
    TFA9872_BF_IECLKOOR= 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
    TFA9872_BF_IETDMER= 0x4a00,    /*!< Enable tdm error                                   */
    TFA9872_BF_IECLPR= 0x4a20,    /*!< Enable clip                                        */
    TFA9872_BF_IELP0 = 0x4a40,    /*!< Enable low power mode0                             */
    TFA9872_BF_IELP1 = 0x4a50,    /*!< Enable low power mode1                             */
    TFA9872_BF_IELA  = 0x4a60,    /*!< Enable low noise detection                         */
    TFA9872_BF_IEVDDPH= 0x4a70,    /*!< Enable VDDP greater tehn VBAT                      */
    TFA9872_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA9872_BF_IPOPLLS= 0x4c10,    /*!< Polarity pll lock                                  */
    TFA9872_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9872_BF_IPOOVDS= 0x4c30,    /*!< Polarity OVP alarm                                 */
    TFA9872_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9872_BF_IPOCLKS= 0x4c50,    /*!< Polarity clocks stable                             */
    TFA9872_BF_IPOMTPB= 0x4c60,    /*!< Polarity mtp busy                                  */
    TFA9872_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9872_BF_IPOSWS= 0x4ca0,    /*!< Polarity amplifier engage                          */
    TFA9872_BF_IPOAMPS= 0x4cc0,    /*!< Polarity enbl amp                                  */
    TFA9872_BF_IPOAREFS= 0x4cd0,    /*!< Polarity ref enable                                */
    TFA9872_BF_IPOADCCR= 0x4ce0,    /*!< Polarity Control ADC                               */
    TFA9872_BF_IPOBSTCU= 0x4d00,    /*!< Polarity DCDC current limiting                     */
    TFA9872_BF_IPOBSTHI= 0x4d10,    /*!< Polarity DCDC active                               */
    TFA9872_BF_IPOBSTOC= 0x4d20,    /*!< Polarity DCDC OCP                                  */
    TFA9872_BF_IPOBSTPC= 0x4d30,    /*!< Polarity bst peakcur                               */
    TFA9872_BF_IPOBSTVC= 0x4d40,    /*!< Polarity DCDC level 1x                             */
    TFA9872_BF_IPOBST86= 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
    TFA9872_BF_IPOBST93= 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
    TFA9872_BF_IPOOCPR= 0x4d90,    /*!< Polarity ocp alarm                                 */
    TFA9872_BF_IPOMWSRC= 0x4da0,    /*!< Polarity waits HW I2C settings                     */
    TFA9872_BF_IPOMWSMU= 0x4dc0,    /*!< Polarity man audio mute sequence                   */
    TFA9872_BF_IPCLKOOR= 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
    TFA9872_BF_IPOTDMER= 0x4e00,    /*!< Polarity tdm error                                 */
    TFA9872_BF_IPOCLPR= 0x4e20,    /*!< Polarity clip right                                */
    TFA9872_BF_IPOLP0= 0x4e40,    /*!< Polarity low power mode0                           */
    TFA9872_BF_IPOLP1= 0x4e50,    /*!< Polarity low power mode1                           */
    TFA9872_BF_IPOLA = 0x4e60,    /*!< Polarity low noise mode                            */
    TFA9872_BF_IPOVDDPH= 0x4e70,    /*!< Polarity VDDP greater than VBAT                    */
    TFA9872_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
    TFA9872_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
    TFA9872_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
    TFA9872_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9872_BF_BSSBY = 0x50f0,    /*!< Bypass battery safeguard                           */
    TFA9872_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA9872_BF_INTSMUTE= 0x5110,    /*!< Soft mute HW                                       */
    TFA9872_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9872_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
    TFA9872_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA9872_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9872_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA9872_BF_SLOPESET= 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
    TFA9872_BF_PGAGAIN= 0x6081,    /*!< PGA gain selection                                 */
    TFA9872_BF_PGALPE= 0x60b0,    /*!< Lowpass enable                                     */
    TFA9872_BF_LPM0BYP= 0x6110,    /*!< bypass low power idle mode                         */
    TFA9872_BF_TDMDCG= 0x6123,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
    TFA9872_BF_TDMSPKG= 0x6163,    /*!< Total gain depending on INPLEV setting (channel 0) */
    TFA9872_BF_STIDLEEN= 0x61b0,    /*!< enable idle feature for channel 1                  */
    TFA9872_BF_LNMODE= 0x62e1,    /*!< ctrl select mode                                   */
    TFA9872_BF_LPM1MODE= 0x64e1,    /*!< low power mode control                             */
    TFA9872_BF_LPM1DIS= 0x65c0,    /*!< low power mode1  detector control                  */
    TFA9872_BF_TDMSRCMAP= 0x6801,    /*!< tdm source mapping                                 */
    TFA9872_BF_TDMSRCAS= 0x6821,    /*!< Sensed value  A                                    */
    TFA9872_BF_TDMSRCBS= 0x6841,    /*!< Sensed value  B                                    */
    TFA9872_BF_ANCSEL= 0x6881,    /*!< anc input                                          */
    TFA9872_BF_ANC1C = 0x68a0,    /*!< ANC one s complement                               */
    TFA9872_BF_SAMMODE= 0x6901,    /*!< sam enable                                         */
    TFA9872_BF_SAMSEL= 0x6920,    /*!< sam source                                         */
    TFA9872_BF_PDMOSELH= 0x6931,    /*!< pdm out value when pdm_clk is higth                */
    TFA9872_BF_PDMOSELL= 0x6951,    /*!< pdm out value when pdm_clk is low                  */
    TFA9872_BF_SAMOSEL= 0x6970,    /*!< ram output on mode sam and audio                   */
    TFA9872_BF_LP0   = 0x6e00,    /*!< low power mode 0 detection                         */
    TFA9872_BF_LP1   = 0x6e10,    /*!< low power mode 1 detection                         */
    TFA9872_BF_LA    = 0x6e20,    /*!< low amplitude detection                            */
    TFA9872_BF_VDDPH = 0x6e30,    /*!< vddp greater than vbat                             */
    TFA9872_BF_DELCURCOMP= 0x6f02,    /*!< delay to allign compensation signal with current sense signal */
    TFA9872_BF_SIGCURCOMP= 0x6f40,    /*!< polarity of compensation for current sense         */
    TFA9872_BF_ENCURCOMP= 0x6f50,    /*!< enable current sense compensation                  */
    TFA9872_BF_SELCLPPWM= 0x6f60,    /*!< Select pwm clip flag                               */
    TFA9872_BF_LVLCLPPWM= 0x6f72,    /*!< set the amount of pwm pulse that may be skipped before clip-flag is triggered */
    TFA9872_BF_DCVOS = 0x7002,    /*!< Second boost voltage level                         */
    TFA9872_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA9872_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9872_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA9872_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA9872_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
    TFA9872_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
    TFA9872_BF_DCVOF = 0x7402,    /*!< 1st boost voltage level                            */
    TFA9872_BF_DCTRACK= 0x7430,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
    TFA9872_BF_DCTRIP= 0x7444,    /*!< 1st Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9872_BF_DCHOLD= 0x7494,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
    TFA9872_BF_DCTRIP2= 0x7534,    /*!< 2nd Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9872_BF_DCTRIPT= 0x7584,    /*!< Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9872_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA9872_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9872_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9872_BF_CMTPI = 0xa350,    /*!< Start copying all the data from mtp to I2C mtp registers */
    TFA9872_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9872_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9872_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9872_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9872_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9872_BF_SWPROFIL= 0xee0f,    /*!< Software profile data                              */
    TFA9872_BF_SWVSTEP= 0xef0f,    /*!< Software vstep information                         */
    TFA9872_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9872_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9872_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9872_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9872_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9872_BF_CUSTINFO= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9872_BF_R25C  = 0xf50f,    /*!< Ron resistance of  speaker coil                    */
} nxpTfa9872BfEnumList_t;
#define TFA9872_NAMETABLE static tfaBfName_t Tfa9872DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x30, "AMPE"},    /* Activate Amplifier                                , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0xb0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xc0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0x120, "MANSCONF"},    /* I2C configured                                    , */\
   { 0x140, "MANAOOSC"},    /* Internal osc off at PWDN                          , */\
   { 0x1d0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "RCVNS"},    /* Noise shaper selection                            , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x510, "SSE"},    /* Enable speaker path                               , */\
   { 0x530, "VSE"},    /* Voltage sense                                     , */\
   { 0x550, "CSE"},    /* Current sense                                     , */\
   { 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
   { 0x580, "PGAE"},    /* Enable PGA chop clock                             , */\
   { 0x590, "SSTDME"},    /* Sub-system TDM                                    , */\
   { 0x5a0, "SSPBSTE"},    /* Sub-system boost                                  , */\
   { 0x5b0, "SSADCE"},    /* Sub-system ADC                                    , */\
   { 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
   { 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
   { 0xda0, "STSMUTE"},    /* Side tone soft mute                               , */\
   { 0xdb0, "ST1C"},    /* side tone one s complement                        , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1060, "MTPB"},    /* MTP busy                                          , */\
   { 0x1070, "NOCLK"},    /* Lost clock                                        , */\
   { 0x10a0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x10d0, "AREFS"},    /* References enable                                 , */\
   { 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active                                       , */\
   { 0x1120, "DCOCPOK"},    /* DCDC OCP nmos                                     , */\
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "STMUTE"},    /* side tone mute state                              , */\
   { 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11d0, "TDMERR"},    /* TDM error                                         , */\
   { 0x1300, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1340, "CLIPAH"},    /* Clipping A to Vddp                                , */\
   { 0x1350, "CLIPAL"},    /* Clipping A to gnd                                 , */\
   { 0x1360, "CLIPBH"},    /* Clipping B to Vddp                                , */\
   { 0x1370, "CLIPBL"},    /* Clipping B to gnd                                 , */\
   { 0x1380, "OCDS"},    /* OCP  amplifier                                    , */\
   { 0x1390, "CLIPS"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "OCPOKMC"},    /* OCPOK MICVDD                                      , */\
   { 0x13b0, "MANALARM"},    /* Alarm state                                       , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/9.5 V)                , */\
   { 0x2040, "TDME"},    /* Enable interface                                  , */\
   { 0x2050, "TDMMODE"},    /* Slave/master                                      , */\
   { 0x2060, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "TDMFSLN"},    /* FS length (master mode only)                      , */\
   { 0x20b0, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x20c3, "TDMNBCK"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x2144, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2194, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x21e0, "TDMDEL"},    /* data delay to FS                                  , */\
   { 0x21f0, "TDMADJ"},    /* data adjustment                                   , */\
   { 0x2201, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2224, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2271, "TDMTXDFO"},    /* Format unused bits                                , */\
   { 0x2291, "TDMTXUS0"},    /* Format unused slots DATAO                         , */\
   { 0x2300, "TDMSPKE"},    /* Control audio tdm channel in 0 (spkr + dcdc)      , */\
   { 0x2310, "TDMDCE"},    /* Control audio  tdm channel in 1  (dcdc)           , */\
   { 0x2330, "TDMCSE"},    /* current sense vbat temperature and vddp feedback  , */\
   { 0x2340, "TDMVSE"},    /* Voltage sense vbat temperature and vddp feedback  , */\
   { 0x2603, "TDMSPKS"},    /* tdm slot for sink 0 (speaker + dcdc)              , */\
   { 0x2643, "TDMDCS"},    /* tdm slot for  sink 1  (dcdc)                      , */\
   { 0x26c3, "TDMCSS"},    /* Slot Position of current sense vbat temperature and vddp feedback, */\
   { 0x2703, "TDMVSS"},    /* Slot Position of Voltage sense vbat temperature and vddp feedback, */\
   { 0x3111, "PDMSTSEL"},    /* Side tone input                                   , */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTPLLS"},    /* Status PLL lock                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOVDS"},    /* Status OVP alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTCLKS"},    /* Status clocks stable                              , */\
   { 0x4060, "ISTMTPB"},    /* Status MTP busy                                   , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x40a0, "ISTSWS"},    /* Status amplifier engage                           , */\
   { 0x40c0, "ISTAMPS"},    /* Status amplifier enable                           , */\
   { 0x40d0, "ISTAREFS"},    /* Status Ref enable                                 , */\
   { 0x40e0, "ISTADCCR"},    /* Status Control ADC                                , */\
   { 0x4100, "ISTBSTCU"},    /* Status DCDC current limiting                      , */\
   { 0x4110, "ISTBSTHI"},    /* Status DCDC active                                , */\
   { 0x4120, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4130, "ISTBSTPKCUR"},    /* Status bst peakcur                                , */\
   { 0x4140, "ISTBSTVC"},    /* Status DCDC level 1x                              , */\
   { 0x4150, "ISTBST86"},    /* Status DCDC level 1.14x                           , */\
   { 0x4160, "ISTBST93"},    /* Status DCDC level 1.07x                           , */\
   { 0x4190, "ISTOCPR"},    /* Status ocp alarm                                  , */\
   { 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
   { 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
   { 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4220, "ISTCLPR"},    /* Status clip                                       , */\
   { 0x4240, "ISTLP0"},    /* Status low power mode0                            , */\
   { 0x4250, "ISTLP1"},    /* Status low power mode1                            , */\
   { 0x4260, "ISTLA"},    /* Status low noise detection                        , */\
   { 0x4270, "ISTVDDPH"},    /* Status VDDP greater than VBAT                     , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLPLLS"},    /* Clear PLL lock                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOVDS"},    /* Clear OVP alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLCLKS"},    /* Clear clocks stable                               , */\
   { 0x4460, "ICLMTPB"},    /* Clear mtp busy                                    , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x44a0, "ICLSWS"},    /* Clear amplifier engage                            , */\
   { 0x44c0, "ICLAMPS"},    /* Clear enbl amp                                    , */\
   { 0x44d0, "ICLAREFS"},    /* Clear ref enable                                  , */\
   { 0x44e0, "ICLADCCR"},    /* Clear control ADC                                 , */\
   { 0x4500, "ICLBSTCU"},    /* Clear DCDC current limiting                       , */\
   { 0x4510, "ICLBSTHI"},    /* Clear DCDC active                                 , */\
   { 0x4520, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4530, "ICLBSTPC"},    /* Clear bst peakcur                                 , */\
   { 0x4540, "ICLBSTVC"},    /* Clear DCDC level 1x                               , */\
   { 0x4550, "ICLBST86"},    /* Clear DCDC level 1.14x                            , */\
   { 0x4560, "ICLBST93"},    /* Clear DCDC level 1.07x                            , */\
   { 0x4590, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
   { 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
   { 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
   { 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4620, "ICLCLPR"},    /* Clear clip                                        , */\
   { 0x4640, "ICLLP0"},    /* Clear low power mode0                             , */\
   { 0x4650, "ICLLP1"},    /* Clear low power mode1                             , */\
   { 0x4660, "ICLLA"},    /* Clear low noise detection                         , */\
   { 0x4670, "ICLVDDPH"},    /* Clear VDDP greater then VBAT                      , */\
   { 0x4800, "IEVDDS"},    /* Enable por                                        , */\
   { 0x4810, "IEPLLS"},    /* Enable pll lock                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOVDS"},    /* Enable OVP alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IECLKS"},    /* Enable clocks stable                              , */\
   { 0x4860, "IEMTPB"},    /* Enable mtp busy                                   , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x48a0, "IESWS"},    /* Enable amplifier engage                           , */\
   { 0x48c0, "IEAMPS"},    /* Enable enbl amp                                   , */\
   { 0x48d0, "IEAREFS"},    /* Enable ref enable                                 , */\
   { 0x48e0, "IEADCCR"},    /* Enable Control ADC                                , */\
   { 0x4900, "IEBSTCU"},    /* Enable DCDC current limiting                      , */\
   { 0x4910, "IEBSTHI"},    /* Enable DCDC active                                , */\
   { 0x4920, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4930, "IEBSTPC"},    /* Enable bst peakcur                                , */\
   { 0x4940, "IEBSTVC"},    /* Enable DCDC level 1x                              , */\
   { 0x4950, "IEBST86"},    /* Enable DCDC level 1.14x                           , */\
   { 0x4960, "IEBST93"},    /* Enable DCDC level 1.07x                           , */\
   { 0x4990, "IEOCPR"},    /* Enable ocp alarm                                  , */\
   { 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
   { 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
   { 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4a20, "IECLPR"},    /* Enable clip                                       , */\
   { 0x4a40, "IELP0"},    /* Enable low power mode0                            , */\
   { 0x4a50, "IELP1"},    /* Enable low power mode1                            , */\
   { 0x4a60, "IELA"},    /* Enable low noise detection                        , */\
   { 0x4a70, "IEVDDPH"},    /* Enable VDDP greater tehn VBAT                     , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
   { 0x4c10, "IPOPLLS"},    /* Polarity pll lock                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOVDS"},    /* Polarity OVP alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOCLKS"},    /* Polarity clocks stable                            , */\
   { 0x4c60, "IPOMTPB"},    /* Polarity mtp busy                                 , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x4ca0, "IPOSWS"},    /* Polarity amplifier engage                         , */\
   { 0x4cc0, "IPOAMPS"},    /* Polarity enbl amp                                 , */\
   { 0x4cd0, "IPOAREFS"},    /* Polarity ref enable                               , */\
   { 0x4ce0, "IPOADCCR"},    /* Polarity Control ADC                              , */\
   { 0x4d00, "IPOBSTCU"},    /* Polarity DCDC current limiting                    , */\
   { 0x4d10, "IPOBSTHI"},    /* Polarity DCDC active                              , */\
   { 0x4d20, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4d30, "IPOBSTPC"},    /* Polarity bst peakcur                              , */\
   { 0x4d40, "IPOBSTVC"},    /* Polarity DCDC level 1x                            , */\
   { 0x4d50, "IPOBST86"},    /* Polarity DCDC level 1.14x                         , */\
   { 0x4d60, "IPOBST93"},    /* Polarity DCDC level 1.07x                         , */\
   { 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
   { 0x4df0, "IPCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4e20, "IPOCLPR"},    /* Polarity clip right                               , */\
   { 0x4e40, "IPOLP0"},    /* Polarity low power mode0                          , */\
   { 0x4e50, "IPOLP1"},    /* Polarity low power mode1                          , */\
   { 0x4e60, "IPOLA"},    /* Polarity low noise mode                           , */\
   { 0x4e70, "IPOVDDPH"},    /* Polarity VDDP greater than VBAT                   , */\
   { 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass battery safeguard                          , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
   { 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
   { 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x6081, "PGAGAIN"},    /* PGA gain selection                                , */\
   { 0x60b0, "PGALPE"},    /* Lowpass enable                                    , */\
   { 0x6110, "LPM0BYP"},    /* bypass low power idle mode                        , */\
   { 0x6123, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x6163, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x61b0, "STIDLEEN"},    /* enable idle feature for channel 1                 , */\
   { 0x62e1, "LNMODE"},    /* ctrl select mode                                  , */\
   { 0x64e1, "LPM1MODE"},    /* low power mode control                            , */\
   { 0x65c0, "LPM1DIS"},    /* low power mode1  detector control                 , */\
   { 0x6801, "TDMSRCMAP"},    /* tdm source mapping                                , */\
   { 0x6821, "TDMSRCAS"},    /* Sensed value  A                                   , */\
   { 0x6841, "TDMSRCBS"},    /* Sensed value  B                                   , */\
   { 0x6881, "ANCSEL"},    /* anc input                                         , */\
   { 0x68a0, "ANC1C"},    /* ANC one s complement                              , */\
   { 0x6901, "SAMMODE"},    /* sam enable                                        , */\
   { 0x6920, "SAMSEL"},    /* sam source                                        , */\
   { 0x6931, "PDMOSELH"},    /* pdm out value when pdm_clk is higth               , */\
   { 0x6951, "PDMOSELL"},    /* pdm out value when pdm_clk is low                 , */\
   { 0x6970, "SAMOSEL"},    /* ram output on mode sam and audio                  , */\
   { 0x6e00, "LP0"},    /* low power mode 0 detection                        , */\
   { 0x6e10, "LP1"},    /* low power mode 1 detection                        , */\
   { 0x6e20, "LA"},    /* low amplitude detection                           , */\
   { 0x6e30, "VDDPH"},    /* vddp greater than vbat                            , */\
   { 0x6f02, "DELCURCOMP"},    /* delay to allign compensation signal with current sense signal, */\
   { 0x6f40, "SIGCURCOMP"},    /* polarity of compensation for current sense        , */\
   { 0x6f50, "ENCURCOMP"},    /* enable current sense compensation                 , */\
   { 0x6f60, "SELCLPPWM"},    /* Select pwm clip flag                              , */\
   { 0x6f72, "LVLCLPPWM"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7002, "DCVOS"},    /* Second boost voltage level                        , */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
   { 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7402, "DCVOF"},    /* 1st boost voltage level                           , */\
   { 0x7430, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7444, "DCTRIP"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7494, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x7534, "DCTRIP2"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7584, "DCTRIPT"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0xa107, "MTPK"},    /* MTP KEY2 register                                 , */\
   { 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
   { 0xa350, "CMTPI"},    /* Start copying all the data from mtp to I2C mtp registers, */\
   { 0xa360, "CIMTP"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0xa50f, "MTPRDMSB"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "MTPRDLSB"},    /* LSB word of MTP manual read data                  , */\
   { 0xb108, "EXTTS"},    /* External temperature (C)                          , */\
   { 0xb190, "TROS"},    /* Select temp Speaker calibration                   , */\
   { 0xee0f, "SWPROFIL"},    /* Software profile data                             , */\
   { 0xef0f, "SWVSTEP"},    /* Software vstep information                        , */\
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "CUSTINFO"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf50f, "R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum TFA9872_irq {
	TFA9872_irq_stvdds = 0,
	TFA9872_irq_stplls = 1,
	TFA9872_irq_stotds = 2,
	TFA9872_irq_stovds = 3,
	TFA9872_irq_stuvds = 4,
	TFA9872_irq_stclks = 5,
	TFA9872_irq_stmtpb = 6,
	TFA9872_irq_stnoclk = 7,
	TFA9872_irq_stsws = 10,
	TFA9872_irq_stamps = 12,
	TFA9872_irq_starefs = 13,
	TFA9872_irq_stadccr = 14,
	TFA9872_irq_stbstcu = 16,
	TFA9872_irq_stbsthi = 17,
	TFA9872_irq_stbstoc = 18,
	TFA9872_irq_stbstpkcur = 19,
	TFA9872_irq_stbstvc = 20,
	TFA9872_irq_stbst86 = 21,
	TFA9872_irq_stbst93 = 22,
	TFA9872_irq_stocpr = 25,
	TFA9872_irq_stmwsrc = 26,
	TFA9872_irq_stmwsmu = 28,
	TFA9872_irq_stclkoor = 31,
	TFA9872_irq_sttdmer = 32,
	TFA9872_irq_stclpr = 34,
	TFA9872_irq_stlp0 = 36,
	TFA9872_irq_stlp1 = 37,
	TFA9872_irq_stla = 38,
	TFA9872_irq_stvddph = 39,
	TFA9872_irq_max = 40,
	TFA9872_irq_all = -1 /* all irqs */};

#define TFA9872_IRQ_NAMETABLE static tfaIrqName_t Tfa9872IrqNames[]= {\
	{ 0, "STVDDS"},\
	{ 1, "STPLLS"},\
	{ 2, "STOTDS"},\
	{ 3, "STOVDS"},\
	{ 4, "STUVDS"},\
	{ 5, "STCLKS"},\
	{ 6, "STMTPB"},\
	{ 7, "STNOCLK"},\
	{ 8, "8"},\
	{ 9, "9"},\
	{ 10, "STSWS"},\
	{ 11, "11"},\
	{ 12, "STAMPS"},\
	{ 13, "STAREFS"},\
	{ 14, "STADCCR"},\
	{ 15, "15"},\
	{ 16, "STBSTCU"},\
	{ 17, "STBSTHI"},\
	{ 18, "STBSTOC"},\
	{ 19, "STBSTPKCUR"},\
	{ 20, "STBSTVC"},\
	{ 21, "STBST86"},\
	{ 22, "STBST93"},\
	{ 23, "23"},\
	{ 24, "24"},\
	{ 25, "STOCPR"},\
	{ 26, "STMWSRC"},\
	{ 27, "27"},\
	{ 28, "STMWSMU"},\
	{ 29, "29"},\
	{ 30, "30"},\
	{ 31, "STCLKOOR"},\
	{ 32, "STTDMER"},\
	{ 33, "33"},\
	{ 34, "STCLPR"},\
	{ 35, "35"},\
	{ 36, "STLP0"},\
	{ 37, "STLP1"},\
	{ 38, "STLA"},\
	{ 39, "STVDDPH"},\
	{ 40, "40"},\
};
#endif /* _TFA9872_TFAFIELDNAMES_H */
