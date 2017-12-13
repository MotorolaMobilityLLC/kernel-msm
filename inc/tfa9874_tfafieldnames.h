/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9874_TFAFIELDNAMES_H
#define _TFA9874_TFAFIELDNAMES_H

#define TFA9874_I2CVERSION    12

typedef enum nxpTfa9874BfEnumList {
    TFA9874_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9874_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9874_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
    TFA9874_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9874_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9874_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9874_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA9874_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9874_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA9874_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA9874_BF_OPENMTP= 0x01e0,    /*!< Control for FAIM protection                        */
    TFA9874_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA9874_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9874_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA9874_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9874_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9874_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA9874_BF_SSFAIME= 0x05c0,    /*!< Sub-system FAIM                                    */
    TFA9874_BF_AMPOCRT= 0x0802,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9874_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9874_BF_DCOCPOK= 0x1010,    /*!< DCDC OCP nmos   (sticky register , clear on read)  */
    TFA9874_BF_OTDS  = 0x1020,    /*!< OTP alarm   (sticky register , clear on read)      */
    TFA9874_BF_OCDS  = 0x1030,    /*!< OCP  amplifier   (sticky register , clear on read) */
    TFA9874_BF_UVDS  = 0x1040,    /*!< UVP alarm  (sticky register , clear on read)       */
    TFA9874_BF_MANALARM= 0x1050,    /*!< Alarm state                                        */
    TFA9874_BF_TDMERR= 0x1060,    /*!< TDM error                                          */
    TFA9874_BF_NOCLK = 0x1070,    /*!< Lost clock  (sticky register , clear on read)      */
    TFA9874_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9874_BF_DCDCA = 0x1110,    /*!< DCDC active  (sticky register , clear on read)     */
    TFA9874_BF_DCHVBAT= 0x1130,    /*!< DCDC level 1x                                      */
    TFA9874_BF_DCH114= 0x1140,    /*!< DCDC level 1.14x                                   */
    TFA9874_BF_DCH107= 0x1150,    /*!< DCDC level 1.07x                                   */
    TFA9874_BF_PLLS  = 0x1160,    /*!< PLL lock                                           */
    TFA9874_BF_CLKS  = 0x1170,    /*!< Clocks stable                                      */
    TFA9874_BF_TDMLUTER= 0x1180,    /*!< TDM LUT error                                      */
    TFA9874_BF_TDMSTAT= 0x1192,    /*!< TDM status bits                                    */
    TFA9874_BF_MTPB  = 0x11c0,    /*!< MTP busy                                           */
    TFA9874_BF_SWS   = 0x11d0,    /*!< Amplifier engage                                   */
    TFA9874_BF_AMPS  = 0x11e0,    /*!< Amplifier enable                                   */
    TFA9874_BF_AREFS = 0x11f0,    /*!< References enable                                  */
    TFA9874_BF_OCPOAP= 0x1300,    /*!< OCPOK pmos A                                       */
    TFA9874_BF_OCPOAN= 0x1310,    /*!< OCPOK nmos A                                       */
    TFA9874_BF_OCPOBP= 0x1320,    /*!< OCPOK pmos B                                       */
    TFA9874_BF_OCPOBN= 0x1330,    /*!< OCPOK nmos B                                       */
    TFA9874_BF_OVDS  = 0x1380,    /*!< OVP alarm                                          */
    TFA9874_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
    TFA9874_BF_ADCCR = 0x13a0,    /*!< Control ADC                                        */
    TFA9874_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA9874_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA9874_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA9874_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA9874_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA9874_BF_DCMODE= 0x1471,    /*!< DCDC mode status bits                              */
    TFA9874_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9874_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9874_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/13 V)                  */
    TFA9874_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
    TFA9874_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA9874_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA9874_BF_TDMFSLN= 0x2073,    /*!< FS length (master mode only)                       */
    TFA9874_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA9874_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9874_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA9874_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA9874_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA9874_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA9874_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA9874_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA9874_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA9874_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits                                 */
    TFA9874_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots DATAO                          */
    TFA9874_BF_TDMSPKE= 0x2300,    /*!< Control audio tdm channel in 0 (spkr + dcdc)       */
    TFA9874_BF_TDMDCE= 0x2310,    /*!< Control audio  tdm channel in 1  (dcdc)            */
    TFA9874_BF_TDMCSE= 0x2330,    /*!< current sense vbat temperature and vddp feedback   */
    TFA9874_BF_TDMVSE= 0x2340,    /*!< Voltage sense vbat temperature and vddp feedback   */
    TFA9874_BF_TDMSPKS= 0x2603,    /*!< tdm slot for sink 0 (speaker + dcdc)               */
    TFA9874_BF_TDMDCS= 0x2643,    /*!< tdm slot for  sink 1  (dcdc)                       */
    TFA9874_BF_TDMCSS= 0x26c3,    /*!< Slot Position of current sense vbat temperature and vddp feedback */
    TFA9874_BF_TDMVSS= 0x2703,    /*!< Slot Position of Voltage sense vbat temperature and vddp feedback */
    TFA9874_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9874_BF_ISTBSTOC= 0x4010,    /*!< Status DCDC OCP                                    */
    TFA9874_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9874_BF_ISTOCPR= 0x4030,    /*!< Status ocp alarm                                   */
    TFA9874_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9874_BF_ISTMANALARM= 0x4050,    /*!< Status  nanager Alarm state                        */
    TFA9874_BF_ISTTDMER= 0x4060,    /*!< Status tdm error                                   */
    TFA9874_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9874_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9874_BF_ICLBSTOC= 0x4410,    /*!< Clear DCDC OCP                                     */
    TFA9874_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9874_BF_ICLOCPR= 0x4430,    /*!< Clear ocp alarm                                    */
    TFA9874_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9874_BF_ICLMANALARM= 0x4450,    /*!< clear  nanager Alarm state                         */
    TFA9874_BF_ICLTDMER= 0x4460,    /*!< Clear tdm error                                    */
    TFA9874_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9874_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA9874_BF_IEBSTOC= 0x4810,    /*!< Enable DCDC OCP                                    */
    TFA9874_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9874_BF_IEOCPR= 0x4830,    /*!< Enable ocp alarm                                   */
    TFA9874_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9874_BF_IEMANALARM= 0x4850,    /*!< Enable  nanager Alarm state                        */
    TFA9874_BF_IETDMER= 0x4860,    /*!< Enable tdm error                                   */
    TFA9874_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9874_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA9874_BF_IPOBSTOC= 0x4c10,    /*!< Polarity DCDC OCP                                  */
    TFA9874_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9874_BF_IPOOCPR= 0x4c30,    /*!< Polarity ocp alarm                                 */
    TFA9874_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9874_BF_IPOMANALARM= 0x4c50,    /*!< Polarity  nanager Alarm state                      */
    TFA9874_BF_IPOTDMER= 0x4c60,    /*!< Polarity tdm error                                 */
    TFA9874_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9874_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
    TFA9874_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
    TFA9874_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
    TFA9874_BF_VBATFLTL= 0x5080,    /*!< vbat filter limit                                  */
    TFA9874_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9874_BF_BSSBY = 0x50f0,    /*!< Bypass battery safeguard                           */
    TFA9874_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA9874_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9874_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
    TFA9874_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA9874_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9874_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA9874_BF_SLOPESET= 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
    TFA9874_BF_TDMDCG= 0x6123,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
    TFA9874_BF_TDMSPKG= 0x6163,    /*!< Total gain depending on INPLEV setting (channel 0) */
    TFA9874_BF_LNMODE= 0x62e1,    /*!< ctrl select mode                                   */
    TFA9874_BF_LPM1MODE= 0x64e1,    /*!< low power mode control                             */
    TFA9874_BF_TDMSRCMAP= 0x6802,    /*!< tdm source mapping                                 */
    TFA9874_BF_TDMSRCAS= 0x6831,    /*!< Sensed value  A                                    */
    TFA9874_BF_TDMSRCBS= 0x6851,    /*!< Sensed value  B                                    */
    TFA9874_BF_TDMSRCACLIP= 0x6871,    /*!< clip information  (analog /digital) for source0    */
    TFA9874_BF_TDMSRCBCLIP= 0x6891,    /*!< clip information  (analog /digital) for source1    */
    TFA9874_BF_LP1   = 0x6e10,    /*!< low power mode 1 detection                         */
    TFA9874_BF_LA    = 0x6e20,    /*!< low amplitude detection                            */
    TFA9874_BF_VDDPH = 0x6e30,    /*!< vddp greater than vbat                             */
    TFA9874_BF_DELCURCOMP= 0x6f02,    /*!< delay to allign compensation signal with current sense signal */
    TFA9874_BF_SIGCURCOMP= 0x6f40,    /*!< polarity of compensation for current sense         */
    TFA9874_BF_ENCURCOMP= 0x6f50,    /*!< enable current sense compensation                  */
    TFA9874_BF_LVLCLPPWM= 0x6f72,    /*!< set the amount of pwm pulse that may be skipped before clip-flag is triggered */
    TFA9874_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA9874_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9874_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA9874_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA9874_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
    TFA9874_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
    TFA9874_BF_DCTRACK= 0x7430,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIP= 0x7444,    /*!< 1st Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9874_BF_DCHOLD= 0x7494,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIP2= 0x7534,    /*!< 2nd Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9874_BF_DCTRIPT= 0x7584,    /*!< Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIPHYSTE= 0x75f0,    /*!< Enable hysteresis on booster trip levels           */
    TFA9874_BF_DCVOF = 0x7635,    /*!< First boost voltage level                          */
    TFA9874_BF_DCVOS = 0x7695,    /*!< Second boost voltage level                         */
    TFA9874_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA9874_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9874_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9874_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9874_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9874_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9874_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9874_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9874_BF_SWPROFIL= 0xee0f,    /*!< Software profile data                              */
    TFA9874_BF_SWVSTEP= 0xef0f,    /*!< Software vstep information                         */
    TFA9874_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9874_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9874_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9874_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9874_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9874_BF_CUSTINFO= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9874_BF_R25C  = 0xf50f,    /*!< Ron resistance of  speaker coil                    */
} nxpTfa9874BfEnumList_t;
#define TFA9874_NAMETABLE static tfaBfName_t Tfa9874DatasheetNames[]= {\
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
   { 0x1e0, "OPENMTP"},    /* Control for FAIM protection                       , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
   { 0x802, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "DCOCPOK"},    /* DCDC OCP nmos   (sticky register , clear on read) , */\
   { 0x1020, "OTDS"},    /* OTP alarm   (sticky register , clear on read)     , */\
   { 0x1030, "OCDS"},    /* OCP  amplifier   (sticky register , clear on read), */\
   { 0x1040, "UVDS"},    /* UVP alarm  (sticky register , clear on read)      , */\
   { 0x1050, "MANALARM"},    /* Alarm state                                       , */\
   { 0x1060, "TDMERR"},    /* TDM error                                         , */\
   { 0x1070, "NOCLK"},    /* Lost clock  (sticky register , clear on read)     , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active  (sticky register , clear on read)    , */\
   { 0x1130, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1140, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1150, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1160, "PLLS"},    /* PLL lock                                          , */\
   { 0x1170, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1180, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x1192, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11c0, "MTPB"},    /* MTP busy                                          , */\
   { 0x11d0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x11e0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x11f0, "AREFS"},    /* References enable                                 , */\
   { 0x1300, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1380, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1390, "CLIPS"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
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
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOCPR"},    /* Status ocp alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTMANALARM"},    /* Status  nanager Alarm state                       , */\
   { 0x4060, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLMANALARM"},    /* clear  nanager Alarm state                        , */\
   { 0x4460, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4800, "IEVDDS"},    /* Enable por                                        , */\
   { 0x4810, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOCPR"},    /* Enable ocp alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IEMANALARM"},    /* Enable  nanager Alarm state                       , */\
   { 0x4860, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
   { 0x4c10, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOMANALARM"},    /* Polarity  nanager Alarm state                     , */\
   { 0x4c60, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5080, "VBATFLTL"},    /* vbat filter limit                                 , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass battery safeguard                          , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
   { 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x6123, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x6163, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x62e1, "LNMODE"},    /* ctrl select mode                                  , */\
   { 0x64e1, "LPM1MODE"},    /* low power mode control                            , */\
   { 0x6802, "TDMSRCMAP"},    /* tdm source mapping                                , */\
   { 0x6831, "TDMSRCAS"},    /* Sensed value  A                                   , */\
   { 0x6851, "TDMSRCBS"},    /* Sensed value  B                                   , */\
   { 0x6871, "TDMSRCACLIP"},    /* clip information  (analog /digital) for source0   , */\
   { 0x6891, "TDMSRCBCLIP"},    /* clip information  (analog /digital) for source1   , */\
   { 0x6e10, "LP1"},    /* low power mode 1 detection                        , */\
   { 0x6e20, "LA"},    /* low amplitude detection                           , */\
   { 0x6e30, "VDDPH"},    /* vddp greater than vbat                            , */\
   { 0x6f02, "DELCURCOMP"},    /* delay to allign compensation signal with current sense signal, */\
   { 0x6f40, "SIGCURCOMP"},    /* polarity of compensation for current sense        , */\
   { 0x6f50, "ENCURCOMP"},    /* enable current sense compensation                 , */\
   { 0x6f72, "LVLCLPPWM"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
   { 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7430, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7444, "DCTRIP"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7494, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x7534, "DCTRIP2"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7584, "DCTRIPT"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x75f0, "DCTRIPHYSTE"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7635, "DCVOF"},    /* First boost voltage level                         , */\
   { 0x7695, "DCVOS"},    /* Second boost voltage level                        , */\
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
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "CUSTINFO"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf50f, "R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum tfa9874_irq {
	tfa9874_irq_stvdds = 0,
	tfa9874_irq_stbstoc = 1,
	tfa9874_irq_stotds = 2,
	tfa9874_irq_stocpr = 3,
	tfa9874_irq_stuvds = 4,
	tfa9874_irq_stmanalarm = 5,
	tfa9874_irq_sttdmer = 6,
	tfa9874_irq_stnoclk = 7,
	tfa9874_irq_max = 8,
	tfa9874_irq_all = -1 /* all irqs */};

#define TFA9874_IRQ_NAMETABLE static tfaIrqName_t Tfa9874IrqNames[]= {\
	{ 0, "STVDDS"},\
	{ 1, "STBSTOC"},\
	{ 2, "STOTDS"},\
	{ 3, "STOCPR"},\
	{ 4, "STUVDS"},\
	{ 5, "STMANALARM"},\
	{ 6, "STTDMER"},\
	{ 7, "STNOCLK"},\
	{ 8, "8"},\
};
#endif /* _TFA9874_TFAFIELDNAMES_H */
