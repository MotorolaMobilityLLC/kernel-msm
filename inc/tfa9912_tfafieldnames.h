/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9912_TFAFIELDNAMES_H
#define _TFA9912_TFAFIELDNAMES_H

#define TFA9912_I2CVERSION    1.41

typedef enum nxpTfa9912BfEnumList {
    TFA9912_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9912_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9912_BF_CFE   = 0x0020,    /*!< Enable CoolFlux                                    */
    TFA9912_BF_AMPE  = 0x0030,    /*!< Enables the Amplifier                              */
    TFA9912_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9912_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA9912_BF_AMPC  = 0x0060,    /*!< CoolFlux controls amplifier                        */
    TFA9912_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9912_BF_FSSSEL= 0x0090,    /*!< Audio sample reference                             */
    TFA9912_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9912_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA9912_BF_AMPINSEL= 0x0101,    /*!< Amplifier input selection                          */
    TFA9912_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9912_BF_MANCOLD= 0x0130,    /*!< Execute cold start                                 */
    TFA9912_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA9912_BF_MANROBOD= 0x0150,    /*!< Reaction on BOD                                    */
    TFA9912_BF_BODE  = 0x0160,    /*!< BOD Enable                                         */
    TFA9912_BF_BODHYS= 0x0170,    /*!< BOD Hysteresis                                     */
    TFA9912_BF_BODFILT= 0x0181,    /*!< BOD filter                                         */
    TFA9912_BF_BODTHLVL= 0x01a1,    /*!< BOD threshold                                      */
    TFA9912_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA9912_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
    TFA9912_BF_MANWDE= 0x01f0,    /*!< Watchdog enable                                    */
    TFA9912_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA9912_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9912_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA9912_BF_BYPHVBF= 0x02b0,    /*!< Bypass HVBAT filter                                */
    TFA9912_BF_TDMC  = 0x02c0,    /*!< TDM Compatibility with TFA9872                     */
    TFA9912_BF_ENBLADC10= 0x02e0,    /*!< ADC10 Enable -  I2C direct mode                    */
    TFA9912_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9912_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9912_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA9912_BF_ENCFCKSEL= 0x0430,    /*!< Coolflux DSP clock scaling, low power mode         */
    TFA9912_BF_CFCKSEL= 0x0441,    /*!< Coolflux DSP clock scaler selection for low power mode */
    TFA9912_BF_TDMINFSEL= 0x0460,    /*!< TDM clock selection                                */
    TFA9912_BF_DISBLAUTOCLKSEL= 0x0470,    /*!< Disable Automatic dsp clock source selection       */
    TFA9912_BF_SELCLKSRC= 0x0480,    /*!< I2C selection of DSP clock when auto select is disabled */
    TFA9912_BF_SELTIMSRC= 0x0490,    /*!< I2C selection of Watchdog and Timer clock          */
    TFA9912_BF_SSLEFTE= 0x0500,    /*!<                                                    */
    TFA9912_BF_SPKSSEN= 0x0510,    /*!< Enable speaker path                                */
    TFA9912_BF_VSLEFTE= 0x0520,    /*!<                                                    */
    TFA9912_BF_VSRIGHTE= 0x0530,    /*!< Voltage sense                                      */
    TFA9912_BF_CSLEFTE= 0x0540,    /*!<                                                    */
    TFA9912_BF_CSRIGHTE= 0x0550,    /*!< Current sense                                      */
    TFA9912_BF_SSPDME= 0x0560,    /*!< Sub-system PDM                                     */
    TFA9912_BF_PGALE = 0x0570,    /*!< Enable PGA chop clock for left channel             */
    TFA9912_BF_PGARE = 0x0580,    /*!< Enable PGA chop clock                              */
    TFA9912_BF_SSTDME= 0x0590,    /*!< Sub-system TDM                                     */
    TFA9912_BF_SSPBSTE= 0x05a0,    /*!< Sub-system boost                                   */
    TFA9912_BF_SSADCE= 0x05b0,    /*!< Sub-system ADC                                     */
    TFA9912_BF_SSFAIME= 0x05c0,    /*!< Sub-system FAIM                                    */
    TFA9912_BF_SSCFTIME= 0x05d0,    /*!< CF Sub-system timer                                */
    TFA9912_BF_SSCFWDTE= 0x05e0,    /*!< CF Sub-system WDT                                  */
    TFA9912_BF_FAIMVBGOVRRL= 0x05f0,    /*!< Over rule of vbg for FaIM access                   */
    TFA9912_BF_SAMSPKSEL= 0x0600,    /*!< Input selection for TAP/SAM                        */
    TFA9912_BF_PDM2IISEN= 0x0610,    /*!< PDM2IIS Bridge enable                              */
    TFA9912_BF_TAPRSTBYPASS= 0x0620,    /*!< Tap decimator reset bypass - Bypass the decimator reset from tapdec */
    TFA9912_BF_CARDECISEL0= 0x0631,    /*!< Cardec input 0 sel                                 */
    TFA9912_BF_CARDECISEL1= 0x0651,    /*!< Cardec input sel                                   */
    TFA9912_BF_TAPDECSEL= 0x0670,    /*!< Select TAP/Cardec for TAP                          */
    TFA9912_BF_COMPCOUNT= 0x0680,    /*!< Comparator o/p filter selection                    */
    TFA9912_BF_STARTUPMODE= 0x0691,    /*!< Startup Mode Selection                             */
    TFA9912_BF_AUTOTAP= 0x06b0,    /*!< Enable auto tap switching                          */
    TFA9912_BF_COMPINITIME= 0x06c1,    /*!< Comparator initialization time to be used in Tap Machine */
    TFA9912_BF_ANAPINITIME= 0x06e1,    /*!< Analog initialization time to be used in Tap Machine */
    TFA9912_BF_CCHKTH= 0x0707,    /*!< Clock check Higher Threshold                       */
    TFA9912_BF_CCHKTL= 0x0787,    /*!< Clock check Higher Threshold                       */
    TFA9912_BF_AMPOCRT= 0x0802,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9912_BF_AMPTCRR= 0x0832,    /*!< Amplifier on-off criteria for tap mode entry       */
    TFA9912_BF_STGS  = 0x0d00,    /*!< PDM side tone gain selector                        */
    TFA9912_BF_STGAIN= 0x0d18,    /*!< Side tone gain                                     */
    TFA9912_BF_STSMUTE= 0x0da0,    /*!< Side tone soft mute                                */
    TFA9912_BF_ST1C  = 0x0db0,    /*!< side tone one s complement                         */
    TFA9912_BF_CMFBEL= 0x0e80,    /*!< CMFB enable left                                   */
    TFA9912_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9912_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
    TFA9912_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA9912_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA9912_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA9912_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
    TFA9912_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
    TFA9912_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
    TFA9912_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA9912_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
    TFA9912_BF_WDS   = 0x10b0,    /*!< Watchdog                                           */
    TFA9912_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA9912_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA9912_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA9912_BF_BODNOK= 0x10f0,    /*!< BOD                                                */
    TFA9912_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9912_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
    TFA9912_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos                                      */
    TFA9912_BF_DCPEAKCUR= 0x1130,    /*!< Indicates current is max in DC-to-DC converter     */
    TFA9912_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA9912_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA9912_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA9912_BF_STMUTEB= 0x1170,    /*!< side tone (un)mute busy                            */
    TFA9912_BF_STMUTE= 0x1180,    /*!< side tone mute state                               */
    TFA9912_BF_TDMLUTER= 0x1190,    /*!< TDM LUT error                                      */
    TFA9912_BF_TDMSTAT= 0x11a2,    /*!< TDM status bits                                    */
    TFA9912_BF_TDMERR= 0x11d0,    /*!< TDM error                                          */
    TFA9912_BF_HAPTIC= 0x11e0,    /*!< Status haptic driver                               */
    TFA9912_BF_OCPOAP= 0x1300,    /*!< OCPOK pmos A                                       */
    TFA9912_BF_OCPOAN= 0x1310,    /*!< OCPOK nmos A                                       */
    TFA9912_BF_OCPOBP= 0x1320,    /*!< OCPOK pmos B                                       */
    TFA9912_BF_OCPOBN= 0x1330,    /*!< OCPOK nmos B                                       */
    TFA9912_BF_CLIPAH= 0x1340,    /*!< Clipping A to Vddp                                 */
    TFA9912_BF_CLIPAL= 0x1350,    /*!< Clipping A to gnd                                  */
    TFA9912_BF_CLIPBH= 0x1360,    /*!< Clipping B to Vddp                                 */
    TFA9912_BF_CLIPBL= 0x1370,    /*!< Clipping B to gnd                                  */
    TFA9912_BF_OCDS  = 0x1380,    /*!< OCP  amplifier                                     */
    TFA9912_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
    TFA9912_BF_TCMPTRG= 0x13a0,    /*!< Status Tap comparator triggered                    */
    TFA9912_BF_TAPDET= 0x13b0,    /*!< Status Tap  detected                               */
    TFA9912_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA9912_BF_MANWAIT2= 0x13d0,    /*!< Wait CF config                                     */
    TFA9912_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA9912_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA9912_BF_SPKSL = 0x1400,    /*!< Left speaker status                                */
    TFA9912_BF_SPKS  = 0x1410,    /*!< Speaker status                                     */
    TFA9912_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA9912_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA9912_BF_DCMODE= 0x1471,    /*!< DCDC mode status bits                              */
    TFA9912_BF_DSPCLKSRC= 0x1490,    /*!< DSP clock source selected by manager               */
    TFA9912_BF_STARTUPMODSTAT= 0x14a1,    /*!< Startup Mode Selected by Manager(Read Only)        */
    TFA9912_BF_TSPMSTATE= 0x14c3,    /*!< Tap Machine State                                  */
    TFA9912_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9912_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9912_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/13 V)                  */
    TFA9912_BF_DCILCF= 0x17a0,    /*!< DCDC current limiting for DSP                      */
    TFA9912_BF_TDMUC = 0x2000,    /*!< Mode setting                                       */
    TFA9912_BF_DIO4SEL= 0x2011,    /*!< DIO4 Input selection                               */
    TFA9912_BF_TDME  = 0x2040,    /*!< Enable TDM interface                               */
    TFA9912_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA9912_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA9912_BF_TDMFSLN= 0x2073,    /*!< FS length                                          */
    TFA9912_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA9912_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9912_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA9912_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA9912_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA9912_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA9912_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA9912_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA9912_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA9912_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits in a slot                       */
    TFA9912_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots GAINIO                         */
    TFA9912_BF_TDMTXUS1= 0x22b1,    /*!< Format unused slots DIO1                           */
    TFA9912_BF_TDMTXUS2= 0x22d1,    /*!< Format unused slots DIO2                           */
    TFA9912_BF_TDMGIE= 0x2300,    /*!< Control gain (channel in 0)                        */
    TFA9912_BF_TDMDCE= 0x2310,    /*!< Control audio  left (channel in 1 )                */
    TFA9912_BF_TDMSPKE= 0x2320,    /*!< Control audio right (channel in 2 )                */
    TFA9912_BF_TDMCSE= 0x2330,    /*!< Current sense                                      */
    TFA9912_BF_TDMVSE= 0x2340,    /*!< Voltage sense                                      */
    TFA9912_BF_TDMGOE= 0x2350,    /*!< DSP Gainout                                        */
    TFA9912_BF_TDMCF2E= 0x2360,    /*!< DSP 2                                              */
    TFA9912_BF_TDMCF3E= 0x2370,    /*!< DSP 3                                              */
    TFA9912_BF_TDMCFE= 0x2380,    /*!< DSP                                                */
    TFA9912_BF_TDMES6= 0x2390,    /*!< Loopback of Audio left (channel 1)                 */
    TFA9912_BF_TDMES7= 0x23a0,    /*!< Loopback of Audio right (channel 2)                */
    TFA9912_BF_TDMCF4E= 0x23b0,    /*!< AEC ref right control                              */
    TFA9912_BF_TDMPD1E= 0x23c0,    /*!< PDM 1 control                                      */
    TFA9912_BF_TDMPD2E= 0x23d0,    /*!< PDM 2 control                                      */
    TFA9912_BF_TDMGIN= 0x2401,    /*!< IO gainin                                          */
    TFA9912_BF_TDMLIO= 0x2421,    /*!< IO audio left                                      */
    TFA9912_BF_TDMRIO= 0x2441,    /*!< IO audio right                                     */
    TFA9912_BF_TDMCSIO= 0x2461,    /*!< IO Current Sense                                   */
    TFA9912_BF_TDMVSIO= 0x2481,    /*!< IO voltage sense                                   */
    TFA9912_BF_TDMGOIO= 0x24a1,    /*!< IO gain out                                        */
    TFA9912_BF_TDMCFIO2= 0x24c1,    /*!< IO DSP 2                                           */
    TFA9912_BF_TDMCFIO3= 0x24e1,    /*!< IO DSP 3                                           */
    TFA9912_BF_TDMCFIO= 0x2501,    /*!< IO DSP                                             */
    TFA9912_BF_TDMLPB6= 0x2521,    /*!< IO Source 6                                        */
    TFA9912_BF_TDMLPB7= 0x2541,    /*!< IO Source 7                                        */
    TFA9912_BF_TDMGS = 0x2603,    /*!< Control gainin                                     */
    TFA9912_BF_TDMDCS= 0x2643,    /*!< tdm slot for audio left (channel 1)                */
    TFA9912_BF_TDMSPKS= 0x2683,    /*!< tdm slot for audio right (channel 2)               */
    TFA9912_BF_TDMCSS= 0x26c3,    /*!< Slot Position of Current Sense Out                 */
    TFA9912_BF_TDMVSS= 0x2703,    /*!< Slot Position of Voltage sense                     */
    TFA9912_BF_TDMCGOS= 0x2743,    /*!< Slot Position of GAIN out                          */
    TFA9912_BF_TDMCF2S= 0x2783,    /*!< Slot Position DSPout2                              */
    TFA9912_BF_TDMCF3S= 0x27c3,    /*!< Slot Position DSPout3                              */
    TFA9912_BF_TDMCFS= 0x2803,    /*!< Slot Position of DSPout                            */
    TFA9912_BF_TDMEDAT6S= 0x2843,    /*!< Slot Position of loopback channel left             */
    TFA9912_BF_TDMEDAT7S= 0x2883,    /*!< Slot Position of loopback channel right            */
    TFA9912_BF_TDMTXUS3= 0x2901,    /*!< Format unused slots D3                             */
    TFA9912_BF_PDMSM = 0x3100,    /*!< PDM control                                        */
    TFA9912_BF_PDMSTSEL= 0x3110,    /*!< PDM Decimator input selection                      */
    TFA9912_BF_PDMSTENBL= 0x3120,    /*!< Side tone input enable                             */
    TFA9912_BF_PDMLSEL= 0x3130,    /*!< PDM data selection for left channel during PDM direct mode */
    TFA9912_BF_PDMRSEL= 0x3140,    /*!< PDM data selection for right channel during PDM direct mode */
    TFA9912_BF_MICVDDE= 0x3150,    /*!< Enable MICVDD                                      */
    TFA9912_BF_PDMCLRAT= 0x3201,    /*!< PDM BCK/Fs ratio                                   */
    TFA9912_BF_PDMGAIN= 0x3223,    /*!< PDM gain                                           */
    TFA9912_BF_PDMOSEL= 0x3263,    /*!< PDM output selection - RE/FE data combination      */
    TFA9912_BF_SELCFHAPD= 0x32a0,    /*!< Select the source for haptic data output (not for customer) */
    TFA9912_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9912_BF_ISTPLLS= 0x4010,    /*!< Status PLL lock                                    */
    TFA9912_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9912_BF_ISTOVDS= 0x4030,    /*!< Status OVP alarm                                   */
    TFA9912_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9912_BF_ISTCLKS= 0x4050,    /*!< Status clocks stable                               */
    TFA9912_BF_ISTMTPB= 0x4060,    /*!< Status MTP busy                                    */
    TFA9912_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9912_BF_ISTSPKS= 0x4080,    /*!< Status speaker error                               */
    TFA9912_BF_ISTACS= 0x4090,    /*!< Status cold start                                  */
    TFA9912_BF_ISTSWS= 0x40a0,    /*!< Status amplifier engage                            */
    TFA9912_BF_ISTWDS= 0x40b0,    /*!< Status watchdog                                    */
    TFA9912_BF_ISTAMPS= 0x40c0,    /*!< Status amplifier enable                            */
    TFA9912_BF_ISTAREFS= 0x40d0,    /*!< Status Ref enable                                  */
    TFA9912_BF_ISTADCCR= 0x40e0,    /*!< Status Control ADC                                 */
    TFA9912_BF_ISTBODNOK= 0x40f0,    /*!< Status BOD                                         */
    TFA9912_BF_ISTBSTCU= 0x4100,    /*!< Status DCDC current limiting                       */
    TFA9912_BF_ISTBSTHI= 0x4110,    /*!< Status DCDC active                                 */
    TFA9912_BF_ISTBSTOC= 0x4120,    /*!< Status DCDC OCP                                    */
    TFA9912_BF_ISTBSTPKCUR= 0x4130,    /*!< Status bst peakcur                                 */
    TFA9912_BF_ISTBSTVC= 0x4140,    /*!< Status DCDC level 1x                               */
    TFA9912_BF_ISTBST86= 0x4150,    /*!< Status DCDC level 1.14x                            */
    TFA9912_BF_ISTBST93= 0x4160,    /*!< Status DCDC level 1.07x                            */
    TFA9912_BF_ISTRCVLD= 0x4170,    /*!< Status rcvldop ready                               */
    TFA9912_BF_ISTOCPL= 0x4180,    /*!< Status ocp alarm left                              */
    TFA9912_BF_ISTOCPR= 0x4190,    /*!< Status ocp alarm                                   */
    TFA9912_BF_ISTMWSRC= 0x41a0,    /*!< Status Waits HW I2C settings                       */
    TFA9912_BF_ISTMWCFC= 0x41b0,    /*!< Status waits CF config                             */
    TFA9912_BF_ISTMWSMU= 0x41c0,    /*!< Status Audio mute sequence                         */
    TFA9912_BF_ISTCFMER= 0x41d0,    /*!< Status cfma error                                  */
    TFA9912_BF_ISTCFMAC= 0x41e0,    /*!< Status cfma ack                                    */
    TFA9912_BF_ISTCLKOOR= 0x41f0,    /*!< Status flag_clk_out_of_range                       */
    TFA9912_BF_ISTTDMER= 0x4200,    /*!< Status tdm error                                   */
    TFA9912_BF_ISTCLPL= 0x4210,    /*!< Status clip left                                   */
    TFA9912_BF_ISTCLPR= 0x4220,    /*!< Status clip                                        */
    TFA9912_BF_ISTOCPM= 0x4230,    /*!< Status mic ocpok                                   */
    TFA9912_BF_ISTLP1= 0x4250,    /*!< Status low power mode1                             */
    TFA9912_BF_ISTLA = 0x4260,    /*!< Status low amplitude detection                     */
    TFA9912_BF_ISTVDDP= 0x4270,    /*!< Status VDDP greater than VBAT                      */
    TFA9912_BF_ISTTAPDET= 0x4280,    /*!< Status Tap  detected                               */
    TFA9912_BF_ISTAUDMOD= 0x4290,    /*!< Status Audio Mode activated                        */
    TFA9912_BF_ISTSAMMOD= 0x42a0,    /*!< Status SAM Mode activated                          */
    TFA9912_BF_ISTTAPMOD= 0x42b0,    /*!< Status Tap  Mode Activated                         */
    TFA9912_BF_ISTTAPTRG= 0x42c0,    /*!< Status Tap comparator triggered                    */
    TFA9912_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9912_BF_ICLPLLS= 0x4410,    /*!< Clear PLL lock                                     */
    TFA9912_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9912_BF_ICLOVDS= 0x4430,    /*!< Clear OVP alarm                                    */
    TFA9912_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9912_BF_ICLCLKS= 0x4450,    /*!< Clear clocks stable                                */
    TFA9912_BF_ICLMTPB= 0x4460,    /*!< Clear mtp busy                                     */
    TFA9912_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9912_BF_ICLSPKS= 0x4480,    /*!< Clear speaker error                                */
    TFA9912_BF_ICLACS= 0x4490,    /*!< Clear cold started                                 */
    TFA9912_BF_ICLSWS= 0x44a0,    /*!< Clear amplifier engage                             */
    TFA9912_BF_ICLWDS= 0x44b0,    /*!< Clear watchdog                                     */
    TFA9912_BF_ICLAMPS= 0x44c0,    /*!< Clear enbl amp                                     */
    TFA9912_BF_ICLAREFS= 0x44d0,    /*!< Clear ref enable                                   */
    TFA9912_BF_ICLADCCR= 0x44e0,    /*!< Clear control ADC                                  */
    TFA9912_BF_ICLBODNOK= 0x44f0,    /*!< Clear BOD                                          */
    TFA9912_BF_ICLBSTCU= 0x4500,    /*!< Clear DCDC current limiting                        */
    TFA9912_BF_ICLBSTHI= 0x4510,    /*!< Clear DCDC active                                  */
    TFA9912_BF_ICLBSTOC= 0x4520,    /*!< Clear DCDC OCP                                     */
    TFA9912_BF_ICLBSTPC= 0x4530,    /*!< Clear bst peakcur                                  */
    TFA9912_BF_ICLBSTVC= 0x4540,    /*!< Clear DCDC level 1x                                */
    TFA9912_BF_ICLBST86= 0x4550,    /*!< Clear DCDC level 1.14x                             */
    TFA9912_BF_ICLBST93= 0x4560,    /*!< Clear DCDC level 1.07x                             */
    TFA9912_BF_ICLRCVLD= 0x4570,    /*!< Clear rcvldop ready                                */
    TFA9912_BF_ICLOCPL= 0x4580,    /*!< Clear ocp alarm left                               */
    TFA9912_BF_ICLOCPR= 0x4590,    /*!< Clear ocp alarm                                    */
    TFA9912_BF_ICLMWSRC= 0x45a0,    /*!< Clear wait HW I2C settings                         */
    TFA9912_BF_ICLMWCFC= 0x45b0,    /*!< Clear wait cf config                               */
    TFA9912_BF_ICLMWSMU= 0x45c0,    /*!< Clear audio mute sequence                          */
    TFA9912_BF_ICLCFMER= 0x45d0,    /*!< Clear cfma err                                     */
    TFA9912_BF_ICLCFMAC= 0x45e0,    /*!< Clear cfma ack                                     */
    TFA9912_BF_ICLCLKOOR= 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
    TFA9912_BF_ICLTDMER= 0x4600,    /*!< Clear tdm error                                    */
    TFA9912_BF_ICLCLPL= 0x4610,    /*!< Clear clip left                                    */
    TFA9912_BF_ICLCLP= 0x4620,    /*!< Clear clip                                         */
    TFA9912_BF_ICLOCPM= 0x4630,    /*!< Clear mic ocpok                                    */
    TFA9912_BF_ICLLP1= 0x4650,    /*!< Clear low power mode1                              */
    TFA9912_BF_ICLLA = 0x4660,    /*!< Clear low amplitude detection                      */
    TFA9912_BF_ICLVDDP= 0x4670,    /*!< Clear VDDP greater then VBAT                       */
    TFA9912_BF_ICLTAPDET= 0x4680,    /*!< Clear Tap  detected                                */
    TFA9912_BF_ICLAUDMOD= 0x4690,    /*!< Clear Audio Mode activated                         */
    TFA9912_BF_ICLSAMMOD= 0x46a0,    /*!< Clear SAM Mode activated                           */
    TFA9912_BF_ICLTAPMOD= 0x46b0,    /*!< Clear Tap  Mode Activated                          */
    TFA9912_BF_ICLTAPTRG= 0x46c0,    /*!< Clear Comparator Interrupt                         */
    TFA9912_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA9912_BF_IEPLLS= 0x4810,    /*!< Enable pll lock                                    */
    TFA9912_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9912_BF_IEOVDS= 0x4830,    /*!< Enable OVP alarm                                   */
    TFA9912_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9912_BF_IECLKS= 0x4850,    /*!< Enable clocks stable                               */
    TFA9912_BF_IEMTPB= 0x4860,    /*!< Enable mtp busy                                    */
    TFA9912_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9912_BF_IESPKS= 0x4880,    /*!< Enable speaker error                               */
    TFA9912_BF_IEACS = 0x4890,    /*!< Enable cold started                                */
    TFA9912_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
    TFA9912_BF_IEWDS = 0x48b0,    /*!< Enable watchdog                                    */
    TFA9912_BF_IEAMPS= 0x48c0,    /*!< Enable enbl amp                                    */
    TFA9912_BF_IEAREFS= 0x48d0,    /*!< Enable ref enable                                  */
    TFA9912_BF_IEADCCR= 0x48e0,    /*!< Enable Control ADC                                 */
    TFA9912_BF_IEBODNOK= 0x48f0,    /*!< Enable BOD                                         */
    TFA9912_BF_IEBSTCU= 0x4900,    /*!< Enable DCDC current limiting                       */
    TFA9912_BF_IEBSTHI= 0x4910,    /*!< Enable DCDC active                                 */
    TFA9912_BF_IEBSTOC= 0x4920,    /*!< Enable DCDC OCP                                    */
    TFA9912_BF_IEBSTPC= 0x4930,    /*!< Enable bst peakcur                                 */
    TFA9912_BF_IEBSTVC= 0x4940,    /*!< Enable DCDC level 1x                               */
    TFA9912_BF_IEBST86= 0x4950,    /*!< Enable DCDC level 1.14x                            */
    TFA9912_BF_IEBST93= 0x4960,    /*!< Enable DCDC level 1.07x                            */
    TFA9912_BF_IERCVLD= 0x4970,    /*!< Enable rcvldop ready                               */
    TFA9912_BF_IEOCPL= 0x4980,    /*!< Enable ocp alarm left                              */
    TFA9912_BF_IEOCPR= 0x4990,    /*!< Enable ocp alarm                                   */
    TFA9912_BF_IEMWSRC= 0x49a0,    /*!< Enable waits HW I2C settings                       */
    TFA9912_BF_IEMWCFC= 0x49b0,    /*!< Enable man wait cf config                          */
    TFA9912_BF_IEMWSMU= 0x49c0,    /*!< Enable man Audio mute sequence                     */
    TFA9912_BF_IECFMER= 0x49d0,    /*!< Enable cfma err                                    */
    TFA9912_BF_IECFMAC= 0x49e0,    /*!< Enable cfma ack                                    */
    TFA9912_BF_IECLKOOR= 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
    TFA9912_BF_IETDMER= 0x4a00,    /*!< Enable tdm error                                   */
    TFA9912_BF_IECLPL= 0x4a10,    /*!< Enable clip left                                   */
    TFA9912_BF_IECLPR= 0x4a20,    /*!< Enable clip                                        */
    TFA9912_BF_IEOCPM1= 0x4a30,    /*!< Enable mic ocpok                                   */
    TFA9912_BF_IELP1 = 0x4a50,    /*!< Enable low power mode1                             */
    TFA9912_BF_IELA  = 0x4a60,    /*!< Enable low amplitude detection                     */
    TFA9912_BF_IEVDDP= 0x4a70,    /*!< Enable VDDP greater than VBAT                      */
    TFA9912_BF_IETAPDET= 0x4a80,    /*!< Enable Tap  detected                               */
    TFA9912_BF_IEAUDMOD= 0x4a90,    /*!< Enable Audio Mode activated                        */
    TFA9912_BF_IESAMMOD= 0x4aa0,    /*!< Enable SAM Mode activated                          */
    TFA9912_BF_IETAPMOD= 0x4ab0,    /*!< Enable Tap  Mode Activated                         */
    TFA9912_BF_IETAPTRG= 0x4ac0,    /*!< Enable comparator interrupt                        */
    TFA9912_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA9912_BF_IPOPLLS= 0x4c10,    /*!< Polarity pll lock                                  */
    TFA9912_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9912_BF_IPOOVDS= 0x4c30,    /*!< Polarity OVP alarm                                 */
    TFA9912_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9912_BF_IPOCLKS= 0x4c50,    /*!< Polarity clocks stable                             */
    TFA9912_BF_IPOMTPB= 0x4c60,    /*!< Polarity mtp busy                                  */
    TFA9912_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9912_BF_IPOSPKS= 0x4c80,    /*!< Polarity speaker error                             */
    TFA9912_BF_IPOACS= 0x4c90,    /*!< Polarity cold started                              */
    TFA9912_BF_IPOSWS= 0x4ca0,    /*!< Polarity amplifier engage                          */
    TFA9912_BF_IPOWDS= 0x4cb0,    /*!< Polarity watchdog                                  */
    TFA9912_BF_IPOAMPS= 0x4cc0,    /*!< Polarity enbl amp                                  */
    TFA9912_BF_IPOAREFS= 0x4cd0,    /*!< Polarity ref enable                                */
    TFA9912_BF_IPOADCCR= 0x4ce0,    /*!< Polarity Control ADC                               */
    TFA9912_BF_IPOBODNOK= 0x4cf0,    /*!< Polarity BOD                                       */
    TFA9912_BF_IPOBSTCU= 0x4d00,    /*!< Polarity DCDC current limiting                     */
    TFA9912_BF_IPOBSTHI= 0x4d10,    /*!< Polarity DCDC active                               */
    TFA9912_BF_IPOBSTOC= 0x4d20,    /*!< Polarity DCDC OCP                                  */
    TFA9912_BF_IPOBSTPC= 0x4d30,    /*!< Polarity bst peakcur                               */
    TFA9912_BF_IPOBSTVC= 0x4d40,    /*!< Polarity DCDC level 1x                             */
    TFA9912_BF_IPOBST86= 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
    TFA9912_BF_IPOBST93= 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
    TFA9912_BF_IPORCVLD= 0x4d70,    /*!< Polarity rcvldop ready                             */
    TFA9912_BF_IPOOCPL= 0x4d80,    /*!< Polarity ocp alarm left                            */
    TFA9912_BF_IPOOCPR= 0x4d90,    /*!< Polarity ocp alarm                                 */
    TFA9912_BF_IPOMWSRC= 0x4da0,    /*!< Polarity waits HW I2C settings                     */
    TFA9912_BF_IPOMWCFC= 0x4db0,    /*!< Polarity man wait cf config                        */
    TFA9912_BF_IPOMWSMU= 0x4dc0,    /*!< Polarity man audio mute sequence                   */
    TFA9912_BF_IPOCFMER= 0x4dd0,    /*!< Polarity cfma err                                  */
    TFA9912_BF_IPOCFMAC= 0x4de0,    /*!< Polarity cfma ack                                  */
    TFA9912_BF_IPOCLKOOR= 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
    TFA9912_BF_IPOTDMER= 0x4e00,    /*!< Polarity tdm error                                 */
    TFA9912_BF_IPOCLPL= 0x4e10,    /*!< Polarity clip left                                 */
    TFA9912_BF_IPOCLPR= 0x4e20,    /*!< Polarity clip                                      */
    TFA9912_BF_IPOOCPM= 0x4e30,    /*!< Polarity mic ocpok                                 */
    TFA9912_BF_IPOLP1= 0x4e50,    /*!< Polarity low power mode1                           */
    TFA9912_BF_IPOLA = 0x4e60,    /*!< Polarity low amplitude detection                   */
    TFA9912_BF_IPOVDDP= 0x4e70,    /*!< Polarity VDDP greater than VBAT                    */
    TFA9912_BF_IPOLTAPDET= 0x4e80,    /*!< PolarityTap  detected                              */
    TFA9912_BF_IPOLAUDMOD= 0x4e90,    /*!< PolarityAudio Mode activated                       */
    TFA9912_BF_IPOLSAMMOD= 0x4ea0,    /*!< PolaritySAM Mode activated                         */
    TFA9912_BF_IPOLTAPMOD= 0x4eb0,    /*!< Polarity Tap  Mode Activated                       */
    TFA9912_BF_IPOLTAPTRG= 0x4ec0,    /*!< PolarityTap  Comparator Trigger                    */
    TFA9912_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
    TFA9912_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
    TFA9912_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
    TFA9912_BF_BSSRR = 0x5082,    /*!< Battery Safeguard release time                     */
    TFA9912_BF_BSSHY = 0x50b1,    /*!< Battery Safeguard hysteresis                       */
    TFA9912_BF_BSSAC = 0x50d0,    /*!< Reset clipper - Auto clear                         */
    TFA9912_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9912_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA9912_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA9912_BF_INTSMUTE= 0x5110,    /*!< Soft mute HW                                       */
    TFA9912_BF_CFSML = 0x5120,    /*!< Soft mute FW left                                  */
    TFA9912_BF_CFSM  = 0x5130,    /*!< Soft mute FW                                       */
    TFA9912_BF_HPFBYPL= 0x5140,    /*!< Bypass HPF left                                    */
    TFA9912_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9912_BF_DPSAL = 0x5160,    /*!< Enable DPSA left                                   */
    TFA9912_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
    TFA9912_BF_VOL   = 0x5187,    /*!< FW volume control for primary audio channel        */
    TFA9912_BF_HNDSFRCV= 0x5200,    /*!< Selection receiver                                 */
    TFA9912_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA9912_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9912_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA9912_BF_SLOPESET= 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
    TFA9912_BF_CFTAPPAT= 0x5c07,    /*!< Coolflux tap pattern                               */
    TFA9912_BF_TAPDBGINFO= 0x5c83,    /*!< Reserved                                           */
    TFA9912_BF_TATPSTAT1= 0x5d0f,    /*!< Tap Status 1 from CF FW                            */
    TFA9912_BF_TCOMPTHR= 0x5f03,    /*!< Comparator threshold (in uV)                       */
    TFA9912_BF_PGAGAIN= 0x6081,    /*!< PGA gain selection                                 */
    TFA9912_BF_TDMSPKG= 0x6123,    /*!< System gain (INPLEV 0)                             */
    TFA9912_BF_LPM1LVL= 0x6505,    /*!< low power mode1 detector   ctrl threshold for low_audio_lvl  */
    TFA9912_BF_LPM1HLD= 0x6565,    /*!< Low power mode1 detector, ctrl hold time before low audio is reckoned to be low audio */
    TFA9912_BF_LPM1DIS= 0x65c0,    /*!< low power mode1 detector control                   */
    TFA9912_BF_DCDIS = 0x6630,    /*!< DCDC                                               */
    TFA9912_BF_TDMSRCMAP= 0x6801,    /*!< tdm source mapping                                 */
    TFA9912_BF_TDMSRCAS= 0x6821,    /*!< frame a selection                                  */
    TFA9912_BF_TDMSRCBS= 0x6841,    /*!< frame b selection                                  */
    TFA9912_BF_ANC1C = 0x68a0,    /*!< ANC one s complement                               */
    TFA9912_BF_SAMMODE= 0x6901,    /*!< Sam mode                                           */
    TFA9912_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA9912_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9912_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA9912_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA9912_BF_DCINSEL= 0x70c1,    /*!< DCDC IIR input Selection                           */
    TFA9912_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
    TFA9912_BF_DCTRIP= 0x7504,    /*!< Adaptive boost trip levels 1, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCTRIP2= 0x7554,    /*!< Adaptive boost trip level 2, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCTRIPT= 0x75a4,    /*!< Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCVOF = 0x7635,    /*!< First boost voltage level                          */
    TFA9912_BF_DCVOS = 0x7695,    /*!< Second boost voltage level                         */
    TFA9912_BF_RST   = 0x9000,    /*!< Reset                                              */
    TFA9912_BF_DMEM  = 0x9011,    /*!< Target memory                                      */
    TFA9912_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA9912_BF_CFINT = 0x9040,    /*!< Interrupt - auto clear                             */
    TFA9912_BF_CFCGATE= 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA9912_BF_REQCMD= 0x9080,    /*!< Firmware event request rpc command                 */
    TFA9912_BF_REQRST= 0x9090,    /*!< Firmware event request reset restart               */
    TFA9912_BF_REQMIPS= 0x90a0,    /*!< Firmware event request short on mips               */
    TFA9912_BF_REQMUTED= 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA9912_BF_REQVOL= 0x90c0,    /*!< Firmware event request volume ready                */
    TFA9912_BF_REQDMG= 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA9912_BF_REQCAL= 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA9912_BF_REQRSV= 0x90f0,    /*!< Firmware event request reserved                    */
    TFA9912_BF_MADD  = 0x910f,    /*!< Memory address                                     */
    TFA9912_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA9912_BF_ERR   = 0x9307,    /*!< Error flags                                        */
    TFA9912_BF_ACKCMD= 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA9912_BF_ACKRST= 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA9912_BF_ACKMIPS= 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA9912_BF_ACKMUTED= 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA9912_BF_ACKVOL= 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA9912_BF_ACKDMG= 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA9912_BF_ACKCAL= 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA9912_BF_ACKRSV= 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA9912_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA9912_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9912_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9912_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9912_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9912_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9912_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9912_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9912_BF_SWPROFIL= 0xee0f,    /*!< Software profile data                              */
    TFA9912_BF_SWVSTEP= 0xef0f,    /*!< Software vstep information                         */
    TFA9912_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9912_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9912_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9912_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9912_BF_DCMCCCL= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9912_BF_USERDEF= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9912_BF_R25C  = 0xf40f,    /*!< Ron resistance of  speaker coil                    */
} nxpTfa9912BfEnumList_t;
#define TFA9912_NAMETABLE static tfaBfName_t Tfa9912DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x30, "AMPE"},    /* Enables the Amplifier                             , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x90, "FSSSEL"},    /* Audio sample reference                            , */\
   { 0xb0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xc0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0x101, "AMPINSEL"},    /* Amplifier input selection                         , */\
   { 0x120, "MANSCONF"},    /* I2C configured                                    , */\
   { 0x130, "MANCOLD"},    /* Execute cold start                                , */\
   { 0x140, "MANAOOSC"},    /* Internal osc off at PWDN                          , */\
   { 0x150, "MANROBOD"},    /* Reaction on BOD                                   , */\
   { 0x160, "BODE"},    /* BOD Enable                                        , */\
   { 0x170, "BODHYS"},    /* BOD Hysteresis                                    , */\
   { 0x181, "BODFILT"},    /* BOD filter                                        , */\
   { 0x1a1, "BODTHLVL"},    /* BOD threshold                                     , */\
   { 0x1d0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "RCVNS"},    /* Noise shaper selection                            , */\
   { 0x1f0, "MANWDE"},    /* Watchdog enable                                   , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "TDMC"},    /* TDM Compatibility with TFA9872                    , */\
   { 0x2e0, "ENBLADC10"},    /* ADC10 Enable -  I2C direct mode                   , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x430, "ENCFCKSEL"},    /* Coolflux DSP clock scaling, low power mode        , */\
   { 0x441, "CFCKSEL"},    /* Coolflux DSP clock scaler selection for low power mode, */\
   { 0x460, "TDMINFSEL"},    /* TDM clock selection                               , */\
   { 0x470, "DISBLAUTOCLKSEL"},    /* Disable Automatic dsp clock source selection      , */\
   { 0x480, "SELCLKSRC"},    /* I2C selection of DSP clock when auto select is disabled, */\
   { 0x490, "SELTIMSRC"},    /* I2C selection of Watchdog and Timer clock         , */\
   { 0x500, "SSLEFTE"},    /*                                                   , */\
   { 0x510, "SPKSSEN"},    /* Enable speaker path                               , */\
   { 0x520, "VSLEFTE"},    /*                                                   , */\
   { 0x530, "VSRIGHTE"},    /* Voltage sense                                     , */\
   { 0x540, "CSLEFTE"},    /*                                                   , */\
   { 0x550, "CSRIGHTE"},    /* Current sense                                     , */\
   { 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
   { 0x570, "PGALE"},    /* Enable PGA chop clock for left channel            , */\
   { 0x580, "PGARE"},    /* Enable PGA chop clock                             , */\
   { 0x590, "SSTDME"},    /* Sub-system TDM                                    , */\
   { 0x5a0, "SSPBSTE"},    /* Sub-system boost                                  , */\
   { 0x5b0, "SSADCE"},    /* Sub-system ADC                                    , */\
   { 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
   { 0x5d0, "SSCFTIME"},    /* CF Sub-system timer                               , */\
   { 0x5e0, "SSCFWDTE"},    /* CF Sub-system WDT                                 , */\
   { 0x5f0, "FAIMVBGOVRRL"},    /* Over rule of vbg for FaIM access                  , */\
   { 0x600, "SAMSPKSEL"},    /* Input selection for TAP/SAM                       , */\
   { 0x610, "PDM2IISEN"},    /* PDM2IIS Bridge enable                             , */\
   { 0x620, "TAPRSTBYPASS"},    /* Tap decimator reset bypass - Bypass the decimator reset from tapdec, */\
   { 0x631, "CARDECISEL0"},    /* Cardec input 0 sel                                , */\
   { 0x651, "CARDECISEL1"},    /* Cardec input sel                                  , */\
   { 0x670, "TAPDECSEL"},    /* Select TAP/Cardec for TAP                         , */\
   { 0x680, "COMPCOUNT"},    /* Comparator o/p filter selection                   , */\
   { 0x691, "STARTUPMODE"},    /* Startup Mode Selection                            , */\
   { 0x6b0, "AUTOTAP"},    /* Enable auto tap switching                         , */\
   { 0x6c1, "COMPINITIME"},    /* Comparator initialization time to be used in Tap Machine, */\
   { 0x6e1, "ANAPINITIME"},    /* Analog initialization time to be used in Tap Machine, */\
   { 0x707, "CCHKTH"},    /* Clock check Higher Threshold                      , */\
   { 0x787, "CCHKTL"},    /* Clock check Higher Threshold                      , */\
   { 0x802, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x832, "AMPTCRR"},    /* Amplifier on-off criteria for tap mode entry      , */\
   { 0xd00, "STGS"},    /* PDM side tone gain selector                       , */\
   { 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
   { 0xda0, "STSMUTE"},    /* Side tone soft mute                               , */\
   { 0xdb0, "ST1C"},    /* side tone one s complement                        , */\
   { 0xe80, "CMFBEL"},    /* CMFB enable left                                  , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1060, "MTPB"},    /* MTP busy                                          , */\
   { 0x1070, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1090, "ACS"},    /* Cold Start                                        , */\
   { 0x10a0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x10b0, "WDS"},    /* Watchdog                                          , */\
   { 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x10d0, "AREFS"},    /* References enable                                 , */\
   { 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x10f0, "BODNOK"},    /* BOD                                               , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active                                       , */\
   { 0x1120, "DCOCPOK"},    /* DCDC OCP nmos                                     , */\
   { 0x1130, "DCPEAKCUR"},    /* Indicates current is max in DC-to-DC converter    , */\
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "STMUTE"},    /* side tone mute state                              , */\
   { 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11d0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11e0, "HAPTIC"},    /* Status haptic driver                              , */\
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
   { 0x13a0, "TCMPTRG"},    /* Status Tap comparator triggered                   , */\
   { 0x13b0, "TAPDET"},    /* Status Tap  detected                              , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "MANWAIT2"},    /* Wait CF config                                    , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1400, "SPKSL"},    /* Left speaker status                               , */\
   { 0x1410, "SPKS"},    /* Speaker status                                    , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1490, "DSPCLKSRC"},    /* DSP clock source selected by manager              , */\
   { 0x14a1, "STARTUPMODSTAT"},    /* Startup Mode Selected by Manager(Read Only)       , */\
   { 0x14c3, "TSPMSTATE"},    /* Tap Machine State                                 , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
   { 0x17a0, "DCILCF"},    /* DCDC current limiting for DSP                     , */\
   { 0x2000, "TDMUC"},    /* Mode setting                                      , */\
   { 0x2011, "DIO4SEL"},    /* DIO4 Input selection                              , */\
   { 0x2040, "TDME"},    /* Enable TDM interface                              , */\
   { 0x2050, "TDMMODE"},    /* Slave/master                                      , */\
   { 0x2060, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "TDMFSLN"},    /* FS length                                         , */\
   { 0x20b0, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x20c3, "TDMNBCK"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x2144, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2194, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x21e0, "TDMDEL"},    /* data delay to FS                                  , */\
   { 0x21f0, "TDMADJ"},    /* data adjustment                                   , */\
   { 0x2201, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2224, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2271, "TDMTXDFO"},    /* Format unused bits in a slot                      , */\
   { 0x2291, "TDMTXUS0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "TDMTXUS1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "TDMTXUS2"},    /* Format unused slots DIO2                          , */\
   { 0x2300, "TDMGIE"},    /* Control gain (channel in 0)                       , */\
   { 0x2310, "TDMDCE"},    /* Control audio  left (channel in 1 )               , */\
   { 0x2320, "TDMSPKE"},    /* Control audio right (channel in 2 )               , */\
   { 0x2330, "TDMCSE"},    /* Current sense                                     , */\
   { 0x2340, "TDMVSE"},    /* Voltage sense                                     , */\
   { 0x2350, "TDMGOE"},    /* DSP Gainout                                       , */\
   { 0x2360, "TDMCF2E"},    /* DSP 2                                             , */\
   { 0x2370, "TDMCF3E"},    /* DSP 3                                             , */\
   { 0x2380, "TDMCFE"},    /* DSP                                               , */\
   { 0x2390, "TDMES6"},    /* Loopback of Audio left (channel 1)                , */\
   { 0x23a0, "TDMES7"},    /* Loopback of Audio right (channel 2)               , */\
   { 0x23b0, "TDMCF4E"},    /* AEC ref right control                             , */\
   { 0x23c0, "TDMPD1E"},    /* PDM 1 control                                     , */\
   { 0x23d0, "TDMPD2E"},    /* PDM 2 control                                     , */\
   { 0x2401, "TDMGIN"},    /* IO gainin                                         , */\
   { 0x2421, "TDMLIO"},    /* IO audio left                                     , */\
   { 0x2441, "TDMRIO"},    /* IO audio right                                    , */\
   { 0x2461, "TDMCSIO"},    /* IO Current Sense                                  , */\
   { 0x2481, "TDMVSIO"},    /* IO voltage sense                                  , */\
   { 0x24a1, "TDMGOIO"},    /* IO gain out                                       , */\
   { 0x24c1, "TDMCFIO2"},    /* IO DSP 2                                          , */\
   { 0x24e1, "TDMCFIO3"},    /* IO DSP 3                                          , */\
   { 0x2501, "TDMCFIO"},    /* IO DSP                                            , */\
   { 0x2521, "TDMLPB6"},    /* IO Source 6                                       , */\
   { 0x2541, "TDMLPB7"},    /* IO Source 7                                       , */\
   { 0x2603, "TDMGS"},    /* Control gainin                                    , */\
   { 0x2643, "TDMDCS"},    /* tdm slot for audio left (channel 1)               , */\
   { 0x2683, "TDMSPKS"},    /* tdm slot for audio right (channel 2)              , */\
   { 0x26c3, "TDMCSS"},    /* Slot Position of Current Sense Out                , */\
   { 0x2703, "TDMVSS"},    /* Slot Position of Voltage sense                    , */\
   { 0x2743, "TDMCGOS"},    /* Slot Position of GAIN out                         , */\
   { 0x2783, "TDMCF2S"},    /* Slot Position DSPout2                             , */\
   { 0x27c3, "TDMCF3S"},    /* Slot Position DSPout3                             , */\
   { 0x2803, "TDMCFS"},    /* Slot Position of DSPout                           , */\
   { 0x2843, "TDMEDAT6S"},    /* Slot Position of loopback channel left            , */\
   { 0x2883, "TDMEDAT7S"},    /* Slot Position of loopback channel right           , */\
   { 0x2901, "TDMTXUS3"},    /* Format unused slots D3                            , */\
   { 0x3100, "PDMSM"},    /* PDM control                                       , */\
   { 0x3110, "PDMSTSEL"},    /* PDM Decimator input selection                     , */\
   { 0x3120, "PDMSTENBL"},    /* Side tone input enable                            , */\
   { 0x3130, "PDMLSEL"},    /* PDM data selection for left channel during PDM direct mode, */\
   { 0x3140, "PDMRSEL"},    /* PDM data selection for right channel during PDM direct mode, */\
   { 0x3150, "MICVDDE"},    /* Enable MICVDD                                     , */\
   { 0x3201, "PDMCLRAT"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x3223, "PDMGAIN"},    /* PDM gain                                          , */\
   { 0x3263, "PDMOSEL"},    /* PDM output selection - RE/FE data combination     , */\
   { 0x32a0, "SELCFHAPD"},    /* Select the source for haptic data output (not for customer), */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTPLLS"},    /* Status PLL lock                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOVDS"},    /* Status OVP alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTCLKS"},    /* Status clocks stable                              , */\
   { 0x4060, "ISTMTPB"},    /* Status MTP busy                                   , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4080, "ISTSPKS"},    /* Status speaker error                              , */\
   { 0x4090, "ISTACS"},    /* Status cold start                                 , */\
   { 0x40a0, "ISTSWS"},    /* Status amplifier engage                           , */\
   { 0x40b0, "ISTWDS"},    /* Status watchdog                                   , */\
   { 0x40c0, "ISTAMPS"},    /* Status amplifier enable                           , */\
   { 0x40d0, "ISTAREFS"},    /* Status Ref enable                                 , */\
   { 0x40e0, "ISTADCCR"},    /* Status Control ADC                                , */\
   { 0x40f0, "ISTBODNOK"},    /* Status BOD                                        , */\
   { 0x4100, "ISTBSTCU"},    /* Status DCDC current limiting                      , */\
   { 0x4110, "ISTBSTHI"},    /* Status DCDC active                                , */\
   { 0x4120, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4130, "ISTBSTPKCUR"},    /* Status bst peakcur                                , */\
   { 0x4140, "ISTBSTVC"},    /* Status DCDC level 1x                              , */\
   { 0x4150, "ISTBST86"},    /* Status DCDC level 1.14x                           , */\
   { 0x4160, "ISTBST93"},    /* Status DCDC level 1.07x                           , */\
   { 0x4170, "ISTRCVLD"},    /* Status rcvldop ready                              , */\
   { 0x4180, "ISTOCPL"},    /* Status ocp alarm left                             , */\
   { 0x4190, "ISTOCPR"},    /* Status ocp alarm                                  , */\
   { 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "ISTMWCFC"},    /* Status waits CF config                            , */\
   { 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x41e0, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4210, "ISTCLPL"},    /* Status clip left                                  , */\
   { 0x4220, "ISTCLPR"},    /* Status clip                                       , */\
   { 0x4230, "ISTOCPM"},    /* Status mic ocpok                                  , */\
   { 0x4250, "ISTLP1"},    /* Status low power mode1                            , */\
   { 0x4260, "ISTLA"},    /* Status low amplitude detection                    , */\
   { 0x4270, "ISTVDDP"},    /* Status VDDP greater than VBAT                     , */\
   { 0x4280, "ISTTAPDET"},    /* Status Tap  detected                              , */\
   { 0x4290, "ISTAUDMOD"},    /* Status Audio Mode activated                       , */\
   { 0x42a0, "ISTSAMMOD"},    /* Status SAM Mode activated                         , */\
   { 0x42b0, "ISTTAPMOD"},    /* Status Tap  Mode Activated                        , */\
   { 0x42c0, "ISTTAPTRG"},    /* Status Tap comparator triggered                   , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLPLLS"},    /* Clear PLL lock                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOVDS"},    /* Clear OVP alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLCLKS"},    /* Clear clocks stable                               , */\
   { 0x4460, "ICLMTPB"},    /* Clear mtp busy                                    , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4480, "ICLSPKS"},    /* Clear speaker error                               , */\
   { 0x4490, "ICLACS"},    /* Clear cold started                                , */\
   { 0x44a0, "ICLSWS"},    /* Clear amplifier engage                            , */\
   { 0x44b0, "ICLWDS"},    /* Clear watchdog                                    , */\
   { 0x44c0, "ICLAMPS"},    /* Clear enbl amp                                    , */\
   { 0x44d0, "ICLAREFS"},    /* Clear ref enable                                  , */\
   { 0x44e0, "ICLADCCR"},    /* Clear control ADC                                 , */\
   { 0x44f0, "ICLBODNOK"},    /* Clear BOD                                         , */\
   { 0x4500, "ICLBSTCU"},    /* Clear DCDC current limiting                       , */\
   { 0x4510, "ICLBSTHI"},    /* Clear DCDC active                                 , */\
   { 0x4520, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4530, "ICLBSTPC"},    /* Clear bst peakcur                                 , */\
   { 0x4540, "ICLBSTVC"},    /* Clear DCDC level 1x                               , */\
   { 0x4550, "ICLBST86"},    /* Clear DCDC level 1.14x                            , */\
   { 0x4560, "ICLBST93"},    /* Clear DCDC level 1.07x                            , */\
   { 0x4570, "ICLRCVLD"},    /* Clear rcvldop ready                               , */\
   { 0x4580, "ICLOCPL"},    /* Clear ocp alarm left                              , */\
   { 0x4590, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
   { 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "ICLMWCFC"},    /* Clear wait cf config                              , */\
   { 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x45e0, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4610, "ICLCLPL"},    /* Clear clip left                                   , */\
   { 0x4620, "ICLCLP"},    /* Clear clip                                        , */\
   { 0x4630, "ICLOCPM"},    /* Clear mic ocpok                                   , */\
   { 0x4650, "ICLLP1"},    /* Clear low power mode1                             , */\
   { 0x4660, "ICLLA"},    /* Clear low amplitude detection                     , */\
   { 0x4670, "ICLVDDP"},    /* Clear VDDP greater then VBAT                      , */\
   { 0x4680, "ICLTAPDET"},    /* Clear Tap  detected                               , */\
   { 0x4690, "ICLAUDMOD"},    /* Clear Audio Mode activated                        , */\
   { 0x46a0, "ICLSAMMOD"},    /* Clear SAM Mode activated                          , */\
   { 0x46b0, "ICLTAPMOD"},    /* Clear Tap  Mode Activated                         , */\
   { 0x46c0, "ICLTAPTRG"},    /* Clear Comparator Interrupt                        , */\
   { 0x4800, "IEVDDS"},    /* Enable por                                        , */\
   { 0x4810, "IEPLLS"},    /* Enable pll lock                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOVDS"},    /* Enable OVP alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IECLKS"},    /* Enable clocks stable                              , */\
   { 0x4860, "IEMTPB"},    /* Enable mtp busy                                   , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4880, "IESPKS"},    /* Enable speaker error                              , */\
   { 0x4890, "IEACS"},    /* Enable cold started                               , */\
   { 0x48a0, "IESWS"},    /* Enable amplifier engage                           , */\
   { 0x48b0, "IEWDS"},    /* Enable watchdog                                   , */\
   { 0x48c0, "IEAMPS"},    /* Enable enbl amp                                   , */\
   { 0x48d0, "IEAREFS"},    /* Enable ref enable                                 , */\
   { 0x48e0, "IEADCCR"},    /* Enable Control ADC                                , */\
   { 0x48f0, "IEBODNOK"},    /* Enable BOD                                        , */\
   { 0x4900, "IEBSTCU"},    /* Enable DCDC current limiting                      , */\
   { 0x4910, "IEBSTHI"},    /* Enable DCDC active                                , */\
   { 0x4920, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4930, "IEBSTPC"},    /* Enable bst peakcur                                , */\
   { 0x4940, "IEBSTVC"},    /* Enable DCDC level 1x                              , */\
   { 0x4950, "IEBST86"},    /* Enable DCDC level 1.14x                           , */\
   { 0x4960, "IEBST93"},    /* Enable DCDC level 1.07x                           , */\
   { 0x4970, "IERCVLD"},    /* Enable rcvldop ready                              , */\
   { 0x4980, "IEOCPL"},    /* Enable ocp alarm left                             , */\
   { 0x4990, "IEOCPR"},    /* Enable ocp alarm                                  , */\
   { 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "IEMWCFC"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x49e0, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4a10, "IECLPL"},    /* Enable clip left                                  , */\
   { 0x4a20, "IECLPR"},    /* Enable clip                                       , */\
   { 0x4a30, "IEOCPM1"},    /* Enable mic ocpok                                  , */\
   { 0x4a50, "IELP1"},    /* Enable low power mode1                            , */\
   { 0x4a60, "IELA"},    /* Enable low amplitude detection                    , */\
   { 0x4a70, "IEVDDP"},    /* Enable VDDP greater than VBAT                     , */\
   { 0x4a80, "IETAPDET"},    /* Enable Tap  detected                              , */\
   { 0x4a90, "IEAUDMOD"},    /* Enable Audio Mode activated                       , */\
   { 0x4aa0, "IESAMMOD"},    /* Enable SAM Mode activated                         , */\
   { 0x4ab0, "IETAPMOD"},    /* Enable Tap  Mode Activated                        , */\
   { 0x4ac0, "IETAPTRG"},    /* Enable comparator interrupt                       , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
   { 0x4c10, "IPOPLLS"},    /* Polarity pll lock                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOVDS"},    /* Polarity OVP alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOCLKS"},    /* Polarity clocks stable                            , */\
   { 0x4c60, "IPOMTPB"},    /* Polarity mtp busy                                 , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "IPOSPKS"},    /* Polarity speaker error                            , */\
   { 0x4c90, "IPOACS"},    /* Polarity cold started                             , */\
   { 0x4ca0, "IPOSWS"},    /* Polarity amplifier engage                         , */\
   { 0x4cb0, "IPOWDS"},    /* Polarity watchdog                                 , */\
   { 0x4cc0, "IPOAMPS"},    /* Polarity enbl amp                                 , */\
   { 0x4cd0, "IPOAREFS"},    /* Polarity ref enable                               , */\
   { 0x4ce0, "IPOADCCR"},    /* Polarity Control ADC                              , */\
   { 0x4cf0, "IPOBODNOK"},    /* Polarity BOD                                      , */\
   { 0x4d00, "IPOBSTCU"},    /* Polarity DCDC current limiting                    , */\
   { 0x4d10, "IPOBSTHI"},    /* Polarity DCDC active                              , */\
   { 0x4d20, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4d30, "IPOBSTPC"},    /* Polarity bst peakcur                              , */\
   { 0x4d40, "IPOBSTVC"},    /* Polarity DCDC level 1x                            , */\
   { 0x4d50, "IPOBST86"},    /* Polarity DCDC level 1.14x                         , */\
   { 0x4d60, "IPOBST93"},    /* Polarity DCDC level 1.07x                         , */\
   { 0x4d70, "IPORCVLD"},    /* Polarity rcvldop ready                            , */\
   { 0x4d80, "IPOOCPL"},    /* Polarity ocp alarm left                           , */\
   { 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "IPOMWCFC"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "IPOCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4e10, "IPOCLPL"},    /* Polarity clip left                                , */\
   { 0x4e20, "IPOCLPR"},    /* Polarity clip                                     , */\
   { 0x4e30, "IPOOCPM"},    /* Polarity mic ocpok                                , */\
   { 0x4e50, "IPOLP1"},    /* Polarity low power mode1                          , */\
   { 0x4e60, "IPOLA"},    /* Polarity low amplitude detection                  , */\
   { 0x4e70, "IPOVDDP"},    /* Polarity VDDP greater than VBAT                   , */\
   { 0x4e80, "IPOLTAPDET"},    /* PolarityTap  detected                             , */\
   { 0x4e90, "IPOLAUDMOD"},    /* PolarityAudio Mode activated                      , */\
   { 0x4ea0, "IPOLSAMMOD"},    /* PolaritySAM Mode activated                        , */\
   { 0x4eb0, "IPOLTAPMOD"},    /* Polarity Tap  Mode Activated                      , */\
   { 0x4ec0, "IPOLTAPTRG"},    /* PolarityTap  Comparator Trigger                   , */\
   { 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5082, "BSSRR"},    /* Battery Safeguard release time                    , */\
   { 0x50b1, "BSSHY"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50d0, "BSSAC"},    /* Reset clipper - Auto clear                        , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
   { 0x5120, "CFSML"},    /* Soft mute FW left                                 , */\
   { 0x5130, "CFSM"},    /* Soft mute FW                                      , */\
   { 0x5140, "HPFBYPL"},    /* Bypass HPF left                                   , */\
   { 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
   { 0x5160, "DPSAL"},    /* Enable DPSA left                                  , */\
   { 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x5187, "VOL"},    /* FW volume control for primary audio channel       , */\
   { 0x5200, "HNDSFRCV"},    /* Selection receiver                                , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x5c07, "CFTAPPAT"},    /* Coolflux tap pattern                              , */\
   { 0x5c83, "TAPDBGINFO"},    /* Reserved                                          , */\
   { 0x5d0f, "TATPSTAT1"},    /* Tap Status 1 from CF FW                           , */\
   { 0x5f03, "TCOMPTHR"},    /* Comparator threshold (in uV)                      , */\
   { 0x6081, "PGAGAIN"},    /* PGA gain selection                                , */\
   { 0x6123, "TDMSPKG"},    /* System gain (INPLEV 0)                            , */\
   { 0x6505, "LPM1LVL"},    /* low power mode1 detector   ctrl threshold for low_audio_lvl , */\
   { 0x6565, "LPM1HLD"},    /* Low power mode1 detector, ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x65c0, "LPM1DIS"},    /* low power mode1 detector control                  , */\
   { 0x6630, "DCDIS"},    /* DCDC                                              , */\
   { 0x6801, "TDMSRCMAP"},    /* tdm source mapping                                , */\
   { 0x6821, "TDMSRCAS"},    /* frame a selection                                 , */\
   { 0x6841, "TDMSRCBS"},    /* frame b selection                                 , */\
   { 0x68a0, "ANC1C"},    /* ANC one s complement                              , */\
   { 0x6901, "SAMMODE"},    /* Sam mode                                          , */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70c1, "DCINSEL"},    /* DCDC IIR input Selection                          , */\
   { 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7504, "DCTRIP"},    /* Adaptive boost trip levels 1, effective only when boost_intelligent is set to 1, */\
   { 0x7554, "DCTRIP2"},    /* Adaptive boost trip level 2, effective only when boost_intelligent is set to 1, */\
   { 0x75a4, "DCTRIPT"},    /* Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x7635, "DCVOF"},    /* First boost voltage level                         , */\
   { 0x7695, "DCVOS"},    /* Second boost voltage level                        , */\
   { 0x9000, "RST"},    /* Reset                                             , */\
   { 0x9011, "DMEM"},    /* Target memory                                     , */\
   { 0x9030, "AIF"},    /* Auto increment                                    , */\
   { 0x9040, "CFINT"},    /* Interrupt - auto clear                            , */\
   { 0x9050, "CFCGATE"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x9090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x910f, "MADD"},    /* Memory address                                    , */\
   { 0x920f, "MEMA"},    /* Activate memory access                            , */\
   { 0x9307, "ERR"},    /* Error flags                                       , */\
   { 0x9380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
   { 0xa107, "MTPK"},    /* MTP KEY2 register                                 , */\
   { 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
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
   { 0xf042, "DCMCCCL"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "USERDEF"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf40f, "R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum tfa9912_irq {
	tfa9912_irq_stvdds = 0,
	tfa9912_irq_stplls = 1,
	tfa9912_irq_stotds = 2,
	tfa9912_irq_stovds = 3,
	tfa9912_irq_stuvds = 4,
	tfa9912_irq_stclks = 5,
	tfa9912_irq_stmtpb = 6,
	tfa9912_irq_stnoclk = 7,
	tfa9912_irq_stspks = 8,
	tfa9912_irq_stacs = 9,
	tfa9912_irq_stsws = 10,
	tfa9912_irq_stwds = 11,
	tfa9912_irq_stamps = 12,
	tfa9912_irq_starefs = 13,
	tfa9912_irq_stadccr = 14,
	tfa9912_irq_stbodnok = 15,
	tfa9912_irq_stbstcu = 16,
	tfa9912_irq_stbsthi = 17,
	tfa9912_irq_stbstoc = 18,
	tfa9912_irq_stbstpkcur = 19,
	tfa9912_irq_stbstvc = 20,
	tfa9912_irq_stbst86 = 21,
	tfa9912_irq_stbst93 = 22,
	tfa9912_irq_strcvld = 23,
	tfa9912_irq_stocpl = 24,
	tfa9912_irq_stocpr = 25,
	tfa9912_irq_stmwsrc = 26,
	tfa9912_irq_stmwcfc = 27,
	tfa9912_irq_stmwsmu = 28,
	tfa9912_irq_stcfmer = 29,
	tfa9912_irq_stcfmac = 30,
	tfa9912_irq_stclkoor = 31,
	tfa9912_irq_sttdmer = 32,
	tfa9912_irq_stclpl = 33,
	tfa9912_irq_stclpr = 34,
	tfa9912_irq_stocpm = 35,
	tfa9912_irq_stlp1 = 37,
	tfa9912_irq_stla = 38,
	tfa9912_irq_stvddp = 39,
	tfa9912_irq_sttapdet = 40,
	tfa9912_irq_staudmod = 41,
	tfa9912_irq_stsammod = 42,
	tfa9912_irq_sttapmod = 43,
	tfa9912_irq_sttaptrg = 44,
	tfa9912_irq_max = 45,
	tfa9912_irq_all = -1 /* all irqs */};

#define TFA9912_IRQ_NAMETABLE static tfaIrqName_t Tfa9912IrqNames[]= {\
	{ 0, "STVDDS"},\
	{ 1, "STPLLS"},\
	{ 2, "STOTDS"},\
	{ 3, "STOVDS"},\
	{ 4, "STUVDS"},\
	{ 5, "STCLKS"},\
	{ 6, "STMTPB"},\
	{ 7, "STNOCLK"},\
	{ 8, "STSPKS"},\
	{ 9, "STACS"},\
	{ 10, "STSWS"},\
	{ 11, "STWDS"},\
	{ 12, "STAMPS"},\
	{ 13, "STAREFS"},\
	{ 14, "STADCCR"},\
	{ 15, "STBODNOK"},\
	{ 16, "STBSTCU"},\
	{ 17, "STBSTHI"},\
	{ 18, "STBSTOC"},\
	{ 19, "STBSTPKCUR"},\
	{ 20, "STBSTVC"},\
	{ 21, "STBST86"},\
	{ 22, "STBST93"},\
	{ 23, "STRCVLD"},\
	{ 24, "STOCPL"},\
	{ 25, "STOCPR"},\
	{ 26, "STMWSRC"},\
	{ 27, "STMWCFC"},\
	{ 28, "STMWSMU"},\
	{ 29, "STCFMER"},\
	{ 30, "STCFMAC"},\
	{ 31, "STCLKOOR"},\
	{ 32, "STTDMER"},\
	{ 33, "STCLPL"},\
	{ 34, "STCLPR"},\
	{ 35, "STOCPM"},\
	{ 36, "36"},\
	{ 37, "STLP1"},\
	{ 38, "STLA"},\
	{ 39, "STVDDP"},\
	{ 40, "STTAPDET"},\
	{ 41, "STAUDMOD"},\
	{ 42, "STSAMMOD"},\
	{ 43, "STTAPMOD"},\
	{ 44, "STTAPTRG"},\
};
#endif /* _TFA9912_TFAFIELDNAMES_H */
