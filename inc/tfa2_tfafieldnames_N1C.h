/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 
#define TFA9888_I2CVERSION 18

typedef enum nxpTfa2BfEnumList {
    TFA2_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA2_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA2_BF_CFE   = 0x0020,    /*!< Enable CoolFlux                                    */
    TFA2_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
    TFA2_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA2_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA2_BF_AMPC  = 0x0060,    /*!< CoolFlux controls amplifier                        */
    TFA2_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA2_BF_FSSSEL= 0x0091,    /*!< Audio sample reference                             */
    TFA2_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA2_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA2_BF_AMPINSEL= 0x0101,    /*!< Amplifier input selection                          */
    TFA2_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA2_BF_MANCOLD= 0x0130,    /*!< Execute cold start                                 */
    TFA2_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA2_BF_MANROBOD= 0x0150,    /*!< Reaction on BOD                                    */
    TFA2_BF_BODE  = 0x0160,    /*!< BOD Enable                                         */
    TFA2_BF_BODHYS= 0x0170,    /*!< BOD Hysteresis                                     */
    TFA2_BF_BODFILT= 0x0181,    /*!< BOD filter                                         */
    TFA2_BF_BODTHLVL= 0x01a1,    /*!< BOD threshold                                      */
    TFA2_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA2_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
    TFA2_BF_MANWDE= 0x01f0,    /*!< Watchdog manager reaction                          */
    TFA2_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA2_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA2_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA2_BF_BYPHVBF= 0x02b0,    /*!< Bypass HVBAT filter                                */
    TFA2_BF_LDOBYP= 0x02c0,    /*!< Receiver LDO bypass                                */
    TFA2_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA2_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA2_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA2_BF_SSLEFTE= 0x0500,    /*!< Enable left channel                                */
    TFA2_BF_SSRIGHTE= 0x0510,    /*!< Enable right channel                               */
    TFA2_BF_VSLEFTE= 0x0520,    /*!< Voltage sense left                                 */
    TFA2_BF_VSRIGHTE= 0x0530,    /*!< Voltage sense right                                */
    TFA2_BF_CSLEFTE= 0x0540,    /*!< Current sense left                                 */
    TFA2_BF_CSRIGHTE= 0x0550,    /*!< Current sense right                                */
    TFA2_BF_SSPDME= 0x0560,    /*!< Sub-system PDM                                     */
    TFA2_BF_STGAIN= 0x0d18,    /*!< Side tone gain                                     */
    TFA2_BF_PDMSMUTE= 0x0da0,    /*!< Side tone soft mute                                */
    TFA2_BF_SWVSTEP= 0x0e06,    /*!< Register for the host SW to record the current active vstep */
    TFA2_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA2_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
    TFA2_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA2_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA2_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA2_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
    TFA2_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
    TFA2_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
    TFA2_BF_SPKS  = 0x1080,    /*!< Speaker error                                      */
    TFA2_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA2_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
    TFA2_BF_WDS   = 0x10b0,    /*!< Watchdog                                           */
    TFA2_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA2_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA2_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA2_BF_BODNOK= 0x10f0,    /*!< BOD                                                */
    TFA2_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA2_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
    TFA2_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos                                      */
    TFA2_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA2_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA2_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA2_BF_STMUTEB= 0x1170,    /*!< side tone (un)mute busy                            */
    TFA2_BF_STMUTE= 0x1180,    /*!< side tone mute state                               */
    TFA2_BF_TDMLUTER= 0x1190,    /*!< TDM LUT error                                      */
    TFA2_BF_TDMSTAT= 0x11a2,    /*!< TDM status bits                                    */
    TFA2_BF_TDMERR= 0x11d0,    /*!< TDM error                                          */
    TFA2_BF_HAPTIC= 0x11e0,    /*!< Status haptic driver                               */
    TFA2_BF_OCPOAPL= 0x1200,    /*!< OCPOK pmos A left                                  */
    TFA2_BF_OCPOANL= 0x1210,    /*!< OCPOK nmos A left                                  */
    TFA2_BF_OCPOBPL= 0x1220,    /*!< OCPOK pmos B left                                  */
    TFA2_BF_OCPOBNL= 0x1230,    /*!< OCPOK nmos B left                                  */
    TFA2_BF_CLIPAHL= 0x1240,    /*!< Clipping A left to Vddp                            */
    TFA2_BF_CLIPALL= 0x1250,    /*!< Clipping A left to gnd                             */
    TFA2_BF_CLIPBHL= 0x1260,    /*!< Clipping B left to Vddp                            */
    TFA2_BF_CLIPBLL= 0x1270,    /*!< Clipping B left to gnd                             */
    TFA2_BF_OCPOAPRC= 0x1280,    /*!< OCPOK pmos A RCV                                   */
    TFA2_BF_OCPOANRC= 0x1290,    /*!< OCPOK nmos A RCV                                   */
    TFA2_BF_OCPOBPRC= 0x12a0,    /*!< OCPOK pmos B RCV                                   */
    TFA2_BF_OCPOBNRC= 0x12b0,    /*!< OCPOK nmos B RCV                                   */
    TFA2_BF_RCVLDOR= 0x12c0,    /*!< RCV LDO regulates                                  */
    TFA2_BF_RCVLDOBR= 0x12d0,    /*!< Receiver LDO ready                                 */
    TFA2_BF_OCDSL = 0x12e0,    /*!< OCP left amplifier                                 */
    TFA2_BF_CLIPSL= 0x12f0,    /*!< Amplifier left clipping                            */
    TFA2_BF_OCPOAPR= 0x1300,    /*!< OCPOK pmos A right                                 */
    TFA2_BF_OCPOANR= 0x1310,    /*!< OCPOK nmos A right                                 */
    TFA2_BF_OCPOBPR= 0x1320,    /*!< OCPOK pmos B right                                 */
    TFA2_BF_OCPOBNR= 0x1330,    /*!< OCPOK nmos B right                                 */
    TFA2_BF_CLIPAHR= 0x1340,    /*!< Clipping A right to Vddp                           */
    TFA2_BF_CLIPALR= 0x1350,    /*!< Clipping A right to gnd                            */
    TFA2_BF_CLIPBHR= 0x1360,    /*!< Clipping B left to Vddp                            */
    TFA2_BF_CLIPBLR= 0x1370,    /*!< Clipping B right to gnd                            */
    TFA2_BF_OCDSR = 0x1380,    /*!< OCP right amplifier                                */
    TFA2_BF_CLIPSR= 0x1390,    /*!< Amplifier right clipping                           */
    TFA2_BF_OCPOKMC= 0x13a0,    /*!< OCPOK MICVDD                                       */
    TFA2_BF_MANALARM= 0x13b0,    /*!< Alarm state                                        */
    TFA2_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA2_BF_MANWAIT2= 0x13d0,    /*!< Wait CF config                                     */
    TFA2_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA2_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA2_BF_SPKSL = 0x1400,    /*!< Left speaker status                                */
    TFA2_BF_SPKSR = 0x1410,    /*!< Right speaker status                               */
    TFA2_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA2_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA2_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA2_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA2_BF_TDMUC = 0x2003,    /*!< Usecase setting                                    */
    TFA2_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
    TFA2_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA2_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA2_BF_TDMFSLN= 0x2073,    /*!< FS length (master mode only)                       */
    TFA2_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA2_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA2_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA2_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA2_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA2_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA2_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA2_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA2_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA2_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits                                 */
    TFA2_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots GAINIO                         */
    TFA2_BF_TDMTXUS1= 0x22b1,    /*!< Format unused slots DIO1                           */
    TFA2_BF_TDMTXUS2= 0x22d1,    /*!< Format unused slots DIO2                           */
    TFA2_BF_TDMLE = 0x2310,    /*!< Control audio left                                 */
    TFA2_BF_TDMRE = 0x2320,    /*!< Control audio right                                */
    TFA2_BF_TDMVSRE= 0x2340,    /*!< Control voltage sense right                        */
    TFA2_BF_TDMCSRE= 0x2350,    /*!< Control current sense right                        */
    TFA2_BF_TDMVSLE= 0x2360,    /*!< Voltage sense left control                         */
    TFA2_BF_TDMCSLE= 0x2370,    /*!< Current sense left control                         */
    TFA2_BF_TDMCFRE= 0x2380,    /*!< DSP out right control                              */
    TFA2_BF_TDMCFLE= 0x2390,    /*!< DSP out left control                               */
    TFA2_BF_TDMCF3E= 0x23a0,    /*!< AEC ref left control                               */
    TFA2_BF_TDMCF4E= 0x23b0,    /*!< AEC ref right control                              */
    TFA2_BF_TDMPD1E= 0x23c0,    /*!< PDM 1 control                                      */
    TFA2_BF_TDMPD2E= 0x23d0,    /*!< PDM 2 control                                      */
    TFA2_BF_TDMLIO= 0x2421,    /*!< IO audio left                                      */
    TFA2_BF_TDMRIO= 0x2441,    /*!< IO audio right                                     */
    TFA2_BF_TDMVSRIO= 0x2481,    /*!< IO voltage sense right                             */
    TFA2_BF_TDMCSRIO= 0x24a1,    /*!< IO current sense right                             */
    TFA2_BF_TDMVSLIO= 0x24c1,    /*!< IO voltage sense left                              */
    TFA2_BF_TDMCSLIO= 0x24e1,    /*!< IO current sense left                              */
    TFA2_BF_TDMCFRIO= 0x2501,    /*!< IO dspout right                                    */
    TFA2_BF_TDMCFLIO= 0x2521,    /*!< IO dspout left                                     */
    TFA2_BF_TDMCF3IO= 0x2541,    /*!< IO AEC ref left control                            */
    TFA2_BF_TDMCF4IO= 0x2561,    /*!< IO AEC ref right control                           */
    TFA2_BF_TDMPD1IO= 0x2581,    /*!< IO pdm1                                            */
    TFA2_BF_TDMPD2IO= 0x25a1,    /*!< IO pdm2                                            */
    TFA2_BF_TDMLS = 0x2643,    /*!< Position audio left                                */
    TFA2_BF_TDMRS = 0x2683,    /*!< Position audio right                               */
    TFA2_BF_TDMVSRS= 0x2703,    /*!< Position voltage sense right                       */
    TFA2_BF_TDMCSRS= 0x2743,    /*!< Position current sense right                       */
    TFA2_BF_TDMVSLS= 0x2783,    /*!< Position voltage sense left                        */
    TFA2_BF_TDMCSLS= 0x27c3,    /*!< Position current sense left                        */
    TFA2_BF_TDMCFRS= 0x2803,    /*!< Position dspout right                              */
    TFA2_BF_TDMCFLS= 0x2843,    /*!< Position dspout left                               */
    TFA2_BF_TDMCF3S= 0x2883,    /*!< Position AEC ref left control                      */
    TFA2_BF_TDMCF4S= 0x28c3,    /*!< Position AEC ref right control                     */
    TFA2_BF_TDMPD1S= 0x2903,    /*!< Position pdm1                                      */
    TFA2_BF_TDMPD2S= 0x2943,    /*!< Position pdm2                                      */
    TFA2_BF_PDMSM = 0x3100,    /*!< PDM control                                        */
    TFA2_BF_PDMSTSEL= 0x3111,    /*!< Side tone input                                    */
    TFA2_BF_PDMLSEL= 0x3130,    /*!< PDM data selection for left channel during PDM direct mode */
    TFA2_BF_PDMRSEL= 0x3140,    /*!< PDM data selection for right channel during PDM direct mode */
    TFA2_BF_MICVDDE= 0x3150,    /*!< Enable MICVDD                                      */
    TFA2_BF_PDMCLRAT= 0x3201,    /*!< PDM BCK/Fs ratio                                   */
    TFA2_BF_PDMGAIN= 0x3223,    /*!< PDM gain                                           */
    TFA2_BF_PDMOSEL= 0x3263,    /*!< PDM output selection - RE/FE data combination      */
    TFA2_BF_SELCFHAPD= 0x32a0,    /*!< Select the source for haptic data output (not for customer) */
    TFA2_BF_HAPTIME= 0x3307,    /*!< Duration (ms)                                      */
    TFA2_BF_HAPLEVEL= 0x3387,    /*!< DC value (FFS)                                     */
    TFA2_BF_GPIODIN= 0x3403,    /*!< Receiving value                                    */
    TFA2_BF_GPIOCTRL= 0x3500,    /*!< GPIO master control over GPIO1/2 ports (not for customer) */
    TFA2_BF_GPIOCONF= 0x3513,    /*!< Configuration                                      */
    TFA2_BF_GPIODOUT= 0x3553,    /*!< Transmitting value                                 */
    TFA2_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA2_BF_ISTPLLS= 0x4010,    /*!< Status PLL lock                                    */
    TFA2_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA2_BF_ISTOVDS= 0x4030,    /*!< Status OVP alarm                                   */
    TFA2_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA2_BF_ISTCLKS= 0x4050,    /*!< Status clocks stable                               */
    TFA2_BF_ISTMTPB= 0x4060,    /*!< Status MTP busy                                    */
    TFA2_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA2_BF_ISTSPKS= 0x4080,    /*!< Status speaker error                               */
    TFA2_BF_ISTACS= 0x4090,    /*!< Status cold start                                  */
    TFA2_BF_ISTSWS= 0x40a0,    /*!< Status amplifier engage                            */
    TFA2_BF_ISTWDS= 0x40b0,    /*!< Status watchdog                                    */
    TFA2_BF_ISTAMPS= 0x40c0,    /*!< Status amplifier enable                            */
    TFA2_BF_ISTAREFS= 0x40d0,    /*!< Status Ref enable                                  */
    TFA2_BF_ISTADCCR= 0x40e0,    /*!< Status Control ADC                                 */
    TFA2_BF_ISTBODNOK= 0x40f0,    /*!< Status BOD                                         */
    TFA2_BF_ISTBSTCU= 0x4100,    /*!< Status DCDC current limiting                       */
    TFA2_BF_ISTBSTHI= 0x4110,    /*!< Status DCDC active                                 */
    TFA2_BF_ISTBSTOC= 0x4120,    /*!< Status DCDC OCP                                    */
    TFA2_BF_ISTBSTPKCUR= 0x4130,    /*!< Status bst peakcur                                 */
    TFA2_BF_ISTBSTVC= 0x4140,    /*!< Status DCDC level 1x                               */
    TFA2_BF_ISTBST86= 0x4150,    /*!< Status DCDC level 1.14x                            */
    TFA2_BF_ISTBST93= 0x4160,    /*!< Status DCDC level 1.07x                            */
    TFA2_BF_ISTRCVLD= 0x4170,    /*!< Status rcvldop ready                               */
    TFA2_BF_ISTOCPL= 0x4180,    /*!< Status ocp alarm left                              */
    TFA2_BF_ISTOCPR= 0x4190,    /*!< Status ocp alarm right                             */
    TFA2_BF_ISTMWSRC= 0x41a0,    /*!< Status Waits HW I2C settings                       */
    TFA2_BF_ISTMWCFC= 0x41b0,    /*!< Status waits CF config                             */
    TFA2_BF_ISTMWSMU= 0x41c0,    /*!< Status Audio mute sequence                         */
    TFA2_BF_ISTCFMER= 0x41d0,    /*!< Status cfma error                                  */
    TFA2_BF_ISTCFMAC= 0x41e0,    /*!< Status cfma ack                                    */
    TFA2_BF_ISTCLKOOR= 0x41f0,    /*!< Status flag_clk_out_of_range                       */
    TFA2_BF_ISTTDMER= 0x4200,    /*!< Status tdm error                                   */
    TFA2_BF_ISTCLPL= 0x4210,    /*!< Status clip left                                   */
    TFA2_BF_ISTCLPR= 0x4220,    /*!< Status clip right                                  */
    TFA2_BF_ISTOCPM= 0x4230,    /*!< Status mic ocpok                                   */
    TFA2_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA2_BF_ICLPLLS= 0x4410,    /*!< Clear PLL lock                                     */
    TFA2_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA2_BF_ICLOVDS= 0x4430,    /*!< Clear OVP alarm                                    */
    TFA2_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA2_BF_ICLCLKS= 0x4450,    /*!< Clear clocks stable                                */
    TFA2_BF_ICLMTPB= 0x4460,    /*!< Clear mtp busy                                     */
    TFA2_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA2_BF_ICLSPKS= 0x4480,    /*!< Clear speaker error                                */
    TFA2_BF_ICLACS= 0x4490,    /*!< Clear cold started                                 */
    TFA2_BF_ICLSWS= 0x44a0,    /*!< Clear amplifier engage                             */
    TFA2_BF_ICLWDS= 0x44b0,    /*!< Clear watchdog                                     */
    TFA2_BF_ICLAMPS= 0x44c0,    /*!< Clear enbl amp                                     */
    TFA2_BF_ICLAREFS= 0x44d0,    /*!< Clear ref enable                                   */
    TFA2_BF_ICLADCCR= 0x44e0,    /*!< Clear control ADC                                  */
    TFA2_BF_ICLBODNOK= 0x44f0,    /*!< Clear BOD                                          */
    TFA2_BF_ICLBSTCU= 0x4500,    /*!< Clear DCDC current limiting                        */
    TFA2_BF_ICLBSTHI= 0x4510,    /*!< Clear DCDC active                                  */
    TFA2_BF_ICLBSTOC= 0x4520,    /*!< Clear DCDC OCP                                     */
    TFA2_BF_ICLBSTPC= 0x4530,    /*!< Clear bst peakcur                                  */
    TFA2_BF_ICLBSTVC= 0x4540,    /*!< Clear DCDC level 1x                                */
    TFA2_BF_ICLBST86= 0x4550,    /*!< Clear DCDC level 1.14x                             */
    TFA2_BF_ICLBST93= 0x4560,    /*!< Clear DCDC level 1.07x                             */
    TFA2_BF_ICLRCVLD= 0x4570,    /*!< Clear rcvldop ready                                */
    TFA2_BF_ICLOCPL= 0x4580,    /*!< Clear ocp alarm left                               */
    TFA2_BF_ICLOCPR= 0x4590,    /*!< Clear ocp alarm right                              */
    TFA2_BF_ICLMWSRC= 0x45a0,    /*!< Clear wait HW I2C settings                         */
    TFA2_BF_ICLMWCFC= 0x45b0,    /*!< Clear wait cf config                               */
    TFA2_BF_ICLMWSMU= 0x45c0,    /*!< Clear audio mute sequence                          */
    TFA2_BF_ICLCFMER= 0x45d0,    /*!< Clear cfma err                                     */
    TFA2_BF_ICLCFMAC= 0x45e0,    /*!< Clear cfma ack                                     */
    TFA2_BF_ICLCLKOOR= 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
    TFA2_BF_ICLTDMER= 0x4600,    /*!< Clear tdm error                                    */
    TFA2_BF_ICLCLPL= 0x4610,    /*!< Clear clip left                                    */
    TFA2_BF_ICLCLPR= 0x4620,    /*!< Clear clip right                                   */
    TFA2_BF_ICLOCPM= 0x4630,    /*!< Clear mic ocpok                                    */
    TFA2_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA2_BF_IEPLLS= 0x4810,    /*!< Enable pll lock                                    */
    TFA2_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA2_BF_IEOVDS= 0x4830,    /*!< Enable OVP alarm                                   */
    TFA2_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA2_BF_IECLKS= 0x4850,    /*!< Enable clocks stable                               */
    TFA2_BF_IEMTPB= 0x4860,    /*!< Enable mtp busy                                    */
    TFA2_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA2_BF_IESPKS= 0x4880,    /*!< Enable speaker error                               */
    TFA2_BF_IEACS = 0x4890,    /*!< Enable cold started                                */
    TFA2_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
    TFA2_BF_IEWDS = 0x48b0,    /*!< Enable watchdog                                    */
    TFA2_BF_IEAMPS= 0x48c0,    /*!< Enable enbl amp                                    */
    TFA2_BF_IEAREFS= 0x48d0,    /*!< Enable ref enable                                  */
    TFA2_BF_IEADCCR= 0x48e0,    /*!< Enable Control ADC                                 */
    TFA2_BF_IEBODNOK= 0x48f0,    /*!< Enable BOD                                         */
    TFA2_BF_IEBSTCU= 0x4900,    /*!< Enable DCDC current limiting                       */
    TFA2_BF_IEBSTHI= 0x4910,    /*!< Enable DCDC active                                 */
    TFA2_BF_IEBSTOC= 0x4920,    /*!< Enable DCDC OCP                                    */
    TFA2_BF_IEBSTPC= 0x4930,    /*!< Enable bst peakcur                                 */
    TFA2_BF_IEBSTVC= 0x4940,    /*!< Enable DCDC level 1x                               */
    TFA2_BF_IEBST86= 0x4950,    /*!< Enable DCDC level 1.14x                            */
    TFA2_BF_IEBST93= 0x4960,    /*!< Enable DCDC level 1.07x                            */
    TFA2_BF_IERCVLD= 0x4970,    /*!< Enable rcvldop ready                               */
    TFA2_BF_IEOCPL= 0x4980,    /*!< Enable ocp alarm left                              */
    TFA2_BF_IEOCPR= 0x4990,    /*!< Enable ocp alarm right                             */
    TFA2_BF_IEMWSRC= 0x49a0,    /*!< Enable waits HW I2C settings                       */
    TFA2_BF_IEMWCFC= 0x49b0,    /*!< Enable man wait cf config                          */
    TFA2_BF_IEMWSMU= 0x49c0,    /*!< Enable man Audio mute sequence                     */
    TFA2_BF_IECFMER= 0x49d0,    /*!< Enable cfma err                                    */
    TFA2_BF_IECFMAC= 0x49e0,    /*!< Enable cfma ack                                    */
    TFA2_BF_IECLKOOR= 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
    TFA2_BF_IETDMER= 0x4a00,    /*!< Enable tdm error                                   */
    TFA2_BF_IECLPL= 0x4a10,    /*!< Enable clip left                                   */
    TFA2_BF_IECLPR= 0x4a20,    /*!< Enable clip right                                  */
    TFA2_BF_IEOCPM1= 0x4a30,    /*!< Enable mic ocpok                                   */
    TFA2_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA2_BF_IPOPLLS= 0x4c10,    /*!< Polarity pll lock                                  */
    TFA2_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA2_BF_IPOOVDS= 0x4c30,    /*!< Polarity OVP alarm                                 */
    TFA2_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA2_BF_IPOCLKS= 0x4c50,    /*!< Polarity clocks stable                             */
    TFA2_BF_IPOMTPB= 0x4c60,    /*!< Polarity mtp busy                                  */
    TFA2_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA2_BF_IPOSPKS= 0x4c80,    /*!< Polarity speaker error                             */
    TFA2_BF_IPOACS= 0x4c90,    /*!< Polarity cold started                              */
    TFA2_BF_IPOSWS= 0x4ca0,    /*!< Polarity amplifier engage                          */
    TFA2_BF_IPOWDS= 0x4cb0,    /*!< Polarity watchdog                                  */
    TFA2_BF_IPOAMPS= 0x4cc0,    /*!< Polarity enbl amp                                  */
    TFA2_BF_IPOAREFS= 0x4cd0,    /*!< Polarity ref enable                                */
    TFA2_BF_IPOADCCR= 0x4ce0,    /*!< Polarity Control ADC                               */
    TFA2_BF_IPOBODNOK= 0x4cf0,    /*!< Polarity BOD                                       */
    TFA2_BF_IPOBSTCU= 0x4d00,    /*!< Polarity DCDC current limiting                     */
    TFA2_BF_IPOBSTHI= 0x4d10,    /*!< Polarity DCDC active                               */
    TFA2_BF_IPOBSTOC= 0x4d20,    /*!< Polarity DCDC OCP                                  */
    TFA2_BF_IPOBSTPC= 0x4d30,    /*!< Polarity bst peakcur                               */
    TFA2_BF_IPOBSTVC= 0x4d40,    /*!< Polarity DCDC level 1x                             */
    TFA2_BF_IPOBST86= 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
    TFA2_BF_IPOBST93= 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
    TFA2_BF_IPORCVLD= 0x4d70,    /*!< Polarity rcvldop ready                             */
    TFA2_BF_IPOOCPL= 0x4d80,    /*!< Polarity ocp alarm left                            */
    TFA2_BF_IPOOCPR= 0x4d90,    /*!< Polarity ocp alarm right                           */
    TFA2_BF_IPOMWSRC= 0x4da0,    /*!< Polarity waits HW I2C settings                     */
    TFA2_BF_IPOMWCFC= 0x4db0,    /*!< Polarity man wait cf config                        */
    TFA2_BF_IPOMWSMU= 0x4dc0,    /*!< Polarity man audio mute sequence                   */
    TFA2_BF_IPOCFMER= 0x4dd0,    /*!< Polarity cfma err                                  */
    TFA2_BF_IPOCFMAC= 0x4de0,    /*!< Polarity cfma ack                                  */
    TFA2_BF_IPCLKOOR= 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
    TFA2_BF_IPOTDMER= 0x4e00,    /*!< Polarity tdm error                                 */
    TFA2_BF_IPOCLPL= 0x4e10,    /*!< Polarity clip left                                 */
    TFA2_BF_IPOCLPR= 0x4e20,    /*!< Polarity clip right                                */
    TFA2_BF_IPOOCPM= 0x4e30,    /*!< Polarity mic ocpok                                 */
    TFA2_BF_BSSCR = 0x5001,    /*!< Battery protection attack Time                     */
    TFA2_BF_BSST  = 0x5023,    /*!< Battery protection threshold voltage level         */
    TFA2_BF_BSSRL = 0x5061,    /*!< Battery protection maximum reduction               */
    TFA2_BF_BSSRR = 0x5082,    /*!< Battery protection release time                    */
    TFA2_BF_BSSHY = 0x50b1,    /*!< Battery protection hysteresis                      */
    TFA2_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA2_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA2_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA2_BF_INTSMUTE= 0x5110,    /*!< Soft mute HW                                       */
    TFA2_BF_CFSML = 0x5120,    /*!< Soft mute FW left                                  */
    TFA2_BF_CFSMR = 0x5130,    /*!< Soft mute FW right                                 */
    TFA2_BF_HPFBYPL= 0x5140,    /*!< Bypass HPF left                                    */
    TFA2_BF_HPFBYPR= 0x5150,    /*!< Bypass HPF right                                   */
    TFA2_BF_DPSAL = 0x5160,    /*!< Enable DPSA left                                   */
    TFA2_BF_DPSAR = 0x5170,    /*!< Enable DPSA right                                  */
    TFA2_BF_VOL   = 0x5187,    /*!< FW volume control for primary audio channel        */
    TFA2_BF_HNDSFRCV= 0x5200,    /*!< Selection receiver                                 */
    TFA2_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA2_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA2_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA2_BF_SLOPESET= 0x52e1,    /*!< Set slope                                          */
    TFA2_BF_VOLSEC= 0x5a07,    /*!< FW volume control for secondary audio channel      */
    TFA2_BF_SWPROFIL= 0x5a87,    /*!< Software profile data                              */
    TFA2_BF_DCVO  = 0x7002,    /*!< Boost voltage                                      */
    TFA2_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA2_BF_DCCV  = 0x7071,    /*!< Coil Value                                         */
    TFA2_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA2_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA2_BF_DCSYNCP= 0x70b2,    /*!< DCDC synchronization off + 7 positions             */
    TFA2_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
    TFA2_BF_RST   = 0x9000,    /*!< Reset                                              */
    TFA2_BF_DMEM  = 0x9011,    /*!< Target memory                                      */
    TFA2_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA2_BF_CFINT = 0x9040,    /*!< Interrupt - auto clear                             */
    TFA2_BF_CFCGATE= 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA2_BF_REQ   = 0x9087,    /*!< request for access (8 channels)                    */
    TFA2_BF_REQCMD= 0x9080,    /*!< Firmware event request rpc command                 */
    TFA2_BF_REQRST= 0x9090,    /*!< Firmware event request reset restart               */
    TFA2_BF_REQMIPS= 0x90a0,    /*!< Firmware event request short on mips               */
    TFA2_BF_REQMUTED= 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA2_BF_REQVOL= 0x90c0,    /*!< Firmware event request volume ready                */
    TFA2_BF_REQDMG= 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA2_BF_REQCAL= 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA2_BF_REQRSV= 0x90f0,    /*!< Firmware event request reserved                    */
    TFA2_BF_MADD  = 0x910f,    /*!< Memory address                                     */
    TFA2_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA2_BF_ERR   = 0x9307,    /*!< Error flags                                        */
    TFA2_BF_ACK   = 0x9387,    /*!< Acknowledge of requests                            */
    TFA2_BF_ACKCMD= 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA2_BF_ACKRST= 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA2_BF_ACKMIPS= 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA2_BF_ACKMUTED= 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA2_BF_ACKVOL= 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA2_BF_ACKDMG= 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA2_BF_ACKCAL= 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA2_BF_ACKRSV= 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA2_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA2_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA2_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA2_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA2_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA2_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA2_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA2_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA2_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA2_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA2_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA2_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA2_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA2_BF_R25CL = 0xf40f,    /*!< Ron resistance of left channel speaker coil        */
    TFA2_BF_R25CR = 0xf50f,    /*!< Ron resistance of right channel speaker coil       */
} nxpTfa2BfEnumList_t;
#define TFA2_NAMETABLE static tfaBfName_t Tfa2DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x30, "AMPE"},    /* Activate Amplifier                                , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x91, "FSSSEL"},    /* Audio sample reference                            , */\
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
   { 0x1f0, "MANWDE"},    /* Watchdog manager reaction                         , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "LDOBYP"},    /* Receiver LDO bypass                               , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x500, "SSLEFTE"},    /* Enable left channel                               , */\
   { 0x510, "SSRIGHTE"},    /* Enable right channel                              , */\
   { 0x520, "VSLEFTE"},    /* Voltage sense left                                , */\
   { 0x530, "VSRIGHTE"},    /* Voltage sense right                               , */\
   { 0x540, "CSLEFTE"},    /* Current sense left                                , */\
   { 0x550, "CSRIGHTE"},    /* Current sense right                               , */\
   { 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
   { 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
   { 0xda0, "PDMSMUTE"},    /* Side tone soft mute                               , */\
   { 0xe06, "SWVSTEP"},    /* Register for the host SW to record the current active vstep, */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1060, "MTPB"},    /* MTP busy                                          , */\
   { 0x1070, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1080, "SPKS"},    /* Speaker error                                     , */\
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
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "STMUTE"},    /* side tone mute state                              , */\
   { 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11d0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11e0, "HAPTIC"},    /* Status haptic driver                              , */\
   { 0x1200, "OCPOAPL"},    /* OCPOK pmos A left                                 , */\
   { 0x1210, "OCPOANL"},    /* OCPOK nmos A left                                 , */\
   { 0x1220, "OCPOBPL"},    /* OCPOK pmos B left                                 , */\
   { 0x1230, "OCPOBNL"},    /* OCPOK nmos B left                                 , */\
   { 0x1240, "CLIPAHL"},    /* Clipping A left to Vddp                           , */\
   { 0x1250, "CLIPALL"},    /* Clipping A left to gnd                            , */\
   { 0x1260, "CLIPBHL"},    /* Clipping B left to Vddp                           , */\
   { 0x1270, "CLIPBLL"},    /* Clipping B left to gnd                            , */\
   { 0x1280, "OCPOAPRC"},    /* OCPOK pmos A RCV                                  , */\
   { 0x1290, "OCPOANRC"},    /* OCPOK nmos A RCV                                  , */\
   { 0x12a0, "OCPOBPRC"},    /* OCPOK pmos B RCV                                  , */\
   { 0x12b0, "OCPOBNRC"},    /* OCPOK nmos B RCV                                  , */\
   { 0x12c0, "RCVLDOR"},    /* RCV LDO regulates                                 , */\
   { 0x12d0, "RCVLDOBR"},    /* Receiver LDO ready                                , */\
   { 0x12e0, "OCDSL"},    /* OCP left amplifier                                , */\
   { 0x12f0, "CLIPSL"},    /* Amplifier left clipping                           , */\
   { 0x1300, "OCPOAPR"},    /* OCPOK pmos A right                                , */\
   { 0x1310, "OCPOANR"},    /* OCPOK nmos A right                                , */\
   { 0x1320, "OCPOBPR"},    /* OCPOK pmos B right                                , */\
   { 0x1330, "OCPOBNR"},    /* OCPOK nmos B right                                , */\
   { 0x1340, "CLIPAHR"},    /* Clipping A right to Vddp                          , */\
   { 0x1350, "CLIPALR"},    /* Clipping A right to gnd                           , */\
   { 0x1360, "CLIPBHR"},    /* Clipping B left to Vddp                           , */\
   { 0x1370, "CLIPBLR"},    /* Clipping B right to gnd                           , */\
   { 0x1380, "OCDSR"},    /* OCP right amplifier                               , */\
   { 0x1390, "CLIPSR"},    /* Amplifier right clipping                          , */\
   { 0x13a0, "OCPOKMC"},    /* OCPOK MICVDD                                      , */\
   { 0x13b0, "MANALARM"},    /* Alarm state                                       , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "MANWAIT2"},    /* Wait CF config                                    , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1400, "SPKSL"},    /* Left speaker status                               , */\
   { 0x1410, "SPKSR"},    /* Right speaker status                              , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x2003, "TDMUC"},    /* Usecase setting                                   , */\
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
   { 0x2291, "TDMTXUS0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "TDMTXUS1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "TDMTXUS2"},    /* Format unused slots DIO2                          , */\
   { 0x2310, "TDMLE"},    /* Control audio left                                , */\
   { 0x2320, "TDMRE"},    /* Control audio right                               , */\
   { 0x2340, "TDMVSRE"},    /* Control voltage sense right                       , */\
   { 0x2350, "TDMCSRE"},    /* Control current sense right                       , */\
   { 0x2360, "TDMVSLE"},    /* Voltage sense left control                        , */\
   { 0x2370, "TDMCSLE"},    /* Current sense left control                        , */\
   { 0x2380, "TDMCFRE"},    /* DSP out right control                             , */\
   { 0x2390, "TDMCFLE"},    /* DSP out left control                              , */\
   { 0x23a0, "TDMCF3E"},    /* AEC ref left control                              , */\
   { 0x23b0, "TDMCF4E"},    /* AEC ref right control                             , */\
   { 0x23c0, "TDMPD1E"},    /* PDM 1 control                                     , */\
   { 0x23d0, "TDMPD2E"},    /* PDM 2 control                                     , */\
   { 0x2421, "TDMLIO"},    /* IO audio left                                     , */\
   { 0x2441, "TDMRIO"},    /* IO audio right                                    , */\
   { 0x2481, "TDMVSRIO"},    /* IO voltage sense right                            , */\
   { 0x24a1, "TDMCSRIO"},    /* IO current sense right                            , */\
   { 0x24c1, "TDMVSLIO"},    /* IO voltage sense left                             , */\
   { 0x24e1, "TDMCSLIO"},    /* IO current sense left                             , */\
   { 0x2501, "TDMCFRIO"},    /* IO dspout right                                   , */\
   { 0x2521, "TDMCFLIO"},    /* IO dspout left                                    , */\
   { 0x2541, "TDMCF3IO"},    /* IO AEC ref left control                           , */\
   { 0x2561, "TDMCF4IO"},    /* IO AEC ref right control                          , */\
   { 0x2581, "TDMPD1IO"},    /* IO pdm1                                           , */\
   { 0x25a1, "TDMPD2IO"},    /* IO pdm2                                           , */\
   { 0x2643, "TDMLS"},    /* Position audio left                               , */\
   { 0x2683, "TDMRS"},    /* Position audio right                              , */\
   { 0x2703, "TDMVSRS"},    /* Position voltage sense right                      , */\
   { 0x2743, "TDMCSRS"},    /* Position current sense right                      , */\
   { 0x2783, "TDMVSLS"},    /* Position voltage sense left                       , */\
   { 0x27c3, "TDMCSLS"},    /* Position current sense left                       , */\
   { 0x2803, "TDMCFRS"},    /* Position dspout right                             , */\
   { 0x2843, "TDMCFLS"},    /* Position dspout left                              , */\
   { 0x2883, "TDMCF3S"},    /* Position AEC ref left control                     , */\
   { 0x28c3, "TDMCF4S"},    /* Position AEC ref right control                    , */\
   { 0x2903, "TDMPD1S"},    /* Position pdm1                                     , */\
   { 0x2943, "TDMPD2S"},    /* Position pdm2                                     , */\
   { 0x3100, "PDMSM"},    /* PDM control                                       , */\
   { 0x3111, "PDMSTSEL"},    /* Side tone input                                   , */\
   { 0x3130, "PDMLSEL"},    /* PDM data selection for left channel during PDM direct mode, */\
   { 0x3140, "PDMRSEL"},    /* PDM data selection for right channel during PDM direct mode, */\
   { 0x3150, "MICVDDE"},    /* Enable MICVDD                                     , */\
   { 0x3201, "PDMCLRAT"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x3223, "PDMGAIN"},    /* PDM gain                                          , */\
   { 0x3263, "PDMOSEL"},    /* PDM output selection - RE/FE data combination     , */\
   { 0x32a0, "SELCFHAPD"},    /* Select the source for haptic data output (not for customer), */\
   { 0x3307, "HAPTIME"},    /* Duration (ms)                                     , */\
   { 0x3387, "HAPLEVEL"},    /* DC value (FFS)                                    , */\
   { 0x3403, "GPIODIN"},    /* Receiving value                                   , */\
   { 0x3500, "GPIOCTRL"},    /* GPIO master control over GPIO1/2 ports (not for customer), */\
   { 0x3513, "GPIOCONF"},    /* Configuration                                     , */\
   { 0x3553, "GPIODOUT"},    /* Transmitting value                                , */\
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
   { 0x4190, "ISTOCPR"},    /* Status ocp alarm right                            , */\
   { 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "ISTMWCFC"},    /* Status waits CF config                            , */\
   { 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x41e0, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4210, "ISTCLPL"},    /* Status clip left                                  , */\
   { 0x4220, "ISTCLPR"},    /* Status clip right                                 , */\
   { 0x4230, "ISTOCPM"},    /* Status mic ocpok                                  , */\
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
   { 0x4590, "ICLOCPR"},    /* Clear ocp alarm right                             , */\
   { 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "ICLMWCFC"},    /* Clear wait cf config                              , */\
   { 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x45e0, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4610, "ICLCLPL"},    /* Clear clip left                                   , */\
   { 0x4620, "ICLCLPR"},    /* Clear clip right                                  , */\
   { 0x4630, "ICLOCPM"},    /* Clear mic ocpok                                   , */\
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
   { 0x4990, "IEOCPR"},    /* Enable ocp alarm right                            , */\
   { 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "IEMWCFC"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x49e0, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4a10, "IECLPL"},    /* Enable clip left                                  , */\
   { 0x4a20, "IECLPR"},    /* Enable clip right                                 , */\
   { 0x4a30, "IEOCPM1"},    /* Enable mic ocpok                                  , */\
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
   { 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm right                          , */\
   { 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "IPOMWCFC"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "IPCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4e10, "IPOCLPL"},    /* Polarity clip left                                , */\
   { 0x4e20, "IPOCLPR"},    /* Polarity clip right                               , */\
   { 0x4e30, "IPOOCPM"},    /* Polarity mic ocpok                                , */\
   { 0x5001, "BSSCR"},    /* Battery protection attack Time                    , */\
   { 0x5023, "BSST"},    /* Battery protection threshold voltage level        , */\
   { 0x5061, "BSSRL"},    /* Battery protection maximum reduction              , */\
   { 0x5082, "BSSRR"},    /* Battery protection release time                   , */\
   { 0x50b1, "BSSHY"},    /* Battery protection hysteresis                     , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
   { 0x5120, "CFSML"},    /* Soft mute FW left                                 , */\
   { 0x5130, "CFSMR"},    /* Soft mute FW right                                , */\
   { 0x5140, "HPFBYPL"},    /* Bypass HPF left                                   , */\
   { 0x5150, "HPFBYPR"},    /* Bypass HPF right                                  , */\
   { 0x5160, "DPSAL"},    /* Enable DPSA left                                  , */\
   { 0x5170, "DPSAR"},    /* Enable DPSA right                                 , */\
   { 0x5187, "VOL"},    /* FW volume control for primary audio channel       , */\
   { 0x5200, "HNDSFRCV"},    /* Selection receiver                                , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e1, "SLOPESET"},    /* Set slope                                         , */\
   { 0x5a07, "VOLSEC"},    /* FW volume control for secondary audio channel     , */\
   { 0x5a87, "SWPROFIL"},    /* Software profile data                             , */\
   { 0x7002, "DCVO"},    /* Boost voltage                                     , */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Coil Value                                        , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70b2, "DCSYNCP"},    /* DCDC synchronization off + 7 positions            , */\
   { 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
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
   { 0x9387, "ACK"},    /* Acknowledge of requests                           , */\
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
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf40f, "R25CL"},    /* Ron resistance of left channel speaker coil       , */\
   { 0xf50f, "R25CR"},    /* Ron resistance of right channel speaker coil      , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum tfa2_irq {
	tfa2_irq_stvdds = 0,
	tfa2_irq_stplls = 1,
	tfa2_irq_stotds = 2,
	tfa2_irq_stovds = 3,
	tfa2_irq_stuvds = 4,
	tfa2_irq_stclks = 5,
	tfa2_irq_stmtpb = 6,
	tfa2_irq_stnoclk = 7,
	tfa2_irq_stspks = 8,
	tfa2_irq_stacs = 9,
	tfa2_irq_stsws = 10,
	tfa2_irq_stwds = 11,
	tfa2_irq_stamps = 12,
	tfa2_irq_starefs = 13,
	tfa2_irq_stadccr = 14,
	tfa2_irq_stbodnok = 15,
	tfa2_irq_stbstcu = 16,
	tfa2_irq_stbsthi = 17,
	tfa2_irq_stbstoc = 18,
	tfa2_irq_stbstpkcur = 19,
	tfa2_irq_stbstvc = 20,
	tfa2_irq_stbst86 = 21,
	tfa2_irq_stbst93 = 22,
	tfa2_irq_strcvld = 23,
	tfa2_irq_stocpl = 24,
	tfa2_irq_stocpr = 25,
	tfa2_irq_stmwsrc = 26,
	tfa2_irq_stmwcfc = 27,
	tfa2_irq_stmwsmu = 28,
	tfa2_irq_stcfmer = 29,
	tfa2_irq_stcfmac = 30,
	tfa2_irq_stclkoor = 31,
	tfa2_irq_sttdmer = 32,
	tfa2_irq_stclpl = 33,
	tfa2_irq_stclpr = 34,
	tfa2_irq_stocpm = 35,
	tfa2_irq_max = 36,
	tfa2_irq_all = -1 /* all irqs */};

#define TFA2_IRQ_NAMETABLE static tfaIrqName_t Tfa2IrqNames[]= {\
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
};
