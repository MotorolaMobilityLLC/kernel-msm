/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9894_TFAFIELDNAMES_H
#define _TFA9894_TFAFIELDNAMES_H

#define TFA9894_I2CVERSION    7

typedef enum nxpTfa9894BfEnumList {
    TFA9894_BF_PWDN  = 0x0000,    /*!< Powerdown control                                  */
    TFA9894_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9894_BF_CFE   = 0x0020,    /*!< Enable CoolFlux DSP                                */
    TFA9894_BF_AMPE  = 0x0030,    /*!< Enable Amplifier                                   */
    TFA9894_BF_DCA   = 0x0040,    /*!< Enable DCDC Boost converter                        */
    TFA9894_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA9894_BF_AMPC  = 0x0060,    /*!< CoolFlux control over amplifier                    */
    TFA9894_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9894_BF_FSSSEL= 0x0090,    /*!< Audio sample reference                             */
    TFA9894_BF_BYPOCP= 0x00a0,    /*!< Bypass OCP                                         */
    TFA9894_BF_TSTOCP= 0x00b0,    /*!< OCP testing control                                */
    TFA9894_BF_BSSS  = 0x00c0,    /*!< Vbat protection steepness                          */
    TFA9894_BF_HPFBYP= 0x00d0,    /*!< Bypass High Pass Filter                            */
    TFA9894_BF_DPSA  = 0x00e0,    /*!< Enable DPSA                                        */
    TFA9894_BF_AMPINSEL= 0x0101,    /*!< Amplifier input selection                          */
    TFA9894_BF_MANSCONF= 0x0120,    /*!< Device I2C settings configured                     */
    TFA9894_BF_MANCOLD= 0x0130,    /*!< Execute cold start                                 */
    TFA9894_BF_MANROBOD= 0x0140,    /*!< Reaction on BOD                                    */
    TFA9894_BF_BODE  = 0x0150,    /*!< Enable BOD (only in direct control mode)           */
    TFA9894_BF_BODHYS= 0x0160,    /*!< Enable Hysteresis of BOD                           */
    TFA9894_BF_BODFILT= 0x0171,    /*!< BOD filter                                         */
    TFA9894_BF_BODTHLVL= 0x0191,    /*!< BOD threshold                                      */
    TFA9894_BF_MUTETO= 0x01b0,    /*!< Time out SB mute sequence                          */
    TFA9894_BF_MANWDE= 0x01c0,    /*!< Watchdog enable                                    */
    TFA9894_BF_OPENMTP= 0x01e0,    /*!< Control for FAIM protection                        */
    TFA9894_BF_FAIMVBGOVRRL= 0x01f0,    /*!< Overrule the enabling of VBG for faim erase/write access */
    TFA9894_BF_AUDFS = 0x0203,    /*!< Audio sample rate Fs                               */
    TFA9894_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9894_BF_FRACTDEL= 0x0255,    /*!< Current sense fractional delay                     */
    TFA9894_BF_TDMPRES= 0x02b1,    /*!< Control for HW manager                             */
    TFA9894_BF_AMPOCRT= 0x02d2,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9894_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9894_BF_REFCKEXT= 0x0401,    /*!< PLL external reference clock                       */
    TFA9894_BF_REFCKSEL= 0x0420,    /*!< PLL internal reference clock                       */
    TFA9894_BF_MCLKSEL= 0x0432,    /*!< Master Clock Selection                             */
    TFA9894_BF_MANAOOSC= 0x0460,    /*!< Internal OSC1M off at PWDN                         */
    TFA9894_BF_ACKCLDDIS= 0x0470,    /*!< Automatic PLL reference clock selection for cold start */
    TFA9894_BF_SPKSSEN= 0x0510,    /*!< Enable speaker sub-system                          */
    TFA9894_BF_MTPSSEN= 0x0520,    /*!< Enable FAIM sub-system                             */
    TFA9894_BF_WDTCLKEN= 0x0530,    /*!< Enable Coolflux watchdog clock                     */
    TFA9894_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9894_BF_PLLS  = 0x1010,    /*!< PLL Lock                                           */
    TFA9894_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA9894_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA9894_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA9894_BF_OCDS  = 0x1050,    /*!< OCP amplifier (sticky register, clear on read)     */
    TFA9894_BF_CLKS  = 0x1060,    /*!< Clocks stable                                      */
    TFA9894_BF_MTPB  = 0x1070,    /*!< MTP busy                                           */
    TFA9894_BF_NOCLK = 0x1080,    /*!< Lost clock                                         */
    TFA9894_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA9894_BF_WDS   = 0x10a0,    /*!< Watchdog                                           */
    TFA9894_BF_SWS   = 0x10b0,    /*!< Amplifier engage                                   */
    TFA9894_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA9894_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA9894_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA9894_BF_BODNOK= 0x10f0,    /*!< BOD Flag - VDD NOT OK                              */
    TFA9894_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9894_BF_DCDCA = 0x1110,    /*!< DCDC active (sticky register, clear on read)       */
    TFA9894_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos (sticky register, clear on read)     */
    TFA9894_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA9894_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA9894_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA9894_BF_SPKS  = 0x1170,    /*!< Speaker status                                     */
    TFA9894_BF_CLKOOR= 0x1180,    /*!< External clock status                              */
    TFA9894_BF_MANALARM= 0x1190,    /*!< Alarm state                                        */
    TFA9894_BF_TDMERR= 0x11a0,    /*!< TDM error                                          */
    TFA9894_BF_TDMLUTER= 0x11b0,    /*!< TDM lookup table error                             */
    TFA9894_BF_OCPOAP= 0x1200,    /*!< OCPOK pmos A                                       */
    TFA9894_BF_OCPOAN= 0x1210,    /*!< OCPOK nmos A                                       */
    TFA9894_BF_OCPOBP= 0x1220,    /*!< OCPOK pmos B                                       */
    TFA9894_BF_OCPOBN= 0x1230,    /*!< OCPOK nmos B                                       */
    TFA9894_BF_CLIPS = 0x1240,    /*!< Amplifier clipping                                 */
    TFA9894_BF_MANMUTE= 0x1250,    /*!< Audio mute sequence                                */
    TFA9894_BF_MANOPER= 0x1260,    /*!< Device in Operating state                          */
    TFA9894_BF_LP1   = 0x1270,    /*!< Low power MODE1 detection                          */
    TFA9894_BF_LA    = 0x1280,    /*!< Low amplitude detection                            */
    TFA9894_BF_VDDPH = 0x1290,    /*!< VDDP greater than VBAT flag                        */
    TFA9894_BF_TDMSTAT= 0x1402,    /*!< TDM Status bits                                    */
    TFA9894_BF_MANSTATE= 0x1433,    /*!< Device Manager status                              */
    TFA9894_BF_DCMODE= 0x14b1,    /*!< DCDC mode status bits                              */
    TFA9894_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9894_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9894_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage (1023*VDDP/13V)                    */
    TFA9894_BF_TDME  = 0x2000,    /*!< Enable interface                                   */
    TFA9894_BF_TDMSPKE= 0x2010,    /*!< Control audio tdm channel in sink0                 */
    TFA9894_BF_TDMDCE= 0x2020,    /*!< Control audio tdm channel in sink1                 */
    TFA9894_BF_TDMCSE= 0x2030,    /*!< Source 0 enable                                    */
    TFA9894_BF_TDMVSE= 0x2040,    /*!< Source 1 enable                                    */
    TFA9894_BF_TDMCFE= 0x2050,    /*!< Source 2 enable                                    */
    TFA9894_BF_TDMCF2E= 0x2060,    /*!< Source 3 enable                                    */
    TFA9894_BF_TDMCLINV= 0x2070,    /*!< Reception data to BCK clock                        */
    TFA9894_BF_TDMFSPOL= 0x2080,    /*!< FS polarity                                        */
    TFA9894_BF_TDMDEL= 0x2090,    /*!< Data delay to FS                                   */
    TFA9894_BF_TDMADJ= 0x20a0,    /*!< Data adjustment                                    */
    TFA9894_BF_TDMOOMP= 0x20b1,    /*!< Received audio compression                         */
    TFA9894_BF_TDMNBCK= 0x2103,    /*!< TDM NBCK - Bit clock to FS ratio                   */
    TFA9894_BF_TDMFSLN= 0x2143,    /*!< FS length (master mode only)                       */
    TFA9894_BF_TDMSLOTS= 0x2183,    /*!< N-slots in Frame                                   */
    TFA9894_BF_TDMTXDFO= 0x21c1,    /*!< Format unused bits                                 */
    TFA9894_BF_TDMTXUS0= 0x21e1,    /*!< Format unused slots DATAO                          */
    TFA9894_BF_TDMSLLN= 0x2204,    /*!< N-bits in slot                                     */
    TFA9894_BF_TDMBRMG= 0x2254,    /*!< N-bits remaining                                   */
    TFA9894_BF_TDMSSIZE= 0x22a4,    /*!< Sample size per slot                               */
    TFA9894_BF_TDMSPKS= 0x2303,    /*!< TDM slot for sink 0                                */
    TFA9894_BF_TDMDCS= 0x2343,    /*!< TDM slot for sink 1                                */
    TFA9894_BF_TDMCFSEL= 0x2381,    /*!< TDM Source 2 data selection                        */
    TFA9894_BF_TDMCF2SEL= 0x23a1,    /*!< TDM Source 3 data selection                        */
    TFA9894_BF_TDMCSS= 0x2403,    /*!< Slot Position of source 0 data                     */
    TFA9894_BF_TDMVSS= 0x2443,    /*!< Slot Position of source 1 data                     */
    TFA9894_BF_TDMCFS= 0x2483,    /*!< Slot Position of source 2 data                     */
    TFA9894_BF_TDMCF2S= 0x24c3,    /*!< Slot Position of source 3 data                     */
    TFA9894_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9894_BF_ISTBSTOC= 0x4010,    /*!< Status DCDC OCP                                    */
    TFA9894_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9894_BF_ISTOCPR= 0x4030,    /*!< Status OCP alarm                                   */
    TFA9894_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9894_BF_ISTMANALARM= 0x4050,    /*!< Status manager alarm state                         */
    TFA9894_BF_ISTTDMER= 0x4060,    /*!< Status TDM error                                   */
    TFA9894_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9894_BF_ISTCFMER= 0x4080,    /*!< Status cfma error                                  */
    TFA9894_BF_ISTCFMAC= 0x4090,    /*!< Status cfma ack                                    */
    TFA9894_BF_ISTSPKS= 0x40a0,    /*!< Status coolflux speaker error                      */
    TFA9894_BF_ISTACS= 0x40b0,    /*!< Status cold started                                */
    TFA9894_BF_ISTWDS= 0x40c0,    /*!< Status watchdog reset                              */
    TFA9894_BF_ISTBODNOK= 0x40d0,    /*!< Status brown out detect                            */
    TFA9894_BF_ISTLP1= 0x40e0,    /*!< Status low power mode1 detect                      */
    TFA9894_BF_ISTCLKOOR= 0x40f0,    /*!< Status clock out of range                          */
    TFA9894_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9894_BF_ICLBSTOC= 0x4410,    /*!< Clear DCDC OCP                                     */
    TFA9894_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9894_BF_ICLOCPR= 0x4430,    /*!< Clear OCP alarm                                    */
    TFA9894_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9894_BF_ICLMANALARM= 0x4450,    /*!< Clear manager alarm state                          */
    TFA9894_BF_ICLTDMER= 0x4460,    /*!< Clear TDM error                                    */
    TFA9894_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9894_BF_ICLCFMER= 0x4480,    /*!< Clear cfma err                                     */
    TFA9894_BF_ICLCFMAC= 0x4490,    /*!< Clear cfma ack                                     */
    TFA9894_BF_ICLSPKS= 0x44a0,    /*!< Clear coolflux speaker error                       */
    TFA9894_BF_ICLACS= 0x44b0,    /*!< Clear cold started                                 */
    TFA9894_BF_ICLWDS= 0x44c0,    /*!< Clear watchdog reset                               */
    TFA9894_BF_ICLBODNOK= 0x44d0,    /*!< Clear brown out detect                             */
    TFA9894_BF_ICLLP1= 0x44e0,    /*!< Clear low power mode1 detect                       */
    TFA9894_BF_ICLCLKOOR= 0x44f0,    /*!< Clear clock out of range                           */
    TFA9894_BF_IEVDDS= 0x4800,    /*!< Enable POR                                         */
    TFA9894_BF_IEBSTOC= 0x4810,    /*!< Enable DCDC OCP                                    */
    TFA9894_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9894_BF_IEOCPR= 0x4830,    /*!< Enable OCP alarm                                   */
    TFA9894_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9894_BF_IEMANALARM= 0x4850,    /*!< Enable Manager Alarm state                         */
    TFA9894_BF_IETDMER= 0x4860,    /*!< Enable TDM error                                   */
    TFA9894_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9894_BF_IECFMER= 0x4880,    /*!< Enable cfma err                                    */
    TFA9894_BF_IECFMAC= 0x4890,    /*!< Enable cfma ack                                    */
    TFA9894_BF_IESPKS= 0x48a0,    /*!< Enable coolflux speaker error                      */
    TFA9894_BF_IEACS = 0x48b0,    /*!< Enable cold started                                */
    TFA9894_BF_IEWDS = 0x48c0,    /*!< Enable watchdog reset                              */
    TFA9894_BF_IEBODNOK= 0x48d0,    /*!< Enable brown out detect                            */
    TFA9894_BF_IELP1 = 0x48e0,    /*!< Enable low power mode1 detect                      */
    TFA9894_BF_IECLKOOR= 0x48f0,    /*!< Enable clock out of range                          */
    TFA9894_BF_IPOVDDS= 0x4c00,    /*!< Polarity POR                                       */
    TFA9894_BF_IPOBSTOC= 0x4c10,    /*!< Polarity DCDC OCP                                  */
    TFA9894_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9894_BF_IPOOCPR= 0x4c30,    /*!< Polarity ocp alarm                                 */
    TFA9894_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9894_BF_IPOMANALARM= 0x4c50,    /*!< Polarity manager alarm state                       */
    TFA9894_BF_IPOTDMER= 0x4c60,    /*!< Polarity TDM error                                 */
    TFA9894_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9894_BF_IPOCFMER= 0x4c80,    /*!< Polarity cfma err                                  */
    TFA9894_BF_IPOCFMAC= 0x4c90,    /*!< Polarity cfma ack                                  */
    TFA9894_BF_IPOSPKS= 0x4ca0,    /*!< Polarity coolflux speaker error                    */
    TFA9894_BF_IPOACS= 0x4cb0,    /*!< Polarity cold started                              */
    TFA9894_BF_IPOWDS= 0x4cc0,    /*!< Polarity watchdog reset                            */
    TFA9894_BF_IPOBODNOK= 0x4cd0,    /*!< Polarity brown out detect                          */
    TFA9894_BF_IPOLP1= 0x4ce0,    /*!< Polarity low power mode1 detect                    */
    TFA9894_BF_IPOCLKOOR= 0x4cf0,    /*!< Polarity clock out of range                        */
    TFA9894_BF_BSSCR = 0x5001,    /*!< Battery safeguard attack time                      */
    TFA9894_BF_BSST  = 0x5023,    /*!< Battery safeguard threshold voltage level          */
    TFA9894_BF_BSSRL = 0x5061,    /*!< Battery safeguard maximum reduction                */
    TFA9894_BF_BSSRR = 0x5082,    /*!< Battery safeguard release time                     */
    TFA9894_BF_BSSHY = 0x50b1,    /*!< Battery Safeguard hysteresis                       */
    TFA9894_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9894_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA9894_BF_CFSM  = 0x5130,    /*!< Coolflux firmware soft mute control                */
    TFA9894_BF_VOL   = 0x5187,    /*!< CF firmware volume control                         */
    TFA9894_BF_CLIPCTRL= 0x5202,    /*!< Clip control setting                               */
    TFA9894_BF_SLOPEE= 0x5230,    /*!< Enables slope control                              */
    TFA9894_BF_SLOPESET= 0x5240,    /*!< Slope speed setting (binary coded)                 */
    TFA9894_BF_AMPGAIN= 0x5287,    /*!< Amplifier gain                                     */
    TFA9894_BF_TDMDCG= 0x5703,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
    TFA9894_BF_TDMSPKG= 0x5743,    /*!< Total gain depending on INPLEV setting (channel 0) */
    TFA9894_BF_DCINSEL= 0x5781,    /*!< VAMP_OUT2 input selection                          */
    TFA9894_BF_LNMODE= 0x5881,    /*!< Low noise gain mode control                        */
    TFA9894_BF_LPM1MODE= 0x5ac1,    /*!< Low power mode control                             */
    TFA9894_BF_TDMSRCMAP= 0x5d02,    /*!< TDM source mapping                                 */
    TFA9894_BF_TDMSRCAS= 0x5d31,    /*!< Sensed value A                                     */
    TFA9894_BF_TDMSRCBS= 0x5d51,    /*!< Sensed value B                                     */
    TFA9894_BF_TDMSRCACLIP= 0x5d71,    /*!< Clip information (analog /digital) for source0     */
    TFA9894_BF_TDMSRCBCLIP= 0x5d91,    /*!< Clip information (analog /digital) for source1     */
    TFA9894_BF_DELCURCOMP= 0x6102,    /*!< Delay to allign compensation signal with current sense signal */
    TFA9894_BF_SIGCURCOMP= 0x6130,    /*!< Polarity of compensation for current sense         */
    TFA9894_BF_ENCURCOMP= 0x6140,    /*!< Enable current sense compensation                  */
    TFA9894_BF_LVLCLPPWM= 0x6152,    /*!< Set the amount of pwm pulse that may be skipped before clip-flag is triggered */
    TFA9894_BF_DCVOF = 0x7005,    /*!< First Boost Voltage Level                          */
    TFA9894_BF_DCVOS = 0x7065,    /*!< Second Boost Voltage Level                         */
    TFA9894_BF_DCMCC = 0x70c3,    /*!< Max Coil Current                                   */
    TFA9894_BF_DCCV  = 0x7101,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9894_BF_DCIE  = 0x7120,    /*!< Adaptive boost mode                                */
    TFA9894_BF_DCSR  = 0x7130,    /*!< Soft ramp up/down                                  */
    TFA9894_BF_DCDIS = 0x7140,    /*!< DCDC on/off                                        */
    TFA9894_BF_DCPWM = 0x7150,    /*!< DCDC PWM only mode                                 */
    TFA9894_BF_DCTRACK= 0x7160,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_DCTRIP= 0x7204,    /*!< 1st adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9894_BF_DCTRIP2= 0x7254,    /*!< 2nd adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9894_BF_DCTRIPT= 0x72a4,    /*!< Track adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_DCTRIPHYSTE= 0x72f0,    /*!< Enable hysteresis on booster trip levels           */
    TFA9894_BF_DCHOLD= 0x7304,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_RST   = 0x9000,    /*!< Reset for Coolflux DSP                             */
    TFA9894_BF_DMEM  = 0x9011,    /*!< Target memory for CFMA using I2C interface         */
    TFA9894_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA9894_BF_CFINT = 0x9040,    /*!< Coolflux Interrupt - auto clear                    */
    TFA9894_BF_CFCGATE= 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA9894_BF_REQCMD= 0x9080,    /*!< Firmware event request rpc command                 */
    TFA9894_BF_REQRST= 0x9090,    /*!< Firmware event request reset restart               */
    TFA9894_BF_REQMIPS= 0x90a0,    /*!< Firmware event request short on mips               */
    TFA9894_BF_REQMUTED= 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA9894_BF_REQVOL= 0x90c0,    /*!< Firmware event request volume ready                */
    TFA9894_BF_REQDMG= 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA9894_BF_REQCAL= 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA9894_BF_REQRSV= 0x90f0,    /*!< Firmware event request reserved                    */
    TFA9894_BF_MADD  = 0x910f,    /*!< CF memory address                                  */
    TFA9894_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA9894_BF_ERR   = 0x9307,    /*!< CF error flags                                     */
    TFA9894_BF_ACKCMD= 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA9894_BF_ACKRST= 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA9894_BF_ACKMIPS= 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA9894_BF_ACKMUTED= 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA9894_BF_ACKVOL= 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA9894_BF_ACKDMG= 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA9894_BF_ACKCAL= 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA9894_BF_ACKRSV= 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA9894_BF_MTPK  = 0xa107,    /*!< KEY2 to access KEY2 protected registers, customer key */
    TFA9894_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9894_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9894_BF_CMTPI = 0xa350,    /*!< Start copying all the data from mtp to I2C mtp registers - auto clear */
    TFA9894_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp - auto clear */
    TFA9894_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9894_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9894_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9894_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9894_BF_SWPROFIL= 0xe00f,    /*!< Software profile data                              */
    TFA9894_BF_SWVSTEP= 0xe10f,    /*!< Software vstep information                         */
    TFA9894_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9894_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9894_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9894_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9894_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9894_BF_CUSTINFO= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9894_BF_R25C  = 0xf50f,    /*!< Ron resistance of speaker coil                     */
} nxpTfa9894BfEnumList_t;
#define TFA9894_NAMETABLE static tfaBfName_t Tfa9894DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown control                                 , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux DSP                               , */\
   { 0x30, "AMPE"},    /* Enable Amplifier                                  , */\
   { 0x40, "DCA"},    /* Enable DCDC Boost converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux control over amplifier                   , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x90, "FSSSEL"},    /* Audio sample reference                            , */\
   { 0xa0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xb0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0xc0, "BSSS"},    /* Vbat protection steepness                         , */\
   { 0xd0, "HPFBYP"},    /* Bypass High Pass Filter                           , */\
   { 0xe0, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x101, "AMPINSEL"},    /* Amplifier input selection                         , */\
   { 0x120, "MANSCONF"},    /* Device I2C settings configured                    , */\
   { 0x130, "MANCOLD"},    /* Execute cold start                                , */\
   { 0x140, "MANROBOD"},    /* Reaction on BOD                                   , */\
   { 0x150, "BODE"},    /* Enable BOD (only in direct control mode)          , */\
   { 0x160, "BODHYS"},    /* Enable Hysteresis of BOD                          , */\
   { 0x171, "BODFILT"},    /* BOD filter                                        , */\
   { 0x191, "BODTHLVL"},    /* BOD threshold                                     , */\
   { 0x1b0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1c0, "MANWDE"},    /* Watchdog enable                                   , */\
   { 0x1e0, "OPENMTP"},    /* Control for FAIM protection                       , */\
   { 0x1f0, "FAIMVBGOVRRL"},    /* Overrule the enabling of VBG for faim erase/write access, */\
   { 0x203, "AUDFS"},    /* Audio sample rate Fs                              , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* Current sense fractional delay                    , */\
   { 0x2b1, "TDMPRES"},    /* Control for HW manager                            , */\
   { 0x2d2, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external reference clock                      , */\
   { 0x420, "REFCKSEL"},    /* PLL internal reference clock                      , */\
   { 0x432, "MCLKSEL"},    /* Master Clock Selection                            , */\
   { 0x460, "MANAOOSC"},    /* Internal OSC1M off at PWDN                        , */\
   { 0x470, "ACKCLDDIS"},    /* Automatic PLL reference clock selection for cold start, */\
   { 0x510, "SPKSSEN"},    /* Enable speaker sub-system                         , */\
   { 0x520, "MTPSSEN"},    /* Enable FAIM sub-system                            , */\
   { 0x530, "WDTCLKEN"},    /* Enable Coolflux watchdog clock                    , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL Lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "OCDS"},    /* OCP amplifier (sticky register, clear on read)    , */\
   { 0x1060, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1070, "MTPB"},    /* MTP busy                                          , */\
   { 0x1080, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1090, "ACS"},    /* Cold Start                                        , */\
   { 0x10a0, "WDS"},    /* Watchdog                                          , */\
   { 0x10b0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x10d0, "AREFS"},    /* References enable                                 , */\
   { 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x10f0, "BODNOK"},    /* BOD Flag - VDD NOT OK                             , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active (sticky register, clear on read)      , */\
   { 0x1120, "DCOCPOK"},    /* DCDC OCP nmos (sticky register, clear on read)    , */\
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "SPKS"},    /* Speaker status                                    , */\
   { 0x1180, "CLKOOR"},    /* External clock status                             , */\
   { 0x1190, "MANALARM"},    /* Alarm state                                       , */\
   { 0x11a0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11b0, "TDMLUTER"},    /* TDM lookup table error                            , */\
   { 0x1200, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1210, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1220, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1230, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1240, "CLIPS"},    /* Amplifier clipping                                , */\
   { 0x1250, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x1260, "MANOPER"},    /* Device in Operating state                         , */\
   { 0x1270, "LP1"},    /* Low power MODE1 detection                         , */\
   { 0x1280, "LA"},    /* Low amplitude detection                           , */\
   { 0x1290, "VDDPH"},    /* VDDP greater than VBAT flag                       , */\
   { 0x1402, "TDMSTAT"},    /* TDM Status bits                                   , */\
   { 0x1433, "MANSTATE"},    /* Device Manager status                             , */\
   { 0x14b1, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage (1023*VDDP/13V)                   , */\
   { 0x2000, "TDME"},    /* Enable interface                                  , */\
   { 0x2010, "TDMSPKE"},    /* Control audio tdm channel in sink0                , */\
   { 0x2020, "TDMDCE"},    /* Control audio tdm channel in sink1                , */\
   { 0x2030, "TDMCSE"},    /* Source 0 enable                                   , */\
   { 0x2040, "TDMVSE"},    /* Source 1 enable                                   , */\
   { 0x2050, "TDMCFE"},    /* Source 2 enable                                   , */\
   { 0x2060, "TDMCF2E"},    /* Source 3 enable                                   , */\
   { 0x2070, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2080, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x2090, "TDMDEL"},    /* Data delay to FS                                  , */\
   { 0x20a0, "TDMADJ"},    /* Data adjustment                                   , */\
   { 0x20b1, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2103, "TDMNBCK"},    /* TDM NBCK - Bit clock to FS ratio                  , */\
   { 0x2143, "TDMFSLN"},    /* FS length (master mode only)                      , */\
   { 0x2183, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x21c1, "TDMTXDFO"},    /* Format unused bits                                , */\
   { 0x21e1, "TDMTXUS0"},    /* Format unused slots DATAO                         , */\
   { 0x2204, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2254, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x22a4, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2303, "TDMSPKS"},    /* TDM slot for sink 0                               , */\
   { 0x2343, "TDMDCS"},    /* TDM slot for sink 1                               , */\
   { 0x2381, "TDMCFSEL"},    /* TDM Source 2 data selection                       , */\
   { 0x23a1, "TDMCF2SEL"},    /* TDM Source 3 data selection                       , */\
   { 0x2403, "TDMCSS"},    /* Slot Position of source 0 data                    , */\
   { 0x2443, "TDMVSS"},    /* Slot Position of source 1 data                    , */\
   { 0x2483, "TDMCFS"},    /* Slot Position of source 2 data                    , */\
   { 0x24c3, "TDMCF2S"},    /* Slot Position of source 3 data                    , */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOCPR"},    /* Status OCP alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTMANALARM"},    /* Status manager alarm state                        , */\
   { 0x4060, "ISTTDMER"},    /* Status TDM error                                  , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4080, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x4090, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x40a0, "ISTSPKS"},    /* Status coolflux speaker error                     , */\
   { 0x40b0, "ISTACS"},    /* Status cold started                               , */\
   { 0x40c0, "ISTWDS"},    /* Status watchdog reset                             , */\
   { 0x40d0, "ISTBODNOK"},    /* Status brown out detect                           , */\
   { 0x40e0, "ISTLP1"},    /* Status low power mode1 detect                     , */\
   { 0x40f0, "ISTCLKOOR"},    /* Status clock out of range                         , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOCPR"},    /* Clear OCP alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLMANALARM"},    /* Clear manager alarm state                         , */\
   { 0x4460, "ICLTDMER"},    /* Clear TDM error                                   , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4480, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x4490, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x44a0, "ICLSPKS"},    /* Clear coolflux speaker error                      , */\
   { 0x44b0, "ICLACS"},    /* Clear cold started                                , */\
   { 0x44c0, "ICLWDS"},    /* Clear watchdog reset                              , */\
   { 0x44d0, "ICLBODNOK"},    /* Clear brown out detect                            , */\
   { 0x44e0, "ICLLP1"},    /* Clear low power mode1 detect                      , */\
   { 0x44f0, "ICLCLKOOR"},    /* Clear clock out of range                          , */\
   { 0x4800, "IEVDDS"},    /* Enable POR                                        , */\
   { 0x4810, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOCPR"},    /* Enable OCP alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IEMANALARM"},    /* Enable Manager Alarm state                        , */\
   { 0x4860, "IETDMER"},    /* Enable TDM error                                  , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4880, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x4890, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x48a0, "IESPKS"},    /* Enable coolflux speaker error                     , */\
   { 0x48b0, "IEACS"},    /* Enable cold started                               , */\
   { 0x48c0, "IEWDS"},    /* Enable watchdog reset                             , */\
   { 0x48d0, "IEBODNOK"},    /* Enable brown out detect                           , */\
   { 0x48e0, "IELP1"},    /* Enable low power mode1 detect                     , */\
   { 0x48f0, "IECLKOOR"},    /* Enable clock out of range                         , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity POR                                      , */\
   { 0x4c10, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOMANALARM"},    /* Polarity manager alarm state                      , */\
   { 0x4c60, "IPOTDMER"},    /* Polarity TDM error                                , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4c90, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4ca0, "IPOSPKS"},    /* Polarity coolflux speaker error                   , */\
   { 0x4cb0, "IPOACS"},    /* Polarity cold started                             , */\
   { 0x4cc0, "IPOWDS"},    /* Polarity watchdog reset                           , */\
   { 0x4cd0, "IPOBODNOK"},    /* Polarity brown out detect                         , */\
   { 0x4ce0, "IPOLP1"},    /* Polarity low power mode1 detect                   , */\
   { 0x4cf0, "IPOCLKOOR"},    /* Polarity clock out of range                       , */\
   { 0x5001, "BSSCR"},    /* Battery safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery safeguard maximum reduction               , */\
   { 0x5082, "BSSRR"},    /* Battery safeguard release time                    , */\
   { 0x50b1, "BSSHY"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5130, "CFSM"},    /* Coolflux firmware soft mute control               , */\
   { 0x5187, "VOL"},    /* CF firmware volume control                        , */\
   { 0x5202, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5230, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x5240, "SLOPESET"},    /* Slope speed setting (binary coded)                , */\
   { 0x5287, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x5703, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x5743, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x5781, "DCINSEL"},    /* VAMP_OUT2 input selection                         , */\
   { 0x5881, "LNMODE"},    /* Low noise gain mode control                       , */\
   { 0x5ac1, "LPM1MODE"},    /* Low power mode control                            , */\
   { 0x5d02, "TDMSRCMAP"},    /* TDM source mapping                                , */\
   { 0x5d31, "TDMSRCAS"},    /* Sensed value A                                    , */\
   { 0x5d51, "TDMSRCBS"},    /* Sensed value B                                    , */\
   { 0x5d71, "TDMSRCACLIP"},    /* Clip information (analog /digital) for source0    , */\
   { 0x5d91, "TDMSRCBCLIP"},    /* Clip information (analog /digital) for source1    , */\
   { 0x6102, "DELCURCOMP"},    /* Delay to allign compensation signal with current sense signal, */\
   { 0x6130, "SIGCURCOMP"},    /* Polarity of compensation for current sense        , */\
   { 0x6140, "ENCURCOMP"},    /* Enable current sense compensation                 , */\
   { 0x6152, "LVLCLPPWM"},    /* Set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7005, "DCVOF"},    /* First Boost Voltage Level                         , */\
   { 0x7065, "DCVOS"},    /* Second Boost Voltage Level                        , */\
   { 0x70c3, "DCMCC"},    /* Max Coil Current                                  , */\
   { 0x7101, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7120, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7130, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x7140, "DCDIS"},    /* DCDC on/off                                       , */\
   { 0x7150, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7160, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7204, "DCTRIP"},    /* 1st adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7254, "DCTRIP2"},    /* 2nd adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x72a4, "DCTRIPT"},    /* Track adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x72f0, "DCTRIPHYSTE"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7304, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x9000, "RST"},    /* Reset for Coolflux DSP                            , */\
   { 0x9011, "DMEM"},    /* Target memory for CFMA using I2C interface        , */\
   { 0x9030, "AIF"},    /* Auto increment                                    , */\
   { 0x9040, "CFINT"},    /* Coolflux Interrupt - auto clear                   , */\
   { 0x9050, "CFCGATE"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x9090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x910f, "MADD"},    /* CF memory address                                 , */\
   { 0x920f, "MEMA"},    /* Activate memory access                            , */\
   { 0x9307, "ERR"},    /* CF error flags                                    , */\
   { 0x9380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
   { 0xa107, "MTPK"},    /* KEY2 to access KEY2 protected registers, customer key, */\
   { 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
   { 0xa350, "CMTPI"},    /* Start copying all the data from mtp to I2C mtp registers - auto clear, */\
   { 0xa360, "CIMTP"},    /* Start copying data from I2C mtp registers to mtp - auto clear, */\
   { 0xa50f, "MTPRDMSB"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "MTPRDLSB"},    /* LSB word of MTP manual read data                  , */\
   { 0xb108, "EXTTS"},    /* External temperature (C)                          , */\
   { 0xb190, "TROS"},    /* Select temp Speaker calibration                   , */\
   { 0xe00f, "SWPROFIL"},    /* Software profile data                             , */\
   { 0xe10f, "SWVSTEP"},    /* Software vstep information                        , */\
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "CUSTINFO"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf50f, "R25C"},    /* Ron resistance of speaker coil                    , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum tfa9894_irq {
	tfa9894_irq_max = -1,
	tfa9894_irq_all = -1 /* all irqs */};

#define TFA9894_IRQ_NAMETABLE static tfaIrqName_t Tfa9894IrqNames[]= {\
};
#endif /* _TFA9894_TFAFIELDNAMES_H */
